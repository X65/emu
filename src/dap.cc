#include "./dap.h"

#include "log.h"

#include "ui/ui_util.h"
#include "ui/ui_settings.h"
#define UI_DBG_USE_W65C816S
#include "chips/w65c816s.h"
#include "ui/ui_dbg.h"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <map>
#include <vector>

#include "sokol_app.h"

#include "dap/types.h"
#include "dap/io.h"
#include "dap/network.h"
#include "dap/protocol.h"
#include "dap/session.h"

#ifdef _MSC_VER
    #define OS_WINDOWS 1
#endif

#ifdef OS_WINDOWS
    #include <fcntl.h>  // _O_BINARY
    #include <io.h>     // _setmode
#endif                  // OS_WINDOWS

static struct {
    bool dbg_connect_requested;
} before_init_state;

static struct {
    bool inited;
    bool enabled;

    bool stdio;        // use stdin/stdout
    const char* port;  // use TCP port

    webapi_interface_t funcs;

    void* session;  // DAP session
    void* server;   // DAP TCP server
} state;

// Uncomment the line below and change <path-to-log-file> to a file path to
// write all DAP communications to the given path.
//
// #define LOG_TO_FILE "<path-to-log-file>"
#define LOG_TO_FILE "dap.log"

std::shared_ptr<dap::Writer> log;

enum {
    DAP_INFO = 1000,
    DAP_NETWORK,
    DAP_SESSION_ERROR,
    DAP_NETWORK_ERROR,
};

#define EMU_RAM_SIZE (1 << 24)  // 16MB

// Hard-coded identifiers for the one thread, frame, variable and source.
// These numbers have no meaning, and just need to remain constant for the
// duration of the service.
const int threadId = 1;
const int registersVariablesReferenceId = 1;
const int registerCVariablesReferenceId = 10;
const int registerPVariablesReferenceId = 11;

// ---------------------------------------------------------------------------

static void dap_dbg_connect(void) {
    if (state.inited) {
        if (state.funcs.dbg_connect) {
            LOG_INFO(DAP_INFO, "dbg_connect() called");
            state.funcs.dbg_connect();
        }
    }
    else {
        before_init_state.dbg_connect_requested = true;
    }
}

static void dap_dbg_disconnect(void) {
    if (state.inited && state.funcs.dbg_disconnect) {
        LOG_INFO(DAP_INFO, "dbg_disconnect() called");
        state.funcs.dbg_disconnect();
    }
}

static void dap_boot(void) {
    if (state.inited && state.funcs.boot) {
        LOG_INFO(DAP_INFO, "dap_boot() called");
        state.funcs.boot();
    }
}

static void dap_reset(void) {
    if (state.inited && state.funcs.reset) {
        LOG_INFO(DAP_INFO, "reset() called");
        state.funcs.reset();
    }
}

static bool dap_ready(void) {
    if (state.inited && state.funcs.ready) {
        LOG_INFO(DAP_INFO, "ready() called");
        return state.funcs.ready();
    }
    else {
        return false;
    }
}

static bool dap_load(void* ptr, int size) {
    if (state.inited && state.funcs.load && ptr && ((size_t)size > sizeof(webapi_fileheader_t))) {
        LOG_INFO(DAP_INFO, "load(%p, %d) called", ptr, size);
        const webapi_fileheader_t* hdr = (webapi_fileheader_t*)ptr;
        if ((hdr->magic[0] != 'C') || (hdr->magic[1] != 'H') || (hdr->magic[2] != 'I') || (hdr->magic[3] != 'P')) {
            return false;
        }
        return state.funcs.load((chips_range_t){ .ptr = ptr, .size = (size_t)size });
    }
    return false;
}

static bool dap_load_file_internal(char* file) {
    if (state.funcs.load_file != NULL) {
        LOG_INFO(DAP_INFO, "load_file(%s) called", file);
        return state.funcs.load_file(file);
    }
    else {
        return false;
    }
}

static bool dap_unload_file() {
    if (state.funcs.unload_file != NULL) {
        LOG_INFO(DAP_INFO, "unload_file() called");
        return state.funcs.unload_file();
    }
    else {
        return false;
    }
}

