#include "./x65.h"

#include "chips/clk.h"

#include <string.h>  // memcpy, memset

#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

static uint64_t _x65_vpu_fetch(uint64_t pins, void* user_data);
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

    // initialize the hardware
    sys->pins = w65816_init(&sys->cpu, &(w65816_desc_t){});
    m6526_init(&sys->cia_1);
    m6526_init(&sys->cia_2);
    ria816_init(&sys->ria);
    cgia_init(&sys->cgia, &(cgia_desc_t){
        .tick_hz = X65_FREQUENCY,
        .fetch_cb = _x65_vpu_fetch,
        .framebuffer = {
            .ptr = sys->fb,
            .size = sizeof(sys->fb),
        },
        .user_data = sys,
    });
    const beeper_desc_t beeper_desc = {
        .tick_hz = X65_FREQUENCY,
        .sound_hz = _X65_DEFAULT(desc->audio.sample_rate, 44100),
        .base_volume = _X65_DEFAULT(desc->audio.volume, 1.0f),
    };
    beeper_init(&sys->beeper[0], &beeper_desc);
    beeper_init(&sys->beeper[1], &beeper_desc);
    m6581_init(
        &sys->sid,
        &(m6581_desc_t){
            .tick_hz = X65_FREQUENCY,
            .sound_hz = _X65_DEFAULT(desc->audio.sample_rate, 44100),
            .magnitude = _X65_DEFAULT(desc->audio.volume, 1.0f),
        });
    ymf262_init(
        &sys->opl3,
        &(ymf262_desc_t){
            .tick_hz = X65_FREQUENCY,
            .sound_hz = _X65_DEFAULT(desc->audio.sample_rate, 44100),
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
    sys->kbd_joy1_mask = sys->kbd_joy2_mask = 0;
    sys->joy_joy1_mask = sys->joy_joy2_mask = 0;
    sys->pins |= W65816_RES;
    m6526_reset(&sys->cia_1);
    m6526_reset(&sys->cia_2);
    ria816_reset(&sys->ria);
    cgia_reset(&sys->cgia);
    beeper_reset(&sys->beeper[0]);
    beeper_reset(&sys->beeper[1]);
    m6581_reset(&sys->sid);
    ymf262_reset(&sys->opl3);
}

void x65_set_running(x65_t* sys, bool running) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->running = running;
    if (sys->running) {
        // copy CPU vectors from RAM to RIA (in case it were fastloaded)
        sys->ria.reg[RIA816_CPU_N_COP] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_N_COP);
        sys->ria.reg[RIA816_CPU_N_COP + 1] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_N_COP + 1);
        sys->ria.reg[RIA816_CPU_N_BRK] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_N_BRK);
        sys->ria.reg[RIA816_CPU_N_BRK + 1] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_N_BRK + 1);
        sys->ria.reg[RIA816_CPU_N_ABORTB] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_N_ABORTB);
        sys->ria.reg[RIA816_CPU_N_ABORTB + 1] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_N_ABORTB + 1);
        sys->ria.reg[RIA816_CPU_N_NMIB] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_N_NMIB);
        sys->ria.reg[RIA816_CPU_N_NMIB + 1] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_N_NMIB + 1);
        sys->ria.reg[RIA816_CPU_N_IRQB] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_N_IRQB);
        sys->ria.reg[RIA816_CPU_N_IRQB + 1] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_N_IRQB + 1);
        sys->ria.reg[RIA816_CPU_E_COP] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_E_COP);
        sys->ria.reg[RIA816_CPU_E_COP + 1] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_E_COP + 1);
        sys->ria.reg[RIA816_CPU_E_ABORTB] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_E_ABORTB);
        sys->ria.reg[RIA816_CPU_E_ABORTB + 1] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_E_ABORTB + 1);
        sys->ria.reg[RIA816_CPU_E_NMIB] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_E_NMIB);
        sys->ria.reg[RIA816_CPU_E_NMIB + 1] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_E_NMIB + 1);
        sys->ria.reg[RIA816_CPU_E_RESETB] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_E_RESETB);
        sys->ria.reg[RIA816_CPU_E_RESETB + 1] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_E_RESETB + 1);
        sys->ria.reg[RIA816_CPU_E_IRQB_BRK] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_E_IRQB_BRK);
        sys->ria.reg[RIA816_CPU_E_IRQB_BRK + 1] = mem_rd(&sys->mem, X65_IO_RIA_BASE + RIA816_CPU_E_IRQB_BRK + 1);
        // and CGIA VRAM
        cgia_mirror_vram(&sys->cgia);
    }
}

