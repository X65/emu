#include <stdio.h>
#define CHIPS_IMPL
#include "chips/m6526.h"
#undef CHIPS_IMPL

#include "ria816.h"
#include "sys/mem.h"
#include <math.h>

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

// fixed point precision for more precise error accumulation
#define RIA816_FIXEDPOINT_SCALE (256)

void ria816_init(ria816_t* c, const ria816_desc_t* desc) {
    CHIPS_ASSERT(c);
    memset(c, 0, sizeof(*c));
    rb_init(&c->uart_rx);
    rb_init(&c->uart_tx);

    m6526_init(&c->cia);

    c->ticks_per_ms = desc->tick_hz * RIA816_FIXEDPOINT_SCALE / 1000000;

    c->api_cb = desc->api_cb;
    c->user_data = desc->user_data;

    // Seed the random number generator
    srand((unsigned int)time(NULL));
}

void ria816_reset(ria816_t* c) {
    CHIPS_ASSERT(c);
    c->pins = 0;
    c->us = 0;
    rb_init(&c->uart_rx);
    rb_init(&c->uart_tx);
    m6526_reset(&c->cia);
}

static uint64_t _ria816_tick(ria816_t* c, uint64_t pins) {
    c->ticks_counter += RIA816_FIXEDPOINT_SCALE;
    if (c->ticks_counter >= c->ticks_per_ms) {
        c->us += 1;
        c->ticks_counter -= c->ticks_per_ms;

        c->cia.pins = _m6526_tick(&c->cia, pins);
    }
    return pins;
}

// The X65 window exposes only timers and interrupt control, not CIA ports.
static uint8_t _ria816_timer_register(uint8_t offset) {
    offset &= RIA816_TIMERS_RS;
    return offset < 4 ? M6526_REG_TALO + offset : 8 + offset;
}

static uint8_t _ria816_timers_read(ria816_t* ria, uint8_t offset) {
    return m6526_read(&ria->cia, _ria816_timer_register(offset));
}

uint8_t ria816_timers_peek(const ria816_t* ria, uint8_t offset) {
    return m6526_peek(&ria->cia, _ria816_timer_register(offset));
}

void ria816_timers_write(ria816_t* ria, uint8_t offset, uint8_t data) {
    _m6526_write(&ria->cia, _ria816_timer_register(offset), data);
}

uint8_t ria816_uart_status(const ria816_t* c) {
    uint8_t data = 0;
    if (rb_is_empty(&c->uart_rx))
        data &= ~0b01000000;
    else
        data |= 0b01000000;
    if (rb_is_full(&c->uart_tx))
        data &= ~0b10000000;
    else
        data |= 0b10000000;
    return data;
}

uint8_t ria816_reg_read(ria816_t* c, uint8_t addr) {
    uint8_t data = 0xFF;

    switch (addr) {
        // multiplication accelerator
        case RIA816_MATH_MULAB:
        case RIA816_MATH_MULAB + 1:
        case RIA816_MATH_MULAB + 2:
        case RIA816_MATH_MULAB + 3: {
            uint32_t mul = RIA816_REG16(c->reg, RIA816_MATH_OPERA) * RIA816_REG16(c->reg, RIA816_MATH_OPERB);
            data = ((uint8_t*)&mul)[addr & 0x03];
        } break;
        // division accelerator
        case RIA816_MATH_DIVAB:
        case RIA816_MATH_DIVAB + 1: {
            const int16_t oper_a = RIA816_REG16(c->reg, RIA816_MATH_OPERA);
            const uint16_t oper_b = RIA816_REG16(c->reg, RIA816_MATH_OPERB);
            uint16_t div = oper_b ? (oper_a / oper_b) : 0xFFFF;
            data = ((uint8_t*)&div)[addr & 0x01];
        } break;

        // Time Of Day
        case RIA816_TIME_TM:
        case RIA816_TIME_TM + 1:
        case RIA816_TIME_TM + 2:
        case RIA816_TIME_TM + 3:
        case RIA816_TIME_TM + 4:
        case RIA816_TIME_TM + 5: {
            data = ((uint8_t*)&c->us)[(addr - 2) & 0x07];
        } break;

        case RIA816_UART_READY: {
            data = ria816_uart_status(c);
        } break;
        case RIA816_UART_TX_RX: rb_get(&c->uart_rx, &data); break;

        case RIA816_HW_RNG:
        case RIA816_HW_RNG + 1: data = (uint8_t)rand(); break;

        case RIA816_IRQ_ENABLE: data = c->irq_enable; break;
        case RIA816_IRQ_STATUS: data = ~(c->int_status); break;

        case RIA816_API_STACK:
            data = xstack[xstack_ptr];
            if (xstack_ptr < XSTACK_SIZE) ++xstack_ptr;
            break;

        default: data = c->reg[addr];
    }
    return data;
}

