/*#
    # ui_tca6416a.h

    Debug visualization UI for tca6416a.h

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
        - tca6416a.h
        - ui_chip.h
        - ui_settings.h

    Include the following headers before including the *implementation*:
        - imgui.h
        - tca6416a.h
        - ui_chip.h
        - ui_util.h

    All strings provided to ui_tca6416a_init() must remain alive until
    ui_tca6416a_discard() is called!

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
#include "chips/tca6416a.h"
#include "ui/ui_chip.h"
#include "ui/ui_settings.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* setup parameters for ui_tca6416a_init()
    NOTE: all string data must remain alive until ui_tca6416a_discard()!
*/
typedef struct ui_tca6416a_desc_t {
    const char* title;        /* window title */
    tca6416a_t* gpio;         /* tca6416a_t instance to track */
    int x, y;                 /* initial window position */
    int w, h;                 /* initial window size (or default size if 0) */
    bool open;                /* initial open state */
    ui_chip_desc_t chip_desc; /* chip visualization desc */
} ui_tca6416a_desc_t;

typedef struct ui_tca6416a_t {
    const char* title;
    tca6416a_t* gpio;
    float init_x, init_y;
    float init_w, init_h;
    bool open;
    bool last_open;
    bool valid;
    ui_chip_t chip;
} ui_tca6416a_t;

void ui_tca6416a_init(ui_tca6416a_t* win, const ui_tca6416a_desc_t* desc);
void ui_tca6416a_discard(ui_tca6416a_t* win);
void ui_tca6416a_draw(ui_tca6416a_t* win);
void ui_tca6416a_save_settings(ui_tca6416a_t* win, ui_settings_t* settings);
void ui_tca6416a_load_settings(ui_tca6416a_t* win, const ui_settings_t* settings);

#ifdef __cplusplus
} /* extern "C" */
#endif
