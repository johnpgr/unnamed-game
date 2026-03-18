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

static const char* LOG_LEVEL_COLORS[] = {
    "\033[1;41m",
    "\033[1;31m",
    "\033[1;33m",
    "\033[1;32m",
    "\033[1;36m",
    "\033[0;90m",
};

static const char* LOG_LEVEL_TAGS[] = {
    "[FATAL]",
    "[ERROR]",
    "[WARN]",
    "[INFO]",
    "[DEBUG]",
    "[TRACE]",
};

static const char* LOG_COLOR_RESET = "\033[0m";

inline void LogWriteV(
    LogLevel level,
    const char* file,
    int line,
    const char* fmt,
    va_list args
) __attribute__((format(printf, 4, 0)));

inline void LogWrite(
    LogLevel level,
    const char* file,
    int line,
    const char* fmt,
    ...
) __attribute__((format(printf, 4, 5)));

inline void
LogWriteV(LogLevel level, const char* file, int line, const char* fmt, va_list args) {
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
        LOG_LEVEL_COLORS[level_index],
        LOG_LEVEL_TAGS[level_index],
        file,
        line,
        msg,
        LOG_COLOR_RESET
    );

    FILE* stream = level_index <= (usize)LOG_LEVEL_ERROR ? stderr : stdout;
    (void)fputs(out, stream);
}

inline void LogWrite(
    LogLevel level,
    const char* file,
    int line,
    const char* fmt,
    ...
) {
    va_list args;
    va_start(args, fmt);
    LogWriteV(level, file, line, fmt, args);
    va_end(args);
}

#define LOG_FATAL(...) LogWrite(LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) LogWrite(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) LogWrite(LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) LogWrite(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)

#ifndef NDEBUG
#define LOG_DEBUG(...) LogWrite(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_TRACE(...) LogWrite(LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG_DEBUG(...) ((void)0)
#define LOG_TRACE(...) ((void)0)
#endif
