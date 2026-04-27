#pragma once
/*#
    # ui_crt.h

    UI panel to toggle and tune the CRT post-process effect implemented in
    common/gfx.c.

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

    Include the following headers before including the *implementation*:
        - imgui.h
        - ui_settings.h
        - common/gfx.h
#*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* title;
    int x, y;
    int w, h;
    bool open;
} ui_crt_desc_t;

typedef struct {
    const char* title;
    float init_x, init_y;
    float init_w, init_h;
    bool open;
    bool last_open;
    bool valid;
} ui_crt_t;

void ui_crt_init(ui_crt_t* win, const ui_crt_desc_t* desc);
void ui_crt_discard(ui_crt_t* win);
void ui_crt_draw(ui_crt_t* win);
void ui_crt_save_settings(ui_crt_t* win, ui_settings_t* settings);
void ui_crt_load_settings(ui_crt_t* win, const ui_settings_t* settings);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*-- IMPLEMENTATION (include in C++ source) ----------------------------------*/
#ifdef CHIPS_UI_IMPL
#ifndef __cplusplus
#error "implementation must be compiled as C++"
#endif
#include <string.h> /* memset */
#include "common/gfx.h"
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

void ui_crt_init(ui_crt_t* win, const ui_crt_desc_t* desc) {
    CHIPS_ASSERT(win && desc);
    CHIPS_ASSERT(desc->title);
    memset(win, 0, sizeof(ui_crt_t));
    win->title = desc->title;
    win->init_x = (float) desc->x;
    win->init_y = (float) desc->y;
    win->init_w = (float) ((desc->w == 0) ? 320 : desc->w);
    win->init_h = (float) ((desc->h == 0) ? 248 : desc->h);
    win->open = win->last_open = desc->open;
    win->valid = true;
}

void ui_crt_discard(ui_crt_t* win) {
    CHIPS_ASSERT(win && win->valid);
    win->valid = false;
}

void ui_crt_draw(ui_crt_t* win) {
    CHIPS_ASSERT(win && win->valid && win->title);
    ui_util_handle_window_open_dirty(&win->open, &win->last_open);
    if (!win->open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(win->init_x, win->init_y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(win->init_w, win->init_h), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(win->title, &win->open)) {
        bool enabled = gfx_crt_enabled();
        if (ImGui::Checkbox("Enable CRT effect", &enabled)) {
            gfx_crt_set_enabled(enabled);
        }
        ImGui::Separator();
        gfx_crt_params_t p = gfx_crt_get_params();
        bool changed = false;
        changed |= ImGui::SliderFloat("Scanlines",  &p.scanline_intensity, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("RGB mask",   &p.mask_intensity,     0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Curvature",  &p.curvature,          0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Vignette",   &p.vignette,           0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Blur",       &p.blur,               0.0f, 2.0f);
        changed |= ImGui::SliderFloat("Gamma",      &p.gamma,              0.5f, 2.5f);
        if (changed) {
            gfx_crt_set_params(p);
        }
        if (ImGui::Button("Reset to defaults")) {
            gfx_crt_set_params(gfx_crt_default_params());
        }
    }
    ImGui::End();
}

void ui_crt_save_settings(ui_crt_t* win, ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    ui_settings_add(settings, win->title, win->open);
}

void ui_crt_load_settings(ui_crt_t* win, const ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    win->open = ui_settings_isopen(settings, win->title);
}
#endif /* CHIPS_UI_IMPL */
