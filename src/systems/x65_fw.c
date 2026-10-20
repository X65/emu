/*
    x65_fw.c -- the board around the firmware's RIA API (see x65_fw.h)
*/
#include "./x65_fw.h"
#include "../log.h"

#include "api/api.h"
#include "api/clk.h"
#include "api/dir.h"
#include "api/oem.h"
#include "api/std.h"
#include "hid/kbd.h"
#include "mon/mon.h"
#include "sys/cfg.h"
#include "sys/cpu.h"
// both firmware trees have a sys/pix.h and the south one is found first
#include "firmware/src/north/sys/pix.h"
#include "term/font.h"
#include <fatfs/ff.h>
#include <hardware/timer.h>
#include <pico/aon_timer.h>
#include <pico/rand.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

// The firmware's globals (declared in north/sys/mem.h). regs[] mirrors
// $FFC0-$FFFF, of which the API code touches $FFF0-$FFF3.
volatile uint8_t regs[0x40];
uint8_t xstack[XSTACK_SIZE + 1];
size_t volatile xstack_ptr;
static_assert(X65_API_XSTACK_SIZE == XSTACK_SIZE, "x65_api_state_t.xstack must hold the firmware's xstack");

static x65_t* g_sys;

void x65_fw_bind(x65_t* sys) {
    g_sys = sys;
}

void x65_fw_init(void) {
    static bool done;
    if (done) return;
    done = true;
    font_init();  // the south chip's power-up
    oem_init();   // selects the default code page, which reaches the font over PIX
    clk_init();   // sets the process TZ to the board's default, UTC0
}

void x65_fw_run(void) {
    api_run();
    clk_run();
}

void x65_fw_stop(void) {
    api_stop();
    oem_stop();
}

void x65_fw_task(void) {
    api_task();
}

void x65_fw_save(x65_api_state_t* dst) {
    memcpy(dst->regs, (const uint8_t*)&REGS(0xFFF0), sizeof(dst->regs));
    dst->rejected = api_rejected;
    dst->xstack_ptr = (uint16_t)xstack_ptr;
    memcpy(dst->xstack, xstack, sizeof(dst->xstack));
}

void x65_fw_load(const x65_api_state_t* src) {
    memcpy((uint8_t*)&REGS(0xFFF0), src->regs, sizeof(src->regs));
    api_rejected = src->rejected;
    xstack_ptr = src->xstack_ptr;
    memcpy(xstack, src->xstack, sizeof(src->xstack));
    // The op that was running sits in file-scope statics inside the firmware
    // (api_active_op, the chargen progress) and cannot be restored. Run the
    // stop hooks so they reset, and fail the op the way a lost PIX reply would,
    // so the guest polling $FFF3 gets an answer.
    x65_fw_stop();
    if (API_BUSY) {
        const uint8_t op = API_OP;
        api_return_errno(API_EIO);
        LOG_WARNING("RIA API op $%02X was in flight when the snapshot was taken; failed with EIO", op);
    }
}

/*== hardware the firmware's api/ code calls ================================*/

// pico/rand.h. One rand() per byte, like the $FFE2/3 register in ria816.c, so
// the result does not depend on RAND_MAX and --seed repeats both.
uint32_t get_rand_32(void) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        v = (v << 8) | (uint8_t)rand();
    }
    return v;
}

// hardware/timer.h: the emulated clock, so clk_api_clock() is deterministic.
// clk_run() samples it right after the RIA zeroes it, in init and reset.
uint64_t time_us_64(void) {
    return g_sys ? g_sys->ria.us : 0;
}

// pico/aon_timer.h: the host wall clock plus whatever offset the guest set.
static bool aon_running;
static struct timespec aon_offset;

static void _host_now(struct timespec* ts) {
#ifdef TIME_UTC
    timespec_get(ts, TIME_UTC);
#else  // msvcrt mingw has no C11 timespec_get
    ts->tv_sec = time(NULL);
    ts->tv_nsec = 0;
#endif
}

static struct timespec _timespec_add(struct timespec a, struct timespec b) {
    struct timespec r = { .tv_sec = a.tv_sec + b.tv_sec, .tv_nsec = a.tv_nsec + b.tv_nsec };
    while (r.tv_nsec >= 1000000000L) {
        r.tv_nsec -= 1000000000L;
        r.tv_sec += 1;
    }
    while (r.tv_nsec < 0) {
        r.tv_nsec += 1000000000L;
        r.tv_sec -= 1;
    }
    return r;
}

bool aon_timer_start(const struct timespec* ts) {
    // The board starts at whatever time the firmware passes (noon, 1970) until
    // something sets it. A host knows the time, so start there instead.
    (void)ts;
    aon_running = true;
    aon_offset = (struct timespec){ 0, 0 };
    return true;
}

bool aon_timer_stop(void) {
    aon_running = false;
    return true;
}

bool aon_timer_is_running(void) {
    return aon_running;
}

bool aon_timer_set_time(const struct timespec* ts) {
    struct timespec now;
    _host_now(&now);
    aon_offset = _timespec_add(*ts, (struct timespec){ -now.tv_sec, -now.tv_nsec });
    return true;
}

