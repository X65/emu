#include "./ui_vram_debugger.h"

#include "imgui.h"
#include "common/ui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef __cplusplus
    #error "implementation must be compiled as C++"
#endif
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

#define cgia_t fwcgia_t
#include "firmware/src/south/cgia/cgia.h"
#undef cgia_t

#define _UI_VRAM_DBG_TEX_MAX_W (384)
#define _UI_VRAM_DBG_TEX_H     (1024)

static const char* _ui_vram_dbg_mode_labels =
    "MODE0 - palette text/tile\0"
    "MODE1 - palette bitmap\0"
    "MODE2 - attribute text/tile\0"
    "MODE3 - attribute bitmap\0"
    "MODE7 - affine chunky\0"
    "Sprites\0\0";

/* ---- helpers ----------------------------------------------------------- */

static inline uint32_t _ui_vram_dbg_color(const ui_vram_debugger_t* w, uint8_t idx) {
    return w->cgia->hwcolors[idx] | 0xFF000000u;
}

static inline void _ui_vram_dbg_reset_colors(ui_vram_debugger_t* w) {
    for (int i = 0; i < 8; i++) {
        /* a useful default ramp from the palette: spread across the 32 unique colors */
        w->colors[i] = (uint8_t)(i + i * 32 * 3);
    }
}

static inline uint8_t _ui_vram_dbg_read(const ui_vram_debugger_t* w, int bank, uint32_t off) {
    /* off may exceed 16-bit when the decoder advances past the bank boundary;
       the read callback only takes 16 bits, so wrap. */
    return w->read_cb ? w->read_cb(w->layer, bank, (uint16_t)off, w->user_data) : 0;
}

static inline int _ui_vram_dbg_ceil_log2(int n) {
    int r = 0;
    int v = 1;
    while (v < n) {
        v <<= 1;
        ++r;
    }
    return r;
}

static inline bool _ui_vram_dbg_is_text_mode(int mode) {
    return mode == UI_VRAM_DBG_MODE0 || mode == UI_VRAM_DBG_MODE2;
}

static inline int _ui_vram_dbg_mode7_width_px(const ui_vram_debugger_t* w) {
    /* MODE7 width control mirrors texture_bits: slider value 0..7 encodes 1..8 bits. */
    int exp = w->width;
    if (exp < 0) exp = 0;
    if (exp > 7) exp = 7;
    return 1 << (exp + 1);
}

static inline int _ui_vram_dbg_text_cell_px_w(const ui_vram_debugger_t* w) {
    int px = w->multicolor ? 4 : 8;
    if (w->doubled) px *= 2;
    return px;
}

static int _ui_vram_dbg_pixel_w(const ui_vram_debugger_t* w) {
    /* render width in output pixels for the current settings */
    if (w->mode == UI_VRAM_DBG_MODE7) {
        return _ui_vram_dbg_mode7_width_px(w);
    }
    int px = w->width * (_ui_vram_dbg_is_text_mode(w->mode) ? _ui_vram_dbg_text_cell_px_w(w) : 8);
    if (w->doubled && !_ui_vram_dbg_is_text_mode(w->mode)
        && (w->mode == UI_VRAM_DBG_MODE1 || w->mode == UI_VRAM_DBG_MODE3)) {
        px *= 2;
    }
    if (px > _UI_VRAM_DBG_TEX_MAX_W) px = _UI_VRAM_DBG_TEX_MAX_W;
    if (px < 1) px = 1;
    return px;
}

static int _ui_vram_dbg_vslider_int_target(const ImVec2& slider_min, const ImVec2& slider_max, int v_min, int v_max) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float grab_padding = 2.0f;
    const float slider_sz = (slider_max.y - slider_min.y) - grab_padding * 2.0f;
    if (slider_sz <= 0.0f) return v_min;

    const float v_range_f = (float)((v_min < v_max) ? (v_max - v_min) : (v_min - v_max));
    float grab_sz = style.GrabMinSize;
    const float unit_grab_sz = slider_sz / (v_range_f + 1.0f);
    if (unit_grab_sz > grab_sz) grab_sz = unit_grab_sz;
    if (grab_sz > slider_sz) grab_sz = slider_sz;

    const float slider_usable_sz = slider_sz - grab_sz;
    const float slider_usable_pos_min = slider_min.y + grab_padding + grab_sz * 0.5f;
    float clicked_t = 0.0f;
    if (slider_usable_sz > 0.0f) {
        clicked_t = (ImGui::GetMousePos().y - slider_usable_pos_min) / slider_usable_sz;
    }
    if (clicked_t < 0.0f) clicked_t = 0.0f;
    if (clicked_t > 1.0f) clicked_t = 1.0f;
    clicked_t = 1.0f - clicked_t; /* vertical sliders invert the axis */

    if (clicked_t <= 0.0f || v_min == v_max) return v_min;
    if (clicked_t >= 1.0f) return v_max;

    const float v_new_off_f = (float)(v_max - v_min) * clicked_t;
    return v_min + (int)(v_new_off_f + (v_min > v_max ? -0.5f : 0.5f));
}

