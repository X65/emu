#pragma once
/*
    # ymf825.h

    Yamaha YMF825 emulator (aka SD-1)

    ## Emulated Pins

    ***********************************
    *           +-----------+         *
    *    CS --->|           |<--- A0  *
    *    RW --->|           |...      *
    *           |           |<--- A4  *
    *           |   YMF825  |         *
    *           |           |<--> D0  *
    *           |           |...      *
    *           |           |<--> D7  *
    *           |           |         *
    *           +-----------+         *
    ***********************************

    The emulation exposes "real" address/data bus in place of SPI interface.

    The emulation has an additional "virtual pin" which is set to active
    whenever a new sample is ready (YMF825_SAMPLE).

    ## Links

    - https://device.yamaha.com/ja/lsi/products/sound_generator/images/4MF825A40.pdf
    - https://github.com/danielrfry/ymf825board
    - https://github.com/danielrfry/opl2sd1

*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// address bus pins A0..A4
#define YMF825_PIN_A0 (0)
#define YMF825_PIN_A1 (1)
#define YMF825_PIN_A2 (2)
#define YMF825_PIN_A3 (3)
#define YMF825_PIN_A4 (4)

// data bus pins D0..D7
#define YMF825_PIN_D0 (16)
#define YMF825_PIN_D1 (17)
#define YMF825_PIN_D2 (18)
#define YMF825_PIN_D3 (19)
#define YMF825_PIN_D4 (20)
#define YMF825_PIN_D5 (21)
#define YMF825_PIN_D6 (22)
#define YMF825_PIN_D7 (23)

// shared control pins
#define YMF825_PIN_RW (24) /* same as M6502_RW */

// chip-specific pins
#define YMF825_PIN_CS     (40) /* chip-select */
#define YMF825_PIN_SAMPLE (41) /* virtual "audio sample ready" pin */

// pin bit masks
#define YMF825_A0        (1ULL << YMF825_PIN_A0)
#define YMF825_A1        (1ULL << YMF825_PIN_A1)
#define YMF825_A2        (1ULL << YMF825_PIN_A2)
#define YMF825_A3        (1ULL << YMF825_PIN_A3)
#define YMF825_A4        (1ULL << YMF825_PIN_A4)
#define YMF825_ADDR_MASK (0x1F)
#define YMF825_D0        (1ULL << YMF825_PIN_D0)
#define YMF825_D1        (1ULL << YMF825_PIN_D1)
#define YMF825_D2        (1ULL << YMF825_PIN_D2)
#define YMF825_D3        (1ULL << YMF825_PIN_D3)
#define YMF825_D4        (1ULL << YMF825_PIN_D4)
#define YMF825_D5        (1ULL << YMF825_PIN_D5)
#define YMF825_D6        (1ULL << YMF825_PIN_D6)
#define YMF825_D7        (1ULL << YMF825_PIN_D7)
#define YMF825_RW        (1ULL << YMF825_PIN_RW)
#define YMF825_CS        (1ULL << YMF825_PIN_CS)
#define YMF825_SAMPLE    (1ULL << YMF825_PIN_SAMPLE)

