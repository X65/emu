#include "./ui_cgia.h"

#include "ui/ui_util.h"

#include "imgui.h"
#include <cmath>

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
    ImGui::Text("%s%02X", label, val);
    ImGui::SameLine();
    char desc_id[64];
    sprintf(desc_id, "%3d 0x%02X##regclr", val, val);
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

static void _ui_cgia_decode_DL(const cgia_t* cgia, uint16_t offset) {
    uint8_t* ptr = cgia->vram[0] + offset;
    uint8_t dl_instr = *ptr;
    switch (dl_instr & 0x0F) {
        case 0x0:  // INSTR0 - blank lines
            ImGui::Text("BLNK : %3d", dl_instr >> 4);
            break;
        case 0x1:  // duplicate lines - copy last raster line n-times
            ImGui::Text("DUPL : %3d", dl_instr >> 4);
            break;
        case 0x2:  // INSTR1 - JMP
            ImGui::Text("JMP  : %04x", (uint16_t)*(ptr + 1) | ((uint16_t)*(ptr + 2) << 8));
            break;
        case 0x3:  // Load Memory
            ImGui::Text(
                "LOAD : %s%s%s%s",
                dl_instr & 0b00010000 ? "LMS " : "",
                dl_instr & 0b00100000 ? "LCS " : "",
                dl_instr & 0b01000000 ? "LBS " : "",
                dl_instr & 0b10000000 ? "LCG " : "");
            break;
        case 0x4:  // Set 8-bit register
            ImGui::Text("REG8 : %02x = %02x (%3d)", (dl_instr & 0b01110000), *(ptr + 1), *(ptr + 1));
            break;
        case 0x5:  // Set 16-bit register
            ImGui::Text(
                "REG16: %02x = %04x (%5d)",
                (dl_instr & 0b01110000),
                (uint16_t)*(ptr + 1) | ((uint16_t)*(ptr + 2) << 8),
                (uint16_t)*(ptr + 1) | ((uint16_t)*(ptr + 2) << 8));
            break;
        case 0x6:  // N/A
        case 0x7:  // N/A
            ImGui::Text("???%02x: %02x: ", dl_instr & 0x0F, dl_instr);
            break;
        case (0x0 | CGIA_MODE_BIT):  // MODE0 (8)
            ImGui::Text("MODE0: ???");
            break;
        case (0x1 | CGIA_MODE_BIT):  // MODE1 (9)
            ImGui::Text("MODE1: ???");
            break;
        case (0x2 | CGIA_MODE_BIT):  // MODE2 (A) - text/tile mode
            ImGui::Text("MODE2: text/tile");
            break;
        case (0x3 | CGIA_MODE_BIT):  // MODE3 (B) - bitmap mode
            ImGui::Text("MODE3: bitmap");
            break;
        case (0x4 | CGIA_MODE_BIT):  // MODE4 (C) - multicolor text/tile mode
            ImGui::Text("MODE4: multicolor text/tile");
            break;
        case (0x5 | CGIA_MODE_BIT):  // MODE5 (D) - multicolor bitmap mode
            ImGui::Text("MODE5: multicolor bitmap");
            break;
        case (0x6 | CGIA_MODE_BIT):  // MODE6 (E) - Hold-and-Modify (HAM) mode
            ImGui::Text("MODE6: HAM");
            break;
        case (0x7 | CGIA_MODE_BIT):  // MODE7 (F) - affine transform mode
            ImGui::Text("MODE7: affine transform");
            break;
    }
}

static void _ui_cgia_decode_BG_flags(uint8_t flags) {
    if (flags & 0b00000001) ImGui::Text("  TRANSPARENT");
    if (flags & 0b00001000) ImGui::Text("  BORDER_TRANSPARENT");
    if (flags & 0b00010000) ImGui::Text("  DOUBLE_WIDTH");
}

