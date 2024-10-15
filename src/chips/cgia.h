#pragma once

#include "chips/chips_common.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CGIA_H                      (768)
#define CGIA_V                      (240)
#define CGIA_FRAMEBUFFER_WIDTH      (CGIA_H)
#define CGIA_FRAMEBUFFER_HEIGHT     (CGIA_V * 2)  // lines are doubled
#define CGIA_FRAMEBUFFER_SIZE_BYTES (CGIA_FRAMEBUFFER_WIDTH * CGIA_FRAMEBUFFER_HEIGHT)

// address bus lines (with CS active A0..A4 is register address)
// CGIA has direct access to shared memory, thus other address lines are not needed
#define CGIA_PIN_A0 (0)
#define CGIA_PIN_A1 (1)
#define CGIA_PIN_A2 (2)
#define CGIA_PIN_A3 (3)
#define CGIA_PIN_A4 (4)

// data bus pins D0..D7
#define CGIA_PIN_D0 (16)
#define CGIA_PIN_D1 (17)
#define CGIA_PIN_D2 (18)
#define CGIA_PIN_D3 (19)
#define CGIA_PIN_D4 (20)
#define CGIA_PIN_D5 (21)
#define CGIA_PIN_D6 (22)
#define CGIA_PIN_D7 (23)

// shared control pins
#define CGIA_PIN_RW  (24)  // shared with m6502 CPU
#define CGIA_PIN_IRQ (26)  // routed via Interrupt Controller
#define CGIA_PIN_NMI (27)  // connected directly to CPU

// chip-specific control pins
#define CGIA_PIN_CS (40)

// pin bit masks
#define CGIA_A0 (1ULL << CGIA_PIN_A0)
#define CGIA_A1 (1ULL << CGIA_PIN_A1)
#define CGIA_A2 (1ULL << CGIA_PIN_A2)
#define CGIA_A3 (1ULL << CGIA_PIN_A3)
#define CGIA_A4 (1ULL << CGIA_PIN_A4)
// #define CGIA_A5  (1ULL << CGIA_PIN_A5)
// #define CGIA_A6  (1ULL << CGIA_PIN_A6)
// #define CGIA_A7  (1ULL << CGIA_PIN_A7)
// #define CGIA_A8  (1ULL << CGIA_PIN_A8)
#define CGIA_D0  (1ULL << CGIA_PIN_D0)
#define CGIA_D1  (1ULL << CGIA_PIN_D1)
#define CGIA_D2  (1ULL << CGIA_PIN_D2)
#define CGIA_D3  (1ULL << CGIA_PIN_D3)
#define CGIA_D4  (1ULL << CGIA_PIN_D4)
#define CGIA_D5  (1ULL << CGIA_PIN_D5)
#define CGIA_D6  (1ULL << CGIA_PIN_D6)
#define CGIA_D7  (1ULL << CGIA_PIN_D7)
#define CGIA_RW  (1ULL << CGIA_PIN_RW)
#define CGIA_IRQ (1ULL << CGIA_PIN_IRQ)
#define CGIA_NMI (1ULL << CGIA_PIN_NMI)
#define CGIA_CS  (1ULL << CGIA_PIN_CS)

// number of registers
#define CGIA_NUM_REGS (32)
// register address mask
#define CGIA_REG_MASK (CGIA_NUM_REGS - 1)

// extract 8-bit data bus from 64-bit pins
#define CGIA_GET_DATA(p) ((uint8_t)(((p) & 0xFF0000ULL) >> 16))
// merge 8-bit data bus value into 64-bit pins
#define CGIA_SET_DATA(p, d) \
    { p = (((p) & ~0xFF0000ULL) | (((d) << 16) & 0xFF0000ULL)); }

// memory fetch callback, used to feed data into the cgia
typedef uint8_t (*cgia_fetch_t)(uint32_t addr, void* user_data);

// setup parameters for cgia_init() function
typedef struct {
    // pointer and size of external framebuffer (at least CGIA_FRAMEBUFFER_SIZE_BYTES big)
    chips_range_t framebuffer;
    // visible CRT area decoded into framebuffer (in pixels)
    chips_rect_t screen;
    // the memory-fetch callback
    cgia_fetch_t fetch_cb;
    // optional user-data for fetch callback
    void* user_data;
} cgia_desc_t;

// register bank
typedef struct {
    union
    {
        uint8_t regs[CGIA_NUM_REGS];
        struct {
            uint8_t ec;          // border color
            uint8_t bc[4];       // background colors
            uint8_t mm[2];       // sprite multicolor 0
            uint8_t mc[8];       // sprite colors
            uint8_t unused[17];  // not writable, return 0xFF on read
        };
    };
} cgia_registers_t;

// the cgia state structure
typedef struct {
    bool debug_vis;  // toggle this to switch debug visualization on/off
    cgia_registers_t reg;

    cgia_fetch_t fetch_cb;  // memory-fetch callback
    void* user_data;        // optional user-data for fetch callback

    uint64_t pins;
} cgia_t;

// initialize a new cgia_t instance
void cgia_init(cgia_t* cgia, const cgia_desc_t* desc);
// reset a cgia_t instance
void cgia_reset(cgia_t* cgia);
// tick the cgia instance
uint64_t cgia_tick(cgia_t* cgia, uint64_t pins);

extern uint32_t cgia_rgb_palette[256];
uint32_t cgia_color(size_t i);

#ifdef __cplusplus
}  // extern "C"
#endif
