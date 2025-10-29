#pragma once

#include "sokol_log.h"

#ifdef __cplusplus
extern "C" {
#endif
void log_func(uint32_t log_level, const char* log_id, const char* filename, uint32_t line_nr, const char* fmt, ...)
    __attribute__((format(printf, 5, 6)));
#ifdef __cplusplus
} /* extern "C" */
#endif

#define LOG_PANIC(message, ...)   log_func(0, __func__, __FILE__, __LINE__, message, ##__VA_ARGS__);
#define LOG_ERROR(message, ...)   log_func(1, __func__, __FILE__, __LINE__, message, ##__VA_ARGS__);
#define LOG_WARNING(message, ...) log_func(2, __func__, __FILE__, __LINE__, message, ##__VA_ARGS__);
#define LOG_INFO(message, ...)    log_func(3, __func__, __FILE__, __LINE__, message, ##__VA_ARGS__);
