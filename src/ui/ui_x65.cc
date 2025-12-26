#include "./ui_x65.h"
#include "../log.h"

#include "imgui.h"
#include "IconsLucide.h"
#include "args.h"
#include <filesystem>

#ifdef __EMSCRIPTEN__
    #include <emscripten/version.h>
#endif

#ifndef __cplusplus
    #error "implementation must be compiled as C++"
#endif
#include <string.h> /* memset */
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif
#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

static void _ui_x65_draw_menu(ui_x65_t* ui) {
    CHIPS_ASSERT(ui && ui->x65 && ui->boot_cb);
    if (ImGui::BeginMainMenuBar()) {
        ImGui::Text("%s", ui->x65->running ? (ui->dbg.dbg.stopped ? ICON_LC_PAUSE : ICON_LC_PLAY) : ICON_LC_SQUARE);
        if (arguments.rom && ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", arguments.rom);
            ImGui::EndTooltip();
        }
        if (ImGui::SmallButton(ICON_LC_ROTATE_CCW)) {
            LOG_INFO("=== RESET ===");
            x65_reset(ui->x65);
            ui_dbg_reset(&ui->dbg);
        }
        if (ImGui::SmallButton(ICON_LC_POWER)) {
            LOG_INFO("=== COLD BOOT ===");
            ui->boot_cb(ui->x65);
            ui_dbg_reboot(&ui->dbg);
        }
        if (ImGui::BeginMenu("System")) {
            if (ImGui::MenuItem(ui->x65->running ? "Running" : ICON_LC_PLAY " Run", 0, ui->x65->running)) {
                x65_set_running(ui->x65, !ui->x65->running);
            }
            ui_snapshot_menus(&ui->snapshot);
            if (ImGui::MenuItem(ICON_LC_ROTATE_CCW " Reset")) {
                LOG_INFO("=== RESET ===");
                x65_reset(ui->x65);
                ui_dbg_reset(&ui->dbg);
            }
            if (ImGui::MenuItem(ICON_LC_POWER " Cold Boot")) {
                LOG_INFO("=== COLD BOOT ===");
                ui->boot_cb(ui->x65);
                ui_dbg_reboot(&ui->dbg);
            }
            if (ImGui::BeginMenu(ICON_LC_GAMEPAD " Controller")) {
                if (ImGui::MenuItem("None", 0, ui->x65->joystick_type == X65_JOYSTICKTYPE_NONE)) {
                    ui->x65->joystick_type = X65_JOYSTICKTYPE_NONE;
                }
                if (ImGui::MenuItem(
                        ICON_LC_JOYSTICK " Digital #1",
                        0,
                        ui->x65->joystick_type == X65_JOYSTICKTYPE_DIGITAL_1)) {
                    ui->x65->joystick_type = X65_JOYSTICKTYPE_DIGITAL_1;
                }
                if (ImGui::MenuItem(
                        ICON_LC_JOYSTICK " Digital #2",
                        0,
                        ui->x65->joystick_type == X65_JOYSTICKTYPE_DIGITAL_2)) {
                    ui->x65->joystick_type = X65_JOYSTICKTYPE_DIGITAL_2;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(ICON_LC_MICROCHIP " Hardware")) {
            ImGui::MenuItem(ICON_LC_AUDIO_WAVEFORM " Audio Output", 0, &ui->audio.open);
            ImGui::MenuItem(ICON_LC_MONITOR " Display", 0, &ui->display.open);
            ImGui::MenuItem(ICON_LC_CPU " WDC 65C816 (CPU)", 0, &ui->cpu.open);
            ImGui::MenuItem(ICON_LC_HDMI_PORT " CGIA (VPU)", 0, &ui->cgia.open);
            ImGui::MenuItem(ICON_LC_AUDIO_LINES " MOS 6581 (SID)", 0, &ui->sid.open);
            ImGui::MenuItem(ICON_LC_BLEND " RIA816 (Bridge)", 0, &ui->ria.open);
            ImGui::MenuItem(ICON_LC_MICROCHIP " TI TCA6416A (GPIO)", 0, &ui->gpio.open);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(ICON_LC_BUG " Debug")) {
            ImGui::MenuItem(ICON_LC_CABLE " UART Console (USB CDC)", 0, &ui->ria_uart.open);
            ImGui::MenuItem(ICON_LC_CPU " CPU Debugger", 0, &ui->dbg.ui.open);
            ImGui::MenuItem(ICON_LC_BUG_PLAY " Breakpoints", 0, &ui->dbg.ui.breakpoints.open);
            ImGui::MenuItem(ICON_LC_TIMER " Stopwatch", 0, &ui->dbg.ui.stopwatch.open);
            ImGui::MenuItem("Execution History", 0, &ui->dbg.ui.history.open);
            ImGui::MenuItem("Memory Heatmap", 0, &ui->dbg.ui.heatmap.open);
            if (ImGui::BeginMenu("Memory Editor")) {
                ImGui::MenuItem("Window #1", 0, &ui->memedit[0].open);
                ImGui::MenuItem("Window #2", 0, &ui->memedit[1].open);
                ImGui::MenuItem("Window #3", 0, &ui->memedit[2].open);
                ImGui::MenuItem("Window #4", 0, &ui->memedit[3].open);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Disassembler")) {
                ImGui::MenuItem("Window #1", 0, &ui->dasm[0].open);
                ImGui::MenuItem("Window #2", 0, &ui->dasm[1].open);
                ImGui::MenuItem("Window #3", 0, &ui->dasm[2].open);
                ImGui::MenuItem("Window #4", 0, &ui->dasm[3].open);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools")) {
            ImGui::MenuItem("About...", NULL, &ui->show_about);
            ImGui::MenuItem(ICON_LC_LOGS " Log", 0, &ui->app_log.open);
            ui_util_options_menu();
            if (ui->inject.menu_cb) {
                ui->inject.menu_cb();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

static void _ui_x65_draw_about(ui_x65_t* ui) {
    CHIPS_ASSERT(ui);
    if (!ui->show_about) {
        return;
    }
    if (!ImGui::Begin("About Emu", &ui->show_about, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }
    ImGui::Text("%s (%s)", app_name, app_version);

    ImGui::TextLinkOpenURL("Homepage", "https://x65.zone/");
    ImGui::SameLine();
    ImGui::TextLinkOpenURL("Releases", app_releases_address);
    ImGui::SameLine();
    ImGui::TextLinkOpenURL("Bugs", app_bug_address);

    ImGui::Separator();
    ImGui::Text("By Tomasz Sterna and X65 project contributors.");
    ImGui::Text("Licensed under the 0BSD License, see LICENSE for more information.");
    ImGui::Text("Based on");
    ImGui::SameLine();
    ImGui::TextLinkOpenURL("chips emulators", "https://github.com/floooh/chips");
    ImGui::SameLine();
    ImGui::Text("by Andre Weissflog.");

    static bool show_config_info = false;
    ImGui::Checkbox("Config/Build Information", &show_config_info);
    if (show_config_info) {
        bool copy_to_clipboard = ImGui::Button("Copy to clipboard");
        ImVec2 child_size = ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 18);
        ImGui::BeginChild(ImGui::GetID("cfg_infos"), child_size, ImGuiChildFlags_FrameStyle);
        if (copy_to_clipboard) {
            ImGui::LogToClipboard();
            ImGui::LogText("```\n");  // Back quotes will make text appears without formatting when pasting on GitHub
        }

        ImGui::Text("%s (%s)", app_name, app_version);
        ImGui::Separator();
        ImGui::Text(
            "sizeof(size_t): %d, sizeof(ImDrawIdx): %d, sizeof(ImDrawVert): %d",
            (int)sizeof(size_t),
            (int)sizeof(ImDrawIdx),
            (int)sizeof(ImDrawVert));
        ImGui::Text("define: __cplusplus=%d", (int)__cplusplus);
#ifdef _WIN32
        ImGui::Text("define: _WIN32");
#endif
#ifdef _WIN64
        ImGui::Text("define: _WIN64");
#endif
#ifdef __linux__
        ImGui::Text("define: __linux__");
#endif
#ifdef __APPLE__
        ImGui::Text("define: __APPLE__");
#endif
#ifdef _MSC_VER
        ImGui::Text("define: _MSC_VER=%d", _MSC_VER);
#endif
#ifdef _MSVC_LANG
        ImGui::Text("define: _MSVC_LANG=%d", (int)_MSVC_LANG);
#endif
#ifdef __MINGW32__
        ImGui::Text("define: __MINGW32__");
#endif
#ifdef __MINGW64__
        ImGui::Text("define: __MINGW64__");
#endif
#ifdef __GNUC__
        ImGui::Text("define: __GNUC__=%d", (int)__GNUC__);
#endif
#ifdef __clang_version__
        ImGui::Text("define: __clang_version__=%s", __clang_version__);
#endif
#ifdef __EMSCRIPTEN__
        ImGui::Text("define: __EMSCRIPTEN__");
        ImGui::Text("Emscripten: %d.%d.%d", __EMSCRIPTEN_major__, __EMSCRIPTEN_minor__, __EMSCRIPTEN_tiny__);
#else
        ImGui::Text("CWD: %s", std::filesystem::current_path().c_str());
#endif
        ImGui::Separator();
        ImGui::Text("ROM = %s", arguments.rom);
        ImGui::Text("OUTPUT_FILE = %s", arguments.output_file);
        ImGui::Text("VERBOSE = %s", arguments.verbose ? "yes" : "no");
        ImGui::Text("SILENT = %s", arguments.silent ? "yes" : "no");
        ImGui::Text("ZERO_MEM = %s", arguments.zeromem ? "on" : "off");
        ImGui::Text("JOYSTICK = %s", arguments.joy ? "on" : "off");
        ImGui::Text("DAP = %s", arguments.dap ? "on" : "off");
        ImGui::Text("DAP_PORT = %s", arguments.dap_port);

        if (copy_to_clipboard) {
            ImGui::LogText("\n```\n");
            ImGui::LogFinish();
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

/* keep disassembler layer at the start */
#define _UI_X65_MEMLAYER_CPU  (0) /* CPU visible mapping */
#define _UI_X65_MEMLAYER_RAM  (1) /* RAM banks */
#define _UI_X65_MEMLAYER_VRAM (2) /* CGIA VRAM banks */
#define _UI_X65_MEMLAYER_NUM  (3)
#define _UI_X65_CODELAYER_NUM (3) /* number of valid layers for disassembler */

static const char* _ui_x65_memlayer_names[_UI_X65_MEMLAYER_NUM] = {
    "CPU Mapped",
    "RAM Bank",
    "VRAM Cache Bank",
};
static const int _ui_x65_memlayer_banks[_UI_X65_MEMLAYER_NUM] = {
    256,
    256,
    2,
};

static uint8_t _ui_x65_mem_read(int layer, int bank, uint16_t addr, void* user_data) {
    CHIPS_ASSERT(user_data);
    ui_x65_t* ui = (ui_x65_t*)user_data;
    x65_t* x65 = ui->x65;
    switch (layer) {
        case _UI_X65_MEMLAYER_CPU: return mem_rd(x65, (uint8_t)bank, addr);
        case _UI_X65_MEMLAYER_RAM: return x65->ram[((bank & 0xFF) << 16) | addr];
        case _UI_X65_MEMLAYER_VRAM: return x65->cgia.vram[bank & 0x1][addr];
        default: return 0xFF;
    }
}

static void _ui_x65_mem_write(int layer, int bank, uint16_t addr, uint8_t data, void* user_data) {
    CHIPS_ASSERT(user_data);
    ui_x65_t* ui = (ui_x65_t*)user_data;
    x65_t* x65 = ui->x65;
    switch (layer) {
        case _UI_X65_MEMLAYER_CPU: mem_wr(x65, (uint8_t)bank, addr, data); break;
        case _UI_X65_MEMLAYER_RAM: x65->ram[((bank & 0xFF) << 16) | addr] = data; break;
        case _UI_X65_MEMLAYER_VRAM: x65->cgia.vram[bank & 0x1][addr] = data; break;
    }
}

static int _ui_x65_eval_bp(ui_dbg_t* dbg_win, int trap_id, uint64_t pins, void* user_data) {
    (void)pins;
    CHIPS_ASSERT(user_data);
    ui_x65_t* ui = (ui_x65_t*)user_data;
    x65_t* x65 = ui->x65;
    int scanline = x65->cgia.scan_line;
    for (int i = 0; (i < dbg_win->dbg.num_breakpoints) && (trap_id == 0); i++) {
        const ui_dbg_breakpoint_t* bp = &dbg_win->dbg.breakpoints[i];
        if (bp->enabled) {
            switch (bp->type) {
                /* scanline number */
                case UI_DBG_BREAKTYPE_USER + 0:
                    if ((ui->dbg_scanline != scanline) && (scanline == bp->val)) {
                        trap_id = UI_DBG_BP_BASE_TRAPID + i;
                    }
                    break;
                /* next scanline */
                case UI_DBG_BREAKTYPE_USER + 1:
                    if (ui->dbg_scanline != scanline) {
                        trap_id = UI_DBG_BP_BASE_TRAPID + i;
                    }
                    break;
                /* next frame */
                case UI_DBG_BREAKTYPE_USER + 2:
                    if ((ui->dbg_scanline != scanline) && (scanline == 0)) {
                        trap_id = UI_DBG_BP_BASE_TRAPID + i;
                    }
                    break;
            }
        }
    }
    ui->dbg_scanline = scanline;
    return trap_id;
}

static const ui_chip_pin_t _ui_x65_cpu65816_pins[] = {
    { "D0",    0,  W65816_D0    },
    { "D1",    1,  W65816_D1    },
    { "D2",    2,  W65816_D2    },
    { "D3",    3,  W65816_D3    },
    { "D4",    4,  W65816_D4    },
    { "D5",    5,  W65816_D5    },
    { "D6",    6,  W65816_D6    },
    { "D7",    7,  W65816_D7    },
    { "RW",    9,  W65816_RW    },
    { "VPA",   10, W65816_VPA   },
    { "VDA",   11, W65816_VDA   },
    { "RDY",   12, W65816_RDY   },
    { "IRQ",   13, W65816_IRQ   },
    { "NMI",   14, W65816_NMI   },
    { "RES",   15, W65816_RES   },
    { "ABORT", 16, W65816_ABORT },
    { "A0",    24, W65816_A0    },
    { "A1",    25, W65816_A1    },
    { "A2",    26, W65816_A2    },
    { "A3",    27, W65816_A3    },
    { "A4",    28, W65816_A4    },
    { "A5",    29, W65816_A5    },
    { "A6",    30, W65816_A6    },
    { "A7",    31, W65816_A7    },
    { "A8",    32, W65816_A8    },
    { "A9",    33, W65816_A9    },
    { "A10",   34, W65816_A10   },
    { "A11",   35, W65816_A11   },
    { "A12",   36, W65816_A12   },
    { "A13",   37, W65816_A13   },
    { "A14",   38, W65816_A14   },
    { "A15",   39, W65816_A15   },
    { "A16",   40, W65816_A16   },
    { "A17",   41, W65816_A17   },
    { "A18",   42, W65816_A18   },
    { "A19",   43, W65816_A19   },
    { "A20",   44, W65816_A20   },
    { "A21",   45, W65816_A21   },
    { "A22",   46, W65816_A22   },
    { "A23",   47, W65816_A23   },
};

static const ui_chip_pin_t _ui_x65_ria_pins[] = {
    { "D0",  0,  RIA816_D0  },
    { "D1",  1,  RIA816_D1  },
    { "D2",  2,  RIA816_D2  },
    { "D3",  3,  RIA816_D3  },
    { "D4",  4,  RIA816_D4  },
    { "D5",  5,  RIA816_D5  },
    { "D6",  6,  RIA816_D6  },
    { "D7",  7,  RIA816_D7  },
    { "RW",  9,  RIA816_RW  },
    { "CS",  10, RIA816_CS  },
    { "RS0", 11, RIA816_RS0 },
    { "RS1", 12, RIA816_RS1 },
    { "RS2", 13, RIA816_RS2 },
    { "RS3", 14, RIA816_RS3 },
    { "RS4", 15, RIA816_RS4 },
    { "RS5", 16, RIA816_RS5 },
    { "IRQ", 19, RIA816_IRQ },
};

static const ui_chip_pin_t _ui_x65_gpio_pins[] = {
    { "D0",    0,  TCA6416A_D0    },
    { "D1",    1,  TCA6416A_D1    },
    { "D2",    2,  TCA6416A_D2    },
    { "D3",    3,  TCA6416A_D3    },
    { "D4",    4,  TCA6416A_D4    },
    { "D5",    5,  TCA6416A_D5    },
    { "D6",    6,  TCA6416A_D6    },
    { "D7",    7,  TCA6416A_D7    },
    { "RS0",   9,  TCA6416A_RS0   },
    { "RS1",   10, TCA6416A_RS1   },
    { "RS2",   11, TCA6416A_RS2   },
    { "RW",    13, TCA6416A_RW    },
    { "CS",    14, TCA6416A_CS    },
    { "RESET", 15, TCA6416A_RESET },
    { "INT",   16, TCA6416A_INT   },
    { "P00",   17, TCA6416A_P00   },
    { "P01",   18, TCA6416A_P01   },
    { "P02",   19, TCA6416A_P02   },
    { "P03",   20, TCA6416A_P03   },
    { "P04",   21, TCA6416A_P04   },
    { "P05",   22, TCA6416A_P05   },
    { "P06",   23, TCA6416A_P06   },
    { "P07",   24, TCA6416A_P07   },
    { "P10",   26, TCA6416A_P10   },
    { "P11",   27, TCA6416A_P11   },
    { "P12",   28, TCA6416A_P12   },
    { "P13",   29, TCA6416A_P13   },
    { "P14",   30, TCA6416A_P14   },
    { "P15",   31, TCA6416A_P15   },
    { "P16",   32, TCA6416A_P16   },
    { "P17",   33, TCA6416A_P17   },
};

static const ui_chip_pin_t _ui_x65_sid_pins[] = {
    { "D0", 0,  M6581_D0 },
    { "D1", 1,  M6581_D1 },
    { "D2", 2,  M6581_D2 },
    { "D3", 3,  M6581_D3 },
    { "D4", 4,  M6581_D4 },
    { "D5", 5,  M6581_D5 },
    { "D6", 6,  M6581_D6 },
    { "D7", 7,  M6581_D7 },
    { "A0", 8,  M6581_A0 },
    { "A1", 9,  M6581_A1 },
    { "A2", 10, M6581_A2 },
    { "A3", 11, M6581_A3 },
    { "CS", 13, M6581_CS },
    { "RW", 14, M6581_RW }
};

static const ui_chip_pin_t _ui_x65_cgia_pins[] = {
    { "D0",  0,  CGIA_D0  },
    { "D1",  1,  CGIA_D1  },
    { "D2",  2,  CGIA_D2  },
    { "D3",  3,  CGIA_D3  },
    { "D4",  4,  CGIA_D4  },
    { "D5",  5,  CGIA_D5  },
    { "D6",  6,  CGIA_D6  },
    { "D7",  7,  CGIA_D7  },
    { "INT", 9,  CGIA_INT },
    { "A0",  10, CGIA_A0  },
    { "A1",  11, CGIA_A1  },
    { "A2",  12, CGIA_A2  },
    { "A3",  13, CGIA_A3  },
    { "A4",  14, CGIA_A4  },
    { "A5",  15, CGIA_A5  },
    { "A6",  16, CGIA_A6  },
    { "CS",  18, CGIA_CS  },
    { "RW",  19, CGIA_RW  },
};

void ui_x65_init(ui_x65_t* ui, const ui_x65_desc_t* ui_desc) {
    CHIPS_ASSERT(ui && ui_desc);
    CHIPS_ASSERT(ui_desc->x65);
    CHIPS_ASSERT(ui_desc->boot_cb);
    ui->x65 = ui_desc->x65;
    ui->boot_cb = ui_desc->boot_cb;
    ui->inject = ui_desc->inject;
    ui_snapshot_init(&ui->snapshot, &ui_desc->snapshot);
    int x = 20, y = 20, dx = 10, dy = 10;
    {
        ui_dbg_desc_t desc = { 0 };
        desc.title = "CPU Debugger";
        desc.x = x;
        desc.y = y;
        desc.w65816 = &ui->x65->cpu;
        desc.freq_hz = X65_FREQUENCY;
        desc.scanline_ticks = ui->x65->cgia.h_period / CGIA_FIXEDPOINT_SCALE;
        desc.frame_ticks = MODE_V_TOTAL_LINES * ui->x65->cgia.h_period / CGIA_FIXEDPOINT_SCALE;
        desc.read_cb = _ui_x65_mem_read;
        desc.break_cb = _ui_x65_eval_bp;
        desc.texture_cbs = ui_desc->dbg_texture;
        desc.debug_cbs = ui_desc->dbg_debug;
        desc.keys = ui_desc->dbg_keys;
        desc.user_data = ui;
        desc.labels = ui_desc->labels;
        /* custom breakpoint types */
        desc.user_breaktypes[0].label = "Scanline at";
        desc.user_breaktypes[0].show_val16 = true;
        desc.user_breaktypes[1].label = "Next Scanline";
        desc.user_breaktypes[2].label = "Next Frame";
        ui_dbg_init(&ui->dbg, &desc);
    }
    x += dx;
    y += dy;
    {
        ui_w65816_desc_t desc = { 0 };
        desc.title = "WDC 65C816";
        desc.cpu = &ui->x65->cpu;
        desc.x = x;
        desc.y = y;
        UI_CHIP_INIT_DESC(&desc.chip_desc, "65C816", 48, _ui_x65_cpu65816_pins);
        ui_w65816_init(&ui->cpu, &desc);
    }
    x += dx;
    y += dy;
    {
        ui_ria816_desc_t desc = { 0 };
        desc.title = "RIA816";
        desc.ria = &ui->x65->ria;
        desc.x = x;
        desc.y = y;
        UI_CHIP_INIT_DESC(&desc.chip_desc, "RIA816", 20, _ui_x65_ria_pins);
        ui_ria816_init(&ui->ria, &desc);
    }
    x += dx;
    y += dy;
    {
        ui_console_desc_t desc = { 0 };
        desc.title = "UART Console";
        desc.rx = &ui->x65->ria.uart_rx;
        desc.tx = &ui->x65->ria.uart_tx;
        desc.x = x;
        desc.y = y;
        ui_console_init(&ui->ria_uart, &desc);
    }
    x += dx;
    y += dy;
    {
        ui_tca6416a_desc_t desc = { 0 };
        desc.title = "TCA6416A (GPIO)";
        desc.gpio = &ui->x65->gpio;
        desc.x = x;
        desc.y = y;
        UI_CHIP_INIT_DESC(&desc.chip_desc, "TCA6416A", 34, _ui_x65_gpio_pins);
        desc.chip_desc.chip_width = 80;
        ui_tca6416a_init(&ui->gpio, &desc);
    }
    x += dx;
    y += dy;
    {
        ui_m6581_desc_t desc = { 0 };
        desc.title = "MOS 6581 (SID)";
        desc.sid = &ui->x65->sid;
        desc.x = x;
        desc.y = y;
        UI_CHIP_INIT_DESC(&desc.chip_desc, "6581", 16, _ui_x65_sid_pins);
        ui_m6581_init(&ui->sid, &desc);
    }
    x += dx;
    y += dy;
    {
        ui_cgia_desc_t desc = { 0 };
        desc.title = "CGIA - Color Graphic Interface Adaptor";
        desc.cgia = &ui->x65->cgia;
        desc.x = x;
        desc.y = y;
        UI_CHIP_INIT_DESC(&desc.chip_desc, "CGIA", 20, _ui_x65_cgia_pins);
        ui_cgia_init(&ui->cgia, &desc);
    }
    x += dx;
    y += dy;
    {
        ui_audio_desc_t desc = { 0 };
        desc.title = "Audio Output";
        desc.sample_buffer = ui->x65->audio.sample_buffer;
        desc.num_samples = ui->x65->audio.num_samples;
        desc.x = x;
        desc.y = y;
        ui_audio_init(&ui->audio, &desc);
    }
    x += dx;
    y += dy;
    {
        ui_display_desc_t desc = { 0 };
        desc.title = "Display";
        desc.x = x;
        desc.y = y;
        desc.w = 320;
        desc.h = 200 + 20;
        ui_display_init(&ui->display, &desc);
    }
    x += dx;
    y += dy;
    {
        ui_memedit_desc_t desc = { 0 };
        for (int i = 0; i < _UI_X65_MEMLAYER_NUM; i++) {
            desc.layers[i] = _ui_x65_memlayer_names[i];
            desc.layer_banks[i] = _ui_x65_memlayer_banks[i];
        }
        desc.read_cb = _ui_x65_mem_read;
        desc.write_cb = _ui_x65_mem_write;
        desc.user_data = ui;
        static const char* titles[] = { "Memory Editor #1",
                                        "Memory Editor #2",
                                        "Memory Editor #3",
                                        "Memory Editor #4" };
        for (int i = 0; i < 4; i++) {
            desc.title = titles[i];
            desc.x = x;
            desc.y = y;
            ui_memedit_init(&ui->memedit[i], &desc);
            x += dx;
            y += dy;
        }
    }
    x += dx;
    y += dy;
    {
        ui_dasm_desc_t desc = { 0 };
        for (int i = 0; i < _UI_X65_CODELAYER_NUM; i++) {
            desc.layers[i] = _ui_x65_memlayer_names[i];
            desc.layer_banks[i] = _ui_x65_memlayer_banks[i];
        }
        desc.cpu_type = UI_DASM_CPUTYPE_W65C816S;
        desc.cpu = ui->cpu.cpu;
        desc.start_addr = mem_rd16(ui->x65, 0, 0xFFFC);
        desc.read_cb = _ui_x65_mem_read;
        desc.user_data = ui;
        desc.labels = ui_desc->labels;
        static const char* titles[4] = { "Disassembler #1", "Disassembler #2", "Disassembler #3", "Disassembler #4" };
        for (int i = 0; i < 4; i++) {
            desc.title = titles[i];
            desc.x = x;
            desc.y = y;
            ui_dasm_init(&ui->dasm[i], &desc);
            x += dx;
            y += dy;
        }
    }
    x += dx;
    y += dy;
    {
        ui_app_log_desc_t desc = { 0 };
        desc.title = "Log messages";
        desc.x = x;
        desc.y = y;
        ui_app_log_init(&ui->app_log, &desc);
    }
}

void ui_x65_discard(ui_x65_t* ui) {
    CHIPS_ASSERT(ui && ui->x65);
    ui_w65816_discard(&ui->cpu);
    ui_ria816_discard(&ui->ria);
    ui_tca6416a_discard(&ui->gpio);
    ui_m6581_discard(&ui->sid);
    ui_cgia_discard(&ui->cgia);
    ui_console_discard(&ui->ria_uart);
    ui_audio_discard(&ui->audio);
    ui_display_discard(&ui->display);
    for (int i = 0; i < 4; i++) {
        ui_memedit_discard(&ui->memedit[i]);
        ui_dasm_discard(&ui->dasm[i]);
    }
    ui_dbg_discard(&ui->dbg);
    ui_app_log_discard(&ui->app_log);
    ui->x65 = 0;
}

void ui_x65_draw(ui_x65_t* ui, const ui_x65_frame_t* frame) {
    CHIPS_ASSERT(ui && ui->x65 && frame);
    _ui_x65_draw_menu(ui);
    _ui_x65_draw_about(ui);
    ui_audio_draw(&ui->audio, ui->x65->audio.sample_pos);
    ui_display_draw(&ui->display, &frame->display);
    ui_w65816_draw(&ui->cpu);
    ui_ria816_draw(&ui->ria);
    ui_tca6416a_draw(&ui->gpio);
    ui_m6581_draw(&ui->sid);
    ui_cgia_draw(&ui->cgia);
    ui_console_draw(&ui->ria_uart);
    for (int i = 0; i < 4; i++) {
        ui_memedit_draw(&ui->memedit[i]);
        ui_dasm_draw(&ui->dasm[i]);
    }
    ui_dbg_draw(&ui->dbg);
    ui_app_log_draw(&ui->app_log);
}

chips_debug_t ui_x65_get_debug(ui_x65_t* ui) {
    CHIPS_ASSERT(ui);
    chips_debug_t res = {};
    res.callback.func = (chips_debug_func_t)ui_dbg_tick;
    res.callback.user_data = &ui->dbg;
    res.stopped = &ui->dbg.dbg.stopped;
    return res;
}

void ui_x65_save_settings(ui_x65_t* ui, ui_settings_t* settings) {
    CHIPS_ASSERT(ui && settings);
    ui_w65816_save_settings(&ui->cpu, settings);
    ui_ria816_save_settings(&ui->ria, settings);
    ui_tca6416a_save_settings(&ui->gpio, settings);
    ui_m6581_save_settings(&ui->sid, settings);
    ui_cgia_save_settings(&ui->cgia, settings);
    ui_console_save_settings(&ui->ria_uart, settings);
    ui_audio_save_settings(&ui->audio, settings);
    ui_display_save_settings(&ui->display, settings);
    for (int i = 0; i < 4; i++) {
        ui_memedit_save_settings(&ui->memedit[i], settings);
    }
    for (int i = 0; i < 4; i++) {
        ui_dasm_save_settings(&ui->dasm[i], settings);
    }
    ui_dbg_save_settings(&ui->dbg, settings);
    ui_app_log_save_settings(&ui->app_log, settings);
}

void ui_x65_load_settings(ui_x65_t* ui, const ui_settings_t* settings) {
    CHIPS_ASSERT(ui && settings);
    ui_w65816_load_settings(&ui->cpu, settings);
    ui_ria816_load_settings(&ui->ria, settings);
    ui_tca6416a_load_settings(&ui->gpio, settings);
    ui_m6581_load_settings(&ui->sid, settings);
    ui_cgia_load_settings(&ui->cgia, settings);
    ui_console_load_settings(&ui->ria_uart, settings);
    ui_audio_load_settings(&ui->audio, settings);
    ui_display_load_settings(&ui->display, settings);
    for (int i = 0; i < 4; i++) {
        ui_memedit_load_settings(&ui->memedit[i], settings);
    }
    for (int i = 0; i < 4; i++) {
        ui_dasm_load_settings(&ui->dasm[i], settings);
    }
    ui_dbg_load_settings(&ui->dbg, settings);
    ui_app_log_load_settings(&ui->app_log, settings);
}
#ifdef __clang__
    #pragma clang diagnostic pop
#endif
