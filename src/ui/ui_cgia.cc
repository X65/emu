#include "./ui_cgia.h"

#include "ui/ui_util.h"

#include "imgui.h"

#ifndef __cplusplus
    #error "implementation must be compiled as C++"
#endif
#include <string.h> /* memset */
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

#define cgia_t fwcgia_t
#include "firmware/src/ria/cgia/cgia.h"
#undef cgia_t

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
    if (ImGui::CollapsingHeader("Hardware Colors")) {
        ImVec4 c;
        const ImVec2 size(18, 18);
        for (int i = 0; i < CGIA_COLORS_NUM; i++) {
            c = ImColor(win->cgia->hwcolors[i]);
            ImGui::PushID(i);
            char desc_id[64];
            sprintf(desc_id, "%d 0x%02X##hw_color", i, i);
            ImGui::ColorButton(desc_id, c, ImGuiColorEditFlags_NoAlpha, size);
            ImGui::PopID();
            if (((i + 1) % 8) != 0) {
                ImGui::SameLine();
            }
        }
    }
}

static void _ui_cgia_draw_color(const ui_cgia_t* win, const char* label, uint8_t val) {
    ImGui::Text("%s%X", label, val);
    ImGui::SameLine();
    char desc_id[64];
    sprintf(desc_id, "%d 0x%02X##regclr", val, val);
    ImGui::ColorButton(desc_id, ImColor(win->cgia->hwcolors[val]), ImGuiColorEditFlags_NoAlpha, ImVec2(12, 12));
}

static void _ui_cgia_draw_rgb(const char* label, uint32_t val) {
    ImGui::Text("%s", label);
    ImGui::SameLine();
    ImGui::ColorButton("##rgbclr", ImColor(val | 0xFF000000), ImGuiColorEditFlags_NoAlpha, ImVec2(12, 12));
}

static void _ui_cgia_draw_registers(const ui_cgia_t* win) {
    if (ImGui::CollapsingHeader("Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
        const fwcgia_t* chip = (fwcgia_t*)&win->cgia->reg;
        ui_util_b8("planes: ", chip->planes);

        ImGui::Text("bckgnd_bank: %02X", chip->bckgnd_bank);
        ImGui::SameLine();
        ImGui::Text("sprite_bank: %02X", chip->sprite_bank);

        _ui_cgia_draw_color(win, "back_color: ", chip->back_color);
    }
}

static void _ui_cgia_draw_raster_unit(const ui_cgia_t* win) {
    if (ImGui::CollapsingHeader("Raster Unit", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("H Period:     %4d", win->cgia->h_period);
        ImGui::Text("H Counter:    %4d", win->cgia->h_count);
        ImGui::Text("Line Counter: %4d", win->cgia->l_count);
        ImGui::Text("Active Line:  %4d", win->cgia->active_line);
    }
}

static void _ui_cgia_draw_planes(const ui_cgia_t* win) {
    const fwcgia_t* chip = (fwcgia_t*)&win->cgia->reg;
    for (int i = 0; i < CGIA_PLANES; i++) {
        const bool plane_active = chip->planes & (1u << i);
        const bool plane_type_sprite = chip->planes & (0x10u << i);
        char label[64];
        sprintf(label, "Plane %d (%s)", i, plane_type_sprite ? "sprites" : "background");
        if (ImGui::CollapsingHeader(label, plane_active ? 0 : ImGuiTreeNodeFlags_Bullet)) {
            ImGui::Text("Plane active: %s", plane_active ? "ON " : "OFF");
            if (plane_type_sprite) {
                ImGui::Text(
                    "offset:%04X (mem:%06X)",
                    chip->plane[i].offset,
                    (chip->sprite_bank << 16) | chip->plane[i].offset);

                ui_util_b8("sprites active: ", chip->plane[i].regs.sprite.active);
                ImGui::Text("border: %d columns", chip->plane[i].regs.sprite.border_columns);
                ImGui::Text("start_y: %d", chip->plane[i].regs.sprite.start_y);
                ImGui::SameLine();
                ImGui::Text("stop_y: %d", chip->plane[i].regs.sprite.stop_y);
            }
            else {
                ImGui::Text(
                    "offset:%04X (mem:%06X)",
                    chip->plane[i].offset,
                    (chip->bckgnd_bank << 16) | chip->plane[i].offset);

                ui_util_b8("flags: ", chip->plane[i].regs.bckgnd.flags);
                ImGui::Text("border: %d columns", chip->plane[i].regs.bckgnd.border_columns);
                ImGui::Text("row_height: %d", chip->plane[i].regs.bckgnd.row_height);
                ImGui::Separator();
                ImGui::Text("stride: %d", chip->plane[i].regs.bckgnd.stride);
                _ui_cgia_draw_color(win, "shared_colors: ", chip->plane[i].regs.bckgnd.shared_color[0]);
                ImGui::SameLine();
                _ui_cgia_draw_color(win, "", chip->plane[i].regs.bckgnd.shared_color[1]);
                ImGui::Text("scroll_x: %d", chip->plane[i].regs.bckgnd.scroll_x);
                ImGui::Text("offset_x: %d", chip->plane[i].regs.bckgnd.offset_x);
                ImGui::Text("scroll_y: %d", chip->plane[i].regs.bckgnd.scroll_y);
                ImGui::Text("offset_y: %d", chip->plane[i].regs.bckgnd.offset_y);
                ImGui::Separator();
                // ui_util_b8("flags: ", chip->plane[i].regs.affine.flags);
                // ImGui::Text("border: %d columns", chip->plane[i].regs.affine.border_columns);
                // ImGui::Text("row_height: %d", chip->plane[i].regs.affine.row_height);
                ui_util_b8("texture_bits: ", chip->plane[i].regs.affine.texture_bits);
                ImGui::Text("width: %d", 2 ^ (chip->plane[i].regs.affine.texture_bits & 0x0F));
                ImGui::SameLine();
                ImGui::Text("height: %d", 2 ^ (chip->plane[i].regs.affine.texture_bits >> 4));
                ImGui::Text("u: %d", chip->plane[i].regs.affine.u);
                ImGui::SameLine();
                ImGui::Text("v: %d", chip->plane[i].regs.affine.v);
                ImGui::Text("du: %d", chip->plane[i].regs.affine.du);
                ImGui::SameLine();
                ImGui::Text("dv: %d", chip->plane[i].regs.affine.dv);
                ImGui::Text("dx: %d", chip->plane[i].regs.affine.dx);
                ImGui::SameLine();
                ImGui::Text("dy: %d", chip->plane[i].regs.affine.dy);
            }
        }
    }
}

void ui_cgia_draw(ui_cgia_t* win) {
    CHIPS_ASSERT(win && win->valid);
    if (!win->open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(win->init_x, win->init_y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(win->init_w, win->init_h), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(win->title, &win->open)) {
        ImGui::BeginChild("##cgia_chip", ImVec2(176, 0), true);
        ui_chip_draw(&win->chip, win->cgia->pins);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##cgia_state", ImVec2(0, 0), true);
        _ui_cgia_draw_hwcolors(win);
        _ui_cgia_draw_registers(win);
        _ui_cgia_draw_raster_unit(win);
        _ui_cgia_draw_planes(win);
        ImGui::EndChild();
    }
    ImGui::End();
}
