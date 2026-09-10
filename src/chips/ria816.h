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

#include "chips/m6526.h"
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
#define RIA816_PIN_CS        (40)
#define RIA816_PIN_IRQ       (41)
#define RIA816_PIN_TIMERS_CS (48)
#define RIA816_PIN_RGB_CS    (49)
#define RIA816_PIN_BUZZER_CS (50)
#define RIA816_PIN_HID_CS    (52)

// interrupt sources
#define RIA816_INT_CIA_MASK (0x01)  // CIA interrupt

// pin bit masks
#define RIA816_RS0       (1ULL << RIA816_PIN_RS0)
#define RIA816_RS1       (1ULL << RIA816_PIN_RS1)
#define RIA816_RS2       (1ULL << RIA816_PIN_RS2)
#define RIA816_RS3       (1ULL << RIA816_PIN_RS3)
#define RIA816_RS4       (1ULL << RIA816_PIN_RS4)
#define RIA816_RS5       (1ULL << RIA816_PIN_RS5)
#define RIA816_RS        (RIA816_RS5 | RIA816_RS4 | RIA816_RS3 | RIA816_RS2 | RIA816_RS1 | RIA816_RS0)
#define RIA816_D0        (1ULL << RIA816_PIN_D0)
#define RIA816_D1        (1ULL << RIA816_PIN_D1)
#define RIA816_D2        (1ULL << RIA816_PIN_D2)
#define RIA816_D3        (1ULL << RIA816_PIN_D3)
#define RIA816_D4        (1ULL << RIA816_PIN_D4)
#define RIA816_D5        (1ULL << RIA816_PIN_D5)
#define RIA816_D6        (1ULL << RIA816_PIN_D6)
#define RIA816_D7        (1ULL << RIA816_PIN_D7)
#define RIA816_DB_PINS   (0xFF0000ULL)
#define RIA816_RW        (1ULL << RIA816_PIN_RW)
#define RIA816_CS        (1ULL << RIA816_PIN_CS)
#define RIA816_IRQ       (1ULL << RIA816_PIN_IRQ)
#define RIA816_TIMERS_CS (1ULL << RIA816_PIN_TIMERS_CS)
#define RIA816_TIMERS_RS (RIA816_RS2 | RIA816_RS1 | RIA816_RS0)
#define RIA816_RGB_CS    (1ULL << RIA816_PIN_RGB_CS)
#define RIA816_RGB_RS    (RIA816_RS3 | RIA816_RS2 | RIA816_RS1 | RIA816_RS0)
#define RIA816_BUZZER_CS (1ULL << RIA816_PIN_BUZZER_CS)
#define RIA816_BUZZER_RS (RIA816_RS1 | RIA816_RS0)
#define RIA816_HID_CS    (1ULL << RIA816_PIN_HID_CS)
#define RIA816_HID_RS    (RIA816_RS3 | RIA816_RS2 | RIA816_RS1 | RIA816_RS0)

