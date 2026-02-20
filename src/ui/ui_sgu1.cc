#include "./ui_sgu1.h"
#include "imgui.h"
#include "imgui_toggle.h"
#include "ui/ui_util.h"
#include <algorithm>
#include <cstdio>
#include <cstdint>

#ifndef __cplusplus
    #error "implementation must be compiled as C++"
#endif
#include <string.h> /* memset */
#ifdef _MSC_VER
    #define _USE_MATH_DEFINES
#endif
#include <math.h> /* INFINITY */
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

void ui_sgu1_init(ui_sgu1_t* win, const ui_sgu1_desc_t* desc) {
    CHIPS_ASSERT(win && desc);
    CHIPS_ASSERT(desc->title);
    CHIPS_ASSERT(desc->sgu);
    memset(win, 0, sizeof(ui_sgu1_t));
    win->title = desc->title;
    win->sgu = desc->sgu;
    win->init_x = (float)desc->x;
    win->init_y = (float)desc->y;
    win->init_w = (float)((desc->w == 0) ? 496 : desc->w);
    win->init_h = (float)((desc->h == 0) ? 410 : desc->h);
    win->open = win->last_open = desc->open;
    win->valid = true;
    ui_chip_init(&win->chip, &desc->chip_desc);
}

void ui_sgu1_discard(ui_sgu1_t* win) {
    CHIPS_ASSERT(win && win->valid);
    win->valid = false;
}

static void ui_util_s8(int8_t val) {
    ImGui::Text("%s%02X", val < 0 ? "-" : "", val < 0 ? -val : val);
}

static void _ui_sgu1_op_flags(uint8_t reg, uint8_t value) {
    switch (reg) {
        // R0: [7]TRM [6]VIB [5:4]KSR [3:0]MUL
        case 0: {
            ImGui::Text(
                "%s %s KSR:%01X MUL:%01X",
                SGU_OP0_TRM(value) ? "TRM" : "trm",
                SGU_OP0_VIB(value) ? "VIB" : "vib",
                SGU_OP0_KSR(value),
                SGU_OP0_MUL(value));
        } break;
        // R1: [7:6]KSL [5:0]TL_lo6
        case 1: {
            ImGui::Text("KSL:%01X TL<:%02X", SGU_OP1_KSL(value), SGU_OP1_TL_LO6(value));
        } break;
        // R2: [7:4]AR_lo4 [3:0]DR_lo4
        case 2: {
            ImGui::Text("AR<:%01X DR<:%01X", SGU_OP2_AR_LO4(value), SGU_OP2_DR_LO4(value));
        } break;
        // R3: [7:4]SL [3:0]RR
        case 3: {
            ImGui::Text("SL:%01X  RR:%01X", SGU_OP3_SL(value), SGU_OP3_RR(value));
        } break;
        // R4: [7:5]DT [4:0]SR
        case 4: {
            ImGui::Text("DT:%01X  SR:%02X", SGU_OP4_DT(value), SGU_OP4_SR(value));
        } break;
        // R5: [7:5]DELAY [4]FIX [3:0]WPAR
        case 5: {
            ImGui::Text(
                "DEL:%01X %s WPAR:%01X",
                SGU_OP5_DELAY(value),
                SGU_OP5_FIX(value) ? "FIX" : "   ",
                SGU_OP5_WPAR(value));
        } break;
        // R6: [7]TRMD [6]VIBD [5]SYNC [4]RING [3:1]MOD [0]TL_msb
        case 6: {
            ImGui::Text(
                "%s %s %s %s MOD:%01X TL>:%01X",
                SGU_OP6_TRMD(value) ? "TRMD" : "trmd",
                SGU_OP6_VIBD(value) ? "VIBD" : "vibd",
                SGU_OP6_SYNC(value) ? "SYNC" : "sync",
                SGU_OP6_RING(value) ? "RING" : "ring",
                SGU_OP6_MOD(value),
                SGU_OP6_TL_MSB(value));
        } break;
        // R7: [7:5]OUT [4]AR_msb [3]DR_msb [2:0]WAVE
        case 7: {
            ImGui::Text(
                "OUT:%01X AR>:%01X DR>:%01X WAV:%01X",
                SGU_OP7_OUT(value),
                SGU_OP7_AR_MSB(value),
                SGU_OP7_DR_MSB(value),
                SGU_OP7_WAVE(value));
        } break;
    }
}

