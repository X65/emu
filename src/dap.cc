#include "./dap.h"

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

    auto session = dap::Session::create();

    // Handle errors reported by the Session. These errors include protocol
    // parsing errors and receiving messages with no handler.
    session->onError([&](const char* msg) {
        if (log) {
            dap::writef(log, "dap::Session error: %s\n", msg);
            log->close();
        }
        // terminate.fire();
    });

    // All the handlers we care about have now been registered.

    if (dap->std) {
        // We now bind the session to stdin and stdout to connect to the client.
        // After the call to bind() we should start receiving requests, starting with
        // the Initialize request.
        std::shared_ptr<dap::Reader> in = dap::file(stdin, false);
        std::shared_ptr<dap::Writer> out = dap::file(stdout, false);
        if (log) {
            dap::writef(log, "dap::stdio binding\n");
            session->bind(spy(in, log), spy(out, log));
        }
        else {
            session->bind(in, out);
        }
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

            // The Initialize request is the first message sent from the client and
            // the response reports debugger capabilities.
            // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Initialize
            session->registerHandler([&](const dap::InitializeRequest&) {
                dap::InitializeResponse response;
                if (log) {
                    dap::writef(log, "Server received initialize request from client\n");
                }
                return response;
            });

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
            if (log) {
                dap::writef(log, "Server closing connection\n");
            }
        };

        // Error handler
        auto onError = [&](const char* msg) {
            if (log) {
                dap::writef(log, "dap::network server error: %s\n", msg);
            }
        };

        // Create the network server
        auto server = dap::net::Server::create();
        // Start listening on kPort.
        // onClientConnected will be called when a client wants to connect.
        // onError will be called on any connection errors.
        server->start(kPort, onClientConnected, onError);

        if (log) {
            dap::writef(log, "dap::network listening on port %d\n", kPort);
        }

        dap->server = server.release();
    }

    dap->session = session.release();
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
