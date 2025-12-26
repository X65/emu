#include "./x65.h"
#include "../args.h"
#include "../log.h"

#include "chips/clk.h"

#include <stdlib.h>
#include <string.h>  // memcpy, memset

#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

// declare function to sync RAM writes to CGIA L1 cache
void cgia_ram_write(uint8_t bank, uint16_t addr, uint8_t data);

static uint8_t _x65_vpu_fetch(uint32_t addr, void* user_data);
static void _x65_api_call(uint8_t data, void* user_data);

#define _X65_DEFAULT(val, def) (((val) != 0) ? (val) : (def))

void x65_init(x65_t* sys, const x65_desc_t* desc) {
    CHIPS_ASSERT(sys && desc);
    if (desc->debug.callback.func) {
        CHIPS_ASSERT(desc->debug.stopped);
    }

    memset(sys, 0, sizeof(x65_t));
    if (!arguments.zeromem)
        for (int i = 0; i < X65_RAM_SIZE_BYTES; i++) {
            sys->ram[i] = rand() & 0xFF;  // fill RAM with random data
        }

    sys->valid = true;
    sys->running = false;
    sys->joystick_type = desc->joystick_type;
    sys->debug = desc->debug;
    sys->audio.callback = desc->audio.callback;
    sys->audio.num_samples = _X65_DEFAULT(desc->audio.num_samples, X65_DEFAULT_AUDIO_SAMPLES);
    CHIPS_ASSERT(sys->audio.num_samples <= X65_MAX_AUDIO_SAMPLES);

    // initialize the hardware
    sys->pins = w65816_init(&sys->cpu, &(w65816_desc_t){});
    ria816_init(
        &sys->ria,
        &(ria816_desc_t){
            .tick_hz = X65_FREQUENCY,
            .api_cb = _x65_api_call,
            .user_data = sys,
        });
    tca6416a_init(&sys->gpio, 0xff, 0xff);
    cgia_init(&sys->cgia, &(cgia_desc_t){
        .tick_hz = X65_FREQUENCY,
        .fetch_cb = _x65_vpu_fetch,
        .user_data = sys,
        .framebuffer = {
            .ptr = sys->fb,
            .size = sizeof(sys->fb),
        },
    });
    m6581_init(
        &sys->sid,
        &(m6581_desc_t){
            .tick_hz = X65_FREQUENCY,
            .sound_hz = _X65_DEFAULT(desc->audio.sample_rate, 44100),
            .magnitude = _X65_DEFAULT(desc->audio.volume, 1.0f),
        });
    beeper_init(
        &sys->beeper,
        &(beeper_desc_t){
            .tick_hz = X65_FREQUENCY,
            .sound_hz = _X65_DEFAULT(desc->audio.sample_rate, 44100),
            .base_volume = _X65_DEFAULT(desc->audio.volume, 1.0f),
        });
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
    ria816_reset(&sys->ria);
    tca6416a_reset(&sys->gpio, 0xff, 0xff);
    cgia_reset(&sys->cgia);
    m6581_reset(&sys->sid);
    beeper_reset(&sys->beeper);
}

void x65_set_running(x65_t* sys, bool running) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->running = running;
}

