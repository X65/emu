#include "./ui_sgu1.h"
#include "chips/su/su.h"
#include "imgui.h"
#include "imgui_toggle.h"
#include "ui/ui_util.h"

#ifndef __cplusplus
    #error "implementation must be compiled as C++"
#endif
#include <string.h> /* memset */
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

static void _ui_sgu1_draw_state(ui_sgu1_t* win) {
    sgu1_t* sgu = win->sgu;
    SoundUnit* su = static_cast<SoundUnit*>(sgu->su);
    const float cw0 = 158.0f;
    const float cw = 62.0f;
    const float h = ImGui::GetTextLineHeight();

    if (ImGui::BeginTable("##su_channels", SGU1_NUM_CHANNELS + 1)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, cw0);
        ImGui::TableSetupColumn("Chn0", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Chn1", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Chn2", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Chn3", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Chn4", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Chn5", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Chn6", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableSetupColumn("Chn7", ImGuiTableColumnFlags_WidthFixed, cw);
        ImGui::TableHeadersRow();

        ImGui::TableNextColumn();
        ImGui::Text("Muted");
        ImGui::TableNextColumn();
        ImGuiToggleConfig toggle_config;
        toggle_config.Flags = ImGuiToggleFlags_Animated | ImGuiToggleFlags_A11y;
        toggle_config.Size = ImVec2(1.75f * h, h);
        toggle_config.A11yStyle = ImGuiToggleA11yStyle_Glyph;
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::PushID(i);
            ImGui::Toggle("##muted", &su->muted[i], toggle_config);
            ImGui::PopID();
            ImGui::TableNextColumn();
        }
        ImGui::Text("Frequency");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%04X", su->chan[i].freq);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Volume");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%02X", su->chan[i].vol);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Panning");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%02X", su->chan[i].pan);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Control Flags");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ui_util_b8("", su->chan[i].flags0);
            ui_util_b8("", su->chan[i].flags1);
            ImGui::TableNextColumn();
        }
        ImGui::Text("  WAVE");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            switch (su->chan[i].flags0 & SGU1_FLAGS0_WAVE_MASK) {
                case SGU1_FLAGS0_WAVE_PULSE: ImGui::Text("PULSE"); break;
                case SGU1_FLAGS0_WAVE_SAWTOOTH: ImGui::Text("SAWTOOTH"); break;
                case SGU1_FLAGS0_WAVE_SINE: ImGui::Text("SINE"); break;
                case SGU1_FLAGS0_WAVE_TRIANGLE: ImGui::Text("TRIANGLE"); break;
                case SGU1_FLAGS0_WAVE_NOISE: ImGui::Text("NOISE"); break;
                case SGU1_FLAGS0_WAVE_PERIODIC_NOISE: ImGui::Text("PERIODIC NOISE"); break;
                case SGU1_FLAGS0_WAVE_XOR_SINE: ImGui::Text("XOR SINE"); break;
                case SGU1_FLAGS0_WAVE_XOR_TRIANGLE: ImGui::Text("XOR TRIANGLE"); break;
                default: ImGui::Text("???"); break;
            }
            ImGui::TableNextColumn();
        }
        ImGui::Text("  PCM");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%s", (su->chan[i].flags0 & SGU1_FLAGS0_PCM_MASK) ? "YES" : "NO");
            ImGui::TableNextColumn();
        }
        ImGui::Text("  RINGMOD");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%s", (su->chan[i].flags0 & SGU1_FLAGS0_CTL_RING_MOD) ? "ON" : "OFF");
            ImGui::TableNextColumn();
        }
        ImGui::Text("  NSLOW");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%s", (su->chan[i].flags0 & SGU1_FLAGS0_CTL_NSLOW) ? "ON" : "OFF");
            ImGui::TableNextColumn();
        }
        ImGui::Text("  NSHIGH");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%s", (su->chan[i].flags0 & SGU1_FLAGS0_CTL_NSHIGH) ? "ON" : "OFF");
            ImGui::TableNextColumn();
        }
        ImGui::Text("  NSBAND");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%s", (su->chan[i].flags0 & SGU1_FLAGS0_CTL_NSBAND) ? "ON" : "OFF");
            ImGui::TableNextColumn();
        }
        ImGui::Text("  PHASE RESET");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%s", (su->chan[i].flags1 & SGU1_FLAGS1_PHASE_RESET) ? "ON" : "OFF");
            ImGui::TableNextColumn();
        }
        ImGui::Text("  FILTER PHASE RESET");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%s", (su->chan[i].flags1 & SGU1_FLAGS1_FILTER_PHASE_RESET) ? "ON" : "OFF");
            ImGui::TableNextColumn();
        }
        ImGui::Text("  PCM LOOP");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%s", (su->chan[i].flags1 & SGU1_FLAGS1_PCM_LOOP) ? "ON" : "OFF");
            ImGui::TableNextColumn();
        }
        ImGui::Text("  TIMER SYNC");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%s", (su->chan[i].flags1 & SGU1_FLAGS1_TIMER_SYNC) ? "ON" : "OFF");
            ImGui::TableNextColumn();
        }
        ImGui::Text("  FREQ SWEEP");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%s", (su->chan[i].flags1 & SGU1_FLAGS1_FREQ_SWEEP) ? "ON" : "OFF");
            ImGui::TableNextColumn();
        }
        ImGui::Text("  VOL SWEEP");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%s", (su->chan[i].flags1 & SGU1_FLAGS1_VOL_SWEEP) ? "ON" : "OFF");
            ImGui::TableNextColumn();
        }
        ImGui::Text("  CUT_SWEEP");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%s", (su->chan[i].flags1 & SGU1_FLAGS1_CUT_SWEEP) ? "ON" : "OFF");
            ImGui::TableNextColumn();
        }
        ImGui::Text("Cutoff");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%04X", su->chan[i].cutoff);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Duty");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%02X", su->chan[i].duty);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Reson");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%02X", su->chan[i].reson);
            ImGui::TableNextColumn();
        }
        ImGui::Text("PCM pos");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%04X", su->chan[i].pcmpos);
            ImGui::TableNextColumn();
        }
        ImGui::Text("PCM bnd");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%04X", su->chan[i].pcmbnd);
            ImGui::TableNextColumn();
        }
        ImGui::Text("PCM rst");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%04X", su->chan[i].pcmrst);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Freq Speed");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%04X", su->chan[i].swfreq.speed);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Freq Amount");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%02X", su->chan[i].swfreq.amt);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Freq Bound");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%02X", su->chan[i].swfreq.bound);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Volume Speed");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%04X", su->chan[i].swvol.speed);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Volume Amount");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%02X", su->chan[i].swvol.amt);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Volume Bound");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%02X", su->chan[i].swvol.bound);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Cutoff Speed");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%04X", su->chan[i].swcut.speed);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Cutoff Amount");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%02X", su->chan[i].swcut.amt);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Sweep Cutoff Bound");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%02X", su->chan[i].swcut.bound);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Special 1C");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%02X", su->chan[i].special1C);
            ImGui::TableNextColumn();
        }
        ImGui::Text("Special 1D");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%02X", su->chan[i].special1D);
            ImGui::TableNextColumn();
        }
        ImGui::Text("restimer");
        ImGui::TableNextColumn();
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::Text("%04X", su->chan[i].restimer);
            ImGui::TableNextColumn();
        }
        ImGui::EndTable();
    }
    if (ImGui::BeginTable("##su_waves", 2)) {
        char buf[32];
        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            ImGui::TableNextColumn();
            ImGui::PushID(i);
            ImVec2 area = ImGui::GetContentRegionAvail();
            area.y = h * 4.0f;
            snprintf(buf, sizeof(buf), "Chn%d", i);
            ImGui::PlotLines(
                "##samples",
                sgu->voice[i].sample_buffer,
                SGU1_AUDIO_SAMPLES,
                0,
                buf,
                -4096.0f,
                +4095.0f,
                area);
            ImGui::PopID();
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
