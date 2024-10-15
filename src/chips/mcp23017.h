#pragma once
/*
    # mcp23017.h

    MCP23017 - general purpose parallel I/O expansion

    ## Emulated Pins

    ************************************
    *           +-----------+          *
    *    CS --->|           |<--- FLAG *
    *    RW --->|           |---> PC   *
    *   RES --->|           |---> SP   *
    *   IRQ <---|           |<--- TOD  *
    *           |           |<--- CNT  *
    *           |           |          *
    *   RS0 --->|  MCP23017 |<--> PA0  *
    *   RS1 --->|           |...       *
    *   RS2 --->|           |<--> PA7  *
    *   RS3 --->|           |          *
    *           |           |<--> PB0  *
    *   DB0 --->|           |...       *
    *        ...|           |<--> PB7  *
    *   DB7 --->|           |          *
    *           +-----------+          *
    ************************************

    ## LINKS:
    https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-Data-Sheet-DS20001952.pdf

    TODO: Documentation

*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// register select same as lower 4 shared address bus bits
#define MCP23017_PIN_RS0 (0)
#define MCP23017_PIN_RS1 (1)
#define MCP23017_PIN_RS2 (2)
#define MCP23017_PIN_RS3 (3)

// data bus pins shared with CPU
#define MCP23017_PIN_D0 (16)
#define MCP23017_PIN_D1 (17)
#define MCP23017_PIN_D2 (18)
#define MCP23017_PIN_D3 (19)
#define MCP23017_PIN_D4 (20)
#define MCP23017_PIN_D5 (21)
#define MCP23017_PIN_D6 (22)
#define MCP23017_PIN_D7 (23)

// control pins shared with CPU
#define MCP23017_PIN_RW  (24)  // same as M6502_RW
#define MCP23017_PIN_IRQ (26)  // same as M6502_IRQ

// chip-specific control pins
#define MCP23017_PIN_CS   (40)
#define MCP23017_PIN_FLAG (41)
#define MCP23017_PIN_PC   (42)
#define MCP23017_PIN_SP   (43)
#define MCP23017_PIN_TOD  (44)
#define MCP23017_PIN_CNT  (45)

// port I/O pins
#define MCP23017_PIN_PA0 (48)
#define MCP23017_PIN_PA1 (49)
#define MCP23017_PIN_PA2 (50)
#define MCP23017_PIN_PA3 (51)
#define MCP23017_PIN_PA4 (52)
#define MCP23017_PIN_PA5 (53)
#define MCP23017_PIN_PA6 (54)
#define MCP23017_PIN_PA7 (55)

#define MCP23017_PIN_PB0 (56)
#define MCP23017_PIN_PB1 (57)
#define MCP23017_PIN_PB2 (58)
#define MCP23017_PIN_PB3 (59)
#define MCP23017_PIN_PB4 (60)
#define MCP23017_PIN_PB5 (61)
#define MCP23017_PIN_PB6 (62)
#define MCP23017_PIN_PB7 (63)

// pin bit masks
#define MCP23017_RS0     (1ULL << MCP23017_PIN_RS0)
#define MCP23017_RS1     (1ULL << MCP23017_PIN_RS1)
#define MCP23017_RS2     (1ULL << MCP23017_PIN_RS2)
#define MCP23017_RS3     (1ULL << MCP23017_PIN_RS3)
#define MCP23017_RS      (MCP23017_RS3 | MCP23017_RS2 | MCP23017_RS1 | MCP23017_RS0)
#define MCP23017_D0      (1ULL << MCP23017_PIN_D0)
#define MCP23017_D1      (1ULL << MCP23017_PIN_D1)
#define MCP23017_D2      (1ULL << MCP23017_PIN_D2)
#define MCP23017_D3      (1ULL << MCP23017_PIN_D3)
#define MCP23017_D4      (1ULL << MCP23017_PIN_D4)
#define MCP23017_D5      (1ULL << MCP23017_PIN_D5)
#define MCP23017_D6      (1ULL << MCP23017_PIN_D6)
#define MCP23017_D7      (1ULL << MCP23017_PIN_D7)
#define MCP23017_DB_PINS (0xFF0000ULL)
#define MCP23017_RW      (1ULL << MCP23017_PIN_RW)
#define MCP23017_IRQ     (1ULL << MCP23017_PIN_IRQ)
#define MCP23017_CS      (1ULL << MCP23017_PIN_CS)
#define MCP23017_FLAG    (1ULL << MCP23017_PIN_FLAG)
#define MCP23017_PC      (1ULL << MCP23017_PIN_PC)
#define MCP23017_SP      (1ULL << MCP23017_PIN_SP)
#define MCP23017_TOD     (1ULL << MCP23017_PIN_TOD)
#define MCP23017_CNT     (1ULL << MCP23017_PIN_CNT)
#define MCP23017_PA0     (1ULL << MCP23017_PIN_PA0)
#define MCP23017_PA1     (1ULL << MCP23017_PIN_PA1)
#define MCP23017_PA2     (1ULL << MCP23017_PIN_PA2)
#define MCP23017_PA3     (1ULL << MCP23017_PIN_PA3)
#define MCP23017_PA4     (1ULL << MCP23017_PIN_PA4)
#define MCP23017_PA5     (1ULL << MCP23017_PIN_PA5)
#define MCP23017_PA6     (1ULL << MCP23017_PIN_PA6)
#define MCP23017_PA7     (1ULL << MCP23017_PIN_PA7)
#define MCP23017_PA_PINS \
    (MCP23017_PA0 | MCP23017_PA1 | MCP23017_PA2 | MCP23017_PA3 | MCP23017_PA4 | MCP23017_PA5 | MCP23017_PA6 \
     | MCP23017_PA7)
#define MCP23017_PB0 (1ULL << MCP23017_PIN_PB0)
#define MCP23017_PB1 (1ULL << MCP23017_PIN_PB1)
#define MCP23017_PB2 (1ULL << MCP23017_PIN_PB2)
#define MCP23017_PB3 (1ULL << MCP23017_PIN_PB3)
#define MCP23017_PB4 (1ULL << MCP23017_PIN_PB4)
#define MCP23017_PB5 (1ULL << MCP23017_PIN_PB5)
#define MCP23017_PB6 (1ULL << MCP23017_PIN_PB6)
#define MCP23017_PB7 (1ULL << MCP23017_PIN_PB7)
#define MCP23017_PB_PINS \
    (MCP23017_PB0 | MCP23017_PB1 | MCP23017_PB2 | MCP23017_PB3 | MCP23017_PB4 | MCP23017_PB5 | MCP23017_PB6 \
     | MCP23017_PB7)

// register indices
#define MCP23017_REG_PRA     (0)   // peripheral data reg A
#define MCP23017_REG_PRB     (1)   // peripheral data reg B
#define MCP23017_REG_DDRA    (2)   // data direction reg A
#define MCP23017_REG_DDRB    (3)   // data direction reg B
#define MCP23017_REG_TALO    (4)   // timer A low register
#define MCP23017_REG_TAHI    (5)   // timer A high register
#define MCP23017_REG_TBLO    (6)   // timer B low register
#define MCP23017_REG_TBHI    (7)   // timer B high register
#define MCP23017_REG_TOD10TH (8)   // 10ths of seconds register
#define MCP23017_REG_TODSEC  (9)   // seconds register
#define MCP23017_REG_TODMIN  (10)  // minutes register
#define MCP23017_REG_TODHR   (11)  // hours am/pm register
#define MCP23017_REG_SDR     (12)  // serial data register
#define MCP23017_REG_ICR     (13)  // interrupt control register
#define MCP23017_REG_CRA     (14)  // control register A
#define MCP23017_REG_CRB     (15)  // control register B

// mcp23017 state
typedef struct {
    uint64_t pins;
} mcp23017_t;

// extract 8-bit data bus from 64-bit pins
#define MCP23017_GET_DATA(p) ((uint8_t)((p) >> 16))
// merge 8-bit data bus value into 64-bit pins
#define MCP23017_SET_DATA(p, d) \
    { p = (((p) & ~0xFF0000ULL) | (((d) << 16) & 0xFF0000ULL)); }
// extract port A pins
#define MCP23017_GET_PA(p) ((uint8_t)((p) >> 48))
// extract port B pins
#define MCP23017_GET_PB(p) ((uint8_t)((p) >> 56))
// merge port A pins into pin mask
#define MCP23017_SET_PA(p, a) \
    { p = ((p) & 0xFF00FFFFFFFFFFFFULL) | (((a) & 0xFFULL) << 48); }
// merge port B pins into pin mask
#define MCP23017_SET_PB(p, b) \
    { p = ((p) & 0x00FFFFFFFFFFFFFFULL) | (((b) & 0xFFULL) << 56); }
// merge port A and B pins into pin mask
#define MCP23017_SET_PAB(p, a, b) \
    { p = ((p) & 0x0000FFFFFFFFFFFFULL) | (((a) & 0xFFULL) << 48) | (((b) & 0xFFULL) << 56); }

// initialize a new mcp23017_t instance
void mcp23017_init(mcp23017_t* c);
// reset an existing mcp23017_t instance
void mcp23017_reset(mcp23017_t* c);
// tick the mcp23017_t instance
uint64_t mcp23017_tick(mcp23017_t* c, uint64_t pins);

#ifdef __cplusplus
}  // extern "C"
#endif
