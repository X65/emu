#pragma once
#include <stdint.h>
#include <stdbool.h>

#include "ui/ui_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/* setup parameters for ui_app_log_init()
    NOTE: all string data must remain alive until ui_app_log_discard()!
*/
typedef struct {
    const char* title; /* window title */
    int x, y;          /* initial window position */
    int w, h;          /* initial window width and height */
    bool open;         /* initial open state */
} ui_app_log_desc_t;

typedef struct {
    const char* title;
    float init_x, init_y;
    float init_w, init_h;
    bool open;
    bool last_open;
    bool valid;
} ui_app_log_t;

void ui_app_log_init(ui_app_log_t* win, const ui_app_log_desc_t* desc);
void ui_app_log_discard(ui_app_log_t* win);
void ui_app_log_draw(ui_app_log_t* win);
void ui_app_log_save_settings(ui_app_log_t* win, ui_settings_t* settings);
void ui_app_log_load_settings(ui_app_log_t* win, const ui_settings_t* settings);

#ifdef __cplusplus
} /* extern "C" */
#endif
