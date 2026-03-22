#pragma once

#include <cstdarg>
#include <cstdio>

#include "base/typedef.h"

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

inline void log_write_v(
    LogLevel level,
    char const *file,
    int line,
    char const *fmt,
    va_list args
) __attribute__((format(printf, 4, 0)));

inline void log_write(
    LogLevel level,
    char const *file,
    int line,
    char const *fmt,
    ...
) __attribute__((format(printf, 4, 5)));

inline void log_write_v(
    LogLevel level,
    char const *file,
    int line,
    char const *fmt,
    va_list args
) {
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
        "%s%s %s:%d: %s%s\n",
        log_level_colors[level_index],
        log_level_tags[level_index],
        file,
        line,
        msg,
        log_color_reset
    );

    FILE *stream = level_index <= (usize)LOG_LEVEL_ERROR ? stderr : stdout;
    (void)fputs(out, stream);
}

inline void log_write(
    LogLevel level,
    char const *file,
    int line,
    char const *fmt,
    ...
) {
    va_list args;
    va_start(args, fmt);
    log_write_v(level, file, line, fmt, args);
    va_end(args);
}

#define LOG_FATAL(...)                                                         \
    log_write(LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)                                                         \
    log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) log_write(LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)

#ifndef NDEBUG
#define LOG_DEBUG(...)                                                         \
    log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_TRACE(...)                                                         \
    log_write(LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG_DEBUG(...) ((void)0)
#define LOG_TRACE(...) ((void)0)
#endif