static void _ui_vram_dbg_resize_tex(ui_vram_debugger_t* w) {
    int new_w = _ui_vram_dbg_pixel_w(w);
    if (new_w == w->tex_w) return;
    if (w->tex) ui_destroy_texture(w->tex);
    w->tex_w = new_w;
    w->tex = ui_create_texture(w->tex_w, w->tex_h, "vram_debugger");
}

static void _ui_vram_dbg_clear_pixels(ui_vram_debugger_t* w) {
    memset(w->pixels, 0, (size_t)w->tex_w * (size_t)w->tex_h * sizeof(uint32_t));
}

/* ---- decoders ---------------------------------------------------------- */

/* Emit one pixel (or two if doubled) at (out_x, out_y) into pixels.
   Returns next x position. */
static inline int _emit_px(ui_vram_debugger_t* w, int x, int y, uint32_t rgba, bool doubled) {
    if (y < 0 || y >= w->tex_h) return x + (doubled ? 2 : 1);
    if (x >= 0 && x < w->tex_w) w->pixels[y * w->tex_w + x] = rgba;
    ++x;
    if (doubled) {
        if (x >= 0 && x < w->tex_w) w->pixels[y * w->tex_w + x] = rgba;
        ++x;
    }
    return x;
}

static inline int _ui_vram_dbg_row_h(const ui_vram_debugger_t* w) {
    return w->row_h > 0 ? w->row_h : 1;
}

static inline uint32_t
_ui_vram_dbg_cell_byte_addr(const ui_vram_debugger_t* w, int y, int unit, int units_per_cell_row) {
    const int row_h = _ui_vram_dbg_row_h(w);
    const int cell_row = y / row_h;
    const int row_in_cell = y % row_h;
    return w->addr + (uint32_t)((cell_row * units_per_cell_row + unit) * row_h + row_in_cell);
}

static inline uint32_t _ui_vram_dbg_text_cell_addr(const ui_vram_debugger_t* w, int y, int col, int cols) {
    const int row_h = _ui_vram_dbg_row_h(w);
    const int cell_row = y / row_h;
    return w->addr + (uint32_t)(cell_row * cols + col);
}

static void _ui_vram_dbg_source_tooltip(const ui_vram_debugger_t* w, int x, int y) {
    const bool doubled_mode = w->mode == UI_VRAM_DBG_MODE0 || w->mode == UI_VRAM_DBG_MODE1
        || w->mode == UI_VRAM_DBG_MODE2 || w->mode == UI_VRAM_DBG_MODE3;
    const int scale_x = w->doubled && doubled_mode ? 2 : 1;
    const int logical_x = x / scale_x;

    ImGui::BeginTooltip();
    switch (w->mode) {
        case UI_VRAM_DBG_MODE0:
        case UI_VRAM_DBG_MODE2: {
            const int row_h = _ui_vram_dbg_row_h(w);
            const int char_shift = _ui_vram_dbg_ceil_log2(row_h);
            const int cell_w = _ui_vram_dbg_text_cell_px_w(w);
            const int col = x / cell_w;
            if (col >= 0 && col < w->width) {
                uint32_t chr_addr = _ui_vram_dbg_text_cell_addr(w, y, col, w->width);
                uint8_t chr = _ui_vram_dbg_read(w, w->bank, chr_addr);
                uint8_t chr_idx = chr;
                if (w->mode == UI_VRAM_DBG_MODE0) {
                    if (w->multicolor) {
                        int eff_bpp = (w->bpp - 2) & 0x1;
                        chr_idx = (uint8_t)(chr & ((1u << (8 - eff_bpp)) - 1u));
                    }
                    else {
                        chr_idx = (uint8_t)(chr & ((1u << (9 - w->bpp)) - 1u));
                    }
                }
                uint32_t glyph_addr = w->chargen_addr + ((uint32_t)chr_idx << char_shift) + (uint32_t)(y % row_h);
                uint8_t glyph = _ui_vram_dbg_read(w, w->chargen_bank, glyph_addr);
                ImGui::Text("Cell:  %02X:%04X = %02X %c", w->bank, (uint16_t)chr_addr, chr, isascii(chr) ? chr : ' ');
                ImGui::Text("Glyph: %02X:%04X = %02X", w->chargen_bank, (uint16_t)glyph_addr, glyph);
            }
            break;
        }
        case UI_VRAM_DBG_MODE1: {
            const int cell_w = w->multicolor ? 4 : 8;
            const int col = logical_x / cell_w;
            if (col >= 0 && col < w->width) {
                int byte_idx = 0;
                if (w->multicolor) {
                    byte_idx = w->bpp - 1;
                }
                else {
                    const int pixel_in_cell = logical_x - col * cell_w;
                    byte_idx = (pixel_in_cell * w->bpp) / 8;
                }
                uint32_t addr = _ui_vram_dbg_cell_byte_addr(w, y, col * w->bpp + byte_idx, w->width * w->bpp);
                uint8_t byte = _ui_vram_dbg_read(w, w->bank, addr);
                ImGui::Text("%02X:%04X = %02X", w->bank, (uint16_t)addr, byte);
            }
            break;
        }
        case UI_VRAM_DBG_MODE3: {
            const int cell_w = w->multicolor ? 4 : 8;
            const int col = logical_x / cell_w;
            if (col >= 0 && col < w->width) {
                uint32_t addr = _ui_vram_dbg_cell_byte_addr(w, y, col, w->width);
                uint8_t byte = _ui_vram_dbg_read(w, w->bank, addr);
                ImGui::Text("%02X:%04X = %02X", w->bank, (uint16_t)addr, byte);
            }
            break;
        }
        case UI_VRAM_DBG_MODE7: {
            const int width_px = _ui_vram_dbg_mode7_width_px(w);
            if (x < width_px) {
                uint32_t addr = w->addr + (uint32_t)y * (uint32_t)width_px + (uint32_t)x;
                uint8_t byte = _ui_vram_dbg_read(w, w->bank, addr);
                ImGui::Text("%02X:%04X = %02X", w->bank, (uint16_t)addr, byte);
            }
            break;
        }
        case UI_VRAM_DBG_SPRITE: {
            const int cell_w = w->multicolor ? 4 : 8;
            const int col = logical_x / cell_w;
            if (col >= 0 && col < w->width) {
                uint32_t addr = w->addr + (uint32_t)y * (uint32_t)w->width + (uint32_t)col;
                uint8_t byte = _ui_vram_dbg_read(w, w->bank, addr);
                ImGui::Text("%02X:%04X = %02X", w->bank, (uint16_t)addr, byte);
            }
            break;
        }
        default: break;
    }

    ImGui::EndTooltip();
}

