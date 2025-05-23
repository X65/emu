#pragma once

#include "webapi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool stdio;
    const char* port;
    webapi_interface_t funcs;
    uint8_t* memory;
} dap_desc_t;

void dap_init(const dap_desc_t* desc);
void dap_shutdown();
void dap_process();

// stop_reason: WEBAPI_STOPREASON_xxx
void dap_event_stopped(int stop_reason, uint32_t addr);
void dap_event_continued(void);
void dap_event_reboot(void);
void dap_event_reset(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
