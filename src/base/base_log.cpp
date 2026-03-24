#include <stdio.h>

#include "base/base_log.h"

global_variable char const* log_level_colors[] = {
    "\033[1;41m",
    "\033[1;31m",
    "\033[1;33m",
    "\033[1;32m",
    "\033[1;36m",
    "\033[0;90m",
};

global_variable char const* log_level_tags[] = {
    "[FATAL]",
    "[ERROR]",
    "[WARN]",
    "[INFO]",
    "[DEBUG]",
    "[TRACE]",
};

global_variable char const* log_color_reset = "\033[0m";

void log_write_v(LogLevel level, char const* fmt, va_list args) {
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

    FILE* stream = level_index <= (usize)LOG_LEVEL_ERROR ? stderr : stdout;
    (void)fputs(out, stream);
}

void log_write(LogLevel level, char const* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write_v(level, fmt, args);
    va_end(args);
}
