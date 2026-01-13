#define CHIPS_IMPL
#include "chips/m6526.h"
#undef CHIPS_IMPL

#include "ria816.h"
#include "sys/mem.h"

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
        case RIA816_MATH_MULAB + 1: {
            uint16_t mul = RIA816_REG16(c->reg, RIA816_MATH_OPERA) * RIA816_REG16(c->reg, RIA816_MATH_OPERB);
            data = (addr == RIA816_MATH_MULAB) ? (mul & 0xFF) : (mul >> 8);
        } break;
        // division accelerator
        case RIA816_MATH_DIVAB:
        case RIA816_MATH_DIVAB + 1: {
            const int16_t oper_a = RIA816_REG16(c->reg, RIA816_MATH_OPERA);
            const uint16_t oper_b = RIA816_REG16(c->reg, RIA816_MATH_OPERB);
            uint16_t div = oper_b ? (oper_a / oper_b) : 0xFFFF;
            data = (addr == RIA816_MATH_DIVAB) ? (div & 0xFF) : (div >> 8);
        } break;

        // Time Of Day
        case RIA816_TIME_TM:
        case RIA816_TIME_TM + 1:
        case RIA816_TIME_TM + 2:
        case RIA816_TIME_TM + 3:
        case RIA816_TIME_TM + 4:
        case RIA816_TIME_TM + 5: {
            data = ((uint8_t*)&c->us)[addr & 0x07];
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

typedef uint32_t DWORD;
#include "hid/hid.c"
static void kbd_queue_char(char ch) {}
static void kbd_queue_key(uint8_t modifier, uint8_t keycode, bool initial_press) {}
#include "hid/kbd.c"
#include "hid/mou.c"
#include "hid/pad.c"
uint8_t ria816_hid_read(ria816_t* c, uint8_t reg) {
    uint8_t data = 0xFF;  // invalid
    switch (HID_dev & 0xF) {
        case RIA_HID_DEV_KEYBOARD: data = kbd_get_reg((HID_dev & 0xF0) | reg); break;
        case RIA_HID_DEV_MOUSE: data = mou_get_reg(reg); break;
        case RIA_HID_DEV_GAMEPAD: data = pad_get_reg(HID_dev >> 4, reg); break;
    }
    return data;
}
void ria816_hid_write(ria816_t* c, uint8_t reg, uint8_t data) {
    if (reg == 0x00)  // HID SELECT
        HID_dev = data;
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
    }
    if (pins & RIA816_TIMERS_CS) {
        // CIA timers emulation
        uint8_t addr = pins & M6526_RS;
        if (addr < M6526_REG_ICR) addr -= 4;
        if (pins & M6526_RW) {
            uint8_t data = m6526_read(&c->cia, addr);
            M6526_SET_DATA(pins, data);
        }
        else {
            uint8_t data = M6526_GET_DATA(pins);
            _m6526_write(&c->cia, addr, data);
        }
    }

    pins = _ria816_update_irq(c, pins);

    c->pins = pins;
    return pins;
}
