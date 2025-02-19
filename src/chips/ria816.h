#pragma once
/*
    # ria816.h

    RIA for 65816 CPU - RaspberryPi Interface Adapter

    ## Emulated Pins
    *************************************
    *           +-----------+           *
    *    A0 --->|           |           *
    *        ...|           |           *
    *   A23 --->|           |           *
    *           |           |           *
    *           |           |           *
    *           |           |           *
    *           |           |           *
    *    D0 <-->|           |           *
    *        ...|   RIA     |           *
    *    D7 <-->|           |           *
    *           |           |           *
    *    RW --->|           |           *
    *           |           |           *
    *           |           |           *
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

    Depending on the emulated system, the I/O and control pins.

    On return ria816_tick() returns a modified pin mask where the following
    pins might have changed state:

    - data bus pins D0..D7 if this was a register read function.

    To reset a ria816_t instance, call ria816_reset():

    ~~~C
    ria816_reset(&sys->ria);
    ~~~

*/
#include <stdint.h>
#include <stdbool.h>

#include "util/ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

// register select same as lower 6 shared address bus bits
#define RIA816_PIN_RS0 (0)
#define RIA816_PIN_RS1 (1)
#define RIA816_PIN_RS2 (2)
#define RIA816_PIN_RS3 (3)
#define RIA816_PIN_RS4 (4)
#define RIA816_PIN_RS5 (5)

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
#define RIA816_PIN_RW (24)  // same as M6502_RW

// chip-specific control pins
#define RIA816_PIN_CS  (40)
#define RIA816_PIN_IRQ (41)

// pin bit masks
#define RIA816_RS0     (1ULL << RIA816_PIN_RS0)
#define RIA816_RS1     (1ULL << RIA816_PIN_RS1)
#define RIA816_RS2     (1ULL << RIA816_PIN_RS2)
#define RIA816_RS3     (1ULL << RIA816_PIN_RS3)
#define RIA816_RS4     (1ULL << RIA816_PIN_RS4)
#define RIA816_RS5     (1ULL << RIA816_PIN_RS5)
#define RIA816_RS      (RIA816_RS5 | RIA816_RS4 | RIA816_RS3 | RIA816_RS2 | RIA816_RS1 | RIA816_RS0)
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
#define RIA816_CS      (1ULL << RIA816_PIN_CS)
#define RIA816_IRQ     (1ULL << RIA816_PIN_IRQ)

// register indices
#define RIA816_MATH_OPERA     (0x00)  // Operand A for multiplication and division.
#define RIA816_MATH_OPERB     (0x02)  // Operand B for multiplication and division.
#define RIA816_MATH_MULAB     (0x04)  // OPERA * OPERB.
#define RIA816_MATH_DIVAB     (0x06)  // Signed OPERA / unsigned OPERB.
#define RIA816_TIME_TM        (0x08)  // Time Of Day (ms) - 64bits (8 bytes)
#define RIA816_DMA_ADDRSRC    (0x10)  // DMA source address.
#define RIA816_DMA_STEPSRC    (0x13)  // DMA source step.
#define RIA816_DMA_ADDRDST    (0x14)  // DMA destination address.
#define RIA816_DMA_STEPDST    (0x17)  // DMA destination step.
#define RIA816_DMA_COUNT      (0x18)  // DMA transfers count.
#define RIA816_DMA_DMAERR     (0x19)  // DMA transfer errno.
#define RIA816_FS_FDA         (0x1A)  // File-descriptor A number. (Obtained from open() API call.)
#define RIA816_FS_FDARW       (0x1B)  // Read bytes from the FDA. Write bytes to the FDA.
#define RIA816_FS_FDB         (0x1C)  // File-descriptor B number.
#define RIA816_FS_FDBRW       (0x1D)  // Read bytes from the FDB. Write bytes to the FDB.
#define RIA816_UART_READY     (0x20)  // Flow control for UART FIFO.
#define RIA816_UART_TX_RX     (0x21)  // Write bytes to the UART. Read bytes from the UART.
#define RIA816_HW_RNG         (0x22)  // Random Number Generator.
#define RIA816_CPU_N_COP      (0x24)  // 65816 vector.
#define RIA816_CPU_N_BRK      (0x26)  // 65816 vector.
#define RIA816_CPU_N_ABORTB   (0x28)  // 65816 vector.
#define RIA816_CPU_N_NMIB     (0x2A)  // 65816 vector.
#define RIA816_CPU_N_IRQB     (0x2E)  // 65816 vector.
#define RIA816_API_STACK      (0x30)  // 512 bytes for passing call parameters.
#define RIA816_API_OP         (0x31)  // Write the API operation id here to begin a kernel call.
#define RIA816_API_ERRNO      (0x32)  // API error number.
#define RIA816_API_BUSY       (0x33)  // Bit 7 high while operation is running.
#define RIA816_CPU_E_COP      (0x34)  // 65816 vector.
#define RIA816_EXT_IO         (0x36)  // Bitmap of 8x 32byte chunks for mapping RAM into I/O area.
#define RIA816_EXT_MEM        (0x37)  // reserved for future use (MMU)
#define RIA816_CPU_E_ABORTB   (0x38)  // 65816 vector.
#define RIA816_CPU_E_NMIB     (0x3A)  // 6502 vector.
#define RIA816_CPU_E_RESETB   (0x3C)  // 6502 vector.
#define RIA816_CPU_E_IRQB_BRK (0x3E)  // 6502 vector.
#define RIA816_NUM_REGS       (64)

// ria816 state
typedef struct {
    uint8_t reg[RIA816_NUM_REGS];
    ring_buffer_t uart_rx;
    ring_buffer_t uart_tx;
    uint64_t pins;
} ria816_t;

// extract 8-bit data bus from 64-bit pins
#define RIA816_GET_DATA(p) ((uint8_t)((p) >> 16))
// merge 8-bit data bus value into 64-bit pins
#define RIA816_SET_DATA(p, d) \
    { p = (((p) & ~0xFF0000ULL) | (((d) << 16) & 0xFF0000ULL)); }

#define RIA816_REG16(regs, ADDR) (uint16_t)((uint16_t)(regs[ADDR]) | ((uint16_t)(regs[ADDR + 1]) << 8))

// initialize a new RIA816 instance
void ria816_init(ria816_t* ria816);
// reset an existing RIA816 instance
void ria816_reset(ria816_t* ria816);
// tick the RIA816
uint64_t ria816_tick(ria816_t* ria816, uint64_t pins);

uint8_t ria816_uart_status(const ria816_t* c);

#ifdef __cplusplus
}  // extern "C"
#endif
