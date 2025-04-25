#include "./dap.h"

#include "log.h"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <csignal>

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

void dap_register_session(dap::Session* session) {
    // Handle errors reported by the Session. These errors include protocol
    // parsing errors and receiving messages with no handler.
    session->onError([&](const char* msg) {
        LOG_ERROR(DAP_SESSION_ERROR, "%s", msg);
        // terminate.fire();
    });

    // The Initialize request is the first message sent from the client and
    // the response reports debugger capabilities.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Initialize
    session->registerHandler([&](const dap::InitializeRequest& request) {
        dap::InitializeResponse response;
        LOG_INFO(DAP_INFO, "Client '%s' initialize", request.clientName.value("unknown").c_str());
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
    session->registerSentHandler(
        [&](const dap::ResponseOrError<dap::InitializeResponse>&) { session->send(dap::InitializedEvent()); });
}

void dap_init(dap_t* dap, bool std, const char* port) {
    memset(dap, 0, sizeof(dap_t));
    dap->std = std;
    dap->port = port;

#ifdef LOG_TO_FILE
    log = dap::file(LOG_TO_FILE);
#endif

    if (dap->std) {
#ifdef OS_WINDOWS
        // Change stdin & stdout from text mode to binary mode.
        // This ensures sequences of \r\n are not changed to \n.
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif  // OS_WINDOWS
    }

    if (dap->std) {
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

        dap->session = session.release();
    }
    if (dap->port) {
        int kPort = atoi(dap->port);

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

        dap->server = server.release();
    }
}

void dap_shutdown(dap_t* dap) {
#ifdef LOG_TO_FILE
    log->close();
#endif

#ifndef _WIN32
    if (dap->std) {
        // DAP server might be waiting for stdin and will not join reading thread
        // Kill the whole process, as we are going down anyway…
        kill(getpid(), SIGTERM);
    }
#endif
}
