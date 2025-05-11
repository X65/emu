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

/// Store address the debugger stopped, to generate stacktrace later.
/// We cannot just take the PC from the CPU, as the debugger stops AFTER
/// the instruction has been executed and PC points to the next instruction.
int dap_event_stopped_addr = -1;

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

static bool dap_is_ready(void) {
    if (state.inited && state.funcs.ready) {
        // LOG_INFO(DAP_INFO, "ready() called");
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
    dap_event_stopped_addr = -1;

    if (state.inited && state.funcs.dbg_continue) {
        LOG_INFO(DAP_INFO, "dbg_continue() called");
        state.funcs.dbg_continue();
    }
}

static void dap_dbg_step_next(void) {
    dap_event_stopped_addr = -1;

    if (state.inited && state.funcs.dbg_step_next) {
        LOG_INFO(DAP_INFO, "dbg_step_next() called");
        state.funcs.dbg_step_next();
    }
}

static void dap_dbg_step_into(void) {
    dap_event_stopped_addr = -1;

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

/// request a disassembly, returns ptr to heap-allocated array of 'num_lines' webapi_dasm_line_t structs which must be
/// freed with dap_free() NOTE: may return 0!
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

    dap_event_stopped_addr = addr;

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

    dap_event_stopped_addr = -1;

    if (state.session) {
        auto sess = static_cast<dap::Session*>(state.session);

        // Notify UI about continued event
        dap::ContinuedEvent continuedEvent;
        continuedEvent.threadId = threadId;
        continuedEvent.allThreadsContinued = true;
        sess->send(continuedEvent);
    }
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

static bool do_dap_boot = false;
static bool do_send_thread_info = false;
static bool do_dap_reset = false;
static bool do_dap_pause = false;
static bool do_dap_continue = false;
static bool do_dap_stepForward = false;
static bool do_dap_stepIn = false;

std::mutex dap_breakpoints_update_mutex;
static std::map<dap::integer, std::vector<uint32_t>> dap_breakpoints = {};
static std::map<dap::integer, std::vector<uint32_t>> dap_breakpoints_update = {};

enum CPURegisterByte { CPU_REG_BYTE_BOTH, CPU_REG_BYTE_LOW, CPU_REG_BYTE_HIGH };

static dap::Variable evaluateCPURegister(uint8_t registerId, CPURegisterByte byte = CPU_REG_BYTE_BOTH) {
    assert(registerId < WEBAPI_CPUSTATE_MAX);
    webapi_cpu_state_t cpu_state = state.funcs.dbg_cpu_state();
    assert(cpu_state.items[WEBAPI_CPUSTATE_TYPE] == WEBAPI_CPUTYPE_65816);

    dap::VariablePresentationHint regHint;
    regHint.kind = "property";

    dap::Variable regVar;
    regVar.type = "register";
    regVar.presentationHint = regHint;
    regVar.variablesReference = 0;

    char buffer[20];
    switch (byte) {
        case CPU_REG_BYTE_BOTH: sprintf(buffer, "$%04X", cpu_state.items[registerId]); break;
        case CPU_REG_BYTE_LOW: sprintf(buffer, "$%02X", cpu_state.items[registerId] & 0xFF); break;
        case CPU_REG_BYTE_HIGH: sprintf(buffer, "$%02X", (cpu_state.items[registerId] >> 8) & 0xFF); break;
        default: assert(false);
    }
    regVar.value = buffer;

    return regVar;
}

static dap::Variable evaluateCPUFlag(uint8_t flagId) {
    webapi_cpu_state_t cpu_state = state.funcs.dbg_cpu_state();
    assert(cpu_state.items[WEBAPI_CPUSTATE_TYPE] == WEBAPI_CPUTYPE_65816);

    dap::VariablePresentationHint regHint;
    regHint.kind = "property";

    dap::Variable regVar;
    regVar.type = "register";
    regVar.presentationHint = regHint;
    regVar.variablesReference = 0;

    regVar.value = (cpu_state.items[WEBAPI_CPUSTATE_65816_P] & flagId) ? '1' : '0';
    return regVar;
}

static auto variableEvaluators = std::map<std::string, std::function<dap::Variable(const std::string&)>>{
    { "reg.C",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPURegister(WEBAPI_CPUSTATE_65816_C);
          regVar.name = "C";
          regVar.variablesReference = registerCVariablesReferenceId;
          return regVar;
      } },
    { "reg.A",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPURegister(WEBAPI_CPUSTATE_65816_C, CPU_REG_BYTE_LOW);
          regVar.name = "A";
          return regVar;
      } },
    { "reg.B",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPURegister(WEBAPI_CPUSTATE_65816_C, CPU_REG_BYTE_HIGH);
          regVar.name = "B";
          return regVar;
      } },
    { "reg.X",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPURegister(WEBAPI_CPUSTATE_65816_X);
          regVar.name = "X";
          return regVar;
      } },

    { "reg.Y",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPURegister(WEBAPI_CPUSTATE_65816_Y);
          regVar.name = "Y";
          return regVar;
      } },

    { "reg.S",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPURegister(WEBAPI_CPUSTATE_65816_S);
          regVar.name = "S";
          return regVar;
      } },

    { "reg.PC",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPURegister(WEBAPI_CPUSTATE_65816_PC);
          regVar.name = "PC";
          return regVar;
      } },

    { "reg.D",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPURegister(WEBAPI_CPUSTATE_65816_D);
          regVar.name = "D";
          return regVar;
      } },

    { "reg.DBR",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPURegister(WEBAPI_CPUSTATE_65816_DBR, CPU_REG_BYTE_LOW);
          regVar.name = "DBR";
          return regVar;
      } },

    { "reg.P",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPURegister(WEBAPI_CPUSTATE_65816_P, CPU_REG_BYTE_LOW);
          regVar.name = "P";

          webapi_cpu_state_t cpu_state = state.funcs.dbg_cpu_state();
          assert(cpu_state.items[WEBAPI_CPUSTATE_TYPE] == WEBAPI_CPUTYPE_65816);

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
          regVar.value = f_str;
          regVar.variablesReference = registerPVariablesReferenceId;

          return regVar;
      } },

    { "reg.PBR",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPURegister(WEBAPI_CPUSTATE_65816_PBR, CPU_REG_BYTE_LOW);
          regVar.name = "PBR";
          return regVar;
      } },

    { "flag.C",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPUFlag(W65816_CF);
          regVar.name = "Carry";
          return regVar;
      } },
    { "flag.Z",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPUFlag(W65816_ZF);
          regVar.name = "Zero";
          return regVar;
      } },
    { "flag.I",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPUFlag(W65816_IF);
          regVar.name = "IRQ disable";
          return regVar;
      } },
    { "flag.D",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPUFlag(W65816_DF);
          regVar.name = "Decimal";
          return regVar;
      } },
    { "flag.B",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPUFlag(W65816_BF);
          regVar.name = "Break";
          return regVar;
      } },
    { "flag.X",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPUFlag(W65816_XF);
          regVar.name = "indeX";
          return regVar;
      } },
    { "flag.M",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPUFlag(W65816_MF);
          regVar.name = "Memory/Accumulator";
          return regVar;
      } },
    { "flag.V",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPUFlag(W65816_VF);
          regVar.name = "oVerflow";
          return regVar;
      } },
    { "flag.N",
     [](const std::string&) -> dap::Variable {
          dap::Variable regVar = evaluateCPUFlag(W65816_NF);
          regVar.name = "Negative";
          return regVar;
      } },
};

