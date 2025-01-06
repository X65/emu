#include "./cgia.h"

#include <string.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

#define _CGIA_CLAMP(x) ((x) > 255 ? 255 : (x))
#define _CGIA_RGBA(r, g, b) \
    (0xFF000000 | _CGIA_CLAMP((r * 4) / 3) | (_CGIA_CLAMP((g * 4) / 3) << 8) | (_CGIA_CLAMP((b * 4) / 3) << 16))

void cgia_init(cgia_t* vdg, const cgia_desc_t* desc) {
    CHIPS_ASSERT(vdg && desc);
    CHIPS_ASSERT(desc->framebuffer.ptr && (desc->framebuffer.size <= CGIA_FRAMEBUFFER_SIZE_BYTES));
    CHIPS_ASSERT(desc->fetch_cb);
    CHIPS_ASSERT((desc->tick_hz > 0) && (desc->tick_hz < CGIA_TICK_HZ));

    memset(vdg, 0, sizeof(*vdg));
    vdg->fb = desc->framebuffer.ptr;
    vdg->fetch_cb = desc->fetch_cb;
    vdg->user_data = desc->user_data;

    /* compute counter periods, the CGIA is always clocked at 3.579 MHz,
       and the frequency of how the tick function is called must be
       communicated to the init function

       one scanline is 228 3.5 CGIA ticks
    */
    int64_t tmp = (228LL * desc->tick_hz * CGIA_FIXEDPOINT_SCALE) / CGIA_TICK_HZ;
    vdg->h_period = (int)tmp;
    // hsync starts at tick 10 of a scanline
    tmp = (10LL * desc->tick_hz * CGIA_FIXEDPOINT_SCALE) / CGIA_TICK_HZ;
    vdg->h_sync_start = (int)tmp;
    // hsync is 16 ticks long
    tmp = (26LL * desc->tick_hz * CGIA_FIXEDPOINT_SCALE) / CGIA_TICK_HZ;
    vdg->h_sync_end = (int)tmp;

    /* the default graphics mode color palette

       the CGIA outputs three color values:
        - Y' - six level analog luminance
        - phiA - three level analog (U)
        - phiB - three level analog (V)

        see discussion here: http://forums.bannister.org/ubbthreads.php?ubb=showflat&Number=64986

        NEW VALUES from here: http://www.stardot.org.uk/forums/viewtopic.php?f=44&t=12503

        green:      19 146  11
        yellow:    155 150  10
        blue:        2  22 175
        red:       155  22   7
        buff:      141 150 154
        cyan:       15 143 155
        magenta:   139  39 155
        orange:    140  31  11

        color intensities are slightly boosted
    */
    vdg->hwcolors[CGIA_HWCOLOR_GFX_GREEN] = _CGIA_RGBA(19, 146, 11);
    vdg->hwcolors[CGIA_HWCOLOR_GFX_YELLOW] = _CGIA_RGBA(155, 150, 10);
    vdg->hwcolors[CGIA_HWCOLOR_GFX_BLUE] = _CGIA_RGBA(2, 22, 175);
    vdg->hwcolors[CGIA_HWCOLOR_GFX_RED] = _CGIA_RGBA(155, 22, 7);
    vdg->hwcolors[CGIA_HWCOLOR_GFX_BUFF] = _CGIA_RGBA(141, 150, 154);
    vdg->hwcolors[CGIA_HWCOLOR_GFX_CYAN] = _CGIA_RGBA(15, 143, 155);
    vdg->hwcolors[CGIA_HWCOLOR_GFX_MAGENTA] = _CGIA_RGBA(139, 39, 155);
    vdg->hwcolors[CGIA_HWCOLOR_GFX_ORANGE] = _CGIA_RGBA(140, 31, 11);
    vdg->hwcolors[CGIA_HWCOLOR_ALNUM_GREEN] = _CGIA_RGBA(19, 146, 11);
    vdg->hwcolors[CGIA_HWCOLOR_ALNUM_DARK_GREEN] = 0xFF002400;
    vdg->hwcolors[CGIA_HWCOLOR_ALNUM_ORANGE] = _CGIA_RGBA(140, 31, 11);
    vdg->hwcolors[CGIA_HWCOLOR_ALNUM_DARK_ORANGE] = 0xFF000E22;
    vdg->hwcolors[CGIA_HWCOLOR_BLACK] = 0xFF111111;
}