/* MODE 1 — palette bitmap. Row height is the byte stride between cell rows. */
static void _decode_mode1(ui_vram_debugger_t* w) {
    const int bpp = w->bpp;
    const int cols = w->width; /* number of 8-px columns per row */
    const bool multi = w->multicolor;
    const bool dbl = w->doubled;
    const uint8_t mask = (uint8_t)((1u << bpp) - 1u);
    const int bytes_per_col = bpp;

    for (int y = 0; y < w->tex_h; ++y) {
        int x = 0;
        for (int c = 0; c < cols && x < w->tex_w; ++c) {
            /* assemble bpp bytes MSB-first into a chunk */
            uint32_t chunk = 0;
            for (int b = 0; b < bytes_per_col; ++b) {
                const uint32_t off = _ui_vram_dbg_cell_byte_addr(w, y, c * bytes_per_col + b, cols * bytes_per_col);
                chunk = (chunk << 8) | _ui_vram_dbg_read(w, w->bank, off);
            }
            if (multi) {
                for (int sh = 6; sh >= 0 && x < w->tex_w; sh -= 2) {
                    /* multicolor uses 2-bit indices regardless of bpp; encoder's bpp-2 trick */
                    uint8_t idx = (uint8_t)((chunk >> sh) & 0x3);
                    if (!w->transparent || idx) {
                        uint8_t color = w->colors[idx & 7];
                        uint32_t rgba = _ui_vram_dbg_color(w, color);
                        x = _emit_px(w, x, y, rgba, dbl);
                    }
                    else {
                        x += dbl ? 2 : 1;
                    }
                }
            }
            else {
                for (int sh = 7; sh >= 0 && x < w->tex_w; --sh) {
                    uint8_t idx = (uint8_t)((chunk >> (sh * bpp)) & mask);
                    if (!w->transparent || idx) {
                        uint8_t color = w->colors[idx & 7];
                        if (idx > 7) color ^= 0x4; /* half-bright */
                        uint32_t rgba = _ui_vram_dbg_color(w, color);
                        x = _emit_px(w, x, y, rgba, dbl);
                    }
                    else {
                        x += dbl ? 2 : 1;
                    }
                }
            }
        }
    }
}

/* MODE 0 — palette text/tile. chr stream at addr, chargen at chargen_addr. */
static void _decode_mode0(ui_vram_debugger_t* w) {
    const int bpp = w->bpp;
    const int cols = w->width;
    const int row_h = _ui_vram_dbg_row_h(w);
    const int char_shift = _ui_vram_dbg_ceil_log2(row_h);
    const bool multi = w->multicolor;
    const bool dbl = w->doubled;

    for (int y = 0; y < w->tex_h; ++y) {
        const int row = y % row_h;
        for (int c = 0; c < cols; ++c) {
            uint8_t chr = _ui_vram_dbg_read(w, w->bank, _ui_vram_dbg_text_cell_addr(w, y, c, cols));
            uint8_t color_idx;
            uint8_t chr_mask;
            if (multi) {
                int eff_bpp = (bpp - 2) & 0x1; /* 0 or 1 */
                color_idx = (uint8_t)((chr >> (6 - eff_bpp)) & 0x4);
                chr_mask = (uint8_t)((1u << (8 - eff_bpp)) - 1u);
            }
            else {
                color_idx = (uint8_t)((chr >> (8 - bpp)) & 0xE);
                chr_mask = (uint8_t)((1u << (9 - bpp)) - 1u);
            }
            uint8_t chr_lo = (uint8_t)(chr & chr_mask);
            uint8_t bits = _ui_vram_dbg_read(
                w,
                w->chargen_bank,
                w->chargen_addr + ((uint32_t)chr_lo << char_shift) + (uint32_t)row);
            int x = c * _ui_vram_dbg_text_cell_px_w(w);
            if (multi) {
                for (int sh = 6; sh >= 0 && x < w->tex_w; sh -= 2) {
                    uint8_t idx = (uint8_t)(color_idx | ((bits >> sh) & 0x3));
                    if (!w->transparent || idx) {
                        uint8_t color = w->colors[idx & 7];
                        x = _emit_px(w, x, y, _ui_vram_dbg_color(w, color), dbl);
                    }
                    else {
                        x += dbl ? 2 : 1;
                    }
                }
            }
            else {
                for (int sh = 7; sh >= 0 && x < w->tex_w; --sh) {
                    uint8_t idx = (uint8_t)(color_idx | ((bits >> sh) & 0x1));
                    if (!w->transparent || idx) {
                        uint8_t color = w->colors[idx & 7];
                        if (idx > 7) color ^= 0x4;
                        x = _emit_px(w, x, y, _ui_vram_dbg_color(w, color), dbl);
                    }
                    else {
                        x += dbl ? 2 : 1;
                    }
                }
            }
        }
    }
}