static uint64_t _x65_tick(x65_t* sys, uint64_t pins) {
    if (!sys->running) {
        // keep CPU in RESET state
        pins |= W65816_RES;
    }

    // tick the CPU
    pins = w65816_tick(&sys->cpu, pins);
    const uint32_t addr = W65816_GET_ADDR(pins) & 0xFFFFFF;

    // those pins are set each tick by the CIAs and VIC
    pins &= ~(W65816_IRQ | W65816_NMI | W65816_RDY);

    /*  address decoding

        When the RDY pin is active (during bad lines), no CPU/chip
        communication takes place starting with the first read access.
    */
    bool mem_access = false;
    uint64_t cgia_pins = pins & W65816_PIN_MASK;
    uint64_t ria_pins = pins & W65816_PIN_MASK;
    uint64_t gpio_pins = pins & W65816_PIN_MASK;
    uint64_t sid_pins = pins & W65816_PIN_MASK;
    if ((pins & (W65816_RDY | W65816_RW)) != (W65816_RDY | W65816_RW)) {
        if (sys->ria.reg[RIA816_EXT_IO] && ((addr & 0xFF00) == X65_EXT_BASE)) {
            const uint8_t slot = (addr & 0xFF) >> 5;
            if ((sys->ria.reg[RIA816_EXT_IO] & (1U << slot))) switch (slot) {
                    case 0x00: {
                        // OPL-3 (FC00..FC1F)
                        // opl3_pins |= YMF262_CS;
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
            else if (addr >= 0xFFA0) {
                // NOT_USED (FFA0..FFBF)
            }
            else if (addr >= X65_IO_TIMERS_BASE) {
                // GPIO (FF98..FF9F)
                ria_pins |= RIA816_TIMERS_CS;
            }
            else if (addr >= X65_IO_GPIO_BASE) {
                // GPIO (FF80..FF97)
                gpio_pins |= TCA6416A_CS;
            }
            else if (addr >= X65_IO_CGIA_BASE) {
                // CGIA (FF00..FF7F)
                cgia_pins |= CGIA_CS;
            }
            else if (addr >= X65_IO_XCSP_BASE) {
                // SID (FEC0..FEFF)
                sid_pins |= M6581_CS;
            }
            else if (addr >= X65_IO_MIXER_BASE) {
                // MIXER (FEB0..FEBF)
                // FIXME: MIXER_CS;
            }
        }
        else {
            mem_access = true;
        }
    }

    /* tick GPIO extender chip:
        In Port 0:
            joystick 1 input
        In Port 1:
            joystick 2 input

        INT pin is connected to the Interrupt Controller pin 1
    */
    {
        const uint8_t p0 = ~(sys->kbd_joy1_mask | sys->joy_joy1_mask);
        const uint8_t p1 = ~(sys->kbd_joy2_mask | sys->joy_joy2_mask);
        TCA6416A_SET_P01(gpio_pins, p0, p1);
        gpio_pins = tca6416a_tick(&sys->gpio, gpio_pins);
        if (gpio_pins & TCA6416A_INT) {
            sys->ria.int_status |= X65_INT_GPIO;
        }
        else {
            sys->ria.int_status &= ~X65_INT_GPIO;
        }
        if ((gpio_pins & (TCA6416A_CS | TCA6416A_RW)) == (TCA6416A_CS | TCA6416A_RW)) {
            pins = W65816_COPY_DATA(pins, gpio_pins);
        }
    }

    /* tick RIA816:
     */
    {
        ria_pins = ria816_tick(&sys->ria, ria_pins);
        if ((ria_pins & (RIA816_CS | RIA816_RW)) == (RIA816_CS | RIA816_RW)) {
            pins = W65816_COPY_DATA(pins, ria_pins);
        }
        if (ria_pins & RIA816_IRQ) {
            sys->ria.int_status |= X65_INT_RIA;
        }
        else {
            sys->ria.int_status &= ~X65_INT_RIA;
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
            pins = W65816_COPY_DATA(pins, sid_pins);
        }
    }

    /* NAND gate in interrupt "controller"
     */
    {
        if (sys->ria.int_status) {
            pins |= W65816_IRQ;
        }
    }

    /* remaining CPU IO and memory accesses, those don't fit into the
       "universal tick model" (yet?)
    */
    if (mem_access) {
        if (pins & W65816_RW) {
            // memory read
            W65816_SET_DATA(pins, mem_ram_read(sys, addr));
        }
        else {
            // memory write
            mem_ram_write(sys, addr, W65816_GET_DATA(pins));
        }
    }
    return pins;
}

uint8_t mem_rd(x65_t* sys, uint8_t bank, uint16_t addr) {
    if (bank == 0) {
        if (addr >= 0xFFC0) {
            return sys->ria.reg[addr & 0x3F];
        }
        // else if (addr >= 0xFEC0 && addr < 0xFF00) {
        //     return sys->sd1.registers[addr & YMF825_ADDR_MASK];
        // }
        else if (addr >= 0xFF00 && addr < 0xFF80) {
            return cgia_reg_read((uint8_t)addr);
        }
    }
    return sys->ram[(bank << 16) | addr];
}
void mem_wr(x65_t* sys, uint8_t bank, uint16_t addr, uint8_t data) {
    if (bank == 0) {
        if (addr >= 0xFFC0) {
            sys->ria.reg[addr & 0x3F] = data;
            return;
        }
        // else if (addr >= 0xFF80 && addr < 0xFF88) {
        //     sys->sd1.registers[addr & YMF825_ADDR_MASK] = data;
        //     return;
        // }
        else if (addr >= 0xFF00 && addr < 0xFF80) {
            cgia_reg_write((uint8_t)addr, data);
            return;
        }
    }
    // else
    mem_ram_write(sys, (bank << 16) | addr, data);
}

void mem_ram_write(x65_t* sys, uint32_t addr, uint8_t data) {
    sys->ram[addr] = data;
    cgia_ram_write((uint8_t)(addr >> 16), (uint16_t)addr, data);
}

uint8_t mem_ram_read(x65_t* sys, uint32_t addr) {
    return sys->ram[addr];
}

uint8_t _x65_vpu_fetch(uint32_t addr, void* user_data) {
    x65_t* sys = (x65_t*)user_data;
    return sys->ram[addr & 0xFFFFFF];
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
    return num_ticks;
}

void x65_key_down(x65_t* sys, int key_code) {
    CHIPS_ASSERT(sys && sys->valid);
    uint8_t m = 0;
    switch (key_code) {
        case 0x20: m = X65_JOYSTICK_BTN; break;    // SPACE
        case 0x08: m = X65_JOYSTICK_LEFT; break;   // CURSOR_LEFT
        case 0x09: m = X65_JOYSTICK_RIGHT; break;  // CURSOR_RIGHT
        case 0x0A: m = X65_JOYSTICK_DOWN; break;   // CURSOR_DOWN
        case 0x0B: m = X65_JOYSTICK_UP; break;     // CURSOR_UP
        default: break;
    }
    if (m != 0) {
        switch (sys->joystick_type) {
            case X65_JOYSTICKTYPE_DIGITAL_1: sys->kbd_joy1_mask |= m; break;
            case X65_JOYSTICKTYPE_DIGITAL_2: sys->kbd_joy2_mask |= m; break;
            case X65_JOYSTICKTYPE_DIGITAL_12:
                sys->kbd_joy1_mask |= m;
                sys->kbd_joy2_mask |= m;
                break;
            case X65_JOYSTICKTYPE_NONE:
            default: break;
        }
    }
}

void x65_key_up(x65_t* sys, int key_code) {
    CHIPS_ASSERT(sys && sys->valid);
    uint8_t m = 0;
    switch (key_code) {
        case 0x20: m = X65_JOYSTICK_BTN; break;
        case 0x08: m = X65_JOYSTICK_LEFT; break;
        case 0x09: m = X65_JOYSTICK_RIGHT; break;
        case 0x0A: m = X65_JOYSTICK_DOWN; break;
        case 0x0B: m = X65_JOYSTICK_UP; break;
        default: break;
    }
    if (m != 0) {
        switch (sys->joystick_type) {
            case X65_JOYSTICKTYPE_DIGITAL_1: sys->kbd_joy1_mask &= ~m; break;
            case X65_JOYSTICKTYPE_DIGITAL_2: sys->kbd_joy2_mask &= ~m; break;
            case X65_JOYSTICKTYPE_DIGITAL_12:
                sys->kbd_joy1_mask &= ~m;
                sys->kbd_joy2_mask &= ~m;
                break;
            case X65_JOYSTICKTYPE_NONE:
            default: break;
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

bool x65_quickload_xex(x65_t* sys, chips_range_t data) {
    CHIPS_ASSERT(sys && sys->valid && data.ptr);
    if (data.size < 2) {
        LOG_ERROR("File too small");
        return false;
    }
    const uint8_t* ptr = (uint8_t*)data.ptr;

    bool reset_lo_loaded = false;
    bool reset_hi_loaded = false;

    uint8_t load_bank = 0x00;

    // $FFFF is required in first block
    if (ptr[0] != 0xff || ptr[1] != 0xff) {
        LOG_ERROR("Missing $FFFF header");
        return false;
    }

    while (ptr < ((uint8_t*)data.ptr + data.size)) {
        size_t data_left = (uint8_t*)data.ptr + data.size - ptr;
        if (data_left < 4) {
            LOG_ERROR("File truncated");
            return false;
        }
        if (ptr[0] == 0xff && ptr[1] == 0xff) {
            // skip header
            ptr += 2;
            data_left -= 2;
            if (data_left < 4) {
                LOG_ERROR("File truncated");
                return false;
            }
        }

        const uint16_t start_addr = ptr[1] << 8 | ptr[0];
        ptr += 2;
        const uint16_t end_addr = ptr[1] << 8 | ptr[0];
        ptr += 2;
        LOG_INFO("Loading block: $%04X-$%04X", start_addr, end_addr);

        data_left = (uint8_t*)data.ptr + data.size - ptr;
        if (data_left < (end_addr - start_addr + 1) || start_addr > end_addr) {
            LOG_ERROR("Block truncated");
            return false;
        }
        if (start_addr == end_addr && start_addr == 0xFFFE) {
            load_bank = *ptr++;
            LOG_INFO("Loading to bank: $%02X", load_bank);
        }
        else {
            uint16_t addr = start_addr;
            while (addr <= end_addr && addr >= start_addr) {
                if (addr == 0xfffc) reset_lo_loaded = true;
                if (addr == 0xfffd) reset_hi_loaded = true;
                mem_wr(sys, load_bank, addr++, *ptr++);
            }
        }
    }

    if (reset_lo_loaded && reset_hi_loaded) {
        LOG_INFO("Reset vector set - running")
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
    *sys = im;
    return true;
}

#include "api/api.h"
#include "sys/cpu.h"
#include "term/font.h"

volatile uint8_t __regs[0x40];
uint8_t xstack[XSTACK_SIZE + 1];
size_t volatile xstack_ptr;

static bool api_stack_pop_uint16(x65_t* sys, uint16_t* value) {
    return rb_get(&sys->ria.api_stack, &((uint8_t*)value)[0]) && rb_get(&sys->ria.api_stack, &((uint8_t*)value)[1]);
}
static bool api_stack_push_uint16(x65_t* sys, uint16_t value) {
    return rb_put(&sys->ria.api_stack, ((uint8_t*)&value)[1]) && rb_put(&sys->ria.api_stack, ((uint8_t*)&value)[0]);
}

void _x65_api_call(uint8_t data, void* user_data) {
    x65_t* sys = (x65_t*)user_data;

    // sync RIA regs
    memcpy(__regs, sys->ria.reg, sizeof(__regs));

    switch (data) {
        case API_OP_ZXSTACK: {
            rb_init(&sys->ria.api_stack);
        } break;
        case API_OP_PHI2: {
            api_stack_push_uint16(sys, CPU_PHI2_DEFAULT);
        } break;
        case API_OP_OEM_GET_CHARGEN: {
            uint8_t value;
            if (!rb_get(&sys->ria.api_stack, &value)) break;
            uint32_t mem_addr = (uint32_t)value;
            if (!rb_get(&sys->ria.api_stack, &value)) break;
            mem_addr |= (uint32_t)value << 8;
            if (!rb_get(&sys->ria.api_stack, &value)) break;
            mem_addr |= (uint32_t)value << 16;
            if (!rb_get(&sys->ria.api_stack, &value)) break;
            uint16_t code_page = (uint16_t)value;
            if (!rb_get(&sys->ria.api_stack, &value)) break;
            code_page |= (uint16_t)value << 8;

            // copy chargen to memory
            for (size_t i = 0; i < 256 * 8; ++i) {
                mem_ram_write(sys, mem_addr++, font_get_byte(i, code_page));
            }
        } break;
        case API_OP_HALT:  // STOP CPU
            sys->running = false;
            break;
        default: fprintf(stderr, "Unhandled RIA API call: %02x\n", data);
    }

    // sync RIA regs back
    memcpy(sys->ria.reg, __regs, sizeof(__regs));
}

#define RP6502_CODE_PAGE 0
#include "firmware/src/south/term/font.c"
