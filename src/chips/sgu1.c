#include "./sgu1.h"

#include <stdio.h>
#include <stdlib.h>
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

#define SGU1_SERVICE_BANK (0xFF)

/* Service bank register map. The identification block occupies $00..$0F, the
   status block starts at $10 and the control block at $18; the gaps between
   them are reserved so the two blocks can grow without moving anything. */
#define SGU1_SVC_MAGIC         (0x00) /* $00..$03, "SGU1" */
#define SGU1_SVC_MAGIC_END     (0x03)
#define SGU1_SVC_VER_MAJOR     (0x04)
#define SGU1_SVC_VER_MINOR     (0x05)
#define SGU1_SVC_UNIQUE_ID     (0x06) /* $06..$0D, 8 bytes */
#define SGU1_SVC_UNIQUE_ID_END (0x0D)
#define SGU1_SVC_PCM_BANKS     (0x0E)
#define SGU1_SVC_SVC_BANKS     (0x0F)
#define SGU1_SVC_STATUS        (0x10)
#define SGU1_SVC_CHIP_RESET    (0x18)
#define SGU1_SVC_SAMPLE_OFF_LO (0x1C)
#define SGU1_SVC_SAMPLE_OFF_HI (0x1D)
#define SGU1_SVC_SAMPLE_BANK   (0x1E)
#define SGU1_SVC_SAMPLE_DATA   (0x1F)
#define SGU1_SVC_MASTER_VOL    (0x20)

#define SGU1_VERSION_MAJOR (0x01)
#define SGU1_VERSION_MINOR (0x00)

// Service banks beyond $FF. The $FE bank that will carry DSP registers and
// programming does not exist yet, so software must discover zero of them.
#define SGU1_SVC_EXTRA_BANKS (0)

// CHIP_RESET: the high nybble must be the magic $A or the write is ignored, so
// a stray store cannot silence the chip. The low nybble names what to reset.
#define SGU1_RESET_MAGIC      (0xA0)
#define SGU1_RESET_MAGIC_MASK (0xF0)
#define SGU1_RESET_VOICES     (1 << 0) /* -> SGU_RESET_VOICES   */
#define SGU1_RESET_TIMEBASE   (1 << 1) /* -> SGU_RESET_TIMEBASE */
#define SGU1_RESET_MIX        (1 << 2) /* -> SGU_RESET_MIX      */
#define SGU1_RESET_SVC        (1 << 3) /* the service registers below, host-side */

static const uint8_t _sgu1_svc_magic[] = { 'S', 'G', 'U', '1' };

// The unique id is the RP2350 board id on hardware. The emulator has no board,
// and silicon never reports an all-zero id, so all-zero is how software tells
// it is talking to an emulated chip.
static const uint8_t _sgu1_svc_unique_id[8] = { 0 };

void sgu1_init(sgu1_t* sgu, const sgu1_desc_t* desc) {
    CHIPS_ASSERT(sgu && desc);
    CHIPS_ASSERT(desc->tick_hz > 0);
    memset(sgu, 0, sizeof(*sgu));
    sgu->sample_mag = desc->magnitude;
    sgu->tick_period = (desc->tick_hz * SGU1_FIXEDPOINT_SCALE) / SGU_CHIP_CLOCK;
    sgu->tick_counter = sgu->tick_period;
    size_t pcm_size = SGU1_PCM_BANKS * SGU_PCM_BANK_SIZE;
    int8_t* pcm = malloc(pcm_size);
    CHIPS_ASSERT(pcm);
    SGU_Init(&sgu->sgu, pcm, pcm_size);
    if (desc->dump_file) {
        sgu->dump_file = fopen(desc->dump_file, "w");
        if (!sgu->dump_file) {
            fprintf(stderr, "sgu1: cannot open register dump file '%s'\n", desc->dump_file);
        }
    }
}

void sgu1_discard(sgu1_t* sgu) {
    CHIPS_ASSERT(sgu);
    free(sgu->sgu.pcm);
    sgu->sgu.pcm = 0;
    sgu->sgu.pcm_size = 0;
    if (sgu->dump_file) {
        fclose(sgu->dump_file);
        sgu->dump_file = 0;
    }
}

/* Reset the service-bank registers. Note this deliberately leaves
   selected_channel alone: the channel select at $3F is the register window,
   shared by every bank, not service state -- clearing it from a CHIP_RESET
   write would deselect the service bank out from under the very sequence
   issuing the reset. Only a chip reset (sgu1_reset) clears the select. */
static void _sgu1_service_reset(sgu1_t* sgu) {
    sgu->svc_sample_offset = 0;
    sgu->svc_sample_bank = 0;
    // Muted out of reset, deliberately: master volume gates the whole mix, and
    // audio hardware must not blast whatever noise the register file powers up
    // with. Raising it is the OS's job, once the channels are in a known state.
    sgu->svc_master_vol = 0;
}

