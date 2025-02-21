#include "./ui_ymf262.h"

#include "ui/ui_util.h"

#include <imgui.h>

#ifndef __cplusplus
    #error "implementation must be compiled as C++"
#endif
#include <string.h> /* memset */
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

void ui_ymf262_init(ui_ymf262_t* win, const ui_ymf262_desc_t* desc) {
    CHIPS_ASSERT(win && desc);
    CHIPS_ASSERT(desc->title);
    CHIPS_ASSERT(desc->opl3);
    memset(win, 0, sizeof(ui_ymf262_t));
    win->title = desc->title;
    win->opl3 = desc->opl3;
    win->init_x = (float)desc->x;
    win->init_y = (float)desc->y;
    win->init_w = (float)((desc->w == 0) ? 1000 : desc->w);
    win->init_h = (float)((desc->h == 0) ? 410 : desc->h);
    win->open = win->last_open = desc->open;
    win->valid = true;
    ui_chip_init(&win->chip, &desc->chip_desc);
}

void ui_ymf262_discard(ui_ymf262_t* win) {
    CHIPS_ASSERT(win && win->valid);
    win->valid = false;
}

static void _ui_ymf262_draw_state(ui_ymf262_t* win) {
    esfm_chip* opl3 = &win->opl3->chip;
    const float cw0 = 112.0f;
    const float cw = 28.0f;
    ImGui::Text("Bank0 Addr Latch: %04X", win->opl3->addr[0]);
    ImGui::Text("Bank1 Addr Latch: %04X", win->opl3->addr[1]);
    if (ImGui::CollapsingHeader("Wave Generator", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("##opl3_channels", 19)) {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, cw0);
            ImGui::TableSetupColumn("Ch00", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch01", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch02", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch03", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch04", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch05", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch06", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch07", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch08", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch09", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch10", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch11", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch12", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch13", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch14", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch15", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch16", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableSetupColumn("Ch17", ImGuiTableColumnFlags_WidthFixed, cw);
            ImGui::TableHeadersRow();
            ImGui::TableNextColumn();
            ImGui::Text("Key On");
            ImGui::TableNextColumn();
            for (int i = 0; i < 18; i++) {
                ImGui::Text("%s", opl3->channels[i].key_on ? "YES" : "NO");
                ImGui::TableNextColumn();
            }
            ImGui::Text("4 Op");
            ImGui::TableNextColumn();
            for (int i = 0; i < 18; i++) {
                ImGui::Text("%s", opl3->channels[i].emu_mode_4op_enable ? "YES" : "NO");
                ImGui::TableNextColumn();
            }
            for (int s = 0; s < 4; s++) {
                ImGui::Text("%d Freq No", s);
                ImGui::TableNextColumn();
                for (int i = 0; i < 18; i++) {
                    ImGui::Text("%03X", opl3->channels[i].slots[s].f_num);
                    ImGui::TableNextColumn();
                }
                for (int o = 0; o < 2; o++) {
                    ImGui::Text("  Out Enable %d", o);
                    ImGui::TableNextColumn();
                    for (int i = 0; i < 18; i++) {
                        ImGui::Text("%s", opl3->channels[i].slots[s].out_enable[o] ? "YES" : "NO");
                        ImGui::TableNextColumn();
                    }
                }
                ImGui::Text("  Output");
                ImGui::TableNextColumn();
                for (int i = 0; i < 18; i++) {
                    ImGui::Text("%02X", opl3->channels[i].slots[s].output_level);
                    ImGui::TableNextColumn();
                }
                ImGui::Text("  Waveform");
                ImGui::TableNextColumn();
                for (int i = 0; i < 18; i++) {
                    ImGui::Text("%01X", opl3->channels[i].slots[s].waveform);
                    ImGui::TableNextColumn();
                }
                ImGui::Text("  Attack Rate");
                ImGui::TableNextColumn();
                for (int i = 0; i < 18; i++) {
                    ImGui::Text("%01X", opl3->channels[i].slots[s].attack_rate);
                    ImGui::TableNextColumn();
                }
                ImGui::Text("  Decay Rate");
                ImGui::TableNextColumn();
                for (int i = 0; i < 18; i++) {
                    ImGui::Text("%01X", opl3->channels[i].slots[s].decay_rate);
                    ImGui::TableNextColumn();
                }
                ImGui::Text("  Sustain Level");
                ImGui::TableNextColumn();
                for (int i = 0; i < 18; i++) {
                    ImGui::Text("%01X", opl3->channels[i].slots[s].sustain_lvl);
                    ImGui::TableNextColumn();
                }
                ImGui::Text("  Release Rate");
                ImGui::TableNextColumn();
                for (int i = 0; i < 18; i++) {
                    ImGui::Text("%01X", opl3->channels[i].slots[s].release_rate);
                    ImGui::TableNextColumn();
                }
                ImGui::Text("  Tremolo Enable");
                ImGui::TableNextColumn();
                for (int i = 0; i < 18; i++) {
                    ImGui::Text("%s", opl3->channels[i].slots[s].tremolo_en ? "YES" : "NO");
                    ImGui::TableNextColumn();
                }
                ImGui::Text("  Tremolo Deep");
                ImGui::TableNextColumn();
                for (int i = 0; i < 18; i++) {
                    ImGui::Text("%s", opl3->channels[i].slots[s].tremolo_deep ? "YES" : "NO");
                    ImGui::TableNextColumn();
                }
                ImGui::Text("  Vibrato Enable");
                ImGui::TableNextColumn();
                for (int i = 0; i < 18; i++) {
                    ImGui::Text("%s", opl3->channels[i].slots[s].vibrato_en ? "YES" : "NO");
                    ImGui::TableNextColumn();
                }
                ImGui::Text("  Vibrato Deep");
                ImGui::TableNextColumn();
                for (int i = 0; i < 18; i++) {
                    ImGui::Text("%s", opl3->channels[i].slots[s].vibrato_deep ? "YES" : "NO");
                    ImGui::TableNextColumn();
                }
            }
            ImGui::EndTable();
        }
    }
}

void ui_ymf262_draw(ui_ymf262_t* win) {
    CHIPS_ASSERT(win && win->valid);
    ui_util_handle_window_open_dirty(&win->open, &win->last_open);
    if (!win->open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(win->init_x, win->init_y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(win->init_w, win->init_h), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(win->title, &win->open)) {
        ImGui::BeginChild("##ymf262_chip", ImVec2(176, 0), true);
        ui_chip_draw(&win->chip, win->opl3->pins);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##ymf262_state", ImVec2(0, 0), true);
        _ui_ymf262_draw_state(win);
        ImGui::EndChild();
    }
    ImGui::End();
}

void ui_ymf262_save_settings(ui_ymf262_t* win, ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    ui_settings_add(settings, win->title, win->open);
}

void ui_ymf262_load_settings(ui_ymf262_t* win, const ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    win->open = ui_settings_isopen(settings, win->title);
}
