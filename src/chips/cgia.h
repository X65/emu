#pragma once
/*
    cgia.h -- Color Graphic Interface Adaptor

    Do this:
        #define CHIPS_IMPL
    before you include this file in *one* C or C++ file to create the
    implementation.

    Optionally provide the following macros with your own implementation

        CHIPS_ASSERT(c)     -- your own assert macro (default: assert(c))

    EMULATED PINS:

                  +-----------+
            FS <--|           |--> A0
            HS <--|           |...
          (RP) <--|           |--> A12
                  |           |
            AG -->|           |
            AS -->|   CGIA    |
        INTEXT -->|           |<-- D0
           GM0 -->|           |...
           GM1 -->|           |<-- D7
           GM2 -->|           |
           INV -->|           |
           CSS -->|           |
                  +-----------+

    FIXME: documentation

    ## zlib/libpng license

    Copyright (c) 2025 Tomasz Sterna
    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source
        distribution.
*/

#include "chips/chips_common.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
    Pin definitions

    The 13 address bus pins A0 to A12 are on the same location as the
    first 13 CPU address bus pins.

    The data bus pins are on the same location as the CPU's.

    The control pins start at location 40.

    For the synchronization output pins FS (field sync), HS (horizontal sync),
    and RP (Row Preset) not only the current state is important, but
    also the transitions from active to inactive. To capture these
    transitions, two extra 64-bit pin struct members are provided
    which store the raising and falling transition state for each
    pin bit.

    FS (field sync)         --  The inactive-to-active transition coincides with the
                                end of the active display area. The active-to-inactive
                                transition coincides with the trailing edge of the
                                vertical synchronization pulse.

    HS (horizontal sync)    --  The HS pulse coincides with the horizontal synchronization
                                pulse furnished to the television receiver by the VDG.
                                The inactive-to-active transition coincides with the
                                leading edge of the horizontal synchronization pulse
                                and the active-to-inactive transition coincides with the
                                trailing egde.

    RP (row preset)         --  The RP pin will go active every 12 lines to reset
                                a counter for an optional external character rom.

    Mode select pin functions:

    | AG | AS | INT/EXT | INV | GM2 | GM1 | GM0 | # colors | description
    +----+----+---------+-----+-----+-----+-----+----------+------------------------------
    |  0 |  0 |    0    |  0  |  -  |  -  |  -  |     2    | internal alphanum
    |  0 |  0 |    0    |  1  |  -  |  -  |  -  |     2    | internal alphanum, inverted
    |  0 |  0 |    1    |  0  |  -  |  -  |  -  |     2    | external alphanum
    |  0 |  0 |    1    |  1  |  -  |  -  |  -  |     2    | external alphanum, inverted
    +----+----+---------+-----+-----+-----+-----+----------+------------------------------
    |  0 |  1 |    0    |  -  |  -  |  -  |  -  |     8    | semigraphics4 (SG4)
    |  0 |  1 |    1    |  -  |  -  |  -  |  -  |     8    | semigraphics6 (SG6)
    +----+----+---------+-----+-----+-----+-----+----------+------------------------------
    |  1 |  - |    -    |  -  |  0  |  0  |  0  |     4    | 64x64 color graphics 1 (CG1)
    |  1 |  - |    -    |  -  |  0  |  0  |  1  |     2    | 128x64 resolution graphics 1 (RG1)
    |  1 |  - |    -    |  -  |  0  |  1  |  0  |     4    | 128x64 color graphics 2 (CG2)
    |  1 |  - |    -    |  -  |  0  |  1  |  1  |     2    | 128x96 resolution graphics 2 (RG2)
    |  1 |  - |    -    |  -  |  1  |  0  |  0  |     4    | 128x96 color graphics 3 (CG3)
    |  1 |  - |    -    |  -  |  1  |  0  |  1  |     2    | 128x192 resolution graphics 3 (RG3)
    |  1 |  - |    -    |  -  |  1  |  1  |  0  |     4    | 128x192 color graphics 6 (CG6)
    |  1 |  - |    -    |  -  |  1  |  1  |  1  |     2    | 256x192 resolution graphics 6 (RG6)

    The CSS pins select between 2 possible color sets.
*/