void sgu1_reset(sgu1_t* sgu) {
    CHIPS_ASSERT(sgu);
    SGU_Reset(&sgu->sgu);
    sgu->tick_counter = sgu->tick_period;
    sgu->sample[0] = sgu->sample[1] = 0.0f;
    sgu->pins = 0;
    sgu->selected_channel = 0;
    _sgu1_service_reset(sgu);
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

static size_t _sgu1_service_pcm_address(const sgu1_t* sgu) {
    return (size_t)sgu->svc_sample_bank * SGU_PCM_BANK_SIZE + sgu->svc_sample_offset;
}

static uint8_t _sgu1_service_read(sgu1_t* sgu, uint8_t reg) {
    if (reg >= SGU1_SVC_MAGIC && reg <= SGU1_SVC_MAGIC_END) {
        return _sgu1_svc_magic[reg - SGU1_SVC_MAGIC];
    }
    if (reg >= SGU1_SVC_UNIQUE_ID && reg <= SGU1_SVC_UNIQUE_ID_END) {
        return _sgu1_svc_unique_id[reg - SGU1_SVC_UNIQUE_ID];
    }
    switch (reg) {
        case SGU1_SVC_VER_MAJOR: return SGU1_VERSION_MAJOR;
        case SGU1_SVC_VER_MINOR: return SGU1_VERSION_MINOR;
        // A discovery register tells the truth about the build it runs on: the
        // emulator maps more PCM than the hardware does.
        case SGU1_SVC_PCM_BANKS: return SGU1_PCM_BANKS;
        case SGU1_SVC_SVC_BANKS: return SGU1_SVC_EXTRA_BANKS;
        // Read-to-clear. The core accumulates the flags and hands them over in
        // one atomic exchange, so a clip latched by the render path is either
        // reported here or survives to the next read. Note the debugger's
        // mem_rd path lands here too: parking a memory editor on this address
        // eats clip events, exactly as a debugger read would on real hardware.
        case SGU1_SVC_STATUS: return (uint8_t)(SGU_GetFlags(&sgu->sgu) & 0xFFu);
        case SGU1_SVC_SAMPLE_OFF_LO: return (uint8_t)sgu->svc_sample_offset;
        case SGU1_SVC_SAMPLE_OFF_HI: return (uint8_t)(sgu->svc_sample_offset >> 8);
        case SGU1_SVC_SAMPLE_BANK: return sgu->svc_sample_bank;
        case SGU1_SVC_SAMPLE_DATA: {
            size_t address = _sgu1_service_pcm_address(sgu);
            uint8_t data = 0;
            if (address < sgu->sgu.pcm_size) {
                data = (uint8_t)sgu->sgu.pcm[address];
            }
            sgu->svc_sample_offset++;
            return data;
        }
        case SGU1_SVC_MASTER_VOL: return sgu->svc_master_vol;
        // SGU1_SVC_CHIP_RESET is write-only and reads back as a reserved
        // offset, i.e. 0.
        default: return 0;
    }
}

/* CHIP_RESET ($18). Gated on the $A magic nybble; the low nybble selects what
   to reset. The core domains are only *requested* here -- performing them from
   the bus write would tear the register file out from under a render already in
   flight on another core, so the core latches the request and carries it out at
   its next sample boundary. The service registers are host-side and touched
   only by this bus, so they reset straight away.

   PCM sample memory is never cleared by any reset: it is host-loaded data
   rather than chip state, and a 64 KiB memset inside the 48 kHz sample deadline
   would overrun the hardware's render budget. */
static void _sgu1_service_chip_reset(sgu1_t* sgu, uint8_t data) {
    if ((data & SGU1_RESET_MAGIC_MASK) != SGU1_RESET_MAGIC) {
        return;
    }
    uint32_t parts = 0;
    if (data & SGU1_RESET_VOICES) {
        parts |= SGU_RESET_VOICES;
    }
    if (data & SGU1_RESET_TIMEBASE) {
        parts |= SGU_RESET_TIMEBASE;
    }
    if (data & SGU1_RESET_MIX) {
        parts |= SGU_RESET_MIX;
    }
    if (parts) {
        SGU_RequestReset(&sgu->sgu, parts);
    }
    if (data & SGU1_RESET_SVC) {
        _sgu1_service_reset(sgu);
    }
}

static void _sgu1_service_write(sgu1_t* sgu, uint8_t reg, uint8_t data) {
    switch (reg) {
        case SGU1_SVC_CHIP_RESET: _sgu1_service_chip_reset(sgu, data); break;
        // Identification block and STATUS are read-only; writes are dropped
        // rather than falling through to the reserved default, so the intent is
        // on the record.
        case SGU1_SVC_VER_MAJOR:
        case SGU1_SVC_VER_MINOR:
        case SGU1_SVC_PCM_BANKS:
        case SGU1_SVC_SVC_BANKS:
        case SGU1_SVC_STATUS: break;
        case SGU1_SVC_SAMPLE_OFF_LO: sgu->svc_sample_offset = (sgu->svc_sample_offset & 0xFF00u) | data; break;
        case SGU1_SVC_SAMPLE_OFF_HI:
            sgu->svc_sample_offset = (uint16_t)((sgu->svc_sample_offset & 0x00FFu) | ((uint16_t)data << 8));
            break;
        case SGU1_SVC_SAMPLE_BANK: sgu->svc_sample_bank = data; break;
        case SGU1_SVC_SAMPLE_DATA: {
            size_t address = _sgu1_service_pcm_address(sgu);
            if (address < sgu->sgu.pcm_size) {
                sgu->sgu.pcm[address] = (int8_t)data;
            }
            sgu->svc_sample_offset++;
            break;
        }
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
