#include "./x65.h"

#include "chips/clk.h"

#include <string.h>  // memcpy, memset

#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

#define _X65_SCREEN_WIDTH  (392)
#define _X65_SCREEN_HEIGHT (272)
#define _X65_SCREEN_X      (64)
#define _X65_SCREEN_Y      (24)

static uint8_t _x65_cpu_port_in(void* user_data);
static void _x65_cpu_port_out(uint8_t data, void* user_data);
static uint16_t _x65_vic_fetch(uint16_t addr, void* user_data);
static void _x65_update_memory_map(x65_t* sys);
static void _x65_init_key_map(x65_t* sys);
static void _x65_init_memory_map(x65_t* sys);

#define _X65_DEFAULT(val, def) (((val) != 0) ? (val) : (def))

void x65_init(x65_t* sys, const x65_desc_t* desc) {
    CHIPS_ASSERT(sys && desc);
    if (desc->debug.callback.func) {
        CHIPS_ASSERT(desc->debug.stopped);
    }

    memset(sys, 0, sizeof(x65_t));
    sys->valid = true;
    sys->running = false;
    sys->joystick_type = desc->joystick_type;
    sys->debug = desc->debug;
    sys->audio.callback = desc->audio.callback;
    sys->audio.num_samples = _X65_DEFAULT(desc->audio.num_samples, X65_DEFAULT_AUDIO_SAMPLES);
    CHIPS_ASSERT(sys->audio.num_samples <= X65_MAX_AUDIO_SAMPLES);
    CHIPS_ASSERT(desc->roms.chars.ptr && (desc->roms.chars.size == sizeof(sys->rom_char)));
    memcpy(sys->rom_char, desc->roms.chars.ptr, sizeof(sys->rom_char));

    // initialize the hardware
    sys->cpu_port = 0xF7;  // for initial memory mapping
    sys->io_mapped = true;

    sys->pins = m6502_init(
        &sys->cpu,
        &(m6502_desc_t){
            .m6510_in_cb = _x65_cpu_port_in,
            .m6510_out_cb = _x65_cpu_port_out,
            .m6510_io_pullup = 0x17,
            .m6510_io_floating = 0xC8,
            .m6510_user_data = sys,
        });
    m6526_init(&sys->cia_1);
    m6526_init(&sys->cia_2);
    ria816_init(&sys->ria);
    m6569_init(&sys->vic, &(m6569_desc_t){
        .fetch_cb = _x65_vic_fetch,
        .framebuffer = {
            .ptr = sys->fb,
            .size = sizeof(sys->fb),
        },
        .screen = {
            .x = _X65_SCREEN_X,
            .y = _X65_SCREEN_Y,
            .width = _X65_SCREEN_WIDTH,
            .height = _X65_SCREEN_HEIGHT,
        },
        .user_data = sys,
    });
    m6581_init(
        &sys->sid,
        &(m6581_desc_t){
            .tick_hz = X65_FREQUENCY,
            .sound_hz = _X65_DEFAULT(desc->audio.sample_rate, 44100),
            .magnitude = _X65_DEFAULT(desc->audio.volume, 1.0f),
        });
    _x65_init_key_map(sys);
    _x65_init_memory_map(sys);
}

void x65_discard(x65_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->valid = false;
}

void x65_reset(x65_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->cpu_port = 0xF7;
    sys->kbd_joy1_mask = sys->kbd_joy2_mask = 0;
    sys->joy_joy1_mask = sys->joy_joy2_mask = 0;
    sys->io_mapped = true;
    _x65_update_memory_map(sys);
    sys->pins |= M6502_RES;
    m6526_reset(&sys->cia_1);
    m6526_reset(&sys->cia_2);
    ria816_reset(&sys->ria);
    m6569_reset(&sys->vic);
    m6581_reset(&sys->sid);
}

void x65_set_running(x65_t* sys, bool running) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->running = running;
}

