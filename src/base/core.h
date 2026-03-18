#pragma once

#include <cstdlib>

#include "base/log.h"

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

#if OS_WINDOWS
#define export extern "C" __declspec(dllexport)
#else
#define export extern "C"
#endif

#define BIT(x) (1ULL << (x))
#define KB 1024ULL
#define MB (KB * KB)
#define GB (MB * KB)
#define TB (GB * KB)

#define IS_TRIVIAL_TYPE(T) (__is_trivial(T) && __is_trivially_copyable(T))

#if COMPILER_MSVC
#define assume(expr) __assume(expr)
#elif COMPILER_CLANG || COMPILER_GCC
#define assume(expr)                                                                               \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            __builtin_unreachable();                                                               \
        }                                                                                          \
    } while (0)
#else
#define assume(expr) ((void)0)
#endif

template <typename T> struct RemoveReference {
    typedef T Type;
};

template <typename T> struct RemoveReference<T&> {
    typedef T Type;
};

template <typename T> struct RemoveReference<T&&> {
    typedef T Type;
};

template <typename T> constexpr T&& Forward(typename RemoveReference<T>::Type& value) {
    return static_cast<T&&>(value);
}

template <typename T> constexpr T&& Forward(typename RemoveReference<T>::Type&& value) {
    return static_cast<T&&>(value);
}

template <typename F> struct Defer {
    explicit Defer(F&& f) : f(Forward<F>(f)) {
    }
    ~Defer() {
        f();
    }
    F f;
};

template <typename F> Defer<typename RemoveReference<F>::Type> MakeDefer(F&& f) {
    return Defer<typename RemoveReference<F>::Type>(Forward<F>(f));
};

struct DeferDummy {};

template <typename F> Defer<typename RemoveReference<F>::Type> operator+(DeferDummy, F&& f) {
    return MakeDefer<F>(Forward<F>(f));
}

inline bool IsPow2(u64 value) {
    return value != 0 && (value & (value - 1)) == 0;
}

inline bool AddU64Overflow(u64 a, u64 b, u64* out) {
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

inline bool MulU64Overflow(u64 a, u64 b, u64* out) {
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

inline bool AlignUpPow2U64(u64 value, u64 alignment, u64* out) {
    if (!IsPow2(alignment)) {
        *out = 0;
        return true;
    }

    u64 sum = 0;
    if (AddU64Overflow(value, alignment - 1, &sum)) {
        *out = 0;
        return true;
    }

    *out = sum & ~(alignment - 1);
    return false;
}

#ifndef NDEBUG
#if COMPILER_MSVC
#define assert(expr, msg)                                                                          \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            LOG_FATAL("assertion failed: %s - %s", #expr, msg);                                    \
            __debugbreak();                                                                        \
        }                                                                                          \
    } while (0)
#elif COMPILER_CLANG
#define assert(expr, msg)                                                                          \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            LOG_FATAL("assertion failed: %s - %s", #expr, msg);                                    \
            __builtin_debugtrap();                                                                 \
        }                                                                                          \
    } while (0)
#elif COMPILER_GCC
#define assert(expr, msg)                                                                          \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            LOG_FATAL("assertion failed: %s - %s", #expr, msg);                                    \
            __builtin_trap();                                                                      \
        }                                                                                          \
    } while (0)
#else
#define assert(expr, msg)                                                                          \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            LOG_FATAL("assertion failed: %s - %s", #expr, msg);                                    \
            abort();                                                                               \
        }                                                                                          \
    } while (0)
#endif
#else
#define assert(expr, msg) ((void)sizeof((expr) ? true : false))
#endif

#define CG_DEFER_NAME_IMPL(line) defer_##line
#define CG_DEFER_NAME(line) CG_DEFER_NAME_IMPL(line)
#define defer auto CG_DEFER_NAME(__LINE__) = DeferDummy() + [&]()