/* MODE 3 — attribute bitmap. bg=colors[2], fg=colors[3] substitution. */
static void _decode_mode3(ui_vram_debugger_t* w) {
    const int cols = w->width;
    const bool multi = w->multicolor;
    const bool dbl = w->doubled;

    for (int y = 0; y < w->tex_h; ++y) {
        int x = 0;
        for (int c = 0; c < cols && x < w->tex_w; ++c) {
            uint8_t bits = _ui_vram_dbg_read(w, w->bank, _ui_vram_dbg_cell_byte_addr(w, y, c, cols));
            if (multi) {
                for (int sh = 6; sh >= 0 && x < w->tex_w; sh -= 2) {
                    uint8_t code = (uint8_t)((bits >> sh) & 0x3);
                    uint8_t color = 0;
                    bool transparent = false;
                    switch (code) {
                        case 0:
                            if (w->transparent)
                                transparent = true;
                            else
                                color = w->colors[0];
                            break;
                        case 1: color = w->colors[2]; break;
                        case 2: color = w->colors[3]; break;
                        case 3: color = w->colors[1]; break;
                    }
                    if (transparent) {
                        x += dbl ? 2 : 1;
                    }
                    else {
                        x = _emit_px(w, x, y, _ui_vram_dbg_color(w, color), dbl);
                    }
                }
            }
            else {
                for (int sh = 7; sh >= 0 && x < w->tex_w; --sh) {
                    uint8_t bit_set = (uint8_t)((bits >> sh) & 0x1);
                    if (bit_set) {
                        x = _emit_px(w, x, y, _ui_vram_dbg_color(w, w->colors[3]), dbl);
                    }
                    else if (!w->transparent) {
                        x = _emit_px(w, x, y, _ui_vram_dbg_color(w, w->colors[2]), dbl);
                    }
                    else {
                        x += dbl ? 2 : 1;
                    }
                }
            }
        }
    }
}

/* MODE 2 — attribute text/tile. chr stream + chargen, attribute palette. */
static void _decode_mode2(ui_vram_debugger_t* w) {
    const int cols = w->width;
    const int row_h = _ui_vram_dbg_row_h(w);
    const int char_shift = _ui_vram_dbg_ceil_log2(row_h);
    const bool multi = w->multicolor;
    const bool dbl = w->doubled;

    for (int y = 0; y < w->tex_h; ++y) {
        const int row = y % row_h;
        for (int c = 0; c < cols; ++c) {
            uint8_t chr = _ui_vram_dbg_read(w, w->bank, _ui_vram_dbg_text_cell_addr(w, y, c, cols));
            uint8_t bits =
                _ui_vram_dbg_read(w, w->chargen_bank, w->chargen_addr + ((uint32_t)chr << char_shift) + (uint32_t)row);
            int x = c * _ui_vram_dbg_text_cell_px_w(w);
            if (multi) {
                for (int sh = 6; sh >= 0 && x < w->tex_w; sh -= 2) {
                    uint8_t code = (uint8_t)((bits >> sh) & 0x3);
                    uint8_t color = 0;
                    bool transparent = false;
                    switch (code) {
                        case 0:
                            if (w->transparent)
                                transparent = true;
                            else
                                color = w->colors[0];
                            break;
                        case 1: color = w->colors[2]; break;
                        case 2: color = w->colors[3]; break;
                        case 3: color = w->colors[1]; break;
                    }
                    if (transparent) {
                        x += dbl ? 2 : 1;
                    }
                    else {
                        x = _emit_px(w, x, y, _ui_vram_dbg_color(w, color), dbl);
                    }
                }
            }
            else {
                for (int sh = 7; sh >= 0 && x < w->tex_w; --sh) {
                    uint8_t bit_set = (uint8_t)((bits >> sh) & 0x1);
                    if (bit_set) {
                        x = _emit_px(w, x, y, _ui_vram_dbg_color(w, w->colors[3]), dbl);
                    }
                    else if (!w->transparent) {
                        x = _emit_px(w, x, y, _ui_vram_dbg_color(w, w->colors[2]), dbl);
                    }
                    else {
                        x += dbl ? 2 : 1;
                    }
                }
            }
        }
    }
}

