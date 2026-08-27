#include "./ui_sgu1.h"
#include "imgui.h"
#include "imgui_internal.h" /* TableGetHeaderRowHeight */
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

// Flash duration for the clip indicator. Long enough for a human to catch a
// single isolated clip, short enough that repeated clips read as a flicker
// rather than a solid light.
#define UI_SGU1_CLIP_HOLD_SECS (0.2f)

void ui_sgu1_tick_clip(ui_sgu1_t* win) {
    CHIPS_ASSERT(win && win->valid);
    // Compare, never subtract: clip_count is monotonic but a chip reset or a
    // snapshot restore can move it backwards, and "!=" cannot wedge the lamp on.
    if (win->sgu->clip_count != win->clip_seen) {
        win->clip_seen = win->sgu->clip_count;
        win->clip_hold = UI_SGU1_CLIP_HOLD_SECS;  // retrigger, don't accumulate
    }
    else if (win->clip_hold > 0.0f) {
        // Wall clock, not emulated time: a paused machine makes no new clips so
        // the flash still decays, and fast-forward cannot blink it past noticing.
        win->clip_hold -= ImGui::GetIO().DeltaTime;
        if (win->clip_hold < 0.0f) {
            win->clip_hold = 0.0f;
        }
    }
}

bool ui_sgu1_clip_active(const ui_sgu1_t* win) {
    CHIPS_ASSERT(win);
    return win->clip_hold > 0.0f;
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

    if (ImGui::CollapsingHeader("Chip")) {
        // Everything here goes through sgu1_svc_peek, never sgu1_reg_read: this
        // panel redraws every frame and the real read path would walk the sample
        // pointer and eat the guest's status latch as a side effect.
        char magic[5];
        for (int i = 0; i < 4; i++) {
            magic[i] = (char)sgu1_svc_peek(sgu, (uint8_t)(SGU1_SVC_MAGIC + i));
        }
        magic[4] = '\0';

        ImGui::Text("%s v%u.%u", magic, sgu1_svc_peek(sgu, SGU1_SVC_VER_MAJOR), sgu1_svc_peek(sgu, SGU1_SVC_VER_MINOR));

        char uid[SGU1_SVC_UNIQUE_ID_LEN * 3];
        bool uid_zero = true;
        for (int i = 0; i < SGU1_SVC_UNIQUE_ID_LEN; i++) {
            const uint8_t b = sgu1_svc_peek(sgu, (uint8_t)(SGU1_SVC_UNIQUE_ID + i));
            uid_zero = uid_zero && (b == 0);
            snprintf(uid + i * 3, 4, "%02X ", b);
        }
        // Real silicon reads its id out of OTP and never reports all-zero, so
        // all-zero is how software knows it is talking to an emulated chip.
        ImGui::Text("Unique ID  %s%s", uid, uid_zero ? " (emulated)" : "");
        ImGui::Text(
            "PCM banks  %u  (x %u KB)",
            sgu1_svc_peek(sgu, SGU1_SVC_PCM_BANKS),
            (unsigned)(SGU_PCM_BANK_SIZE / 1024));
        ImGui::Text("Svc banks  %u", sgu1_svc_peek(sgu, SGU1_SVC_SVC_BANKS));

        ImGui::Separator();

        // The lamp runs off clip_count, so a guest read of STATUS cannot blind
        // it and this window cannot steal clips from the guest.
        const bool clip = ui_sgu1_clip_active(win);
        if (clip) {
            ImGui::PushStyleColor(ImGuiCol_Text, 0xFF0000FF);
        }
        ImGui::Text("CLIP  %s", clip ? "###" : "---");
        if (clip) {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        ImGui::Text("  %u events", sgu->clip_count);
        // The guest's own latch, peeked rather than read, so you can tell
        // whether guest code has picked its status up yet.
        const uint8_t guest_status = sgu1_svc_peek(sgu, SGU1_SVC_STATUS);
        ImGui::Text("STATUS  %02X %s", guest_status, guest_status ? "(pending guest read)" : "");

        ImGui::Separator();

        ImGui::Text("Sample ptr  bank %02X  offset %04X", sgu->svc_sample_bank, sgu->svc_sample_offset);
        ImGui::Text("Master vol  %02X", sgu->svc_master_vol);

        ImGui::Separator();

        // Issue real $Ax writes through the register path rather than calling
        // the reset helpers directly, so these buttons exercise the magic gate,
        // the domain mapping and the core's deferred reset exactly as guest code
        // would. The select is restored afterwards; the guest never observes the
        // change because the UI draws between emulated frames.
        // Plain text sits at the top of the line box while buttons carry frame
        // padding, so without this the label rides high against them.
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Reset");
        struct {
            const char* label;
            uint8_t bits;
        } const resets[] = {
            { "Voices",   SGU1_RESET_VOICES                                                         },
            { "Timebase", SGU1_RESET_TIMEBASE                                                       },
            { "Mix",      SGU1_RESET_MIX                                                            },
            { "Service",  SGU1_RESET_SVC                                                            },
            { "All",      SGU1_RESET_VOICES | SGU1_RESET_TIMEBASE | SGU1_RESET_MIX | SGU1_RESET_SVC },
        };
        for (size_t i = 0; i < sizeof(resets) / sizeof(resets[0]); i++) {
            ImGui::SameLine();
            if (ImGui::Button(resets[i].label)) {
                const uint8_t saved_select = sgu->selected_channel;
                sgu1_reg_write(sgu, SGU_REGS_PER_CH - 1, SGU1_SERVICE_BANK);
                sgu1_reg_write(sgu, SGU1_SVC_CHIP_RESET, (uint8_t)(SGU1_RESET_MAGIC | resets[i].bits));
                sgu1_reg_write(sgu, SGU_REGS_PER_CH - 1, saved_select);
            }
        }
    }

    if (ImGui::CollapsingHeader("Channels Output", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImVec4 on_ch_col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        ImVec4 off_ch_col = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        if (ImGui::BeginTable("##sgu_waves", 3)) {
            char buf[32];
            for (int i = 0; i < SGU_CHNS; i++) {
                ImGui::TableNextColumn();
                ImGui::PushID(i);
                ImVec2 area = ImGui::GetContentRegionAvail();
                area.y = h * 4.0f;
                snprintf(buf, sizeof(buf), "CH%d", i);

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
        ImGui::TableSetupColumn("CH0", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("CH1", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("CH2", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("CH3", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("CH4", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("CH5", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("CH6", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("CH7", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("CH8", ImGuiTableColumnFlags_WidthFixed, cw);

        // Hand-rolled header row rather than TableHeadersRow(), so each channel
        // heading can carry its own level meter -- the same shape the Volume
        // row uses in its cells, bar behind and label on top. The meter shows
        // what the voice is actually emitting: SGU_GetEnvelope weights each
        // operator's envelope by its OUT routing and honours the mute toggles,
        // as opposed to the "Volume" row below, which is the programmed
        // register value. Apart from the bar this mirrors TableHeadersRow(),
        // including pushing the column index as the id, which is what keeps the
        // per-column popups addressable.
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers, ImGui::TableGetHeaderRowHeight());
        for (int col = 0; col < SGU_CHNS + 1; col++) {
            if (!ImGui::TableSetColumnIndex(col)) {
                continue;
            }
            ImGui::PushID(col);
            if (col > 0) {
                float level = (float)SGU_GetEnvelope(su, (uint8_t)(col - 1)) / 8192.0f;
                level = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
                // Painted straight into the cell instead of as a widget: it
                // takes no layout space, so the header keeps its normal height,
                // and it lands above the header-row background but below the
                // label drawn by TableHeader() right after.
                const ImRect cell = ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), col);
                ImGui::GetWindowDrawList()->AddRectFilled(
                    cell.Min,
                    ImVec2(cell.Min.x + (cell.Max.x - cell.Min.x) * level, cell.Max.y),
                    ImGui::GetColorU32(ImGuiCol_FrameBgHovered));
            }
            ImGui::TableHeader(ImGui::TableGetColumnName(col));
            ImGui::PopID();
        }

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
                    ImGui::ColorConvertFloat4ToU32(ImVec4(vus[ch], vus[ch], vus[ch], vus[ch] * 2.0f / 3.0f)));
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
            const ImVec4 off_ch_col = ImGui::GetStyleColorVec4(ImGuiCol_TableRowBg);
            const ImVec4 on_ch_col = ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive);
            for (int i = 0; i < SGU_CHNS; i++) {
                ImGui::TableSetBgColor(
                    ImGuiTableBgTarget_CellBg,
                    ImGui::ColorConvertFloat4ToU32(su->chan[i].flags0 & SGU1_FLAGS0_CTL_GATE ? on_ch_col : off_ch_col));
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
