#pragma once
/*#
    # tca6416a.h

    Texas Instruments TCA6416A 16-Bit I/O Expander

    This chip is originally I2C, but here is emulated as a
    memory mapped device with CS.

    Do this:
    ~~~C
    #define CHIPS_IMPL
    ~~~
    before you include this file in *one* C or C++ file to create the
    implementation.

    Optionally provide the following macros with your own implementation
    ~~~C
    CHIPS_ASSERT(c)
    ~~~

    ## Emulated Pins

    *************************************
    *           +------------+          *
    *    CS --->|            |          *
    *    RW --->|            |          *
    * RESET --->|            |          *
    *   INT <---|            |          *
    *           |            |          *
    *           |            |          *
    *   RS0 --->|  TCA6416A  |<--> P00  *
    *   RS1 --->|            |...       *
    *   RS2 --->|            |<--> P07  *
    *           |            |          *
    *           |            |<--> P00  *
    *    D0 --->|            |...       *
    *        ...|            |<--> P07  *
    *    D7 --->|            |          *
    *           +------------+          *
    *************************************

    ## NOT IMPLEMENTED:

    - SPI/I2C interface

    ## LINKS:
    - https://www.ti.com/lit/ds/symlink/tca6416a.pdf

    TODO: Documentation

    ## zlib/libpng license

    Copyright (c) 2025 Tomasz Sterna
    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source
        distribution.
#*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// register select same as lower 3 shared address bus bits
#define TCA6416A_PIN_RS0 (0)
#define TCA6416A_PIN_RS1 (1)
#define TCA6416A_PIN_RS2 (2)

// data bus pins shared with CPU
#define TCA6416A_PIN_D0 (16)
#define TCA6416A_PIN_D1 (17)
#define TCA6416A_PIN_D2 (18)
#define TCA6416A_PIN_D3 (19)
#define TCA6416A_PIN_D4 (20)
#define TCA6416A_PIN_D5 (21)
#define TCA6416A_PIN_D6 (22)
#define TCA6416A_PIN_D7 (23)

// control pins shared with CPU
#define TCA6416A_PIN_RW (24)  // same as M6502_RW

// chip-specific control pins
#define TCA6416A_PIN_CS    (40)
#define TCA6416A_PIN_INT   (41)
#define TCA6416A_PIN_RESET (42)

// port I/O pins
#define TCA6416A_PIN_P00 (48)
#define TCA6416A_PIN_P01 (49)
#define TCA6416A_PIN_P02 (50)
#define TCA6416A_PIN_P03 (51)
#define TCA6416A_PIN_P04 (52)
#define TCA6416A_PIN_P05 (53)
#define TCA6416A_PIN_P06 (54)
#define TCA6416A_PIN_P07 (55)

#define TCA6416A_PIN_P10 (56)
#define TCA6416A_PIN_P11 (57)
#define TCA6416A_PIN_P12 (58)
#define TCA6416A_PIN_P13 (59)
#define TCA6416A_PIN_P14 (60)
#define TCA6416A_PIN_P15 (61)
#define TCA6416A_PIN_P16 (62)
#define TCA6416A_PIN_P17 (63)

// pin bit masks
#define TCA6416A_RS0     (1ULL << TCA6416A_PIN_RS0)
#define TCA6416A_RS1     (1ULL << TCA6416A_PIN_RS1)
#define TCA6416A_RS2     (1ULL << TCA6416A_PIN_RS2)
#define TCA6416A_RS      (TCA6416A_RS2 | TCA6416A_RS1 | TCA6416A_RS0)
#define TCA6416A_D0      (1ULL << TCA6416A_PIN_D0)
#define TCA6416A_D1      (1ULL << TCA6416A_PIN_D1)
#define TCA6416A_D2      (1ULL << TCA6416A_PIN_D2)
#define TCA6416A_D3      (1ULL << TCA6416A_PIN_D3)
#define TCA6416A_D4      (1ULL << TCA6416A_PIN_D4)
#define TCA6416A_D5      (1ULL << TCA6416A_PIN_D5)
#define TCA6416A_D6      (1ULL << TCA6416A_PIN_D6)
#define TCA6416A_D7      (1ULL << TCA6416A_PIN_D7)
#define TCA6416A_DB_PINS (0xFF0000ULL)
#define TCA6416A_RW      (1ULL << TCA6416A_PIN_RW)
#define TCA6416A_INT     (1ULL << TCA6416A_PIN_INT)
#define TCA6416A_CS      (1ULL << TCA6416A_PIN_CS)
#define TCA6416A_RESET   (1ULL << TCA6416A_PIN_RESET)
#define TCA6416A_P00     (1ULL << TCA6416A_PIN_P00)
#define TCA6416A_P01     (1ULL << TCA6416A_PIN_P01)
#define TCA6416A_P02     (1ULL << TCA6416A_PIN_P02)
#define TCA6416A_P03     (1ULL << TCA6416A_PIN_P03)
#define TCA6416A_P04     (1ULL << TCA6416A_PIN_P04)
#define TCA6416A_P05     (1ULL << TCA6416A_PIN_P05)
#define TCA6416A_P06     (1ULL << TCA6416A_PIN_P06)
#define TCA6416A_P07     (1ULL << TCA6416A_PIN_P07)
#define TCA6416A_P0_PINS                                                                                    \
    (TCA6416A_P00 | TCA6416A_P01 | TCA6416A_P02 | TCA6416A_P03 | TCA6416A_P04 | TCA6416A_P05 | TCA6416A_P06 \
     | TCA6416A_P07)
#define TCA6416A_P10 (1ULL << TCA6416A_PIN_P10)
#define TCA6416A_P11 (1ULL << TCA6416A_PIN_P11)
#define TCA6416A_P12 (1ULL << TCA6416A_PIN_P12)
#define TCA6416A_P13 (1ULL << TCA6416A_PIN_P13)
#define TCA6416A_P14 (1ULL << TCA6416A_PIN_P14)
#define TCA6416A_P15 (1ULL << TCA6416A_PIN_P15)
#define TCA6416A_P16 (1ULL << TCA6416A_PIN_P16)
#define TCA6416A_P17 (1ULL << TCA6416A_PIN_P17)
#define TCA6416A_P1_PINS                                                                                    \
    (TCA6416A_P10 | TCA6416A_P11 | TCA6416A_P12 | TCA6416A_P13 | TCA6416A_P14 | TCA6416A_P15 | TCA6416A_P16 \
     | TCA6416A_P17)

// register indices
#define TCA6416A_REG_IN0  (0)  // Input Port 0
#define TCA6416A_REG_IN1  (1)  // Input Port 1
#define TCA6416A_REG_OUT0 (2)  // Output Port 0
#define TCA6416A_REG_OUT1 (3)  // Output Port 1
#define TCA6416A_REG_POL0 (4)  // Polarity Inversion 0
#define TCA6416A_REG_POL1 (5)  // Polarity Inversion 1
#define TCA6416A_REG_CFG0 (6)  // Configuration 0
#define TCA6416A_REG_CFG1 (7)  // Configuration 1

// interrupt sources
#define TCA6416A_INT_P0 (1 << 0)
#define TCA6416A_INT_P1 (1 << 1)

// I/O port state
typedef struct {
    uint8_t cfg;  // data direction register
    uint8_t in;   // input port register
    uint8_t out;  // output port register
    uint8_t pol;  // polarity inversion register
} tca6416a_port_t;

// tca6416a state
typedef struct {
    tca6416a_port_t p0;
    tca6416a_port_t p1;
    uint8_t intr;
    uint64_t pins;
} tca6416a_t;

// extract 8-bit data bus from 64-bit pins
#define TCA6416A_GET_DATA(p) ((uint8_t)((p) >> 16))
// merge 8-bit data bus value into 64-bit pins
#define TCA6416A_SET_DATA(p, d) \
    { p = (((p) & ~0xFF0000ULL) | (((d) << 16) & 0xFF0000ULL)); }
// extract port 0 pins
#define TCA6416A_GET_P0(p) ((uint8_t)((p) >> 48))
// extract port 1 pins
#define TCA6416A_GET_P1(p) ((uint8_t)((p) >> 56))
// merge port 0 pins into pin mask
#define TCA6416A_SET_P0(p, a) \
    { p = ((p) & 0xFF00FFFFFFFFFFFFULL) | (((a) & 0xFFULL) << 48); }
// merge port 1 pins into pin mask
#define TCA6416A_SET_P1(p, b) \
    { p = ((p) & 0x00FFFFFFFFFFFFFFULL) | (((b) & 0xFFULL) << 56); }
// merge port 0 and 1 pins into pin mask
#define TCA6416A_SET_P01(p, a, b) \
    { p = ((p) & 0x0000FFFFFFFFFFFFULL) | (((a) & 0xFFULL) << 48) | (((b) & 0xFFULL) << 56); }

// initialize a new tca6416a_t instance
void tca6416a_init(tca6416a_t* c, uint8_t p0, uint8_t p1);
// reset an existing tca6416a_t instance
void tca6416a_reset(tca6416a_t* c, uint8_t p0, uint8_t p1);
// tick the tca6416a_t instance
uint64_t tca6416a_tick(tca6416a_t* c, uint64_t pins);

#ifdef __cplusplus
}  // extern "C"
#endif