// registers
#define YMF825_I_CLK_EN     (0)  // Clock Enable
#define YMF825_I_RESET      (1)  // Reset
#define YMF825_I_AN_PWR     (2)  // Analog Block Power - down control
#define YMF825_I_SPK_GAIN   (3)  // Speaker Amplifier Gain Setting
#define YMF825_I_HW_ID      (4)  // Hardware ID
#define YMF825_I_INT_0      (5)  // Interrupt
#define YMF825_I_INT_1      (6)
#define YMF825_I_DATA_W     (7)  // Contents Data Write Port
#define YMF825_I_SEQ_0      (8)  // Sequencer Setting
#define YMF825_I_SEQ_1      (9)
#define YMF825_I_SEQ_2      (10)
#define YMF825_I_SYNTH_0    (11)  // Synthesizer Setting
#define YMF825_I_SYNTH_1    (12)
#define YMF825_I_SYNTH_2    (13)
#define YMF825_I_SYNTH_3    (14)
#define YMF825_I_SYNTH_4    (15)
#define YMF825_I_SYNTH_5    (16)
#define YMF825_I_SYNTH_6    (17)
#define YMF825_I_SYNTH_7    (18)
#define YMF825_I_SYNTH_8    (19)
#define YMF825_I_SYNTH_9    (20)
#define YMF825_I_CTRL_0     (21)  // Control Register Read Port
#define YMF825_I_CTRL_1     (22)
#define YMF825_I_SEQ_TM_0   (23)  // Sequencer Time unit Setting
#define YMF825_I_SEQ_TM_1   (24)
#define YMF825_I_VOLUME     (25)  // Master Volume
#define YMF825_I_SW_RESET   (26)  // Soft Reset
#define YMF825_I_DITIME     (27)  // Sequencer Delay, Recovery Function Setting, Volume Interpolation Setting
#define YMF825_I_LFO_RESET  (28)  // LFO Reset
#define YMF825_I_PWR_RAIL   (29)  // Power Rail Selection
#define YMF825_I_RESERVED_0 (30)
#define YMF825_I_RESERVED_1 (31)
#define YMF825_W_CEQ0       (32)  // EQ BAND0 coefficient Write Port
#define YMF825_W_CEQ1       (33)  // EQ BAND1 coefficient Write Port
#define YMF825_W_CEQ2       (34)  // EQ BAND2 coefficient Write Port
#define YMF825_CEQ00_HI     (35)  // Equalizer Coefficient Read Ports
#define YMF825_CEQ00_MD     (36)
#define YMF825_CEQ00_LO     (37)
#define YMF825_CEQ01_HI     (38)
#define YMF825_CEQ01_MD     (39)
#define YMF825_CEQ01_LO     (40)
#define YMF825_CEQ02_HI     (41)
#define YMF825_CEQ02_MD     (42)
#define YMF825_CEQ02_LO     (43)
#define YMF825_CEQ03_HI     (44)
#define YMF825_CEQ03_MD     (45)
#define YMF825_CEQ03_LO     (46)
#define YMF825_CEQ04_HI     (47)
#define YMF825_CEQ04_MD     (48)
#define YMF825_CEQ04_LO     (49)
#define YMF825_CEQ10_HI     (50)
#define YMF825_CEQ10_MD     (51)
#define YMF825_CEQ10_LO     (52)
#define YMF825_CEQ11_HI     (53)
#define YMF825_CEQ11_MD     (54)
#define YMF825_CEQ11_LO     (55)
#define YMF825_CEQ12_HI     (56)
#define YMF825_CEQ12_MD     (57)
#define YMF825_CEQ12_LO     (58)
#define YMF825_CEQ13_HI     (59)
#define YMF825_CEQ13_MD     (60)
#define YMF825_CEQ13_LO     (61)
#define YMF825_CEQ14_HI     (62)
#define YMF825_CEQ14_MD     (63)
#define YMF825_CEQ14_LO     (64)
#define YMF825_CEQ20_HI     (65)
#define YMF825_CEQ20_MD     (66)
#define YMF825_CEQ20_LO     (67)
#define YMF825_CEQ21_HI     (68)
#define YMF825_CEQ21_MD     (69)
#define YMF825_CEQ21_LO     (70)
#define YMF825_CEQ22_HI     (71)
#define YMF825_CEQ22_MD     (72)
#define YMF825_CEQ22_LO     (73)
#define YMF825_CEQ23_HI     (74)
#define YMF825_CEQ23_MD     (75)
#define YMF825_CEQ23_LO     (76)
#define YMF825_CEQ24_HI     (77)
#define YMF825_CEQ24_MD     (78)
#define YMF825_CEQ24_LO     (79)
#define YMF825_COMM         (80)  // Software test communication
#define YMF825_NUM_REGS     (81)

// setup parameters for ymf825_init()
typedef struct {
    int tick_hz;      // frequency at which ymf825_tick() will be called in Hz
    int sound_hz;     // sound sample frequency
    float magnitude;  // output sample magnitude (0=silence to 1=max volume)
} ymf825_desc_t;

// ymf825 instance state
typedef struct {
    int sound_hz;
    // sample generation state
    int sample_period;
    int sample_counter;
    float sample_accum;
    float sample_accum_count;
    float sample_mag;
    float sample;
    // debug inspection
    uint64_t pins;
} ymf825_t;

// initialize a new ymf825_t instance
void ymf825_init(ymf825_t* sid, const ymf825_desc_t* desc);
// reset a ymf825_t instance
void ymf825_reset(ymf825_t* sid);
// tick a ymf825_t instance
uint64_t ymf825_tick(ymf825_t* sid, uint64_t pins);

#ifdef __cplusplus
}  // extern "C"
#endif
