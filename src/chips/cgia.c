#define CGIA_PALETTE_IMPL
#include "./cgia.h"

#define cgia_init fwcgia_init
#include "firmware/src/ria/cgia/cgia.h"
#undef cgia_init

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

#define _CGIA_CLAMP(x) ((x) > 255 ? 255 : (x))
#define _CGIA_RGBA(r, g, b) \
    (0xFF000000 | _CGIA_CLAMP((r * 4) / 3) | (_CGIA_CLAMP((g * 4) / 3) << 8) | (_CGIA_CLAMP((b * 4) / 3) << 16))

// used to access regs from firmware render function which expects global symbol
static cgia_t* CGIA_vpu;
static uint8_t vram_cache[2][256 * 256];

void cgia_init(cgia_t* vpu, const cgia_desc_t* desc) {
    CHIPS_ASSERT(vpu && desc);
    CHIPS_ASSERT(desc->framebuffer.ptr && (desc->framebuffer.size == CGIA_FRAMEBUFFER_SIZE_BYTES));
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

    vpu->vram[0] = vram_cache[0];
    vpu->vram[1] = vram_cache[1];

    CGIA_vpu = vpu;

    fwcgia_init();
}

void cgia_reset(cgia_t* vpu) {
    CHIPS_ASSERT(vpu);
    vpu->h_count = 0;
    vpu->l_count = 0;
    CGIA_vpu = NULL;
}

static uint64_t _cgia_tick(cgia_t* vpu, uint64_t pins) {
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

        uint32_t* src = vpu->linebuffer + CGIA_LINEBUFFER_PADDING;

        if (vpu->l_count >= MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + MODE_V_BACK_PORCH) {
            vpu->active_line = vpu->l_count - (MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + MODE_V_BACK_PORCH);

            if (vpu->active_line % FB_V_REPEAT == 0) {
                // rasterize new line
                cgia_render(vpu->active_line / FB_V_REPEAT, src);
            }
        }

        uint8_t* dst = vpu->fb + (vpu->active_line * CGIA_FRAMEBUFFER_WIDTH);
        for (uint x = 0; x < CGIA_ACTIVE_WIDTH; ++x, ++src) {
            for (uint r = 0; r < FB_H_REPEAT; ++r) {
                *dst++ = *src & 0xFF;
            }
        }
    }

    return pins;
}

#define CGIA_REG16(ADDR) (uint16_t)((uint16_t)(vpu->reg[ADDR]) | ((uint16_t)(vpu->reg[ADDR + 1]) << 8))

static uint8_t _cgia_read(cgia_t* vpu, uint8_t addr) {
    return vpu->reg[addr];
}

static void _cgia_write(cgia_t* vpu, uint8_t addr, uint8_t data) {
    vpu->reg[addr] = data;
}

