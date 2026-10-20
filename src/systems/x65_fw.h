#pragma once
/*
    x65_fw.h -- the board around the firmware's RIA API

    The emulator compiles the X65 firmware's north/api/{api,oem,rng,clk}.c
    unchanged (see X65_FW_API_SOURCES in the root CMakeLists.txt). Those files
    reach the hardware through a handful of calls -- RNG, timers, the PIX link
    to the south chip, RAM writes -- which src/systems/x65_fw.c supplies on top
    of an x65_t. The firmware keeps its state in globals (regs[], xstack[],
    xstack_ptr), so only one machine owns the API at a time: x65_fw_bind() says
    which. x65.c binds in init, reset, exec and the debugger's mem_wr.

    The functions mirror the firmware's main.c lifecycle: init once, then
    run/stop around every CPU run, task on every tick.
*/
#include "x65.h"

#ifdef __cplusplus
extern "C" {
#endif

// the machine the firmware shims act on
void x65_fw_bind(x65_t* sys);
// once per process: oem_init(), clk_init()
void x65_fw_init(void);
// the firmware's run() hooks: api_run(), clk_run()
void x65_fw_run(void);
// the firmware's stop() hooks: api_stop(), oem_stop()
void x65_fw_stop(void);
// one pass of api_task(): latches a pending op and runs one step of it
void x65_fw_task(void);
// copy the firmware's API globals into / out of a snapshot
void x65_fw_save(x65_api_state_t* dst);
void x65_fw_load(const x65_api_state_t* src);

#ifdef __cplusplus
}  // extern "C"
#endif
