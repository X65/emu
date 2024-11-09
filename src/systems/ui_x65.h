#pragma once
/*#
    # ui_x65.h
    Integrated debugging UI for x65.h
#*/
#define UI_DASM_USE_M6502
#define UI_DBG_USE_M6502
#include "systems/x65.h"
#include "ui/ui_chip.h"
#include "ui/ui_kbd.h"
#include "ui/ui_memedit.h"
#include "ui/ui_memmap.h"
#include "ui/ui_dasm.h"
#include "ui/ui_dbg.h"
#include "ui/ui_m6502.h"
#include "ui/ui_audio.h"
#include "ui/ui_snapshot.h"

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
    // ui_cgia_t cgia;
    // ui_ymf825_t sd1;
    ui_audio_t audio;
    ui_kbd_t kbd;
    ui_memmap_t memmap;
    ui_memedit_t memedit[4];
    ui_dasm_t dasm[4];
    ui_dbg_t dbg;
    ui_snapshot_t snapshot;
} ui_x65_t;

void ui_x65_init(ui_x65_t* ui, const ui_x65_desc_t* desc);
void ui_x65_discard(ui_x65_t* ui);
void ui_x65_draw(ui_x65_t* ui);
chips_debug_t ui_x65_get_debug(ui_x65_t* ui);

#ifdef __cplusplus
} /* extern "C" */
#endif