static uint64_t _x65_tick(x65_t* sys, uint64_t pins) {
    if (!sys->running) {
        // keep CPU in RESET state
        pins |= M6502_RES;
    }

    // tick the CPU
    pins = m6502_tick(&sys->cpu, pins);
    const uint16_t addr = M6502_GET_ADDR(pins);

    // those pins are set each tick by the CIAs and VIC
    pins &= ~(M6502_IRQ | M6502_NMI | M6502_RDY | M6510_AEC);

    /*  address decoding

        When the RDY pin is active (during bad lines), no CPU/chip
        communication takes place starting with the first read access.
    */
    bool cpu_io_access = false;
    bool color_ram_access = false;
    bool mem_access = false;
    uint64_t vic_pins = pins & M6502_PIN_MASK;
    uint64_t cia1_pins = pins & M6502_PIN_MASK;
    uint64_t cia2_pins = pins & M6502_PIN_MASK;
    uint64_t ria_pins = pins & M6502_PIN_MASK;
    uint64_t sid_pins = pins & M6502_PIN_MASK;
    if ((pins & (M6502_RDY | M6502_RW)) != (M6502_RDY | M6502_RW)) {
        if (M6510_CHECK_IO(pins)) {
            cpu_io_access = true;
        }
        else {
            if (sys->io_mapped && ((addr & 0xF000) == 0xD000)) {
                if (addr < 0xD400) {
                    // VIC-II (D000..D3FF)
                    vic_pins |= M6569_CS;
                }
                else if (addr < 0xD800) {
                    // SID (D400..D7FF)
                    sid_pins |= M6581_CS;
                }
                else if (addr < 0xDC00) {
                    // read or write the special color Static-RAM bank (D800..DBFF)
                    color_ram_access = true;
                }
                else if (addr < 0xDD00) {
                    // CIA-1 (DC00..DCFF)
                    cia1_pins |= M6526_CS;
                }
                else if (addr < 0xDE00) {
                    // CIA-2 (DD00..DDFF)
                    cia2_pins |= M6526_CS;
                }
            }
            else if ((addr & 0xFF00) == 0xFF00) {
                mem_access = true;  // FIXME: remove
                if (addr >= 0xFFC0) {
                    // RIA (FFC0..FFFF)
                    ria_pins |= RIA816_CS;
                }
                else if (addr >= 0xFFA0) {
                    // CGIA (FFA0..FFBF)
                    // cgia_pins |= CGIA_CS;
                }
                else if (addr >= 0xFF80) {
                    // SD-1 (FF80..FF9F)
                    // sd1_pins |= YMF825_CS;
                }
            }
            else {
                mem_access = true;
            }
        }
    }

    // tick the SID
    {
        sid_pins = m6581_tick(&sys->sid, sid_pins);
        if (sid_pins & M6581_SAMPLE) {
            // new audio sample ready
            sys->audio.sample_buffer[sys->audio.sample_pos++] = sys->sid.sample;
            if (sys->audio.sample_pos == sys->audio.num_samples) {
                if (sys->audio.callback.func) {
                    sys->audio.callback.func(
                        sys->audio.sample_buffer,
                        sys->audio.num_samples,
                        sys->audio.callback.user_data);
                }
                sys->audio.sample_pos = 0;
            }
        }
        if ((sid_pins & (M6581_CS | M6581_RW)) == (M6581_CS | M6581_RW)) {
            pins = M6502_COPY_DATA(pins, sid_pins);
        }
    }

    /* tick CIA-1:

        In Port A:
            joystick 2 input
        In Port B:
            combined keyboard matrix columns and joystick 1
        Cas Port Read => Flag pin

        Out Port A:
            write keyboard matrix lines

        IRQ pin is connected to the CPU IRQ pin
    */
    {
        // cassette port READ pin is connected to CIA-1 FLAG pin
        const uint8_t pa = ~(sys->kbd_joy2_mask | sys->joy_joy2_mask);
        const uint8_t pb = ~(kbd_scan_columns(&sys->kbd) | sys->kbd_joy1_mask | sys->joy_joy1_mask);
        M6526_SET_PAB(cia1_pins, pa, pb);
        cia1_pins = m6526_tick(&sys->cia_1, cia1_pins);
        const uint8_t kbd_lines = ~M6526_GET_PA(cia1_pins);
        kbd_set_active_lines(&sys->kbd, kbd_lines);
        if (cia1_pins & M6502_IRQ) {
            pins |= M6502_IRQ;
        }
        if ((cia1_pins & (M6526_CS | M6526_RW)) == (M6526_CS | M6526_RW)) {
            pins = M6502_COPY_DATA(pins, cia1_pins);
        }
    }

    /* tick CIA-2
        In Port A:
            bits 0..5: output (see cia2_out)
            bits 6..7: serial bus input, not implemented
        In Port B:
            RS232 / user functionality (not implemented)

        Out Port A:
            bits 0..1: VIC-II bank select:
                00: bank 3 C000..FFFF
                01: bank 2 8000..BFFF
                10: bank 1 4000..7FFF
                11: bank 0 0000..3FFF
            bit 2: RS-232 TXD Outout (not implemented)
            bit 3..5: serial bus output (not implemented)
            bit 6..7: input (see cia2_in)
        Out Port B:
            RS232 / user functionality (not implemented)

        CIA-2 IRQ pin connected to CPU NMI pin
    */
    {
        M6526_SET_PAB(cia2_pins, 0xFF, 0xFF);
        cia2_pins = m6526_tick(&sys->cia_2, cia2_pins);
        sys->vic_bank_select = ((~M6526_GET_PA(cia2_pins)) & 3) << 14;
        if (cia2_pins & M6502_IRQ) {
            pins |= M6502_NMI;
        }
        if ((cia2_pins & (M6526_CS | M6526_RW)) == (M6526_CS | M6526_RW)) {
            pins = M6502_COPY_DATA(pins, cia2_pins);
        }
    }

    // the RESTORE key, along with CIA-2 IRQ, is connected to the NMI line,
    if (sys->kbd.scanout_column_masks[8] & 1) {
        pins |= M6502_NMI;
    }

    /* tick RIA816:
     */
    {
        ria_pins = ria816_tick(&sys->ria, ria_pins);
        if ((ria_pins & (RIA816_CS | RIA816_RW)) == (RIA816_CS | RIA816_RW)) {
            pins = M6502_COPY_DATA(pins, ria_pins);
        }
    }

    /* tick the VIC-II display chip:
        - the VIC-II IRQ pin is connected to the CPU IRQ pin and goes
        active when the VIC-II requests a rasterline interrupt
        - the VIC-II BA pin is connected to the CPU RDY pin, and stops
        the CPU on the first CPU read access after BA goes active
        - the VIC-II AEC pin is connected to the CPU AEC pin, currently
        this goes active during a badline, but is not checked
    */
    {
        vic_pins = m6569_tick(&sys->vic, vic_pins);
        pins |= (vic_pins & (M6502_IRQ | M6502_RDY | M6510_AEC));
        if ((vic_pins & (M6569_CS | M6569_RW)) == (M6569_CS | M6569_RW)) {
            pins = M6502_COPY_DATA(pins, vic_pins);
        }
    }

    /* remaining CPU IO and memory accesses, those don't fit into the
       "universal tick model" (yet?)
    */
    if (cpu_io_access) {
        // ...the integrated IO port in the M6510 CPU at addresses 0 and 1
        pins = m6510_iorq(&sys->cpu, pins);
    }
    else if (color_ram_access) {
        if (pins & M6502_RW) {
            M6502_SET_DATA(pins, sys->color_ram[addr & 0x03FF]);
        }
        else {
            sys->color_ram[addr & 0x03FF] = M6502_GET_DATA(pins);
        }
    }
    else if (mem_access) {
        if (pins & M6502_RW) {
            // memory read
            M6502_SET_DATA(pins, mem_rd(&sys->mem_cpu, addr));
        }
        else {
            // memory write
            mem_wr(&sys->mem_cpu, addr, M6502_GET_DATA(pins));
        }
    }
    return pins;
}

