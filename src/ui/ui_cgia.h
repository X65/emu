#pragma once
/*#
    # ui_cgia.h

    Debug visualization for cgia.h

    Do this:
    ~~~C
    #define CHIPS_UI_IMPL
    ~~~
    before you include this file in *one* C++ file to create the
    implementation.

    Optionally provide the following macros with your own implementation

    ~~~C
    CHIPS_ASSERT(c)
    ~~~
        your own assert macro (default: assert(c))

    Include the following headers before the including the *declaration*:
        - cgia.h
        - ui_chip.h

    Include the following headers before including the *implementation*:
        - imgui.h
        - cgia.h
        - ui_chip.h
        - ui_util.h

    All string data provided to ui_cgia_init() must remain alive until
    until ui_cgia_discard() is called!

    ## zlib/libpng license

    Copyright (c) 2018 Andre Weissflog
    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source
        distribution.
#*/

#include "chips/cgia.h"
#include "ui/ui_chip.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* setup parameters for ui_cgia_init()
    NOTE: all string data must remain alive until ui_cgia_discard()!
*/
typedef struct {
    const char* title;        /* window title */
    cgia_t* cgia;             /* pointer to cgia_t instance to track */
    int x, y;                 /* initial window pos */
    int w, h;                 /* initial window size, or 0 for default size */
    bool open;                /* initial open state */
    ui_chip_desc_t chip_desc; /* chip visualization desc */
} ui_cgia_desc_t;

typedef struct {
    const char* title;
    cgia_t* cgia;
    float init_x, init_y;
    float init_w, init_h;
    bool open;
    bool valid;
    ui_chip_t chip;
} ui_cgia_t;

void ui_cgia_init(ui_cgia_t* win, const ui_cgia_desc_t* desc);
void ui_cgia_discard(ui_cgia_t* win);
void ui_cgia_draw(ui_cgia_t* win);

#ifdef __cplusplus
} /* extern "C" */
#endif
