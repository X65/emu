#pragma once
/*#
    # ui_x65.h

    Integrated debugging UI for x65.h

    Do this:
    ~~~C
    #define CHIPS_UI_IMPL
    ~~~
    before you include this file in *one* C++ file to create the
    implementation.

    Optionally provide the following macros with your own implementation

    ~~~C
    CHIPS_ASSERT(c)
    ~~~
        your own assert macro (default: assert(c))

    Include the following headers (and their depenencies) before including
    ui_x65.h both for the declaration and implementation.

    - x65.h
    - mem.h
    - ui_chip.h
    - ui_util.h
    - ui_settings.h
    - ui_m6502.h
    - ui_m6526.h
    - ui_m6569.h
    - ui_m6581.h
    - ui_audio.h
    - ui_display.h
    - ui_dasm.h
    - ui_dbg.h
    - ui_memedit.h
    - ui_memmap.h
    - ui_kbd.h
    - ui_snapshot.h

    ## zlib/libpng license

    Copyright (c) 2018 Andre Weissflog
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
#*/
#include "systems/x65.h"
#define UI_DBG_USE_M6502
#include "ui/ui_settings.h"
#include "ui/ui_util.h"
#include "ui/ui_audio.h"
#include "ui/ui_chip.h"
#include "ui/ui_console.h"
#include "ui/ui_dasm.h"
#include "ui/ui_dbg.h"
#include "ui/ui_display.h"
#include "ui/ui_kbd.h"
#include "ui/ui_m6502.h"
#include "ui/ui_m6526.h"
#include "ui/ui_cgia.h"
#include "ui/ui_m6581.h"
#include "ui/ui_memedit.h"
#include "ui/ui_memmap.h"
#include "ui/ui_snapshot.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// reboot callback
typedef void (*ui_x65_boot_cb)(x65_t* sys);

// setup params for ui_x65_init()
typedef struct {
    x65_t* x65;                              // pointer to x65_t instance to track
    ui_x65_boot_cb boot_cb;                  // reboot callback function
    ui_dbg_texture_callbacks_t dbg_texture;  // texture create/update/destroy callbacks
    ui_dbg_debug_callbacks_t dbg_debug;
    ui_dbg_keys_desc_t dbg_keys;  // user-defined hotkeys for ui_dbg_t
    ui_snapshot_desc_t snapshot;  // snapshot UI setup params
} ui_x65_desc_t;

typedef struct {
    x65_t* x65;
    int dbg_scanline;
    ui_x65_boot_cb boot_cb;
    ui_m6502_t cpu;
    ui_m6526_t cia[2];
    ui_m6581_t sid;
    ui_cgia_t cgia;
    ui_console_t ria_uart;
    ui_audio_t audio;
    ui_display_t display;
    ui_kbd_t kbd;
    ui_memmap_t memmap;
    ui_memedit_t memedit[4];
    ui_dasm_t dasm[4];
    ui_dbg_t dbg;
    ui_snapshot_t snapshot;
    bool show_about;
} ui_x65_t;

typedef struct {
    ui_display_frame_t display;
} ui_x65_frame_t;

void ui_x65_init(ui_x65_t* ui, const ui_x65_desc_t* desc);
void ui_x65_discard(ui_x65_t* ui);
void ui_x65_draw(ui_x65_t* ui, const ui_x65_frame_t* frame);
chips_debug_t ui_x65_get_debug(ui_x65_t* ui);
void ui_x65_save_settings(ui_x65_t* ui, ui_settings_t* settings);
void ui_x65_load_settings(ui_x65_t* ui, const ui_settings_t* settings);

#ifdef __cplusplus
} /* extern "C" */
#endif