static uint64_t _x65_tick(x65_t* sys, uint64_t pins) {
    if (!sys->running) {
        // keep CPU in RESET state
        pins |= W65816_RES;
    }

    // tick the CPU
    pins = w65816_tick(&sys->cpu, pins);
    const uint16_t addr = W65816_GET_ADDR(pins);

    // those pins are set each tick by the CIAs and VIC
    pins &= ~(W65816_IRQ | W65816_NMI | W65816_RDY);

    /*  address decoding

        When the RDY pin is active (during bad lines), no CPU/chip
        communication takes place starting with the first read access.
    */
    bool mem_access = false;
    uint64_t cgia_pins = pins & W65816_PIN_MASK;
    uint64_t ria_pins = pins & W65816_PIN_MASK;
    uint64_t sd1_pins = pins & W65816_PIN_MASK;
    uint64_t opl3_pins = pins & W65816_PIN_MASK;
    if ((pins & (W65816_RDY | W65816_RW)) != (W65816_RDY | W65816_RW)) {
        if (sys->ria.reg[RIA816_EXT_IO] && ((addr & 0xFF00) == X65_EXT_BASE)) {
            const uint8_t slot = (addr & 0xFF) >> 5;
            if ((sys->ria.reg[RIA816_EXT_IO] & (1U << slot))) switch (slot) {
                    case 0x00: {
                        // OPL-3 (FC00..FC1F)
                        opl3_pins |= YMF262_CS;
                    } break;
                    default:
                        if (pins & W65816_RW) {
                            // memory read nothin'
                            W65816_SET_DATA(pins, 0xFF);
                        }
                }
        }
        else if ((addr & X65_IO_BASE) == X65_IO_BASE) {
            if (addr >= X65_IO_RIA_BASE) {
                // RIA (FFC0..FFFF)
                ria_pins |= RIA816_CS;
            }
            else if (addr >= X65_IO_YMF825_BASE) {
                // SD-1 (FF80..FF9F)
                sd1_pins |= 0;  // FIXME: YMF825_CS;
            }
            else if (addr >= X65_IO_CGIA_BASE) {
                // CGIA (FF00..FF7F)
                cgia_pins |= CGIA_CS;
            }
        }
        else {
            mem_access = true;
        }
    }

    // /* tick CIA-1:

    //     In Port A:
    //         joystick 2 input
    //     In Port B:
    //         combined keyboard matrix columns and joystick 1
    //     Cas Port Read => Flag pin

    //     Out Port A:
    //         write keyboard matrix lines

    //     IRQ pin is connected to the CPU IRQ pin
    // */
    // {
    //     // cassette port READ pin is connected to CIA-1 FLAG pin
    //     const uint8_t pa = ~(sys->kbd_joy2_mask | sys->joy_joy2_mask);
    //     const uint8_t pb = ~(kbd_scan_columns(&sys->kbd) | sys->kbd_joy1_mask | sys->joy_joy1_mask);
    //     M6526_SET_PAB(cia1_pins, pa, pb);
    //     cia1_pins = m6526_tick(&sys->cia_1, cia1_pins);
    //     const uint8_t kbd_lines = ~M6526_GET_PA(cia1_pins);
    //     kbd_set_active_lines(&sys->kbd, kbd_lines);
    //     if (cia1_pins & W65816_IRQ) {
    //         pins |= W65816_IRQ;
    //     }
    //     if ((cia1_pins & (M6526_CS | M6526_RW)) == (M6526_CS | M6526_RW)) {
    //         pins = W65816_COPY_DATA(pins, cia1_pins);
    //     }
    // }

    /* tick RIA816:
     */
    {
        ria_pins = ria816_tick(&sys->ria, ria_pins);
        if ((ria_pins & (RIA816_CS | RIA816_RW)) == (RIA816_CS | RIA816_RW)) {
            pins = W65816_COPY_DATA(pins, ria_pins);
        }
    }

    /* tick the CGIA display chip:
     */
    {
        cgia_pins = cgia_tick(&sys->cgia, cgia_pins);
        if (cgia_pins & CGIA_INT) {
            pins |= W65816_NMI;
        }
        if ((cgia_pins & (CGIA_CS | CGIA_RW)) == (CGIA_CS | CGIA_RW)) {
            pins = W65816_COPY_DATA(pins, cgia_pins);
        }
    }

    // tick the audio beepers
    beeper_set(&sys->beeper[0], pwm_get_state(&sys->cgia.pwm[0]));
    beeper_tick(&sys->beeper[0]);
    beeper_set(&sys->beeper[1], pwm_get_state(&sys->cgia.pwm[1]));
    beeper_tick(&sys->beeper[1]);

    // tick the FM chip
    {
        opl3_pins = ymf262_tick(&sys->opl3, opl3_pins);
        if (opl3_pins & YMF262_SAMPLE) {
            // new audio sample ready
            sys->audio.sample_buffer[sys->audio.sample_pos++] =
                (sys->opl3.samples[0] + sys->opl3.samples[1]) / 2.0f  // average left and right channels
                + sys->beeper[0].sample + sys->beeper[1].sample;
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
        if ((opl3_pins & (YMF262_CS | YMF262_RW)) == (YMF262_CS | YMF262_RW)) {
            pins = W65816_COPY_DATA(pins, opl3_pins);
        }
    }

    /* remaining CPU IO and memory accesses, those don't fit into the
       "universal tick model" (yet?)
    */
    if (mem_access) {
        if (pins & W65816_RW) {
            // memory read
            W65816_SET_DATA(pins, mem_rd(&sys->mem, addr));
        }
        else {
            // memory write
            uint8_t data = W65816_GET_DATA(pins);
            mem_wr(&sys->mem, addr, data);
            cgia_mem_wr(&sys->cgia, addr, data);
        }
    }
    return pins;
}

uint64_t _x65_vpu_fetch(uint64_t pins, void* user_data) {
    x65_t* sys = (x65_t*)user_data;
    const uint16_t addr = CGIA_GET_ADDR(pins);
    uint8_t data = sys->ram[addr];
    CGIA_SET_DATA(pins, data);
    return pins;
}

static void _x65_init_memory_map(x65_t* sys) {
    mem_init(&sys->mem);

    /* setup the CPU memory map */
    mem_map_ram(&sys->mem, 0, 0x0000, 0x10000, sys->ram);
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
        mem_wr(&sys->mem, addr++, *ptr++);
    }

    // update the BASIC pointers
    mem_wr16(&sys->mem, 0x2d, end_addr);
    mem_wr16(&sys->mem, 0x2f, end_addr);
    mem_wr16(&sys->mem, 0x31, end_addr);
    mem_wr16(&sys->mem, 0x33, end_addr);
    mem_wr16(&sys->mem, 0xae, end_addr);

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
        if (ptr[0] == 0xff && ptr[1] == 0xff) {
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
            mem_wr(&sys->mem, addr++, *ptr++);
        }
    }

    if (reset_lo_loaded && reset_hi_loaded) {
        x65_set_running(sys, true);
    }

    return true;
}

