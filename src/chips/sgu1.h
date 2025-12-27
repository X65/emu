#pragma once
/*#
    # sgu1.h

    SGU-1 Sound Generator Unit 1 emulation

    ## Emulated Pins

    ***********************************
    *           +-----------+         *
    *    CS --->|           |<--- A0  *
    *    RW --->|           |...      *
    *           |           |<--- A4  *
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

    -

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

#include <stdint.h>
#include <stdbool.h>

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

// channel registers
#define SGU1_CHAN_FREQ_LO         (0)
#define SGU1_CHAN_FREQ_HI         (1)
#define SGU1_CHAN_VOL             (2)
#define SGU1_CHAN_PAN             (3)
#define SGU1_CHAN_FLAGS0          (4)
#define SGU1_CHAN_FLAGS1          (5)
#define SGU1_CHAN_CUTOFF_LO       (6)
#define SGU1_CHAN_CUTOFF_HI       (7)
#define SGU1_CHAN_DUTY            (8)
#define SGU1_CHAN_RESON           (9)
#define SGU1_CHAN_PCMPOS_LO       (10)
#define SGU1_CHAN_PCMPOS_HI       (11)
#define SGU1_CHAN_PCMBND_LO       (12)
#define SGU1_CHAN_PCMBND_HI       (13)
#define SGU1_CHAN_PCMRST_LO       (14)
#define SGU1_CHAN_PCMRST_HI       (15)
#define SGU1_CHAN_SWFREQ_SPEED_LO (16)
#define SGU1_CHAN_SWFREQ_SPEED_HI (17)
#define SGU1_CHAN_SWFREQ_AMT      (18)
#define SGU1_CHAN_SWFREQ_BOUND    (19)
#define SGU1_CHAN_SWVOL_SPEED_LO  (20)
#define SGU1_CHAN_SWVOL_SPEED_HI  (21)
#define SGU1_CHAN_SWVOL_AMT       (22)
#define SGU1_CHAN_SWVOL_BOUND     (23)
#define SGU1_CHAN_SWCUT_SPEED_LO  (24)
#define SGU1_CHAN_SWCUT_SPEED_HI  (25)
#define SGU1_CHAN_SWCUT_AMT       (26)
#define SGU1_CHAN_SWCUT_BOUND     (27)
#define SGU1_CHAN_SPECIAL1C       (28)
#define SGU1_CHAN_SPECIAL1D       (29)
#define SGU1_CHAN_RESTIMER_LO     (20)
#define SGU1_CHAN_RESTIMER_HI     (31)

// setup parameters for sgu1_init()
typedef struct {
    int tick_hz;      // frequency at which sgu1_tick() will be called in Hz
    int sound_hz;     // sound sample frequency
    float magnitude;  // output sample magnitude (0=silence to 1=max volume)
} sgu1_desc_t;

// tsu instance state
typedef struct {
    int sound_hz;
    // sound unit instance
    void* su;
    // sample generation state
    int sample_period;
    int sample_counter;
    float sample_mag;
    float sample;
    // debug inspection
    uint64_t pins;
} sgu1_t;

// initialize a new sgu1_t instance
void sgu1_init(sgu1_t* sgu, const sgu1_desc_t* desc);
// reset a sgu1_t instance
void sgu1_reset(sgu1_t* sgu);
// tick a sgu1_t instance
uint64_t sgu1_tick(sgu1_t* sgu, uint64_t pins);

#ifdef __cplusplus
}  // extern "C"
#endif
