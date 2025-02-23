/*#
    # ui_tca6416a.h

    Debug visualization UI for tca6416a.h

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
        - tca6416a.h
        - ui_chip.h
        - ui_settings.h

    Include the following headers before including the *implementation*:
        - imgui.h
        - tca6416a.h
        - ui_chip.h
        - ui_util.h

    All strings provided to ui_tca6416a_init() must remain alive until
    ui_tca6416a_discard() is called!

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
#*/

#include "./ui_tca6416a.h"

#include "imgui.h"
#include "ui/ui_util.h"

#ifndef __cplusplus
    #error "implementation must be compiled as C++"
#endif
#include <string.h> /* memset */
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

void ui_tca6416a_init(ui_tca6416a_t* win, const ui_tca6416a_desc_t* desc) {
    CHIPS_ASSERT(win && desc);
    CHIPS_ASSERT(desc->title);
    CHIPS_ASSERT(desc->cia);
    memset(win, 0, sizeof(ui_tca6416a_t));
    win->title = desc->title;
    win->gpio = desc->gpio;
    win->init_x = (float)desc->x;
    win->init_y = (float)desc->y;
    win->init_w = (float)((desc->w == 0) ? 572 : desc->w);
    win->init_h = (float)((desc->h == 0) ? 336 : desc->h);
    win->open = win->last_open = desc->open;
    win->valid = true;
    ui_chip_init(&win->chip, &desc->chip_desc);
}

void ui_tca6416a_discard(ui_tca6416a_t* win) {
    CHIPS_ASSERT(win && win->valid);
    win->valid = false;
}

static void _ui_tca6416a_draw_state(ui_tca6416a_t* win) {
    const tca6416a_t* gpio = win->gpio;
    if (ImGui::BeginTable("##cia_ports", 3)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 142);
        ImGui::TableSetupColumn("Port 0", ImGuiTableColumnFlags_WidthFixed, 72);
        ImGui::TableSetupColumn("Port 1", ImGuiTableColumnFlags_WidthFixed, 72);
        ImGui::TableHeadersRow();
        ImGui::TableNextColumn();
        ImGui::Text("Input Port");
        ImGui::TableNextColumn();
        ui_util_b8("", gpio->p0.in);
        ImGui::TableNextColumn();
        ui_util_b8("", gpio->p1.in);
        ImGui::TableNextColumn();
        ImGui::Text("Output Port");
        ImGui::TableNextColumn();
        ui_util_b8("", gpio->p0.out);
        ImGui::TableNextColumn();
        ui_util_b8("", gpio->p1.out);
        ImGui::TableNextColumn();
        ImGui::Text("Polarity Inversion");
        ImGui::TableNextColumn();
        ui_util_b8("", gpio->p0.pol);
        ImGui::TableNextColumn();
        ui_util_b8("", gpio->p1.pol);
        ImGui::TableNextColumn();
        ImGui::Text("Configuration");
        ImGui::TableNextColumn();
        ui_util_b8("", gpio->p0.cfg);
        ImGui::TableNextColumn();
        ui_util_b8("", gpio->p1.cfg);
        ImGui::TableNextColumn();
        ImGui::Separator();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::Text("Interrupt");
        ImGui::TableNextColumn();
        ImGui::Text("%s", (gpio->intr & TCA6416A_INT_P0) ? "Active" : "-");
        ImGui::TableNextColumn();
        ImGui::Text("%s", (gpio->intr & TCA6416A_INT_P1) ? "Active" : "-");
        ImGui::TableNextColumn();
        ImGui::EndTable();
    }
}

void ui_tca6416a_draw(ui_tca6416a_t* win) {
    CHIPS_ASSERT(win && win->valid);
    ui_util_handle_window_open_dirty(&win->open, &win->last_open);
    if (!win->open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(win->init_x, win->init_y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(win->init_w, win->init_h), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(win->title, &win->open)) {
        ImGui::BeginChild("##tca6416a_chip", ImVec2(222, 0), true);
        ui_chip_draw(&win->chip, win->gpio->pins);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##tca6416a_state", ImVec2(0, 0), true);
        _ui_tca6416a_draw_state(win);
        ImGui::EndChild();
    }
    ImGui::End();
}

void ui_tca6416a_save_settings(ui_tca6416a_t* win, ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    ui_settings_add(settings, win->title, win->open);
}

void ui_tca6416a_load_settings(ui_tca6416a_t* win, const ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    win->open = ui_settings_isopen(settings, win->title);
}
