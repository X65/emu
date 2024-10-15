#pragma once
/*
    # ria816.h

    RIA for 65816 CPU - RaspberryPi Interface Adapter

    ## Emulated Pins
    *************************************
    *           +-----------+           *
    *   RS0 --->|           |<--- CA1   *
    *        ...|           |<--> CA2   *
    *   RS3 --->|           |           *
    *           |           |<--> PA0   *
    *   CS1 --->|           |...        *
    *   CS2 --->|           |<--> PA7   *
    *           |           |           *
    *    D0 <-->|           |           *
    *        ...|   RIA     |<--> CB1   *
    *    D7 <-->|           |<--> CB2   *
    *           |           |           *
    *    RW --->|           |<--> PB0   *
    *   IRQ <---|           |...        *
    *           |           |<--> PB7   *
    *           |           |           *
    *           +-----------+           *
    *************************************

    ## How to use

    Call ria816_init() to initialize a new ria816_t instance (note that
    there is no ria816_desc_t struct:

    ~~~C
    ria816_t ria;
    ria816_init(&ria);
    ~~~

    In each system tick, call the ria816_tick() function, this takes
    an input pin mask, and returns a (potentially modified) output
    pin mask.

    Depending on the emulated system, the I/O and control pins
    PA0..PA7, PB0..PB7, CA1, CA2, CB1 and CB2 must be set as needed
    in the input pin mask (these are often connected to the keyboard
    matrix or peripheral devices).

    If the CPU wants to read or write RIA registers, set the CS1 pin
    to 1 (keep CS2 at 0), and set the RW pin depending on whether it's
    a register read (RW=1 means read, RW=0 means write, just like
    on the M6502 CPU), and the RS0..RS3 register select pins
    (usually identical with the shared address bus pins A0..A4).

    Note that the pin positions for RS0..RS3 and RW are shared with the
    respective M6502 pins.

    On return ria816_tick() returns a modified pin mask where the following
    pins might have changed state:

    - the IRQ pin (same bit position as M6502_IRQ)
    - the port A I/O pins PA0..PA7
    - the port A control pins CA1 and CA2
    - the port B I/O pins PB0..PB7
    - the port B control pins CB1 and CB2
    - data bus pins D0..D7 if this was a register read function.

    For an example RIA ticking code, checkout the _vic20_tick() function
    in systems/vic20.h

    To reset a ria816_t instance, call ria816_reset():

    ~~~C
    ria816_reset(&sys->ria);
    ~~~

*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// register select same as lower 4 shared address bus bits
#define RIA816_PIN_RS0 (0)
#define RIA816_PIN_RS1 (1)
#define RIA816_PIN_RS2 (2)
#define RIA816_PIN_RS3 (3)

// data bus pins shared with CPU
#define RIA816_PIN_D0 (16)
#define RIA816_PIN_D1 (17)
#define RIA816_PIN_D2 (18)
#define RIA816_PIN_D3 (19)
#define RIA816_PIN_D4 (20)
#define RIA816_PIN_D5 (21)
#define RIA816_PIN_D6 (22)
#define RIA816_PIN_D7 (23)

// control pins shared with CPU
#define RIA816_PIN_RW  (24)  // same as M6502_RW
#define RIA816_PIN_IRQ (26)  // same as M6502_IRQ

// control pins
#define RIA816_PIN_CS (40)  // chip-select 1, to select: CS1 high, CS2 low

#define RIA816_PIN_CA1 (42)  // peripheral A control 1
#define RIA816_PIN_CA2 (43)  // peripheral A control 2
#define RIA816_PIN_CB1 (44)  // peripheral B control 1
#define RIA816_PIN_CB2 (45)  // peripheral B control 2

// peripheral A port
#define RIA816_PIN_PA0 (48)
#define RIA816_PIN_PA1 (49)
#define RIA816_PIN_PA2 (50)
#define RIA816_PIN_PA3 (51)
#define RIA816_PIN_PA4 (52)
#define RIA816_PIN_PA5 (53)
#define RIA816_PIN_PA6 (54)
#define RIA816_PIN_PA7 (55)

// peripheral B port
#define RIA816_PIN_PB0 (56)
#define RIA816_PIN_PB1 (57)
#define RIA816_PIN_PB2 (58)
#define RIA816_PIN_PB3 (59)
#define RIA816_PIN_PB4 (60)
#define RIA816_PIN_PB5 (61)
#define RIA816_PIN_PB6 (62)
#define RIA816_PIN_PB7 (63)

// pin bit masks
#define RIA816_RS0     (1ULL << RIA816_PIN_RS0)
#define RIA816_RS1     (1ULL << RIA816_PIN_RS1)
#define RIA816_RS2     (1ULL << RIA816_PIN_RS2)
#define RIA816_RS3     (1ULL << RIA816_PIN_RS3)
#define RIA816_RS_PINS (0x0FULL)
#define RIA816_D0      (1ULL << RIA816_PIN_D0)
#define RIA816_D1      (1ULL << RIA816_PIN_D1)
#define RIA816_D2      (1ULL << RIA816_PIN_D2)
#define RIA816_D3      (1ULL << RIA816_PIN_D3)
#define RIA816_D4      (1ULL << RIA816_PIN_D4)
#define RIA816_D5      (1ULL << RIA816_PIN_D5)
#define RIA816_D6      (1ULL << RIA816_PIN_D6)
#define RIA816_D7      (1ULL << RIA816_PIN_D7)
#define RIA816_DB_PINS (0xFF0000ULL)
#define RIA816_RW      (1ULL << RIA816_PIN_RW)
#define RIA816_IRQ     (1ULL << RIA816_PIN_IRQ)
#define RIA816_CS      (1ULL << RIA816_PIN_CS)
#define RIA816_CA1     (1ULL << RIA816_PIN_CA1)
#define RIA816_CA2     (1ULL << RIA816_PIN_CA2)
#define RIA816_CB1     (1ULL << RIA816_PIN_CB1)
#define RIA816_CB2     (1ULL << RIA816_PIN_CB2)
#define RIA816_CA_PINS (RIA816_CA1 | RIA816_CA2)
#define RIA816_CB_PINS (RIA816_CB1 | RIA816_CB2)
#define RIA816_PA0     (1ULL << RIA816_PIN_PA0)
#define RIA816_PA1     (1ULL << RIA816_PIN_PA1)
#define RIA816_PA2     (1ULL << RIA816_PIN_PA2)
#define RIA816_PA3     (1ULL << RIA816_PIN_PA3)
#define RIA816_PA4     (1ULL << RIA816_PIN_PA4)
#define RIA816_PA5     (1ULL << RIA816_PIN_PA5)
#define RIA816_PA6     (1ULL << RIA816_PIN_PA6)
#define RIA816_PA7     (1ULL << RIA816_PIN_PA7)
#define RIA816_PA_PINS \
    (RIA816_PA0 | RIA816_PA1 | RIA816_PA2 | RIA816_PA3 | RIA816_PA4 | RIA816_PA5 | RIA816_PA6 | RIA816_PA7)
#define RIA816_PB0 (1ULL << RIA816_PIN_PB0)
#define RIA816_PB1 (1ULL << RIA816_PIN_PB1)
#define RIA816_PB2 (1ULL << RIA816_PIN_PB2)
#define RIA816_PB3 (1ULL << RIA816_PIN_PB3)
#define RIA816_PB4 (1ULL << RIA816_PIN_PB4)
#define RIA816_PB5 (1ULL << RIA816_PIN_PB5)
#define RIA816_PB6 (1ULL << RIA816_PIN_PB6)
#define RIA816_PB7 (1ULL << RIA816_PIN_PB7)
#define RIA816_PB_PINS \
    (RIA816_PB0 | RIA816_PB1 | RIA816_PB2 | RIA816_PB3 | RIA816_PB4 | RIA816_PB5 | RIA816_PB6 | RIA816_PB7)

// register indices
#define RIA816_REG_RB     (0)  /* input/output register B */
#define RIA816_REG_RA     (1)  /* input/output register A */
#define RIA816_REG_DDRB   (2)  /* data direction B */
#define RIA816_REG_DDRA   (3)  /* data direction A */
#define RIA816_REG_T1CL   (4)  /* T1 low-order latch / counter */
#define RIA816_REG_T1CH   (5)  /* T1 high-order counter */
#define RIA816_REG_T1LL   (6)  /* T1 low-order latches */
#define RIA816_REG_T1LH   (7)  /* T1 high-order latches */
#define RIA816_REG_T2CL   (8)  /* T2 low-order latch / counter */
#define RIA816_REG_T2CH   (9)  /* T2 high-order counter */
#define RIA816_REG_SR     (10) /* shift register */
#define RIA816_REG_ACR    (11) /* auxiliary control register */
#define RIA816_REG_PCR    (12) /* peripheral control register */
#define RIA816_REG_IFR    (13) /* interrupt flag register */
#define RIA816_REG_IER    (14) /* interrupt enable register */
#define RIA816_REG_RA_NOH (15) /* input/output A without handshake */
#define RIA816_NUM_REGS   (16)

// ria816 state
typedef struct {
    uint64_t pins;
} ria816_t;

// extract 8-bit data bus from 64-bit pins
#define RIA816_GET_DATA(p) ((uint8_t)((p) >> 16))
// merge 8-bit data bus value into 64-bit pins
#define RIA816_SET_DATA(p, d) \
    { p = (((p) & ~0xFF0000ULL) | (((d) << 16) & 0xFF0000ULL)); }

// initialize a new 6522 instance
void ria816_init(ria816_t* ria816);
// reset an existing 6522 instance
void ria816_reset(ria816_t* ria816);
// tick the ria816
uint64_t ria816_tick(ria816_t* ria816, uint64_t pins);

#ifdef __cplusplus
}  // extern "C"
#endif