static bool dap_load_snapshot(size_t index) {
    if (state.funcs.load_snapshot != NULL) {
        LOG_INFO(DAP_INFO, "load_snapshot(%zu) called", index);
        return state.funcs.load_snapshot(index);
    }
    else {
        return false;
    }
}

static void dap_save_snapshot(size_t index) {
    if (state.funcs.save_snapshot != NULL) {
        LOG_INFO(DAP_INFO, "save_snapshot(%zu) called", index);
        state.funcs.save_snapshot(index);
    }
}

static void dap_dbg_add_breakpoint(uint32_t addr) {
    if (state.inited && state.funcs.dbg_add_breakpoint) {
        LOG_INFO(DAP_INFO, "dap_dbg_add_breakpoint(%d) called", addr);
        state.funcs.dbg_add_breakpoint(addr);
    }
}

static void dap_dbg_remove_breakpoint(uint32_t addr) {
    if (state.inited && state.funcs.dbg_remove_breakpoint) {
        LOG_INFO(DAP_INFO, "dap_dbg_remove_breakpoint(%d) called", addr);
        state.funcs.dbg_remove_breakpoint(addr);
    }
}

static void dap_dbg_break(void) {
    if (state.inited && state.funcs.dbg_break) {
        LOG_INFO(DAP_INFO, "dbg_break() called");
        state.funcs.dbg_break();
    }
}

static void dap_dbg_continue(void) {
    if (state.inited && state.funcs.dbg_continue) {
        LOG_INFO(DAP_INFO, "dbg_continue() called");
        state.funcs.dbg_continue();
    }
}

static void dap_dbg_step_next(void) {
    if (state.inited && state.funcs.dbg_step_next) {
        LOG_INFO(DAP_INFO, "dbg_step_next() called");
        state.funcs.dbg_step_next();
    }
}

static void dap_dbg_step_into(void) {
    if (state.inited && state.funcs.dbg_step_into) {
        LOG_INFO(DAP_INFO, "dbg_step_into() called");
        state.funcs.dbg_step_into();
    }
}

// return emulator state as JSON-formatted string pointer into WASM heap
static uint16_t* dap_dbg_cpu_state(void) {
    static webapi_cpu_state_t res;
    if (state.inited && state.funcs.dbg_cpu_state) {
        LOG_INFO(DAP_INFO, "dbg_cpu_state() called");
        res = state.funcs.dbg_cpu_state();
    }
    else {
        memset(&res, 0, sizeof(res));
    }
    return &res.items[0];
}

// request a disassembly, returns ptr to heap-allocated array of 'num_lines' webapi_dasm_line_t structs which must be
// freed with dap_free() NOTE: may return 0!
static webapi_dasm_line_t* dap_dbg_request_disassembly(uint32_t addr, int offset_lines, int num_lines) {
    if (num_lines <= 0) {
        return 0;
    }
    if (state.inited && state.funcs.dbg_request_disassembly) {
        webapi_dasm_line_t* out_lines = (webapi_dasm_line_t*)calloc((size_t)num_lines, sizeof(webapi_dasm_line_t));
        LOG_INFO(DAP_INFO, "dbg_request_disassembly() called");
        state.funcs.dbg_request_disassembly(addr, offset_lines, num_lines, out_lines);
        return out_lines;
    }
    else {
        return 0;
    }
}

// reads a memory chunk, returns heap-allocated buffer which must be freed with dap_free()
// NOTE: may return 0!
static uint8_t* dap_dbg_read_memory(uint32_t addr, int num_bytes) {
    if (state.inited && state.funcs.dbg_read_memory) {
        uint8_t* ptr = (uint8_t*)calloc((size_t)num_bytes, 1);
        LOG_INFO(DAP_INFO, "dbg_read_memory() called");
        state.funcs.dbg_read_memory(addr, num_bytes, ptr);
        return ptr;
    }
    else {
        return 0;
    }
}

static bool dap_input_internal(char* text) {
    if (state.funcs.input != NULL && text != NULL) {
        LOG_INFO(DAP_INFO, "input() called");
        state.funcs.input(text);
        return true;
    }
    else {
        return false;
    }
}

// ---------------------------------------------------------------------------

