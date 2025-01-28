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
    ESFM_init(&ymf->opl3);
}

void ymf262_reset(ymf262_t* ymf) {
    CHIPS_ASSERT(ymf);
    ymf->addr[0] = 0;
    ymf->addr[1] = 0;
    ESFM_init(&ymf->opl3);
}

/* tick the sound generation, return true when new sample ready */
static bool _ymf262_tick(ymf262_t* ymf) {
    // generate new sample?
    ymf->sample_counter -= YMF262_FIXEDPOINT_SCALE;
    if (ymf->sample_counter <= 0) {
        ymf->sample_counter += ymf->sample_period;
        ESFM_generate(&ymf->opl3, ymf->samples);
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
            ESFM_write_reg(&ymf->opl3, ymf->addr[0], data);
            break;
        case 0x02:  // bank 1 address latch
            ymf->addr[1] = data;
            break;
        case 0x03:  // bank 1 register write
            ESFM_write_reg(&ymf->opl3, (0x100 | ymf->addr[1]), data);
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
    ESFM_init(&ymf->opl3);
}
