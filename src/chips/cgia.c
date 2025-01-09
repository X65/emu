#define CGIA_PALETTE_IMPL
#include "./cgia.h"

#define cgia_init fwcgia_init
#include "firmware/src/ria/cgia/cgia.h"
#undef cgia_init

#include <string.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

#define _CGIA_CLAMP(x) ((x) > 255 ? 255 : (x))
#define _CGIA_RGBA(r, g, b) \
    (0xFF000000 | _CGIA_CLAMP((r * 4) / 3) | (_CGIA_CLAMP((g * 4) / 3) << 8) | (_CGIA_CLAMP((b * 4) / 3) << 16))

void cgia_init(cgia_t* vpu, const cgia_desc_t* desc) {
    CHIPS_ASSERT(vpu && desc);
    CHIPS_ASSERT(desc->framebuffer.ptr && (desc->framebuffer.size <= CGIA_FRAMEBUFFER_SIZE_BYTES));
    CHIPS_ASSERT(desc->fetch_cb);
    CHIPS_ASSERT((desc->tick_hz > 0) && (desc->tick_hz < (MODE_BIT_CLK_KHZ * 1000)));

    memset(vpu, 0, sizeof(*vpu));
    vpu->fb = desc->framebuffer.ptr;
    vpu->fetch_cb = desc->fetch_cb;
    vpu->user_data = desc->user_data;

    /* compute counter periods, the DVI is clocked at fixed pixel clock,
       and the frequency of how the tick function is called must be
       communicated to the init function
    */
    int64_t tmp = ((int64_t)MODE_H_TOTAL_PIXELS * desc->tick_hz * CGIA_FIXEDPOINT_SCALE) / (MODE_BIT_CLK_KHZ * 1000);
    vpu->h_period = (int)tmp;

    vpu->hwcolors = cgia_rgb_palette;
}

void cgia_reset(cgia_t* vpu) {
    CHIPS_ASSERT(vpu);
    vpu->h_count = 0;
    vpu->l_count = 0;
}

uint64_t cgia_tick(cgia_t* vpu, uint64_t pins) {
    // DVI pixel count
    vpu->h_count += CGIA_FIXEDPOINT_SCALE;

    // rewind horizontal counter?
    if (vpu->h_count >= vpu->h_period) {
        vpu->h_count -= vpu->h_period;
        vpu->l_count++;
        if (vpu->l_count >= MODE_V_TOTAL_LINES) {
            // rewind line counter, field sync off
            vpu->l_count = 0;

            // trigger_vbl = true;
        }

        if (vpu->l_count >= MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + MODE_V_BACK_PORCH) {
            vpu->active_line =
                vpu->l_count - (MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + MODE_V_BACK_PORCH) / FB_V_REPEAT;

            cgia_render(vpu->active_line, (uint32_t*)(vpu->fb + (vpu->l_count * CGIA_FRAMEBUFFER_WIDTH)));
        }
    }
    vpu->pins = pins;
    return pins;
}

void cgia_snapshot_onsave(cgia_t* snapshot) {
    CHIPS_ASSERT(snapshot);
    snapshot->fetch_cb = 0;
    snapshot->user_data = 0;
    snapshot->fb = 0;
}

void cgia_snapshot_onload(cgia_t* snapshot, cgia_t* vpu) {
    CHIPS_ASSERT(snapshot && vpu);
    snapshot->fetch_cb = vpu->fetch_cb;
    snapshot->user_data = vpu->user_data;
    snapshot->fb = vpu->fb;
}

// ---- now comes rendering parts directly from RP816 firmware ----

#define __scratch_x(x)
#define __scratch_y(y)
#define __not_in_flash_func(f) f
#define dma_channel_wait_for_finish_blocking(ch)

typedef struct {
    uint32_t accum[2];
    uint32_t base[3];
    uint32_t ctrl[2];
} interp_hw_t;

static interp_hw_t interp_hw_array[2];
#define interp0 (&interp_hw_array[0])
#define interp1 (&interp_hw_array[1])

typedef struct {
    uint32_t accum[2];
    uint32_t base[3];
    uint32_t ctrl[2];
} interp_hw_save_t;

void interp_save(interp_hw_t* interp, interp_hw_save_t* saver) {
    saver->accum[0] = interp->accum[0];
    saver->accum[1] = interp->accum[1];
    saver->base[0] = interp->base[0];
    saver->base[1] = interp->base[1];
    saver->base[2] = interp->base[2];
    saver->ctrl[0] = interp->ctrl[0];
    saver->ctrl[1] = interp->ctrl[1];
}

