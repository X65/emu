#define UI_DBG_USE_M6502
#include "./ui_x65.h"

#include "imgui.h"

#include "ui/ui_chip.h"
#include "ui/ui_util.h"

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
            ImGui::MenuItem("MOS 6526 #1 (CIA)", 0, &ui->cia[0].open);
            ImGui::MenuItem("MOS 6526 #2 (CIA)", 0, &ui->cia[1].open);
            ImGui::MenuItem("MOS 6581 (SID)", 0, &ui->sid.open);
            ImGui::MenuItem("MOS 6569 (VIC-II)", 0, &ui->vic.open);
            ImGui::MenuItem("RIA UART", 0, &ui->ria_uart.open);
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
#define _UI_X65_MEMLAYER_CPU (0) /* CPU visible mapping */
#define _UI_X65_MEMLAYER_RAM (1) /* RAM blocks */
#define _UI_X65_MEMLAYER_ROM (2) /* ROM blocks */
// #define _UI_X65_MEMLAYER_1541   (3)     /* optional 1541 floppy drive */
#define _UI_X65_MEMLAYER_VIC   (4) /* VIC visible mapping */
#define _UI_X65_MEMLAYER_COLOR (5) /* special static color RAM */
#define _UI_X65_CODELAYER_NUM  (4) /* number of valid layers for disassembler */
#define _UI_X65_MEMLAYER_NUM   (6)

static const char* _ui_x65_memlayer_names[_UI_X65_MEMLAYER_NUM] = { "CPU Mapped",  "RAM Banks",  "ROM Banks",
                                                                    "1541 Floppy", "VIC Mapped", "Color RAM" };

static uint8_t _ui_x65_mem_read(int layer, uint16_t addr, void* user_data) {
    CHIPS_ASSERT(user_data);
    ui_x65_t* ui = (ui_x65_t*)user_data;
    x65_t* x65 = ui->x65;
    switch (layer) {
        case _UI_X65_MEMLAYER_CPU: return mem_rd(&x65->mem_cpu, addr);
        case _UI_X65_MEMLAYER_RAM: return x65->ram[addr];
        case _UI_X65_MEMLAYER_ROM:
            if ((addr >= 0xA000) && (addr < 0xC000)) {
                /* BASIC ROM */
                return x65->rom_basic[addr - 0xA000];
            }
            else if ((addr >= 0xD000) && (addr < 0xE000)) {
                /* Character ROM */
                return x65->rom_char[addr - 0xD000];
            }
            else if (addr >= 0xE000) {
                /* Kernal ROM */
                return x65->rom_kernal[addr - 0xE000];
            }
            else {
                return 0xFF;
            }
            break;
        case _UI_X65_MEMLAYER_VIC: return mem_rd(&x65->mem_vic, addr);
        case _UI_X65_MEMLAYER_COLOR:
            if ((addr >= 0xD800) && (addr < 0xDC00)) {
                /* static COLOR RAM */
                return x65->color_ram[addr - 0xD800];
            }
            else {
                return 0xFF;
            }
        default: return 0xFF;
    }
}

static void _ui_x65_mem_write(int layer, uint16_t addr, uint8_t data, void* user_data) {
    CHIPS_ASSERT(user_data);
    ui_x65_t* ui = (ui_x65_t*)user_data;
    x65_t* x65 = ui->x65;
    switch (layer) {
        case _UI_X65_MEMLAYER_CPU: mem_wr(&x65->mem_cpu, addr, data); break;
        case _UI_X65_MEMLAYER_RAM: x65->ram[addr] = data; break;
        case _UI_X65_MEMLAYER_ROM:
            if ((addr >= 0xA000) && (addr < 0xC000)) {
                /* BASIC ROM */
                x65->rom_basic[addr - 0xA000] = data;
            }
            else if ((addr >= 0xD000) && (addr < 0xE000)) {
                /* Character ROM */
                x65->rom_char[addr - 0xD000] = data;
            }
            else if (addr >= 0xE000) {
                /* Kernal ROM */
                x65->rom_kernal[addr - 0xE000] = data;
            }
            break;
        case _UI_X65_MEMLAYER_VIC: mem_wr(&x65->mem_vic, addr, data); break;
        case _UI_X65_MEMLAYER_COLOR:
            if ((addr >= 0xD800) && (addr < 0xDC00)) {
                /* static COLOR RAM */
                x65->color_ram[addr - 0xD800] = data;
            }
            break;
    }
}

static void _ui_x65_update_memmap(ui_x65_t* ui) {
    CHIPS_ASSERT(ui && ui->x65);
    const x65_t* x65 = ui->x65;
    bool all_ram = (x65->cpu_port & (X65_CPUPORT_HIRAM | X65_CPUPORT_LORAM)) == 0;
    bool basic_rom =
        (x65->cpu_port & (X65_CPUPORT_HIRAM | X65_CPUPORT_LORAM)) == (X65_CPUPORT_HIRAM | X65_CPUPORT_LORAM);
    bool kernal_rom = (x65->cpu_port & X65_CPUPORT_HIRAM) != 0;
    bool io_enabled = !all_ram && ((x65->cpu_port & X65_CPUPORT_CHAREN) != 0);
    bool char_rom = !all_ram && ((x65->cpu_port & X65_CPUPORT_CHAREN) == 0);
    ui_memmap_reset(&ui->memmap);
    ui_memmap_layer(&ui->memmap, "IO");
    ui_memmap_region(&ui->memmap, "IO REGION", 0xD000, 0x1000, io_enabled);
    ui_memmap_layer(&ui->memmap, "ROM");
    ui_memmap_region(&ui->memmap, "BASIC ROM", 0xA000, 0x2000, basic_rom);
    ui_memmap_region(&ui->memmap, "CHAR ROM", 0xD000, 0x1000, char_rom);
    ui_memmap_region(&ui->memmap, "KERNAL ROM", 0xE000, 0x2000, kernal_rom);
    ui_memmap_layer(&ui->memmap, "RAM");
    ui_memmap_region(&ui->memmap, "RAM", 0x0000, 0x10000, true);
}

