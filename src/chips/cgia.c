#include "cgia.h"
#include "cgia_palette.h"

#include <string.h>

#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

// valid register bits
static const uint8_t _cgia_reg_mask[CGIA_NUM_REGS] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // mob 0..3 xy
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // mob 4..7 xy
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,              // msbx, ctrl1, raster, lpx, lpy, mob enabled
    0x3F,                                            // ctrl2
    0xFF,                                            // mob y expansion
    0xFE,                                            // memory pointers
    0x8F,                                            // interrupt latch
    0x0F,                                            // interrupt enabled mask
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // mob data pri, multicolor, x expansion, mob-mob coll, mob-data coll
    // 0x0F,                       // border color
    // 0x0F, 0x0F, 0x0F, 0x0F,     // background colors
    // 0x0F, 0x0F,                 // sprite multicolor 0,1
    // 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,     // sprite colors
    // 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // unused 0..7
    // 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // unused 8..15
    // 0x00                                                // unused 16
};

void cgia_init(cgia_t* cgia, const cgia_desc_t* desc) {
    CHIPS_ASSERT(cgia && desc);
    CHIPS_ASSERT(desc->framebuffer.ptr && (desc->framebuffer.size >= CGIA_FRAMEBUFFER_SIZE_BYTES));
    memset(cgia, 0, sizeof(*cgia));
    // _cgia_init_dvi(&cgia->crt, desc);
    cgia->fetch_cb = desc->fetch_cb;
    cgia->user_data = desc->user_data;
}

void cgia_reset(cgia_t* cgia) {
    CHIPS_ASSERT(cgia);
    // _cgia_reset_dvi(&cgia->dvi);
    // _cgia_reset_graphics_unit(&cgia->gunit);
    // _cgia_reset_sprite_unit(&cgia->sunit);
}

// read chip registers
static uint64_t _cgia_read(cgia_t* cgia, uint64_t pins) {
    cgia_registers_t* r = &cgia->reg;
    uint8_t r_addr = pins & CGIA_REG_MASK;
    uint8_t data;
    switch (r_addr) {
        case 0x1E:
        case 0x1F:
            // registers 0x1E and 0x1F (mob collisions) are cleared on reading
            data = r->regs[r_addr];
            r->regs[r_addr] = 0;
            break;
        default:
            // unconnected bits are returned as 1
            data = r->regs[r_addr] | ~_cgia_reg_mask[r_addr];
            break;
    }
    CGIA_SET_DATA(pins, data);
    return pins;
}

// write chip registers
static void _cgia_write(cgia_t* cgia, uint64_t pins) {
    cgia_registers_t* r = &cgia->reg;
    uint8_t r_addr = pins & CGIA_REG_MASK;
    const uint8_t data = CGIA_GET_DATA(pins) & _cgia_reg_mask[r_addr];
    bool write = true;
    switch (r_addr) {
        case 0x1E:
        case 0x1F:
            // mob collision registers cannot be written
            write = false;
            break;
    }
    if (write) {
        r->regs[r_addr] = data;
    }
}

// internal tick function
static uint64_t _cgia_tick(cgia_t* cgia, uint64_t pins) {
    return pins;
}

// all-in-one tick function
uint64_t cgia_tick(cgia_t* cgia, uint64_t pins) {
    // per-tick actions
    pins = _cgia_tick(cgia, pins);

    // register read/writes
    if (pins & CGIA_CS) {
        if (pins & CGIA_RW) {
            pins = _cgia_read(cgia, pins);
        }
        else {
            _cgia_write(cgia, pins);
        }
    }
    cgia->pins = pins;
    return pins;
}

chips_rect_t cgia_screen(cgia_t* vic) {
    CHIPS_ASSERT(vic);
    return (chips_rect_t){
        .x = 0,
        .y = 0,
        .width = CGIA_FRAMEBUFFER_WIDTH,
        .height = CGIA_FRAMEBUFFER_HEIGHT,
    };
}

chips_range_t cgia_palette(void) {
    return (chips_range_t){
        .ptr = (void*)cgia_rgb_palette,
        .size = sizeof(cgia_rgb_palette),
    };
}

uint32_t cgia_color(size_t i) {
    CHIPS_ASSERT(i < 256);
    return cgia_rgb_palette[i];
}

void cgia_snapshot_onsave(cgia_t* snapshot) {
    CHIPS_ASSERT(snapshot);
    // snapshot->crt.fb = 0;
}

void cgia_snapshot_onload(cgia_t* snapshot, cgia_t* sys) {
    CHIPS_ASSERT(snapshot && sys);
    // snapshot->crt.fb = sys->crt.fb;
}
