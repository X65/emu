typedef unsigned int uint;

// mingw-w64 hides its localtime_r/gmtime_r inlines behind this; the firmware's
// api/clk.c uses them.
#ifndef _POSIX_THREAD_SAFE_FUNCTIONS
    #define _POSIX_THREAD_SAFE_FUNCTIONS 200809L
#endif

// mingw-w64 has no setenv; the firmware's api/clk.c calls it. Implemented in
// src/systems/x65_fw.c on top of _putenv.
int setenv(const char* name, const char* value, int overwrite);
