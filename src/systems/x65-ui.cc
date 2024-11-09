/*
    UI implementaion for x65.c
*/
#define UI_DASM_USE_M6502
#define UI_DBG_USE_M6502
#define CHIPS_UTIL_IMPL
#include "util/m6502dasm.h"
#define CHIPS_UI_IMPL

#include "imgui.h"
#include "ui/ui_util.h"
#include "./ui_x65.h"

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
            ImGui::MenuItem("Keyboard Matrix", 0, &ui->kbd.open);
            ImGui::MenuItem("Audio Output", 0, &ui->audio.open);
            ImGui::MenuItem("MOS 6510 (CPU)", 0, &ui->cpu.open);
            // ImGui::MenuItem("MCP23017 (GPIO)", 0, &ui->gpio.open);
            // ImGui::MenuItem("Yamaha YMF825 (SD-1)", 0, &ui->sd1.open);
            // ImGui::MenuItem("CGIA", 0, &ui->cgia.open);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("CPU Debugger", 0, &ui->dbg.ui.open);
            ImGui::MenuItem("Breakpoints", 0, &ui->dbg.ui.show_breakpoints);
            ImGui::MenuItem("Stopwatch", 0, &ui->dbg.ui.show_stopwatch);
            ImGui::MenuItem("Execution History", 0, &ui->dbg.ui.show_history);
            ImGui::MenuItem("Memory Heatmap", 0, &ui->dbg.ui.show_heatmap);
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
        ui_util_options_menu();
        ImGui::EndMainMenuBar();
    }
}

/* keep disassembler layer at the start */
#define _UI_X65_MEMLAYER_CPU  (0) /* CPU visible mapping */
#define _UI_X65_CODELAYER_NUM (4) /* number of valid layers for disassembler */
#define _UI_X65_MEMLAYER_NUM  (1)

static const char* _ui_x65_memlayer_names[_UI_X65_MEMLAYER_NUM] = {
    "CPU Mapped",
};

static uint8_t _ui_x65_mem_read(int layer, uint16_t addr, void* user_data) {
    CHIPS_ASSERT(user_data);
    ui_x65_t* ui = (ui_x65_t*)user_data;
    x65_t* x65 = ui->x65;
    switch (layer) {
        case _UI_X65_MEMLAYER_CPU: return x65->ram[addr & 0xFFFFFF];
        default: return 0xFF;
    }
}

static void _ui_x65_mem_write(int layer, uint16_t addr, uint8_t data, void* user_data) {
    CHIPS_ASSERT(user_data);
    ui_x65_t* ui = (ui_x65_t*)user_data;
    x65_t* x65 = ui->x65;
    switch (layer) {
        case _UI_X65_MEMLAYER_CPU: x65->ram[addr & 0xFFFFFF] = data; break;
    }
}