bool aon_timer_get_time(struct timespec* ts) {
    if (!aon_running) return false;
    struct timespec now;
    _host_now(&now);
    *ts = _timespec_add(now, aon_offset);
    ts->tv_nsec -= ts->tv_nsec % 1000000L;  // the RP2350 AON timer ticks in ms
    return true;
}

void aon_timer_get_resolution(struct timespec* ts) {
    *ts = (struct timespec){ 0, 1000000L };
}

// sys/cpu.h: the parts of the firmware's cpu.c that are not GPIO
bool cpu_active(void) {
    return g_sys && g_sys->running;
}

bool cpu_api_phi2(void) {
    if (!api_is_xstack_empty()) return api_return_errno(API_EINVAL);
    return api_return_ax(X65_FREQUENCY / 1000);
}

// sys/mem.h: the 16MB RAM
void mem_write_ram(uint32_t addr24, uint8_t data) {
    mem_ram_write(g_sys, addr24 & 0xFFFFFF, data);
}

uint8_t mem_read_ram(uint32_t addr24) {
    return mem_ram_read(g_sys, addr24 & 0xFFFFFF);
}

// sys/pix.h: the link to the south chip, answered on the spot by the same font
// code the south compiles (south/sys/pix.c handles these two commands).
void pix_send_request(pix_req_type_t msg_type, uint8_t req_len5, const uint8_t* req_data, pix_response_t* resp) {
    uint16_t reply = PIX_RESPONSE(PIX_NAK, 0);
    if (msg_type == PIX_DEV_CMD && req_len5 >= 1 && (req_data[0] >> 4) == PIX_DEV_VPU) {
        switch (req_data[0] & 0x0F) {
            case PIX_VPU_CMD_GET_CHARGEN:
                if (req_len5 >= 5) {
                    const uint16_t idx = (uint16_t)(req_data[1] | (req_data[2] << 8));
                    const uint16_t cp = (uint16_t)(req_data[3] | (req_data[4] << 8));
                    const uint16_t has_cp = (cp == 0xFFFF || font_has_code_page(cp)) ? PIX_CHARGEN_HAS_CP : 0;
                    reply = PIX_RESPONSE(PIX_DEV_DATA, font_get_byte(idx, cp) | has_cp);
                }
                break;
            case PIX_VPU_CMD_SET_CODE_PAGE:
                if (req_len5 >= 3) {
                    font_set_code_page((uint16_t)(req_data[1] | (req_data[2] << 8)));
                    reply = PIX_RESPONSE(PIX_ACK, 0);
                }
                break;
            default: break;
        }
    }
    if (resp) {
        resp->reply = reply;
        resp->status = 1;
    }
}

// hid/kbd.h: the real one lives in kbd.c's Pico-only half and needs FatFs
void kbd_rebuild_code_page_cache(void) {}

// sys/cfg.h: the board persists settings to flash; the emulator has none
void cfg_save(void) {}

// mon/mon.h: the monitor's console
void mon_add_response_str(const char* str) {
    LOG_WARNING("RIA: %s", str);
}

// fatfs/ff.h: until the storage phase compiles the real FatFs, accept the code
// pages its f_setcp() accepts (validcp[] in ff.c).
FRESULT f_setcp(WORD cp) {
    static const WORD validcp[] = { 437, 720, 737, 771, 775, 850, 852, 855, 857, 860, 861,
                                    862, 863, 864, 865, 866, 869, 932, 936, 949, 950 };
    for (size_t i = 0; i < sizeof(validcp) / sizeof(validcp[0]); i++) {
        if (validcp[i] == cp) return FR_OK;
    }
    return FR_INVALID_PARAMETER;
}

#ifdef _WIN32
int setenv(const char* name, const char* value, int overwrite) {
    if (!overwrite && getenv(name)) return 0;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s=%s", name, value);
    return _putenv(buf);
}
#endif

/*== ops without a backend yet ==============================================*/

// api_dispatch() names every handler; the stdio and directory ones need FatFs
// and a host storage backend. Until then they fail the way the firmware fails
// an unknown op. Delete this list when std.c and dir.c are compiled in.
#define X65_FW_ENOSYS_OPS(X) \
    X(std_api_open)          \
    X(std_api_close)         \
    X(std_api_read_xstack)   \
    X(std_api_read_xram)     \
    X(std_api_write_xstack)  \
    X(std_api_write_xram)    \
    X(std_api_lseek_cc65)    \
    X(std_api_lseek_llvm)    \
    X(std_api_syncfs)        \
    X(std_api_stdin_opt)     \
    X(dir_api_stat)          \
    X(dir_api_telldir)       \
    X(dir_api_seekdir)       \
    X(dir_api_rewinddir)     \
    X(dir_api_unlink)        \
    X(dir_api_rename)        \
    X(dir_api_chmod)         \
    X(dir_api_utime)         \
    X(dir_api_mkdir)         \
    X(dir_api_chdir)         \
    X(dir_api_chdrive)       \
    X(dir_api_getcwd)        \
    X(dir_api_setlabel)      \
    X(dir_api_getlabel)      \
    X(dir_api_getfree)

#define X(name)                              \
    bool name(void) {                        \
        return api_return_errno(API_ENOSYS); \
    }
X65_FW_ENOSYS_OPS(X)
#undef X

/*== the south chip's font, the other end of the PIX requests above =========*/

#include "firmware/src/south/term/font.c"