void cgia_reset(cgia_t* vdg) {
    CHIPS_ASSERT(vdg);
    vdg->h_count = 0;
    vdg->l_count = 0;
}

/*
    internal character ROM dump from MAME
    (ntsc_square_fontdata8x12 in devices/video/cgia.cpp)
*/
static const uint8_t _cgia_font[64 * 12] = {
    0x00, 0x00, 0x00, 0x1C, 0x22, 0x02, 0x1A, 0x2A, 0x2A, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x14, 0x22, 0x22,
    0x3E, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x12, 0x12, 0x1C, 0x12, 0x12, 0x3C, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1C, 0x22, 0x20, 0x20, 0x20, 0x22, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x12, 0x12, 0x12, 0x12, 0x12,
    0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x20, 0x20, 0x38, 0x20, 0x20, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E,
    0x20, 0x20, 0x38, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x20, 0x20, 0x26, 0x22, 0x22, 0x1E, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x3E, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x22, 0x24, 0x28, 0x30, 0x28, 0x24, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x36, 0x2A, 0x2A, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x22, 0x32, 0x2A, 0x26, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x22, 0x22, 0x22, 0x22, 0x22, 0x3E,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x22, 0x22, 0x3C, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x22,
    0x22, 0x22, 0x2A, 0x24, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x22, 0x22, 0x3C, 0x28, 0x24, 0x22, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1C, 0x22, 0x10, 0x08, 0x04, 0x22, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x22, 0x22, 0x22, 0x14, 0x14, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x2A, 0x2A, 0x36,
    0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x08, 0x14, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22,
    0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x02, 0x04, 0x08, 0x10, 0x20, 0x3E, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x38, 0x20, 0x20, 0x20, 0x20, 0x20, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x10,
    0x08, 0x04, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x1C, 0x2A, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x10, 0x3E, 0x10,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x14, 0x36, 0x00, 0x36, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x1E,
    0x20, 0x1C, 0x02, 0x3C, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x32, 0x04, 0x08, 0x10, 0x26, 0x26, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x28, 0x28, 0x10, 0x2A, 0x24, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x10, 0x20, 0x20, 0x20, 0x10, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x1C, 0x08,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x30, 0x30, 0x10, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x04,
    0x08, 0x10, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x24, 0x24, 0x24, 0x24, 0x24, 0x18, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x02, 0x1C, 0x20,
    0x20, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x02, 0x04, 0x02, 0x22, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x0C, 0x14, 0x3E, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x20, 0x3C, 0x02, 0x02, 0x22, 0x1C,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x20, 0x20, 0x3C, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x02,
    0x04, 0x08, 0x10, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x1C, 0x22, 0x22, 0x1C, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x1E, 0x02, 0x02, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00,
    0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x3E, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18,
    0x24, 0x04, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00,
};

static inline uint8_t _cgia_border_color(uint64_t pins) {
    if (pins & CGIA_AG) {
        // a graphics mode, either green or buff, depending on CSS pin
        return (pins & CGIA_CSS) ? CGIA_HWCOLOR_GFX_BUFF : CGIA_HWCOLOR_GFX_GREEN;
    }
    else {
        // alphanumeric or semigraphics mode, always black
        return CGIA_HWCOLOR_BLACK;
    }
}

static void _cgia_decode_border(cgia_t* vdg, uint64_t pins, size_t y) {
    uint8_t* dst = &(vdg->fb[y * CGIA_FRAMEBUFFER_WIDTH]);
    uint8_t c = _cgia_border_color(pins);
    for (size_t x = 0; x < CGIA_DISPLAY_WIDTH; x++) {
        *dst++ = c;
    }
}

