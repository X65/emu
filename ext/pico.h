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
