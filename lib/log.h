#pragma once

/* ============================================================
 * Log levels
 * ============================================================ */
typedef enum log_level {
    log_level_fatal = 0,
    log_level_error = 1,
    log_level_warn = 2,
    log_level_info = 3,
    log_level_debug = 4,
    log_level_trace = 5,
} log_level;

/* ============================================================
 * Core API
 * ============================================================ */
// clang-format off
static const char* log_level_colors_[] = {
    "\033[1;41m", /* FATAL */
    "\033[1;31m", /* ERROR */
    "\033[1;33m", /* WARN  */
    "\033[1;32m", /* INFO  */
    "\033[1;36m", /* DEBUG */
    "\033[0;90m", /* TRACE */
};

static const char* log_level_tags_[] = {
    "[FATAL]",
    "[ERROR]",
    "[WARN]",
    "[INFO]",
    "[DEBUG]",
    "[TRACE]",
};
// clang-format on

static const char* log_color_reset_ = "\033[0m";

static inline void log_output(
    log_level level,
    const char* file,
    int line,
    const char* fmt,
    ...
) __attribute__((format(printf, 4, 5)));

static inline void log_output(
    log_level level,
    const char* file,
    int line,
    const char* fmt,
    ...
) {
    char msg[16384];
    va_list args;
    va_start(args, fmt);
    (void)vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    char out[16384];
    (void)snprintf(
        out,
        sizeof(out),
        "%s%s %s:%d: %s%s\n",
        log_level_colors_[level],
        log_level_tags_[level],
        file,
        line,
        msg,
        log_color_reset_
    );

    FILE* stream = (level <= log_level_error) ? stderr : stdout;
    (void)fputs(out, stream);
}

/* ============================================================
 * Convenience macros — auto-fill __FILE__ and __LINE__
 * ============================================================ */
#define log_fatal(fmt, ...)                                                    \
    log_output(log_level_fatal, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

#define log_error(fmt, ...)                                                    \
    log_output(log_level_error, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

#define log_warn(fmt, ...)                                                     \
    log_output(log_level_warn, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

#define log_info(fmt, ...)                                                     \
    log_output(log_level_info, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

/* debug and trace disabled in release builds (NDEBUG defined) */
#ifndef NDEBUG
#define log_debug(fmt, ...)                                                    \
    log_output(log_level_debug, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
#define log_trace(fmt, ...)                                                    \
    log_output(log_level_trace, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
#else
#define log_debug(fmt, ...) ((void)0)
#define log_trace(fmt, ...) ((void)0)
#endif
