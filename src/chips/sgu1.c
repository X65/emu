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
#define SGU1_FIXEDPOINT_SCALE (16)
/* move bit into first position */
#define SGU1_BIT(val, bitnr) ((val >> bitnr) & 1)

#define CHIP_DIVIDER  2
#define CHIP_FREQBASE 524288
#define CHIP_CLOCK    618000  // tSU: 6.18MHz (NTSC)

void sgu1_init(sgu1_t* sgu, const sgu1_desc_t* desc) {
    CHIPS_ASSERT(sgu && desc);
    CHIPS_ASSERT(desc->tick_hz > 0);
    CHIPS_ASSERT(desc->sound_hz > 0);
    memset(sgu, 0, sizeof(*sgu));
    sgu->sound_hz = desc->sound_hz;
    sgu->sample_period = (desc->tick_hz * SGU1_FIXEDPOINT_SCALE) / desc->sound_hz;
    sgu->sample_counter = sgu->sample_period;
    sgu->sample_mag = desc->magnitude;
    sgu->tick_period = (desc->tick_hz * SGU1_FIXEDPOINT_SCALE) / (CHIP_CLOCK / CHIP_DIVIDER);
    sgu->tick_counter = sgu->tick_period;
    SoundUnit_Init(&sgu->su, 65536, false);
    sgu->resampler = speex_resampler_init(SGU1_AUDIO_CHANNELS, CHIP_CLOCK / CHIP_DIVIDER, sgu->sound_hz, 10, nullptr);
}

void sgu1_reset(sgu1_t* sgu) {
    CHIPS_ASSERT(sgu);
    SoundUnit_Reset(&sgu->su);
    memset(sgu->reg, 0, sizeof(sgu->reg));
    sgu->tick_counter = sgu->tick_period;
    sgu->sample_counter = sgu->sample_period;
    sgu->sample[0] = sgu->sample[1] = 0.0f;
    sgu->pins = 0;
    speex_resampler_reset_mem(sgu->resampler);
}

/* tick the sound generation, return true when new sample ready */
static uint64_t _sgu1_tick(sgu1_t* sgu, uint64_t pins) {
    /* next sample? */
    sgu->tick_counter -= SGU1_FIXEDPOINT_SCALE;
    if (sgu->tick_counter <= 0) {
        sgu->tick_counter += sgu->tick_period;
        short l, r;
        SoundUnit_NextSample(&sgu->su, &l, &r);
        float in[2] = { ((float)l / 32767.0f), ((float)r / 32767.0f) };
        spx_uint32_t in_len = 1;   // 1 stereo frame
        spx_uint32_t out_len = 1;  // room for 1 stereo frame
        speex_resampler_process_interleaved_float(sgu->resampler, in, &in_len, sgu->sample, &out_len);

        for (int i = 0; i < SGU1_NUM_CHANNELS; i++) {
            sgu->voice[i].sample_buffer[sgu->voice[i].sample_pos++] = (float)(SoundUnit_GetSample(&sgu->su, i));
            if (sgu->voice[i].sample_pos >= SGU1_AUDIO_SAMPLES) {
                sgu->voice[i].sample_pos = 0;
            }
        }
    }

    /* new sample? */
    sgu->sample_counter -= SGU1_FIXEDPOINT_SCALE;
    if (sgu->sample_counter <= 0) {
        sgu->sample_counter += sgu->sample_period;
        sgu->sample[0] = sgu->sample_mag * sgu->sample[0];
        sgu->sample[1] = sgu->sample_mag * sgu->sample[1];
        pins |= SGU1_SAMPLE;
    }
    else {
        pins &= ~SGU1_SAMPLE;
    }
    return pins;
}

static inline uint8_t _sgu1_selected_channel(sgu1_t* sgu) {
    return sgu->reg[SGU1_REG_CHANNEL_SELECT] & (SGU1_NUM_CHANNELS - 1);
}

uint8_t sgu1_reg_read(sgu1_t* sgu, uint8_t reg) {
    uint8_t data;
    if (reg < 32) {
        data = sgu->reg[reg];
    }
    else {
        uint8_t chan = _sgu1_selected_channel(sgu);
        data = ((unsigned char*)sgu->su.chan)[chan << 5 | (reg & (SGU1_NUM_CHANNEL_REGS - 1))];
    }
    return data;
}

void sgu1_reg_write(sgu1_t* sgu, uint8_t reg, uint8_t data) {
    if (reg < 32) {
        if (reg == SGU1_REG_CHANNEL_SELECT) {
            sgu->reg[reg] = data;
        }
    }
    else {
        uint8_t chan = _sgu1_selected_channel(sgu);
        ((unsigned char*)sgu->su.chan)[chan << 5 | (reg & (SGU1_NUM_CHANNEL_REGS - 1))] = data;
    }
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
