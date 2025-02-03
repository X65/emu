#pragma once
/*#
    # ui_ymf262.h

    Debug visualization for ymf262.h (OPL3).

    Optionally provide the following macros with your own implementation

    ~~~C
    CHIPS_ASSERT(c)
    ~~~
        your own assert macro (default: assert(c))

    All strings provided to ui_ymf262_init() must remain alive until
    ui_ymf262_discard() is called!

    ## 0BSD license

    Copyright (c) 2025 Tomasz Sterna
    Permission to use, copy, modify, and/or distribute this software for any purpose
    with or without fee is hereby granted.
    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
    AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
    INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
    OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
    TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
    OF THIS SOFTWARE.
#*/

#include "chips/ymf262.h"
#include "ui/ui_chip.h"
#include "ui/ui_settings.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* setup parameters for ui_ymf262_init()
    NOTE: all string data must remain alive until ui_ymf262_discard()!
*/
typedef struct ui_ymf262_desc_t {
    const char* title;        /* window title */
    ymf262_t* opl3;           /* pointer to ymf262_t instance to track */
    int x, y;                 /* initial window position */
    int w, h;                 /* initial window size (or default size of 0) */
    bool open;                /* initial window open state */
    ui_chip_desc_t chip_desc; /* chip visualization desc */
} ui_ymf262_desc_t;

typedef struct ui_ymf262_t {
    const char* title;
    ymf262_t* opl3;
    float init_x, init_y;
    float init_w, init_h;
    bool open;
    bool last_open;
    bool valid;
    ui_chip_t chip;
} ui_ymf262_t;

void ui_ymf262_init(ui_ymf262_t* win, const ui_ymf262_desc_t* desc);
void ui_ymf262_discard(ui_ymf262_t* win);
void ui_ymf262_draw(ui_ymf262_t* win);
void ui_ymf262_save_settings(ui_ymf262_t* win, ui_settings_t* settings);
void ui_ymf262_load_settings(ui_ymf262_t* win, const ui_settings_t* settings);

#ifdef __cplusplus
} /* extern "C" */
#endif