void ria816_reg_write(ria816_t* c, uint8_t addr, uint8_t data) {
    switch (addr) {
        case RIA816_UART_TX_RX: rb_put(&c->uart_tx, data); break;

        case RIA816_IRQ_STATUS: break;
        case RIA816_IRQ_ENABLE: c->irq_enable = (data && 0x01); break;

        case RIA816_API_STACK:
            if (xstack_ptr) xstack[--xstack_ptr] = data;
            break;
        case RIA816_API_OP_RET:
            if (c->api_cb) c->api_cb(data, c->user_data);
            break;

        default: c->reg[addr] = data;
    }
}

static uint64_t _ria816_update_irq(ria816_t* c, uint64_t pins) {
    pins &= ~RIA816_IRQ;

    if (c->cia.pins & M6526_IRQ && c->irq_enable & RIA816_INT_CIA_MASK) {
        pins |= RIA816_IRQ;
    }
    return pins;
}

#include "sys/ria.h"

static uint8_t HID_dev = 0;

#ifdef NDEBUG
static inline void DBG(const char* fmt, ...) {
    (void)fmt;
}
#else
    #include "log.h"
    #define DBG(...) LOG_INFO(__VA_ARGS__)
#endif
typedef uint32_t DWORD;
#include "hid/hid.c"
#include "hid/kbd.c"
#include "hid/mou.c"
#include "hid/pad.c"
#include <SDL3/SDL.h>