static uint8_t _x65_cpu_port_in(void* user_data) {
    x65_t* sys = (x65_t*)user_data;
    /*
        Input from the integrated M6510 CPU IO port
    */
    uint8_t val = 7;
    return val;
}

static void _x65_cpu_port_out(uint8_t data, void* user_data) {
    x65_t* sys = (x65_t*)user_data;
    /*
        Output to the integrated M6510 CPU IO port

        bits 0..2:  [out] memory configuration
    */
    /* only update memory configuration if the relevant bits have changed */
    bool need_mem_update = 0 != ((sys->cpu_port ^ data) & 7);
    sys->cpu_port = data;
    if (need_mem_update) {
        _x65_update_memory_map(sys);
    }
}

static uint16_t _x65_vic_fetch(uint16_t addr, void* user_data) {
    x65_t* sys = (x65_t*)user_data;
    /*
        Fetch data into the VIC-II.

        The VIC-II has a 14-bit address bus and 12-bit data bus, and
        has a different memory mapping then the CPU (that's why it
        goes through the mem_vic pagetable):
            - a full 16-bit address is formed by taking the address bits
              14 and 15 from the value written to CIA-1 port A
            - the lower 8 bits of the VIC-II data bus are connected
              to the shared system data bus, this is used to read
              character mask and pixel data
            - the upper 4 bits of the VIC-II data bus are hardwired to the
              static color RAM
    */
    addr |= sys->vic_bank_select;
    uint16_t data = (sys->color_ram[addr & 0x03FF] << 8) | mem_rd(&sys->mem_vic, addr);
    return data;
}

