#include <string.h>

#include "chips/clk.h"
#include "x65.h"

#define X65_FREQUENCY (6000000)  // "clock" frequency in Hz // FIXME: X65 has non-monotonic clock

#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

static uint8_t _x65_cgia_fetch(uint32_t addr, void* user_data);

#define _X65_DEFAULT(val, def) (((val) != 0) ? (val) : (def))

void x65_init(x65_t* sys, const x65_desc_t* desc) {
    CHIPS_ASSERT(sys && desc);
    if (desc->debug.callback.func) {
        CHIPS_ASSERT(desc->debug.stopped);
    }

    memset(sys, 0, sizeof(x65_t));
    sys->valid = true;
    sys->debug = desc->debug;
    sys->audio.callback = desc->audio.callback;
    sys->audio.num_samples = _X65_DEFAULT(desc->audio.num_samples, X65_DEFAULT_AUDIO_SAMPLES);
    CHIPS_ASSERT(sys->audio.num_samples <= X65_MAX_AUDIO_SAMPLES);

    sys->pins = m6502_init(
        &sys->cpu,
        &(m6502_desc_t){
            .bcd_disabled = false,
        });
    cgia_init(
        &sys->cgia,
        &(cgia_desc_t){
            .fetch_cb = _x65_cgia_fetch,
            .framebuffer = {
                .ptr = sys->fb,
                .size = sizeof(sys->fb),
            },
            .screen = {
                .x = 0,
                .y = 0,
                .width = CGIA_FRAMEBUFFER_WIDTH,
                .height = CGIA_FRAMEBUFFER_HEIGHT,
            },
            .user_data = sys,
        });
    ymf825_init(
        &sys->sd1,
        &(ymf825_desc_t){
            .tick_hz = X65_FREQUENCY,
            .sound_hz = _X65_DEFAULT(desc->audio.sample_rate, 44100),
            .magnitude = _X65_DEFAULT(desc->audio.volume, 1.0f),
        });
}

void x65_discard(x65_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->valid = false;
}

void x65_reset(x65_t* sys) {
    CHIPS_ASSERT(sys && sys->valid);
    sys->pins |= M6502_RES;
}

static uint64_t _x65_tick(x65_t* sys, uint64_t pins) {
    // tick the CPU
    pins = m6502_tick(&sys->cpu, pins);
    const uint16_t addr = M6502_GET_ADDR(pins);

    // those pins are set each tick by the CIAs and VIC
    pins &= ~(M6502_IRQ | M6502_NMI | M6502_RDY | M6510_AEC);

    /*  address decoding

        When the RDY pin is active (during bad lines), no CPU/chip
        communication takes place starting with the first read access.
    */
    bool mem_access = false;
    bool ext_access = false;
    uint64_t ria_pins = pins & M6502_PIN_MASK;
    uint64_t cgia_pins = pins & M6502_PIN_MASK;
    uint64_t sd1_pins = pins & M6502_PIN_MASK;
    uint64_t gpio_pins = pins & M6502_PIN_MASK;
    if ((pins & (M6502_RDY | M6502_RW)) != (M6502_RDY | M6502_RW)) {
        if ((addr & 0xFF00) == 0xFF00) {
            if (addr < 0xFF80) {
                ext_access = true;
            }
            else if (addr < 0xFFA0) {
                // SD-1 (FF80..FF9F)
                sd1_pins |= YMF825_CS;
            }
            else if (addr < 0xFFC0) {
                // CGIA (FFA0..FFBF)
                cgia_pins |= CGIA_CS;
            }
            else {
                ria_pins |= RIA816_CS;
            }
        }
        else {
            mem_access = true;
        }
    }

    // tick the SD-1
    {
        sd1_pins = ymf825_tick(&sys->sd1, sd1_pins);
        if (sd1_pins & YMF825_SAMPLE) {
            // new audio sample ready
            sys->audio.sample_buffer[sys->audio.sample_pos++] = sys->sd1.sample;
            if (sys->audio.sample_pos == sys->audio.num_samples) {
                if (sys->audio.callback.func) {
                    sys->audio.callback.func(
                        sys->audio.sample_buffer,
                        sys->audio.num_samples,
                        sys->audio.callback.user_data);
                }
                sys->audio.sample_pos = 0;
            }
        }
        if ((sd1_pins & (YMF825_CS | YMF825_RW)) == (YMF825_CS | YMF825_RW)) {
            pins = M6502_COPY_DATA(pins, sd1_pins);
        }
    }

    /* tick the CGIA display chip:
        - the CGIA NMI pin is connected to the CPU NMI pin and goes
        active when the CGIA requests a Vertical Blank Interrupt
    */
    {
        cgia_pins = cgia_tick(&sys->cgia, cgia_pins);
        pins |= (cgia_pins & (M6502_IRQ | M6502_RDY | M6510_AEC));
        if ((cgia_pins & (CGIA_CS | CGIA_RW)) == (CGIA_CS | CGIA_RW)) {
            pins = M6502_COPY_DATA(pins, cgia_pins);
        }
    }

    /* remaining CPU IO and memory accesses, those don't fit into the
       "universal tick model" (yet?)
    */
    if (ext_access) {
        // extension bus addressing
        // N/C - do nothing for now
    }
    else if (mem_access) {
        if (pins & M6502_RW) {
            // memory read
            M6502_SET_DATA(pins, sys->ram[addr & 0xFFFFFF]);
        }
        else {
            // memory write
            sys->ram[addr & 0xFFFFFF] = M6502_GET_DATA(pins);
        }
    }
    return pins;
}

static uint8_t _x65_cgia_fetch(uint32_t addr, void* user_data) {
    x65_t* sys = (x65_t*)user_data;
    // CGIA has direct access to RAM (think dual-port RAM)
    return sys->ram[addr & 0xFFFFFF];
}

uint32_t x65_exec(x65_t* sys, uint32_t micro_seconds) {
    CHIPS_ASSERT(sys && sys->valid);
    uint32_t num_ticks = clk_us_to_ticks(X65_FREQUENCY, micro_seconds);
    uint64_t pins = sys->pins;
    if (!sys->debug.callback.func) {
        // run without debug callback
        for (uint32_t ticks = 0; ticks < num_ticks; ticks++) {
            pins = _x65_tick(sys, pins);
        }
    }
    else {
        // run with debug callback
        for (uint32_t ticks = 0; (ticks < num_ticks) && !(*sys->debug.stopped); ticks++) {
            pins = _x65_tick(sys, pins);
            sys->debug.callback.func(sys->debug.callback.user_data, pins);
        }
    }
    sys->pins = pins;
    return num_ticks;
}

chips_display_info_t x65_display_info(x65_t* sys) {
    chips_display_info_t res = {
        .frame = {
            .dim = {
                .width = CGIA_FRAMEBUFFER_WIDTH,
                .height = CGIA_FRAMEBUFFER_HEIGHT,
            },
            .bytes_per_pixel = 1,
            .buffer = {
                .ptr = sys ? sys->fb : 0,
                .size = CGIA_FRAMEBUFFER_SIZE_BYTES,
            }
        },
        .palette = (chips_range_t){
            .ptr = cgia_rgb_palette,
            .size = 256,
        },
        .screen = (chips_rect_t){
            .x = 0,
            .y = 0,
            .width = CGIA_FRAMEBUFFER_WIDTH,
            .height = CGIA_FRAMEBUFFER_HEIGHT,
        }
    };
    CHIPS_ASSERT(((sys == 0) && (res.frame.buffer.ptr == 0)) || ((sys != 0) && (res.frame.buffer.ptr != 0)));
    return res;
}
