#pragma once
/*
    cursor.h -- the X65 mouse pointer.
*/
#if defined(__cplusplus)
extern "C" {
#endif

// Bind the X65 pointer as the application's arrow cursor. Call once from the
// sokol-app init callback, after the window exists.
void cursor_init(void);

// Give the pointer back. Call from the sokol-app cleanup callback -- see the
// note in cursor.c, this is not optional.
void cursor_shutdown(void);

#if defined(__cplusplus)
}  // extern "C"
#endif
