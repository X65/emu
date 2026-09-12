#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Hold the system screen-saver / idle blanking off while `inhibit` is true.
// Idempotent and cheap, so the frame loop can hand it the current fullscreen
// state every frame and let it notice the transitions.
void screensaver_inhibit(bool inhibit);

// Drop any active inhibition and release whatever the backend is holding.
void screensaver_shutdown(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
