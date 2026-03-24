#pragma once

#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef size_t usize;
typedef ptrdiff_t isize;
typedef float f32;
typedef double f64;

#define internal static
#define local_persist static
#define global_variable static

union vec2 {
    struct {
        f32 x;
        f32 y;
    };
    f32 e[2];
};

union vec4 {
    struct {
        f32 x;
        f32 y;
        f32 z;
        f32 w;
    };
    struct {
        f32 r;
        f32 g;
        f32 b;
        f32 a;
    };
    f32 e[4];
};

#define vec2(x, y) (vec2{{(x), (y)}})
#define vec4(x, y, z, w) (vec4{{(x), (y), (z), (w)}})

inline vec2 operator+(vec2 a, vec2 b) {
    return vec2(a.x + b.x, a.y + b.y);
}

inline vec2 operator-(vec2 a, vec2 b) {
    return vec2(a.x - b.x, a.y - b.y);
}

inline vec2 operator*(f32 scale, vec2 a) {
    return vec2(scale * a.x, scale * a.y);
}

inline vec2 operator*(vec2 a, f32 scale) {
    return scale * a;
}

inline vec2& operator+=(vec2& a, vec2 b) {
    a = a + b;
    return a;
}
