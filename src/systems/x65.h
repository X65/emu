#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdalign.h>

#include "chips/chips_common.h"
#include "chips/kbd.h"
#include "chips/m6502.h"
#include "chips/ria816.h"
#include "chips/cgia.h"
#include "chips/ymf825.h"
#include "chips/mcp23017.h"

#ifdef __cplusplus
extern "C" {
#endif

// bump snapshot version when x65_t memory layout changes
#define X65_SNAPSHOT_VERSION (1)

#define X65_FREQUENCY             (6000000)  // "clock" frequency in Hz // FIXME: X65 has non-monotonic clock
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

// config parameters for x65_init()
typedef struct {
    x65_joystick_type_t joystick_type;  // default is X65_JOYSTICK_NONE
    chips_debug_t debug;                // optional debugging hook
    chips_audio_desc_t audio;           // audio output options
} x65_desc_t;

// X65 emulator state
typedef struct {
    m6502_t cpu;
    ria816_t ria;
    cgia_t cgia;
    ymf825_t sd1;
    mcp23017_t gpio;
    uint64_t pins;

    x65_joystick_type_t joystick_type;
    uint8_t kbd_joy1_mask;  // current joystick-1 state from keyboard-joystick emulation
    uint8_t kbd_joy2_mask;  // current joystick-2 state from keyboard-joystick emulation
    uint8_t joy_joy1_mask;  // current joystick-1 state from x65_joystick()
    uint8_t joy_joy2_mask;  // current joystick-2 state from x65_joystick()

    kbd_t kbd;  // keyboard matrix state
    bool valid;
    chips_debug_t debug;

    struct {
        chips_audio_callback_t callback;
        int num_samples;
        int sample_pos;
        float sample_buffer[X65_MAX_AUDIO_SAMPLES];
    } audio;

    uint8_t ram[1 << 24];  // general ram
    alignas(64) uint8_t fb[CGIA_FRAMEBUFFER_SIZE_BYTES];
} x65_t;

// initialize a new X65 instance
void x65_init(x65_t* sys, const x65_desc_t* desc);
// discard X65 instance
void x65_discard(x65_t* sys);
// reset a X65 instance
void x65_reset(x65_t* sys);
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
// save a snapshot, patches pointers to zero and offsets, returns snapshot version
uint32_t x65_save_snapshot(x65_t* sys, x65_t* dst);
// load a snapshot, returns false if snapshot versions don't match
bool x65_load_snapshot(x65_t* sys, uint32_t version, x65_t* src);

#ifdef __cplusplus
}  // extern "C"
#endif