/* MODE 7 — affine chunky pixmap. 1 byte = 1 palette index. */
static void _decode_mode7(ui_vram_debugger_t* w) {
    const int width_px = _ui_vram_dbg_mode7_width_px(w);
    uint32_t off = w->addr;
    for (int y = 0; y < w->tex_h; ++y) {
        for (int x = 0; x < width_px && x < w->tex_w; ++x) {
            uint8_t b = _ui_vram_dbg_read(w, w->bank, off + (uint32_t)x);
            w->pixels[y * w->tex_w + x] = _ui_vram_dbg_color(w, b);
        }
        off += (uint32_t)width_px;
    }
}

/* Sprite mode — flat byte stream rendered with sprite-style palette
   (color 0 transparent always; multicolor uses 4-color, single uses 2-color). */
static void _decode_sprite(ui_vram_debugger_t* w) {
    const int cols = w->width;
    const bool multi = w->multicolor;
    const bool dbl = w->doubled;

    uint32_t off = w->addr;
    for (int y = 0; y < w->tex_h; ++y) {
        int x = 0;
        for (int c = 0; c < cols && x < w->tex_w; ++c) {
            uint8_t bits = _ui_vram_dbg_read(w, w->bank, off + (uint32_t)c);
            if (multi) {
                for (int sh = 6; sh >= 0 && x < w->tex_w; sh -= 2) {
                    uint8_t code = (uint8_t)((bits >> sh) & 0x3);
                    if (code == 0) {
                        x += dbl ? 2 : 1;
                    }
                    else {
                        uint8_t color = w->colors[code - 1]; /* colors[0..2] */
                        x = _emit_px(w, x, y, _ui_vram_dbg_color(w, color), dbl);
                    }
                }
            }
            else {
                for (int sh = 7; sh >= 0 && x < w->tex_w; --sh) {
                    uint8_t bit_set = (uint8_t)((bits >> sh) & 0x1);
                    if (bit_set) {
                        x = _emit_px(w, x, y, _ui_vram_dbg_color(w, w->colors[0]), dbl);
                    }
                    else {
                        x += dbl ? 2 : 1;
                    }
                }
            }
        }
        off += (uint32_t)cols;
    }
}

static void _ui_vram_dbg_decode(ui_vram_debugger_t* w) {
    _ui_vram_dbg_clear_pixels(w);
    switch (w->mode) {
        case UI_VRAM_DBG_MODE0: _decode_mode0(w); break;
        case UI_VRAM_DBG_MODE1: _decode_mode1(w); break;
        case UI_VRAM_DBG_MODE2: _decode_mode2(w); break;
        case UI_VRAM_DBG_MODE3: _decode_mode3(w); break;
        case UI_VRAM_DBG_MODE7: _decode_mode7(w); break;
        case UI_VRAM_DBG_SPRITE: _decode_sprite(w); break;
        default: break;
    }
    if (w->tex && w->pixels) {
        ui_update_texture(w->tex, w->pixels, w->tex_w * w->tex_h * (int)sizeof(uint32_t));
    }
}

/* ---- lifecycle --------------------------------------------------------- */

void ui_vram_debugger_init(ui_vram_debugger_t* win, const ui_vram_debugger_desc_t* desc) {
    CHIPS_ASSERT(win && desc);
    CHIPS_ASSERT(desc->title);
    CHIPS_ASSERT(desc->cgia);
    memset(win, 0, sizeof(ui_vram_debugger_t));
    win->title = desc->title;
    win->cgia = desc->cgia;
    win->read_cb = desc->read_cb;
    win->user_data = desc->user_data;
    win->init_x = (float)desc->x;
    win->init_y = (float)desc->y;
    win->init_w = (float)((desc->w == 0) ? 786 : desc->w);
    win->init_h = (float)((desc->h == 0) ? 418 : desc->h);
    win->open = win->last_open = desc->open;

    for (int i = 0; i < UI_VRAM_DBG_MAX_LAYERS; i++) {
        if (desc->layers[i]) {
            win->layer_names[i] = desc->layers[i];
            win->layer_banks[i] = desc->layer_banks[i];
            win->num_layers++;
        }
        else {
            break;
        }
    }

    /* defaults */
    win->layer = 0;
    win->bank = 0;
    win->addr = 0x0000;
    win->mode = UI_VRAM_DBG_MODE1;
    win->bpp = 1;
    win->width = 16;
    win->row_h = 8;
    win->chargen_bank = 0;
    win->chargen_addr = 0x0000;
    win->transparent = false;
    win->multicolor = false;
    win->doubled = false;
    win->show_grid = false;
    win->copy_from_plane = 0;
    _ui_vram_dbg_reset_colors(win);

    win->tex_w = _UI_VRAM_DBG_TEX_MAX_W;
    win->tex_h = _UI_VRAM_DBG_TEX_H;
    win->pixels = (uint32_t*)calloc((size_t)win->tex_w * (size_t)win->tex_h, sizeof(uint32_t));
    win->tex = ui_create_texture(win->tex_w, win->tex_h, "vram_debugger");

    win->valid = true;
}

void ui_vram_debugger_discard(ui_vram_debugger_t* win) {
    CHIPS_ASSERT(win && win->valid);
    if (win->tex) {
        ui_destroy_texture(win->tex);
        win->tex = 0;
    }
    if (win->pixels) {
        free(win->pixels);
        win->pixels = nullptr;
    }
    win->valid = false;
}

