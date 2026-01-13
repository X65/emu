#include "./tca6416a.h"

#include <string.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

static void _tca6416a_init_port(tca6416a_port_t* p, uint8_t in) {
    p->cfg = 0xff;  // default inputs
    p->pol = 0;
    p->in = in;
    p->out = 0;
}

void tca6416a_init(tca6416a_t* c, uint8_t p0, uint8_t p1) {
    CHIPS_ASSERT(c);
    memset(c, 0, sizeof(*c));
    _tca6416a_init_port(&c->p0, p0);
    _tca6416a_init_port(&c->p1, p1);
}

void tca6416a_reset(tca6416a_t* c, uint8_t p0, uint8_t p1) {
    CHIPS_ASSERT(c);
    _tca6416a_init_port(&c->p0, p0);
    _tca6416a_init_port(&c->p1, p1);
    c->pins = 0;
}

/*--- port implementation ---*/
static inline uint64_t _tca6416a_read_write_port_pins(tca6416a_t* c, uint64_t pins) {
    c->p0.in = TCA6416A_GET_P0(pins) ^ c->p0.pol;
    c->p1.in = TCA6416A_GET_P1(pins) ^ c->p1.pol;
    // merge output state pins
    c->p0.in &= c->p0.cfg;
    c->p0.in |= ~c->p0.cfg & c->p0.out;
    c->p1.in &= c->p1.cfg;
    c->p1.in |= ~c->p1.cfg & c->p1.out;

    TCA6416A_SET_P01(pins, c->p0.in, c->p1.in);
    return pins;
}

static void _tca6416a_update_int(tca6416a_t* c, uint64_t pins) {
    uint8_t p0 = (TCA6416A_GET_P0(pins) ^ c->p0.pol);
    uint8_t p1 = (TCA6416A_GET_P1(pins) ^ c->p1.pol);

    if ((p0 & c->p0.cfg) != (c->p0.in & c->p0.cfg)) {
        c->intr |= TCA6416A_INT_P0;
    }
    if ((p1 & c->p1.cfg) != (c->p1.in & c->p1.cfg)) {
        c->intr |= TCA6416A_INT_P1;
    }
}

static uint64_t _tca6416a_tick(tca6416a_t* c, uint64_t pins) {
    _tca6416a_update_int(c, pins);
    pins = _tca6416a_read_write_port_pins(c, pins);
    return pins;
}

/* read a register */
uint8_t tca6416a_read(tca6416a_t* c, uint8_t addr) {
    uint8_t data = 0xFF;
    switch (addr) {
        case TCA6416A_REG_IN0:
            data = c->p0.in;
            c->intr &= ~TCA6416A_INT_P0;
            break;
        case TCA6416A_REG_IN1:
            data = c->p1.in;
            c->intr &= ~TCA6416A_INT_P1;
            break;
        case TCA6416A_REG_OUT0: data = c->p0.out; break;
        case TCA6416A_REG_OUT1: data = c->p1.out; break;
        case TCA6416A_REG_POL0: data = c->p0.pol; break;
        case TCA6416A_REG_POL1: data = c->p1.pol; break;
        case TCA6416A_REG_CFG0: data = c->p0.cfg; break;
        case TCA6416A_REG_CFG1: data = c->p1.cfg; break;
    }
    return data;
}

void tca6416a_write(tca6416a_t* c, uint8_t addr, uint8_t data) {
    switch (addr) {
        case TCA6416A_REG_OUT0: c->p0.out = data; break;
        case TCA6416A_REG_OUT1: c->p1.out = data; break;
        case TCA6416A_REG_POL0: c->p0.pol = data; break;
        case TCA6416A_REG_POL1: c->p1.pol = data; break;
        case TCA6416A_REG_CFG0: c->p0.cfg = data; break;
        case TCA6416A_REG_CFG1: c->p1.cfg = data; break;
    }
}

uint64_t tca6416a_tick(tca6416a_t* c, uint64_t pins) {
    pins = _tca6416a_tick(c, pins);
    if (pins & TCA6416A_CS) {
        uint8_t addr = pins & TCA6416A_RS;
        if (pins & TCA6416A_RW) {
            uint8_t data = tca6416a_read(c, addr);
            TCA6416A_SET_DATA(pins, data);
        }
        else {
            uint8_t data = TCA6416A_GET_DATA(pins);
            tca6416a_write(c, addr, data);
        }
    }
    if (c->intr) pins |= TCA6416A_INT;

    c->pins = pins;
    return pins;
}