// address bus pins
#define CGIA_PIN_A0  (0)
#define CGIA_PIN_A1  (1)
#define CGIA_PIN_A2  (2)
#define CGIA_PIN_A3  (3)
#define CGIA_PIN_A4  (4)
#define CGIA_PIN_A5  (5)
#define CGIA_PIN_A6  (6)
#define CGIA_PIN_A7  (7)
#define CGIA_PIN_A8  (8)
#define CGIA_PIN_A9  (9)
#define CGIA_PIN_A10 (10)
#define CGIA_PIN_A11 (11)
#define CGIA_PIN_A12 (12)

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

// synchronization output pins
#define CGIA_PIN_FS (41)  // field sync
#define CGIA_PIN_HS (42)  // horizontal sync
#define CGIA_PIN_RP (43)  // row preset (not emulated)

// mode-select input pins
#define CGIA_PIN_AG     (44)  // graphics mode enable
#define CGIA_PIN_AS     (45)  // semi-graphics mode enable
#define CGIA_PIN_INTEXT (46)  // internal/external select
#define CGIA_PIN_INV    (47)  // invert enable
#define CGIA_PIN_GM0    (48)  // graphics mode select 0
#define CGIA_PIN_GM1    (49)  // graphics mode select 1
#define CGIA_PIN_GM2    (50)  // graphics mode select 2
#define CGIA_PIN_CSS    (51)  // color select pin

// pin bit masks
#define CGIA_A0        (1ULL << CGIA_PIN_A0)
#define CGIA_A1        (1ULL << CGIA_PIN_A1)
#define CGIA_A2        (1ULL << CGIA_PIN_A2)
#define CGIA_A3        (1ULL << CGIA_PIN_A3)
#define CGIA_A4        (1ULL << CGIA_PIN_A4)
#define CGIA_A5        (1ULL << CGIA_PIN_A5)
#define CGIA_A6        (1ULL << CGIA_PIN_A6)
#define CGIA_A7        (1ULL << CGIA_PIN_A7)
#define CGIA_A8        (1ULL << CGIA_PIN_A8)
#define CGIA_A9        (1ULL << CGIA_PIN_A9)
#define CGIA_A10       (1ULL << CGIA_PIN_A10)
#define CGIA_A11       (1ULL << CGIA_PIN_A11)
#define CGIA_A12       (1ULL << CGIA_PIN_A12)
#define CGIA_D0        (1ULL << CGIA_PIN_D0)
#define CGIA_D1        (1ULL << CGIA_PIN_D1)
#define CGIA_D2        (1ULL << CGIA_PIN_D2)
#define CGIA_D3        (1ULL << CGIA_PIN_D3)
#define CGIA_D4        (1ULL << CGIA_PIN_D4)
#define CGIA_D5        (1ULL << CGIA_PIN_D5)
#define CGIA_D6        (1ULL << CGIA_PIN_D6)
#define CGIA_D7        (1ULL << CGIA_PIN_D7)
#define CGIA_RW        (1ULL << CGIA_PIN_RW)
#define CGIA_CS        (1ULL << CGIA_PIN_CS)
#define CGIA_FS        (1ULL << CGIA_PIN_FS)
#define CGIA_HS        (1ULL << CGIA_PIN_HS)
#define CGIA_RP        (1ULL << CGIA_PIN_RP)
#define CGIA_AG        (1ULL << CGIA_PIN_AG)
#define CGIA_AS        (1ULL << CGIA_PIN_AS)
#define CGIA_INTEXT    (1ULL << CGIA_PIN_INTEXT)
#define CGIA_INV       (1ULL << CGIA_PIN_INV)
#define CGIA_GM0       (1ULL << CGIA_PIN_GM0)
#define CGIA_GM1       (1ULL << CGIA_PIN_GM1)
#define CGIA_GM2       (1ULL << CGIA_PIN_GM2)
#define CGIA_CSS       (1ULL << CGIA_PIN_CSS)
#define CGIA_CTRL_PINS (CGIA_AG | CGIA_AS | CGIA_INTEXT | CGIA_INV | CGIA_GM0 | CGIA_GM1 | CGIA_GM2 | CGIA_CSS)

// helper macros to set and extract address and data to/from pin mask

// extract 13-bit address bus from 64-bit pins
#define CGIA_GET_ADDR(p) ((uint16_t)(p & 0xFFFFULL))
// merge 13-bit address bus value into 64-bit pins
#define CGIA_SET_ADDR(p, a) \
    { p = ((p & ~0xFFFFULL) | ((a) & 0xFFFFULL)); }
