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
    win->init_w = (float)((desc->w == 0) ? 630 : desc->w);
    win->init_h = (float)((desc->h == 0) ? 360 : desc->h);
    win->open = win->last_open = desc->open;
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
            sprintf(desc_id, "%03d 0x%02X##hw_color", i, i);
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
        const fwcgia_t* chip = (fwcgia_t*)win->cgia->chip;
        ui_util_b8("mode  : ", chip->mode);
        ui_util_b8("planes: ", chip->planes);

        ImGui::Text(
            "bckgnd_bank: %02X (VRAM%d: %06X/%06X)",
            chip->bckgnd_bank,
            win->cgia->vram_cache[0].cache_ptr_idx,
            win->cgia->vram_cache[0].bank_mask,
            win->cgia->vram_cache[0].wanted_bank_mask);
        ImGui::Text(
            "sprite_bank: %02X (VRAM%d: %06X/%06X)",
            chip->sprite_bank,
            win->cgia->vram_cache[1].cache_ptr_idx,
            win->cgia->vram_cache[1].bank_mask,
            win->cgia->vram_cache[1].wanted_bank_mask);

        _ui_cgia_draw_color(win, "back_color: ", chip->back_color);

        ui_util_u16("INT Raster:", chip->int_raster);
        ui_util_b8("INT Enable: ", chip->int_enable);
        ImGui::SameLine();
        ImGui::Text(
            "%s%s%s",
            chip->int_enable & CGIA_REG_INT_FLAG_VBI ? "VBI " : "",
            chip->int_enable & CGIA_REG_INT_FLAG_DLI ? "DLI " : "",
            chip->int_enable & CGIA_REG_INT_FLAG_RSI ? "RSI " : "");
        ui_util_b8("INT Status: ", chip->int_status);
        ImGui::SameLine();
        ImGui::Text(
            "%s%s%s",
            chip->int_status & CGIA_REG_INT_FLAG_VBI ? "VBI " : "",
            chip->int_status & CGIA_REG_INT_FLAG_DLI ? "DLI " : "",
            chip->int_status & CGIA_REG_INT_FLAG_RSI ? "RSI " : "");
        ui_util_b8("INT Mask  : ", win->cgia->int_mask);
    }
}

