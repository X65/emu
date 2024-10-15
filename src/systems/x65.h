#pragma once

#include <stdalign.h>

#include "chips/chips_common.h"
#include "chips/m6502.h"
#include "chips/ria816.h"
#include "chips/cgia.h"
#include "chips/ymf825.h"
#include "chips/mcp23017.h"

#ifdef __cplusplus
extern "C" {
#endif

#define X65_MAX_AUDIO_SAMPLES     (1024)  // max number of audio samples in internal sample buffer
#define X65_DEFAULT_AUDIO_SAMPLES (128)   // default number of samples in internal sample buffer

// config parameters for x65_init()
typedef struct {
    chips_debug_t debug;       // optional debugging hook
    chips_audio_desc_t audio;  // audio output options
} x65_desc_t;

// X65 emulator state
typedef struct {
    m6502_t cpu;
    ria816_t ria;
    cgia_t cgia;
    ymf825_t sd1;
    mcp23017_t gpio;
    uint64_t pins;

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

#ifdef __cplusplus
}  // extern "C"
#endif
