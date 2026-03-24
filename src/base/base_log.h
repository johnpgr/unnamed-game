#pragma once

#include <stdarg.h>

#include "base/base_types.h"

enum LogLevel : u8 {
    LOG_LEVEL_FATAL = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_LEVEL_TRACE = 5,
};

void log_write_v(LogLevel level, char const* fmt, va_list args)
    __attribute__((format(printf, 2, 0)));

void log_write(LogLevel level, char const* fmt, ...)
    __attribute__((format(printf, 2, 3)));

#define LOG_FATAL(...) log_write(LOG_LEVEL_FATAL, __VA_ARGS__)
#define LOG_ERROR(...) log_write(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_WARN(...) log_write(LOG_LEVEL_WARN, __VA_ARGS__)
#define LOG_INFO(...) log_write(LOG_LEVEL_INFO, __VA_ARGS__)

#ifndef NDEBUG
#define LOG_DEBUG(...) log_write(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_TRACE(...) log_write(LOG_LEVEL_TRACE, __VA_ARGS__)
#else
#define LOG_DEBUG(...) ((void)0)
#define LOG_TRACE(...) ((void)0)
#endif