static void _x65_update_memory_map(x65_t* sys) {
    sys->io_mapped = false;
    // shortcut if HIRAM and LORAM is 0, everything is RAM
    if ((sys->cpu_port & (X65_CPUPORT_HIRAM | X65_CPUPORT_LORAM)) == 0) {
        mem_map_ram(&sys->mem_cpu, 0, 0xA000, 0x6000, sys->ram + 0xA000);
    }
    else {
        // A000..BFFF is RAM
        mem_map_rw(&sys->mem_cpu, 0, 0xA000, 0x2000, sys->ram + 0xA000, sys->ram + 0xA000);

        // E000..FFFF is RAM
        mem_map_rw(&sys->mem_cpu, 0, 0xE000, 0x2000, sys->ram + 0xE000, sys->ram + 0xE000);

        // D000..DFFF can be Char-ROM or I/O
        if (sys->cpu_port & X65_CPUPORT_CHAREN) {
            sys->io_mapped = true;
        }
        else {
            mem_map_rw(&sys->mem_cpu, 0, 0xD000, 0x1000, sys->rom_char, sys->ram + 0xD000);
        }
    }
}

static void _x65_init_memory_map(x65_t* sys) {
    // seperate memory mapping for CPU and VIC-II
    mem_init(&sys->mem_cpu);
    mem_init(&sys->mem_vic);

    /*
        the X65 has a weird RAM init pattern of 64 bytes 00 and 64 bytes FF
        alternating, probably with some randomness sprinkled in
        (see this thread: http://csdb.dk/forums/?roomid=11&topicid=116800&firstpost=2)
        this is important at least for the value of the 'ghost byte' at 0x3FFF,
        which is 0xFF
    */
    size_t i;
    for (i = 0; i < (1 << 16);) {
        for (size_t j = 0; j < 64; j++, i++) {
            sys->ram[i] = 0x00;
        }
        for (size_t j = 0; j < 64; j++, i++) {
            sys->ram[i] = 0xFF;
        }
    }
    CHIPS_ASSERT(i == 0x10000);

    /* setup the initial CPU memory map
       0000..9FFF and C000.CFFF is always RAM
    */
    mem_map_ram(&sys->mem_cpu, 0, 0x0000, 0xA000, sys->ram);
    mem_map_ram(&sys->mem_cpu, 0, 0xC000, 0x1000, sys->ram + 0xC000);
    // A000..BFFF, D000..DFFF and E000..FFFF are configurable
    _x65_update_memory_map(sys);

    /* setup the separate VIC-II memory map (64 KByte RAM) overlayed with
       character ROMS at 0x1000.0x1FFF and 0x9000..0x9FFF
    */
    mem_map_ram(&sys->mem_vic, 1, 0x0000, 0x10000, sys->ram);
    mem_map_rom(&sys->mem_vic, 0, 0x1000, 0x1000, sys->rom_char);
    mem_map_rom(&sys->mem_vic, 0, 0x9000, 0x1000, sys->rom_char);
}

