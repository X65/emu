#include "ria816.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

void ria816_init(ria816_t* c) {
    CHIPS_ASSERT(c);
    memset(c, 0, sizeof(*c));
    rb_init(&c->uart_rx);
    rb_init(&c->uart_tx);

    // Seed the random number generator
    srand((uint)time(NULL));
}

void ria816_reset(ria816_t* c) {
    CHIPS_ASSERT(c);
    c->pins = 0;
    rb_init(&c->uart_rx);
    rb_init(&c->uart_tx);
}

static uint64_t _ria816_tick(ria816_t* c, uint64_t pins) {
    return pins;
}

#define RIA816_REG16(ADDR) (uint16_t)(c->reg[ADDR] | (c->reg[ADDR + 1] << 8))

static uint8_t _ria816_read(ria816_t* c, uint8_t addr) {
    uint8_t data = 0xFF;

    switch (addr) {
        // multiplication accelerator
        case RIA816_MATH_MULAB:
        case RIA816_MATH_MULAB + 1: {
            uint16_t mul = RIA816_REG16(RIA816_MATH_OPERA) * RIA816_REG16(RIA816_MATH_OPERB);
            data = (addr == RIA816_MATH_MULAB) ? (mul & 0xFF) : (mul >> 8);
        } break;
        // division accelerator
        case RIA816_MATH_DIVAB:
        case RIA816_MATH_DIVAB + 1: {
            uint16_t mul = RIA816_REG16(RIA816_MATH_OPERA) / RIA816_REG16(RIA816_MATH_OPERB);
            data = (addr == RIA816_MATH_DIVAB) ? (mul & 0xFF) : (mul >> 8);
        } break;

        case RIA816_UART_READY: {
            if (rb_is_empty(&c->uart_rx))
                data &= ~0b01000000;
            else
                data |= 0b01000000;
            if (rb_is_full(&c->uart_tx))
                data &= ~0b10000000;
            else
                data |= 0b10000000;
        } break;
        case RIA816_UART_TX_RX: rb_get(&c->uart_rx, &data); break;

        case RIA816_HW_RNG:
        case RIA816_HW_RNG + 1: data = rand() & 0xFF; break;

        default: data = c->reg[addr];
    }
    return data;
}

static void _ria816_write(ria816_t* c, uint8_t addr, uint8_t data) {
    switch (addr) {
        case RIA816_UART_TX_RX: rb_put(&c->uart_tx, data); break;
        default: c->reg[addr] = data;
    }
}

uint64_t ria816_tick(ria816_t* c, uint64_t pins) {
    pins = _ria816_tick(c, pins);
    if (pins & RIA816_CS) {
        uint8_t addr = pins & RIA816_RS;
        if (pins & RIA816_RW) {
            uint8_t data = _ria816_read(c, addr);
            RIA816_SET_DATA(pins, data);
        }
        else {
            uint8_t data = RIA816_GET_DATA(pins);
            _ria816_write(c, addr, data);
        }
    }
    c->pins = pins;
    return pins;
}