static int _ui_x65_eval_bp(ui_dbg_t* dbg_win, int trap_id, uint64_t pins, void* user_data) {
    (void)pins;
    CHIPS_ASSERT(user_data);
    ui_x65_t* ui = (ui_x65_t*)user_data;
    x65_t* x65 = ui->x65;
    int scanline = x65->vic.rs.v_count;
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
                    if ((ui->dbg_scanline != scanline) && x65->vic.rs.badline) {
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

static const ui_chip_pin_t _ui_x65_cpu6510_pins[] = {
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
    { "AEC",  12, M6510_AEC  },
    { "IRQ",  13, M6502_IRQ  },
    { "NMI",  14, M6502_NMI  },
    { "RES",  15, M6502_RES  },
    { "P0",   17, M6510_P0   },
    { "P1",   18, M6510_P1   },
    { "P2",   19, M6510_P2   },
    { "A0",   20, M6502_A0   },
    { "A1",   21, M6502_A1   },
    { "A2",   22, M6502_A2   },
    { "A3",   23, M6502_A3   },
    { "A4",   24, M6502_A4   },
    { "A5",   25, M6502_A5   },
    { "A6",   26, M6502_A6   },
    { "A7",   27, M6502_A7   },
    { "A8",   28, M6502_A8   },
    { "A9",   29, M6502_A9   },
    { "A10",  30, M6502_A10  },
    { "A11",  31, M6502_A11  },
    { "A12",  32, M6502_A12  },
    { "A13",  33, M6502_A13  },
    { "A14",  34, M6502_A14  },
    { "A15",  35, M6502_A15  },
    { "P3",   37, M6510_P3   },
    { "P4",   38, M6510_P4   },
    { "P5",   39, M6510_P5   },
};

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

static const ui_chip_pin_t _ui_x65_vic_pins[] = {
    { "DB0", 0,  M6569_D0  },
    { "DB1", 1,  M6569_D1  },
    { "DB2", 2,  M6569_D2  },
    { "DB3", 3,  M6569_D3  },
    { "DB4", 4,  M6569_D4  },
    { "DB5", 5,  M6569_D5  },
    { "DB6", 6,  M6569_D6  },
    { "DB7", 7,  M6569_D7  },
    { "CS",  9,  M6569_CS  },
    { "RW",  10, M6569_RW  },
    { "IRQ", 11, M6569_IRQ },
    { "BA",  12, M6569_BA  },
    { "AEC", 13, M6569_AEC },
    { "A0",  14, M6569_A0  },
    { "A1",  15, M6569_A1  },
    { "A2",  16, M6569_A2  },
    { "A3",  17, M6569_A3  },
    { "A4",  18, M6569_A4  },
    { "A5",  19, M6569_A5  },
    { "A6",  20, M6569_A6  },
    { "A7",  21, M6569_A7  },
    { "A8",  22, M6569_A8  },
    { "A9",  23, M6569_A9  },
    { "A10", 24, M6569_A10 },
    { "A11", 25, M6569_A11 },
    { "A12", 26, M6569_A12 },
    { "A13", 27, M6569_A13 }
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
        desc.scanline_ticks = M6569_HTOTAL;
        desc.frame_ticks = M6569_HTOTAL * M6569_VTOTAL;
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
        desc.title = "MOS 6510";
        desc.cpu = &ui->x65->cpu;
        desc.x = x;
        desc.y = y;
        desc.h = 390;
        UI_CHIP_INIT_DESC(&desc.chip_desc, "6510", 40, _ui_x65_cpu6510_pins);
        ui_m6502_init(&ui->cpu, &desc);
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
        ui_m6569_desc_t desc = { 0 };
        desc.title = "MOS 6569 (VIC-II)";
        desc.vic = &ui->x65->vic;
        desc.x = x;
        desc.y = y;
        UI_CHIP_INIT_DESC(&desc.chip_desc, "6569", 28, _ui_x65_vic_pins);
        ui_m6569_init(&ui->vic, &desc);
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
        desc.start_addr = mem_rd16(&ui->x65->mem_cpu, 0xFFFC);
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
    ui_m6526_discard(&ui->cia[0]);
    ui_m6526_discard(&ui->cia[1]);
    ui_m6581_discard(&ui->sid);
    ui_m6569_discard(&ui->vic);
    ui_console_discard(&ui->ria_uart);
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
    ImGui::DockSpaceOverViewport(
        0,
        ImGui::GetMainViewport(),
        ImGuiDockNodeFlags_NoDockingOverCentralNode | ImGuiDockNodeFlags_PassthruCentralNode);
    if (ui->memmap.open) {
        _ui_x65_update_memmap(ui);
    }
    ui_audio_draw(&ui->audio, ui->x65->audio.sample_pos);
    ui_kbd_draw(&ui->kbd);
    ui_m6502_draw(&ui->cpu);
    ui_m6526_draw(&ui->cia[0]);
    ui_m6526_draw(&ui->cia[1]);
    ui_m6581_draw(&ui->sid);
    ui_m6569_draw(&ui->vic);
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

#ifdef __clang__
    #pragma clang diagnostic pop
#endif