static void _x65_init_key_map(x65_t* sys) {
    /*
        http://sta.x65.org/cbm64kbdlay.html
        http://sta.x65.org/cbm64petkey.html
    */
    kbd_init(&sys->kbd, 1);

    const char* keymap =
        // no shift
        "        "
        "3WA4ZSE "
        "5RD6CFTX"
        "7YG8BHUV"
        "9IJ0MKON"
        "+PL-.:@,"
        "~*;  = /"  // ~ is actually the British Pound sign
        "1  2  Q "

        // shift
        "        "
        "#wa$zse "
        "%rd&cftx"
        "'yg(bhuv"
        ")ij0mkon"
        " pl >[ <"
        "$ ]    ?"
        "!  \"  q ";
    CHIPS_ASSERT(strlen(keymap) == 128);
    // shift is column 7, line 1
    kbd_register_modifier(&sys->kbd, 0, 7, 1);
    // ctrl is column 2, line 7
    kbd_register_modifier(&sys->kbd, 1, 2, 7);
    for (int shift = 0; shift < 2; shift++) {
        for (int column = 0; column < 8; column++) {
            for (int line = 0; line < 8; line++) {
                int c = keymap[shift * 64 + line * 8 + column];
                if (c != 0x20) {
                    kbd_register_key(&sys->kbd, c, column, line, shift ? (1 << 0) : 0);
                }
            }
        }
    }

    // special keys
    kbd_register_key(&sys->kbd, X65_KEY_SPACE, 4, 7, 0);     // space
    kbd_register_key(&sys->kbd, X65_KEY_CSRLEFT, 2, 0, 1);   // cursor left (shift+cursor right)
    kbd_register_key(&sys->kbd, X65_KEY_CSRRIGHT, 2, 0, 0);  // cursor right
    kbd_register_key(&sys->kbd, X65_KEY_CSRDOWN, 7, 0, 0);   // cursor down
    kbd_register_key(&sys->kbd, X65_KEY_CSRUP, 7, 0, 1);     // cursor up (shift+cursor down)
    kbd_register_key(&sys->kbd, X65_KEY_DEL, 0, 0, 0);       // delete
    kbd_register_key(&sys->kbd, X65_KEY_INST, 0, 0, 1);      // inst (shift+del)
    kbd_register_key(&sys->kbd, X65_KEY_HOME, 3, 6, 0);      // home
    kbd_register_key(&sys->kbd, X65_KEY_CLR, 3, 6, 1);       // clear (shift+home)
    kbd_register_key(&sys->kbd, X65_KEY_RETURN, 1, 0, 0);    // return
    kbd_register_key(&sys->kbd, X65_KEY_CTRL, 2, 7, 0);      // ctrl
    kbd_register_key(&sys->kbd, X65_KEY_CBM, 5, 7, 0);       // C= commodore key
    kbd_register_key(&sys->kbd, X65_KEY_RESTORE, 0, 8, 0);   // restore (connected to the NMI line)
    kbd_register_key(&sys->kbd, X65_KEY_STOP, 7, 7, 0);      // run stop
    kbd_register_key(&sys->kbd, X65_KEY_RUN, 7, 7, 1);       // run (shift+run stop)
    kbd_register_key(&sys->kbd, X65_KEY_LEFT, 1, 7, 0);      // left arrow symbol
    kbd_register_key(&sys->kbd, X65_KEY_F1, 4, 0, 0);        // F1
    kbd_register_key(&sys->kbd, X65_KEY_F2, 4, 0, 1);        // F2
    kbd_register_key(&sys->kbd, X65_KEY_F3, 5, 0, 0);        // F3
    kbd_register_key(&sys->kbd, X65_KEY_F4, 5, 0, 1);        // F4
    kbd_register_key(&sys->kbd, X65_KEY_F5, 6, 0, 0);        // F5
    kbd_register_key(&sys->kbd, X65_KEY_F6, 6, 0, 1);        // F6
    kbd_register_key(&sys->kbd, X65_KEY_F7, 3, 0, 0);        // F7
    kbd_register_key(&sys->kbd, X65_KEY_F8, 3, 0, 1);        // F8
}