// stop_reason is UI_DBG_STOP_REASON_xxx
void dap_event_stopped(int stop_reason, uint32_t addr) {
    LOG_INFO(DAP_INFO, "dap_event_stopped(stop_reason=%d, addr=%04x) called", stop_reason, addr);

    if (state.session) {
        auto sess = static_cast<dap::Session*>(state.session);

        // Notify UI about stopped event
        dap::StoppedEvent stoppedEvent;
        switch (stop_reason) {
            case UI_DBG_STOP_REASON_UNKNOWN: stoppedEvent.reason = "unknown"; break;
            case UI_DBG_STOP_REASON_BREAK: stoppedEvent.reason = "pause"; break;
            case UI_DBG_STOP_REASON_BREAKPOINT:
                stoppedEvent.hitBreakpointIds = dap::array<dap::integer>{ addr };
                stoppedEvent.reason = "breakpoint";
                break;
            case UI_DBG_STOP_REASON_STEP: stoppedEvent.reason = "step"; break;
        }
        stoppedEvent.threadId = threadId;
        stoppedEvent.allThreadsStopped = true;
        sess->send(stoppedEvent);
    }
}

void dap_event_continued(void) {
    LOG_INFO(DAP_INFO, "dap_event_continued() called");

#if defined(__EMSCRIPTEN__)
    dap_js_event_continued();
#endif
}

void dap_event_reboot(void) {
    LOG_INFO(DAP_INFO, "dap_event_reboot() called");

#if defined(__EMSCRIPTEN__)
    dap_js_event_reboot();
#endif
}

void dap_event_reset(void) {
    LOG_INFO(DAP_INFO, "dap_event_reset() called");

#if defined(__EMSCRIPTEN__)
    dap_js_event_reset();
#endif
}

// ---------------------------------------------------------------------------

static bool run_dap_boot = false;

std::mutex dap_breakpoints_update_mutex;
static std::map<dap::integer, std::vector<uint32_t>> dap_breakpoints = {};
static std::map<dap::integer, std::vector<uint32_t>> dap_breakpoints_update = {};