static void pad_synth_report(pad_connection_t* conn, void* data, uint16_t event_type, pad_xram_t* report) {
    DBG("Type: 0x%X - %p, slot: %d", event_type, data, conn->slot);

    uint8_t dpad = 0;
    uint8_t button0 = 0;
    uint8_t button1 = 0;

    if (event_type >= SDL_EVENT_JOYSTICK_AXIS_MOTION && event_type <= SDL_EVENT_JOYSTICK_UPDATE_COMPLETE) {
        // Joystick event

        report->lx = SDL_GetJoystickAxis(data, 0) / 256;
        report->ly = SDL_GetJoystickAxis(data, 1) / 256;
        report->rx = SDL_GetJoystickAxis(data, 2) / 256;
        report->ry = SDL_GetJoystickAxis(data, 3) / 256;
        report->lt = SDL_GetJoystickAxis(data, 4) / 256;
        report->rt = SDL_GetJoystickAxis(data, 5) / 256;

        uint8_t hat = SDL_GetJoystickHat(data, 0);
        if (hat & SDL_HAT_UP) dpad |= 1;
        if (hat & SDL_HAT_DOWN) dpad |= 2;
        if (hat & SDL_HAT_LEFT) dpad |= 4;
        if (hat & SDL_HAT_RIGHT) dpad |= 8;

        uint32_t buttons = 0;
        for (int i = 0; i < PAD_MAX_BUTTONS; i++) {
            if (SDL_GetJoystickButton(data, conn->button_offsets[i])) {
                buttons |= (1 << i);
            }
        }
        button0 = buttons & 0xFF;
        button1 = (buttons >> 8) & 0xFF;
    }
    else if (event_type >= SDL_EVENT_GAMEPAD_AXIS_MOTION && event_type <= SDL_EVENT_GAMEPAD_UPDATE_COMPLETE) {
        // Gamepad event

        report->lx = SDL_GetGamepadAxis(data, SDL_GAMEPAD_AXIS_LEFTX) / 256;
        report->ly = SDL_GetGamepadAxis(data, SDL_GAMEPAD_AXIS_LEFTY) / 256;
        report->rx = SDL_GetGamepadAxis(data, SDL_GAMEPAD_AXIS_RIGHTX) / 256;
        report->ry = SDL_GetGamepadAxis(data, SDL_GAMEPAD_AXIS_RIGHTY) / 256;
        report->lt = SDL_GetGamepadAxis(data, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 256;
        report->rt = SDL_GetGamepadAxis(data, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 256;

        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_DPAD_UP)) dpad |= (1 << 0);
        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) dpad |= (1 << 1);
        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) dpad |= (1 << 2);
        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) dpad |= (1 << 3);

        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_SOUTH)) button0 |= (1 << 0);           // A
        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_EAST)) button0 |= (1 << 1);            // B
        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1)) button0 |= (1 << 2);   // C
        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_WEST)) button0 |= (1 << 3);            // X
        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_NORTH)) button0 |= (1 << 4);           // Y
        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1)) button0 |= (1 << 5);    // Z
        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) button0 |= (1 << 6);   // L1
        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) button0 |= (1 << 7);  // R1

        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_BACK)) button1 |= (1 << 2);
        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_START)) button1 |= (1 << 3);
        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_GUIDE)) button1 |= (1 << 4);
        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_LEFT_STICK)) button1 |= (1 << 5);   // L3
        if (SDL_GetGamepadButton(data, SDL_GAMEPAD_BUTTON_RIGHT_STICK)) button1 |= (1 << 6);  // R3
    }

    report->dpad |= dpad & 0x0F;  // only lower 4 bits are dpad
    report->button0 = button0;
    report->button1 = button1;
    DBG("\t%s: 0x%02X, Sticks: 0x%02X, Buttons: 0x%02X 0x%02X, Sticks: L(%d,%d) R(%d,%d), Triggers: L(%d) R(%d)",
        SDL_IsGamepad(SDL_GetGamepadID(data)) ? "Gamepad" : "Joystick",
        report->dpad,
        report->sticks,
        report->button0,
        report->button1,
        report->lx,
        report->ly,
        report->rx,
        report->ry,
        report->lt,
        report->rt);
}

// --- scripted gamepads ---------------------------------------------------
//
// Reports pushed in by ria816_pad_inject() shadow the corresponding SDL
// device.  Selector index 0 is the firmware's merged view, so it has to merge
// the injected pads too, following pad_get_reg()'s rule: only pads whose
// connected flag is set contribute.

#define PAD_CONNECTED_BIT 0x80

static uint8_t pad_inject_regs[RIA816_PAD_SLOTS][RIA816_PAD_REGS];
static uint8_t pad_inject_mask;  // bit n set: slot n is scripted

void ria816_pad_inject(uint8_t pad, const uint8_t report[RIA816_PAD_REGS]) {
    if (pad < 1 || pad > RIA816_PAD_SLOTS) return;
    memcpy(pad_inject_regs[pad - 1], report, RIA816_PAD_REGS);
    pad_inject_regs[pad - 1][0] |= PAD_CONNECTED_BIT;
    pad_inject_mask |= 1u << (pad - 1);
}

void ria816_pad_release(uint8_t pad) {
    if (pad < 1 || pad > RIA816_PAD_SLOTS) return;
    pad_inject_mask &= ~(1u << (pad - 1));
}

bool ria816_pad_injected(uint8_t pad) {
    if (pad < 1 || pad > RIA816_PAD_SLOTS) return false;
    return (pad_inject_mask & (1u << (pad - 1))) != 0;
}

// --- scripted keyboard ---------------------------------------------------
//
// The keyboard register file is a 256-bit map of held keys, one bit per USB
// HID usage id: byte `keycode >> 3`, bit `keycode & 7`.  Injected keys are
// merged with the real ones rather than shadowing them.

static uint8_t kbd_inject[RIA816_KBD_BYTES];

void ria816_key_set(uint8_t keycode) {
    kbd_inject[keycode >> 3] |= (uint8_t)(1u << (keycode & 7));
}

void ria816_keys_clear(void) {
    memset(kbd_inject, 0, sizeof(kbd_inject));
}