static uint64_t _cgia_decode_scanline(cgia_t* vdg, uint64_t pins, size_t y) {
    uint8_t* dst = &(vdg->fb[(y + CGIA_TOP_BORDER_LINES) * CGIA_FRAMEBUFFER_WIDTH]);
    uint8_t bc = _cgia_border_color(pins);
    void* ud = vdg->user_data;

    // left border
    for (size_t i = 0; i < CGIA_BORDER_PIXELS; i++) {
        *dst++ = bc;
    }

    // visible scanline
    if (pins & CGIA_AG) {
        // one of the 8 graphics modes
        size_t sub_mode = (uint8_t)((pins & (CGIA_GM2 | CGIA_GM1)) / CGIA_GM1);
        if (pins & CGIA_GM0) {
            /*  one of the 'resolution modes' (1 bit == 1 pixel block)
                GM2|GM1:
                    00:    RG1, 128x64, 16 bytes per row
                    01:    RG2, 128x96, 16 bytes per row
                    10:    RG3, 128x192, 16 bytes per row
                    11:    RG6, 256x192, 32 bytes per row
            */
            size_t dots_per_bit = (sub_mode < 3) ? 2 : 1;
            size_t bytes_per_row = (sub_mode < 3) ? 16 : 32;
            size_t row_height = (pins & CGIA_GM2) ? 1 : (pins & CGIA_GM1) ? 2 : 3;
            uint16_t addr = (y / row_height) * bytes_per_row;
            uint8_t fg_color = (pins & CGIA_CSS) ? CGIA_HWCOLOR_GFX_BUFF : CGIA_HWCOLOR_GFX_GREEN;
            for (size_t x = 0; x < bytes_per_row; x++) {
                CGIA_SET_ADDR(pins, addr++);
                pins = vdg->fetch_cb(pins, ud);
                uint8_t m = CGIA_GET_DATA(pins);
                for (int p = 7; p >= 0; p--) {
                    uint8_t c = ((m >> p) & 1) ? fg_color : CGIA_HWCOLOR_BLACK;
                    for (size_t d = 0; d < dots_per_bit; d++) {
                        *dst++ = c;
                    }
                }
            }
        }
        else {
            /*  one of the 'color modes' (2 bits per pixel == 4 colors, CSS select
                lower or upper half of palette)
                GM2|GM1:
                    00: CG1, 64x64, 16 bytes per row
                    01: CG2, 128x64, 32 bytes per row
                    10: CG3, 128x96, 32 bytes per row
                    11: CG6, 128x192, 32 bytes per row
            */
            uint8_t color_offset = (pins & CGIA_CSS) ? 4 : 0;
            size_t dots_per_2bit = (sub_mode == 0) ? 4 : 2;
            size_t bytes_per_row = (sub_mode == 0) ? 16 : 32;
            size_t row_height = (pins & CGIA_GM2) ? ((pins & CGIA_GM1) ? 1 : 2) : 3;
            uint16_t addr = (y / row_height) * bytes_per_row;
            for (size_t x = 0; x < bytes_per_row; x++) {
                CGIA_SET_ADDR(pins, addr++);
                pins = vdg->fetch_cb(pins, ud);
                uint8_t m = CGIA_GET_DATA(pins);
                for (int p = 6; p >= 0; p -= 2) {
                    const uint8_t c = ((m >> p) & 3) + color_offset;
                    for (size_t d = 0; d < dots_per_2bit; d++) {
                        *dst++ = c;
                    }
                }
            }
        }
    }
    else {
        //  we're in alphanumeric/semigraphics mode, one cell is 8x12 pixels

        // the vidmem src address and offset into the font data
        uint16_t addr = (y / 12) * 32;
        uint8_t m;  // the pixel bitmask
        size_t chr_y = y % 12;
        // bit shifters to extract a 2x2 or 2x3 semigraphics 2-bit stack
        size_t shift_2x2 = (1 - (chr_y / 6)) * 2;
        size_t shift_2x3 = (2 - (chr_y / 4)) * 2;
        uint8_t alnum_fg = (pins & CGIA_CSS) ? CGIA_HWCOLOR_ALNUM_ORANGE : CGIA_HWCOLOR_ALNUM_GREEN;
        uint8_t alnum_bg = (pins & CGIA_CSS) ? CGIA_HWCOLOR_ALNUM_DARK_ORANGE : CGIA_HWCOLOR_ALNUM_DARK_GREEN;
        for (size_t x = 0; x < 32; x++) {
            CGIA_SET_ADDR(pins, addr++);
            pins = vdg->fetch_cb(pins, ud);
            uint8_t chr = CGIA_GET_DATA(pins);
            if (pins & CGIA_AS) {
                // semigraphics mode
                uint8_t fg_color;
                if (pins & CGIA_INTEXT) {
                    /*  2x3 semigraphics, 2 color sets at 4 colors (selected by CSS pin)
                        |C1|C0|L5|L4|L3|L2|L1|L0|

                        +--+--+
                        |L5|L4|
                        +--+--+
                        |L3|L2|
                        +--+--+
                        |L1|L0|
                        +--+--+
                    */

                    // extract the 2 horizontal bits from one of the 3 stacks
                    m = (chr >> shift_2x3) & 3;
                    // 2 bits of color, CSS bit selects upper or lower half of color palette
                    fg_color = ((chr >> 6) & 3) + ((pins & CGIA_CSS) ? 4 : 0);
                }
                else {
                    /*  2x2 semigraphics, 8 colors + black
                        |xx|C2|C1|C0|L3|L2|L1|L0|

                        +--+--+
                        |L3|L2|
                        +--+--+
                        |L1|L0|
                        +--+--+
                    */

                    // extract the 2 horizontal bits from the upper or lower stack
                    m = (chr >> shift_2x2) & 3;
                    // 3 color bits directly point into the color palette
                    fg_color = (chr >> 4) & 7;
                }
                // write the horizontal pixel blocks (2 blocks @ 4 pixel each)
                for (int p = 1; p >= 0; p--) {
                    uint8_t c = (m & (1 << p)) ? fg_color : CGIA_HWCOLOR_BLACK;
                    *dst++ = c;
                    *dst++ = c;
                    *dst++ = c;
                    *dst++ = c;
                }
            }
            else {
                /*  alphanumeric mode
                    FIXME: INT_EXT (switch between internal and external font
                */
                uint8_t m = _cgia_font[(chr & 0x3F) * 12 + chr_y];
                if (pins & CGIA_INV) {
                    m = ~m;
                }
                for (int p = 7; p >= 0; p--) {
                    *dst++ = m & (1 << p) ? alnum_fg : alnum_bg;
                }
            }
        }
    }

    // right border
    for (size_t i = 0; i < CGIA_BORDER_PIXELS; i++) {
        *dst++ = bc;
    }

    return pins;
}

