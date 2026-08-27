#pragma once
/*#
    # sgu1.h

    SGU-1 Sound Generator Unit 1 emulation

    ## Emulated Pins

    ***********************************
    *           +-----------+         *
    *    CS --->|           |<--- A0  *
    *    RW --->|           |...      *
    *           |           |<--- A5  *
    *           |   SGU-1   |         *
    *           |           |<--> D0  *
    *           |           |...      *
    *           |           |<--> D7  *
    *           |           |         *
    *           +-----------+         *
    ***********************************

    The emulation has an additional "virtual pin" which is set to active
    whenever a new sample is ready (SGU1_SAMPLE).

    ## Links

    - https://tildearrow.org/furnace/doc/latest/4-instrument/su.html

    ## 0BSD license

    Copyright (c) 2025 Tomasz Sterna

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#*/

#include <speex_resampler.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// sgu.h is a plain C header and carries no linkage guard of its own, so giving
// it one is this header's job. Its standard includes are pulled in above,
// outside the block, leaving only its own declarations inside it.
#include "sgu-1/sgu.h"

// address bus pins A0..A5
#define SGU1_PIN_A0 (0)
#define SGU1_PIN_A1 (1)
#define SGU1_PIN_A2 (2)
#define SGU1_PIN_A3 (3)
#define SGU1_PIN_A4 (4)
#define SGU1_PIN_A5 (5)

// data bus pins D0..D7
#define SGU1_PIN_D0 (16)
#define SGU1_PIN_D1 (17)
#define SGU1_PIN_D2 (18)
#define SGU1_PIN_D3 (19)
#define SGU1_PIN_D4 (20)
#define SGU1_PIN_D5 (21)
#define SGU1_PIN_D6 (22)
#define SGU1_PIN_D7 (23)

// shared control pins
#define SGU1_PIN_RW (24) /* same as M6502_RW */

// chip-specific pins
#define SGU1_PIN_CS     (40) /* chip-select */
#define SGU1_PIN_SAMPLE (41) /* virtual "audio sample ready" pin */

// pin bit masks
#define SGU1_A0        (1ULL << SGU1_PIN_A0)
#define SGU1_A1        (1ULL << SGU1_PIN_A1)
#define SGU1_A2        (1ULL << SGU1_PIN_A2)
#define SGU1_A3        (1ULL << SGU1_PIN_A3)
#define SGU1_A4        (1ULL << SGU1_PIN_A4)
#define SGU1_A5        (1ULL << SGU1_PIN_A5)
#define SGU1_ADDR_MASK (0x3F)
#define SGU1_D0        (1ULL << SGU1_PIN_D0)
#define SGU1_D1        (1ULL << SGU1_PIN_D1)
#define SGU1_D2        (1ULL << SGU1_PIN_D2)
#define SGU1_D3        (1ULL << SGU1_PIN_D3)
#define SGU1_D4        (1ULL << SGU1_PIN_D4)
#define SGU1_D5        (1ULL << SGU1_PIN_D5)
#define SGU1_D6        (1ULL << SGU1_PIN_D6)
#define SGU1_D7        (1ULL << SGU1_PIN_D7)
#define SGU1_RW        (1ULL << SGU1_PIN_RW)
#define SGU1_CS        (1ULL << SGU1_PIN_CS)
#define SGU1_SAMPLE    (1ULL << SGU1_PIN_SAMPLE)

#define SGU1_PCM_BANKS (4)

// Service bank ($FF channel select) register map. The identification block
// occupies $00..$0F, the status block starts at $10 and the control block at
// $18; the gaps between them are reserved so either block can grow without
// moving anything.
#define SGU1_SERVICE_BANK (0xFF)

#define SGU1_SVC_MAGIC         (0x00)  // $00..$03, "SGU1"
#define SGU1_SVC_MAGIC_END     (0x03)
#define SGU1_SVC_VER_MAJOR     (0x04)
#define SGU1_SVC_VER_MINOR     (0x05)
#define SGU1_SVC_UNIQUE_ID     (0x06)  // $06..$0D, 8 bytes
#define SGU1_SVC_UNIQUE_ID_END (0x0D)
#define SGU1_SVC_UNIQUE_ID_LEN (8)
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

