#include "./dap.h"

#include "log.h"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <map>
#include <vector>

#include "sokol_app.h"

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

// ---------------------------------------------------------------------------

static void dap_dbg_connect(void) {
    if (state.inited) {
        if (state.funcs.dbg_connect) {
            state.funcs.dbg_connect();
        }
    }
    else {
        before_init_state.dbg_connect_requested = true;
    }
}

static void dap_dbg_disconnect(void) {
    if (state.inited && state.funcs.dbg_disconnect) {
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
        state.funcs.reset();
    }
}

static bool dap_ready(void) {
    if (state.inited && state.funcs.ready) {
        return state.funcs.ready();
    }
    else {
        return false;
    }
}

static bool dap_load(void* ptr, int size) {
    if (state.inited && state.funcs.load && ptr && ((size_t)size > sizeof(webapi_fileheader_t))) {
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
        return state.funcs.load_file(file);
    }
    else {
        return false;
    }
}

static bool dap_unload_file() {
    if (state.funcs.unload_file != NULL) {
        return state.funcs.unload_file();
    }
    else {
        return false;
    }
}

static bool dap_load_snapshot(size_t index) {
    if (state.funcs.load_snapshot != NULL) {
        return state.funcs.load_snapshot(index);
    }
    else {
        return false;
    }
}

static void dap_save_snapshot(size_t index) {
    if (state.funcs.save_snapshot != NULL) {
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
        state.funcs.dbg_break();
    }
}

static void dap_dbg_continue(void) {
    if (state.inited && state.funcs.dbg_continue) {
        state.funcs.dbg_continue();
    }
}

static void dap_dbg_step_next(void) {
    if (state.inited && state.funcs.dbg_step_next) {
        state.funcs.dbg_step_next();
    }
}

static void dap_dbg_step_into(void) {
    if (state.inited && state.funcs.dbg_step_into) {
        state.funcs.dbg_step_into();
    }
}

// return emulator state as JSON-formatted string pointer into WASM heap
static uint16_t* dap_dbg_cpu_state(void) {
    static webapi_cpu_state_t res;
    if (state.inited && state.funcs.dbg_cpu_state) {
        res = state.funcs.dbg_cpu_state();
    }
    else {
        memset(&res, 0, sizeof(res));
    }
    return &res.items[0];
}

// request a disassembly, returns ptr to heap-allocated array of 'num_lines' webapi_dasm_line_t structs which must be
// freed with dap_free() NOTE: may return 0!
static webapi_dasm_line_t* dap_dbg_request_disassembly(uint16_t addr, int offset_lines, int num_lines) {
    if (num_lines <= 0) {
        return 0;
    }
    if (state.inited && state.funcs.dbg_request_disassembly) {
        webapi_dasm_line_t* out_lines = (webapi_dasm_line_t*)calloc((size_t)num_lines, sizeof(webapi_dasm_line_t));
        state.funcs.dbg_request_disassembly(addr, offset_lines, num_lines, out_lines);
        return out_lines;
    }
    else {
        return 0;
    }
}

// reads a memory chunk, returns heap-allocated buffer which must be freed with dap_free()
// NOTE: may return 0!
static uint8_t* dap_dbg_read_memory(uint16_t addr, int num_bytes) {
    if (state.inited && state.funcs.dbg_read_memory) {
        uint8_t* ptr = (uint8_t*)calloc((size_t)num_bytes, 1);
        state.funcs.dbg_read_memory(addr, num_bytes, ptr);
        return ptr;
    }
    else {
        return 0;
    }
}

static bool dap_input_internal(char* text) {
    if (state.funcs.input != NULL && text != NULL) {
        state.funcs.input(text);
        return true;
    }
    else {
        return false;
    }
}

// ---------------------------------------------------------------------------

// stop_reason is UI_DBG_STOP_REASON_xxx
void dap_event_stopped(int stop_reason, uint16_t addr) {
#if defined(__EMSCRIPTEN__)
    dap_js_event_stopped(stop_reason, addr);
#else
    (void)stop_reason;
    (void)addr;
#endif
}

void dap_event_continued(void) {
#if defined(__EMSCRIPTEN__)
    dap_js_event_continued();
#endif
}

void dap_event_reboot(void) {
#if defined(__EMSCRIPTEN__)
    dap_js_event_reboot();
#endif
}

void dap_event_reset(void) {
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
        return response;
    });

    // When the Initialize response has been sent, we need to send the initialized event.
    // We use the registerSentHandler() to ensure the event is sent *after* the initialize response.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Events_Initialized
    session->registerSentHandler([&](const dap::ResponseOrError<dap::InitializeResponse>&) {
        // Send the initialized event after the initialize response.
        session->send(dap::InitializedEvent());
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

            if (!request.source.sourceReference.has_value()) {
                LOG_ERROR(DAP_SESSION_ERROR, "Source reference not provided in SetBreakpointsRequest");
                return dap::Error("Source reference not provided in SetBreakpointsRequest");
            }

            if (!request.lines.has_value()) {
                LOG_ERROR(DAP_SESSION_ERROR, "Lines not provided in SetBreakpointsRequest");
                return dap::Error("Lines not provided in SetBreakpointsRequest");
            }

            dap::integer source = request.source.sourceReference.value();

            if (dap_breakpoints_update.find(source) != dap_breakpoints_update.end()) {
                LOG_ERROR(DAP_SESSION_ERROR, "SetBreakpointsRequest is coming too fast");
                return dap::Error("SetBreakpointsRequest is coming too fast");
            }

            auto breakpoints = request.lines.value();

            {
                std::lock_guard<std::mutex> lock(dap_breakpoints_update_mutex);

                dap_breakpoints_update[source] = {};
                for (size_t i = 0; i < breakpoints.size(); i++) {
                    dap::integer address = breakpoints[i];
                    bool valid = address >= 0 && address < EMU_RAM_SIZE;
                    if (valid) dap_breakpoints_update[source].push_back((uint32_t)address);
                    response.breakpoints[i].verified = valid;
                }
            }
            return response;
        });
}

void dap_init(const dap_desc_t* desc) {
    assert(desc);
    state.inited = true;
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

        dap_register_session(session.get());

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

            session->bind(socket);

            dap_register_session(session.get());

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
    if (run_dap_boot) {
        run_dap_boot = false;
        dap_boot();
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