static void _ui_cgia_draw_planes(const ui_cgia_t* win) {
    fwcgia_t* chip = (fwcgia_t*)&win->cgia->reg;
    for (int i = 0; i < CGIA_PLANES; i++) {
        ImGui::PushID(i);
        bool plane_active = chip->planes & (1u << i);
        const bool plane_type_sprite = chip->planes & (0x10u << i);
        char label[64];
        sprintf(label, "Plane %d (%s)", i, plane_type_sprite ? "sprites" : "background");
        if (ImGui::CollapsingHeader(label, plane_active ? 0 : ImGuiTreeNodeFlags_Bullet)) {
            ImGui::Checkbox(plane_active ? "Plane active" : "Plane inactive", &plane_active);
            if (plane_active) {
                chip->planes |= (1u << i);
            }
            else {
                chip->planes &= ~(1u << i);
            }
            if (win->cgia->internal[i].wait_vbl) {
                ImGui::SameLine();
                ImGui::Text(" Wait VBL");
            }
            if (plane_type_sprite) {
                if (win->cgia->internal[i].sprites_need_update) {
                    ImGui::SameLine();
                    ImGui::Text(" Need update");
                }
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
                    "MS:%04x CS:%04x BS:%04x CG:%04x",
                    win->cgia->internal[i].memory_scan,
                    win->cgia->internal[i].colour_scan,
                    win->cgia->internal[i].backgr_scan,
                    win->cgia->internal[i].chargen_offset);
                ImGui::Text(
                    "offset:%04X (mem:%06X)",
                    chip->plane[i].offset,
                    (chip->bckgnd_bank << 16) | chip->plane[i].offset);

                ImGui::SameLine();
                _ui_cgia_decode_DL(win->cgia, chip->plane[i].offset);
                ui_util_b8("flags: ", chip->plane[i].regs.bckgnd.flags);
                _ui_cgia_decode_BG_flags(chip->plane[i].regs.bckgnd.flags);
                ImGui::Text("border: %d columns", chip->plane[i].regs.bckgnd.border_columns);
                ImGui::Text("row_height: %d", chip->plane[i].regs.bckgnd.row_height + 1);
                ImGui::Text("row_line  : %d", win->cgia->internal[i].row_line_count);
                ImGui::Separator();
                ImGui::Text("stride: %d", chip->plane[i].regs.bckgnd.stride);
                ImGui::Text("colors:");
                for (int c = 0; c < 2; ++c) {
                    ImGui::SameLine();
                    ImGui::PushID(c);
                    _ui_cgia_draw_color(win, "", chip->plane[i].regs.bckgnd.shared_color[c]);
                    ImGui::PopID();
                }
                ImGui::SameLine();
                ImGui::Text("|");
                for (int c = 2; c < 8; ++c) {
                    ImGui::SameLine();
                    ImGui::PushID(c);
                    _ui_cgia_draw_color(win, "", chip->plane[i].regs.ham.base_color[c]);
                    ImGui::PopID();
                }
                ImGui::Text("scroll_x: %d", chip->plane[i].regs.bckgnd.scroll_x);
                ImGui::Text("offset_x: %d", chip->plane[i].regs.bckgnd.offset_x);
                ImGui::Text("scroll_y: %d", chip->plane[i].regs.bckgnd.scroll_y);
                ImGui::Text("offset_y: %d", chip->plane[i].regs.bckgnd.offset_y);
                ImGui::Separator();
                // ui_util_b8("flags: ", chip->plane[i].regs.affine.flags);
                // ImGui::Text("border: %d columns", chip->plane[i].regs.affine.border_columns);
                // ImGui::Text("row_height: %d", chip->plane[i].regs.affine.row_height);
                ui_util_b8("texture_bits: ", chip->plane[i].regs.affine.texture_bits);
                ImGui::SameLine();
                ImGui::Text("w: %d", (int)pow(2, (chip->plane[i].regs.affine.texture_bits & 0x0F)));
                ImGui::SameLine();
                ImGui::Text("h: %d", (int)pow(2, (chip->plane[i].regs.affine.texture_bits >> 4)));
                ImGui::Text(" u: %d", chip->plane[i].regs.affine.u);
                ImGui::SameLine();
                ImGui::Text(" v: %d", chip->plane[i].regs.affine.v);
                ImGui::Text("du: %d", chip->plane[i].regs.affine.du);
                ImGui::SameLine();
                ImGui::Text("dv: %d", chip->plane[i].regs.affine.dv);
                ImGui::Text("dx: %d", chip->plane[i].regs.affine.dx);
                ImGui::SameLine();
                ImGui::Text("dy: %d", chip->plane[i].regs.affine.dy);
            }
        }
        ImGui::PopID();
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
