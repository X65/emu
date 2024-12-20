#include "ria816.h"

#include <string.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

void ria816_init(ria816_t* c) {
    CHIPS_ASSERT(c);
    memset(c, 0, sizeof(*c));
}

void ria816_reset(ria816_t* c) {
    CHIPS_ASSERT(c);
    c->pins = 0;
}

static uint64_t _ria816_tick(ria816_t* c, uint64_t pins) {
    return pins;
}

/* read a register */
static uint8_t _ria816_read(ria816_t* c, uint8_t addr) {
    uint8_t data = 0xFF;
    switch (addr) {}
    return data;
}

static void _ria816_write(ria816_t* c, uint8_t addr, uint8_t data) {
    switch (addr) {}
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
