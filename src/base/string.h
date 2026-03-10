#pragma once

struct String {
    const u8* str;
    u64 size;

    // --- Static Constructors ---

    // Create from a literal: String s = String::lit("Hello");
    static String lit(const char* s) {
        assert(s != nullptr, "String literal source must not be null!");
        String result = {(const u8*)s, (u64)strlen(s)};
        return result;
    }

    // Create from a null-terminated C string without taking ownership.
    static String fromCStr(const char* s) {
        assert(s != nullptr, "String source must not be null!");
        String result = {(const u8*)s, (u64)strlen(s)};
        return result;
    }

    // Create a copy in an arena: String s = String::copy(arena, other_string);
    static String copy(Arena* arena, String source) {
        String result = {};
        result.size = source.size;
        u64 buffer_size = 0;
        bool size_overflow =
            u64AddOverflow(source.size, 1ULL, &buffer_size);
        assert(!size_overflow, "String copy size overflow!");
        u8* buffer = arena->push<u8>(buffer_size);
        if (source.size > 0) {
            assert(source.str != nullptr, "String source must not be null!");
            memcpy(buffer, source.str, source.size);
        }
        buffer[source.size] = 0;
        result.str = buffer;
        return result;
    }

    static String copyCStr(Arena* arena, const char* source) {
        return copy(arena, fromCStr(source));
    }

    // Format into an arena: String s = String::format(arena, "Score: %d", 10);
    static String fmt(Arena* arena, const char* format, ...)
        __attribute__((format(printf, 2, 3))) {
        va_list args;
        va_start(args, format);

        // 1. Determine size
        va_list args_copy;
        va_copy(args_copy, args);
        int size_needed = vsnprintf(nullptr, 0, format, args_copy);
        va_end(args_copy);
        assert(size_needed >= 0, "String formatting failed!");

        // 2. Allocate and Format
        String result = {};
        result.size = (u64)size_needed;
        u64 buffer_size = 0;
        bool buffer_overflow =
            u64AddOverflow(result.size, 1ULL, &buffer_size);
        assert(!buffer_overflow, "String formatting size overflow!");
        u8* buffer = arena->push<u8>(buffer_size);
        int size_written = vsnprintf((char*)buffer, buffer_size, format, args);
        assert(size_written == size_needed, "String formatting length mismatch!");
        result.str = buffer;
        va_end(args);
        return result;
    }

    // --- Methods ---

    // Slice: String sub = my_str.substring(0, 5);
    String substring(u64 start, u64 end) const {
        u64 safe_end = (end > size) ? size : end;
        u64 safe_start = (start > safe_end) ? safe_end : start;
        const u8* start_ptr = (str != nullptr) ? (str + safe_start) : nullptr;
        String result = {start_ptr, safe_end - safe_start};
        return result;
    }

    // Check equality
    bool equals(String other) const {
        if (size != other.size) return false;
        if (size == 0) return true;
        return memcmp(str, other.str, size) == 0;
    }

    // Interop with C APIs: fopen(my_str.toCStr(arena), "rb");
    const char* toCStr(Arena* arena) const {
        return (const char*)String::copy(arena, *this).str;
    }
};

// --- Concatenation Helper ---
inline String strConcat(Arena* arena, String a, String b) {
    String result = {};
    bool size_overflow = u64AddOverflow(a.size, b.size, &result.size);
    assert(!size_overflow, "String concatenation size overflow!");
    u64 buffer_size = 0;
    bool buffer_overflow =
        u64AddOverflow(result.size, 1ULL, &buffer_size);
    assert(!buffer_overflow, "String concatenation size overflow!");
    u8* buffer = arena->push<u8>(buffer_size);
    if (a.size > 0) {
        assert(a.str != nullptr, "Left string source must not be null!");
        memcpy(buffer, a.str, a.size);
    }
    if (b.size > 0) {
        assert(b.str != nullptr, "Right string source must not be null!");
        memcpy(buffer + a.size, b.str, b.size);
    }
    buffer[result.size] = 0;
    result.str = buffer;
    return result;
}
