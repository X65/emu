#include "x65.h"
#include "chips/clk.h"

#include <string.h>  // memcpy, memset

#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

#define _X65_SCREEN_WIDTH  (CGIA_FRAMEBUFFER_WIDTH)
#define _X65_SCREEN_HEIGHT (CGIA_FRAMEBUFFER_HEIGHT)
#define _X65_SCREEN_X      (64)
#define _X65_SCREEN_Y      (24)

static uint8_t _x65_cgia_fetch(uint32_t addr, void* user_data);
static void _x65_init_key_map(x65_t* sys);

#define _X65_DEFAULT(val, def) (((val) != 0) ? (val) : (def))

void x65_init(x65_t* sys, const x65_desc_t* desc) {
    CHIPS_ASSERT(sys && desc);
    if (desc->debug.callback.func) {
        CHIPS_ASSERT(desc->debug.stopped);
    }

    memset(sys, 0, sizeof(x65_t));
    sys->valid = true;
    sys->joystick_type = desc->joystick_type;
    sys->debug = desc->debug;
    sys->audio.callback = desc->audio.callback;
    sys->audio.num_samples = _X65_DEFAULT(desc->audio.num_samples, X65_DEFAULT_AUDIO_SAMPLES);
    CHIPS_ASSERT(sys->audio.num_samples <= X65_MAX_AUDIO_SAMPLES);

    sys->pins = m6502_init(
        &sys->cpu,
        &(m6502_desc_t){
            .bcd_disabled = false,
        });
    cgia_init(
        &sys->cgia,
        &(cgia_desc_t){
            .fetch_cb = _x65_cgia_fetch,
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
    ymf825_init(
        &sys->sd1,
        &(ymf825_desc_t){
            .tick_hz = X65_FREQUENCY,
            .sound_hz = _X65_DEFAULT(desc->audio.sample_rate, 44100),
            .magnitude = _X65_DEFAULT(desc->audio.volume, 1.0f),
        });
    _x65_init_key_map(sys);
}

void x65_discard(x65_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->valid = false;
}

void x65_reset(x65_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->kbd_joy1_mask = sys->kbd_joy2_mask = 0;
    sys->joy_joy1_mask = sys->joy_joy2_mask = 0;
    sys->pins |= M6502_RES;
    cgia_reset(&sys->cgia);
    ymf825_reset(&sys->sd1);
}

static uint64_t _x65_tick(x65_t* sys, uint64_t pins) {
    // tick the CPU
    pins = m6502_tick(&sys->cpu, pins);
    const uint16_t addr = M6502_GET_ADDR(pins);

    // those pins are set each tick by the CIAs and VIC
    pins &= ~(M6502_IRQ | M6502_NMI | M6502_RDY | M6510_AEC);

    /*  address decoding

        When the RDY pin is active (during bad lines), no CPU/chip
        communication takes place starting with the first read access.
    */
    bool mem_access = false;
    bool ext_access = false;
    uint64_t ria_pins = pins & M6502_PIN_MASK;
    uint64_t cgia_pins = pins & M6502_PIN_MASK;
    uint64_t sd1_pins = pins & M6502_PIN_MASK;
    uint64_t gpio_pins = pins & M6502_PIN_MASK;
    if ((pins & (M6502_RDY | M6502_RW)) != (M6502_RDY | M6502_RW)) {
        if ((addr & 0xFF00) == 0xFF00) {
            if (addr < 0xFF80) {
                ext_access = true;
            }
            else if (addr < 0xFFA0) {
                // SD-1 (FF80..FF9F)
                sd1_pins |= YMF825_CS;
            }
            else if (addr < 0xFFC0) {
                // CGIA (FFA0..FFBF)
                cgia_pins |= CGIA_CS;
            }
            else {
                ria_pins |= RIA816_CS;
            }
        }
        else {
            mem_access = true;
        }
    }

    // tick the SD-1
    {
        sd1_pins = ymf825_tick(&sys->sd1, sd1_pins);
        if (sd1_pins & YMF825_SAMPLE) {
            // new audio sample ready
            sys->audio.sample_buffer[sys->audio.sample_pos++] = sys->sd1.sample;
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
        if ((sd1_pins & (YMF825_CS | YMF825_RW)) == (YMF825_CS | YMF825_RW)) {
            pins = M6502_COPY_DATA(pins, sd1_pins);
        }
    }

    /* tick the CGIA display chip:
        - the CGIA IRQ pin is connected to the CPU IRQ pin and goes
        active when the CGIA requests a rasterline interrupt
        - the CGIA NMI pin is connected to the CPU NMI pin and goes
        active when the CGIA requests a Vertical Blank Interrupt
    */
    {
        cgia_pins = cgia_tick(&sys->cgia, cgia_pins);
        pins |= (cgia_pins & (M6502_IRQ | M6502_RDY | M6510_AEC));
        if ((cgia_pins & (CGIA_CS | CGIA_RW)) == (CGIA_CS | CGIA_RW)) {
            pins = M6502_COPY_DATA(pins, cgia_pins);
        }
    }

    /* remaining CPU IO and memory accesses, those don't fit into the
       "universal tick model" (yet?)
    */
    if (ext_access) {
        // extension bus addressing
        // N/C - do nothing for now
    }
    else if (mem_access) {
        if (pins & M6502_RW) {
            // memory read
            M6502_SET_DATA(pins, sys->ram[addr & 0xFFFFFF]);
        }
        else {
            // memory write
            sys->ram[addr & 0xFFFFFF] = M6502_GET_DATA(pins);
        }
    }
    return pins;
}

static uint8_t _x65_cgia_fetch(uint32_t addr, void* user_data) {
    x65_t* sys = (x65_t*)user_data;
    // CGIA has direct access to RAM (think dual-port RAM)
    return sys->ram[addr & 0xFFFFFF];
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

chips_display_info_t x65_display_info(x65_t* sys) {
    chips_display_info_t res = {
        .frame = {
            .dim = {
                .width = CGIA_FRAMEBUFFER_WIDTH,
                .height = CGIA_FRAMEBUFFER_HEIGHT,
            },
            .bytes_per_pixel = 1,
            .buffer = {
                .ptr = sys ? sys->fb : 0,
                .size = CGIA_FRAMEBUFFER_SIZE_BYTES,
            }
        },
        .palette = cgia_palette(),
    };
    if (sys) {
        res.screen = cgia_screen(&sys->cgia);
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
    cgia_snapshot_onsave(&dst->cgia);
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
    cgia_snapshot_onload(&im.cgia, &sys->cgia);
    *sys = im;
    return true;
}
