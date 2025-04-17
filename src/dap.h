#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool std;    // use stdin/stdout
    char* port;  // use TCP port
} dap_t;

void dap_init(dap_t* dap, bool std, char* port);

#ifdef __cplusplus
} /* extern "C" */
#endif
