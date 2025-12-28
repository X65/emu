#pragma once
/*#
    # ui_sgu1.h

    Debug visualization for sgu1.h (SGU-1).

    Optionally provide the following macros with your own implementation

    ~~~C
    CHIPS_ASSERT(c)
    ~~~
        your own assert macro (default: assert(c))

    All strings provided to ui_sgu1_init() must remain alive until
    ui_sgu1_discard() is called!

    ## zlib/libpng license

    Copyright (c) 2025 Tomasz Sterna
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
#include "chips/sgu1.h"
#include "ui/ui_chip.h"
#include "ui/ui_settings.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* setup parameters for ui_sgu1_init()
    NOTE: all string data must remain alive until ui_sgu1_discard()!
*/
typedef struct ui_sgu1_desc_t {
    const char* title;        /* window title */
    sgu1_t* sgu;              /* pointer to sgu1_t instance to track */
    int x, y;                 /* initial window position */
    int w, h;                 /* initial window size (or default size of 0) */
    bool open;                /* initial window open state */
    ui_chip_desc_t chip_desc; /* chip visualization desc */
} ui_sgu1_desc_t;

typedef struct ui_sgu1_t {
    const char* title;
    sgu1_t* sgu;
    float init_x, init_y;
    float init_w, init_h;
    bool open;
    bool last_open;
    bool valid;
    ui_chip_t chip;
} ui_sgu1_t;

void ui_sgu1_init(ui_sgu1_t* win, const ui_sgu1_desc_t* desc);
void ui_sgu1_discard(ui_sgu1_t* win);
void ui_sgu1_draw(ui_sgu1_t* win);
void ui_sgu1_save_settings(ui_sgu1_t* win, ui_settings_t* settings);
void ui_sgu1_load_settings(ui_sgu1_t* win, const ui_settings_t* settings);

#ifdef __cplusplus
} /* extern "C" */
#endif