uint64_t cgia_tick(cgia_t* vdg, uint64_t pins) {
    // output pins will be set each tick
    pins &= ~(CGIA_HS | CGIA_FS);

    // horizontal and vertical field sync
    vdg->h_count += CGIA_FIXEDPOINT_SCALE;
    if ((vdg->h_count >= vdg->h_sync_start) && (vdg->h_count < vdg->h_sync_end)) {
        // horizontal sync on
        pins |= CGIA_HS;
        if (vdg->l_count == CGIA_FSYNC_START) {
            // switch field sync on
            vdg->fs = true;
        }
    }
    if (vdg->fs) {
        pins |= CGIA_FS;
    }

    // rewind horizontal counter?
    if (vdg->h_count >= vdg->h_period) {
        vdg->h_count -= vdg->h_period;
        vdg->l_count++;
        if (vdg->l_count >= CGIA_ALL_LINES) {
            // rewind line counter, field sync off
            vdg->l_count = 0;
            vdg->fs = false;
        }
        if (vdg->l_count < CGIA_VBLANK_LINES) {
            // inside vblank area, nothing to do
        }
        else if (vdg->l_count < CGIA_DISPLAY_START) {
            // top border
            size_t y = (size_t)(vdg->l_count - CGIA_VBLANK_LINES);
            _cgia_decode_border(vdg, pins, y);
        }
        else if (vdg->l_count < CGIA_DISPLAY_END) {
            // visible area
            size_t y = (size_t)(vdg->l_count - CGIA_DISPLAY_START);
            pins = _cgia_decode_scanline(vdg, pins, y);
        }
        else if (vdg->l_count < CGIA_BOTTOM_BORDER_END) {
            // bottom border
            size_t y = (size_t)(vdg->l_count - CGIA_VBLANK_LINES);
            _cgia_decode_border(vdg, pins, y);
        }
    }
    vdg->pins = pins;
    return pins;
}

void cgia_snapshot_onsave(cgia_t* snapshot) {
    CHIPS_ASSERT(snapshot);
    snapshot->fetch_cb = 0;
    snapshot->user_data = 0;
    snapshot->fb = 0;
}

void cgia_snapshot_onload(cgia_t* snapshot, cgia_t* sys) {
    CHIPS_ASSERT(snapshot && sys);
    snapshot->fetch_cb = sys->fetch_cb;
    snapshot->user_data = sys->user_data;
    snapshot->fb = sys->fb;
}