// register indices
#define RIA816_MATH_OPERA     (0x00)  // Operand A for multiplication and division.
#define RIA816_MATH_OPERB     (0x02)  // Operand B for multiplication and division.
#define RIA816_MATH_MULAB     (0x04)  // OPERA * OPERB.
#define RIA816_MATH_DIVAB     (0x08)  // Signed OPERA / unsigned OPERB.
#define RIA816_TIME_TM        (0x0A)  // Time Of Day (us) - 48bits (6 bytes)
#define RIA816_DMA_ADDRSRC    (0x10)  // DMA source address.
#define RIA816_DMA_STEPSRC    (0x13)  // DMA source step.
#define RIA816_DMA_ADDRDST    (0x14)  // DMA destination address.
#define RIA816_DMA_STEPDST    (0x17)  // DMA destination step.
#define RIA816_DMA_COUNT      (0x18)  // DMA transfers count.
#define RIA816_DMA_DMAERR     (0x19)  // DMA transfer errno.
#define RIA816_FS_FDA         (0x1A)  // File-descriptor A number. (Obtained from open() API call.)
#define RIA816_FS_FDB         (0x1B)  // File-descriptor B number.
#define RIA816_FS_FDARW       (0x1C)  // Read bytes from the FDA. Write bytes to the FDA.
#define RIA816_FS_FDBRW       (0x1D)  // Read bytes from the FDB. Write bytes to the FDB.
#define RIA816_FS_FDAST       (0x1E)  // File-descriptor A status.
#define RIA816_FS_FDBST       (0x1F)  // File-descriptor B status.
#define RIA816_UART_READY     (0x20)  // Flow control for UART FIFO.
#define RIA816_UART_TX_RX     (0x21)  // Write bytes to the UART. Read bytes from the UART.
#define RIA816_HW_RNG         (0x22)  // Random Number Generator.
#define RIA816_CPU_N_COP      (0x24)  // 65816 vector.
#define RIA816_CPU_N_BRK      (0x26)  // 65816 vector.
#define RIA816_CPU_N_ABORTB   (0x28)  // 65816 vector.
#define RIA816_CPU_N_NMIB     (0x2A)  // 65816 vector.
#define RIA816_IRQ_ENABLE     (0x2C)  // RIA interrupts enable
#define RIA816_IRQ_STATUS     (0x2D)  // Interrupt Controller status
#define RIA816_CPU_N_IRQB     (0x2E)  // 65816 vector.
#define RIA816_API_OP_RET     (0x30)  // Write the API operation id here to begin a kernel call.
#define RIA816_API_RET_HI     (0x31)  // High byte of 16 bit return value. Otherwise `0`.
#define RIA816_API_STACK      (0x32)  // 512 bytes for passing call parameters.
#define RIA816_API_STATUS     (0x33)  // Bit 7 high while operation is running. Bit 0 high when ERRNO.
#define RIA816_CPU_E_COP      (0x34)  // 65816 vector.
#define RIA816_EXT_IO         (0x36)  // Bitmap of 8x 64byte chunks for mapping RAM into I/O area.
#define RIA816_EXT_MEM        (0x37)  // reserved for future use (MMU)
#define RIA816_CPU_E_ABORTB   (0x38)  // 65816 vector.
#define RIA816_CPU_E_NMIB     (0x3A)  // 6502 vector.
#define RIA816_CPU_E_RESETB   (0x3C)  // 6502 vector.
#define RIA816_CPU_E_IRQB_BRK (0x3E)  // 6502 vector.
#define RIA816_NUM_REGS       (64)

// a memory-fetch callback, used to read video memory bytes into the CGIA
typedef void (*ria816_api_call_t)(uint8_t data, void* user_data);

// the ria816 setup parameters
typedef struct {
    // the CPU tick rate in hz
    int tick_hz;
    // API callback
    ria816_api_call_t api_cb;
    // optional user-data for the API callback
    void* user_data;
} ria816_desc_t;

// ria816 state
typedef struct {
    uint8_t reg[RIA816_NUM_REGS];
    ring_buffer_t uart_rx;
    ring_buffer_t uart_tx;
    m6526_t cia;
    uint8_t int_status;  // interrupts "controller"
    uint8_t irq_enable;  // RIA interrupts enable [. . . . . . . TIMERS]
    uint64_t us;         // monotonic clock
    int ticks_per_ms;
    int ticks_counter;
    uint64_t pins;
    // Buzzer state
    uint32_t buzzer_phase;
    uint32_t buzzer_period;
    uint16_t buzzer_freq;
    uint8_t  buzzer_duty;
    // API callback
    ria816_api_call_t api_cb;
    // optional user-data for the API callback
    void* user_data;
} ria816_t;