void ui_vram_debugger_save_settings(ui_vram_debugger_t* win, ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    ui_settings_add(settings, win->title, win->open);
}

void ui_vram_debugger_load_settings(ui_vram_debugger_t* win, const ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    win->open = ui_settings_isopen(settings, win->title);
}

/* ---- draw -------------------------------------------------------------- */

static bool _is_mode_chargen(int mode) {
    return mode == UI_VRAM_DBG_MODE0 || mode == UI_VRAM_DBG_MODE2;
}

static bool _is_mode_palette_bpp(int mode) {
    return mode == UI_VRAM_DBG_MODE0 || mode == UI_VRAM_DBG_MODE1;
}

static bool _is_mode_with_doubled(int mode) {
    return mode == UI_VRAM_DBG_MODE0 || mode == UI_VRAM_DBG_MODE1 || mode == UI_VRAM_DBG_MODE2
        || mode == UI_VRAM_DBG_MODE3;
}

static bool _is_mode_with_multicolor(int mode) {
    return mode == UI_VRAM_DBG_MODE0 || mode == UI_VRAM_DBG_MODE1 || mode == UI_VRAM_DBG_MODE2
        || mode == UI_VRAM_DBG_MODE3 || mode == UI_VRAM_DBG_SPRITE;
}

static bool _is_mode_with_row_h(int mode) {
    return mode == UI_VRAM_DBG_MODE0 || mode == UI_VRAM_DBG_MODE1 || mode == UI_VRAM_DBG_MODE2
        || mode == UI_VRAM_DBG_MODE3;
}

/* 256-color palette popup: 16x16 grid, click sets *idx. */
static void _draw_palette_popup(const ui_vram_debugger_t* w, const char* popup_id, uint8_t* idx) {
    if (ImGui::BeginPopup(popup_id)) {
        ImGui::Text("Pick color (current: %02X)", *idx);
        ImGui::Separator();
        for (int i = 0; i < CGIA_COLORS_NUM; i++) {
            ImGui::PushID(i);
            ImVec4 c = ImColor(w->cgia->hwcolors[i] | 0xFF000000u);
            char id[32];
            snprintf(id, sizeof(id), "##c%d", i);
            if (ImGui::ColorButton(id, c, ImGuiColorEditFlags_NoAlpha, ImVec2(14, 14))) {
                *idx = (uint8_t)i;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
            if (((i + 1) % 16) != 0) ImGui::SameLine();
        }
        ImGui::EndPopup();
    }
}

/* Render a single color slot button that opens the picker popup. */
static void _draw_color_slot(const ui_vram_debugger_t* w, int slot, uint8_t* idx, bool enabled) {
    ImGui::PushID(slot);
    char popup_id[24];
    snprintf(popup_id, sizeof(popup_id), "pal_pick_%d", slot);
    ImVec4 c = ImColor(w->cgia->hwcolors[*idx] | 0xFF000000u);
    if (!enabled) ImGui::BeginDisabled();
    if (ImGui::ColorButton("##slot", c, ImGuiColorEditFlags_NoAlpha, ImVec2(22, 22))) {
        ImGui::OpenPopup(popup_id);
    }
    if (!enabled) ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Color %d: %02X", slot, *idx);
    }
    _draw_palette_popup(w, popup_id, idx);
    ImGui::PopID();
}