// extract 8-bit data bus from 64-bit pins
#define CGIA_GET_DATA(p) ((uint8_t)(((p) & 0xFF0000ULL) >> 16))
// merge 8-bit data bus value into 64-bit pins
#define CGIA_SET_DATA(p, d) \
    { p = (((p) & ~0xFF0000ULL) | (((d) << 16) & 0xFF0000ULL)); }

// public constants
#define CGIA_VBLANK_LINES        (13)   // 13 lines vblank at top of screen
#define CGIA_TOP_BORDER_LINES    (25)   // 25 lines top border
#define CGIA_DISPLAY_LINES       (192)  // 192 lines visible display area
#define CGIA_BOTTOM_BORDER_LINES (26)   // 26 lines bottom border
#define CGIA_VRETRACE_LINES      (6)    // 6 'lines' for vertical retrace
#define CGIA_ALL_LINES           (262)  // all of the above
#define CGIA_DISPLAY_START       (CGIA_VBLANK_LINES + CGIA_TOP_BORDER_LINES)
#define CGIA_DISPLAY_END         (CGIA_DISPLAY_START + CGIA_DISPLAY_LINES)
#define CGIA_BOTTOM_BORDER_END   (CGIA_DISPLAY_END + CGIA_BOTTOM_BORDER_LINES)
#define CGIA_FSYNC_START         (CGIA_DISPLAY_END)

// hardware color indices
#define CGIA_HWCOLOR_GFX_GREEN         (0)
#define CGIA_HWCOLOR_GFX_YELLOW        (1)
#define CGIA_HWCOLOR_GFX_BLUE          (2)
#define CGIA_HWCOLOR_GFX_RED           (3)
#define CGIA_HWCOLOR_GFX_BUFF          (4)
#define CGIA_HWCOLOR_GFX_CYAN          (5)
#define CGIA_HWCOLOR_GFX_MAGENTA       (6)
#define CGIA_HWCOLOR_GFX_ORANGE        (7)
#define CGIA_HWCOLOR_ALNUM_GREEN       (8)
#define CGIA_HWCOLOR_ALNUM_DARK_GREEN  (9)
#define CGIA_HWCOLOR_ALNUM_ORANGE      (10)
#define CGIA_HWCOLOR_ALNUM_DARK_ORANGE (11)
#define CGIA_HWCOLOR_BLACK             (12)
#define CGIA_HWCOLOR_NUM               (13)

// pixel width and height of entire visible area, including border
#define CGIA_DISPLAY_WIDTH  (320)
#define CGIA_DISPLAY_HEIGHT (CGIA_TOP_BORDER_LINES + CGIA_DISPLAY_LINES + CGIA_BOTTOM_BORDER_LINES)

// framebuffer width and height
#define CGIA_FRAMEBUFFER_WIDTH      (512)
#define CGIA_FRAMEBUFFER_HEIGHT     (CGIA_DISPLAY_HEIGHT)
#define CGIA_FRAMEBUFFER_SIZE_BYTES (CGIA_FRAMEBUFFER_WIDTH * CGIA_FRAMEBUFFER_HEIGHT)

// pixel width and height of only the image area, without border
#define CGIA_IMAGE_WIDTH  (256)
#define CGIA_IMAGE_HEIGHT (192)

// horizontal border width
#define CGIA_BORDER_PIXELS ((CGIA_DISPLAY_WIDTH - CGIA_IMAGE_WIDTH) / 2)

// the CGIA is always clocked at 3.579 MHz
#define CGIA_TICK_HZ (3579545)

// fixed point precision for more precise error accumulation
#define CGIA_FIXEDPOINT_SCALE (16)

// a memory-fetch callback, used to read video memory bytes into the CGIA
typedef uint64_t (*cgia_fetch_t)(uint64_t pins, void* user_data);

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
    int h_count;
    int h_sync_start;
    int h_sync_end;
    int h_period;
    int l_count;

    // true during field-sync
    bool fs;

    // the fetch callback function
    cgia_fetch_t fetch_cb;
    // optional user-data for the fetch-callback
    void* user_data;
    // pointer to uint8_t buffer where decoded video image is written too
    uint8_t* fb;
    // hardware colors
    uint32_t hwcolors[CGIA_HWCOLOR_NUM];
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