void dap_register_session(dap::Session* session) {
    // Handle errors reported by the Session. These errors include protocol
    // parsing errors and receiving messages with no handler.
    session->onError([&](const char* msg) {
        LOG_ERROR(DAP_SESSION_ERROR, "%s", msg);
        // sapp_request_quit();
    });

    // The Initialize request is the first message sent from the client and
    // the response reports debugger capabilities.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Initialize
    session->registerHandler([&](const dap::InitializeRequest& request) {
        LOG_INFO(DAP_INFO, "Client '%s' initialize", request.clientName.value("unknown").c_str());

        run_dap_boot = true;

        dap::InitializeResponse response;
        response.supportsConfigurationDoneRequest = true;
        response.supportsEvaluateForHovers = true;
        response.supportsSetVariable = true;
        response.supportsTerminateRequest = true;
        response.supportTerminateDebuggee = true;
        response.supportsRestartRequest = true;
        response.supportSuspendDebuggee = true;
        response.supportsValueFormattingOptions = true;
        response.supportsReadMemoryRequest = true;
        response.supportsWriteMemoryRequest = false;
        response.supportsDisassembleRequest = true;
        response.supportsStepInTargetsRequest = true;
        return response;
    });

    // The Launch request is made when the client instructs the debugger adapter
    // to start the debuggee. This request contains the launch arguments.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Launch
    session->registerHandler([&](const dap::LaunchRequest& request) {
        LOG_INFO(DAP_INFO, "Launch request")

        if (request.noDebug.value(false)) {
            LOG_INFO(DAP_INFO, "Launching debuggee without debugging");
            state.enabled = false;
        }

        dap_dbg_connect();

        return dap::LaunchResponse();
    });

    // Handler for disconnect requests
    session->registerHandler([&](const dap::DisconnectRequest& request) {
        LOG_INFO(DAP_INFO, "Disconnect request");

        if (request.terminateDebuggee.value(false)) {
            LOG_INFO(DAP_INFO, "Terminating debuggee");
            sapp_request_quit();
        }
        else {
            dap_dbg_disconnect();
        }

        return dap::DisconnectResponse();
    });

    // Handler for terminate requests
    session->registerHandler([&](const dap::TerminateRequest& request) {
        const bool restart = request.restart.value(false);
        LOG_INFO(DAP_INFO, "%s request", restart ? "Restart" : "Terminate");
        if (restart) {
            dap_reset();
        }
        else {
            sapp_request_quit();
        }
        return dap::TerminateResponse();
    });

    // The ConfigurationDone request is made by the client once all configuration
    // requests have been made.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_ConfigurationDone
    session->registerHandler([&](const dap::ConfigurationDoneRequest&) {
        dap_ready();
        return dap::ConfigurationDoneResponse();
    });

    // The SetBreakpoints request instructs the debugger to clear and set a number
    // of line breakpoints for a specific source file.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_SetBreakpoints
    session->registerHandler(
        [&](const dap::SetBreakpointsRequest& request) -> dap::ResponseOrError<dap::SetBreakpointsResponse> {
            dap::SetBreakpointsResponse response;

            if (!request.lines.has_value()) {
                LOG_ERROR(DAP_SESSION_ERROR, "Lines not provided in SetBreakpointsRequest");
                return dap::Error("Lines not provided in SetBreakpointsRequest");
            }

            auto breakpoints = request.lines.value();
            response.breakpoints.resize(breakpoints.size());

            if (!request.source.sourceReference.has_value()) {
                LOG_ERROR(DAP_SESSION_ERROR, "Source reference not provided in SetBreakpointsRequest");

                for (size_t i = 0; i < breakpoints.size(); i++) {
                    response.breakpoints[i].verified = false;
                    response.breakpoints[i].reason = "failed";
                    response.breakpoints[i].message = "Source reference not provided in SetBreakpointsRequest";
                }
                return response;
            }

            dap::integer source = request.source.sourceReference.value();

            if (dap_breakpoints_update.find(source) != dap_breakpoints_update.end()) {
                LOG_ERROR(DAP_SESSION_ERROR, "SetBreakpointsRequest is coming too fast");
                return dap::Error("SetBreakpointsRequest is coming too fast");
            }

            {
                std::lock_guard<std::mutex> lock(dap_breakpoints_update_mutex);

                dap_breakpoints_update[source] = {};
                for (size_t i = 0; i < breakpoints.size(); i++) {
                    dap::integer address = breakpoints[i];
                    bool valid = address >= 0 && address < EMU_RAM_SIZE;
                    response.breakpoints[i].verified = valid;
                    if (valid) {
                        response.breakpoints[i].id = address;
                        char buffer[20];
                        sprintf(buffer, "$%06lX", address & 0xFFFFFF);
                        response.breakpoints[i].message = buffer;

                        dap_breakpoints_update[source].push_back((uint32_t)address);
                    }
                }
            }
            return response;
        });

    // The SetInstructionBreakpoints request instructs the debugger to clear and set a number
    // of instruction breakpoints.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_SetInstructionBreakpoints
    session->registerHandler(
        [&](const dap::SetInstructionBreakpointsRequest&)
            -> dap::ResponseOrError<dap::SetInstructionBreakpointsResponse> {
            return dap::Error("SetInstructionBreakpoints not implemented yet");
        });

    // The Threads request queries the debugger's list of active threads.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Threads
    session->registerHandler([&](const dap::ThreadsRequest&) {
        dap::ThreadsResponse response;
        dap::Thread thread;
        thread.id = threadId;
        thread.name = "CPU";
        response.threads.push_back(thread);
        return response;
    });

    // The StackTrace request reports the stack frames (call stack) for a given
    // thread. This debugger only exposes a single stack frame for the
    // single thread.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StackTrace
    session->registerHandler(
        [&](const dap::StackTraceRequest& request) -> dap::ResponseOrError<dap::StackTraceResponse> {
            if (request.threadId != threadId) {
                return dap::Error("Unknown threadId '%d'", int(request.threadId));
            }

            webapi_cpu_state_t cpu_state = state.funcs.dbg_cpu_state();
            assert(cpu_state.items[WEBAPI_CPUSTATE_TYPE] == WEBAPI_CPUTYPE_65816);

            dap::integer address =
                (cpu_state.items[WEBAPI_CPUSTATE_65816_PBR] << 16) | cpu_state.items[WEBAPI_CPUSTATE_65816_PC];

            char buffer[20];
            sprintf(buffer, "$%06lX", address & 0xFFFFFF);

            dap::StackFrame frame;
            frame.name = buffer;
            frame.id = address;
            frame.line = address;
            /// If attribute `source` is missing or doesn't exist, `column` is 0 and should be ignored by the client.
            frame.column = 0;

            dap::StackTraceResponse response;
            response.stackFrames.push_back(frame);
            return response;
        });

    // The Scopes request reports all the scopes of the given stack frame.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Scopes
    session->registerHandler([&](const dap::ScopesRequest& request) -> dap::ResponseOrError<dap::ScopesResponse> {
        if (request.frameId > 0xFFFFFF) {
            return dap::Error("Unknown frameId '%d'", int(request.frameId));
        }

        dap::Scope scope;
        scope.name = "Registers";
        scope.presentationHint = "registers";
        scope.variablesReference = registersVariablesReferenceId;

        dap::ScopesResponse response;
        response.scopes.push_back(scope);
        return response;
    });

    // The Variables request reports all the variables for the given scope.
    // This example debugger only exposes a single 'currentLine' variable for the
    // single 'Locals' scope.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Variables
    session->registerHandler([&](const dap::VariablesRequest& request) -> dap::ResponseOrError<dap::VariablesResponse> {
        char buffer[20];
        webapi_cpu_state_t cpu_state = state.funcs.dbg_cpu_state();
        assert(cpu_state.items[WEBAPI_CPUSTATE_TYPE] == WEBAPI_CPUTYPE_65816);

        dap::VariablesResponse response;

        dap::VariablePresentationHint regHint;
        regHint.kind = "property";

        dap::Variable regVar;
        regVar.type = "register";
        regVar.presentationHint = regHint;
        regVar.variablesReference = 0;

        switch (request.variablesReference) {
            case registersVariablesReferenceId: {
                regVar.name = "C";
                regVar.evaluateName = "reg.C";
                sprintf(buffer, "$%04X", cpu_state.items[WEBAPI_CPUSTATE_65816_C]);
                regVar.value = buffer;
                regVar.variablesReference = registerCVariablesReferenceId;
                response.variables.push_back(regVar);
                regVar.variablesReference = 0;

                regVar.name = "X";
                regVar.evaluateName = "reg.X";
                sprintf(buffer, "$%04X", cpu_state.items[WEBAPI_CPUSTATE_65816_X]);
                regVar.value = buffer;
                response.variables.push_back(regVar);

                regVar.name = "Y";
                regVar.evaluateName = "reg.Y";
                sprintf(buffer, "$%04X", cpu_state.items[WEBAPI_CPUSTATE_65816_Y]);
                regVar.value = buffer;
                response.variables.push_back(regVar);

                regVar.name = "S";
                regVar.evaluateName = "reg.S";
                sprintf(buffer, "$%04X", cpu_state.items[WEBAPI_CPUSTATE_65816_S]);
                regVar.value = buffer;
                response.variables.push_back(regVar);

                regVar.name = "PC";
                regVar.evaluateName = "reg.PC";
                sprintf(buffer, "$%04X", cpu_state.items[WEBAPI_CPUSTATE_65816_PC]);
                regVar.value = buffer;
                response.variables.push_back(regVar);

                const uint8_t f = cpu_state.items[WEBAPI_CPUSTATE_65816_P] & 0xFF;
                bool emulation = cpu_state.items[WEBAPI_CPUSTATE_65816_E];
                char f_str[9] = { (f & W65816_NF) ? 'N' : '-',
                                  (f & W65816_VF) ? 'V' : '-',
                                  emulation ? ((f & W65816_UF) ? '1' : '-') : ((f & W65816_MF) ? 'M' : '-'),
                                  emulation ? ((f & W65816_BF) ? 'B' : '-') : ((f & W65816_XF) ? 'X' : '-'),
                                  (f & W65816_DF) ? 'D' : '-',
                                  (f & W65816_IF) ? 'I' : '-',
                                  (f & W65816_ZF) ? 'Z' : '-',
                                  (f & W65816_CF) ? 'C' : '-',
                                  0 };
                regVar.name = "P";
                regVar.evaluateName = "reg.P";
                regVar.value = f_str;
                regVar.variablesReference = registerPVariablesReferenceId;
                response.variables.push_back(regVar);
                regVar.variablesReference = 0;

                regVar.name = "D";
                regVar.evaluateName = "reg.D";
                sprintf(buffer, "$%04X", cpu_state.items[WEBAPI_CPUSTATE_65816_D]);
                regVar.value = buffer;
                response.variables.push_back(regVar);

                regVar.name = "DBR";
                regVar.evaluateName = "reg.DBR";
                sprintf(buffer, "$%02X", cpu_state.items[WEBAPI_CPUSTATE_65816_DBR]);
                regVar.value = buffer;
                response.variables.push_back(regVar);

                regVar.name = "PBR";
                regVar.evaluateName = "reg.PBR";
                sprintf(buffer, "$%02X", cpu_state.items[WEBAPI_CPUSTATE_65816_PBR]);
                regVar.value = buffer;
                response.variables.push_back(regVar);

                break;
            }
            case registerCVariablesReferenceId: {
                regVar.name = "A";
                regVar.evaluateName = "reg.A";
                sprintf(buffer, "$%02X", cpu_state.items[WEBAPI_CPUSTATE_65816_C] & 0xFF);
                regVar.value = buffer;
                response.variables.push_back(regVar);

                regVar.name = "B";
                regVar.evaluateName = "reg.B";
                sprintf(buffer, "$%02X", (cpu_state.items[WEBAPI_CPUSTATE_65816_C] >> 8) & 0xFF);
                regVar.value = buffer;
                response.variables.push_back(regVar);

                break;
            }
            case registerPVariablesReferenceId: {
                const uint8_t flags = cpu_state.items[WEBAPI_CPUSTATE_65816_P] & 0xFF;
                bool emulation = cpu_state.items[WEBAPI_CPUSTATE_65816_E];

                regVar.name = "Carry";
                regVar.evaluateName = "flag.C";
                regVar.value = (flags & W65816_CF) ? '1' : '0';
                response.variables.push_back(regVar);

                regVar.name = "Zero";
                regVar.evaluateName = "flag.Z";
                regVar.value = (flags & W65816_ZF) ? '1' : '0';
                response.variables.push_back(regVar);

                regVar.name = "IRQ disable";
                regVar.evaluateName = "flag.I";
                regVar.value = (flags & W65816_IF) ? '1' : '0';
                response.variables.push_back(regVar);

                regVar.name = "Decimal";
                regVar.evaluateName = "flag.D";
                regVar.value = (flags & W65816_DF) ? '1' : '0';
                response.variables.push_back(regVar);

                if (emulation) {
                    regVar.name = "Break";
                    regVar.evaluateName = "flag.B";
                    regVar.value = (flags & W65816_BF) ? '1' : '0';
                    response.variables.push_back(regVar);
                }
                else {
                    regVar.name = "indeX";
                    regVar.evaluateName = "flag.X";
                    regVar.value = (flags & W65816_XF) ? '1' : '0';
                    response.variables.push_back(regVar);

                    regVar.name = "Memory/Accumulator";
                    regVar.evaluateName = "flag.M";
                    regVar.value = (flags & W65816_MF) ? '1' : '0';
                    response.variables.push_back(regVar);
                }

                regVar.name = "oVerflow";
                regVar.evaluateName = "flag.V";
                regVar.value = (flags & W65816_VF) ? '1' : '0';
                response.variables.push_back(regVar);

                regVar.name = "Zero";
                regVar.evaluateName = "flag.N";
                regVar.value = (flags & W65816_NF) ? '1' : '0';
                response.variables.push_back(regVar);

                break;
            }
            default: {
                return dap::Error("Unknown variablesReference '%d'", int(request.variablesReference));
            }
        }

        return response;
    });
}

