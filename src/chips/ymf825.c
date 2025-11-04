#include "./ymf825.h"

#include <string.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

static const uint8_t YMF825_REG_RESET_VALUE[YMF825_NUM_REGISTERS] = {
    0x00, 0x80, 0x0F, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x60, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void ymf825_init(ymf825_t* sd1, const ymf825_desc_t* desc) {
    CHIPS_ASSERT(sd1 && desc);
    CHIPS_ASSERT(desc->tick_hz > 0);
    CHIPS_ASSERT(desc->sound_hz > 0);
    memset(sd1, 0, sizeof(*sd1));
    sd1->sound_hz = desc->sound_hz;
    sd1->sample_period = (desc->tick_hz * YMF825_FIXEDPOINT_SCALE) / desc->sound_hz;
    sd1->sample_counter = sd1->sample_period;
    sd1->resampler.rateratio = (desc->sound_hz << YMF825_RESAMPLER_FRAC) / YMF825_SAMPLE_RATE;

    for (int i = 0; i < sizeof(YMF825_REG_RESET_VALUE); i++) {
        sd1->registers[i] = YMF825_REG_RESET_VALUE[i];
    }
}

void ymf825_reset(ymf825_t* sd1) {
    CHIPS_ASSERT(sd1);
    sd1->resampler.samplecnt = 0;
}

/* tick the sound generation, return true when new sample ready */
static bool _ymf825_tick(ymf825_t* sd1) {
    // generate new sample?
    sd1->sample_counter -= YMF825_FIXEDPOINT_SCALE;
    if (sd1->sample_counter <= 0) {
        sd1->sample_counter += sd1->sample_period;

        // spin OPL3 chip in its own rate
        // until we have two samples we can linearly (I know… bad for audio)
        // interpolate the sample in our requested audio rate
        while (sd1->resampler.samplecnt >= sd1->resampler.rateratio) {
            sd1->resampler.oldsamples[0] = sd1->resampler.samples[0];
            sd1->resampler.oldsamples[1] = sd1->resampler.samples[1];
            // TODO: SD-1 sample generation
            sd1->resampler.samplecnt -= sd1->resampler.rateratio;
        }
        sd1->resampler.samplecnt += 1 << YMF825_RESAMPLER_FRAC;

        float sample_0 =
            ((float)(sd1->resampler.oldsamples[0] * (sd1->resampler.rateratio - sd1->resampler.samplecnt)
                     + sd1->resampler.samples[0] * sd1->resampler.samplecnt)
             / (float)sd1->resampler.rateratio);
        float sample_1 =
            ((float)(sd1->resampler.oldsamples[1] * (sd1->resampler.rateratio - sd1->resampler.samplecnt)
                     + sd1->resampler.samples[1] * sd1->resampler.samplecnt)
             / (float)sd1->resampler.rateratio);

        // convert uint16_t range to float in range -1.0f to 1.0f
        sd1->samples[0] = sample_0 < 0 ? sample_0 / 32768.0f : sample_0 / 32767.0f;
        sd1->samples[1] = sample_1 < 0 ? sample_1 / 32768.0f : sample_1 / 32767.0f;

        return true;  // new sample is ready
    }
    // fallthrough: no new sample ready yet
    return false;
}

/* read a register */
static uint64_t _ymf825_read(ymf825_t* sd1, uint64_t pins) {
    const uint8_t reg = YMF825_GET_ADDR(pins);
    uint8_t data = sd1->registers[reg];
    YMF825_SET_DATA(pins, data);
    return pins;
}

/* write a register */
static void _ymf825_write(ymf825_t* sd1, uint64_t pins) {
    const uint8_t reg = YMF825_GET_ADDR(pins);
    const uint8_t data = YMF825_GET_DATA(pins);
    sd1->registers[reg] = data;
}

/* the all-in-one tick function */
uint64_t ymf825_tick(ymf825_t* sd1, uint64_t pins) {
    CHIPS_ASSERT(sd1);

    /* register read/write */
    if (pins & YMF825_CS) {
        if (pins & YMF825_RW) {
            // read
            pins = _ymf825_read(sd1, pins);
        }
        else {
            // write
            _ymf825_write(sd1, pins);
        }
    }

    /* then perform the regular per-tick actions */
    pins |= _ymf825_tick(sd1) ? YMF825_SAMPLE : 0;

    sd1->pins = pins;
    return pins;
}

void ymf825_snapshot_onsave(ymf825_t* snapshot) {
    CHIPS_ASSERT(snapshot);
}

void ymf825_snapshot_onload(ymf825_t* snapshot, ymf825_t* sd1) {
    CHIPS_ASSERT(snapshot && sd1);
}
