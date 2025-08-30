#pragma once
/*
    cgia.h -- Color Graphic Interface Adaptor

    Optionally provide the following macros with your own implementation

        CHIPS_ASSERT(c)     -- your own assert macro (default: assert(c))

                  +-----------+
            RW -->|           |--> A0
                  |           |...
            CS -->|           |--> A23
                  |           |
           INT <--|           |
                  |   CGIA    |
                  |           |<-- D0
                  |           |...
                  |           |<-- D7
                  |           |
                  |           |
                  |           |
                  +-----------+

    FIXME: documentation

    ## 0BSD license

    Copyright (c) 2025 Tomasz Sterna

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "chips/chips_common.h"
#include "chips/pwm.h"

#include "firmware/src/ria/cgia/cgia_palette.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
    Pin definitions

    The address bus pins are on the same location as the CPU address bus pins.
    The data bus pins are on the same location as the CPU's.

    The control pins start at location 40.

    Pin 40 is chip-select.
    R/W is directly connected from M6502_RW.

    CGIA has its own 2*64kB of fast SRAM memory to generate video from.
    It is used to mirror two banks of CPU RAM (bckgnd_bank and sprite_bank).
    It is done when an internal bank register is changed, then if the bank
    is not currently cached a DMA copy of 64kB is triggered from CPU memory.
    Afterwards, CGIA monitors every memory write on data bus and updates local
    VRAM caches accordingly.

*/

// address bus pins
#define CGIA_PIN_A0 (0)
#define CGIA_PIN_A1 (1)
#define CGIA_PIN_A2 (2)
#define CGIA_PIN_A3 (3)
#define CGIA_PIN_A4 (4)
#define CGIA_PIN_A5 (5)
#define CGIA_PIN_A6 (6)

// data bus pins
#define CGIA_PIN_D0 (16)
#define CGIA_PIN_D1 (17)
#define CGIA_PIN_D2 (18)
#define CGIA_PIN_D3 (19)
#define CGIA_PIN_D4 (20)
#define CGIA_PIN_D5 (21)
#define CGIA_PIN_D6 (22)
#define CGIA_PIN_D7 (23)

// shared control pins
#define CGIA_PIN_RW (24) /* same as M6502_RW */

// chip-specific pins
#define CGIA_PIN_CS   (40) /* chip-select */
#define CGIA_PIN_INT  (41) /* INTerrupt */
#define CGIA_PIN_PWM0 (46) /* PWM Output 0 */
#define CGIA_PIN_PWM1 (47) /* PWM Output 1 */

// pin bit masks
#define CGIA_A0      (1ULL << CGIA_PIN_A0)
#define CGIA_A1      (1ULL << CGIA_PIN_A1)
#define CGIA_A2      (1ULL << CGIA_PIN_A2)
#define CGIA_A3      (1ULL << CGIA_PIN_A3)
#define CGIA_A4      (1ULL << CGIA_PIN_A4)
#define CGIA_A5      (1ULL << CGIA_PIN_A5)
#define CGIA_A6      (1ULL << CGIA_PIN_A6)
#define CGIA_D0      (1ULL << CGIA_PIN_D0)
#define CGIA_D1      (1ULL << CGIA_PIN_D1)
#define CGIA_D2      (1ULL << CGIA_PIN_D2)
#define CGIA_D3      (1ULL << CGIA_PIN_D3)
#define CGIA_D4      (1ULL << CGIA_PIN_D4)
#define CGIA_D5      (1ULL << CGIA_PIN_D5)
#define CGIA_D6      (1ULL << CGIA_PIN_D6)
#define CGIA_D7      (1ULL << CGIA_PIN_D7)
#define CGIA_DB_PINS (0xFF0000ULL)
#define CGIA_RW      (1ULL << CGIA_PIN_RW)
#define CGIA_CS      (1ULL << CGIA_PIN_CS)
#define CGIA_INT     (1ULL << CGIA_PIN_INT)

// helper macros to set and extract address and data to/from pin mask

// extract 7-bit register address from 64-bit pins
#define CGIA_GET_REG_ADDR(p) ((uint8_t)(p & 0x7FULL))
// extract 8-bit data bus from 64-bit pins
#define CGIA_GET_DATA(p) ((uint8_t)(((p) & 0xFF0000ULL) >> 16))
// merge 8-bit data bus value into 64-bit pins
#define CGIA_SET_DATA(p, d) \
    { p = (((p) & ~0xFF0000ULL) | (((d) << 16) & 0xFF0000ULL)); }

// timing constants
#include "firmware/src/ria/sys/out.h"