void dap_init(const dap_desc_t* desc) {
    assert(desc);
    state.enabled = true;
    state.stdio = desc->stdio;
    state.port = desc->port;
    state.funcs = desc->funcs;

#ifdef LOG_TO_FILE
    log = dap::file(LOG_TO_FILE);
#endif

    if (state.stdio) {
#ifdef OS_WINDOWS
        // Change stdin & stdout from text mode to binary mode.
        // This ensures sequences of \r\n are not changed to \n.
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif  // OS_WINDOWS
    }

    if (state.stdio) {
        auto session = dap::Session::create();

        dap_register_session(session.get());

        // We now bind the session to stdin and stdout to connect to the client.
        // After the call to bind() we should start receiving requests, starting with
        // the Initialize request.
        std::shared_ptr<dap::Reader> in = dap::file(stdin, false);
        std::shared_ptr<dap::Writer> out = dap::file(stdout, false);
        if (log) {
            session->bind(spy(in, log), spy(out, log));
        }
        else {
            session->bind(in, out);
        }

        state.session = session.release();
    }
    if (state.port) {
        int kPort = atoi(state.port);

        // Callback handler for a socket connection to the server
        auto onClientConnected = [&](const std::shared_ptr<dap::ReaderWriter>& socket) {
            auto session = dap::Session::create();

            // Set the session to close on invalid data. This ensures that data received over the network
            // receives a baseline level of validation before being processed.
            session->setOnInvalidData(dap::kClose);

            dap_register_session(session.get());

            session->bind(socket);

            // Signal used to terminate the server session when a DisconnectRequest
            // is made by the client.
            bool terminate = false;
            std::condition_variable cv;
            std::mutex mutex;  // guards 'terminate'

            // The Disconnect request is made by the client before it disconnects
            // from the server.
            // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Disconnect
            session->registerHandler([&](const dap::DisconnectRequest&) {
                // Client wants to disconnect. Set terminate to true, and signal the
                // condition variable to unblock the server thread.
                std::unique_lock<std::mutex> lock(mutex);
                terminate = true;
                cv.notify_one();
                return dap::DisconnectResponse{};
            });

            // Wait for the client to disconnect (or reach a 5 second timeout)
            // before releasing the session and disconnecting the socket to the
            // client.
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait_for(lock, std::chrono::seconds(5), [&] { return terminate; });
            LOG_INFO(DAP_INFO, "Server closing connection");
        };

        // Error handler
        auto onError = [&](const char* msg) { LOG_ERROR(DAP_NETWORK_ERROR, "%s", msg); };

        // Create the network server
        auto server = dap::net::Server::create();
        // Start listening on kPort.
        // onClientConnected will be called when a client wants to connect.
        // onError will be called on any connection errors.
        server->start(kPort, onClientConnected, onError);

        LOG_INFO(DAP_NETWORK, "dap::network listening on port %d", kPort);

        state.server = server.release();
    }
}