static int _ui_x65_eval_bp(ui_dbg_t* dbg_win, int trap_id, uint64_t pins, void* user_data) {
    (void)pins;
    CHIPS_ASSERT(user_data);
    ui_x65_t* ui = (ui_x65_t*)user_data;
    x65_t* x65 = ui->x65;
    int scanline = 0;  // FIXME: x65->vic.rs.v_count;
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
                    if ((ui->dbg_scanline != scanline) /* FIXME: && x65->vic.rs.badline */) {
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

static const ui_chip_pin_t _ui_x65_cpu6502_pins[] = {
    { "D0",   0,  M6502_D0   },
    { "D1",   1,  M6502_D1   },
    { "D2",   2,  M6502_D2   },
    { "D3",   3,  M6502_D3   },
    { "D4",   4,  M6502_D4   },
    { "D5",   5,  M6502_D5   },
    { "D6",   6,  M6502_D6   },
    { "D7",   7,  M6502_D7   },
    { "RW",   9,  M6502_RW   },
    { "SYNC", 10, M6502_SYNC },
    { "RDY",  11, M6502_RDY  },
    { "IRQ",  12, M6502_IRQ  },
    { "NMI",  13, M6502_NMI  },
    { "RES",  14, M6502_RES  },
    { "A0",   16, M6502_A0   },
    { "A1",   17, M6502_A1   },
    { "A2",   18, M6502_A2   },
    { "A3",   19, M6502_A3   },
    { "A4",   20, M6502_A4   },
    { "A5",   21, M6502_A5   },
    { "A6",   22, M6502_A6   },
    { "A7",   23, M6502_A7   },
    { "A8",   24, M6502_A8   },
    { "A9",   25, M6502_A9   },
    { "A10",  26, M6502_A10  },
    { "A11",  27, M6502_A11  },
    { "A12",  28, M6502_A12  },
    { "A13",  29, M6502_A13  },
    { "A14",  30, M6502_A14  },
    { "A15",  31, M6502_A15  },
};

static const ui_chip_pin_t _ui_x65_sd1_pins[] = {
    { "D0", 0,  YMF825_D0 },
    { "D1", 1,  YMF825_D1 },
    { "D2", 2,  YMF825_D2 },
    { "D3", 3,  YMF825_D3 },
    { "D4", 4,  YMF825_D4 },
    { "D5", 5,  YMF825_D5 },
    { "D6", 6,  YMF825_D6 },
    { "D7", 7,  YMF825_D7 },
    { "A0", 8,  YMF825_A0 },
    { "A1", 9,  YMF825_A1 },
    { "A2", 10, YMF825_A2 },
    { "A3", 11, YMF825_A3 },
    { "CS", 13, YMF825_CS },
    { "RW", 14, YMF825_RW }
};

static const ui_chip_pin_t _ui_x65_cgia_pins[] = {
    { "DB0", 0,  CGIA_D0  },
    { "DB1", 1,  CGIA_D1  },
    { "DB2", 2,  CGIA_D2  },
    { "DB3", 3,  CGIA_D3  },
    { "DB4", 4,  CGIA_D4  },
    { "DB5", 5,  CGIA_D5  },
    { "DB6", 6,  CGIA_D6  },
    { "DB7", 7,  CGIA_D7  },
    { "CS",  9,  CGIA_CS  },
    { "RW",  10, CGIA_RW  },
    { "IRQ", 11, CGIA_IRQ },
    // { "BA",  12, CGIA_BA  },
    // { "AEC", 13, CGIA_AEC },
    { "A0",  14, CGIA_A0  },
    { "A1",  15, CGIA_A1  },
    { "A2",  16, CGIA_A2  },
    { "A3",  17, CGIA_A3  },
    { "A4",  18, CGIA_A4  },
    // { "A5",  19, CGIA_A5  },
    // { "A6",  20, CGIA_A6  },
    // { "A7",  21, CGIA_A7  },
    // { "A8",  22, CGIA_A8  },
    // { "A9",  23, CGIA_A9  },
    // { "A10", 24, CGIA_A10 },
    // { "A11", 25, CGIA_A11 },
    // { "A12", 26, CGIA_A12 },
    // { "A13", 27, CGIA_A13 }
};

void ui_x65_init(ui_x65_t* ui, const ui_x65_desc_t* ui_desc) {
    CHIPS_ASSERT(ui && ui_desc);
    CHIPS_ASSERT(ui_desc->x65);
    CHIPS_ASSERT(ui_desc->boot_cb);
    ui->x65 = ui_desc->x65;
    ui->boot_cb = ui_desc->boot_cb;
    ui_snapshot_init(&ui->snapshot, &ui_desc->snapshot);
    int x = 20, y = 20, dx = 10, dy = 10;
    {
        ui_dbg_desc_t desc = { 0 };
        desc.title = "CPU Debugger";
        desc.x = x;
        desc.y = y;
        desc.m6502 = &ui->x65->cpu;
        desc.freq_hz = X65_FREQUENCY;
        // desc.scanline_ticks = CGIA_HTOTAL;
        // desc.frame_ticks = CGIA_HTOTAL * CGIA_VTOTAL;
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
        ui_m6502_desc_t desc = { 0 };
        desc.title = "MOS 6502";
        desc.cpu = &ui->x65->cpu;
        desc.x = x;
        desc.y = y;
        desc.h = 390;
        UI_CHIP_INIT_DESC(&desc.chip_desc, "6510", 40, _ui_x65_cpu6502_pins);
        ui_m6502_init(&ui->cpu, &desc);
    }
    x += dx;
    y += dy;
    // {
    //     ui_ymf825_desc_t desc = { 0 };
    //     desc.title = "Yamaha YMF825 (SD1)";
    //     desc.sd1 = &ui->x65->sd1;
    //     desc.x = x;
    //     desc.y = y;
    //     UI_CHIP_INIT_DESC(&desc.chip_desc, "YMF825", 16, _ui_x65_sd1_pins);
    //     ui_ymf825_init(&ui->sd1, &desc);
    // }
    // x += dx;
    // y += dy;
    // {
    //     ui_cgia_desc_t desc = { 0 };
    //     desc.title = "CGIA";
    //     desc.cgia = &ui->x65->cgia;
    //     desc.x = x;
    //     desc.y = y;
    //     UI_CHIP_INIT_DESC(&desc.chip_desc, "CGIA", 28, _ui_x65_cgia_pins);
    //     ui_cgia_init(&ui->vic, &desc);
    // }
    // x += dx;
    // y += dy;
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
        desc.cpu_type = UI_DASM_CPUTYPE_M6502;
        desc.start_addr = ui->x65->ram[0xFFFC] | (uint16_t)(ui->x65->ram[0xFFFD] << 8);
        ;
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
    ui_m6502_discard(&ui->cpu);
    // ui_ymf825_discard(&ui->sd1);
    // ui_cgia_discard(&ui->cgia);
    ui_kbd_discard(&ui->kbd);
    ui_audio_discard(&ui->audio);
    ui_memmap_discard(&ui->memmap);
    for (int i = 0; i < 4; i++) {
        ui_memedit_discard(&ui->memedit[i]);
        ui_dasm_discard(&ui->dasm[i]);
    }
    ui_dbg_discard(&ui->dbg);
    ui->x65 = 0;
}

void ui_x65_draw(ui_x65_t* ui) {
    CHIPS_ASSERT(ui && ui->x65);
    _ui_x65_draw_menu(ui);
    if (ui->memmap.open) {
        // _ui_x65_update_memmap(ui);
    }
    ui_audio_draw(&ui->audio, ui->x65->audio.sample_pos);
    ui_kbd_draw(&ui->kbd);
    ui_m6502_draw(&ui->cpu);
    // ui_ymf825_draw(&ui->sd1);
    // ui_cgia_draw(&ui->cgia);
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

#ifdef __clang__
    #pragma clang diagnostic pop
#endif