static void _ui_sgu1_draw_state(ui_sgu1_t* win) {
    sgu1_t* sgu = win->sgu;
    struct SGU* su = &sgu->sgu;
    const float cw0 = 158.0f;
    const float cw = 62.0f;
    const float h = ImGui::GetTextLineHeight();

    if (ImGui::CollapsingHeader("Channels Output", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImVec4 on_ch_col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        ImVec4 off_ch_col = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        if (ImGui::BeginTable("##sgu_waves", 2)) {
            char buf[32];
            for (int i = 0; i < SGU_CHNS; i++) {
                ImGui::TableNextColumn();
                ImGui::PushID(i);
                ImVec2 area = ImGui::GetContentRegionAvail();
                area.y = h * 4.0f;
                snprintf(buf, sizeof(buf), "Chn%d", i);

                ImGui::PushStyleColor(
                    ImGuiCol_PlotLines,
                    (su->chan[i].flags0 & SGU1_FLAGS0_CTL_GATE) ? on_ch_col : off_ch_col);
                ImGui::PlotLines(
                    "##samples",
                    sgu->voice[i].sample_buffer,
                    SGU1_AUDIO_SAMPLES,
                    sgu->voice[i].sample_pos,
                    buf,
                    -(1 << 15),
                    1 << 15,
                    area);
                ImGui::PopStyleColor();
                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }

    if (ImGui::BeginTable("##su_channels", SGU_CHNS + 1)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, cw0);
        ImGui::TableSetupColumn("Chn0", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Chn1", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Chn2", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Chn3", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Chn4", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Chn5", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Chn6", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Chn7", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Chn8", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableHeadersRow();

        ImGui::TableNextColumn();
        ImGui::Text("Muted");
        ImGui::TableNextColumn();
        ImGuiToggleConfig toggle_config;
        toggle_config.Flags = ImGuiToggleFlags_Animated | ImGuiToggleFlags_A11y;
        toggle_config.Size = ImVec2(1.75f * h, h);
        toggle_config.A11yStyle = ImGuiToggleA11yStyle_Glyph;
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::PushID(i);
            ImGui::Toggle("##muted", &su->muted[i], toggle_config);
            ImGui::PopID();
            ImGui::TableNextColumn();
        }
        ImGui::Text("Frequency");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%04X", su->chan[i].freq);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Volume");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            const int abs = su->chan[i].vol < 0 ? -su->chan[i].vol : su->chan[i].vol;
            char buf[16];
            snprintf(buf, sizeof(buf), "%s%02X", su->chan[i].vol < 0 ? "-" : "", abs);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::GetColorU32(ImGuiCol_FrameBgHovered));
            ImGui::ProgressBar((float)abs / 0x7F, ImVec2(-1.0f, 0.0f), buf);
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::TableNextColumn();
        }
        ImGui::Text("Panning");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ui_util_s8(su->chan[i].pan);
            ImGui::TableNextColumn();
        }
        if (ImGui::CollapsingHeader("Operators")) {
            char buf[16];
            ImGui::TableNextColumn();
            for (int ch = 0; ch < SGU_CHNS; ch++) {
                for (int op = 0; op < SGU_OP_PER_CH; op++) {
                    snprintf(buf, sizeof(buf), "OP%d", op);
                    ImGui::SeparatorText(buf);
                    if (ImGui::BeginItemTooltip()) {
                        ImGui::TextDisabled("Channel %u Operator %u Registers", ch, op);
                        for (uint8_t r = 0; r < SGU_OP_REGS; r++) {
                            ImGui::Text("R%u:", r);
                            ImGui::SameLine();
                            _ui_sgu1_op_flags(r, ((uint8_t*)&su->chan[ch].op[op])[r]);
                        }
                        ImGui::Text(
                            "AR: %02X, DR: %02X, SL: %02X, SR: %02X, RR: %02X",
                            SGU_OP27_AR(su->chan[ch].op[op].reg2, su->chan[ch].op[op].reg7),
                            SGU_OP27_DR(su->chan[ch].op[op].reg2, su->chan[ch].op[op].reg7),
                            SGU_OP3_SL(su->chan[ch].op[op].reg3),
                            SGU_OP4_SR(su->chan[ch].op[op].reg4),
                            SGU_OP3_RR(su->chan[ch].op[op].reg3));
                        static const char* wave_names[] = { "Sine",  "Triangle",       "Sawtooth", "Pulse",
                                                            "Noise", "Periodic Noise", "Reserved", "Sample" };
                        ImGui::Text(
                            "TL: %02X, Wave: %s",
                            SGU_OP16_TL(su->chan[ch].op[op].reg1, su->chan[ch].op[op].reg6),
                            wave_names[SGU_OP7_WAVE(su->chan[ch].op[op].reg7)]);
                        ImGui::EndTooltip();
                    }
                    for (uint8_t r = 0; r < SGU_OP_REGS; r++) {
                        ui_util_b8("", ((uint8_t*)&su->chan[ch].op[op])[r]);
                        if (ImGui::BeginItemTooltip()) {
                            ImGui::TextDisabled("Channel %u Operator %u", ch, op);
                            ImGui::Text("R%u:", r);
                            ImGui::SameLine();
                            _ui_sgu1_op_flags(r, ((uint8_t*)&su->chan[ch].op[op])[r]);
                            ImGui::EndTooltip();
                        }
                    }
                }
                ImGui::TableNextColumn();
            }
        }
        else {
            static float vus[SGU_CHNS] = { 0 };
            static uint8_t op_states[SGU_CHNS] = { 0 };
            ImGui::TableNextColumn();
            for (int ch = 0; ch < SGU_CHNS; ch++) {
                uint8_t op_state = 0;
                for (int op = 0; op < SGU_OP_PER_CH; op++) {
                    for (int reg = 0; reg < SGU_OP_REGS; reg++) {
                        op_state ^= ((uint8_t*)&su->chan[ch].op[op])[reg];
                    }
                }
                if (op_state != op_states[ch]) {
                    op_states[ch] = op_state;
                    vus[ch] = 1.0f;
                }
                else {
                    vus[ch] -= 0.033f;
                    if (vus[ch] < 0.0f) {
                        vus[ch] = 0.0f;
                    }
                }
                ImGui::TableSetBgColor(
                    ImGuiTableBgTarget_CellBg,
                    ImGui::ColorConvertFloat4ToU32(ImVec4(
                        vus[ch],
                        vus[ch],
                        vus[ch],
                        vus[ch] * 2.0f / 3.0f)));  // Semi-transparent red
                ImGui::TableNextColumn();
            }
        }
        if (ImGui::CollapsingHeader("Control Flags", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TableNextColumn();
            for (int i = 0; i < SGU_CHNS; i++) {
                ui_util_b8("", su->chan[i].flags0);
                ui_util_b8("", su->chan[i].flags1);
                ImGui::TableNextColumn();
            }
            ImGui::Text("  GATE");
            ImGui::TableNextColumn();
            for (int i = 0; i < SGU_CHNS; i++) {
                ImGui::PushID(i);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
                if (ImGui::Button((su->chan[i].flags0 & SGU1_FLAGS0_CTL_GATE) ? "ON" : "OFF")) {
                    if (su->chan[i].flags0 & SGU1_FLAGS0_CTL_GATE) {
                        sgu1_direct_reg_write(
                            sgu,
                            (uint16_t)(i * SGU_REGS_PER_CH + SGU_OP_PER_CH * SGU_OP_REGS + SGU1_CHN_FLAGS0),
                            su->chan[i].flags0 & ~SGU1_FLAGS0_CTL_GATE);
                        su->chan[i].flags0 &= ~SGU1_FLAGS0_CTL_GATE;
                    }
                    else {
                        sgu1_direct_reg_write(
                            sgu,
                            (uint16_t)(i * SGU_REGS_PER_CH + SGU_OP_PER_CH * SGU_OP_REGS + SGU1_CHN_FLAGS0),
                            su->chan[i].flags0 | SGU1_FLAGS0_CTL_GATE);
                    }
                }
                ImGui::PopStyleVar();
                ImGui::TableNextColumn();
                ImGui::PopID();
            }
            ImGui::Text("  PCM");
            ImGui::TableNextColumn();
            for (int i = 0; i < SGU_CHNS; i++) {
                ImGui::Text("%s", (su->chan[i].flags0 & SGU1_FLAGS0_PCM_MASK) ? "YES" : "NO");
                ImGui::TableNextColumn();
            }
            ImGui::Text("  RINGMOD");
            ImGui::TableNextColumn();
            for (int i = 0; i < SGU_CHNS; i++) {
                ImGui::Text("%s", (su->chan[i].flags0 & SGU1_FLAGS0_CTL_RING_MOD) ? "ON" : "OFF");
                ImGui::TableNextColumn();
            }
            ImGui::Text("  NSLOW");
            ImGui::TableNextColumn();
            for (int i = 0; i < SGU_CHNS; i++) {
                ImGui::Text("%s", (su->chan[i].flags0 & SGU1_FLAGS0_CTL_NSLOW) ? "ON" : "OFF");
                ImGui::TableNextColumn();
            }
            ImGui::Text("  NSHIGH");
            ImGui::TableNextColumn();
            for (int i = 0; i < SGU_CHNS; i++) {
                ImGui::Text("%s", (su->chan[i].flags0 & SGU1_FLAGS0_CTL_NSHIGH) ? "ON" : "OFF");
                ImGui::TableNextColumn();
            }
            ImGui::Text("  NSBAND");
            ImGui::TableNextColumn();
            for (int i = 0; i < SGU_CHNS; i++) {
                ImGui::Text("%s", (su->chan[i].flags0 & SGU1_FLAGS0_CTL_NSBAND) ? "ON" : "OFF");
                ImGui::TableNextColumn();
            }
            ImGui::Text("  PHASE RESET");
            ImGui::TableNextColumn();
            for (int i = 0; i < SGU_CHNS; i++) {
                ImGui::Text("%s", (su->chan[i].flags1 & SGU1_FLAGS1_PHASE_RESET) ? "ON" : "OFF");
                ImGui::TableNextColumn();
            }
            ImGui::Text("  FILTER PHASE RESET");
            ImGui::TableNextColumn();
            for (int i = 0; i < SGU_CHNS; i++) {
                ImGui::Text("%s", (su->chan[i].flags1 & SGU1_FLAGS1_FILTER_PHASE_RESET) ? "ON" : "OFF");
                ImGui::TableNextColumn();
            }
            ImGui::Text("  PCM LOOP");
            ImGui::TableNextColumn();
            for (int i = 0; i < SGU_CHNS; i++) {
                ImGui::Text("%s", (su->chan[i].flags1 & SGU1_FLAGS1_PCM_LOOP) ? "ON" : "OFF");
                ImGui::TableNextColumn();
            }
            ImGui::Text("  TIMER SYNC");
            ImGui::TableNextColumn();
            for (int i = 0; i < SGU_CHNS; i++) {
                ImGui::Text("%s", (su->chan[i].flags1 & SGU1_FLAGS1_TIMER_SYNC) ? "ON" : "OFF");
                ImGui::TableNextColumn();
            }
            ImGui::Text("  FREQ SWEEP");
            ImGui::TableNextColumn();
            for (int i = 0; i < SGU_CHNS; i++) {
                ImGui::Text("%s", (su->chan[i].flags1 & SGU1_FLAGS1_FREQ_SWEEP) ? "ON" : "OFF");
                ImGui::TableNextColumn();
            }
            ImGui::Text("  VOL SWEEP");
            ImGui::TableNextColumn();
            for (int i = 0; i < SGU_CHNS; i++) {
                ImGui::Text("%s", (su->chan[i].flags1 & SGU1_FLAGS1_VOL_SWEEP) ? "ON" : "OFF");
                ImGui::TableNextColumn();
            }
            ImGui::Text("  CUT SWEEP");
            ImGui::TableNextColumn();
            for (int i = 0; i < SGU_CHNS; i++) {
                ImGui::Text("%s", (su->chan[i].flags1 & SGU1_FLAGS1_CUT_SWEEP) ? "ON" : "OFF");
                ImGui::TableNextColumn();
            }
        }
        else {
            ImGui::TableNextColumn();
            for (int i = 0; i < SGU_CHNS; i++) {
                ui_util_b8("", su->chan[i].flags0);
                ui_util_b8("", su->chan[i].flags1);
                ImGui::TableNextColumn();
            }
        }
        ImGui::Text("Cutoff");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%04X", su->chan[i].cutoff);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Duty");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%02X", su->chan[i].duty);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Reson");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%02X", su->chan[i].reson);
            ImGui::TableNextColumn();
        }
        ImGui::Text("PCM pos");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%04X", su->chan[i].pcmpos);
            ImGui::TableNextColumn();
        }
        ImGui::Text("PCM bnd");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%04X", su->chan[i].pcmbnd);
            ImGui::TableNextColumn();
        }
        ImGui::Text("PCM rst");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%04X", su->chan[i].pcmrst);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Freq Speed");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%04X", su->chan[i].swfreq.speed);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Freq Amount");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%02X", su->chan[i].swfreq.amt);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Freq Bound");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%02X", su->chan[i].swfreq.bound);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Volume Speed");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%04X", su->chan[i].swvol.speed);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Volume Amount");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%02X", su->chan[i].swvol.amt);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Volume Bound");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%02X", su->chan[i].swvol.bound);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Cutoff Speed");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%04X", su->chan[i].swcut.speed);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Cutoff Amount");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%02X", su->chan[i].swcut.amt);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Cutoff Bound");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%02X", su->chan[i].swcut.bound);
            ImGui::TableNextColumn();
        }
        ImGui::Text("restimer");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU_CHNS; i++) {
            ImGui::Text("%04X", su->chan[i].restimer);
            ImGui::TableNextColumn();
        }
        ImGui::EndTable();
    }
}

void ui_sgu1_draw(ui_sgu1_t* win) {
    CHIPS_ASSERT(win && win->valid);
    ui_util_handle_window_open_dirty(&win->open, &win->last_open);
    if (!win->open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(win->init_x, win->init_y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(win->init_w, win->init_h), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(win->title, &win->open)) {
        ImGui::BeginChild("##sgu1_chip", ImVec2(176, 0), true);
        ui_chip_draw(&win->chip, win->sgu->pins);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##sgu1_state", ImVec2(0, 0), true);
        _ui_sgu1_draw_state(win);
        ImGui::EndChild();
    }
    ImGui::End();
}

void ui_sgu1_save_settings(ui_sgu1_t* win, ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    ui_settings_add(settings, win->title, win->open);
}

void ui_sgu1_load_settings(ui_sgu1_t* win, const ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    win->open = ui_settings_isopen(settings, win->title);
}
