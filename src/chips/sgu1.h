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
#include "snd/sgu.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#define SGU1_AUDIO_CHANNELS (2)
#define SGU1_AUDIO_SAMPLES  (1024)

// setup parameters for sgu1_init()
typedef struct {
    int tick_hz;      // frequency at which sgu1_tick() will be called in Hz
    float magnitude;  // output sample magnitude (0=silence to 1=max volume)
} sgu1_desc_t;

// tsu instance state
typedef struct {
    // sound unit instance
    struct SGU sgu;
    uint8_t selected_channel;
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
    // debug inspection
    uint64_t pins;
} sgu1_t;

// initialize a new sgu1_t instance
void sgu1_init(sgu1_t* sgu, const sgu1_desc_t* desc);
// reset a sgu1_t instance
void sgu1_reset(sgu1_t* sgu);
// tick a sgu1_t instance
uint64_t sgu1_tick(sgu1_t* sgu, uint64_t pins);

// for use by debugger
uint8_t sgu1_reg_read(sgu1_t* sgu, uint8_t reg);
void sgu1_reg_write(sgu1_t* sgu, uint8_t reg, uint8_t data);
void sgu1_direct_reg_write(sgu1_t* sgu, uint16_t reg, uint8_t data);

#ifdef __cplusplus
}  // extern "C"
#endif