static dap::Variable evaluateVariable(const std::string& reference) {
    dap::Variable var;
    auto it = variableEvaluators.find(reference);
    if (it != variableEvaluators.end()) {
        var = it->second(reference);
    }
    var.evaluateName = reference;
    return var;
}

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

        do_dap_boot = true;

        dap::InitializeResponse response;
        response.supportTerminateDebuggee = true;
        response.supportSuspendDebuggee = true;
        response.supportsCompletionsRequest = true;
        response.supportsConfigurationDoneRequest = true;
        response.supportsDataBreakpoints = true;
        response.supportsDisassembleRequest = true;
        response.supportsEvaluateForHovers = false;
        response.supportsInstructionBreakpoints = true;
        response.supportsReadMemoryRequest = true;
        response.supportsRestartRequest = true;
        response.supportsSetVariable = true;
        response.supportsStepBack = false;
        response.supportsTerminateRequest = true;
        response.supportsWriteMemoryRequest = false;
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
            do_dap_reset = true;
        }
        else {
            sapp_request_quit();
        }
        return dap::TerminateResponse();
    });

    // Handler for restarts requests
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Restart
    session->registerHandler([&](const dap::RestartRequest&) {
        LOG_INFO(DAP_INFO, "Restart request");
        do_dap_reset = true;
        return dap::RestartResponse();
    });

    // The ConfigurationDone request is made by the client once all configuration
    // requests have been made.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_ConfigurationDone
    session->registerHandler([&](const dap::ConfigurationDoneRequest&) {
        dap_dbg_continue();  // Continue the debuggee
        return dap::ConfigurationDoneResponse();
    });

    // The ConfigurationDone request is made by the client once all configuration
    // requests have been made.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_ConfigurationDone
    session->registerHandler([&](const dap::SourceRequest&) -> dap::ResponseOrError<dap::SourceResponse> {
        return dap::Error("'source' request is not supported. The source is gone. It was never yours to see.");
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

            dap::integer address = dap_event_stopped_addr >= 0
                ? dap_event_stopped_addr
                : (cpu_state.items[WEBAPI_CPUSTATE_65816_PBR] << 16) | cpu_state.items[WEBAPI_CPUSTATE_65816_PC];

            char buffer[64];
            char* buf = buffer + sprintf(buffer, "%02lX %04lX", (address >> 16) & 0xFF, address & 0xFFFF);

            webapi_dasm_line_t* dasm = dap_dbg_request_disassembly(address & 0xFFFFFF, 0, 1);
            if (dasm) {
                *buf++ = ' ';
                memcpy(buf, dasm->chars, dasm->num_chars);
                buf[dasm->num_chars] = '\0';
            }

            dap::StackFrame frame;
            frame.name = buffer;
            frame.id = address;
            frame.line = address;
            /// If attribute `source` is missing or doesn't exist, `column` is 0 and should be ignored by the client.
            frame.column = 0;

            free(dasm);

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
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Variables
    session->registerHandler([&](const dap::VariablesRequest& request) -> dap::ResponseOrError<dap::VariablesResponse> {
        dap::VariablesResponse response;

        switch (request.variablesReference) {
            case registersVariablesReferenceId: {
                response.variables.push_back(evaluateVariable("reg.C"));
                response.variables.push_back(evaluateVariable("reg.X"));
                response.variables.push_back(evaluateVariable("reg.Y"));
                response.variables.push_back(evaluateVariable("reg.S"));
                response.variables.push_back(evaluateVariable("reg.PC"));
                response.variables.push_back(evaluateVariable("reg.P"));
                response.variables.push_back(evaluateVariable("reg.D"));
                response.variables.push_back(evaluateVariable("reg.DBR"));
                response.variables.push_back(evaluateVariable("reg.PBR"));
                break;
            }
            case registerCVariablesReferenceId: {
                response.variables.push_back(evaluateVariable("reg.A"));
                response.variables.push_back(evaluateVariable("reg.B"));
                break;
            }
            case registerPVariablesReferenceId: {
                webapi_cpu_state_t cpu_state = state.funcs.dbg_cpu_state();
                assert(cpu_state.items[WEBAPI_CPUSTATE_TYPE] == WEBAPI_CPUTYPE_65816);

                response.variables.push_back(evaluateVariable("flag.C"));
                response.variables.push_back(evaluateVariable("flag.Z"));
                response.variables.push_back(evaluateVariable("flag.I"));
                response.variables.push_back(evaluateVariable("flag.D"));
                if (cpu_state.items[WEBAPI_CPUSTATE_65816_E]) {
                    response.variables.push_back(evaluateVariable("flag.B"));
                }
                else {
                    response.variables.push_back(evaluateVariable("flag.X"));
                    response.variables.push_back(evaluateVariable("flag.M"));
                }
                response.variables.push_back(evaluateVariable("flag.V"));
                response.variables.push_back(evaluateVariable("flag.N"));
                break;
            }
            default: {
                return dap::Error("Unknown variablesReference '%d'", int(request.variablesReference));
            }
        }

        return response;
    });

    // Evaluates the given expression in the context of a stack frame.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Evaluate
    session->registerHandler([&](const dap::EvaluateRequest& request) -> dap::ResponseOrError<dap::EvaluateResponse> {
        dap::EvaluateResponse response;

        auto evaluator = variableEvaluators.find(request.expression);
        if (evaluator != variableEvaluators.end()) {
            auto val = evaluator->second(request.expression);
            response.result = val.value;
            response.type = val.type;
            response.variablesReference = val.variablesReference;
            response.presentationHint = val.presentationHint;
            return response;
        }

        return dap::Error("Unknown expression");
    });

    // The Pause request instructs the debugger to pause execution of one or all
    // threads.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Pause
    session->registerHandler([&](const dap::PauseRequest&) {
        do_dap_pause = true;
        return dap::PauseResponse();
    });

    // The Continue request instructs the debugger to resume execution of one or
    // all threads.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Continue
    session->registerHandler([&](const dap::ContinueRequest&) {
        do_dap_continue = true;
        return dap::ContinueResponse();
    });

    // The Next request instructs the debugger to single line step for a specific
    // thread.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Next
    session->registerHandler([&](const dap::NextRequest&) {
        do_dap_stepForward = true;
        return dap::NextResponse();
    });

    // The StepIn request instructs the debugger to step-in for a specific thread.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StepIn
    session->registerHandler([&](const dap::StepInRequest&) {
        do_dap_stepIn = true;
        return dap::StepInResponse();
    });

    // The StepOut request instructs the debugger to step-out for a specific
    // thread.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StepOut
    session->registerHandler([&](const dap::StepOutRequest&) {
        // Step-out is not supported.
        return dap::Error("Step-out is not supported");
    });

    // The StepOut request instructs the debugger to step-out for a specific
    // thread.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StepOut
    session->registerHandler(
        [&](const dap::DisassembleRequest& request) -> dap::ResponseOrError<dap::DisassembleResponse> {
            dap::DisassembleResponse response;

            if (!request.memoryReference.starts_with("0x")) {
                return dap::Error("Invalid memory reference '%s'", request.memoryReference.c_str());
            }

            const auto address = std::stoi(request.memoryReference, nullptr, 0);

            auto instrOffset = (int)request.instructionOffset.value(0);
            auto instrLines = (int)request.instructionCount;

            response.instructions.resize(instrLines);

            char buffer[64];
            webapi_dasm_line_t* dasm = dap_dbg_request_disassembly(address & 0xFFFFFF, instrOffset, instrLines);

            for (int i = 0; i < instrLines; ++i) {
                memcpy(buffer, dasm[i].chars, dasm[i].num_chars);
                buffer[dasm[i].num_chars] = '\0';
                response.instructions[i].instruction = buffer;

                for (int j = 0; j < dasm[i].num_bytes; ++j) {
                    sprintf(buffer + 3 * j, "%02X ", dasm[i].bytes[j]);
                }
                // remove last space
                if (dasm[i].num_bytes) buffer[3 * dasm[i].num_bytes - 1] = '\0';
                response.instructions[i].instructionBytes = buffer;

                sprintf(buffer, "0x%06X", dasm[i].addr);
                response.instructions[i].address = buffer;

                response.instructions[i].presentationHint =
                    response.instructions[i].instruction == "???" ? "invalid" : "normal";
            }

            free(dasm);

            return response;
        });
}

void dap_init(const dap_desc_t* desc) {
    assert(desc);
    state.enabled = true;
    state.stdio = desc->stdio;
    state.port = desc->port;
    state.funcs = desc->funcs;
    state.inited = true;

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
    if (do_send_thread_info && dap_is_ready()) {
        do_send_thread_info = false;

        if (state.session) {
            auto sess = static_cast<dap::Session*>(state.session);
            // Broadcast the existence of the single thread to the client.
            dap::ThreadEvent threadStartedEvent;
            threadStartedEvent.reason = "started";
            threadStartedEvent.threadId = threadId;
            sess->send(threadStartedEvent);
        }
    }

    if (do_dap_boot) {
        do_dap_boot = false;

        dap_boot();
        dap_dbg_break();  // wait for configuration

        // https://microsoft.github.io/debug-adapter-protocol/specification#Events_Initialized
        if (state.session) {
            auto sess = static_cast<dap::Session*>(state.session);
            sess->send(dap::InitializedEvent());
        }

        do_send_thread_info = true;
    }

    if (do_dap_reset) {
        do_dap_reset = false;

        dap_reset();
    }

    if (do_dap_pause) {
        do_dap_pause = false;

        dap_dbg_break();
    }

    if (do_dap_continue) {
        do_dap_continue = false;

        dap_dbg_continue();
    }

    if (do_dap_stepForward) {
        do_dap_stepForward = false;

        dap_dbg_step_next();
    }

    if (do_dap_stepIn) {
        do_dap_stepIn = false;

        dap_dbg_step_into();
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