chips_display_info_t x65_display_info(x65_t* sys) {
    const chips_display_info_t res = {
        .frame = {
            .dim = {
                .width = CGIA_FRAMEBUFFER_WIDTH,
                .height = CGIA_FRAMEBUFFER_HEIGHT,
            },
            .bytes_per_pixel = 4,
            .buffer = {
                .ptr = sys ? sys->fb : 0,
                .size = CGIA_FRAMEBUFFER_SIZE_BYTES,
            }
        },
        .screen = {
            .x = 0,
            .y = 0,
            .width = CGIA_DISPLAY_WIDTH,
            .height = CGIA_DISPLAY_HEIGHT,
        },
    };
    CHIPS_ASSERT(((sys == 0) && (res.frame.buffer.ptr == 0)) || ((sys != 0) && (res.frame.buffer.ptr != 0)));
    return res;
}

uint32_t x65_save_snapshot(x65_t* sys, x65_t* dst) {
    CHIPS_ASSERT(sys && dst);
    *dst = *sys;
    chips_debug_snapshot_onsave(&dst->debug);
    chips_audio_callback_snapshot_onsave(&dst->audio.callback);
    w65816_snapshot_onsave(&dst->cpu);
    cgia_snapshot_onsave(&dst->cgia);
    ymf262_snapshot_onsave(&dst->opl3);
    mem_snapshot_onsave(&dst->mem, sys);
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
    w65816_snapshot_onload(&im.cpu, &sys->cpu);
    cgia_snapshot_onload(&im.cgia, &sys->cgia);
    ymf262_snapshot_onload(&im.opl3, &sys->opl3);
    mem_snapshot_onload(&im.mem, sys);
    *sys = im;
    return true;
}

