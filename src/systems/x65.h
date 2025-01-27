#pragma once
/*#
    # x65.h

    An X65 emulator in C

    Optionally provide the following macros with your own implementation

    ~~~C
    CHIPS_ASSERT(c)
    ~~~
        your own assert macro (default: assert(c))

    You need to include the following headers before including x65.h:

    - chips/chips_common.h
    - chips/m6502.h
    - chips/m6526.h
    - chips/m6569.h
    - chips/m6581.h
    - chips/kbd.h
    - chips/mem.h
    - chips/clk.h

    ## The X65

    TODO!

    ## TODO:

    ## Tests Status

    In chips-test/tests/testsuite-2.15/bin

        branchwrap:     ok
        cia1pb6:        ok
        cia1pb7:        ok
        cia1ta:         FAIL (OK, CIA sysclock not implemented)
        cia1tab:        ok
        cia1tb:         FAIL (OK, CIA sysclock not implemented)
        cia1tb123:      ok
        cia2pb6:        ok
        cia2pb7:        ok
        cia2ta:         FAIL (OK, CIA sysclock not implemented)
        cia2tb:         FAIL (OK, CIA sysclock not implemented)
        cntdef:         ok
        cnto2:          ok
        cpuport:        ok
        cputiming:      ok
        flipos:         ok
        icr01:          ok
        imr:            ok
        irq:            ok
        loadth:         ok
        mmu:            ok
        mmufetch:       ok
        nmi:            FAIL (1 error at 00/5)
        oneshot:        ok
        trap1..17:      ok

    In chips-test/tests/vice-tests/CIA:

    ciavarious:
        - all green, expect cia15.prg, which tests the CIA TOD clock,
          which isn't implemented

    cia-timer/cia-timer-oldcias.prg:
        - left side (CIA-1, IRQ) all green, right side (CIA-2, NMI) some red

    ciatimer/dd0dtest/dd0dtest.prg (NMI related):
        - some errors

    irqdelay:   all green

    mirrors/ciamirrors.prg: green

    reload0:
        reload0a.prg:   red
        reload0b.prg:   red

    shiftregister:
        cia-icr-test-continues-old.prg: green
        cia-icr-test-oneshot-old.prg: green
        cia-icr-test2-continues.prg: some red
        cia-icr-test2-oneshot.prg: some red
        cia-sp-test-continues-old.prg: much red (ok, CIA SP not implemented)
        cia-sp-test-oneshot-old.prg: much red (ok, CIA SP not implemented)

    timerbasics:    all green

    in chips-test/tests/vice-tests/interrupts:

    branchquirk:
        branchquirk-old.prg:    green
        branchquirk-nmiold.prg: red

    cia-int:
        cia-int-irq.prg:    green??
        cia-int-nmi.prg:    green??

    irq-ackn-bug:
        cia1.prg:       green
        cia2.prg:       green
        irq-ack-vicii.prg:  red
        irq-ackn_after_cli.prg: ???
        irq-ackn_after_cli2.prg: ???

    irqdma: (takes a long time)
        all fail?

    irqdummy/irqdummy.prg:  green

    irqnmi/irqnmi-old.prg: left (irq) green,right (nmi) red

    VICII:

    D011Test:                   TODO
    banking/banking.prg:        ok
    border:                     fail (border not opened)
    colorram/test.prg:          ok
    colorsplit/colorsplit.prg:  fail (horizontal offsets)
    dentest:    (these were mostly fixed by moving the raster interrupt check
                 in m6569.h to tick 63, which made the otherwise some tests
                 were flickering because a second raster interrupt wasn't stable)
        den01-48-0.prg:         ok
        den01-48-1.prg:         ok
        den01-48-2.prg:         ok
        den01-49-0.prg:         ok
        den01-49-1.prg:         ok
        den01-49-2.prg:         ok
        den10-48-0.prg:         ok
        den10-48-1.prg:         ok
        den10-48-2.prg:         FAIL
        den10-51-0.prg:         ok
        den10-51-1.prg:         ok
        den10-51-2.prg:         ok
        den10-51-3.prg:         ok
        denrsel-0.prg:          ok
        denrsel-1.prg:          ok
        denrsel-2.prg:          ok
        denrsel-63.prg:         ok
        denrsel-s0.prg:         ok
        denrsel-s1.prg:         ok
        denrsel-s2.prg:         ok
        denrsel55.prg:          ok
    dmadelay:
        test1-2a-03.prg:        ok
        test1-2a-04.prg:        FAIL (flickering)
        test1-2a-10.prg:        ok
        test1-2a-11.prg:        FAIL (1 char line off)
        test1-2a-16.prg:        ok
        test1-2a-17.prg:        FAIL (1 char line off)
        test1-2a-18.prg:        FAIL (1 char line/col off)
        test1.prg:              ??? (no reference image)
        test2-28-05.prg:        ok
        test2-28-06.prg:        FAIL (flickering)
        test2-28-11.prg:        ok
        test2-28-12.prg:        FAIL (one char line off)
        test2-28-16.prg:        ok
        test2-28-17.prg:        FAIL (one char line off)
        test2-28-18.prg:        FAIL (one char line/col off)
        test3-28-07.prg:        ok
        test3-28-08.prg:        FAIL (one char line off)
        test3-28-13.prg:        ok
        test3-28-14.prg:        FAIL (one char line off)
        test3-28-18.prg:        ok
        test3-28-19.prg:        FAIL (one char line off)
        test3-28-1a.prg:        FAIL (one char col off)

    fldscroll:  broken
    flibug/blackmail.prg:       reference image doesn't match

    frodotests:
        3fff.prg                ok
        d011h3.prg              FAIL (???)
        fld.prg                 ok
        lrborder:               FAIL (???)
        sprsync:                ok (???)
        stretch:                ok (???)
        tech-tech:              ok
        text26:                 ok

    gfxfetch/gfxfetch.prg:      FAIL (reference image doesn't match in boder)

    greydot/greydot.prg:        FAIL (ref image doesn't match, color bars start one tick late)

    lp-trigger:
        test1.prg:              FAIL (flickering)
        test2.prg:              FAIL

    lplatency/lplatency.prg:    FAIL

    movesplit:                  ????

    phi1timing:                 FAIL

    rasterirq:                  FAIL (reference image doesn't match)

    screenpos:                  FAIL (reference image doesn't match)

    split-tests:
        bascan          FAIL
        fetchsplit      FAIL (flickering characters)
        lightpen        FAIL
        modesplit       FAIL (ref image doesn't match)
        spritescan      FAIL

    sprite0move         ???

    spritebug           TODO
    all other sprite tests: TODO

    vicii-timing:       FAIL (ref image doesn't match)

    ## zlib/libpng license

    Copyright (c) 2018 Andre Weissflog
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

#include "chips/chips_common.h"
#include "chips/kbd.h"
#include "chips/beeper.h"
#include "chips/cgia.h"
#include "chips/m6502.h"
#include "chips/m6526.h"
#include "chips/m6581.h"
#include "chips/ria816.h"
#include "chips/mem.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdalign.h>

#ifdef __cplusplus
extern "C" {
#endif

// bump snapshot version when x65_t memory layout changes
#define X65_SNAPSHOT_VERSION (1)

#define X65_FREQUENCY             (7159090)  // clock frequency in Hz
#define X65_MAX_AUDIO_SAMPLES     (1024)     // max number of audio samples in internal sample buffer
#define X65_DEFAULT_AUDIO_SAMPLES (128)      // default number of samples in internal sample buffer

// X65 joystick types
typedef enum {
    X65_JOYSTICKTYPE_NONE,
    X65_JOYSTICKTYPE_DIGITAL_1,
    X65_JOYSTICKTYPE_DIGITAL_2,
    X65_JOYSTICKTYPE_DIGITAL_12,  // input routed to both joysticks
    X65_JOYSTICKTYPE_PADDLE_1,    // FIXME: not emulated
    X65_JOYSTICKTYPE_PADDLE_2,    // FIXME: not emulated
} x65_joystick_type_t;

// joystick mask bits
#define X65_JOYSTICK_UP    (1 << 0)
#define X65_JOYSTICK_DOWN  (1 << 1)
#define X65_JOYSTICK_LEFT  (1 << 2)
#define X65_JOYSTICK_RIGHT (1 << 3)
#define X65_JOYSTICK_BTN   (1 << 4)

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
#define X65_EXT_SLOTS    (8)
#define X65_EXT_SLOT_LEN (0x200 / X65_EXT_SLOTS)
// IO base addresses
#define X65_IO_BASE        (0xFE00)
#define X65_IO_CGIA_BASE   (0xFF00)
#define X65_IO_YMF825_BASE (0xFF80)
#define X65_IO_RIA_BASE    (0xFFC0)

// config parameters for x65_init()
typedef struct {
    x65_joystick_type_t joystick_type;  // default is X65_JOYSTICK_NONE
    chips_debug_t debug;                // optional debugging hook
    chips_audio_desc_t audio;           // audio output options
} x65_desc_t;

// X65 emulator state
typedef struct {
    m6502_t cpu;
    m6526_t cia_1;
    m6526_t cia_2;
    ria816_t ria;
    cgia_t cgia;
    m6581_t sid;
    uint64_t pins;

    bool running;  // whether CPU is running or held in RESET state

    x65_joystick_type_t joystick_type;
    uint8_t kbd_joy1_mask;  // current joystick-1 state from keyboard-joystick emulation
    uint8_t kbd_joy2_mask;  // current joystick-2 state from keyboard-joystick emulation
    uint8_t joy_joy1_mask;  // current joystick-1 state from x65_joystick()
    uint8_t joy_joy2_mask;  // current joystick-2 state from x65_joystick()

    kbd_t kbd;  // keyboard matrix state
    mem_t mem;  // CPU-visible memory mapping
    bool valid;
    chips_debug_t debug;

    beeper_t beeper[2];
    struct {
        chips_audio_callback_t callback;
        int num_samples;
        int sample_pos;
        float sample_buffer[X65_MAX_AUDIO_SAMPLES];
    } audio;

    uint8_t ram[1 << 16];  // general ram
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
// quickload a .bin/.prg file
bool x65_quickload(x65_t* sys, chips_range_t data);
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
// perform a RUN BASIC call
void x65_basic_run(x65_t* sys);
// perform a LOAD BASIC call
void x65_basic_load(x65_t* sys);
// perform a SYS xxxx BASIC call via the BASIC input buffer
void x65_basic_syscall(x65_t* sys, uint16_t addr);
// returns the SYS call return address (can be used to set a breakpoint)
uint16_t x65_syscall_return_addr(void);

#ifdef __cplusplus
}  // extern "C"
#endif