static uint8_t _ria816_kbd_get_reg(uint8_t idx) {
    uint8_t data = kbd_get_reg(idx);
    if (idx < RIA816_KBD_BYTES) data |= kbd_inject[idx];
    return data;
}

static uint8_t _ria816_pad_get_reg(uint8_t pad, uint8_t reg) {
    if (reg >= RIA816_PAD_REGS) return pad_get_reg(pad, reg);
    if (pad >= 1 && pad <= RIA816_PAD_SLOTS && (pad_inject_mask & (1u << (pad - 1))))
        return pad_inject_regs[pad - 1][reg];
    if (pad != 0 || !pad_inject_mask) return pad_get_reg(pad, reg);

    // Merged view with at least one scripted pad in the mix.
    uint8_t merged = 0;
    for (uint8_t slot = 1; slot <= RIA816_PAD_SLOTS; ++slot) {
        uint8_t connected = (pad_inject_mask & (1u << (slot - 1)))
                                ? pad_inject_regs[slot - 1][0]
                                : pad_get_reg(slot, 0);
        if (connected & PAD_CONNECTED_BIT) merged |= _ria816_pad_get_reg(slot, reg);
    }
    return merged;
}

// --- pad state, for the status display -----------------------------------
//
// The HID registers are selector-driven, so reading them the CPU's way would
// disturb the guest's selection.  These go straight at the merged state.

uint8_t ria816_pad_count(void) {
    uint8_t count = 0;
    for (uint8_t slot = 1; slot <= RIA816_PAD_SLOTS; ++slot) {
        if (_ria816_pad_get_reg(slot, 0) & PAD_CONNECTED_BIT) count++;
    }
    return count;
}

uint8_t ria816_pad_read(uint8_t pad, uint8_t reg) {
    return _ria816_pad_get_reg(pad, reg);
}

uint8_t ria816_hid_read(ria816_t* c, uint8_t reg) {
    uint8_t data = 0xFF;  // invalid
    switch (HID_dev & 0xF) {
        case RIA_HID_DEV_KEYBOARD: data = _ria816_kbd_get_reg((HID_dev & 0xF0) | reg); break;
        case RIA_HID_DEV_MOUSE: data = mou_get_reg(reg); break;
        case RIA_HID_DEV_GAMEPAD: data = _ria816_pad_get_reg(HID_dev >> 4, reg); break;
    }
    return data;
}
void ria816_hid_write(ria816_t* c, uint8_t reg, uint8_t data) {
    if (reg == 0x00)  // HID SELECT
        HID_dev = data;
}

uint8_t ria816_hid_dev(const ria816_t* c) {
    return HID_dev;
}

#include "south/sys/led.c"
static uint8_t RGB_regs[8] = { 0 };

uint8_t ria816_rgb_read(ria816_t* c, uint8_t reg) {
    return RGB_regs[reg & 0x07];
}
void ria816_rgb_write(ria816_t* c, uint8_t reg, uint8_t data) {
    RGB_regs[reg] = data & 0x07;

    RGB_regs[reg] = data;

    switch (reg) {
        case 0:
        case 1:
        case 2:
        case 3:  // RGB332 LED set
            led_set_pixel_rgb332(reg, data);
            break;
        case 4:  // RGB888 LED set
            led_set_pixel(data, RGB_regs[5], RGB_regs[6], RGB_regs[7]);
            break;
    }
}

// Buzzer registers (4 total)
// FREQ_LO ($FFA8), FREQ_HI ($FFA9), DUTY ($FFAA), UNUSED ($FFAB)
static uint8_t buzzer_regs[4] = { 0 };

uint8_t ria816_buzzer_read(ria816_t* c, uint8_t reg) {
    return buzzer_regs[reg & 0x03];
}
void ria816_buzzer_write(ria816_t* c, uint8_t reg, uint8_t data) {
    buzzer_regs[reg & 0x03] = data;
}