// STATUS ($10) bits. Read-to-clear.
#define SGU1_STATUS_CLIP (1 << 0)  // the output stage saturated at least once

// CHIP_RESET ($18): the high nybble must be the magic $A or the write is
// ignored, so a stray store cannot silence the chip. The low nybble names what
// to reset; $A0 is a no-op, $A7 the whole core, $AF core plus service state.
#define SGU1_RESET_MAGIC      (0xA0)
#define SGU1_RESET_MAGIC_MASK (0xF0)
#define SGU1_RESET_VOICES     (1 << 0)  // -> SGU_RESET_VOICES
#define SGU1_RESET_TIMEBASE   (1 << 1)  // -> SGU_RESET_TIMEBASE
#define SGU1_RESET_MIX        (1 << 2)  // -> SGU_RESET_MIX
#define SGU1_RESET_SVC        (1 << 3)  // the service registers, host-side

#define SGU1_AUDIO_CHANNELS (2)
#define SGU1_AUDIO_SAMPLES  (1024)

// setup parameters for sgu1_init()
typedef struct {
    int tick_hz;            // frequency at which sgu1_tick() will be called in Hz
    float magnitude;        // output sample magnitude (0=silence to 1=max volume)
    const char* dump_file;  // if set, dump registers to this file at end of frames with writes
} sgu1_desc_t;

// tsu instance state
typedef struct {
    // sound unit instance
    struct SGU sgu;
    uint8_t selected_channel;
    uint16_t svc_sample_offset;
    uint8_t svc_sample_bank;
    uint8_t svc_master_vol;
    // Guest-visible STATUS latch, read-to-clear through $10. Accumulates the
    // whole status word the core hands over, not just the bits this wrapper
    // understands today, and is the core's width rather than the register's so
    // a future flag above bit 7 cannot be dropped on the way in.
    uint32_t svc_status;
    // Monotonic count of clip events, for the debug UI. Never cleared by a
    // register read, so the UI and guest software cannot starve each other of
    // clip events. A host-side diagnostic like frame_counter: it survives a
    // chip reset and is zeroed only by sgu1_init.
    uint32_t clip_count;
    int tick_period;
    int tick_counter;
    // sample generation state
    float sample_mag;
    float sample[SGU1_AUDIO_CHANNELS];  // Left, Right
    // voice visualization
    struct {
        int sample_pos;
        float sample_buffer[SGU1_AUDIO_SAMPLES];
    } voice[SGU_CHNS];
    // per-frame register dump (enabled via sgu1_desc_t.dump_file)
    FILE* dump_file;         // open dump file, or NULL when disabled
    uint32_t frame_counter;  // running emulated-frame number used as dump timestamp
    bool dirty;              // a register was written during the current frame
    // debug inspection
    uint64_t pins;
} sgu1_t;

// initialize a new sgu1_t instance
void sgu1_init(sgu1_t* sgu, const sgu1_desc_t* desc);
// reset a sgu1_t instance
void sgu1_reset(sgu1_t* sgu);
// discard a sgu1_t instance (closes the register dump file if open)
void sgu1_discard(sgu1_t* sgu);
// tick a sgu1_t instance
uint64_t sgu1_tick(sgu1_t* sgu, uint64_t pins);

// emit the register dump for the frame just ended (no-op unless dump file is
// open and a register was written this frame); advances the frame counter
void sgu1_dump_frame(sgu1_t* sgu);

// Read a service-bank register for inspection: no auto-increment on the sample
// data port, no read-to-clear on STATUS, no change to the channel select. Use
// this from the debug UI -- sgu1_reg_read would move the sample pointer every
// frame and eat the guest's status latch just by having a window open.
uint8_t sgu1_svc_peek(const sgu1_t* sgu, uint8_t reg);

// for use by debugger
uint8_t sgu1_reg_read(sgu1_t* sgu, uint8_t reg);
void sgu1_reg_write(sgu1_t* sgu, uint8_t reg, uint8_t data);
void sgu1_direct_reg_write(sgu1_t* sgu, uint16_t reg, uint8_t data);

#ifdef __cplusplus
}  // extern "C"
#endif
