#pragma once

#include <cstdarg>
#include <cstdio>

#include "base/types.h"

enum LogLevel : u8 {
    LOG_LEVEL_FATAL = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_LEVEL_TRACE = 5,
};

global_variable char const *log_level_colors[] = {
    "\033[1;41m",
    "\033[1;31m",
    "\033[1;33m",
    "\033[1;32m",
    "\033[1;36m",
    "\033[0;90m",
};

global_variable char const *log_level_tags[] = {
    "[FATAL]",
    "[ERROR]",
    "[WARN]",
    "[INFO]",
    "[DEBUG]",
    "[TRACE]",
};

global_variable char const *log_color_reset = "\033[0m";

inline void log_write_v(LogLevel level, char const *fmt, va_list args)
    __attribute__((format(printf, 2, 0)));

inline void log_write(LogLevel level, char const *fmt, ...)
    __attribute__((format(printf, 2, 3)));

inline void log_write_v(LogLevel level, char const *fmt, va_list args) {
    char msg[16384];
    va_list args_copy;
    va_copy(args_copy, args);
    (void)vsnprintf(msg, sizeof(msg), fmt, args_copy);
    va_end(args_copy);

    char out[16384];
    usize level_index = (usize)level;
    (void)snprintf(
        out,
        sizeof(out),
        "%s%s %s%s\n",
        log_level_colors[level_index],
        log_level_tags[level_index],
        msg,
        log_color_reset
    );

    FILE *stream = level_index <= (usize)LOG_LEVEL_ERROR ? stderr : stdout;
    (void)fputs(out, stream);
}

inline void log_write(LogLevel level, char const *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write_v(level, fmt, args);
    va_end(args);
}

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
