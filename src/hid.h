#pragma once

#include <sokol/sokol_app.h>

void hid_init(void);
void hid_shutdown(void);
void hid_reset(void);

void sdl_poll_events();

void hid_key_down(sapp_keycode key_code);
void hid_key_up(sapp_keycode key_code);

// Keyboard injection, for headless scripting and tests.  Real key state only
// ever arrives from the host window, so without this a script cannot press a
// key.  The injected keys are OR-ed into whatever a real keyboard reports, as
// a second keyboard would be.  `usage` is a USB HID usage id.
void hid_key_inject(uint8_t usage);
void hid_keys_release(void);
