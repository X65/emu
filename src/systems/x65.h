#pragma once
/*#
    # x65.h

    An X65 emulator in C

    Optionally provide the following macros with your own implementation

    ~~~C
    CHIPS_ASSERT(c)
    ~~~
        your own assert macro (default: assert(c))

    ## The X65

    TODO!

    ## TODO:

    ## Tests Status

    In chips-test/tests/testsuite-2.15/bin
    ???

    ## 0BSD license

    Copyright (c) 2018 Tomasz Sterna
#*/

#include "chips/chips_common.h"
#include "chips/beeper.h"
#include "chips/cgia.h"
#include "chips/tca6416a.h"
#include "chips/w65c816s.h"
#include "chips/ria816.h"
#include "chips/ymf262.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdalign.h>

#ifdef __cplusplus
extern "C" {
#endif

// bump snapshot version when x65_t memory layout changes
#define X65_SNAPSHOT_VERSION (1)

#define X65_FREQUENCY             (1826300)  // clock frequency in Hz
#define X65_MAX_AUDIO_SAMPLES     (1024)     // max number of audio samples in internal sample buffer
#define X65_DEFAULT_AUDIO_SAMPLES (128)      // default number of samples in internal sample buffer

// X65 joystick types
typedef enum {
    X65_JOYSTICKTYPE_NONE,
    X65_JOYSTICKTYPE_DIGITAL_1,
    X65_JOYSTICKTYPE_DIGITAL_2,
    X65_JOYSTICKTYPE_DIGITAL_12,  // input routed to both joysticks
} x65_joystick_type_t;

// joystick mask bits
#define X65_JOYSTICK_UP    (1 << 0)
#define X65_JOYSTICK_DOWN  (1 << 1)
#define X65_JOYSTICK_LEFT  (1 << 2)
#define X65_JOYSTICK_RIGHT (1 << 3)
#define X65_JOYSTICK_BTN   (1 << 5)

// special keyboard keys
#define X65_KEY_SPACE    (0x20)  // space
#define X65_KEY_CSRLEFT  (0x08)  // cursor left (shift+cursor right)
#define X65_KEY_CSRRIGHT (0x09)  // cursor right
#define X65_KEY_CSRDOWN  (0x0A)  // cursor down
#define X65_KEY_CSRUP    (0x0B)  // cursor up (shift+cursor down)
#define X65_KEY_DEL      (0x01)  // delete
#define X65_KEY_INST     (0x10)  // inst (shift+del)
#define X65_KEY_HOME     (0x0C)  // home
#define X65_KEY_CLR      (0x02)  // clear (shift+home)
#define X65_KEY_RETURN   (0x0D)  // return
#define X65_KEY_CTRL     (0x0E)  // ctrl
#define X65_KEY_CBM      (0x0F)  // C= commodore key
#define X65_KEY_RESTORE  (0xFF)  // restore (connected to the NMI line)
#define X65_KEY_STOP     (0x03)  // run stop
#define X65_KEY_RUN      (0x07)  // run (shift+run stop)
#define X65_KEY_LEFT     (0x04)  // left arrow symbol
#define X65_KEY_F1       (0xF1)  // F1
#define X65_KEY_F2       (0xF2)  // F2
#define X65_KEY_F3       (0xF3)  // F3
#define X65_KEY_F4       (0xF4)  // F4
#define X65_KEY_F5       (0xF5)  // F5
#define X65_KEY_F6       (0xF6)  // F6
#define X65_KEY_F7       (0xF7)  // F7
#define X65_KEY_F8       (0xF8)  // F8

// Extension bus devices
// split in 8 slots of 64 bytes
#define X65_EXT_BASE     (0xFC00)
#define X65_EXT_LEN      (0x200)
#define X65_EXT_SLOTS    (8)
#define X65_EXT_SLOT_LEN (X65_EXT_LEN / X65_EXT_SLOTS)
// reserved for future use (MMU)
// default state is visible RAM
#define X65_EXT_MEM (0xF800)
// IO base addresses
#define X65_IO_BASE        (0xFE00)
#define X65_IO_MIXER_BASE  (0xFEB0)
#define X65_IO_YMF825_BASE (0xFEC0)
#define X65_IO_CGIA_BASE   (0xFF00)
#define X65_IO_GPIO_BASE   (0xFF80)
#define X65_IO_TIMERS_BASE (0xFF88)
#define X65_IO_RIA_BASE    (0xFFC0)

#define X65_RAM_SIZE_BYTES (1 << 24)  // 16 MBytes of RAM

// interrupt "controller" lines
#define X65_INT_RIA  (1 << 0)  // RIA interrupt
#define X65_INT_GPIO (1 << 1)  // GPIO interrupt
#define X65_INT_AUD  (1 << 2)  // AUD interrupt
#define X65_INT_I2C  (1 << 3)  // I2C interrupt
#define X65_INT_IO0  (1 << 4)  // IO0 interrupt
#define X65_INT_IO1  (1 << 5)  // IO1 interrupt
#define X65_INT_IO2  (1 << 6)  // IO2 interrupt
#define X65_INT_IO3  (1 << 7)  // IO3 interrupt

// config parameters for x65_init()
typedef struct {
    x65_joystick_type_t joystick_type;  // default is X65_JOYSTICK_NONE
    chips_debug_t debug;                // optional debugging hook
    chips_audio_desc_t audio;           // audio output options
} x65_desc_t;

// X65 emulator state
typedef struct {
    w65816_t cpu;
    ria816_t ria;
    tca6416a_t gpio;
    cgia_t cgia;
    ymf262_t opl3;
    uint64_t pins;

    bool running;  // whether CPU is running or held in RESET state

    x65_joystick_type_t joystick_type;
    uint8_t kbd_joy1_mask;  // current joystick-1 state from keyboard-joystick emulation
    uint8_t kbd_joy2_mask;  // current joystick-2 state from keyboard-joystick emulation
    uint8_t joy_joy1_mask;  // current joystick-1 state from x65_joystick()
    uint8_t joy_joy2_mask;  // current joystick-2 state from x65_joystick()

    bool valid;
    chips_debug_t debug;

    beeper_t beeper[2];
    struct {
        chips_audio_callback_t callback;
        int num_samples;
        int sample_pos;
        float sample_buffer[X65_MAX_AUDIO_SAMPLES];
    } audio;

    alignas(64) uint8_t ram[X65_RAM_SIZE_BYTES];
    alignas(64) uint32_t fb[CGIA_FRAMEBUFFER_SIZE_BYTES / 4];
} x65_t;

// initialize a new X65 instance
void x65_init(x65_t* sys, const x65_desc_t* desc);
// discard X65 instance
void x65_discard(x65_t* sys);
// reset a X65 instance
void x65_reset(x65_t* sys);
// start/stop X65 CPU
void x65_set_running(x65_t* sys, bool running);
// get framebuffer and display attributes
chips_display_info_t x65_display_info(x65_t* sys);
// tick X65 instance for a given number of microseconds, return number of ticks executed
uint32_t x65_exec(x65_t* sys, uint32_t micro_seconds);
// send a key-down event to the X65
void x65_key_down(x65_t* sys, int key_code);
// send a key-up event to the X65
void x65_key_up(x65_t* sys, int key_code);
// enable/disable joystick emulation
void x65_set_joystick_type(x65_t* sys, x65_joystick_type_t type);
// get current joystick emulation type
x65_joystick_type_t x65_joystick_type(x65_t* sys);
// set joystick mask (combination of X65_JOYSTICK_*)
void x65_joystick(x65_t* sys, uint8_t joy1_mask, uint8_t joy2_mask);
// quickload a .xex file
bool x65_quickload_xex(x65_t* sys, chips_range_t data);
// insert tape as .TAP file (c1530 must be enabled)
bool x65_insert_tape(x65_t* sys, chips_range_t data);
// remove tape file
void x65_remove_tape(x65_t* sys);
// return true if a tape is currently inserted
bool x65_tape_inserted(x65_t* sys);
// start the tape (press the Play button)
void x65_tape_play(x65_t* sys);
// stop the tape (unpress the Play button
void x65_tape_stop(x65_t* sys);
// return true if tape motor is on
bool x65_is_tape_motor_on(x65_t* sys);
// save a snapshot, patches pointers to zero and offsets, returns snapshot version
uint32_t x65_save_snapshot(x65_t* sys, x65_t* dst);
// load a snapshot, returns false if snapshot versions don't match
bool x65_load_snapshot(x65_t* sys, uint32_t version, x65_t* src);

// ---- memory access functions ----------------------------------------------
/* write a byte to (PS)RAM, mirroring to CGIA L1 cache */
void mem_ram_write(x65_t* sys, uint32_t addr, uint8_t data);
/* read a byte from (PS)RAM */
uint8_t mem_ram_read(x65_t* sys, uint32_t addr);
/* read a byte like a CPU */
uint8_t mem_rd(x65_t* sys, uint8_t bank, uint16_t addr);
/* write a byte like a CPU */
void mem_wr(x65_t* sys, uint8_t bank, uint16_t addr, uint8_t data);
/* helper method to write a 16-bit value, does 2 mem_wr() */
static inline void mem_wr16(x65_t* sys, uint8_t bank, uint16_t addr, uint16_t data) {
    mem_wr(sys, bank, addr, (uint8_t)data);
    mem_wr(sys, bank, addr + 1, (uint8_t)(data >> 8));
}
/* helper method to read a 16-bit value, does 2 mem_rd() */
static inline uint16_t mem_rd16(x65_t* sys, uint8_t bank, uint16_t addr) {
    uint8_t l = mem_rd(sys, bank, addr);
    uint8_t h = mem_rd(sys, bank, addr + 1);
    return (h << 8) | l;
}

#ifdef __cplusplus
}  // extern "C"
#endif
