#include "sokol_time.h"
#include "clock.h"
#include <assert.h>
#include <stdbool.h>

typedef struct {
    bool valid;
    uint64_t cur_time;
    uint64_t last_frame;
} clock_state_t;
static clock_state_t state;

void clock_init(void) {
    // reboot and reset come back through here; re-running stm_setup() only moves
    // sokol_time's epoch, and nothing in the app holds an absolute stm timestamp
    // across a frame, so the reset is invisible
    stm_setup();
    state = (clock_state_t) {
        .valid = true,
        .cur_time = 0,
        .last_frame = stm_now(),
    };
}

uint32_t clock_frame_time(void) {
    assert(state.valid);
    // Measure the real interval instead of asking sapp_frame_duration(): that is a
    // 256-frame rolling average which *discards* any duration outside +-20% of
    // itself, so a dropped vsync reads back as a normal 16.7ms frame. The emulated
    // time for the frame the compositor skipped is then never run, and the ~800
    // audio samples it would have produced are gone for good -- a push-based audio
    // FIFO has no way to make them up, so the hole comes out as a dropout.
    uint32_t frame_time_us = (uint32_t)stm_us(stm_laptime(&state.last_frame));
    // prevent death-spiral on host systems that are too slow to emulate
    // in real time, or during long frames (e.g. debugging). Emulating a frame
    // costs roughly a quarter of the wall time it represents, so catching up on
    // this much still fits inside a 60Hz budget, and it absorbs a couple of
    // dropped vsyncs in a row without dropping audio.
    if (frame_time_us > 48000) {
        frame_time_us = 48000;
    }
    state.cur_time += frame_time_us;
    return frame_time_us;
}

uint32_t clock_frame_count_60hz(void) {
    assert(state.valid);
    return (uint32_t) (state.cur_time / 16667);
}
