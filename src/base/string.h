#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "base/arena.h"

struct String {
    u8 const *str;
    u64 size;
};

inline String string_substring(String source, u64 start, u64 end) {
    u64 safe_end = (end > source.size) ? source.size : end;
    u64 safe_start = (start > safe_end) ? safe_end : start;
    u8 const *start_ptr =
        (source.str != nullptr) ? (source.str + safe_start) : nullptr;
    String result = {start_ptr, safe_end - safe_start};
    return result;
}

inline bool string_equals(String a, String b) {
    if(a.size != b.size) {
        return false;
    }

    if(a.size == 0) {
        return true;
    }

    return memcmp(a.str, b.str, a.size) == 0;
}

inline String string_lit(char const *s) {
    assert(s != nullptr, "String literal source must not be null!");
    String result = {(u8 const *)s, (u64)strlen(s)};
    return result;
}

inline String string_from_cstr(char const *s) {
    assert(s != nullptr, "String source must not be null!");
    String result = {(u8 const *)s, (u64)strlen(s)};
    return result;
}

inline String string_copy(Arena *arena, String source) {
    String result = {};
    result.size = source.size;
    u64 buffer_size = 0;
    bool size_overflow = add_u64_overflow(source.size, 1ULL, &buffer_size);
    assert(!size_overflow, "String copy size overflow!");
    u8 *buffer = push_array(arena, buffer_size, u8);
    if(source.size > 0) {
        assert(source.str != nullptr, "String source must not be null!");
        memcpy(buffer, source.str, source.size);
    }
    buffer[source.size] = 0;
    result.str = buffer;
    return result;
}

inline char const *string_to_cstr(Arena *arena, String source) {
    return (char const *)string_copy(arena, source).str;
}

inline String string_copy_cstr(Arena *arena, char const *source) {
    return string_copy(arena, string_from_cstr(source));
}

inline String string_fmt(Arena *arena, char const *format, ...)
    __attribute__((format(printf, 2, 3)));

inline String string_fmt(Arena *arena, char const *format, ...) {
    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);
    int size_needed = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);
    assert(size_needed >= 0, "String formatting failed!");

    String result = {};
    result.size = (u64)size_needed;
    u64 buffer_size = 0;
    bool buffer_overflow = add_u64_overflow(result.size, 1ULL, &buffer_size);
    assert(!buffer_overflow, "String formatting size overflow!");
    u8 *buffer = push_array(arena, buffer_size, u8);
    int size_written = vsnprintf((char *)buffer, buffer_size, format, args);
    assert(size_written == size_needed, "String formatting length mismatch!");
    result.str = buffer;
    va_end(args);
    return result;
}

inline String string_concat(Arena *arena, String a, String b) {
    String result = {};
    bool size_overflow = add_u64_overflow(a.size, b.size, &result.size);
    assert(!size_overflow, "String concatenation size overflow!");
    u64 buffer_size = 0;
    bool buffer_overflow = add_u64_overflow(result.size, 1ULL, &buffer_size);
    assert(!buffer_overflow, "String concatenation size overflow!");
    u8 *buffer = push_array(arena, buffer_size, u8);
    if(a.size > 0) {
        assert(a.str != nullptr, "Left string source must not be null!");
        memcpy(buffer, a.str, a.size);
    }
    if(b.size > 0) {
        assert(b.str != nullptr, "Right string source must not be null!");
        memcpy(buffer + a.size, b.str, b.size);
    }
    buffer[result.size] = 0;
    result.str = buffer;
    return result;
}
