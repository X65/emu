#pragma once

#include <sokol/sokol_app.h>

void hid_init(void);
void hid_shutdown(void);
void hid_reset(void);

void sdl_poll_events();

void hid_key_down(sapp_keycode key_code);
void hid_key_up(sapp_keycode key_code);
