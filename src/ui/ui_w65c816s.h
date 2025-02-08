#pragma once
/*#
    # ui_w65c816s.h

    Debug visualization UI for w65c816s.h

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

    Include the following headers before the including the *declaration*:
        - w65c816s.h
        - ui_chip.h

    Include the following headers before including the *implementation*:
        - imgui.h
        - w65c816s.h
        - ui_chip.h
        - ui_util.h
        - ui_settings.h

    All strings provided to ui_w65816_init() must remain alive until
    ui_w65816_discard() is called!

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
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* setup parameters for ui_w65816_init()
    NOTE: all string data must remain alive until ui_w65816_discard()!
*/
typedef struct {
    const char* title;          /* window title */
    w65816_t* cpu;               /* w65816_t instance to track */
    int x, y;                   /* initial window position */
    int w, h;                   /* initial window width and height */
    bool open;                  /* initial open state */
    ui_chip_desc_t chip_desc;   /* chips visualization desc */
} ui_w65816_desc_t;

typedef struct {
    const char* title;
    w65816_t* cpu;
    float init_x, init_y;
    float init_w, init_h;
    bool open;
    bool last_open;
    bool valid;
    ui_chip_t chip;
} ui_w65816_t;

void ui_w65816_init(ui_w65816_t* win, const ui_w65816_desc_t* desc);
void ui_w65816_discard(ui_w65816_t* win);
void ui_w65816_draw(ui_w65816_t* win);
void ui_w65816_save_settings(ui_w65816_t* win, ui_settings_t* settings);
void ui_w65816_load_settings(ui_w65816_t* win, const ui_settings_t* settings);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*-- IMPLEMENTATION (include in C++ source) ----------------------------------*/
#ifdef CHIPS_UI_IMPL
#ifndef __cplusplus
#error "implementation must be compiled as C++"
#endif
#include <string.h> /* memset */
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

void ui_w65816_init(ui_w65816_t* win, const ui_w65816_desc_t* desc) {
    CHIPS_ASSERT(win && desc);
    CHIPS_ASSERT(desc->title);
    CHIPS_ASSERT(desc->cpu);
    memset(win, 0, sizeof(ui_w65816_t));
    win->title = desc->title;
    win->cpu = desc->cpu;
    win->init_x = (float) desc->x;
    win->init_y = (float) desc->y;
    win->init_w = (float) ((desc->w == 0) ? 360 : desc->w);
    win->init_h = (float) ((desc->h == 0) ? 440 : desc->h);
    win->open = win->last_open = desc->open;
    win->valid = true;
    ui_chip_init(&win->chip, &desc->chip_desc);
}

void ui_w65816_discard(ui_w65816_t* win) {
    CHIPS_ASSERT(win && win->valid);
    win->valid = false;
}

static void _ui_w65816_regs(ui_w65816_t* win) {
    w65816_t* cpu = win->cpu;
    ImGui::Text("A:  %02X", cpu->A);
    ImGui::Text("X:  %02X", cpu->X);
    ImGui::Text("Y:  %02X", cpu->Y);
    ImGui::Text("S:  %02X", cpu->S);
    const uint8_t f = cpu->P;
    char f_str[9] = {
        (f & W65816_NF) ? 'N':'-',
        (f & W65816_VF) ? 'V':'-',
        (f & W65816_XF) ? 'X':'-',
        (f & W65816_BF) ? 'B':'-',
        (f & W65816_DF) ? 'D':'-',
        (f & W65816_IF) ? 'I':'-',
        (f & W65816_ZF) ? 'Z':'-',
        (f & W65816_CF) ? 'C':'-',
        0
    };
    ImGui::Text("P:  %02X %s", f, f_str);
    ImGui::Text("PC: %04X", cpu->PC);
    ImGui::Separator();
    ImGui::Text("BCD: %s", cpu->bcd_enabled ? "enabled":"disabled");
}

void ui_w65816_draw(ui_w65816_t* win) {
    CHIPS_ASSERT(win && win->valid && win->cpu);
    ui_util_handle_window_open_dirty(&win->open, &win->last_open);
    if (!win->open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(win->init_x, win->init_y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(win->init_w, win->init_h), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(win->title, &win->open)) {
        ImGui::BeginChild("##w65816_chip", ImVec2(176, 0), true);
        ui_chip_draw(&win->chip, win->cpu->PINS);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##w65816_regs", ImVec2(0, 0), true);
        _ui_w65816_regs(win);
        ImGui::EndChild();
    }
    ImGui::End();
}

void ui_w65816_save_settings(ui_w65816_t* win, ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    ui_settings_add(settings, win->title, win->open);
}

void ui_w65816_load_settings(ui_w65816_t* win, const ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    win->open = ui_settings_isopen(settings, win->title);
}
#endif /* CHIPS_UI_IMPL */
