#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "base/base_string.h"

String string_substring(String source, u64 start, u64 end) {
    u64 safe_end = (end > source.size) ? source.size : end;
    u64 safe_start = (start > safe_end) ? safe_end : start;
    u8 const* start_ptr =
        (source.str != nullptr) ? (source.str + safe_start) : nullptr;
    String result = {start_ptr, safe_end - safe_start};
    return result;
}

bool string_equals(String a, String b) {
    if(a.size != b.size) {
        return false;
    }

    if(a.size == 0) {
        return true;
    }

    return memcmp(a.str, b.str, a.size) == 0;
}

String string_lit(char const* s) {
    ASSERT(s != nullptr, "String literal source must not be null!");
    String result = {(u8 const*)s, (u64)strlen(s)};
    return result;
}

String string_from_cstr(char const* s) {
    ASSERT(s != nullptr, "String source must not be null!");
    String result = {(u8 const*)s, (u64)strlen(s)};
    return result;
}

String string_copy(Arena* arena, String source) {
    String result = {};
    result.size = source.size;
    u64 buffer_size = 0;
    bool size_overflow = add_u64_overflow(source.size, 1ULL, &buffer_size);
    ASSERT(!size_overflow, "String copy size overflow!");
    u8* buffer = push_array(arena, u8, buffer_size);
    if(source.size > 0) {
        ASSERT(source.str != nullptr, "String source must not be null!");
        memcpy(buffer, source.str, source.size);
    }
    buffer[source.size] = 0;
    result.str = buffer;
    return result;
}

char const* string_to_cstr(Arena* arena, String source) {
    return (char const*)string_copy(arena, source).str;
}

String string_copy_cstr(Arena* arena, char const* source) {
    return string_copy(arena, string_from_cstr(source));
}

String string_fmt(Arena* arena, char const* format, ...) {
    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);
    int size_needed = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);
    ASSERT(size_needed >= 0, "String formatting failed!");

    String result = {};
    result.size = (u64)size_needed;
    u64 buffer_size = 0;
    bool buffer_overflow = add_u64_overflow(result.size, 1ULL, &buffer_size);
    ASSERT(!buffer_overflow, "String formatting size overflow!");
    u8* buffer = push_array(arena, u8, buffer_size);
    int size_written = vsnprintf((char*)buffer, buffer_size, format, args);
    ASSERT(size_written == size_needed, "String formatting length mismatch!");
    result.str = buffer;
    va_end(args);
    return result;
}

String string_concat(Arena* arena, String a, String b) {
    String result = {};
    bool size_overflow = add_u64_overflow(a.size, b.size, &result.size);
    ASSERT(!size_overflow, "String concatenation size overflow!");
    u64 buffer_size = 0;
    bool buffer_overflow = add_u64_overflow(result.size, 1ULL, &buffer_size);
    ASSERT(!buffer_overflow, "String concatenation size overflow!");
    u8* buffer = push_array(arena, u8, buffer_size);
    if(a.size > 0) {
        ASSERT(a.str != nullptr, "Left string source must not be null!");
        memcpy(buffer, a.str, a.size);
    }
    if(b.size > 0) {
        ASSERT(b.str != nullptr, "Right string source must not be null!");
        memcpy(buffer + a.size, b.str, b.size);
    }
    buffer[result.size] = 0;
    result.str = buffer;
    return result;
}
