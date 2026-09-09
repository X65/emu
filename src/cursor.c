//------------------------------------------------------------------------------
//  cursor.c
//
//  The X65 mouse pointer, the same one the sgu-tracker draws (its
//  assets/cursor.png). The art is kept as ASCII rather than as a pixel dump so
//  it stays readable and editable in place -- at 20x20 with six colours there is
//  nothing a decoder would buy us.
//
//  It is bound to the ARROW slot, which is what sokol_imgui asks for whenever
//  ImGui does not want a shape of its own. So the pointer is ours everywhere,
//  while the I-beam and the resize arrows stay the system's.
//------------------------------------------------------------------------------
#include "cursor.h"
#include "sokol_app.h"
#include <stdint.h>

#define CURSOR_WIDTH     (20)
#define CURSOR_HEIGHT    (20)
#define CURSOR_HOTSPOT_X (3)
#define CURSOR_HOTSPOT_Y (2)

// The row length is the array bound, so a mistyped row is a compile error.
// clang-format off
static const char cursor_art[CURSOR_HEIGHT][CURSOR_WIDTH + 1] = {
    "..####..............",
    ".#WWWW#.............",
    "#CWWWWW#............",
    "#CWW--WW#...........",
    "#CWW---WW#..........",
    "#CWW----WW#.........",
    "#CWW-----WW#........",
    "#CWW-++---WW#.......",
    "#CWW+++++--WW#......",
    "#CWW++++++--WW#.....",
    "#CWW++++++++-WW#....",
    "#CWW+++++++++-WW#...",
    "#CWW+++++++++++W#...",
    "#CWW++++++++WWWW#...",
    "#CWW+++++++WW###....",
    "#CWW+++++++W#.......",
    "#CWWWWWW+++W#.......",
    "#C----CWWWWW#.......",
    ".######C----#.......",
    ".......#####........",
};
// clang-format on

static const struct {
    char ch;
    uint8_t r, g, b, a;
} cursor_palette[] = {
    { '.', 0x00, 0x00, 0x00, 0x00 }, // transparent (magic pink in the source art)
    { '#', 0x00, 0x00, 0x00, 0xFF }, // outline
    { 'W', 0xFF, 0xFF, 0xFF, 0xFF }, // highlight
    { '+', 0xDA, 0xDA, 0xDA, 0xFF }, // body
    { '-', 0xBC, 0xBC, 0xBC, 0xFF }, // shade
    { 'C', 0x00, 0xAF, 0xD7, 0xFF }, // X65 cyan
};

void cursor_init(void) {
    uint8_t pixels[CURSOR_WIDTH * CURSOR_HEIGHT * 4] = { 0 };
    for (int y = 0; y < CURSOR_HEIGHT; y++) {
        for (int x = 0; x < CURSOR_WIDTH; x++) {
            const char ch = cursor_art[y][x];
            for (size_t i = 0; i < sizeof(cursor_palette) / sizeof(cursor_palette[0]); i++) {
                if (cursor_palette[i].ch == ch) {
                    uint8_t* dst = &pixels[(y * CURSOR_WIDTH + x) * 4];
                    dst[0] = cursor_palette[i].r;
                    dst[1] = cursor_palette[i].g;
                    dst[2] = cursor_palette[i].b;
                    dst[3] = cursor_palette[i].a;
                    break;
                }
            }
        }
    }
    sapp_bind_mouse_cursor_image(
        SAPP_MOUSECURSOR_ARROW,
        &(sapp_image_desc){
            .width = CURSOR_WIDTH,
            .height = CURSOR_HEIGHT,
            .cursor_hotspot_x = CURSOR_HOTSPOT_X,
            .cursor_hotspot_y = CURSOR_HOTSPOT_Y,
            .pixels = (sapp_range){ pixels, sizeof(pixels) },
    });
    sapp_set_mouse_cursor(SAPP_MOUSECURSOR_ARROW);
}

void cursor_shutdown(void) {
    // Not merely tidy: sokol-app's own teardown unbinds every custom cursor
    // image, but it does that in _sapp_discard_state(), which on X11 runs
    // *after* XCloseDisplay(). Unbinding the active cursor there re-defines the
    // window cursor on a closed display and segfaults. Doing it here, from the
    // cleanup callback, the display is still up and sokol's later pass finds
    // nothing left to release.
    sapp_unbind_mouse_cursor_image(SAPP_MOUSECURSOR_ARROW);
}