void ui_vram_debugger_draw(ui_vram_debugger_t* win) {
    CHIPS_ASSERT(win && win->valid);
    ui_util_handle_window_open_dirty(&win->open, &win->last_open);
    if (!win->open) return;

    ImGui::SetNextWindowPos(ImVec2(win->init_x, win->init_y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(win->init_w, win->init_h), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(win->title, &win->open)) {
        ImGui::End();
        return;
    }

    /* re-decode every frame so live memory updates show through */
    _ui_vram_dbg_resize_tex(win);
    _ui_vram_dbg_decode(win);

    /* memedit-style sizing for hex inputs */
    const ImGuiStyle& style = ImGui::GetStyle();
    const float gw = ImGui::CalcTextSize("F").x + 1.0f;
    const float stepper_extra_width = 2 * ImGui::GetFrameHeight() + style.ItemInnerSpacing.x * 2.0f;
    const float bank_input_width = 2 * gw + stepper_extra_width + style.FramePadding.x * 2.0f;
    const float addr_input_width = 4 * gw + stepper_extra_width + style.FramePadding.x * 2.0f;
    static const int step = 1, step_fast = 0x10;

    /* ---------------- LEFT: image + side scrollbar ---------------- */
    const float right_w = 344.0f;
    ImGui::BeginChild("##vram_dbg_image", ImVec2(-right_w, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    const bool image_hovered = ImGui::IsWindowHovered();

    /* address scrollbar — vertical slider beside the image, drives `addr`.
       step = bytes per row so each notch advances one rendered row. */
    int bytes_per_row = win->width;
    if (win->mode == UI_VRAM_DBG_MODE0 || win->mode == UI_VRAM_DBG_MODE2) {
        bytes_per_row = win->width; /* tile codes: 1 byte per tile */
    }
    else if (win->mode == UI_VRAM_DBG_MODE1) {
        bytes_per_row = win->width * win->bpp;
    }
    else if (win->mode == UI_VRAM_DBG_MODE3) {
        bytes_per_row = win->width;
    }
    else if (win->mode == UI_VRAM_DBG_MODE7) {
        bytes_per_row = _ui_vram_dbg_mode7_width_px(win);
    }
    else if (win->mode == UI_VRAM_DBG_SPRITE) {
        bytes_per_row = win->width;
    }
    if (bytes_per_row < 1) bytes_per_row = 1;

    int max_addr = 0xFFFF; /* one bank window; user changes bank to scroll further */
    int addr_int = (int)win->addr;
    const float mouse_wheel = ImGui::GetIO().MouseWheel;
    if (image_hovered && mouse_wheel != 0.0f) {
        addr_int -= (mouse_wheel > 0.0f) ? 0x100 : -0x100;
        if (addr_int < 0) addr_int = 0;
        if (addr_int > max_addr) addr_int = max_addr;
        win->addr = (uint16_t)addr_int;
    }
    /* Fit the preview height to the current left pane height. */
    float img_h = ImGui::GetContentRegionAvail().y;
    if (img_h > (float)win->tex_h) img_h = (float)win->tex_h;
    if (img_h < ImGui::GetFrameHeight()) img_h = ImGui::GetFrameHeight();

    if (ImGui::VSliderInt("##addr_scroll", ImVec2(18, img_h), &addr_int, max_addr, 0, "")) {
        if (addr_int < 0) addr_int = 0;
        if (addr_int > max_addr) addr_int = max_addr;
        win->addr = (uint16_t)addr_int;
    }
    if (ImGui::IsItemHovered()) {
        const ImVec2 slider_min = ImGui::GetItemRectMin();
        const ImVec2 slider_max = ImGui::GetItemRectMax();
        const int target_addr = _ui_vram_dbg_vslider_int_target(slider_min, slider_max, max_addr, 0);
        ImGui::SetTooltip("Address: %04X", target_addr);
    }
    ImGui::SameLine();

    /* The image. Drawn at 1:1; scrollbar above scrolls memory window. */
    ImVec2 img_size = ImVec2((float)win->tex_w, img_h);
    ImVec2 img_pos = ImGui::GetCursorScreenPos();
    ImGui::Image(win->tex, img_size, ImVec2(0, 0), ImVec2(1.0f, img_h / (float)win->tex_h));
    if (ImGui::IsItemHovered()) {
        ImVec2 mouse = ImGui::GetMousePos();
        int px = (int)(mouse.x - img_pos.x);
        int py = (int)(mouse.y - img_pos.y);
        if (px < 0) px = 0;
        if (py < 0) py = 0;
        if (px >= win->tex_w) px = win->tex_w - 1;
        if (py >= (int)img_h) py = (int)img_h - 1;
        _ui_vram_dbg_source_tooltip(win, px, py);
    }

    if (win->show_grid) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 gcol = IM_COL32(255, 255, 255, 64);
        int col_w_px = (win->mode == UI_VRAM_DBG_MODE7) ? _ui_vram_dbg_mode7_width_px(win) : 8;
        if (col_w_px < 1) col_w_px = 1;
        for (int x = col_w_px; x < win->tex_w; x += col_w_px) {
            dl->AddLine(ImVec2(img_pos.x + x, img_pos.y), ImVec2(img_pos.x + x, img_pos.y + img_h), gcol);
        }
        if (_is_mode_with_row_h(win->mode) && win->row_h > 0) {
            for (int y = win->row_h; y < (int)img_h; y += win->row_h) {
                dl->AddLine(ImVec2(img_pos.x, img_pos.y + y), ImVec2(img_pos.x + win->tex_w, img_pos.y + y), gcol);
            }
        }
    }

    ImGui::EndChild();
    ImGui::SameLine();

    /* ---------------- RIGHT: controls ---------------- */
    ImGui::BeginChild("##vram_dbg_ctl", ImVec2(0, 0), true);

    /* Source memory */
    ImGui::SeparatorText("Source");
    ImGui::Combo("View", &win->layer, win->layer_names, win->num_layers);
    int max_bank = (win->num_layers > 0) ? win->layer_banks[win->layer] : 1;
    if (max_bank > 1) {
        ImGui::SetNextItemWidth(bank_input_width);
        static const int bstep = 1, bstep_fast = 16;
        ImGui::InputScalar(
            "Bank##bank_idx",
            ImGuiDataType_S32,
            &win->bank,
            &bstep,
            &bstep_fast,
            "%02X",
            ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
        if (win->bank < 0) win->bank = 0;
        if (win->bank >= max_bank) win->bank = max_bank - 1;
        ImGui::SameLine();
    }
    {
        ImGui::SetNextItemWidth(addr_input_width);
        int addr_local = (int)win->addr;
        if (ImGui::InputScalar(
                "Address",
                ImGuiDataType_S32,
                &addr_local,
                &step,
                &step_fast,
                "%04X",
                ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase)) {
            if (addr_local < 0) addr_local = 0;
            if (addr_local > 0xFFFF) addr_local = 0xFFFF;
            win->addr = (uint16_t)addr_local;
        }
    }

    /* Mode */
    ImGui::SeparatorText("Mode");
    ImGui::Combo("Mode", &win->mode, _ui_vram_dbg_mode_labels);

    /* Color depth */
    {
        bool en = _is_mode_palette_bpp(win->mode);
        if (!en) ImGui::BeginDisabled();
        if (ImGui::SliderInt("Color depth", &win->bpp, 1, 4, "%d bpp")) {
            if (win->bpp < 1) win->bpp = 1;
            if (win->bpp > 4) win->bpp = 4;
        }
        if (!en) ImGui::EndDisabled();
    }

    /* Width */
    if (win->mode == UI_VRAM_DBG_MODE7) {
        if (win->width < 0) win->width = 0;
        if (win->width > 7) win->width = 7;
        char width_label[16];
        snprintf(width_label, sizeof(width_label), "%d px", _ui_vram_dbg_mode7_width_px(win));
        if (ImGui::SliderInt("Width", &win->width, 0, 7, width_label)) {
            if (win->width < 0) win->width = 0;
            if (win->width > 7) win->width = 7;
        }
    }
    else {
        int max_w = 48;
        if (win->width < 1) win->width = 1;
        if (win->width > max_w) win->width = max_w;
        if (ImGui::SliderInt("Width", &win->width, 1, max_w, "%d cols")) {
            if (win->width < 1) win->width = 1;
            if (win->width > max_w) win->width = max_w;
        }
    }

    /* Row/tile height */
    {
        bool en = _is_mode_with_row_h(win->mode);
        if (!en) ImGui::BeginDisabled();
        const char* lbl =
            (win->mode == UI_VRAM_DBG_MODE0 || win->mode == UI_VRAM_DBG_MODE2) ? "Tile height" : "Row height";
        if (ImGui::SliderInt(lbl, &win->row_h, 1, 32)) {
            if (win->row_h < 1) win->row_h = 1;
            if (win->row_h > 32) win->row_h = 32;
        }
        if (!en) ImGui::EndDisabled();
    }

    /* Chargen address (MODE0/MODE2) */
    {
        bool en = _is_mode_chargen(win->mode);
        if (!en) ImGui::BeginDisabled();
        if (max_bank > 1) {
            ImGui::SetNextItemWidth(bank_input_width);
            static const int s = 1, sf = 16;
            ImGui::InputScalar(
                "Bank##chargen_bank",
                ImGuiDataType_S32,
                &win->chargen_bank,
                &s,
                &sf,
                "%02X",
                ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
            if (win->chargen_bank < 0) win->chargen_bank = 0;
            if (win->chargen_bank >= max_bank) win->chargen_bank = max_bank - 1;
            ImGui::SameLine();
        }
        ImGui::SetNextItemWidth(addr_input_width);
        int cga = (int)win->chargen_addr;
        if (ImGui::InputScalar(
                "Chargen addr",
                ImGuiDataType_S32,
                &cga,
                &step,
                &step_fast,
                "%04X",
                ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase)) {
            if (cga < 0) cga = 0;
            if (cga > 0xFFFF) cga = 0xFFFF;
            win->chargen_addr = (uint16_t)cga;
        }
        if (!en) ImGui::EndDisabled();
    }

    /* Toggles */
    {
        bool mc_en = _is_mode_with_multicolor(win->mode);
        bool db_en = _is_mode_with_doubled(win->mode);
        if (ImGui::BeginTable("##vram_dbg_toggles", 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextColumn();
            if (!mc_en) ImGui::BeginDisabled();
            ImGui::Checkbox("Multicolor", &win->multicolor);
            if (!mc_en) ImGui::EndDisabled();

            ImGui::TableNextColumn();
            if (!db_en) ImGui::BeginDisabled();
            ImGui::Checkbox("Double width", &win->doubled);
            if (!db_en) ImGui::EndDisabled();

            ImGui::TableNextColumn();
            ImGui::Checkbox("Transparent", &win->transparent);

            ImGui::TableNextColumn();
            ImGui::Checkbox("Show grid", &win->show_grid);

            ImGui::EndTable();
        }
    }

    /* Colors */
    ImGui::SeparatorText("Colors");
    for (int i = 0; i < 8; i++) {
        bool en = true;
        if (win->mode == UI_VRAM_DBG_SPRITE) en = (i < 3);
        _draw_color_slot(win, i, &win->colors[i], en);
        if (i != 7) ImGui::SameLine();
    }
    {
        ImGui::SetNextItemWidth(120);
        ImGui::Combo("##copy_from", &win->copy_from_plane, "Plane 0\0Plane 1\0Plane 2\0Plane 3\0");
        ImGui::SameLine();
        if (ImGui::Button("Copy palette")) {
            const fwcgia_t* chip = (const fwcgia_t*)win->cgia->chip;
            if (chip) {
                int p = win->copy_from_plane;
                if (p < 0) p = 0;
                if (p >= CGIA_PLANES) p = CGIA_PLANES - 1;
                for (int i = 0; i < 8; i++) {
                    win->colors[i] = chip->plane[p].bckgnd.color[i];
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            _ui_vram_dbg_reset_colors(win);
        }
    }

    ImGui::EndChild();
    ImGui::End();
}
