#pragma once
/*#
  # ui_vram_debugger.h

    Visual VRAM/memory debugger. Renders a memory region through CGIA decode
    modes (MODE 0/1/2/3/7 and a sprite-style palette) so the user can hunt
    for visual assets — charsets, tilesets, bitmaps, sprite pixel data — by
    eye. Memory is accessed through the same banked layer/bank/addr callback
    used by ui_memedit, so the panel covers the full 16 MB CPU address
    space, not just VRAM.

    All string data provided to ui_vram_debugger_init() must remain alive
    until ui_vram_debugger_discard() is called!

    ## 0BSD license

    Copyright (c) 2026 Tomasz Sterna

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#*/

#include "chips/cgia.h"
#include "ui/ui_settings.h"
#include "ui/ui_util.h" /* for ui_texture_t */

#ifndef UI_VRAM_DBG_MAX_LAYERS
    #define UI_VRAM_DBG_MAX_LAYERS (16)
#endif

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* read callback matches ui_memedit's: (layer, bank, addr16) -> byte */
typedef uint8_t (*ui_vram_dbg_read_t)(int layer, int bank, uint16_t addr, void* user_data);

typedef enum {
    UI_VRAM_DBG_MODE0 = 0, /* CGIA mode 0 — palette text/tile  (uses chargen) */
    UI_VRAM_DBG_MODE1,     /* CGIA mode 1 — palette bitmap                    */
    UI_VRAM_DBG_MODE2,     /* CGIA mode 2 — attribute text/tile (uses chargen) */
    UI_VRAM_DBG_MODE3,     /* CGIA mode 3 — attribute bitmap                  */
    UI_VRAM_DBG_MODE7,     /* CGIA mode 7 — affine chunky pixmap              */
    UI_VRAM_DBG_SPRITE,    /* hunt for sprite-shaped pixel data               */
    UI_VRAM_DBG_MODE_COUNT
} ui_vram_dbg_mode_t;

typedef struct {
    const char* title;
    cgia_t* cgia;
    int banks; /* total bank count (for clamping) */
    const char* layers[UI_VRAM_DBG_MAX_LAYERS];
    int layer_banks[UI_VRAM_DBG_MAX_LAYERS];
    ui_vram_dbg_read_t read_cb;
    void* user_data;
    int x, y;
    int w, h;
    bool open;
} ui_vram_debugger_desc_t;

typedef struct {
    const char* title;
    cgia_t* cgia;
    ui_vram_dbg_read_t read_cb;
    void* user_data;

    const char* layer_names[UI_VRAM_DBG_MAX_LAYERS];
    int layer_banks[UI_VRAM_DBG_MAX_LAYERS];
    int num_layers;

    float init_x, init_y;
    float init_w, init_h;
    bool open;
    bool last_open;
    bool valid;

    /* view state */
    int layer;             /* index into layer_names */
    int bank;              /* current bank within layer */
    uint16_t addr;         /* start address within bank */
    int mode;              /* ui_vram_dbg_mode_t */
    int bpp;               /* 1..4 — only honored in MODE0 / MODE1 */
    int width;             /* columns; MODE7 stores texture width bits-1 (0..7 => 2..256 px) */
    int row_h;             /* 1..32 — only honored in MODE 0/1/2/3 */
    int chargen_bank;      /* bank for chargen lookup (MODE0/MODE2) */
    uint16_t chargen_addr; /* address within chargen_bank */
    uint8_t colors[8];     /* shared_colors[0..7] — 256-palette indices */
    bool transparent;      /* color 0 transparent (encoder's mapped = !transparent) */
    bool multicolor;
    bool doubled; /* double-width pixel */
    bool show_grid;
    int copy_from_plane; /* 0..3 used by "Apply palette from plane N" preset */

    /* render */
    ui_texture_t tex;
    int tex_w, tex_h;
    uint32_t* pixels; /* tex_w * tex_h RGBA8 */
} ui_vram_debugger_t;

void ui_vram_debugger_init(ui_vram_debugger_t* win, const ui_vram_debugger_desc_t* desc);
void ui_vram_debugger_discard(ui_vram_debugger_t* win);
void ui_vram_debugger_draw(ui_vram_debugger_t* win);
void ui_vram_debugger_save_settings(ui_vram_debugger_t* win, ui_settings_t* settings);
void ui_vram_debugger_load_settings(ui_vram_debugger_t* win, const ui_settings_t* settings);

#ifdef __cplusplus
} /* extern "C" */
#endif
