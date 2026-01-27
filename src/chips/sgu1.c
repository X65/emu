#include "./sgu1.h"

#include <string.h>
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
#define SGU1_FIXEDPOINT_SCALE (512)
/* move bit into first position */
#define SGU1_BIT(val, bitnr) ((val >> bitnr) & 1)

void sgu1_init(sgu1_t* sgu, const sgu1_desc_t* desc) {
    CHIPS_ASSERT(sgu && desc);
    CHIPS_ASSERT(desc->tick_hz > 0);
    memset(sgu, 0, sizeof(*sgu));
    sgu->sample_mag = desc->magnitude;
    sgu->tick_period = (desc->tick_hz * SGU1_FIXEDPOINT_SCALE) / SGU_CHIP_CLOCK;
    sgu->tick_counter = sgu->tick_period;
    SGU_Init(&sgu->sgu, 65536);
}

void sgu1_reset(sgu1_t* sgu) {
    CHIPS_ASSERT(sgu);
    SGU_Reset(&sgu->sgu);
    sgu->tick_counter = sgu->tick_period;
    sgu->sample[0] = sgu->sample[1] = 0.0f;
    sgu->pins = 0;
    sgu->selected_channel = 0;
}

/* tick the sound generation, return true when new sample ready */
static uint64_t _sgu1_tick(sgu1_t* sgu, uint64_t pins) {
    pins &= ~SGU1_SAMPLE;
    /* next sample? */
    sgu->tick_counter -= SGU1_FIXEDPOINT_SCALE;
    while (sgu->tick_counter <= 0) {
        sgu->tick_counter += sgu->tick_period;
        int32_t l, r;
        SGU_NextSample(&sgu->sgu, &l, &r);
        sgu->sample[0] = sgu->sample_mag * (float)l / 32767.0f;
        sgu->sample[1] = sgu->sample_mag * (float)r / 32767.0f;
        pins |= SGU1_SAMPLE;

        for (uint8_t i = 0; i < SGU_CHNS; i++) {
            sgu->voice[i].sample_buffer[sgu->voice[i].sample_pos++] = (float)(SGU_GetSample(&sgu->sgu, i));
            if (sgu->voice[i].sample_pos >= SGU1_AUDIO_SAMPLES) {
                sgu->voice[i].sample_pos = 0;
            }
        }
    }
    return pins;
}

uint8_t sgu1_reg_read(sgu1_t* sgu, uint8_t reg) {
    uint8_t data;
    if (reg == SGU_REGS_PER_CH - 1) {
        data = sgu->selected_channel;
    }
    else {
        data = ((unsigned char*)sgu->sgu.chan)[(sgu->selected_channel % SGU_CHNS) << 6 | (reg & (SGU_REGS_PER_CH - 1))];
    }
    return data;
}

void sgu1_reg_write(sgu1_t* sgu, uint8_t reg, uint8_t data) {
    if (reg == SGU_REGS_PER_CH - 1) {
        sgu->selected_channel = data;
    }
    else {
        // ((unsigned char*)sgu->sgu.chan)[(sgu->selected_channel % SGU_CHNS) << 6 | (reg & (SGU_REGS_PER_CH - 1))] =
        // data;
        SGU_Write(&sgu->sgu, (uint16_t)((sgu->selected_channel % SGU_CHNS) << 6) | (reg & (SGU_REGS_PER_CH - 1)), data);
    }
}

void sgu1_direct_reg_write(sgu1_t* sgu, uint16_t reg, uint8_t data) {
    SGU_Write(&sgu->sgu, reg, data);
}

/* read a register */
static uint64_t _sgu1_read(sgu1_t* sgu, uint64_t pins) {
    uint8_t reg = pins & SGU1_ADDR_MASK;
    SGU1_SET_DATA(pins, sgu1_reg_read(sgu, reg));
    return pins;
}

/* write a register */
static void _sgu1_write(sgu1_t* sgu, uint64_t pins) {
    uint8_t reg = pins & SGU1_ADDR_MASK;
    uint8_t data = SGU1_GET_DATA(pins);
    sgu1_reg_write(sgu, reg, data);
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
