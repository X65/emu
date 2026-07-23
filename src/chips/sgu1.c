#include "./sgu1.h"

#include <stdio.h>
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

#define SGU1_SERVICE_BANK      (0xFF)
#define SGU1_SVC_SAMPLE_OFF_LO (0x1C)
#define SGU1_SVC_SAMPLE_OFF_HI (0x1D)
#define SGU1_SVC_SAMPLE_BANK   (0x1E)
#define SGU1_SVC_SAMPLE_DATA   (0x1F)
#define SGU1_SVC_MASTER_VOL    (0x20)

void sgu1_init(sgu1_t* sgu, const sgu1_desc_t* desc) {
    CHIPS_ASSERT(sgu && desc);
    CHIPS_ASSERT(desc->tick_hz > 0);
    memset(sgu, 0, sizeof(*sgu));
    sgu->sample_mag = desc->magnitude;
    sgu->tick_period = (desc->tick_hz * SGU1_FIXEDPOINT_SCALE) / SGU_CHIP_CLOCK;
    sgu->tick_counter = sgu->tick_period;
    SGU_Init(&sgu->sgu, 65536);
    if (desc->dump_file) {
        sgu->dump_file = fopen(desc->dump_file, "w");
        if (!sgu->dump_file) {
            fprintf(stderr, "sgu1: cannot open register dump file '%s'\n", desc->dump_file);
        }
    }
}

void sgu1_discard(sgu1_t* sgu) {
    CHIPS_ASSERT(sgu);
    if (sgu->dump_file) {
        fclose(sgu->dump_file);
        sgu->dump_file = 0;
    }
}

void sgu1_reset(sgu1_t* sgu) {
    CHIPS_ASSERT(sgu);
    SGU_Reset(&sgu->sgu);
    sgu->tick_counter = sgu->tick_period;
    sgu->sample[0] = sgu->sample[1] = 0.0f;
    sgu->pins = 0;
    sgu->selected_channel = 0;
    sgu->svc_sample_offset = 0;
    sgu->svc_sample_bank = 0;
    sgu->svc_master_vol = 0;
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
        // The hardware volume law is not documented yet. Keep the service-bank
        // implementation linear, with 0xFF exactly equal to the prior output.
        float master_gain = (float)sgu->svc_master_vol / 255.0f;
        sgu->sample[0] = sgu->sample_mag * master_gain * (float)l / 32767.0f;
        sgu->sample[1] = sgu->sample_mag * master_gain * (float)r / 32767.0f;
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

static uint8_t _sgu1_service_read(sgu1_t* sgu, uint8_t reg) {
    switch (reg) {
        case SGU1_SVC_SAMPLE_OFF_LO: return (uint8_t)sgu->svc_sample_offset;
        case SGU1_SVC_SAMPLE_OFF_HI: return (uint8_t)(sgu->svc_sample_offset >> 8);
        case SGU1_SVC_SAMPLE_BANK: return sgu->svc_sample_bank;
        case SGU1_SVC_SAMPLE_DATA: {
            uint8_t data = sgu->svc_sample_bank == 0 ? (uint8_t)sgu->sgu.pcm[sgu->svc_sample_offset] : 0;
            sgu->svc_sample_offset++;
            return data;
        }
        case SGU1_SVC_MASTER_VOL: return sgu->svc_master_vol;
        default: return 0;
    }
}

static void _sgu1_service_write(sgu1_t* sgu, uint8_t reg, uint8_t data) {
    switch (reg) {
        case SGU1_SVC_SAMPLE_OFF_LO: sgu->svc_sample_offset = (sgu->svc_sample_offset & 0xFF00u) | data; break;
        case SGU1_SVC_SAMPLE_OFF_HI:
            sgu->svc_sample_offset = (uint16_t)((sgu->svc_sample_offset & 0x00FFu) | ((uint16_t)data << 8));
            break;
        case SGU1_SVC_SAMPLE_BANK: sgu->svc_sample_bank = data; break;
        case SGU1_SVC_SAMPLE_DATA:
            if (sgu->svc_sample_bank == 0) {
                sgu->sgu.pcm[sgu->svc_sample_offset] = (int8_t)data;
            }
            sgu->svc_sample_offset++;
            break;
        case SGU1_SVC_MASTER_VOL: sgu->svc_master_vol = data; break;
        default: break;
    }
}

uint8_t sgu1_reg_read(sgu1_t* sgu, uint8_t reg) {
    reg &= SGU_REGS_PER_CH - 1;
    if (reg == SGU_REGS_PER_CH - 1) {
        return sgu->selected_channel;
    }
    if (sgu->selected_channel < SGU_CHNS) {
        return ((uint8_t*)sgu->sgu.chan)[(sgu->selected_channel << 6) | reg];
    }
    if (sgu->selected_channel == SGU1_SERVICE_BANK) {
        return _sgu1_service_read(sgu, reg);
    }
    return 0xFF;
}

void sgu1_reg_write(sgu1_t* sgu, uint8_t reg, uint8_t data) {
    if (sgu->dump_file) {
        sgu->dirty = true;
    }
    reg &= SGU_REGS_PER_CH - 1;
    if (reg == SGU_REGS_PER_CH - 1) {
        sgu->selected_channel = data;
        return;
    }
    if (sgu->selected_channel < SGU_CHNS) {
        SGU_Write(&sgu->sgu, (uint16_t)(sgu->selected_channel << 6) | reg, data);
    }
    else if (sgu->selected_channel == SGU1_SERVICE_BANK) {
        _sgu1_service_write(sgu, reg, data);
    }
}

void sgu1_direct_reg_write(sgu1_t* sgu, uint16_t reg, uint8_t data) {
    SGU_Write(&sgu->sgu, reg, data);
}

/* dump all registers of all channels for the frame that just ended */
void sgu1_dump_frame(sgu1_t* sgu) {
    CHIPS_ASSERT(sgu);
    sgu->frame_counter++;
    if (!sgu->dump_file || !sgu->dirty) {
        return;
    }
    FILE* f = sgu->dump_file;
    const uint8_t* regs = (const uint8_t*)sgu->sgu.chan;
    fprintf(f, "Frame %u:\n", sgu->frame_counter);
    for (int ch = 0; ch < SGU_CHNS; ch++) {
        fprintf(f, "%02X:", ch);
        for (int r = 0; r < SGU_REGS_PER_CH; r++) {
            fprintf(f, " %02X", regs[(ch << 6) | r]);
        }
        fprintf(f, "\n");
    }
    fflush(f);
    sgu->dirty = false;
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
