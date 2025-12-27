#include "./sgu1.h"
#include "./su/su.h"

#include <string.h>
#ifdef _MSC_VER
    #define _USE_MATH_DEFINES
#endif
#include <math.h> /* tanh */
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

/* extract 8-bit data bus from 64-bit pins */
#define SGU1_GET_DATA(p) ((uint8_t)(((p) & 0xFF0000ULL) >> 16))
/* merge 8-bit data bus value into 64-bit pins */
#define SGU1_SET_DATA(p, d) \
    { p = (((p) & ~0xFF0000ULL) | (((d) << 16) & 0xFF0000ULL)); }
/* fixed point precision for sample period */
#define SGU1_FIXEDPOINT_SCALE (16)
/* move bit into first position */
#define SGU1_BIT(val, bitnr) ((val >> bitnr) & 1)

#define SGU_SU ((SoundUnit*)(sgu->su))

void sgu1_init(sgu1_t* sgu, const sgu1_desc_t* desc) {
    CHIPS_ASSERT(sgu && desc);
    CHIPS_ASSERT(desc->tick_hz > 0);
    CHIPS_ASSERT(desc->sound_hz > 0);
    memset(sgu, 0, sizeof(*sgu));
    sgu->sound_hz = desc->sound_hz;
    sgu->sample_period = (desc->tick_hz * SGU1_FIXEDPOINT_SCALE) / desc->sound_hz;
    sgu->sample_counter = sgu->sample_period;
    sgu->sample_mag = desc->magnitude;
    sgu->su = new SoundUnit();
    SGU_SU->Init();
}

void sgu1_reset(sgu1_t* sgu) {
    CHIPS_ASSERT(sgu);
    SGU_SU->Reset();
    sgu->sample_counter = sgu->sample_period;
    sgu->sample = 0.0f;
    sgu->pins = 0;
}

/* tick the sound generation, return true when new sample ready */
static uint64_t _sgu1_tick(sgu1_t* sgu, uint64_t pins) {
    /* new sample? */
    sgu->sample_counter -= SGU1_FIXEDPOINT_SCALE;
    if (sgu->sample_counter <= 0) {
        sgu->sample_counter += sgu->sample_period;
        short l, r;
        SGU_SU->NextSample(&l, &r);
        sgu->sample = (float)(l + r) / 32768.0f * sgu->sample_mag;
        pins |= SGU1_SAMPLE;
    }
    else {
        pins &= ~SGU1_SAMPLE;
    }
    return pins;
}

/* read a register */
static uint64_t _sgu1_read(sgu1_t* sgu, uint64_t pins) {
    uint8_t reg = pins & SGU1_ADDR_MASK;
    uint8_t data = ((unsigned char*)SGU_SU->chan)[reg];
    SGU1_SET_DATA(pins, data);
    return pins;
}

/* write a register */
static void _sgu1_write(sgu1_t* sgu, uint64_t pins) {
    uint8_t reg = pins & SGU1_ADDR_MASK;
    uint8_t data = SGU1_GET_DATA(pins);
    ((unsigned char*)SGU_SU->chan)[reg] = data;
}

/* the all-in-one tick function */
uint64_t sgu1_tick(sgu1_t* sgu, uint64_t pins) {
    CHIPS_ASSERT(sgu);

    /* first perform the regular per-tick actions */
    pins = _sgu1_tick(sgu, pins);

    /* register read/write */
    if (pins & SGU1_CS) {
        if (pins & SGU1_RW) {
            pins = _sgu1_read(sgu, pins);
        }
        else {
            _sgu1_write(sgu, pins);
        }
    }
    sgu->pins = pins;
    return pins;
}