uint32_t x65_exec(x65_t* sys, uint32_t micro_seconds) {
    CHIPS_ASSERT(sys && sys->valid);
    uint32_t num_ticks = clk_us_to_ticks(X65_FREQUENCY, micro_seconds);
    uint64_t pins = sys->pins;
    if (0 == sys->debug.callback.func) {
        // run without debug callback
        for (uint32_t ticks = 0; ticks < num_ticks; ticks++) {
            pins = _x65_tick(sys, pins);
        }
    }
    else {
        // run with debug callback
        for (uint32_t ticks = 0; (ticks < num_ticks) && !(*sys->debug.stopped); ticks++) {
            pins = _x65_tick(sys, pins);
            sys->debug.callback.func(sys->debug.callback.user_data, pins);
        }
    }
    sys->pins = pins;
    kbd_update(&sys->kbd, micro_seconds);
    return num_ticks;
}

void x65_key_down(x65_t* sys, int key_code) {
    CHIPS_ASSERT(sys && sys->valid);
    if (sys->joystick_type == X65_JOYSTICKTYPE_NONE) {
        kbd_key_down(&sys->kbd, key_code);
    }
    else {
        uint8_t m = 0;
        switch (key_code) {
            case 0x20: m = X65_JOYSTICK_BTN; break;
            case 0x08: m = X65_JOYSTICK_LEFT; break;
            case 0x09: m = X65_JOYSTICK_RIGHT; break;
            case 0x0A: m = X65_JOYSTICK_DOWN; break;
            case 0x0B: m = X65_JOYSTICK_UP; break;
            default: kbd_key_down(&sys->kbd, key_code); break;
        }
        if (m != 0) {
            switch (sys->joystick_type) {
                case X65_JOYSTICKTYPE_DIGITAL_1: sys->kbd_joy1_mask |= m; break;
                case X65_JOYSTICKTYPE_DIGITAL_2: sys->kbd_joy2_mask |= m; break;
                case X65_JOYSTICKTYPE_DIGITAL_12:
                    sys->kbd_joy1_mask |= m;
                    sys->kbd_joy2_mask |= m;
                    break;
                default: break;
            }
        }
    }
}

void x65_key_up(x65_t* sys, int key_code) {
    CHIPS_ASSERT(sys && sys->valid);
    if (sys->joystick_type == X65_JOYSTICKTYPE_NONE) {
        kbd_key_up(&sys->kbd, key_code);
    }
    else {
        uint8_t m = 0;
        switch (key_code) {
            case 0x20: m = X65_JOYSTICK_BTN; break;
            case 0x08: m = X65_JOYSTICK_LEFT; break;
            case 0x09: m = X65_JOYSTICK_RIGHT; break;
            case 0x0A: m = X65_JOYSTICK_DOWN; break;
            case 0x0B: m = X65_JOYSTICK_UP; break;
            default: kbd_key_up(&sys->kbd, key_code); break;
        }
        if (m != 0) {
            switch (sys->joystick_type) {
                case X65_JOYSTICKTYPE_DIGITAL_1: sys->kbd_joy1_mask &= ~m; break;
                case X65_JOYSTICKTYPE_DIGITAL_2: sys->kbd_joy2_mask &= ~m; break;
                case X65_JOYSTICKTYPE_DIGITAL_12:
                    sys->kbd_joy1_mask &= ~m;
                    sys->kbd_joy2_mask &= ~m;
                    break;
                default: break;
            }
        }
    }
}

