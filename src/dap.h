#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool std;          // use stdin/stdout
    const char* port;  // use TCP port

    void* session;  // DAP session
    void* server;   // DAP TCP server
} dap_t;

void dap_init(dap_t* dap, bool std, const char* port);
void dap_shutdown(dap_t* dap);

#ifdef __cplusplus
} /* extern "C" */
#endif
