#pragma once

#include "sokol_log.h"

#ifdef __cplusplus
extern "C" {
#endif
void log_func(uint32_t log_level, uint32_t log_id, uint32_t line_nr, const char* filename, const char* fmt, ...)
    __attribute__((format(printf, 5, 6)));
#ifdef __cplusplus
} /* extern "C" */
#endif

#define LOG_PANIC(id, message, ...)   log_func(0, id, __LINE__, __FILE__, message, ##__VA_ARGS__);
#define LOG_ERROR(id, message, ...)   log_func(1, id, __LINE__, __FILE__, message, ##__VA_ARGS__);
#define LOG_WARNING(id, message, ...) log_func(2, id, __LINE__, __FILE__, message, ##__VA_ARGS__);
#define LOG_INFO(id, message, ...)    log_func(3, id, __LINE__, __FILE__, message, ##__VA_ARGS__);
