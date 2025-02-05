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

    pwm_init(&vpu->pwm[0], desc->tick_hz);
    pwm_init(&vpu->pwm[1], desc->tick_hz);
}

void cgia_reset(cgia_t* vpu) {
    CHIPS_ASSERT(vpu);
    vpu->h_count = 0;
    vpu->l_count = 0;
    pwm_reset(&vpu->pwm[0]);
    pwm_reset(&vpu->pwm[1]);
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
            cgia_vbi();
        }

        uint32_t* src = vpu->linebuffer + CGIA_LINEBUFFER_PADDING;

        if (vpu->l_count >= MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + MODE_V_BACK_PORCH) {
            vpu->active_line = vpu->l_count - (MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + MODE_V_BACK_PORCH);

            if (vpu->active_line % FB_V_REPEAT == 0) {
                // rasterize new line
                vpu->badline = true;
                vpu->raster_line = (uint16_t)(vpu->active_line / FB_V_REPEAT);
                cgia_render(vpu->raster_line, src);

                // bump right after processing, so CPU is free to modify regs
                // before next line rasterization starts
                ++vpu->raster_line;
                if (vpu->raster_line >= CGIA_ACTIVE_HEIGHT) vpu->raster_line = 0;
            }
            else {
                vpu->badline = false;
            }
        }
        else {
            vpu->raster_line = 0;
        }

        uint32_t* dst = vpu->fb + (vpu->active_line * CGIA_FRAMEBUFFER_WIDTH);
        for (unsigned int x = 0; x < CGIA_ACTIVE_WIDTH; ++x, ++src) {
            for (unsigned int r = 0; r < FB_H_REPEAT; ++r) {
                *dst++ = *src | 0xFF000000;  // set ALPHA channel to 100% opacity
            }
        }
    }

    return pins;
}

#define CGIA_REG16(ADDR) (uint16_t)((uint16_t)(vpu->reg[ADDR]) | ((uint16_t)(vpu->reg[(ADDR) + 1]) << 8))

static uint8_t _cgia_read(cgia_t* vpu, uint8_t addr) {
    switch (addr) {
        case CGIA_REG_INT_STATUS: return vpu->reg[CGIA_REG_INT_STATUS] & vpu->reg[CGIA_REG_INT_ENABLE];

        case CGIA_REG_RASTER: return vpu->raster_line & 0xFF;
        case CGIA_REG_RASTER + 1: return vpu->raster_line >> 8;
    }

    return vpu->reg[addr];
}

static void _cgia_write(cgia_t* vpu, uint8_t addr, uint8_t data) {
    vpu->reg[addr] = data;

    switch (addr) {
        case CGIA_REG_INT_ENABLE: vpu->reg[addr] = data & 0b11100000; break;
        case CGIA_REG_INT_STATUS: cgia_clear_int(); break;

        case CGIA_REG_PWM_0_FREQ:
        case CGIA_REG_PWM_0_FREQ + 1: pwm_set_freq(&vpu->pwm[0], CGIA_REG16(CGIA_REG_PWM_0_FREQ)); break;
        case CGIA_REG_PWM_0_DUTY: pwm_set_duty(&vpu->pwm[0], data); break;
        case CGIA_REG_PWM_1_FREQ:
        case CGIA_REG_PWM_1_FREQ + 1: pwm_set_freq(&vpu->pwm[1], CGIA_REG16(CGIA_REG_PWM_1_FREQ)); break;
        case CGIA_REG_PWM_1_DUTY: pwm_set_duty(&vpu->pwm[1], data); break;
    }
}

static void _copy_internal_regs(cgia_t* vpu);

