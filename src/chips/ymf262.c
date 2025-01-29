#include "./ymf262.h"

#include <string.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

void ymf262_init(ymf262_t* ymf, const ymf262_desc_t* desc) {
    CHIPS_ASSERT(ymf && desc);
    CHIPS_ASSERT(desc->tick_hz > 0);
    CHIPS_ASSERT(desc->sound_hz > 0);
    memset(ymf, 0, sizeof(*ymf));
    ymf->sound_hz = desc->sound_hz;
    ymf->sample_period = (desc->tick_hz * YMF262_FIXEDPOINT_SCALE) / desc->sound_hz;
    ymf->sample_counter = ymf->sample_period;
    ymf->resampler.rateratio = (desc->sound_hz << YMF262_RESAMPLER_FRAC) / YMF262_SAMPLE_RATE;
    ESFM_init(&ymf->chip);
}

void ymf262_reset(ymf262_t* ymf) {
    CHIPS_ASSERT(ymf);
    ymf->addr[0] = 0;
    ymf->addr[1] = 0;
    ESFM_init(&ymf->chip);
    ymf->resampler.samplecnt = 0;
}

/* tick the sound generation, return true when new sample ready */
static bool _ymf262_tick(ymf262_t* ymf) {
    // generate new sample?
    ymf->sample_counter -= YMF262_FIXEDPOINT_SCALE;
    if (ymf->sample_counter <= 0) {
        ymf->sample_counter += ymf->sample_period;

        // spin OPL3 chip in its own rate
        // until we have two samples we can linearly (I know… bad for audio)
        // interpolate the sample in our requested audio rate
        while (ymf->resampler.samplecnt >= ymf->resampler.rateratio) {
            ymf->resampler.oldsamples[0] = ymf->resampler.samples[0];
            ymf->resampler.oldsamples[1] = ymf->resampler.samples[1];
            ESFM_generate(&ymf->chip, ymf->resampler.samples);
            ymf->resampler.samplecnt -= ymf->resampler.rateratio;
        }
        ymf->resampler.samplecnt += 1 << YMF262_RESAMPLER_FRAC;

        float sample_0 =
            ((float)(ymf->resampler.oldsamples[0] * (ymf->resampler.rateratio - ymf->resampler.samplecnt)
                     + ymf->resampler.samples[0] * ymf->resampler.samplecnt)
             / (float)ymf->resampler.rateratio);
        float sample_1 =
            ((float)(ymf->resampler.oldsamples[1] * (ymf->resampler.rateratio - ymf->resampler.samplecnt)
                     + ymf->resampler.samples[1] * ymf->resampler.samplecnt)
             / (float)ymf->resampler.rateratio);

        // convert uint16_t range to float in range -1.0f to 1.0f
        ymf->samples[0] = sample_0 < 0 ? sample_0 / 32768.0f : sample_0 / 32767.0f;
        ymf->samples[1] = sample_1 < 0 ? sample_1 / 32768.0f : sample_1 / 32767.0f;

        return true;  // new sample is ready
    }
    // fallthrough: no new sample ready yet
    return false;
}

/* read a register */
static uint64_t _ymf262_read(ymf262_t* ymf, uint64_t pins) {
    const uint8_t reg = YMF262_GET_ADDR(pins);
    uint8_t data;
    switch (reg) {
        case 0x00:        // status register
            data = 0x00;  // FIXME: implement?
            break;
        default: data = 0xFF; break;
    }
    YMF262_SET_DATA(pins, data);
    return pins;
}

/* write a register */
static void _ymf262_write(ymf262_t* ymf, uint64_t pins) {
    const uint8_t reg = YMF262_GET_ADDR(pins);
    const uint8_t data = YMF262_GET_DATA(pins);
    switch (reg) {
        case 0x00:  // bank 0 address latch
            ymf->addr[0] = data;
            break;
        case 0x01:  // bank 0 register write
            ESFM_write_reg(&ymf->chip, ymf->addr[0], data);
            break;
        case 0x02:  // bank 1 address latch
            ymf->addr[1] = data;
            break;
        case 0x03:  // bank 1 register write
            ESFM_write_reg(&ymf->chip, (0x100 | ymf->addr[1]), data);
            break;
    }
}

/* the all-in-one tick function */
uint64_t ymf262_tick(ymf262_t* ymf, uint64_t pins) {
    CHIPS_ASSERT(ymf);

    /* register read/write */
    if (pins & YMF262_CS) {
        if (pins & YMF262_RW) {
            // read
            pins = _ymf262_read(ymf, pins);
        }
        else {
            // write
            _ymf262_write(ymf, pins);
        }
    }

    /* then perform the regular per-tick actions */
    pins |= _ymf262_tick(ymf) ? YMF262_SAMPLE : 0;

    ymf->pins = pins;
    return pins;
}

void ymf262_snapshot_onsave(ymf262_t* snapshot) {
    CHIPS_ASSERT(snapshot);
}

void ymf262_snapshot_onload(ymf262_t* snapshot, ymf262_t* ymf) {
    CHIPS_ASSERT(snapshot && ymf);
    ESFM_init(&ymf->chip);
}