#define MODE_H_TOTAL_PIXELS (MODE_H_FRONT_PORCH + MODE_H_SYNC_WIDTH + MODE_H_BACK_PORCH + MODE_H_ACTIVE_PIXELS)
#define MODE_V_TOTAL_LINES  (MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + MODE_V_BACK_PORCH + MODE_V_ACTIVE_LINES)

// hardware color palette
#define CGIA_HWCOLOR_NUM (CGIA_COLORS_NUM)

// framebuffer width and height
#define CGIA_FRAMEBUFFER_WIDTH      (MODE_H_ACTIVE_PIXELS)
#define CGIA_FRAMEBUFFER_HEIGHT     (MODE_V_ACTIVE_LINES)
#define CGIA_FRAMEBUFFER_SIZE_BYTES (CGIA_FRAMEBUFFER_WIDTH * CGIA_FRAMEBUFFER_HEIGHT * 4)

// linebuffer used to rasterize a line
#define CGIA_LINEBUFFER_PADDING (-SCHAR_MIN)  // maximum scroll of signed 8 bit
#define CGIA_LINEBUFFER_WIDTH   (CGIA_FRAMEBUFFER_WIDTH / FB_H_REPEAT + 2 * CGIA_LINEBUFFER_PADDING)

// pixel width and height of entire visible area
#define CGIA_DISPLAY_WIDTH  (MODE_H_ACTIVE_PIXELS)
#define CGIA_DISPLAY_HEIGHT (MODE_V_ACTIVE_LINES)

// rasterized pixel width and height of entire visible area
#define CGIA_ACTIVE_WIDTH  (MODE_H_ACTIVE_PIXELS / FB_H_REPEAT)
#define CGIA_ACTIVE_HEIGHT (MODE_V_ACTIVE_LINES / FB_V_REPEAT)

// fixed point precision for more precise error accumulation
#define CGIA_FIXEDPOINT_SCALE (256)

// a memory-fetch callback, used to read video memory bytes into the CGIA
typedef uint8_t (*cgia_fetch_t)(uint32_t data, void* user_data);

// CGIA has 7 address lines
#define CGIA_NUM_REGS (1U << 7)

// the cgia setup parameters
typedef struct {
    // the CPU tick rate in hz
    int tick_hz;
    // pointer to an uint8_t framebuffer where video image is written to (must be at least 512*244 bytes)
    chips_range_t framebuffer;
    // memory-fetch callback
    cgia_fetch_t fetch_cb;
    // optional user-data for the fetch callback
    void* user_data;
} cgia_desc_t;

// the cgia state struct
typedef struct {
    // last pin state
    uint64_t pins;

    // internal counters
    uint h_count;
    uint h_period;
    uint v_count;

    uint scan_line;  // currently rendered physical scan line

    // CGIA internal registers
    uint8_t* chip;

    // copy of CGIA internal registers
    struct cgia_internal {
        uint16_t memory_scan;
        uint16_t colour_scan;
        uint16_t backgr_scan;
        uint16_t chargen_offset;
        uint8_t row_line_count;
        bool wait_vbl;
        bool sprites_need_update;
        uint16_t sprite_dsc_offsets[8];
    } internal[4];

    // VRAM cache information
    struct {
        uint32_t bank_mask;
        uint32_t wanted_bank_mask;
        uint8_t cache_ptr_idx;
    } vram_cache[2];

    // Interrupt mask
    uint8_t int_mask;

    // the fetch callback function
    cgia_fetch_t fetch_cb;
    // optional user-data for the fetch-callback
    void* user_data;
    // pointer to uint8_t buffer where decoded video image is written too
    uint32_t* fb;
    // hardware colors
    uint32_t* hwcolors;
    // VRAM banks
    uint8_t* vram[2];
    // rasterizer linebuffer
    uint32_t linebuffer[CGIA_LINEBUFFER_WIDTH];
} cgia_t;

// initialize a new cgia_t instance
void cgia_init(cgia_t* vpu, const cgia_desc_t* desc);
// reset a cgia_t instance
void cgia_reset(cgia_t* vpu);
// tick the cgia_t instance, this will call the fetch_cb and generate the image
uint64_t cgia_tick(cgia_t* vpu, uint64_t pins);
// prepare cgia_t snapshot for saving
void cgia_snapshot_onsave(cgia_t* snapshot);
// fixup cgia_t snapshot after loading
void cgia_snapshot_onload(cgia_t* snapshot, cgia_t* sys);
// copy VRAM - after fastload
void cgia_mirror_vram(cgia_t* vpu);
// read CGIA register value
uint8_t cgia_reg_read(uint8_t reg_no);
// write CGIA register
void cgia_reg_write(uint8_t reg_no, uint8_t value);

#ifdef __cplusplus
}  // extern "C"
#endif
