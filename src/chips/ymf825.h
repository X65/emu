#pragma once
/*
    ymf825.h   -- YMF825 (SD-1 Sound Designer 1) sound chip emulator

    Optionally provide the following macros with your own implementation

    CHIPS_ASSERT(c)     -- your own assert macro (default: assert(c))

    EMULATED PINS:

             +-----------+
       CS -->|           |<-> D0
      R/W -->|           |...
             |           |<-> D7
             |           |
             |           |<-- A0
             |           |<-- A1
             |           |
             |           |
             |           |--> IRQ
             |           |
             |           |<-- IC
             +-----------+

    NOT EMULATED:

    - the RESET pin state is ignored
    - IRQ is not generated
    - status register always reads 0

    ## 0BSD license

    Copyright (c) 2025 Tomasz Sterna
*/

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// lower 6 bits shared address bus
#define YMF825_PIN_A0 (0)
#define YMF825_PIN_A1 (1)
#define YMF825_PIN_A2 (2)
#define YMF825_PIN_A3 (3)
#define YMF825_PIN_A4 (4)
#define YMF825_PIN_A5 (5)

// 8 bits shared with CPU data bus
#define YMF825_PIN_D0 (16)
#define YMF825_PIN_D1 (17)
#define YMF825_PIN_D2 (18)
#define YMF825_PIN_D3 (19)
#define YMF825_PIN_D4 (20)
#define YMF825_PIN_D5 (21)
#define YMF825_PIN_D6 (22)
#define YMF825_PIN_D7 (23)

// control pins shared with CPU
#define YMF825_PIN_RW (24)

// chip-specific control pins
#define YMF825_PIN_CS  (40)
#define YMF825_PIN_IRQ (41)

// virtual 'audio sample ready' pin
#define YMF825_PIN_SAMPLE (42)

// pin bit masks
#define YMF825_A0     (1ULL << YMF825_PIN_A0)
#define YMF825_A1     (1ULL << YMF825_PIN_A1)
#define YMF825_A2     (1ULL << YMF825_PIN_A2)
#define YMF825_A3     (1ULL << YMF825_PIN_A3)
#define YMF825_A4     (1ULL << YMF825_PIN_A4)
#define YMF825_A5     (1ULL << YMF825_PIN_A5)
#define YMF825_D0     (1ULL << YMF825_PIN_D0)
#define YMF825_D1     (1ULL << YMF825_PIN_D1)
#define YMF825_D2     (1ULL << YMF825_PIN_D2)
#define YMF825_D3     (1ULL << YMF825_PIN_D3)
#define YMF825_D4     (1ULL << YMF825_PIN_D4)
#define YMF825_D5     (1ULL << YMF825_PIN_D5)
#define YMF825_D6     (1ULL << YMF825_PIN_D6)
#define YMF825_D7     (1ULL << YMF825_PIN_D7)
#define YMF825_RW     (1ULL << YMF825_PIN_RW)
#define YMF825_CS     (1ULL << YMF825_PIN_CS)
#define YMF825_IRQ    (1ULL << YMF825_PIN_IRQ)
#define YMF825_SAMPLE (1ULL << YMF825_PIN_SAMPLE)

// number of registers
#define YMF825_NUM_REGISTERS (0x40)

// samplerate of the chip
#define YMF825_SAMPLE_RATE (48000)
// error-accumulation precision boost
#define YMF825_RESAMPLER_FRAC   (10)
#define YMF825_FIXEDPOINT_SCALE (16)

// setup parameters for ymf825_init() call
typedef struct {
    int tick_hz;  /* frequency at which ymf825_tick() will be called in Hz */
    int sound_hz; /* number of samples that will be produced per second */
} ymf825_desc_t;

// YMF825 state
typedef struct {
    uint64_t pins;  // last pin state for debug inspection

    int sound_hz;  // keep samplerate for chip resets

    // sample generation state
    int sample_period;
    int sample_counter;
    float samples[2];

    uint8_t registers[YMF825_NUM_REGISTERS];

    struct {
        int32_t rateratio;
        int32_t samplecnt;
        int16_t oldsamples[2];
        int16_t samples[2];
    } resampler;
} ymf825_t;

#define YMF825_ADDR_MASK (0x3FULL)
/* extract 6-bit address bus from 64-bit pins */
#define YMF825_GET_ADDR(p) ((uint8_t)((p) & YMF825_ADDR_MASK))
// extract 8-bit data bus from 64-bit pins
#define YMF825_GET_DATA(p) ((uint8_t)((p) >> 16))
// merge 8-bit data bus value into 64-bit pins
#define YMF825_SET_DATA(p, d) \
    { p = ((p) & ~0xFF0000ULL) | (((d) << 16) & 0xFF0000ULL); }

// initialize a YMF825 instance
void ymf825_init(ymf825_t* ymf, const ymf825_desc_t* desc);
// reset an existing YMF825 instance
void ymf825_reset(ymf825_t* ymf);
// tick the YMF825, return true if a new sample is ready
uint64_t ymf825_tick(ymf825_t* ymf, uint64_t pins);
// prepare ymf825_t snapshot for saving
void ymf825_snapshot_onsave(ymf825_t* snapshot);
// fixup ymf825_t snapshot after loading
void ymf825_snapshot_onload(ymf825_t* snapshot, ymf825_t* sys);

#ifdef __cplusplus
}  // extern "C"
#endif
