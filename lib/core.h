#pragma once

#include <stdlib.h>

#if defined(__clang__)
#define COMPILER_CLANG 1
#else
#define COMPILER_CLANG 0
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define COMPILER_GCC 1
#else
#define COMPILER_GCC 0
#endif

#if defined(_MSC_VER)
#define COMPILER_MSVC 1
#include <intrin.h>
#else
#define COMPILER_MSVC 0
#endif

#if defined(_WIN32) || defined(_WIN64)
#define OS_WINDOWS 1
#else
#define OS_WINDOWS 0
#endif

#if defined(__APPLE__) && defined(__MACH__)
#define OS_MAC 1
#else
#define OS_MAC 0
#endif

#if defined(__linux__)
#define OS_LINUX 1
#else
#define OS_LINUX 0
#endif

#define IS_TRIVIAL_TYPE(T) (__is_trivial(T) && __is_trivially_copyable(T))

inline void debug_break() {
#if COMPILER_MSVC
    __debugbreak();
#elif COMPILER_CLANG
    __builtin_debugtrap();
#elif COMPILER_GCC
    __builtin_trap();
#else
    abort();
#endif
}

#ifndef NDEBUG

#define assert(expr)                                                           \
    do {                                                                       \
        if (!(expr)) {                                                         \
            log_fatal("assertion failed: %s", #expr);                          \
            debug_break();                                                     \
        }                                                                      \
    } while (0)

#define assert_msg(expr, msg)                                                  \
    do {                                                                       \
        if (!(expr)) {                                                         \
            log_fatal("assertion failed: %s — %s", #expr, msg);                \
            debug_break();                                                     \
        }                                                                      \
    } while (0)

#else

#define assert(expr) ((void)sizeof((expr) ? true : false))
#define assert_msg(expr, msg) ((void)sizeof((expr) ? true : false))

#endif

inline bool is_pow2(u64 value) {
    return value != 0 && (value & (value - 1)) == 0;
}

inline bool u64_add_overflow(u64 a, u64 b, u64* out) {
#if COMPILER_CLANG || COMPILER_GCC
    return __builtin_add_overflow(a, b, out);
#else
    u64 max_u64 = ~(u64)0;
    if (a > max_u64 - b) {
        *out = 0;
        return true;
    }
    *out = a + b;
    return false;
#endif
}

inline bool u64_mul_overflow(u64 a, u64 b, u64* out) {
#if COMPILER_CLANG || COMPILER_GCC
    return __builtin_mul_overflow(a, b, out);
#else
    u64 max_u64 = ~(u64)0;
    if (a != 0 && b > max_u64 / a) {
        *out = 0;
        return true;
    }
    *out = a * b;
    return false;
#endif
}

inline bool u64_align_up_pow2(u64 value, u64 alignment, u64* out) {
    if (!is_pow2(alignment)) {
        *out = 0;
        return true;
    }

    u64 sum = 0;
    if (u64_add_overflow(value, alignment - 1, &sum)) {
        *out = 0;
        return true;
    }

    *out = sum & ~(alignment - 1);
    return false;
}