// extract 8-bit data bus from 64-bit pins
#define RIA816_GET_DATA(p) ((uint8_t)((p) >> 16))
// merge 8-bit data bus value into 64-bit pins
#define RIA816_SET_DATA(p, d) \
    { p = (((p) & ~0xFF0000ULL) | (((d) << 16) & 0xFF0000ULL)); }

#define RIA816_REG16(regs, ADDR) (uint16_t)((uint16_t)(regs[ADDR]) | ((uint16_t)(regs[ADDR + 1]) << 8))

// initialize a new RIA816 instance
void ria816_init(ria816_t* ria816, const ria816_desc_t* desc);
// reset an existing RIA816 instance
void ria816_reset(ria816_t* ria816);
// tick the RIA816
uint64_t ria816_tick(ria816_t* ria816, uint64_t pins);

// Timer window offsets are the low three address bits. These do not tick.
// Peek preserves the interrupt latch and pipeline a bus read would acknowledge.
uint8_t ria816_timers_peek(const ria816_t* ria, uint8_t offset);
void ria816_timers_write(ria816_t* ria, uint8_t offset, uint8_t data);

uint8_t ria816_uart_status(const ria816_t* c);
uint8_t ria816_reg_read(ria816_t* c, uint8_t addr);
void ria816_reg_write(ria816_t* c, uint8_t addr, uint8_t data);
uint8_t ria816_hid_read(ria816_t* c, uint8_t reg);
void ria816_hid_write(ria816_t* c, uint8_t reg, uint8_t data);
uint8_t ria816_hid_dev(const ria816_t* c);

// Gamepad injection, for headless scripting and tests.  A real gamepad report
// only ever arrives from an SDL device, so without this a script can drive
// joystick 1 through the GPIO expander and nothing else -- the four-player
// HID path is unreachable, and the HID registers are not CPU-writable either.
//
// `pad` is 1..4, matching the HID selector's index nibble; `report` is the
// ten-byte pad_xram_t the firmware exposes at $FFB0 (dpad+flags, sticks,
// button0, button1, lx, ly, rx, ry, lt, rt).  The connected flag is set for
// you.  An injected pad hides the real device in that slot until released.
#define RIA816_PAD_SLOTS  4
#define RIA816_PAD_REGS  10
void ria816_pad_inject(uint8_t pad, const uint8_t report[RIA816_PAD_REGS]);
void ria816_pad_release(uint8_t pad);
bool ria816_pad_injected(uint8_t pad);

// Pad state for the status display, read without touching the HID selector the
// guest owns.  `pad` 0 is the firmware's merged view of every connected pad,
// 1..4 a single slot; `reg` indexes pad_xram_t as above.  RIA816_PAD_BUTTONS
// is the width of button0+button1: the report's bit positions are labels
// (A B C X Y Z L1 R1 | L2 R2 Select Start Home L3 R3), not the device's own
// button numbering, so there is no shorter honest width for a given pad.
#define RIA816_PAD_BUTTONS 16
uint8_t ria816_pad_count(void);
uint8_t ria816_pad_read(uint8_t pad, uint8_t reg);

// Keyboard injection, for headless scripting and tests.  Real key state only
// ever arrives from the host window, so without this a script cannot press a
// key.  The injected keys are OR-ed into whatever a real keyboard reports, as
// a second keyboard would be.  `keycode` is a USB HID usage id.
#define RIA816_KBD_BYTES 32
void ria816_key_set(uint8_t keycode);
void ria816_keys_clear(void);
uint8_t ria816_rgb_read(ria816_t* c, uint8_t reg);
void ria816_rgb_write(ria816_t* c, uint8_t reg, uint8_t data);
void ria816_rgb_get_leds(uint32_t** leds, size_t* leds_no);
uint8_t ria816_buzzer_read(ria816_t* c, uint8_t reg);
void ria816_buzzer_write(ria816_t* c, uint8_t reg, uint8_t data);
bool ria816_buzzer_tick(ria816_t* c);

#ifdef __cplusplus
}  // extern "C"
#endif
