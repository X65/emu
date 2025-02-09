#if defined(__GNUC__)
#define SOKOL_UNREACHABLE __builtin_unreachable()
#elif defined(_MSC_VER)
#define SOKOL_UNREACHABLE __assume(0)
#endif
