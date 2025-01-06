#include "./ui_cgia.h"

#include "imgui.h"

#ifndef __cplusplus
    #error "implementation must be compiled as C++"
#endif
#include <string.h> /* memset */
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

void ui_cgia_init(ui_cgia_t* win, const ui_cgia_desc_t* desc) {
    CHIPS_ASSERT(win && desc);
    CHIPS_ASSERT(desc->title);
    CHIPS_ASSERT(desc->cgia);
    memset(win, 0, sizeof(ui_cgia_t));
    win->title = desc->title;
    win->cgia = desc->cgia;
    win->init_x = (float)desc->x;
    win->init_y = (float)desc->y;
    win->init_w = (float)((desc->w == 0) ? 348 : desc->w);
    win->init_h = (float)((desc->h == 0) ? 360 : desc->h);
    win->open = desc->open;
    win->valid = true;
    ui_chip_init(&win->chip, &desc->chip_desc);
}

void ui_cgia_discard(ui_cgia_t* win) {
    CHIPS_ASSERT(win && win->valid);
    win->valid = false;
}

static void _ui_cgia_draw_hwcolors(ui_cgia_t* win) {
    ImGui::Text("Gfx Mode Colors:");
    ImVec4 c;
    const ImVec2 size(18, 18);
    for (int i = 0; i < 8; i++) {
        c = ImColor(win->cgia->hwcolors[i]);
        ImGui::PushID(i);
        ImGui::ColorButton("##gm_color", c, ImGuiColorEditFlags_NoAlpha, size);
        ImGui::PopID();
        if (((i + 1) % 4) != 0) {
            ImGui::SameLine();
        }
    }
    ImGui::Separator();
    ImGui::Text("Text Mode Colors:");
    ImVec4 tm_green = ImColor(win->cgia->hwcolors[CGIA_HWCOLOR_ALNUM_GREEN]);
    ImVec4 tm_dark_green = ImColor(win->cgia->hwcolors[CGIA_HWCOLOR_ALNUM_DARK_GREEN]);
    ImVec4 tm_orange = ImColor(win->cgia->hwcolors[CGIA_HWCOLOR_ALNUM_ORANGE]);
    ImVec4 tm_dark_orange = ImColor(win->cgia->hwcolors[CGIA_HWCOLOR_ALNUM_DARK_ORANGE]);
    ImGui::ColorButton("##tm_green", tm_green, ImGuiColorEditFlags_NoAlpha, size);
    ImGui::SameLine();
    ImGui::ColorButton("##tm_dark_green", tm_dark_green, ImGuiColorEditFlags_NoAlpha, size);
    ImGui::SameLine();
    ImGui::ColorButton("##tm_orange", tm_orange, ImGuiColorEditFlags_NoAlpha, size);
    ImGui::SameLine();
    ImGui::ColorButton("##tm_dark_orange", tm_dark_orange, ImGuiColorEditFlags_NoAlpha, size);
}

static void _ui_cgia_draw_values(ui_cgia_t* win) {
    ImGui::Text("H Period:     %4d", win->cgia->h_period);
    ImGui::Text("H Sync Start: %4d", win->cgia->h_sync_start);
    ImGui::Text("H Sync End:   %4d", win->cgia->h_sync_end);
    ImGui::Text("H Counter:    %4d", win->cgia->h_count);
    ImGui::Text("Line Counter: %4d", win->cgia->l_count);
}

void ui_cgia_draw(ui_cgia_t* win) {
    CHIPS_ASSERT(win && win->valid);
    if (!win->open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(win->init_x, win->init_y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(win->init_w, win->init_h), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(win->title, &win->open)) {
        ImGui::BeginChild("##chip", ImVec2(176, 0), true);
        ui_chip_draw(&win->chip, win->cgia->pins);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##state", ImVec2(0, 0), true);
        _ui_cgia_draw_hwcolors(win);
        ImGui::Separator();
        _ui_cgia_draw_values(win);
        ImGui::EndChild();
    }
    ImGui::End();
}
