#include "./ui_x65.h"

#include "imgui.h"
#include "args.h"

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
        if (ImGui::BeginMenu("System")) {
            if (ImGui::MenuItem(ui->x65->running ? "Running" : "Run", 0, ui->x65->running)) {
                x65_set_running(ui->x65, !ui->x65->running);
            }
            ui_snapshot_menus(&ui->snapshot);
            if (ImGui::MenuItem("Reset")) {
                x65_reset(ui->x65);
                ui_dbg_reset(&ui->dbg);
            }
            if (ImGui::MenuItem("Cold Boot")) {
                ui->boot_cb(ui->x65);
                ui_dbg_reboot(&ui->dbg);
            }
            if (ImGui::BeginMenu("Joystick")) {
                if (ImGui::MenuItem("None", 0, ui->x65->joystick_type == X65_JOYSTICKTYPE_NONE)) {
                    ui->x65->joystick_type = X65_JOYSTICKTYPE_NONE;
                }
                if (ImGui::MenuItem("Digital #1", 0, ui->x65->joystick_type == X65_JOYSTICKTYPE_DIGITAL_1)) {
                    ui->x65->joystick_type = X65_JOYSTICKTYPE_DIGITAL_1;
                }
                if (ImGui::MenuItem("Digital #2", 0, ui->x65->joystick_type == X65_JOYSTICKTYPE_DIGITAL_2)) {
                    ui->x65->joystick_type = X65_JOYSTICKTYPE_DIGITAL_2;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Hardware")) {
            ImGui::MenuItem("Memory Map", 0, &ui->memmap.open);
#ifndef USE_WEB
            ImGui::MenuItem("Keyboard Matrix", 0, &ui->kbd.open);
#endif
            ImGui::MenuItem("Audio Output", 0, &ui->audio.open);
            ImGui::MenuItem("Display", 0, &ui->display.open);
            ImGui::MenuItem("WDC 65C816 (CPU)", 0, &ui->cpu.open);
            ImGui::MenuItem("CGIA (VPU)", 0, &ui->cgia.open);
#ifndef USE_WEB
            ImGui::MenuItem("MOS 6526 #1 (CIA)", 0, &ui->cia[0].open);
            ImGui::MenuItem("MOS 6526 #2 (CIA)", 0, &ui->cia[1].open);
            ImGui::MenuItem("MOS 6581 (SID)", 0, &ui->sid.open);
#endif
            ImGui::MenuItem("YMF262 (OPL3)", 0, &ui->opl3.open);
            ImGui::MenuItem("RIA816", 0, &ui->ria.open);
            ImGui::MenuItem("RIA UART", 0, &ui->ria_uart.open);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("CPU Debugger", 0, &ui->dbg.ui.open);
            ImGui::MenuItem("Breakpoints", 0, &ui->dbg.ui.breakpoints.open);
            ImGui::MenuItem("Stopwatch", 0, &ui->dbg.ui.stopwatch.open);
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
            ui_util_options_menu();
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
#endif
        ImGui::Separator();
        ImGui::Text("ROM = %s", arguments.rom);
        ImGui::Text("OUTPUT_FILE = %s", arguments.output_file);
        ImGui::Text("VERBOSE = %s", arguments.verbose ? "yes" : "no");
        ImGui::Text("SILENT = %s", arguments.silent ? "yes" : "no");

        if (copy_to_clipboard) {
            ImGui::LogText("\n```\n");
            ImGui::LogFinish();
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

/* keep disassembler layer at the start */
#define _UI_X65_MEMLAYER_CPU   (0) /* CPU visible mapping */
#define _UI_X65_MEMLAYER_RAM   (1) /* RAM blocks */
#define _UI_X65_MEMLAYER_VRAM0 (2) /* CGIA VRAM bank 0 */
#define _UI_X65_MEMLAYER_VRAM1 (3) /* CGIA VRAM bank 1 */
#define _UI_X65_CODELAYER_NUM  (4) /* number of valid layers for disassembler */
#define _UI_X65_MEMLAYER_NUM   (4)

static const char* _ui_x65_memlayer_names[_UI_X65_MEMLAYER_NUM] = {
    "CPU Mapped",
    "RAM Banks",
    "VRAM0",
    "VRAM1",
};

static uint8_t _ui_x65_mem_read(int layer, uint16_t addr, void* user_data) {
    CHIPS_ASSERT(user_data);
    ui_x65_t* ui = (ui_x65_t*)user_data;
    x65_t* x65 = ui->x65;
    switch (layer) {
        case _UI_X65_MEMLAYER_CPU: return mem_rd(&x65->mem, addr);
        case _UI_X65_MEMLAYER_RAM: return x65->ram[addr];
        case _UI_X65_MEMLAYER_VRAM0: return x65->cgia.vram[0][addr];
        case _UI_X65_MEMLAYER_VRAM1: return x65->cgia.vram[1][addr];
        default: return 0xFF;
    }
}

static void _ui_x65_mem_write(int layer, uint16_t addr, uint8_t data, void* user_data) {
    CHIPS_ASSERT(user_data);
    ui_x65_t* ui = (ui_x65_t*)user_data;
    x65_t* x65 = ui->x65;
    switch (layer) {
        case _UI_X65_MEMLAYER_CPU: mem_wr(&x65->mem, addr, data); break;
        case _UI_X65_MEMLAYER_RAM: x65->ram[addr] = data; break;
        case _UI_X65_MEMLAYER_VRAM0: x65->cgia.vram[0][addr] = data; break;
        case _UI_X65_MEMLAYER_VRAM1: x65->cgia.vram[1][addr] = data; break;
    }
}

static void _ui_x65_update_memmap(ui_x65_t* ui) {
    CHIPS_ASSERT(ui && ui->x65);
    const x65_t* x65 = ui->x65;
    ui_memmap_reset(&ui->memmap);
    ui_memmap_layer(&ui->memmap, "IO");
    ui_memmap_region(&ui->memmap, "DEVICES", X65_IO_BASE, 0x10000 - X65_IO_BASE, true);
    ui_memmap_region(&ui->memmap, "EXTENSION", X65_EXT_BASE, X65_EXT_LEN, x65->ria.reg[RIA816_EXT_IO]);
    ui_memmap_layer(&ui->memmap, "RAM");
    ui_memmap_region(&ui->memmap, "RAM", 0x0000, 0x10000, true);
}

static int _ui_x65_eval_bp(ui_dbg_t* dbg_win, int trap_id, uint64_t pins, void* user_data) {
    (void)pins;
    CHIPS_ASSERT(user_data);
    ui_x65_t* ui = (ui_x65_t*)user_data;
    x65_t* x65 = ui->x65;
    int scanline = x65->cgia.active_line;
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
                /* next badline */
                case UI_DBG_BREAKTYPE_USER + 2:
                    if ((ui->dbg_scanline != scanline) && x65->cgia.badline) {
                        trap_id = UI_DBG_BP_BASE_TRAPID + i;
                    }
                    break;
                /* next frame */
                case UI_DBG_BREAKTYPE_USER + 3:
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
    { "D0",   0,  W65816_D0   },
    { "D1",   1,  W65816_D1   },
    { "D2",   2,  W65816_D2   },
    { "D3",   3,  W65816_D3   },
    { "D4",   4,  W65816_D4   },
    { "D5",   5,  W65816_D5   },
    { "D6",   6,  W65816_D6   },
    { "D7",   7,  W65816_D7   },
    { "RW",   9,  W65816_RW   },
    { "SYNC", 10, W65816_SYNC },
    { "RDY",  11, W65816_RDY  },
    { "IRQ",  12, W65816_IRQ  },
    { "NMI",  13, W65816_NMI  },
    { "RES",  14, W65816_RES  },
    { "A0",   16, W65816_A0   },
    { "A1",   17, W65816_A1   },
    { "A2",   18, W65816_A2   },
    { "A3",   19, W65816_A3   },
    { "A4",   20, W65816_A4   },
    { "A5",   21, W65816_A5   },
    { "A6",   22, W65816_A6   },
    { "A7",   23, W65816_A7   },
    { "A8",   24, W65816_A8   },
    { "A9",   25, W65816_A9   },
    { "A10",  26, W65816_A10  },
    { "A11",  27, W65816_A11  },
    { "A12",  28, W65816_A12  },
    { "A13",  29, W65816_A13  },
    { "A14",  30, W65816_A14  },
    { "A15",  31, W65816_A15  },
};

static const ui_chip_pin_t _ui_x65_cia_pins[] = {
    { "D0",   0,  M6526_D0   },
    { "D1",   1,  M6526_D1   },
    { "D2",   2,  M6526_D2   },
    { "D3",   3,  M6526_D3   },
    { "D4",   4,  M6526_D4   },
    { "D5",   5,  M6526_D5   },
    { "D6",   6,  M6526_D6   },
    { "D7",   7,  M6526_D7   },
    { "RS0",  9,  M6526_RS0  },
    { "RS1",  10, M6526_RS1  },
    { "RS2",  11, M6526_RS2  },
    { "RS3",  12, M6526_RS3  },
    { "RW",   14, M6526_RW   },
    { "CS",   15, M6526_CS   },
    { "PC",   16, M6526_PC   },
    { "TOD",  17, M6526_TOD  },
    { "IRQ",  18, M6526_IRQ  },
    { "FLAG", 19, M6526_FLAG },
    { "PA0",  20, M6526_PA0  },
    { "PA1",  21, M6526_PA1  },
    { "PA2",  22, M6526_PA2  },
    { "PA3",  23, M6526_PA3  },
    { "PA4",  24, M6526_PA4  },
    { "PA5",  25, M6526_PA5  },
    { "PA6",  26, M6526_PA6  },
    { "PA7",  27, M6526_PA7  },
    { "PB0",  29, M6526_PB0  },
    { "PB1",  30, M6526_PB1  },
    { "PB2",  31, M6526_PB2  },
    { "PB3",  32, M6526_PB3  },
    { "PB4",  33, M6526_PB4  },
    { "PB5",  34, M6526_PB5  },
    { "PB6",  35, M6526_PB6  },
    { "PB7",  36, M6526_PB7  },
    { "SP",   38, M6526_SP   },
    { "CNT",  39, M6526_CNT  }
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
    { "RS0", 10, RIA816_RS0 },
    { "RS1", 11, RIA816_RS1 },
    { "RS2", 12, RIA816_RS2 },
    { "RS3", 13, RIA816_RS3 },
    { "RS4", 14, RIA816_RS4 },
    { "RS5", 15, RIA816_RS5 },
    { "RW",  17, RIA816_RW  },
    { "CS",  18, RIA816_CS  },
    { "IRQ", 19, RIA816_IRQ }
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

static const ui_chip_pin_t _ui_x65_ymf262_pins[] = {
    { "D0",  0,  YMF262_D0  },
    { "D1",  1,  YMF262_D1  },
    { "D2",  2,  YMF262_D2  },
    { "D3",  3,  YMF262_D3  },
    { "D4",  4,  YMF262_D4  },
    { "D5",  5,  YMF262_D5  },
    { "D6",  6,  YMF262_D6  },
    { "D7",  7,  YMF262_D7  },
    { "A0",  8,  YMF262_A0  },
    { "A1",  9,  YMF262_A1  },
    { "IC",  11, YMF262_IC  },
    { "RW",  12, YMF262_RW  },
    { "CS",  13, YMF262_CS  },
    { "IRQ", 15, YMF262_IRQ },
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
    ui_snapshot_init(&ui->snapshot, &ui_desc->snapshot);
    ui->show_about = false;
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
        /* custom breakpoint types */
        desc.user_breaktypes[0].label = "Scanline at";
        desc.user_breaktypes[0].show_val16 = true;
        desc.user_breaktypes[1].label = "Next Scanline";
        desc.user_breaktypes[2].label = "Next Badline";
        desc.user_breaktypes[3].label = "Next Frame";
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
        desc.h = 390;
        UI_CHIP_INIT_DESC(&desc.chip_desc, "65816", 32, _ui_x65_cpu65816_pins);
        ui_w65816_init(&ui->cpu, &desc);
    }
    x += dx;
    y += dy;
    {
        ui_m6526_desc_t desc = { 0 };
        desc.title = "MOS 6526 #1 (CIA)";
        desc.cia = &ui->x65->cia_1;
        desc.x = x;
        desc.y = y;
        UI_CHIP_INIT_DESC(&desc.chip_desc, "6526", 40, _ui_x65_cia_pins);
        ui_m6526_init(&ui->cia[0], &desc);
        x += dx;
        y += dy;
        desc.title = "MOS 6526 #2 (CIA)";
        desc.cia = &ui->x65->cia_2;
        desc.x = x;
        desc.y = y;
        ui_m6526_init(&ui->cia[1], &desc);
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
        ui_ymf262_desc_t desc = { 0 };
        desc.title = "YMF262 (OPL3)";
        desc.opl3 = &ui->x65->opl3;
        desc.x = x;
        desc.y = y;
        UI_CHIP_INIT_DESC(&desc.chip_desc, "YMF262", 16, _ui_x65_ymf262_pins);
        ui_ymf262_init(&ui->opl3, &desc);
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
        desc.title = "RIA UART";
        desc.rx = &ui->x65->ria.uart_rx;
        desc.tx = &ui->x65->ria.uart_tx;
        desc.x = x;
        desc.y = y;
        ui_console_init(&ui->ria_uart, &desc);
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
        ui_kbd_desc_t desc = { 0 };
        desc.title = "Keyboard Matrix";
        desc.kbd = &ui->x65->kbd;
        desc.layers[0] = "None";
        desc.layers[1] = "Shift";
        desc.layers[2] = "Ctrl";
        desc.x = x;
        desc.y = y;
        ui_kbd_init(&ui->kbd, &desc);
    }
    x += dx;
    y += dy;
    {
        ui_memedit_desc_t desc = { 0 };
        for (int i = 0; i < _UI_X65_MEMLAYER_NUM; i++) {
            desc.layers[i] = _ui_x65_memlayer_names[i];
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
        ui_memmap_desc_t desc = { 0 };
        desc.title = "Memory Map";
        desc.x = x;
        desc.y = y;
        ui_memmap_init(&ui->memmap, &desc);
    }
    x += dx;
    y += dy;
    {
        ui_dasm_desc_t desc = { 0 };
        for (int i = 0; i < _UI_X65_CODELAYER_NUM; i++) {
            desc.layers[i] = _ui_x65_memlayer_names[i];
        }
        desc.cpu_type = UI_DASM_CPUTYPE_W65C816S;
        desc.start_addr = mem_rd16(&ui->x65->mem, 0xFFFC);
        desc.read_cb = _ui_x65_mem_read;
        desc.user_data = ui;
        static const char* titles[4] = { "Disassembler #1", "Disassembler #2", "Disassembler #2", "Dissassembler #3" };
        for (int i = 0; i < 4; i++) {
            desc.title = titles[i];
            desc.x = x;
            desc.y = y;
            ui_dasm_init(&ui->dasm[i], &desc);
            x += dx;
            y += dy;
        }
    }
}

void ui_x65_discard(ui_x65_t* ui) {
    CHIPS_ASSERT(ui && ui->x65);
    ui_w65816_discard(&ui->cpu);
    ui_m6526_discard(&ui->cia[0]);
    ui_m6526_discard(&ui->cia[1]);
    ui_ria816_discard(&ui->ria);
    ui_m6581_discard(&ui->sid);
    ui_ymf262_discard(&ui->opl3);
    ui_cgia_discard(&ui->cgia);
    ui_console_discard(&ui->ria_uart);
    ui_kbd_discard(&ui->kbd);
    ui_audio_discard(&ui->audio);
    ui_display_discard(&ui->display);
    ui_memmap_discard(&ui->memmap);
    for (int i = 0; i < 4; i++) {
        ui_memedit_discard(&ui->memedit[i]);
        ui_dasm_discard(&ui->dasm[i]);
    }
    ui_dbg_discard(&ui->dbg);
    ui->x65 = 0;
}

void ui_x65_draw(ui_x65_t* ui, const ui_x65_frame_t* frame) {
    CHIPS_ASSERT(ui && ui->x65 && frame);
    _ui_x65_draw_menu(ui);
    _ui_x65_draw_about(ui);
    if (ui->memmap.open) {
        _ui_x65_update_memmap(ui);
    }
    ui_audio_draw(&ui->audio, ui->x65->audio.sample_pos);
    ui_display_draw(&ui->display, &frame->display);
    ui_kbd_draw(&ui->kbd);
    ui_w65816_draw(&ui->cpu);
    ui_m6526_draw(&ui->cia[0]);
    ui_m6526_draw(&ui->cia[1]);
    ui_ria816_draw(&ui->ria);
    ui_m6581_draw(&ui->sid);
    ui_ymf262_draw(&ui->opl3);
    ui_cgia_draw(&ui->cgia);
    ui_console_draw(&ui->ria_uart);
    ui_memmap_draw(&ui->memmap);
    for (int i = 0; i < 4; i++) {
        ui_memedit_draw(&ui->memedit[i]);
        ui_dasm_draw(&ui->dasm[i]);
    }
    ui_dbg_draw(&ui->dbg);
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
    for (int i = 0; i < 2; i++) {
        ui_m6526_save_settings(&ui->cia[i], settings);
    }
    ui_ria816_save_settings(&ui->ria, settings);
    ui_m6581_save_settings(&ui->sid, settings);
    ui_ymf262_save_settings(&ui->opl3, settings);
    ui_cgia_save_settings(&ui->cgia, settings);
    ui_console_save_settings(&ui->ria_uart, settings);
    ui_audio_save_settings(&ui->audio, settings);
    ui_display_save_settings(&ui->display, settings);
    ui_kbd_save_settings(&ui->kbd, settings);
    ui_memmap_save_settings(&ui->memmap, settings);
    for (int i = 0; i < 4; i++) {
        ui_memedit_save_settings(&ui->memedit[i], settings);
    }
    for (int i = 0; i < 4; i++) {
        ui_dasm_save_settings(&ui->dasm[i], settings);
    }
    ui_dbg_save_settings(&ui->dbg, settings);
}

void ui_x65_load_settings(ui_x65_t* ui, const ui_settings_t* settings) {
    CHIPS_ASSERT(ui && settings);
    ui_w65816_load_settings(&ui->cpu, settings);
    for (int i = 0; i < 2; i++) {
        ui_m6526_load_settings(&ui->cia[i], settings);
    }
    ui_ria816_load_settings(&ui->ria, settings);
    ui_m6581_load_settings(&ui->sid, settings);
    ui_ymf262_load_settings(&ui->opl3, settings);
    ui_cgia_load_settings(&ui->cgia, settings);
    ui_console_load_settings(&ui->ria_uart, settings);
    ui_audio_load_settings(&ui->audio, settings);
    ui_display_load_settings(&ui->display, settings);
    ui_kbd_load_settings(&ui->kbd, settings);
    ui_memmap_load_settings(&ui->memmap, settings);
    for (int i = 0; i < 4; i++) {
        ui_memedit_load_settings(&ui->memedit[i], settings);
    }
    for (int i = 0; i < 4; i++) {
        ui_dasm_load_settings(&ui->dasm[i], settings);
    }
    ui_dbg_load_settings(&ui->dbg, settings);
}
#ifdef __clang__
    #pragma clang diagnostic pop
#endif
