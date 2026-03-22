#pragma once

#include "base/typedef.h"

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

inline vec2 &operator+=(vec2 &a, vec2 b) {
    a = a + b;
    return a;
}