void x65_set_joystick_type(x65_t* sys, x65_joystick_type_t type) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->joystick_type = type;
}

x65_joystick_type_t x65_joystick_type(x65_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    return sys->joystick_type;
}

void x65_joystick(x65_t* sys, uint8_t joy1_mask, uint8_t joy2_mask) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->joy_joy1_mask = joy1_mask;
    sys->joy_joy2_mask = joy2_mask;
}

bool x65_quickload(x65_t* sys, chips_range_t data) {
    CHIPS_ASSERT(sys && sys->valid && data.ptr);
    if (data.size < 2) {
        return false;
    }
    const uint8_t* ptr = (uint8_t*)data.ptr;
    const uint16_t start_addr = ptr[1] << 8 | ptr[0];
    ptr += 2;
    const uint16_t end_addr = start_addr + (data.size - 2);
    uint16_t addr = start_addr;
    while (addr < end_addr) {
        mem_wr(&sys->mem_cpu, addr++, *ptr++);
    }

    // update the BASIC pointers
    mem_wr16(&sys->mem_cpu, 0x2d, end_addr);
    mem_wr16(&sys->mem_cpu, 0x2f, end_addr);
    mem_wr16(&sys->mem_cpu, 0x31, end_addr);
    mem_wr16(&sys->mem_cpu, 0x33, end_addr);
    mem_wr16(&sys->mem_cpu, 0xae, end_addr);

    return true;
}

bool x65_quickload_xex(x65_t* sys, chips_range_t data) {
    CHIPS_ASSERT(sys && sys->valid && data.ptr);
    if (data.size < 2) {
        return false;
    }
    const uint8_t* ptr = (uint8_t*)data.ptr;

    bool reset_lo_loaded = false;
    bool reset_hi_loaded = false;

    // $FFFF is required in first block
    if (ptr[0] != 0xff || ptr[1] != 0xff) {
        return false;
    }

    while (ptr < ((uint8_t*)data.ptr + data.size)) {
        size_t data_left = (uint8_t*)data.ptr + data.size - ptr;
        if (data_left < 4) {
            return false;
        }
        if (ptr[0] == 0xff || ptr[1] == 0xff) {
            // skip header
            ptr += 2;
            data_left -= 2;
            if (data_left < 4) {
                return false;
            }
        }

        const uint16_t start_addr = ptr[1] << 8 | ptr[0];
        ptr += 2;
        const uint16_t end_addr = ptr[1] << 8 | ptr[0];
        ptr += 2;

        data_left = (uint8_t*)data.ptr + data.size - ptr;
        if (data_left < (end_addr - start_addr + 1) || start_addr > end_addr) {
            return false;
        }
        uint16_t addr = start_addr;
        while (addr <= end_addr && addr >= start_addr) {
            if (addr == 0xfffc) reset_lo_loaded = true;
            if (addr == 0xfffd) reset_hi_loaded = true;
            mem_wr(&sys->mem_cpu, addr++, *ptr++);
        }
    }

    if (reset_lo_loaded && reset_hi_loaded) {
        sys->running = true;
    }

    return true;
}

chips_display_info_t x65_display_info(x65_t* sys) {
    chips_display_info_t res = {
        .frame = {
            .dim = {
                .width = M6569_FRAMEBUFFER_WIDTH,
                .height = M6569_FRAMEBUFFER_HEIGHT,
            },
            .bytes_per_pixel = 1,
            .buffer = {
                .ptr = sys ? sys->fb : 0,
                .size = M6569_FRAMEBUFFER_SIZE_BYTES,
            }
        },
        .palette = m6569_dbg_palette(),
    };
    if (sys) {
        res.screen = m6569_screen(&sys->vic);
    }
    else {
        res.screen = (chips_rect_t){
            .x = 0,
            .y = 0,
            .width = _X65_SCREEN_WIDTH,
            .height = _X65_SCREEN_HEIGHT,
        };
    };
    CHIPS_ASSERT(((sys == 0) && (res.frame.buffer.ptr == 0)) || ((sys != 0) && (res.frame.buffer.ptr != 0)));
    return res;
}

