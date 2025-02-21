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
    win->init_w = (float)((desc->w == 0) ? 470 : desc->w);
    win->init_h = (float)((desc->h == 0) ? 616 : desc->h);
    win->open = win->last_open = desc->open;
    win->valid = true;
    ui_chip_init(&win->chip, &desc->chip_desc);
}

void ui_ria816_discard(ui_ria816_t* win) {
    CHIPS_ASSERT(win && win->valid);
    win->valid = false;
}

static void _ui_ria816_m6526_draw_state(ui_ria816_t* win) {
    const m6526_t* cia = &win->ria->cia;
    if (ImGui::BeginTable("##cia_timers", 3)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 72);
        ImGui::TableSetupColumn("Timer A", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Timer B", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();
        ImGui::TableNextColumn();
        ImGui::Text("Latch");
        ImGui::TableNextColumn();
        ImGui::Text("%04X", cia->ta.latch);
        ImGui::TableNextColumn();
        ImGui::Text("%04X", cia->tb.latch);
        ImGui::TableNextColumn();
        ImGui::Text("Counter");
        ImGui::TableNextColumn();
        ImGui::Text("%04X", cia->ta.counter);
        ImGui::TableNextColumn();
        ImGui::Text("%04X", cia->tb.counter);
        ImGui::TableNextColumn();
        ImGui::Text("Control");
        ImGui::TableNextColumn();
        ui_util_b8("", cia->ta.cr);
        ImGui::TableNextColumn();
        ui_util_b8("", cia->tb.cr);
        ImGui::TableNextColumn();
        ImGui::Text("  START");
        ImGui::TableNextColumn();
        ImGui::Text("%s", M6526_TIMER_STARTED(cia->ta.cr) ? "STARTED" : "STOP");
        ImGui::TableNextColumn();
        ImGui::Text("%s", M6526_TIMER_STARTED(cia->tb.cr) ? "STARTED" : "STOP");
        ImGui::TableNextColumn();
        ImGui::Text("  PBON");
        ImGui::TableNextColumn();
        ImGui::Text("%s", M6526_PBON(cia->ta.cr) ? "PB6" : "---");
        ImGui::TableNextColumn();
        ImGui::Text("%s", M6526_PBON(cia->tb.cr) ? "PB7" : "---");
        ImGui::TableNextColumn();
        ImGui::Text("  OUTMODE");
        ImGui::TableNextColumn();
        ImGui::Text("%s", M6526_OUTMODE_TOGGLE(cia->ta.cr) ? "TOGGLE" : "PULSE");
        ImGui::TableNextColumn();
        ImGui::Text("%s", M6526_OUTMODE_TOGGLE(cia->tb.cr) ? "TOGGLE" : "PULSE");
        ImGui::TableNextColumn();
        ImGui::Text("  RUNMODE");
        ImGui::TableNextColumn();
        ImGui::Text("%s", M6526_RUNMODE_ONESHOT(cia->ta.cr) ? "ONESHOT" : "CONT");
        ImGui::TableNextColumn();
        ImGui::Text("%s", M6526_RUNMODE_ONESHOT(cia->tb.cr) ? "ONESHOT" : "CONT");
        ImGui::TableNextColumn();
        ImGui::Text("  INMODE");
        ImGui::TableNextColumn();
        ImGui::Text("%s", M6526_TA_INMODE_PHI2(cia->ta.cr) ? "PHI2" : "CNT");
        ImGui::TableNextColumn();
        if (M6526_TB_INMODE_PHI2(cia->tb.cr)) {
            ImGui::Text("PHI2");
        }
        else if (M6526_TB_INMODE_CNT(cia->tb.cr)) {
            ImGui::Text("CNT");
        }
        else if (M6526_TB_INMODE_TA(cia->tb.cr)) {
            ImGui::Text("TA");
        }
        else if (M6526_TB_INMODE_TACNT(cia->tb.cr)) {
            ImGui::Text("TACNT");
        }
        ImGui::TableNextColumn();
        ImGui::Text("  SPMODE");
        ImGui::TableNextColumn();
        ImGui::Text("%s", M6526_TA_SPMODE_OUTPUT(cia->ta.cr) ? "OUTPUT" : "INPUT");
        ImGui::TableNextColumn();
        ImGui::Text("---");
        ImGui::TableNextColumn();
        ImGui::Text("  TODIN");
        ImGui::TableNextColumn();
        ImGui::Text("%s", M6526_TA_TODIN_50HZ(cia->ta.cr) ? "50HZ" : "60HZ");
        ImGui::TableNextColumn();
        ImGui::Text("---");
        ImGui::TableNextColumn();
        ImGui::Text("  ALARM");
        ImGui::TableNextColumn();
        ImGui::Text("%s", M6526_TB_ALARM_ALARM(cia->ta.cr) ? "ALARM" : "CLOCK");
        ImGui::TableNextColumn();
        ImGui::Text("---");
        ImGui::TableNextColumn();
        ImGui::Text("Bit");
        ImGui::TableNextColumn();
        ImGui::Text("%s", cia->ta.t_bit ? "ON" : "OFF");
        ImGui::TableNextColumn();
        ImGui::Text("%s", cia->tb.t_bit ? "ON" : "OFF");
        ImGui::TableNextColumn();
        ImGui::Text("Out");
        ImGui::TableNextColumn();
        ImGui::Text("%s", cia->ta.t_out ? "ON" : "OFF");
        ImGui::TableNextColumn();
        ImGui::Text("%s", cia->tb.t_out ? "ON" : "OFF");
        ImGui::TableNextColumn();
        ImGui::EndTable();
    }
    if (ImGui::BeginTable("##cia_interrupt", 2)) {
        ImGui::TableSetupColumn("Interrupt", ImGuiTableColumnFlags_WidthFixed, 72);
        ImGui::TableHeadersRow();
        ImGui::TableNextColumn();
        ImGui::Text("Mask");
        ImGui::TableNextColumn();
        ui_util_b8("", cia->intr.imr);
        ImGui::TableNextColumn();
        ImGui::Text("Control");
        ImGui::TableNextColumn();
        ui_util_b8("", cia->intr.icr);
        ImGui::TableNextColumn();
        ImGui::EndTable();
    }
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
        _ui_ria816_m6526_draw_state(win);
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