void interp_restore(interp_hw_t* interp, interp_hw_save_t* saver) {
    interp->accum[0] = saver->accum[0];
    interp->accum[1] = saver->accum[1];
    interp->base[0] = saver->base[0];
    interp->base[1] = saver->base[1];
    interp->base[2] = saver->base[2];
    interp->ctrl[0] = saver->ctrl[0];
    interp->ctrl[1] = saver->ctrl[1];
}

static inline void set_default_interp_config() {}
static inline void set_interp_scans(
    uint8_t row_height,
    const uint8_t* memory_scan,
    const uint8_t* colour_scan,
    const uint8_t* backgr_scan) {}
static inline void set_mode7_interp_config(struct cgia_plane_t* plane) {}
static inline void set_mode7_scans(struct cgia_plane_t* plane, uint8_t* memory_scan) {}

static inline uint32_t interp_get_accumulator(interp_hw_t* interp, uint lane) {
    return interp->accum[lane];
}

#define FRAME_WIDTH  CGIA_DISPLAY_WIDTH
#define FRAME_HEIGHT CGIA_DISPLAY_HEIGHT
uint8_t vram_cache[2][256 * 256];

static inline uint32_t* fill_back(uint32_t* buf, uint32_t columns, uint32_t color_idx) {
    // dma_channel_wait_for_finish_blocking(back_chan);
    // dma_channel_set_write_addr(back_chan, buf, false);
    // dma_channel_set_read_addr(back_chan, &cgia_rgb_palette[color_idx], false);
    const uint pixels = columns * CGIA_COLUMN_PX;
    // dma_channel_set_trans_count(back_chan, pixels, true);
    return buf + pixels;
}

#include "firmware/src/ria/cgia/cgia_encode.h"

uint32_t*
cgia_encode_mode_2_shared(uint32_t* rgbbuf, uint32_t columns, uint8_t* character_generator, uint32_t char_shift) {
    return rgbbuf;
}
uint32_t*
cgia_encode_mode_2_mapped(uint32_t* rgbbuf, uint32_t columns, uint8_t* character_generator, uint32_t char_shift) {
    return rgbbuf;
}
uint32_t* cgia_encode_vt(uint32_t* rgbbuf, uint32_t columns, uint8_t* character_generator, uint32_t char_shift) {
    return rgbbuf;
}

uint32_t* cgia_encode_mode_3_shared(uint32_t* rgbbuf, uint32_t columns) {
    return rgbbuf;
}
uint32_t* cgia_encode_mode_3_mapped(uint32_t* rgbbuf, uint32_t columns) {
    return rgbbuf;
}

uint32_t* cgia_encode_mode_4_shared(
    uint32_t* rgbbuf,
    uint32_t columns,
    uint8_t* character_generator,
    uint32_t char_shift,
    uint8_t shared_colors[2]) {
    return rgbbuf;
}

uint32_t* cgia_encode_mode_4_mapped(
    uint32_t* rgbbuf,
    uint32_t columns,
    uint8_t* character_generator,
    uint32_t char_shift,
    uint8_t shared_colors[2]) {
    return rgbbuf;
}

uint32_t* cgia_encode_mode_4_doubled_shared(
    uint32_t* rgbbuf,
    uint32_t columns,
    uint8_t* character_generator,
    uint32_t char_shift,
    uint8_t shared_colors[2]) {
    return rgbbuf;
}

uint32_t* cgia_encode_mode_4_doubled_mapped(
    uint32_t* rgbbuf,
    uint32_t columns,
    uint8_t* character_generator,
    uint32_t char_shift,
    uint8_t shared_colors[2]) {
    return rgbbuf;
}

uint32_t* cgia_encode_mode_5_shared(uint32_t* rgbbuf, uint32_t columns, uint8_t shared_colors[2]) {
    return rgbbuf;
}

uint32_t* cgia_encode_mode_5_mapped(uint32_t* rgbbuf, uint32_t columns, uint8_t shared_colors[2]) {
    return rgbbuf;
}

uint32_t* cgia_encode_mode_5_doubled_shared(uint32_t* rgbbuf, uint32_t columns, uint8_t shared_colors[2]) {
    return rgbbuf;
}

uint32_t* cgia_encode_mode_5_doubled_mapped(uint32_t* rgbbuf, uint32_t columns, uint8_t shared_colors[2]) {
    return rgbbuf;
}

uint32_t* cgia_encode_mode_7(uint32_t* rgbbuf, uint32_t columns) {
    return rgbbuf;
}

void cgia_encode_sprite(uint32_t* rgbbuf, uint32_t* descriptor, uint8_t* line_data, uint32_t width) {}

#include "firmware/src/ria/cgia/cgia.c"
