#include "./dap.h"

#include <cstring>
#include <csignal>

void dap_init(dap_t* dap, bool std, char* port) {
    memset(dap, 0, sizeof(dap_t));
    dap->std = std;
    dap->port = port;
}
