#pragma once
#include <stdint.h>
#include <stdbool.h>

#include "ui/ui_settings.h"
#include "util/ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* setup parameters for ui_console_init()
    NOTE: all string data must remain alive until ui_console_discard()!
*/
typedef struct {
    const char* title; /* window title */
    ring_buffer_t* rx; /* incoming characters */
    ring_buffer_t* tx; /* incoming characters */
    int x, y;          /* initial window position */
    int w, h;          /* initial window width and height */
    bool open;         /* initial open state */
} ui_console_desc_t;

typedef struct {
    const char* title;
    ring_buffer_t* rx;
    ring_buffer_t* tx;
    float init_x, init_y;
    float init_w, init_h;
    bool open;
    bool last_open;
    bool valid;
} ui_console_t;

void ui_console_init(ui_console_t* win, const ui_console_desc_t* desc);
void ui_console_discard(ui_console_t* win);
void ui_console_draw(ui_console_t* win);
void ui_console_process_tx(ui_console_t* win);
void ui_console_save_settings(ui_console_t* win, ui_settings_t* settings);
void ui_console_load_settings(ui_console_t* win, const ui_settings_t* settings);

#ifdef __cplusplus
} /* extern "C" */
#endif