uint32_t x65_save_snapshot(x65_t* sys, x65_t* dst) {
    CHIPS_ASSERT(sys && dst);
    *dst = *sys;
    chips_debug_snapshot_onsave(&dst->debug);
    chips_audio_callback_snapshot_onsave(&dst->audio.callback);
    m6502_snapshot_onsave(&dst->cpu);
    m6569_snapshot_onsave(&dst->vic);
    mem_snapshot_onsave(&dst->mem_cpu, sys);
    mem_snapshot_onsave(&dst->mem_vic, sys);
    return X65_SNAPSHOT_VERSION;
}

bool x65_load_snapshot(x65_t* sys, uint32_t version, x65_t* src) {
    CHIPS_ASSERT(sys && src);
    if (version != X65_SNAPSHOT_VERSION) {
        return false;
    }
    static x65_t im;
    im = *src;
    chips_debug_snapshot_onload(&im.debug, &sys->debug);
    chips_audio_callback_snapshot_onload(&im.audio.callback, &sys->audio.callback);
    m6502_snapshot_onload(&im.cpu, &sys->cpu);
    m6569_snapshot_onload(&im.vic, &sys->vic);
    mem_snapshot_onload(&im.mem_cpu, sys);
    mem_snapshot_onload(&im.mem_vic, sys);
    *sys = im;
    return true;
}

void x65_basic_run(x65_t* sys) {
    CHIPS_ASSERT(sys);
    // write RUN into the keyboard buffer
    uint16_t keybuf = 0x277;
    mem_wr(&sys->mem_cpu, keybuf++, 'R');
    mem_wr(&sys->mem_cpu, keybuf++, 'U');
    mem_wr(&sys->mem_cpu, keybuf++, 'N');
    mem_wr(&sys->mem_cpu, keybuf++, 0x0D);
    // write number of characters, this kicks off evaluation
    mem_wr(&sys->mem_cpu, 0xC6, 4);
}

void x65_basic_load(x65_t* sys) {
    CHIPS_ASSERT(sys);
    // write LOAD
    uint16_t keybuf = 0x277;
    mem_wr(&sys->mem_cpu, keybuf++, 'L');
    mem_wr(&sys->mem_cpu, keybuf++, 'O');
    mem_wr(&sys->mem_cpu, keybuf++, 'A');
    mem_wr(&sys->mem_cpu, keybuf++, 'D');
    mem_wr(&sys->mem_cpu, keybuf++, 0x0D);
    // write number of characters, this kicks off evaluation
    mem_wr(&sys->mem_cpu, 0xC6, 5);
}

void x65_basic_syscall(x65_t* sys, uint16_t addr) {
    CHIPS_ASSERT(sys);
    // write SYS xxxx[Return] into the keyboard buffer (up to 10 chars)
    uint16_t keybuf = 0x277;
    mem_wr(&sys->mem_cpu, keybuf++, 'S');
    mem_wr(&sys->mem_cpu, keybuf++, 'Y');
    mem_wr(&sys->mem_cpu, keybuf++, 'S');
    mem_wr(&sys->mem_cpu, keybuf++, ((addr / 10000) % 10) + '0');
    mem_wr(&sys->mem_cpu, keybuf++, ((addr / 1000) % 10) + '0');
    mem_wr(&sys->mem_cpu, keybuf++, ((addr / 100) % 10) + '0');
    mem_wr(&sys->mem_cpu, keybuf++, ((addr / 10) % 10) + '0');
    mem_wr(&sys->mem_cpu, keybuf++, ((addr / 1) % 10) + '0');
    mem_wr(&sys->mem_cpu, keybuf++, 0x0D);
    // write number of characters, this kicks off evaluation
    mem_wr(&sys->mem_cpu, 0xC6, 9);
}

uint16_t x65_syscall_return_addr(void) {
    return 0xA7EA;
}