uint64_t cgia_tick(cgia_t* vpu, uint64_t pins) {
    pins = _cgia_tick(vpu, pins);
    if (pins & CGIA_CS) {
        uint8_t addr = CGIA_GET_ADDR(pins);
        if (pins & CGIA_RW) {
            uint8_t data = _cgia_read(vpu, addr);
            CGIA_SET_DATA(pins, data);
        }
        else {
            uint8_t data = CGIA_GET_DATA(pins);
            _cgia_write(vpu, addr, data);
        }
    }

    cgia_task();

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

typedef enum {
    INTERP_DEFAULT,
    INTERP_MODE7,
} interp_mode_t;

typedef struct {
    uintptr_t accum[2];
    uintptr_t base[3];
    uint8_t shift[2];
    uint32_t mask[2];
} interp_hw_t;

static interp_hw_t interp_hw_array[2];
#define interp0 (&interp_hw_array[0])
#define interp1 (&interp_hw_array[1])

typedef struct {
    uintptr_t accum[2];
    uintptr_t base[3];
    uint8_t shift[2];
    uint32_t mask[2];
} interp_hw_save_t;

void interp_save(interp_hw_t* interp, interp_hw_save_t* saver) {
    saver->accum[0] = interp->accum[0];
    saver->accum[1] = interp->accum[1];
    saver->base[0] = interp->base[0];
    saver->base[1] = interp->base[1];
    saver->base[2] = interp->base[2];
    saver->shift[0] = interp->shift[0];
    saver->shift[1] = interp->shift[1];
    saver->mask[0] = interp->mask[0];
    saver->mask[1] = interp->mask[1];
}

void interp_restore(interp_hw_t* interp, interp_hw_save_t* saver) {
    interp->accum[0] = saver->accum[0];
    interp->accum[1] = saver->accum[1];
    interp->base[0] = saver->base[0];
    interp->base[1] = saver->base[1];
    interp->base[2] = saver->base[2];
    interp->shift[0] = saver->shift[0];
    interp->shift[1] = saver->shift[1];
    interp->mask[0] = saver->mask[0];
    interp->mask[1] = saver->mask[1];
}

static inline uintptr_t interp_get_accumulator(interp_hw_t* interp, uint lane) {
    return interp->accum[lane];
}
static inline uintptr_t interp_pop_lane_result(interp_hw_t* interp, uint lane) {
    assert(lane < 3);
    // compute masked values
    uint32_t lane0 = ((uint32_t)(interp->accum[0]) >> interp->shift[0]) & interp->mask[0];
    uint32_t lane1 = ((uint32_t)(interp->accum[1]) >> interp->shift[1]) & interp->mask[1];
    // advance to next state
    interp->accum[0] += interp->base[0];
    interp->accum[1] += interp->base[1];

    if (lane == 2) {
        return interp->base[2] + (uint32_t)(lane0 + lane1);
    }

    return interp->accum[lane];
}
static inline uintptr_t interp_peek_lane_result(interp_hw_t* interp, uint lane) {
    assert(lane < 2);
    return interp->accum[lane] + interp->base[lane];
}

static inline void set_linear_scans(
    uint8_t row_height,
    const uint8_t* memory_scan,
    const uint8_t* colour_scan,
    const uint8_t* backgr_scan) {
    assert(memory_scan + row_height >= vram_cache[0]);
    assert((uintptr_t)memory_scan + row_height < (uintptr_t)(vram_cache[2]));
    assert(colour_scan + 1 >= vram_cache[0]);
    assert((uintptr_t)colour_scan + 1 < (uintptr_t)(vram_cache[2]));
    assert(backgr_scan + 1 >= vram_cache[0]);
    assert((uintptr_t)backgr_scan + 1 < (uintptr_t)(vram_cache[2]));

    interp0->base[0] = row_height;
    interp0->accum[0] = (uintptr_t)memory_scan;
    interp1->base[0] = 1;
    interp1->accum[0] = (uintptr_t)colour_scan;
    interp1->base[1] = 1;
    interp1->accum[1] = (uintptr_t)backgr_scan;
}
static inline void set_mode7_interp_config(struct cgia_plane_t* plane) {
    // interp0 will scan texture row
    const uint texture_width_bits = plane->regs.affine.texture_bits & 0b0111;
    interp0->shift[0] = CGIA_AFFINE_FRACTIONAL_BITS;
    interp0->mask[0] = (1U << (texture_width_bits)) - 1;
    const uint texture_height_bits = (plane->regs.affine.texture_bits >> 4) & 0b0111;
    interp0->shift[1] = CGIA_AFFINE_FRACTIONAL_BITS - texture_width_bits;
    interp0->mask[1] = ((1U << (texture_height_bits)) - 1) << texture_width_bits;

    // interp1 will scan row begin address
    interp1->shift[0] = CGIA_AFFINE_FRACTIONAL_BITS;
    interp1->mask[0] = (1U << (texture_width_bits)) - 1;
    interp1->shift[1] = 0;
    interp1->mask[1] = ((1U << (texture_height_bits)) - 1) << CGIA_AFFINE_FRACTIONAL_BITS;

    // start texture rows scan
    interp1->accum[0] = plane->regs.affine.u;
    interp1->base[0] = plane->regs.affine.dx;
    interp1->accum[1] = plane->regs.affine.v;
    interp1->base[1] = plane->regs.affine.dy;
    interp1->base[2] = 0;
}
static inline void set_mode7_scans(struct cgia_plane_t* plane, uint8_t* memory_scan) {
    assert(memory_scan >= vram_cache[0]);
    assert((uintptr_t)memory_scan < (uintptr_t)(vram_cache[2]));

    interp0->base[2] = (uintptr_t)memory_scan;
    const uint32_t xy = (uint32_t)interp_pop_lane_result(interp1, 2);
    // start texture columns scan
    interp0->accum[0] = (xy & 0x00FF) << CGIA_AFFINE_FRACTIONAL_BITS;
    interp0->base[0] = plane->regs.affine.du;
    interp0->accum[1] = (xy & 0xFF00);
    interp0->base[1] = plane->regs.affine.dv;
}

#define FRAME_WIDTH  CGIA_DISPLAY_WIDTH
#define FRAME_HEIGHT CGIA_DISPLAY_HEIGHT

static inline uint32_t* fill_back(uint32_t* rgbbuf, uint32_t columns, uint32_t color_idx) {
    uint pixels = columns * CGIA_COLUMN_PX;
    while (pixels) {
        *rgbbuf++ = color_idx;
        --pixels;
    }
    return rgbbuf;
}

#include "firmware/src/ria/cgia/cgia_encode.h"

uint32_t*
cgia_encode_mode_2(uint32_t* rgbbuf, uint32_t columns, uint8_t* character_generator, uint32_t char_shift, bool mapped) {
    while (columns) {
        uintptr_t bg_cl_addr = interp_peek_lane_result(interp1, 1);
        uint8_t bg_cl = *((uint8_t*)bg_cl_addr);
        uintptr_t fg_cl_addr = interp_pop_lane_result(interp1, 0);
        uint8_t fg_cl = *((uint8_t*)fg_cl_addr);
        uintptr_t chr_addr = interp_pop_lane_result(interp0, 0);
        uint8_t chr = *((uint8_t*)chr_addr);
        uint8_t bits = character_generator[chr << char_shift];
        for (int shift = 7; shift >= 0; shift--) {
            uint bit_set = (bits >> shift) & 0b1;
            if (bit_set) {
                *rgbbuf++ = fg_cl;
            }
            else {
                if (mapped) {
                    *rgbbuf++ = bg_cl;
                }
                else {
                    rgbbuf++;  // transparent pixel
                }
            }
        }
        --columns;
    }

    return rgbbuf;
}

inline uint32_t* __attribute__((always_inline))
cgia_encode_mode_2_shared(uint32_t* rgbbuf, uint32_t columns, uint8_t* character_generator, uint32_t char_shift) {
    return cgia_encode_mode_2(rgbbuf, columns, character_generator, char_shift, false);
}
inline uint32_t* __attribute__((always_inline))
cgia_encode_mode_2_mapped(uint32_t* rgbbuf, uint32_t columns, uint8_t* character_generator, uint32_t char_shift) {
    return cgia_encode_mode_2(rgbbuf, columns, character_generator, char_shift, true);
}
uint32_t* cgia_encode_vt(uint32_t* rgbbuf, uint32_t columns, uint8_t* character_generator, uint32_t char_shift) {
    abort();
}

uint32_t* cgia_encode_mode_3(uint32_t* rgbbuf, uint32_t columns, bool mapped) {
    while (columns) {
        uintptr_t bg_cl_addr = interp_peek_lane_result(interp1, 1);
        uint8_t bg_cl = *((uint8_t*)bg_cl_addr);
        uintptr_t fg_cl_addr = interp_pop_lane_result(interp1, 0);
        uint8_t fg_cl = *((uint8_t*)fg_cl_addr);
        uintptr_t bits_addr = interp_pop_lane_result(interp0, 0);
        uint8_t bits = *((uint8_t*)bits_addr);
        for (int shift = 7; shift >= 0; shift--) {
            uint bit_set = (bits >> shift) & 0b1;
            if (bit_set) {
                *rgbbuf++ = fg_cl;
            }
            else {
                if (mapped) {
                    *rgbbuf++ = bg_cl;
                }
                else {
                    rgbbuf++;  // transparent pixel
                }
            }
        }
        --columns;
    }

    return rgbbuf;
}

inline uint32_t* __attribute__((always_inline)) cgia_encode_mode_3_shared(uint32_t* rgbbuf, uint32_t columns) {
    return cgia_encode_mode_3(rgbbuf, columns, false);
}

inline uint32_t* __attribute__((always_inline)) cgia_encode_mode_3_mapped(uint32_t* rgbbuf, uint32_t columns) {
    return cgia_encode_mode_3(rgbbuf, columns, true);
}

inline uint32_t* __attribute__((always_inline)) cgia_encode_mode_4_shared(
    uint32_t* rgbbuf,
    uint32_t columns,
    uint8_t* character_generator,
    uint32_t char_shift,
    uint8_t shared_colors[2]) {
    printf("cgia_encode_mode_4_shared\n");
    return rgbbuf;
}

inline uint32_t* __attribute__((always_inline)) cgia_encode_mode_4_mapped(
    uint32_t* rgbbuf,
    uint32_t columns,
    uint8_t* character_generator,
    uint32_t char_shift,
    uint8_t shared_colors[2]) {
    printf("cgia_encode_mode_4_mapped\n");
    return rgbbuf;
}

inline uint32_t* __attribute__((always_inline)) cgia_encode_mode_4_doubled_shared(
    uint32_t* rgbbuf,
    uint32_t columns,
    uint8_t* character_generator,
    uint32_t char_shift,
    uint8_t shared_colors[2]) {
    printf("cgia_encode_mode_4_doubled_shared\n");
    return rgbbuf;
}

inline uint32_t* __attribute__((always_inline)) cgia_encode_mode_4_doubled_mapped(
    uint32_t* rgbbuf,
    uint32_t columns,
    uint8_t* character_generator,
    uint32_t char_shift,
    uint8_t shared_colors[2]) {
    printf("cgia_encode_mode_4_doubled_mapped\n");
    return rgbbuf;
}

uint32_t* cgia_encode_mode_5(uint32_t* rgbbuf, uint32_t columns, uint8_t shared_colors[2], bool doubled, bool mapped) {
    while (columns) {
        uintptr_t bg_cl_addr = interp_peek_lane_result(interp1, 1);
        uint8_t bg_cl = *((uint8_t*)bg_cl_addr);
        uintptr_t fg_cl_addr = interp_pop_lane_result(interp1, 0);
        uint8_t fg_cl = *((uint8_t*)fg_cl_addr);
        uintptr_t bits_addr = interp_pop_lane_result(interp0, 0);
        uint8_t bits = *((uint8_t*)bits_addr);
        for (int shift = 6; shift >= 0; shift -= 2) {
            uint color_no = (bits >> shift) & 0b11;
            switch (color_no) {
                case 0b00:
                    if (mapped) {
                        *rgbbuf++ = shared_colors[0];
                        if (doubled) *rgbbuf++ = shared_colors[0];
                    }
                    else {
                        rgbbuf++;  // transparent pixel
                        if (doubled) rgbbuf++;
                    }
                    break;
                case 0b01:
                    *rgbbuf++ = bg_cl;
                    if (doubled) *rgbbuf++ = bg_cl;
                    break;
                case 0b10:
                    *rgbbuf++ = fg_cl;
                    if (doubled) *rgbbuf++ = fg_cl;
                    break;
                case 0b11:
                    *rgbbuf++ = shared_colors[1];
                    if (doubled) *rgbbuf++ = shared_colors[1];
                    break;
                default: abort();
            }
        }
        --columns;
    }

    return rgbbuf;
}

inline uint32_t* __attribute__((always_inline))
cgia_encode_mode_5_shared(uint32_t* rgbbuf, uint32_t columns, uint8_t shared_colors[2]) {
    return cgia_encode_mode_5(rgbbuf, columns, shared_colors, false, false);
}

inline uint32_t* __attribute__((always_inline))
cgia_encode_mode_5_mapped(uint32_t* rgbbuf, uint32_t columns, uint8_t shared_colors[2]) {
    return cgia_encode_mode_5(rgbbuf, columns, shared_colors, false, true);
}

inline uint32_t* __attribute__((always_inline))
cgia_encode_mode_5_doubled_shared(uint32_t* rgbbuf, uint32_t columns, uint8_t shared_colors[2]) {
    return cgia_encode_mode_5(rgbbuf, columns, shared_colors, true, false);
}

inline uint32_t* __attribute__((always_inline))
cgia_encode_mode_5_doubled_mapped(uint32_t* rgbbuf, uint32_t columns, uint8_t shared_colors[2]) {
    return cgia_encode_mode_5(rgbbuf, columns, shared_colors, true, true);
}

uint32_t* cgia_encode_mode_7(uint32_t* rgbbuf, uint32_t columns) {
    while (columns) {
        for (int p = 0; p < 8; ++p) {
            uintptr_t cl_addr = interp_pop_lane_result(interp0, 2);
            assert(cl_addr >= (uintptr_t)vram_cache[0]);
            assert(cl_addr < (uintptr_t)vram_cache[2]);
            *rgbbuf++ = *((uint8_t*)cl_addr);
        }
        --columns;
    }

    return rgbbuf;
}

void cgia_encode_sprite(uint32_t* rgbbuf, uint32_t* descriptor, uint8_t* line_data, uint32_t width) {
    printf("cgia_encode_sprite\n");
}

#define CGIA      (*((struct cgia_t*)CGIA_vpu->reg))
#define cgia_init fwcgia_init
#include "firmware/src/ria/cgia/cgia.c"
#undef cgia_init

static void _cgia_copy_vcache_bank(cgia_t* vpu, uint8_t bank) {
    for (size_t i = 0; i < 256 * 256; ++i) {
        uint64_t pins = vpu->fetch_cb((vram_wanted_bank_mask[bank] | i), vpu->user_data);
        vram_cache[bank][i] = CGIA_GET_DATA(pins);
    }
}
static void _cgia_transfer_vcache_bank(uint8_t bank) {
    assert(CGIA_vpu);
    if (vram_wanted_bank_mask[bank] != vram_cache_bank_mask[bank]) {
        _cgia_copy_vcache_bank(CGIA_vpu, bank);
    }
}
void cgia_mirror_vram(cgia_t* vpu) {
    _cgia_copy_vcache_bank(vpu, 0);
    _cgia_copy_vcache_bank(vpu, 1);
}
