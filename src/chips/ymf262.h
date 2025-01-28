#pragma once
/*
    ymf262.h   -- YMF262 (OPL3) sound chip emulator

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

    Copyright (c) 2018 Tomasz Sterna
*/

#include <Nuked-OPL3/opl3.h>

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// lower 2 bits shared address bus
#define YMF262_PIN_A0 (0)
#define YMF262_PIN_A1 (1)

// 8 bits shared with CPU data bus
#define YMF262_PIN_D0 (16)
#define YMF262_PIN_D1 (17)
#define YMF262_PIN_D2 (18)
#define YMF262_PIN_D3 (19)
#define YMF262_PIN_D4 (20)
#define YMF262_PIN_D5 (21)
#define YMF262_PIN_D6 (22)
#define YMF262_PIN_D7 (23)

// control pins shared with CPU
#define YMF262_PIN_RW (24)  // YMF262 has separate WR/RD pins - ignore it for simplicity
#define YMF262_PIN_IC (34)  // "Initial clear" wired to CPU RESET

// chip-specific control pins
#define YMF262_PIN_CS  (40)
#define YMF262_PIN_IRQ (41)

// virtual 'audio sample ready' pin
#define YMF262_PIN_SAMPLE (42)

// pin bit masks
#define YMF262_A0     (1ULL << YMF262_PIN_A0)
#define YMF262_A1     (1ULL << YMF262_PIN_A1)
#define YMF262_D0     (1ULL << YMF262_PIN_D0)
#define YMF262_D1     (1ULL << YMF262_PIN_D1)
#define YMF262_D2     (1ULL << YMF262_PIN_D2)
#define YMF262_D3     (1ULL << YMF262_PIN_D3)
#define YMF262_D4     (1ULL << YMF262_PIN_D4)
#define YMF262_D5     (1ULL << YMF262_PIN_D5)
#define YMF262_D6     (1ULL << YMF262_PIN_D6)
#define YMF262_D7     (1ULL << YMF262_PIN_D7)
#define YMF262_IC     (1ULL << YMF262_PIN_IC)
#define YMF262_RW     (1ULL << YMF262_PIN_RW)
#define YMF262_CS     (1ULL << YMF262_PIN_CS)
#define YMF262_IRQ    (1ULL << YMF262_PIN_IRQ)
#define YMF262_SAMPLE (1ULL << YMF262_PIN_SAMPLE)

// number of registers
#define YMF262_NUM_REGISTERS (0x100)
// number of register banks
#define YMF262_NUM_BANKS (0x2)
// error-accumulation precision boost
#define YMF262_FIXEDPOINT_SCALE (16)

// setup parameters for ymf262_init() call
typedef struct {
    int tick_hz;  /* frequency at which ymf262_tick() will be called in Hz */
    int sound_hz; /* number of samples that will be produced per second */
} ymf262_desc_t;

// YMF262 state
typedef struct {
    uint8_t addr[YMF262_NUM_BANKS];  // register address latch

    uint64_t pins;  // last pin state for debug inspection

    int sound_hz;    // keep samplerate for chip resets
    opl3_chip opl3;  // wrapped opl3 chip emulator

    // sample generation state
    int sample_period;
    int sample_counter;
    int16_t samples[4];
} ymf262_t;

#define YMF262_ADDR_MASK (0x3ULL)
/* extract 2-bit address bus from 64-bit pins */
#define YMF262_GET_ADDR(p) ((uint8_t)((p) & YMF262_ADDR_MASK))
// extract 8-bit data bus from 64-bit pins
#define YMF262_GET_DATA(p) ((uint8_t)((p) >> 16))
// merge 8-bit data bus value into 64-bit pins
#define YMF262_SET_DATA(p, d) \
    { p = ((p) & ~0xFF0000ULL) | (((d) << 16) & 0xFF0000ULL); }

// initialize a YMF262 instance
void ymf262_init(ymf262_t* ymf, const ymf262_desc_t* desc);
// reset an existing YMF262 instance
void ymf262_reset(ymf262_t* ymf);
// tick the YMF262, return true if a new sample is ready
uint64_t ymf262_tick(ymf262_t* ymf, uint64_t pins);
// prepare ymf262_t snapshot for saving
void ymf262_snapshot_onsave(ymf262_t* snapshot);
// fixup ymf262_t snapshot after loading
void ymf262_snapshot_onload(ymf262_t* snapshot, ymf262_t* sys);

#ifdef __cplusplus
}  // extern "C"
#endif
