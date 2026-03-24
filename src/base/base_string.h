#pragma once

#include "base/base_arena.h"

struct String {
    u8 const* str;
    u64 size;
};

String string_substring(String source, u64 start, u64 end);
bool string_equals(String a, String b);
String string_lit(char const* s);
String string_from_cstr(char const* s);
String string_copy(Arena* arena, String source);
char const* string_to_cstr(Arena* arena, String source);
String string_copy_cstr(Arena* arena, char const* source);
String string_fmt(Arena* arena, char const* format, ...)
    __attribute__((format(printf, 2, 3)));
String string_concat(Arena* arena, String a, String b);