bool ria816_buzzer_tick(ria816_t* c) {
    uint8_t duty = buzzer_regs[2];
    uint16_t freq16 = (uint16_t)((buzzer_regs[1] << 8) | buzzer_regs[0]);

    // Check if we need to recompute period due to change in frequency or duty
    if ((freq16 != c->buzzer_freq) || (duty != c->buzzer_duty)) {
        // Calculate frequency in Hz using the same formula as firmware
        // f = 20 * 2^(OCTAVES * freq16 / 65535)
        // OCTAVES ≈ 9.965784285
        const double OCTAVES = 9.965784285;
        double f = 20.0 * pow(2.0, OCTAVES * (double)freq16 / 65535.0);

        // Convert to fixed-point period
        uint32_t tick_hz = (uint32_t)c->ticks_per_ms * 1000000 / RIA816_FIXEDPOINT_SCALE;
        c->buzzer_period = (uint32_t)((double)tick_hz * (double)RIA816_FIXEDPOINT_SCALE / f);

        c->buzzer_freq = freq16;
        c->buzzer_duty = duty;
    }

    // Advance phase by 1 tick in fixed-point units
    c->buzzer_phase += RIA816_FIXEDPOINT_SCALE;

    // Wrap phase if needed
    if (c->buzzer_phase >= c->buzzer_period) {
        c->buzzer_phase -= c->buzzer_period;
    }

    // Calculate duty cycle threshold (in fixed-point units)
    // duty is 0-255, so duty * period / 256 gives us the on-time
    uint32_t duty_threshold = (c->buzzer_period * duty) >> 8;

    // Return true if we're in the "on" portion of the period
    return (c->buzzer_phase < duty_threshold);
}

void ria816_rgb_get_leds(uint32_t** leds, size_t* leds_no) {
    *leds = RGB_LEDS;
    *leds_no = led_used_no;
}

uint64_t ria816_tick(ria816_t* c, uint64_t pins) {
    pins = _ria816_tick(c, pins);
    if (pins & RIA816_CS) {
        uint8_t addr = pins & RIA816_RS;
        if (pins & RIA816_RW) {
            uint8_t data = ria816_reg_read(c, addr);
            RIA816_SET_DATA(pins, data);
        }
        else {
            uint8_t data = RIA816_GET_DATA(pins);
            ria816_reg_write(c, addr, data);
        }
    }
    if (pins & RIA816_HID_CS) {
        // HID devices
        const uint8_t addr = pins & RIA816_HID_RS;
        if (pins & M6526_RW) {
            uint8_t data = ria816_hid_read(c, addr);
            RIA816_SET_DATA(pins, data);
        }
        else {
            uint8_t data = RIA816_GET_DATA(pins);
            ria816_hid_write(c, addr, data);
        }
        pins |= RIA816_CS;  // signal data merge to main loop
    }
    if (pins & RIA816_BUZZER_CS) {
        // BUZZER
        const uint8_t addr = pins & RIA816_BUZZER_RS;
        if (pins & M6526_RW) {
            uint8_t data = ria816_buzzer_read(c, addr);
            RIA816_SET_DATA(pins, data);
        }
        else {
            uint8_t data = RIA816_GET_DATA(pins);
            ria816_buzzer_write(c, addr, data);
        }
        pins |= RIA816_CS;  // signal data merge to main loop
    }
    if (pins & RIA816_RGB_CS) {
        // RGB LEDs
        const uint8_t addr = pins & RIA816_RGB_RS;
        if (pins & M6526_RW) {
            uint8_t data = ria816_rgb_read(c, addr);
            RIA816_SET_DATA(pins, data);
        }
        else {
            uint8_t data = RIA816_GET_DATA(pins);
            ria816_rgb_write(c, addr, data);
        }
        pins |= RIA816_CS;  // signal data merge to main loop
    }
    if (pins & RIA816_TIMERS_CS) {
        // CIA timers emulation
        const uint8_t offset = pins & RIA816_TIMERS_RS;
        if (pins & M6526_RW) {
            uint8_t data = _ria816_timers_read(c, offset);
            M6526_SET_DATA(pins, data);
        }
        else {
            uint8_t data = M6526_GET_DATA(pins);
            ria816_timers_write(c, offset, data);
        }
        pins |= RIA816_CS;  // signal data merge to main loop
    }

    pins = _ria816_update_irq(c, pins);

    c->pins = pins;
    return pins;
}
