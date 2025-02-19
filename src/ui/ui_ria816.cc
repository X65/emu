#include "./ui_ria816.h"

#include "ui/ui_util.h"

#include <imgui.h>

#include <inttypes.h>

#ifndef __cplusplus
    #error "implementation must be compiled as C++"
#endif
#include <string.h> /* memset */
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

void ui_ria816_init(ui_ria816_t* win, const ui_ria816_desc_t* desc) {
    CHIPS_ASSERT(win && desc);
    CHIPS_ASSERT(desc->title);
    CHIPS_ASSERT(desc->ria);
    memset(win, 0, sizeof(ui_ria816_t));
    win->title = desc->title;
    win->ria = desc->ria;
    win->init_x = (float)desc->x;
    win->init_y = (float)desc->y;
    win->init_w = (float)((desc->w == 0) ? 420 : desc->w);
    win->init_h = (float)((desc->h == 0) ? 250 : desc->h);
    win->open = win->last_open = desc->open;
    win->valid = true;
    ui_chip_init(&win->chip, &desc->chip_desc);
}

void ui_ria816_discard(ui_ria816_t* win) {
    CHIPS_ASSERT(win && win->valid);
    win->valid = false;
}

static void _ui_ria816_draw_state(ui_ria816_t* win) {
    const ria816_t* ria = win->ria;
    ui_util_b8("EXTIO: ", ria->reg[RIA816_EXT_IO]);
    uint8_t ria_status = ria816_uart_status(ria);
    ui_util_b8("UART: ", ria_status);
    ImGui::SameLine();
    ImGui::Text("%s%s", ria_status & 0b10000000 ? "CTS " : "", ria_status & 0b01000000 ? "DRD " : "");
    if (ImGui::BeginTable("##cpu_vectors", 3)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 64);
        ImGui::TableSetupColumn("Emulated", ImGuiTableColumnFlags_WidthFixed, 72);
        ImGui::TableSetupColumn("Native", ImGuiTableColumnFlags_WidthFixed, 72);
        ImGui::TableHeadersRow();
        ImGui::TableNextColumn();
        ImGui::Text("IRQ");
        ImGui::TableNextColumn();
        ui_util_u16("", RIA816_REG16(ria->reg, RIA816_CPU_E_IRQB_BRK));
        ImGui::TableNextColumn();
        ui_util_u16("", RIA816_REG16(ria->reg, RIA816_CPU_N_IRQB));
        ImGui::TableNextColumn();
        ImGui::Text("RESET");
        ImGui::TableNextColumn();
        ui_util_u16("", RIA816_REG16(ria->reg, RIA816_CPU_E_RESETB));
        ImGui::TableNextColumn();
        ImGui::Text("    -");
        ImGui::TableNextColumn();
        ImGui::Text("NMI");
        ImGui::TableNextColumn();
        ui_util_u16("", RIA816_REG16(ria->reg, RIA816_CPU_E_NMIB));
        ImGui::TableNextColumn();
        ui_util_u16("", RIA816_REG16(ria->reg, RIA816_CPU_N_NMIB));
        ImGui::TableNextColumn();
        ImGui::Text("ABORT");
        ImGui::TableNextColumn();
        ui_util_u16("", RIA816_REG16(ria->reg, RIA816_CPU_E_ABORTB));
        ImGui::TableNextColumn();
        ui_util_u16("", RIA816_REG16(ria->reg, RIA816_CPU_N_ABORTB));
        ImGui::TableNextColumn();
        ImGui::Text("BRK");
        ImGui::TableNextColumn();
        ui_util_u16("", RIA816_REG16(ria->reg, RIA816_CPU_E_IRQB_BRK));
        ImGui::TableNextColumn();
        ui_util_u16("", RIA816_REG16(ria->reg, RIA816_CPU_N_BRK));
        ImGui::TableNextColumn();
        ImGui::Text("COP");
        ImGui::TableNextColumn();
        ui_util_u16("", RIA816_REG16(ria->reg, RIA816_CPU_E_COP));
        ImGui::TableNextColumn();
        ui_util_u16("", RIA816_REG16(ria->reg, RIA816_CPU_N_COP));
        ImGui::TableNextColumn();
        ImGui::EndTable();
        ImGui::Text("Time: %016" PRIX64, ria->us);
    }
}

void ui_ria816_draw(ui_ria816_t* win) {
    CHIPS_ASSERT(win && win->valid);
    ui_util_handle_window_open_dirty(&win->open, &win->last_open);
    if (!win->open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(win->init_x, win->init_y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(win->init_w, win->init_h), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(win->title, &win->open)) {
        ImGui::BeginChild("##ria816_chip", ImVec2(176, 0), true);
        ui_chip_draw(&win->chip, win->ria->pins);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##ria816_state", ImVec2(0, 0), true);
        _ui_ria816_draw_state(win);
        ImGui::EndChild();
    }
    ImGui::End();
}

void ui_ria816_save_settings(ui_ria816_t* win, ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    ui_settings_add(settings, win->title, win->open);
}

void ui_ria816_load_settings(ui_ria816_t* win, const ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    win->open = ui_settings_isopen(settings, win->title);
}
