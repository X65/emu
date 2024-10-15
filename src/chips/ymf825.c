#include "ymf825.h"

#include <string.h>

#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

/* extract 8-bit data bus from 64-bit pins */
#define YMF825_GET_DATA(p) ((uint8_t)(((p) & 0xFF0000ULL) >> 16))
/* merge 8-bit data bus value into 64-bit pins */
#define YMF825_SET_DATA(p, d) \
    { p = (((p) & ~0xFF0000ULL) | (((d) << 16) & 0xFF0000ULL)); }
/* fixed point precision for sample period */
#define YMF825_FIXEDPOINT_SCALE (16)
/* move bit into first position */
#define YMF825_BIT(val, bitnr) ((val >> bitnr) & 1)

void ymf825_init(ymf825_t* sd1, const ymf825_desc_t* desc) {
    CHIPS_ASSERT(sd1 && desc);
    CHIPS_ASSERT(desc->tick_hz > 0);
    CHIPS_ASSERT(desc->sound_hz > 0);
    memset(sd1, 0, sizeof(*sd1));
    sd1->sound_hz = desc->sound_hz;
    sd1->sample_period = (desc->tick_hz * YMF825_FIXEDPOINT_SCALE) / desc->sound_hz;
    sd1->sample_counter = sd1->sample_period;
    sd1->sample_mag = desc->magnitude;
    sd1->sample_accum_count = 1.0f;
    // for (int i = 0; i < 3; i++) {
    //     _ymf825_init_voice(&sd1->voice[i]);
    // }
    // _ymf825_init_cutoff_table();
    // _ymf825_init_filter(&sd1->filter, sd1->sound_hz);
}

void ymf825_reset(ymf825_t* sd1) {
    CHIPS_ASSERT(sd1);
    // for (int i = 0; i < 3; i++) {
    //     _ymf825_init_voice(&sd1->voice[i]);
    // }
    // _ymf825_init_filter(&sd1->filter, sd1->sound_hz);
    sd1->sample_counter = sd1->sample_period;
    sd1->sample = 0.0f;
    sd1->sample_accum = 0.0f;
    sd1->sample_accum_count = 1.0f;
    sd1->pins = 0;
}

/* tick the sound generation, return true when new sample ready */
static uint64_t _ymf825_tick(ymf825_t* sd1, uint64_t pins) {
    // TODO: implement

    /* new sample? */
    sd1->sample_counter -= YMF825_FIXEDPOINT_SCALE;
    if (sd1->sample_counter <= 0) {
        sd1->sample_counter += sd1->sample_period;
        float s = sd1->sample_accum / sd1->sample_accum_count;
        sd1->sample = sd1->sample_mag * s;
        sd1->sample_accum = 0.0f;
        sd1->sample_accum_count = 0.0f;
        pins |= YMF825_SAMPLE;
    }
    else {
        pins &= ~YMF825_SAMPLE;
    }
    return pins;
}

/* read a register */
static uint64_t _ymf825_read(ymf825_t* sd1, uint64_t pins) {
    uint8_t reg = pins & YMF825_ADDR_MASK;
    uint8_t data;
    switch (reg) {
        default: data = 0x55; break;
    }
    YMF825_SET_DATA(pins, data);
    return pins;
}

/* write a register */
static void _ymf825_write(ymf825_t* sd1, uint64_t pins) {
    uint8_t reg = pins & YMF825_ADDR_MASK;
    // uint8_t data = YMF825_GET_DATA(pins);
    switch (reg) {}
}

/* the all-in-one tick function */
uint64_t ymf825_tick(ymf825_t* sd1, uint64_t pins) {
    CHIPS_ASSERT(sd1);

    /* first perform the regular per-tick actions */
    pins = _ymf825_tick(sd1, pins);

    /* register read/write */
    if (pins & YMF825_CS) {
        if (pins & YMF825_RW) {
            pins = _ymf825_read(sd1, pins);
        }
        else {
            _ymf825_write(sd1, pins);
        }
    }
    sd1->pins = pins;
    return pins;
}
