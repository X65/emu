#pragma once
// dummy file to allow compiling parts of firmware

#define __in_flash(s)
#define __not_in_flash(s)
#define __not_in_flash_func(f) f

#if PICO_C_COMPILER_IS_GNU && (__GNUC__ <= 6 || (__GNUC__ == 7 && (__GNUC_MINOR__ < 3 || !defined(__cplusplus))))
    #define __force_inline inline __always_inline
#else
    #define __force_inline __always_inline
#endif

#define _STRINGIFY(x) #x
#define STRINGIFY(x)  _STRINGIFY(x)

// glibc defines this; mingw-w64 and emscripten's musl do not
#ifndef __always_inline
    #define __always_inline __attribute__((always_inline))
#endif

// the SDK's spin-loop hint; nothing to hint at on a host
static inline void tight_loop_contents(void) {}