void dap_shutdown() {
#ifdef LOG_TO_FILE
    log->close();
#endif

#ifndef _WIN32
    if (state.stdio) {
        // DAP server might be waiting for stdin in fgetc() and will not join reading thread
        // Kill the whole process, as we are going down anyway…
        kill(getpid(), SIGTERM);
    }
#endif
}

/**
 * @brief Processes the Debug Adapter Protocol (DAP) requests.
 *
 * This function is being called from main frame handler thread.
 * It is used to safely cross DAP server and Emu thread boundary.
 */
void dap_process() {
    if (!state.inited) {
        state.inited = true;
    }

    if (run_dap_boot) {
        run_dap_boot = false;
        dap_boot();

        // https://microsoft.github.io/debug-adapter-protocol/specification#Events_Initialized
        if (state.session) {
            auto sess = static_cast<dap::Session*>(state.session);
            sess->send(dap::InitializedEvent());

            // Broadcast the existence of the single thread to the client.
            dap::ThreadEvent threadStartedEvent;
            threadStartedEvent.reason = "started";
            threadStartedEvent.threadId = threadId;
            sess->send(threadStartedEvent);
        }
    }

    while (!dap_breakpoints_update.empty()) {
        dap::integer source;
        std::vector<uint32_t> add_addresses;
        std::vector<uint32_t> remove_addresses;
        {
            std::lock_guard<std::mutex> lock(dap_breakpoints_update_mutex);
            source = dap_breakpoints_update.begin()->first;
            add_addresses = std::move(dap_breakpoints_update.begin()->second);
            dap_breakpoints_update.erase(dap_breakpoints_update.begin());
            remove_addresses = std::move(dap_breakpoints.find(source)->second);
            dap_breakpoints[source] = add_addresses;
        }
        for (auto address : remove_addresses) {
            dap_dbg_remove_breakpoint(address);
        }
        for (auto address : add_addresses) {
            dap_dbg_add_breakpoint(address);
        }
    }
}
