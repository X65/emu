#pragma once
/*
    cgia.h -- Color Graphic Interface Adaptor

    Optionally provide the following macros with your own implementation

        CHIPS_ASSERT(c)     -- your own assert macro (default: assert(c))

                  +-----------+
            RW -->|           |--> A0
                  |           |...
            CS -->|           |--> A6
                  |           |
                  |           |
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

    The 7 address bus pins A0 to A6 are on the same location as the
    first 7 CPU address bus pins.

    The data bus pins are on the same location as the CPU's.

    The control pins start at location 40.

    Pin 40 is chip-select.
    R/W is directly connected from M6502_RW.

    CGIA uses back-channel to directly access RAM. Think dual-port memory.
    In RP hardware it is implemented internally in software.

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
#define CGIA_PIN_CS (40) /* chip-select */

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

// helper macros to set and extract address and data to/from pin mask

// extract 7-bit address bus from 64-bit pins
#define CGIA_GET_ADDR(p) ((uint8_t)(p & 0x7FULL))
// merge 7-bit address bus value into 64-bit pins
#define CGIA_SET_ADDR(p, a) \
    { p = ((p & ~0x7FULL) | ((a) & 0x7FULL)); }
// extract 8-bit data bus from 64-bit pins
#define CGIA_GET_DATA(p) ((uint8_t)(((p) & 0xFF0000ULL) >> 16))
// merge 8-bit data bus value into 64-bit pins
#define CGIA_SET_DATA(p, d) \
    { p = (((p) & ~0xFF0000ULL) | (((d) << 16) & 0xFF0000ULL)); }

// timing constants
// synced from ext/firmware/src/ria/sys/out.c
#define MODE_H_SYNC_POLARITY (0)
#define MODE_H_FRONT_PORCH   (24)
#define MODE_H_SYNC_WIDTH    (72)
#define MODE_H_BACK_PORCH    (96)
#define MODE_H_ACTIVE_PIXELS (768)
#define MODE_V_SYNC_POLARITY (1)
#define MODE_V_FRONT_PORCH   (3)
#define MODE_V_SYNC_WIDTH    (6)
#define MODE_V_BACK_PORCH    (11)
#define MODE_V_ACTIVE_LINES  (480)
#define MODE_BIT_CLK_KHZ     (287500)

#define FB_H_REPEAT (2)
#define FB_V_REPEAT (2)

#define MODE_H_TOTAL_PIXELS (MODE_H_FRONT_PORCH + MODE_H_SYNC_WIDTH + MODE_H_BACK_PORCH + MODE_H_ACTIVE_PIXELS)
#define MODE_V_TOTAL_LINES  (MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + MODE_V_BACK_PORCH + MODE_V_ACTIVE_LINES)

// hardware color palette
#define CGIA_HWCOLOR_NUM (CGIA_COLORS_NUM)

// pixel width and height of entire visible area, including border
#define CGIA_DISPLAY_WIDTH  MODE_H_ACTIVE_PIXELS
#define CGIA_DISPLAY_HEIGHT MODE_V_ACTIVE_LINES

// framebuffer width and height
#define CGIA_LINE_BUFFER_PADDING    (-SCHAR_MIN)  // maximum scroll of signed 8 bit
#define CGIA_FRAMEBUFFER_WIDTH      (CGIA_DISPLAY_WIDTH + 2 * CGIA_LINE_BUFFER_PADDING)
#define CGIA_FRAMEBUFFER_HEIGHT     (CGIA_DISPLAY_HEIGHT)
#define CGIA_FRAMEBUFFER_SIZE_BYTES (CGIA_FRAMEBUFFER_WIDTH * CGIA_FRAMEBUFFER_HEIGHT)

// fixed point precision for more precise error accumulation
#define CGIA_FIXEDPOINT_SCALE (256)

// a memory-fetch callback, used to read video memory bytes into the CGIA
typedef uint64_t (*cgia_fetch_t)(uint64_t pins, void* user_data);

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

    // internal registers (memory mapped)
    uint8_t reg[CGIA_NUM_REGS];

    // internal counters
    int h_count;
    int h_period;
    int l_count;

    int active_line;

    // the fetch callback function
    cgia_fetch_t fetch_cb;
    // optional user-data for the fetch-callback
    void* user_data;
    // pointer to uint8_t buffer where decoded video image is written too
    uint8_t* fb;
    // hardware colors
    uint32_t* hwcolors;
} cgia_t;

// initialize a new cgia_t instance
void cgia_init(cgia_t* vdg, const cgia_desc_t* desc);
// reset a cgia_t instance
void cgia_reset(cgia_t* vdg);
// tick the cgia_t instance, this will call the fetch_cb and generate the image
uint64_t cgia_tick(cgia_t* vdg, uint64_t pins);
// prepare cgia_t snapshot for saving
void cgia_snapshot_onsave(cgia_t* snapshot);
// fixup cgia_t snapshot after loading
void cgia_snapshot_onload(cgia_t* snapshot, cgia_t* sys);

#ifdef __cplusplus
}  // extern "C"
#endif