void x65_basic_run(x65_t* sys) {
    CHIPS_ASSERT(sys);
    // write RUN into the keyboard buffer
    uint16_t keybuf = 0x277;
    mem_wr(&sys->mem, keybuf++, 'R');
    mem_wr(&sys->mem, keybuf++, 'U');
    mem_wr(&sys->mem, keybuf++, 'N');
    mem_wr(&sys->mem, keybuf++, 0x0D);
    // write number of characters, this kicks off evaluation
    mem_wr(&sys->mem, 0xC6, 4);
}

void x65_basic_load(x65_t* sys) {
    CHIPS_ASSERT(sys);
    // write LOAD
    uint16_t keybuf = 0x277;
    mem_wr(&sys->mem, keybuf++, 'L');
    mem_wr(&sys->mem, keybuf++, 'O');
    mem_wr(&sys->mem, keybuf++, 'A');
    mem_wr(&sys->mem, keybuf++, 'D');
    mem_wr(&sys->mem, keybuf++, 0x0D);
    // write number of characters, this kicks off evaluation
    mem_wr(&sys->mem, 0xC6, 5);
}

void x65_basic_syscall(x65_t* sys, uint16_t addr) {
    CHIPS_ASSERT(sys);
    // write SYS xxxx[Return] into the keyboard buffer (up to 10 chars)
    uint16_t keybuf = 0x277;
    mem_wr(&sys->mem, keybuf++, 'S');
    mem_wr(&sys->mem, keybuf++, 'Y');
    mem_wr(&sys->mem, keybuf++, 'S');
    mem_wr(&sys->mem, keybuf++, ((addr / 10000) % 10) + '0');
    mem_wr(&sys->mem, keybuf++, ((addr / 1000) % 10) + '0');
    mem_wr(&sys->mem, keybuf++, ((addr / 100) % 10) + '0');
    mem_wr(&sys->mem, keybuf++, ((addr / 10) % 10) + '0');
    mem_wr(&sys->mem, keybuf++, ((addr / 1) % 10) + '0');
    mem_wr(&sys->mem, keybuf++, 0x0D);
    // write number of characters, this kicks off evaluation
    mem_wr(&sys->mem, 0xC6, 9);
}

uint16_t x65_syscall_return_addr(void) {
    return 0xA7EA;
}