static void _ui_cgia_draw_raster_unit(const ui_cgia_t* win) {
    if (ImGui::CollapsingHeader("Raster Unit", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("H Period:    %4d", win->cgia->h_period / CGIA_FIXEDPOINT_SCALE);
        ImGui::Text("H Counter:   %4d", win->cgia->h_count / CGIA_FIXEDPOINT_SCALE);
        ImGui::Text("V Counter:   %4d", win->cgia->v_count);
        ImGui::Text("V Period:    %4d", MODE_V_TOTAL_LINES - 1);
        ImGui::Text("Scan Line:   %4d", win->cgia->scan_line);
        const fwcgia_t* chip = (fwcgia_t*)win->cgia->chip;
        ImGui::Text("Raster Line: %4d", chip->raster);
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
        case (0x0 | CGIA_DL_MODE_BIT):  // MODE0 (8)
            ImGui::Text("MODE0: ???");
            break;
        case (0x1 | CGIA_DL_MODE_BIT):  // MODE1 (9)
            ImGui::Text("MODE1: ???");
            break;
        case (0x2 | CGIA_DL_MODE_BIT):  // MODE2 (A) - text/tile mode
            ImGui::Text("MODE2: text/tile");
            break;
        case (0x3 | CGIA_DL_MODE_BIT):  // MODE3 (B) - bitmap mode
            ImGui::Text("MODE3: bitmap");
            break;
        case (0x4 | CGIA_DL_MODE_BIT):  // MODE4 (C) - multicolor text/tile mode
            ImGui::Text("MODE4: multicolor text/tile");
            break;
        case (0x5 | CGIA_DL_MODE_BIT):  // MODE5 (D) - multicolor bitmap mode
            ImGui::Text("MODE5: multicolor bitmap");
            break;
        case (0x6 | CGIA_DL_MODE_BIT):  // MODE6 (E) - Hold-and-Modify (HAM) mode
            ImGui::Text("MODE6: HAM");
            break;
        case (0x7 | CGIA_DL_MODE_BIT):  // MODE7 (F) - affine transform mode
            ImGui::Text("MODE7: affine transform");
            break;
    }
}

static void _ui_cgia_decode_BG_flags(uint8_t flags) {
    if (flags & 0b00000001) ImGui::Text("  TRANSPARENT");
    if (flags & 0b00001000) ImGui::Text("  BORDER_TRANSPARENT");
    if (flags & 0b00010000) ImGui::Text("  DOUBLE_WIDTH");
}

static void _ui_cgia_draw_bg_plane(const ui_cgia_t* win, size_t p) {
    fwcgia_t* chip = (fwcgia_t*)win->cgia->chip;
    ImGui::Text(
        "MS:%04x CS:%04x BS:%04x CG:%04x",
        win->cgia->internal[p].memory_scan,
        win->cgia->internal[p].colour_scan,
        win->cgia->internal[p].backgr_scan,
        win->cgia->internal[p].chargen_offset);
    ImGui::Text("offset:%04X (mem:%06X)", chip->offset[p], (chip->bckgnd_bank << 16) | chip->offset[p]);

    ImGui::SameLine();
    _ui_cgia_decode_DL(win->cgia, chip->offset[p]);
    ui_util_b8("flags: ", chip->plane[p].bckgnd.flags);
    _ui_cgia_decode_BG_flags(chip->plane[p].bckgnd.flags);
    ImGui::Text("border: %03d columns", chip->plane[p].bckgnd.border_columns);
    ImGui::Text("row_height: %03d", chip->plane[p].bckgnd.row_height + 1);
    ImGui::Text("row_line  : %03d", win->cgia->internal[p].row_line_count);
    ImGui::Separator();
    ImGui::Text("stride: %03d columns", chip->plane[p].bckgnd.stride);
    ImGui::Text("colors:");
    for (int c = 0; c < 2; ++c) {
        ImGui::SameLine();
        ImGui::PushID(c);
        _ui_cgia_draw_color(win, "", chip->plane[p].bckgnd.color[c]);
        ImGui::PopID();
    }
    ImGui::SameLine();
    ImGui::Text("|");
    for (int c = 2; c < 8; ++c) {
        ImGui::SameLine();
        ImGui::PushID(c);
        _ui_cgia_draw_color(win, "", chip->plane[p].ham.color[c]);
        ImGui::PopID();
    }
    ImGui::Text("scroll_x: %03d", chip->plane[p].bckgnd.scroll_x);
    ImGui::Text("offset_x: %03d", chip->plane[p].bckgnd.offset_x);
    ImGui::Text("scroll_y: %03d", chip->plane[p].bckgnd.scroll_y);
    ImGui::Text("offset_y: %03d", chip->plane[p].bckgnd.offset_y);
    ImGui::Separator();
    // ui_util_b8("flags: ", chip->plane[i].affine.flags);
    // ImGui::Text("border: %03d columns", chip->plane[i].affine.border_columns);
    // ImGui::Text("row_height: %03d", chip->plane[i].affine.row_height);
    ui_util_b8("texture_bits: ", chip->plane[p].affine.texture_bits);
    ImGui::SameLine();
    ImGui::Text("w: %03d", (int)pow(2, (chip->plane[p].affine.texture_bits & 0x0F)));
    ImGui::SameLine();
    ImGui::Text("h: %03d", (int)pow(2, (chip->plane[p].affine.texture_bits >> 4)));
    ImGui::Text(" u: %04x/%.5f", (uint16_t)chip->plane[p].affine.u, chip->plane[p].affine.u / 256.0);
    ImGui::SameLine();
    ImGui::Text(" v: %04x/%.5f", (uint16_t)chip->plane[p].affine.v, chip->plane[p].affine.v / 256.0);
    ImGui::Text("du: %04x/%.5f", (uint16_t)chip->plane[p].affine.du, chip->plane[p].affine.du / 256.0);
    ImGui::SameLine();
    ImGui::Text("dv: %04x/%.5f", (uint16_t)chip->plane[p].affine.dv, chip->plane[p].affine.dv / 256.0);
    ImGui::Text("dx: %04x/%.5f", (uint16_t)chip->plane[p].affine.dx, chip->plane[p].affine.dx / 256.0);
    ImGui::SameLine();
    ImGui::Text("dy: %04x/%.5f", (uint16_t)chip->plane[p].affine.dy, chip->plane[p].affine.dy / 256.0);
}

static void _ui_cgia_draw_sprite_plane(const ui_cgia_t* win, size_t p) {
    fwcgia_t* chip = (fwcgia_t*)win->cgia->chip;
    if (win->cgia->internal[p].sprites_need_update) {
        ImGui::SameLine();
        ImGui::Text(" Need update");
    }
    ImGui::Text("offset:%04X (mem:%06X)", chip->offset[p], (chip->sprite_bank << 16) | chip->offset[p]);

    ui_util_b8("sprites active: ", chip->plane[p].sprite.active);
    ImGui::Text("border: %03d columns", chip->plane[p].sprite.border_columns);
    ImGui::Text("start_y: %03d", chip->plane[p].sprite.start_y);
    ImGui::SameLine();
    ImGui::Text("stop_y : %03d", chip->plane[p].sprite.stop_y);
    ImGui::Separator();
    const float cw0 = 10.0f;
    const float cw = 42.0f;
    uint8_t* vram = win->cgia->vram[win->cgia->vram_cache[1].cache_ptr_idx];
    if (ImGui::BeginTable("##sprite_descriptors", 11)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, cw0);
        ImGui::TableSetupColumn("Offs", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("H", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 56.0f);
        ImGui::TableSetupColumn("Cl0", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Cl1", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Cl2", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Next", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableHeadersRow();
        for (int i = 0; i < CGIA_SPRITES; i++) {
            uint16_t sprite_offset = win->cgia->internal[p].sprite_dsc_offsets[i];
            struct cgia_sprite_t* sprite = (struct cgia_sprite_t*)(vram + sprite_offset);
            ImGui::TableNextColumn();
            ImGui::Text("%d", i);
            ImGui::TableNextColumn();  // Offset
            ImGui::Text("%04X", sprite_offset);
            ImGui::TableNextColumn();  // X
            ImGui::Text("%6d", sprite->pos_x);
            ImGui::TableNextColumn();  // Y
            ImGui::Text("%6d", sprite->pos_y);
            ImGui::TableNextColumn();  // H
            ImGui::Text("%6d", sprite->lines_y);
            ImGui::TableNextColumn();  // Fl
            ui_util_b8("", sprite->flags);
            for (int c = 0; c < 3; c++) {
                ImGui::TableNextColumn();  // Cl
                ImGui::PushID(i * 10 + c);
                _ui_cgia_draw_color(win, "", sprite->color[c]);
                ImGui::PopID();
            }
            ImGui::TableNextColumn();  // Data
            ImGui::Text("%04X", sprite->data_offset);
            ImGui::TableNextColumn();  // Next
            ImGui::Text("%04X", sprite->next_dsc_offset);
        }
        ImGui::EndTable();
    }
}

static void _ui_cgia_draw_planes(const ui_cgia_t* win) {
    fwcgia_t* chip = (fwcgia_t*)win->cgia->chip;
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
                _ui_cgia_draw_sprite_plane(win, i);
            }
            else {
                _ui_cgia_draw_bg_plane(win, i);
            }
        }
        ImGui::PopID();
    }
}

void ui_cgia_draw(ui_cgia_t* win) {
    CHIPS_ASSERT(win && win->valid);
    ui_util_handle_window_open_dirty(&win->open, &win->last_open);
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

void ui_cgia_save_settings(ui_cgia_t* win, ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    ui_settings_add(settings, win->title, win->open);
}

void ui_cgia_load_settings(ui_cgia_t* win, const ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    win->open = ui_settings_isopen(settings, win->title);
}