uint64_t cgia_tick(cgia_t* vpu, uint64_t pins) {
    pins = _cgia_tick(vpu, pins);

    // handle registers
    if (pins & CGIA_CS) {
        uint8_t addr = CGIA_GET_REG_ADDR(pins);
        if (pins & CGIA_RW) {
            uint8_t data = _cgia_read(vpu, addr);
            CGIA_SET_DATA(pins, data);
        }
        else {
            uint8_t data = CGIA_GET_DATA(pins);
            _cgia_write(vpu, addr, data);
        }
    }
    // mirror RAM writes to VRAM cache
    if (!(pins & CGIA_RW)) {
        uint32_t addr = CGIA_GET_ADDR(pins);
        uint8_t data = CGIA_GET_DATA(pins);
        cgia_ram_write(addr, data);
    }

    cgia_task();
    _copy_internal_regs(vpu);

    pwm_tick(&vpu->pwm[0]);
    pwm_tick(&vpu->pwm[1]);

    if (vpu->reg[CGIA_REG_INT_STATUS]) {
        pins |= CGIA_INT;
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

static inline uintptr_t interp_get_accumulator(interp_hw_t* interp, unsigned int lane) {
    return interp->accum[lane];
}
static inline uintptr_t interp_pop_lane_result(interp_hw_t* interp, unsigned int lane) {
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
static inline uintptr_t interp_peek_lane_result(interp_hw_t* interp, unsigned int lane) {
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
static inline void set_mode7_interp_config(union cgia_plane_regs_t* plane) {
    // interp0 will scan texture row
    const unsigned int texture_width_bits = plane->affine.texture_bits & 0b0111;
    interp0->shift[0] = CGIA_AFFINE_FRACTIONAL_BITS;
    interp0->mask[0] = (1U << (texture_width_bits)) - 1;
    const unsigned int texture_height_bits = (plane->affine.texture_bits >> 4) & 0b0111;
    interp0->shift[1] = CGIA_AFFINE_FRACTIONAL_BITS - texture_width_bits;
    interp0->mask[1] = ((1U << (texture_height_bits)) - 1) << texture_width_bits;

    // interp1 will scan row begin address
    interp1->shift[0] = CGIA_AFFINE_FRACTIONAL_BITS;
    interp1->mask[0] = (1U << (texture_width_bits)) - 1;
    interp1->shift[1] = 0;
    interp1->mask[1] = ((1U << (texture_height_bits)) - 1) << CGIA_AFFINE_FRACTIONAL_BITS;

    // start texture rows scan
    interp1->accum[0] = plane->affine.u;
    interp1->base[0] = plane->affine.dx;
    interp1->accum[1] = plane->affine.v;
    interp1->base[1] = plane->affine.dy;
    interp1->base[2] = 0;
}
static inline void set_mode7_scans(union cgia_plane_regs_t* plane, uint8_t* memory_scan) {
    assert(memory_scan >= vram_cache[0]);
    assert((uintptr_t)memory_scan < (uintptr_t)(vram_cache[2]));

    interp0->base[2] = (uintptr_t)memory_scan;
    const uint32_t xy = (uint32_t)interp_pop_lane_result(interp1, 2);
    // start texture columns scan
    interp0->accum[0] = (xy & 0x00FF) << CGIA_AFFINE_FRACTIONAL_BITS;
    interp0->base[0] = plane->affine.du;
    interp0->accum[1] = (xy & 0xFF00);
    interp0->base[1] = plane->affine.dv;
}

#define FRAME_WIDTH  CGIA_DISPLAY_WIDTH
#define FRAME_HEIGHT CGIA_DISPLAY_HEIGHT

static inline uint32_t* fill_back(uint32_t* rgbbuf, uint32_t columns, uint32_t color_idx) {
    unsigned int pixels = columns * CGIA_COLUMN_PX;
    while (pixels) {
        *rgbbuf++ = cgia_rgb_palette[color_idx];
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
            unsigned int bit_set = (bits >> shift) & 0b1;
            if (bit_set) {
                *rgbbuf++ = cgia_rgb_palette[fg_cl];
            }
            else {
                if (mapped) {
                    *rgbbuf++ = cgia_rgb_palette[bg_cl];
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
            unsigned int bit_set = (bits >> shift) & 0b1;
            if (bit_set) {
                *rgbbuf++ = cgia_rgb_palette[fg_cl];
            }
            else {
                if (mapped) {
                    *rgbbuf++ = cgia_rgb_palette[bg_cl];
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

uint32_t* cgia_encode_mode_4(
    uint32_t* rgbbuf,
    uint32_t columns,
    uint8_t* character_generator,
    uint32_t char_shift,
    uint8_t shared_colors[2],
    bool doubled,
    bool mapped) {
    while (columns) {
        uintptr_t bg_cl_addr = interp_peek_lane_result(interp1, 1);
        uint8_t bg_cl = *((uint8_t*)bg_cl_addr);
        uintptr_t fg_cl_addr = interp_pop_lane_result(interp1, 0);
        uint8_t fg_cl = *((uint8_t*)fg_cl_addr);
        uintptr_t chr_addr = interp_pop_lane_result(interp0, 0);
        uint8_t chr = *((uint8_t*)chr_addr);
        uint8_t bits = character_generator[chr << char_shift];
        for (int shift = 6; shift >= 0; shift -= 2) {
            unsigned int color_no = (bits >> shift) & 0b11;
            switch (color_no) {
                case 0b00:
                    if (mapped) {
                        *rgbbuf++ = cgia_rgb_palette[shared_colors[0]];
                        if (doubled) *rgbbuf++ = cgia_rgb_palette[shared_colors[0]];
                    }
                    else {
                        rgbbuf++;  // transparent pixel
                        if (doubled) rgbbuf++;
                    }
                    break;
                case 0b01:
                    *rgbbuf++ = cgia_rgb_palette[bg_cl];
                    if (doubled) *rgbbuf++ = cgia_rgb_palette[bg_cl];
                    break;
                case 0b10:
                    *rgbbuf++ = cgia_rgb_palette[fg_cl];
                    if (doubled) *rgbbuf++ = cgia_rgb_palette[fg_cl];
                    break;
                case 0b11:
                    *rgbbuf++ = cgia_rgb_palette[shared_colors[1]];
                    if (doubled) *rgbbuf++ = cgia_rgb_palette[shared_colors[1]];
                    break;
                default: abort();
            }
        }
        --columns;
    }

    return rgbbuf;
}

inline uint32_t* __attribute__((always_inline)) cgia_encode_mode_4_shared(
    uint32_t* rgbbuf,
    uint32_t columns,
    uint8_t* character_generator,
    uint32_t char_shift,
    uint8_t shared_colors[2]) {
    return cgia_encode_mode_4(rgbbuf, columns, character_generator, char_shift, shared_colors, false, false);
}

inline uint32_t* __attribute__((always_inline)) cgia_encode_mode_4_mapped(
    uint32_t* rgbbuf,
    uint32_t columns,
    uint8_t* character_generator,
    uint32_t char_shift,
    uint8_t shared_colors[2]) {
    return cgia_encode_mode_4(rgbbuf, columns, character_generator, char_shift, shared_colors, false, true);
}

inline uint32_t* __attribute__((always_inline)) cgia_encode_mode_4_doubled_shared(
    uint32_t* rgbbuf,
    uint32_t columns,
    uint8_t* character_generator,
    uint32_t char_shift,
    uint8_t shared_colors[2]) {
    return cgia_encode_mode_4(rgbbuf, columns, character_generator, char_shift, shared_colors, true, false);
}

inline uint32_t* __attribute__((always_inline)) cgia_encode_mode_4_doubled_mapped(
    uint32_t* rgbbuf,
    uint32_t columns,
    uint8_t* character_generator,
    uint32_t char_shift,
    uint8_t shared_colors[2]) {
    return cgia_encode_mode_4(rgbbuf, columns, character_generator, char_shift, shared_colors, true, true);
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
            unsigned int color_no = (bits >> shift) & 0b11;
            switch (color_no) {
                case 0b00:
                    if (mapped) {
                        *rgbbuf++ = cgia_rgb_palette[shared_colors[0]];
                        if (doubled) *rgbbuf++ = cgia_rgb_palette[shared_colors[0]];
                    }
                    else {
                        rgbbuf++;  // transparent pixel
                        if (doubled) rgbbuf++;
                    }
                    break;
                case 0b01:
                    *rgbbuf++ = cgia_rgb_palette[bg_cl];
                    if (doubled) *rgbbuf++ = cgia_rgb_palette[bg_cl];
                    break;
                case 0b10:
                    *rgbbuf++ = cgia_rgb_palette[fg_cl];
                    if (doubled) *rgbbuf++ = cgia_rgb_palette[fg_cl];
                    break;
                case 0b11:
                    *rgbbuf++ = cgia_rgb_palette[shared_colors[1]];
                    if (doubled) *rgbbuf++ = cgia_rgb_palette[shared_colors[1]];
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

#define QUANTA_BITS (3)
uint32_t cgia_encode_mode_6_command(uint8_t cmd, uint32_t current_color, uint8_t base_colors[8]) {
    uint8_t code = (cmd & 0x38) >> 3;
    // Make changes in quanta
    uint8_t delta = (uint8_t)((cmd & 0x0f) << QUANTA_BITS);
    // Sign extend delta
    if (delta & (0b1000 << QUANTA_BITS)) delta |= 0xF0 << QUANTA_BITS;

    uint32_t channel_value;

    switch (code) {
        case 0b000:  // --- 000 - load base color
            current_color = cgia_rgb_palette[base_colors[cmd & 0b111]];
            break;
        case 0b001:  // --- 001 - blend current color
            const uint32_t base_color = cgia_rgb_palette[base_colors[cmd & 0b111]];
#if 0
            // Extract individual color channels
            uint8_t r1 = base_color & 0xFF;
            uint8_t g1 = (base_color >> 8) & 0xFF;
            uint8_t b1 = (base_color >> 16) & 0xFF;
            uint8_t r2 = current_color & 0xFF;
            uint8_t g2 = (current_color >> 8) & 0xFF;
            uint8_t b2 = (current_color >> 16) & 0xFF;
            // Compute averages (no overflow due to uint16_t promotion in addition)
            uint8_t r_avg = (r1 + r2) / 2;
            uint8_t g_avg = (g1 + g2) / 2;
            uint8_t b_avg = (b1 + b2) / 2;
            // Combine back into a single RGB888 value
            current_color = (r_avg << 16) | (g_avg << 8) | b_avg;
#else
            // fast blend using bit shifting and masking voodoo:
            // - remove lowest bits of channels to make place for overflow
            //   and not accumulate overflow with next channel lowest bit
            // - add everything at once (potentially overflowing to next channel)
            // - shift back into position, averaging the sum
            uint32_t fast_blend = ((base_color & 0xFEFEFE) + (current_color & 0xFEFEFE)) >> 1;
            // With above operation we lost lowest bits precision
            // We could ignore it, but we will accumulate total error and color fringing over raster time
            // Let's add it back:
            // - extract lowest bits of all channels
            // - add them and average by dividing (shifting)
            // - mask-out highest channel bits that potentially underflowed from next channel
            // - and finally add the fix to the previously blended value
            current_color = fast_blend + ((((base_color & 0x010101) + (current_color & 0x010101)) >> 1) & 0x7F7F7F);
#endif
            break;
        default:
            switch (code >> 1) {
                case 0b01:                                       // --- 01x - Modify Red channel
                    channel_value = current_color & 0xFF;        // extract RED color value
                    channel_value += delta;                      // add delta
                    current_color = current_color & 0xFFFFFF00;  // remove RED channel from current color
                    current_color |= channel_value & 0xFF;       // merge modified value, clamped to 8 bit
                    break;
                case 0b10:                                       // --- 10x - Modify Green channel
                    channel_value = current_color & 0xFF00;      // extract GREEN color value
                    channel_value += delta << 8;                 // add delta
                    current_color = current_color & 0xFFFF00FF;  // remove GREEN channel from current color
                    current_color |= channel_value & 0xFF00;     // merge modified value, clamped to 8 bit
                    break;
                case 0b11:                                       // --- 11x - Modify Blue channel
                    channel_value = current_color & 0xFF0000;    // extract BLUE color value
                    channel_value += delta << 16;                // add delta
                    current_color = current_color & 0xFF00FFFF;  // remove BLUE channel from current color
                    current_color |= channel_value & 0xFF0000;   // merge modified value, clamped to 8 bit
                    break;
            }
    }
    return current_color;
}
uint32_t*
cgia_encode_mode_6_common(uint32_t* rgbbuf, uint32_t columns, uint8_t base_color[8], uint8_t back_color, bool doubled) {
    // get current color from background color
    uint32_t current_color = cgia_rgb_palette[back_color];
    uintptr_t addr;
    uint8_t byte0, byte1, byte2, cmd;

    while (columns) {
        addr = interp_pop_lane_result(interp0, 0);
        byte0 = *((uint8_t*)addr);
        addr = interp_pop_lane_result(interp0, 0);
        byte1 = *((uint8_t*)addr);
        addr = interp_pop_lane_result(interp0, 0);
        byte2 = *((uint8_t*)addr);

        // extract first command
        cmd = (byte0 >> 2);
        current_color = cgia_encode_mode_6_command(cmd, current_color, base_color);
        *rgbbuf++ = current_color;
        if (doubled) *rgbbuf++ = current_color;

        // extract second command
        cmd = ((byte0 << 4) & 0x30) | (byte1 >> 4);
        current_color = cgia_encode_mode_6_command(cmd, current_color, base_color);
        *rgbbuf++ = current_color;
        if (doubled) *rgbbuf++ = current_color;

        // extract third command
        cmd = ((byte1 << 2) & 0x3C) | (byte2 >> 6);
        current_color = cgia_encode_mode_6_command(cmd, current_color, base_color);
        *rgbbuf++ = current_color;
        if (doubled) *rgbbuf++ = current_color;

        // extract fourth command
        cmd = (byte2 & 0x3F);
        current_color = cgia_encode_mode_6_command(cmd, current_color, base_color);
        *rgbbuf++ = current_color;
        if (doubled) *rgbbuf++ = current_color;

        --columns;
    }

    return rgbbuf;
}

inline uint32_t* __attribute__((always_inline))
cgia_encode_mode_6(uint32_t* rgbbuf, uint32_t columns, uint8_t base_color[8], uint8_t back_color) {
    return cgia_encode_mode_6_common(rgbbuf, columns, base_color, back_color, false);
}

inline uint32_t* __attribute__((always_inline))
cgia_encode_mode_6_doubled(uint32_t* rgbbuf, uint32_t columns, uint8_t base_color[8], uint8_t back_color) {
    return cgia_encode_mode_6_common(rgbbuf, columns, base_color, back_color, true);
}

uint32_t* cgia_encode_mode_7(uint32_t* rgbbuf, uint32_t columns) {
    while (columns) {
        for (int p = 0; p < 8; ++p) {
            uintptr_t cl_addr = interp_pop_lane_result(interp0, 2);
            assert(cl_addr >= (uintptr_t)vram_cache[0]);
            assert(cl_addr < (uintptr_t)vram_cache[2]);
            *rgbbuf++ = cgia_rgb_palette[*((uint8_t*)cl_addr)];
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

static void _copy_internal_regs(cgia_t* vpu) {
    for (int i = 0; i < CGIA_PLANES; ++i) {
        vpu->internal[i].memory_scan = plane_int[i].memory_scan;
        vpu->internal[i].colour_scan = plane_int[i].colour_scan;
        vpu->internal[i].backgr_scan = plane_int[i].backgr_scan;
        vpu->internal[i].chargen_offset = plane_int[i].char_gen_offset;
        vpu->internal[i].row_line_count = plane_int[i].row_line_count;
        vpu->internal[i].wait_vbl = plane_int[i].wait_vbl;
        vpu->internal[i].sprites_need_update = plane_int[i].sprites_need_update;
    }
}
