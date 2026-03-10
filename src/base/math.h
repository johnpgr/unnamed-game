#pragma once

struct IVec2;
struct Mat2x2;
struct Mat3x3;
struct Mat4x4;
struct Quat;

inline f32 lerp(f32 a, f32 t, f32 b) {
    return (1.0f - t) * a + t * b;
}

inline f32 clamp(f32 min_value, f32 value, f32 max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

inline f32 radians(f32 degrees) {
    return degrees * 0.017453292519943295f;
}

inline f32 degrees(f32 radians_value) {
    return radians_value * 57.29577951308232f;
}

struct Vec2 {
    union {
        f32 values[2];
        struct {
            f32 x, y;
        };
        struct {
            f32 u, v;
        };
    };

    Vec2() : x(0), y(0) {
    }
    Vec2(f32 value) : x(value), y(value) {
    }
    Vec2(f32 x, f32 y) : x(x), y(y) {
    }
    Vec2(const IVec2& v);

    static Vec2 zero() {
        return Vec2(0.0f);
    }

    f32& operator[](usize i) {
        return values[i];
    }
    const f32& operator[](usize i) const {
        return values[i];
    }

    Vec2 operator+(const Vec2& v) const {
        return Vec2(x + v.x, y + v.y);
    }
    Vec2 operator-(const Vec2& v) const {
        return Vec2(x - v.x, y - v.y);
    }
    Vec2 operator*(const Vec2& v) const {
        return Vec2(x * v.x, y * v.y);
    }
    Vec2 operator*(f32 s) const {
        return Vec2(x * s, y * s);
    }
    Vec2 operator/(const Vec2& v) const {
        return Vec2(x / v.x, y / v.y);
    }
    Vec2 operator/(f32 s) const {
        return Vec2(x / s, y / s);
    }
    Vec2 operator-() const {
        return Vec2(-x, -y);
    }

    Vec2& operator+=(const Vec2& v) {
        x += v.x;
        y += v.y;
        return *this;
    }
    Vec2& operator-=(const Vec2& v) {
        x -= v.x;
        y -= v.y;
        return *this;
    }
    Vec2& operator*=(const Vec2& v) {
        x *= v.x;
        y *= v.y;
        return *this;
    }
    Vec2& operator*=(f32 s) {
        x *= s;
        y *= s;
        return *this;
    }
    Vec2& operator/=(const Vec2& v) {
        x /= v.x;
        y /= v.y;
        return *this;
    }
    Vec2& operator/=(f32 s) {
        x /= s;
        y /= s;
        return *this;
    }

    bool operator==(const Vec2& v) const {
        return x == v.x && y == v.y;
    }
    bool operator!=(const Vec2& v) const {
        return !(*this == v);
    }

    f32 lengthSquared() const {
        return dot(*this);
    }
    f32 length() const {
        return sqrtf(lengthSquared());
    }
    f32 dot(const Vec2& v) const {
        return x * v.x + y * v.y;
    }
    Vec2 normalized() const {
        f32 len = length();
        return len > 0.0f ? *this / len : Vec2(0.0f);
    }
    void normalize() {
        *this = normalized();
    }
    Vec2 lerped(f32 t, const Vec2& b) const {
        return (*this) * (1.0f - t) + b * t;
    }
};

struct IVec2 {
    union {
        i32 values[2];
        struct {
            i32 x, y;
        };
    };

    IVec2() : x(0), y(0) {
    }
    IVec2(i32 value) : x(value), y(value) {
    }
    IVec2(i32 x, i32 y) : x(x), y(y) {
    }
    IVec2(const Vec2& v) : x((i32)v.x), y((i32)v.y) {
    }

    i32& operator[](usize i) {
        return values[i];
    }
    const i32& operator[](usize i) const {
        return values[i];
    }

    IVec2 operator+(const IVec2& v) const {
        return IVec2(x + v.x, y + v.y);
    }
    IVec2 operator-(const IVec2& v) const {
        return IVec2(x - v.x, y - v.y);
    }
    IVec2 operator*(const IVec2& v) const {
        return IVec2(x * v.x, y * v.y);
    }
    IVec2 operator*(i32 s) const {
        return IVec2(x * s, y * s);
    }
    IVec2 operator/(const IVec2& v) const {
        return IVec2(x / v.x, y / v.y);
    }
    IVec2 operator/(i32 s) const {
        return IVec2(x / s, y / s);
    }
    IVec2 operator-() const {
        return IVec2(-x, -y);
    }

    IVec2& operator+=(const IVec2& v) {
        x += v.x;
        y += v.y;
        return *this;
    }
    IVec2& operator-=(const IVec2& v) {
        x -= v.x;
        y -= v.y;
        return *this;
    }
    IVec2& operator*=(const IVec2& v) {
        x *= v.x;
        y *= v.y;
        return *this;
    }
    IVec2& operator*=(i32 s) {
        x *= s;
        y *= s;
        return *this;
    }
    IVec2& operator/=(const IVec2& v) {
        x /= v.x;
        y /= v.y;
        return *this;
    }
    IVec2& operator/=(i32 s) {
        x /= s;
        y /= s;
        return *this;
    }

    bool operator==(const IVec2& v) const {
        return x == v.x && y == v.y;
    }
    bool operator!=(const IVec2& v) const {
        return !(*this == v);
    }

    i32 lengthSquared() const {
        return dot(*this);
    }
    f32 length() const {
        return sqrtf((f32)lengthSquared());
    }
    i32 dot(const IVec2& v) const {
        return x * v.x + y * v.y;
    }
};

inline Vec2::Vec2(const IVec2& v) : x((f32)v.x), y((f32)v.y) {
}

struct Vec3 {
    union {
        f32 values[3];
        struct {
            f32 x, y, z;
        };
        struct {
            f32 r, g, b;
        };
    };

    Vec3() : x(0), y(0), z(0) {
    }
    Vec3(f32 value) : x(value), y(value), z(value) {
    }
    Vec3(f32 x, f32 y, f32 z) : x(x), y(y), z(z) {
    }
    Vec3(const Vec2& xy, f32 z) : x(xy.x), y(xy.y), z(z) {
    }

    static Vec3 zero() {
        return Vec3(0.0f);
    }

    f32& operator[](usize i) {
        return values[i];
    }
    const f32& operator[](usize i) const {
        return values[i];
    }

    Vec3 operator+(const Vec3& v) const {
        return Vec3(x + v.x, y + v.y, z + v.z);
    }
    Vec3 operator-(const Vec3& v) const {
        return Vec3(x - v.x, y - v.y, z - v.z);
    }
    Vec3 operator*(const Vec3& v) const {
        return Vec3(x * v.x, y * v.y, z * v.z);
    }
    Vec3 operator*(f32 s) const {
        return Vec3(x * s, y * s, z * s);
    }
    Vec3 operator/(const Vec3& v) const {
        return Vec3(x / v.x, y / v.y, z / v.z);
    }
    Vec3 operator/(f32 s) const {
        return Vec3(x / s, y / s, z / s);
    }
    Vec3 operator-() const {
        return Vec3(-x, -y, -z);
    }

    Vec3& operator+=(const Vec3& v) {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }
    Vec3& operator-=(const Vec3& v) {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }
    Vec3& operator*=(const Vec3& v) {
        x *= v.x;
        y *= v.y;
        z *= v.z;
        return *this;
    }
    Vec3& operator*=(f32 s) {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }
    Vec3& operator/=(const Vec3& v) {
        x /= v.x;
        y /= v.y;
        z /= v.z;
        return *this;
    }
    Vec3& operator/=(f32 s) {
        x /= s;
        y /= s;
        z /= s;
        return *this;
    }

    bool operator==(const Vec3& v) const {
        return x == v.x && y == v.y && z == v.z;
    }
    bool operator!=(const Vec3& v) const {
        return !(*this == v);
    }

    f32 lengthSquared() const {
        return dot(*this);
    }
    f32 length() const {
        return sqrtf(lengthSquared());
    }
    f32 dot(const Vec3& v) const {
        return x * v.x + y * v.y + z * v.z;
    }
    Vec3 cross(const Vec3& v) const {
        return Vec3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
    }
    Vec3 normalized() const {
        f32 len = length();
        return len > 0.0f ? *this / len : Vec3(0.0f);
    }
    void normalize() {
        *this = normalized();
    }
    Vec3 lerped(f32 t, const Vec3& b) const {
        return (*this) * (1.0f - t) + b * t;
    }
};

struct Vec4 {
    union {
        f32 values[4];
        struct {
            f32 x, y, z, w;
        };
        struct {
            f32 r, g, b, a;
        };
    };

    Vec4() : x(0), y(0), z(0), w(0) {
    }
    Vec4(f32 value) : x(value), y(value), z(value), w(value) {
    }
    Vec4(f32 x, f32 y, f32 z, f32 w) : x(x), y(y), z(z), w(w) {
    }
    Vec4(const Vec3& xyz, f32 w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {
    }

    static Vec4 zero() {
        return Vec4(0.0f);
    }

    f32& operator[](usize i) {
        return values[i];
    }
    const f32& operator[](usize i) const {
        return values[i];
    }

    Vec4 operator+(const Vec4& v) const {
        return Vec4(x + v.x, y + v.y, z + v.z, w + v.w);
    }
    Vec4 operator-(const Vec4& v) const {
        return Vec4(x - v.x, y - v.y, z - v.z, w - v.w);
    }
    Vec4 operator*(const Vec4& v) const {
        return Vec4(x * v.x, y * v.y, z * v.z, w * v.w);
    }
    Vec4 operator*(f32 s) const {
        return Vec4(x * s, y * s, z * s, w * s);
    }
    Vec4 operator/(const Vec4& v) const {
        return Vec4(x / v.x, y / v.y, z / v.z, w / v.w);
    }
    Vec4 operator/(f32 s) const {
        return Vec4(x / s, y / s, z / s, w / s);
    }
    Vec4 operator-() const {
        return Vec4(-x, -y, -z, -w);
    }

    Vec4& operator+=(const Vec4& v) {
        x += v.x;
        y += v.y;
        z += v.z;
        w += v.w;
        return *this;
    }
    Vec4& operator-=(const Vec4& v) {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        w -= v.w;
        return *this;
    }
    Vec4& operator*=(const Vec4& v) {
        x *= v.x;
        y *= v.y;
        z *= v.z;
        w *= v.w;
        return *this;
    }
    Vec4& operator*=(f32 s) {
        x *= s;
        y *= s;
        z *= s;
        w *= s;
        return *this;
    }
    Vec4& operator/=(const Vec4& v) {
        x /= v.x;
        y /= v.y;
        z /= v.z;
        w /= v.w;
        return *this;
    }
    Vec4& operator/=(f32 s) {
        x /= s;
        y /= s;
        z /= s;
        w /= s;
        return *this;
    }

    bool operator==(const Vec4& v) const {
        return x == v.x && y == v.y && z == v.z && w == v.w;
    }
    bool operator!=(const Vec4& v) const {
        return !(*this == v);
    }

    f32 lengthSquared() const {
        return dot(*this);
    }
    f32 length() const {
        return sqrtf(lengthSquared());
    }
    f32 dot(const Vec4& v) const {
        return x * v.x + y * v.y + z * v.z + w * v.w;
    }
    Vec4 normalized() const {
        f32 len = length();
        return len > 0.0f ? *this / len : Vec4(0.0f);
    }
    void normalize() {
        *this = normalized();
    }
    Vec4 lerped(f32 t, const Vec4& b) const {
        return (*this) * (1.0f - t) + b * t;
    }
    Vec3 xyz() const {
        return Vec3(x, y, z);
    }
};

struct Mat2x2 {
    union {
        f32 elements[2][2];
        Vec2 columns[2];
    };

    Mat2x2() {
        identity();
    }
    Mat2x2(f32 diagonal) {
        setZero();
        columns[0].x = diagonal;
        columns[1].y = diagonal;
    }

    static Mat2x2 zero() {
        return Mat2x2(0.0f);
    }
    static Mat2x2 diagonal(f32 diagonal) {
        return Mat2x2(diagonal);
    }

    Vec2& operator[](usize i) {
        return columns[i];
    }
    const Vec2& operator[](usize i) const {
        return columns[i];
    }

    f32& operator()(usize row, usize col) {
        return columns[col][row];
    }
    const f32& operator()(usize row, usize col) const {
        return columns[col][row];
    }

    Mat2x2 operator+(const Mat2x2& other) const {
        Mat2x2 result(0.0f);
        result.columns[0] = columns[0] + other.columns[0];
        result.columns[1] = columns[1] + other.columns[1];
        return result;
    }
    Mat2x2 operator-(const Mat2x2& other) const {
        Mat2x2 result(0.0f);
        result.columns[0] = columns[0] - other.columns[0];
        result.columns[1] = columns[1] - other.columns[1];
        return result;
    }
    Mat2x2 operator*(f32 scalar) const {
        Mat2x2 result(0.0f);
        result.columns[0] = columns[0] * scalar;
        result.columns[1] = columns[1] * scalar;
        return result;
    }
    Mat2x2 operator/(f32 scalar) const {
        Mat2x2 result(0.0f);
        result.columns[0] = columns[0] / scalar;
        result.columns[1] = columns[1] / scalar;
        return result;
    }
    Vec2 operator*(const Vec2& v) const {
        return columns[0] * v.x + columns[1] * v.y;
    }
    Mat2x2 operator*(const Mat2x2& other) const {
        Mat2x2 result(0.0f);
        result.columns[0] = (*this) * other.columns[0];
        result.columns[1] = (*this) * other.columns[1];
        return result;
    }

    Mat2x2& operator+=(const Mat2x2& other) {
        columns[0] += other.columns[0];
        columns[1] += other.columns[1];
        return *this;
    }
    Mat2x2& operator-=(const Mat2x2& other) {
        columns[0] -= other.columns[0];
        columns[1] -= other.columns[1];
        return *this;
    }
    Mat2x2& operator*=(f32 scalar) {
        columns[0] *= scalar;
        columns[1] *= scalar;
        return *this;
    }
    Mat2x2& operator/=(f32 scalar) {
        columns[0] /= scalar;
        columns[1] /= scalar;
        return *this;
    }
    Mat2x2& operator*=(const Mat2x2& other) {
        *this = *this * other;
        return *this;
    }

    void setZero() {
        columns[0] = Vec2(0.0f);
        columns[1] = Vec2(0.0f);
    }
    void identity() {
        setZero();
        columns[0].x = 1.0f;
        columns[1].y = 1.0f;
    }
    Mat2x2 transposed() const {
        Mat2x2 result(0.0f);
        result(0, 0) = (*this)(0, 0);
        result(0, 1) = (*this)(1, 0);
        result(1, 0) = (*this)(0, 1);
        result(1, 1) = (*this)(1, 1);
        return result;
    }
    void transpose() {
        *this = transposed();
    }
    f32 determinant() const {
        return columns[0].x * columns[1].y - columns[0].y * columns[1].x;
    }
    Mat2x2 inversed() const {
        Mat2x2 result(0.0f);
        f32 inv_det = 1.0f / determinant();
        result(0, 0) = +(*this)(1, 1) * inv_det;
        result(0, 1) = -(*this)(0, 1) * inv_det;
        result(1, 0) = -(*this)(1, 0) * inv_det;
        result(1, 1) = +(*this)(0, 0) * inv_det;
        return result;
    }
    void invert() {
        *this = inversed();
    }
};

struct Mat3x3 {
    union {
        f32 elements[3][3];
        Vec3 columns[3];
    };

    Mat3x3() {
        identity();
    }
    Mat3x3(f32 diagonal) {
        setZero();
        columns[0].x = diagonal;
        columns[1].y = diagonal;
        columns[2].z = diagonal;
    }

    static Mat3x3 zero() {
        return Mat3x3(0.0f);
    }
    static Mat3x3 diagonal(f32 diagonal) {
        return Mat3x3(diagonal);
    }

    Vec3& operator[](usize i) {
        return columns[i];
    }
    const Vec3& operator[](usize i) const {
        return columns[i];
    }

    f32& operator()(usize row, usize col) {
        return columns[col][row];
    }
    const f32& operator()(usize row, usize col) const {
        return columns[col][row];
    }

    Mat3x3 operator+(const Mat3x3& other) const {
        Mat3x3 result(0.0f);
        result.columns[0] = columns[0] + other.columns[0];
        result.columns[1] = columns[1] + other.columns[1];
        result.columns[2] = columns[2] + other.columns[2];
        return result;
    }
    Mat3x3 operator-(const Mat3x3& other) const {
        Mat3x3 result(0.0f);
        result.columns[0] = columns[0] - other.columns[0];
        result.columns[1] = columns[1] - other.columns[1];
        result.columns[2] = columns[2] - other.columns[2];
        return result;
    }
    Mat3x3 operator*(f32 scalar) const {
        Mat3x3 result(0.0f);
        result.columns[0] = columns[0] * scalar;
        result.columns[1] = columns[1] * scalar;
        result.columns[2] = columns[2] * scalar;
        return result;
    }
    Mat3x3 operator/(f32 scalar) const {
        Mat3x3 result(0.0f);
        result.columns[0] = columns[0] / scalar;
        result.columns[1] = columns[1] / scalar;
        result.columns[2] = columns[2] / scalar;
        return result;
    }
    Vec3 operator*(const Vec3& v) const {
        return columns[0] * v.x + columns[1] * v.y + columns[2] * v.z;
    }
    Mat3x3 operator*(const Mat3x3& other) const {
        Mat3x3 result(0.0f);
        result.columns[0] = (*this) * other.columns[0];
        result.columns[1] = (*this) * other.columns[1];
        result.columns[2] = (*this) * other.columns[2];
        return result;
    }

    Mat3x3& operator+=(const Mat3x3& other) {
        columns[0] += other.columns[0];
        columns[1] += other.columns[1];
        columns[2] += other.columns[2];
        return *this;
    }
    Mat3x3& operator-=(const Mat3x3& other) {
        columns[0] -= other.columns[0];
        columns[1] -= other.columns[1];
        columns[2] -= other.columns[2];
        return *this;
    }
    Mat3x3& operator*=(f32 scalar) {
        columns[0] *= scalar;
        columns[1] *= scalar;
        columns[2] *= scalar;
        return *this;
    }
    Mat3x3& operator/=(f32 scalar) {
        columns[0] /= scalar;
        columns[1] /= scalar;
        columns[2] /= scalar;
        return *this;
    }
    Mat3x3& operator*=(const Mat3x3& other) {
        *this = *this * other;
        return *this;
    }

    void setZero() {
        columns[0] = Vec3(0.0f);
        columns[1] = Vec3(0.0f);
        columns[2] = Vec3(0.0f);
    }
    void identity() {
        setZero();
        columns[0].x = 1.0f;
        columns[1].y = 1.0f;
        columns[2].z = 1.0f;
    }
    Mat3x3 transposed() const {
        Mat3x3 result(0.0f);
        for (usize row = 0; row < 3; ++row) {
            for (usize col = 0; col < 3; ++col) {
                result(row, col) = (*this)(col, row);
            }
        }
        return result;
    }
    void transpose() {
        *this = transposed();
    }
    f32 determinant() const {
        Vec3 c0 = columns[1].cross(columns[2]);
        return c0.dot(columns[0]);
    }
    Mat3x3 inversed() const {
        Vec3 cross0 = columns[1].cross(columns[2]);
        Vec3 cross1 = columns[2].cross(columns[0]);
        Vec3 cross2 = columns[0].cross(columns[1]);
        f32 inv_det = 1.0f / cross2.dot(columns[2]);

        Mat3x3 result(0.0f);
        result.columns[0] = cross0 * inv_det;
        result.columns[1] = cross1 * inv_det;
        result.columns[2] = cross2 * inv_det;
        return result.transposed();
    }
    void invert() {
        *this = inversed();
    }
};

struct Mat4x4 {
    union {
        f32 elements[4][4];
        Vec4 columns[4];
        struct {
            f32 ax, ay, az, aw;
            f32 bx, by, bz, bw;
            f32 cx, cy, cz, cw;
            f32 dx, dy, dz, dw;
        };
    };

    Mat4x4() {
        identity();
    }
    Mat4x4(f32 diagonal) {
        setZero();
        columns[0].x = diagonal;
        columns[1].y = diagonal;
        columns[2].z = diagonal;
        columns[3].w = diagonal;
    }

    static Mat4x4 zero() {
        return Mat4x4(0.0f);
    }
    static Mat4x4 diagonal(f32 diagonal) {
        return Mat4x4(diagonal);
    }

    Vec4& operator[](usize i) {
        return columns[i];
    }
    const Vec4& operator[](usize i) const {
        return columns[i];
    }

    f32& operator()(usize row, usize col) {
        return columns[col][row];
    }
    const f32& operator()(usize row, usize col) const {
        return columns[col][row];
    }

    Mat4x4 operator+(const Mat4x4& other) const {
        Mat4x4 result(0.0f);
        result.columns[0] = columns[0] + other.columns[0];
        result.columns[1] = columns[1] + other.columns[1];
        result.columns[2] = columns[2] + other.columns[2];
        result.columns[3] = columns[3] + other.columns[3];
        return result;
    }
    Mat4x4 operator-(const Mat4x4& other) const {
        Mat4x4 result(0.0f);
        result.columns[0] = columns[0] - other.columns[0];
        result.columns[1] = columns[1] - other.columns[1];
        result.columns[2] = columns[2] - other.columns[2];
        result.columns[3] = columns[3] - other.columns[3];
        return result;
    }
    Mat4x4 operator*(f32 scalar) const {
        Mat4x4 result(0.0f);
        result.columns[0] = columns[0] * scalar;
        result.columns[1] = columns[1] * scalar;
        result.columns[2] = columns[2] * scalar;
        result.columns[3] = columns[3] * scalar;
        return result;
    }
    Mat4x4 operator/(f32 scalar) const {
        Mat4x4 result(0.0f);
        result.columns[0] = columns[0] / scalar;
        result.columns[1] = columns[1] / scalar;
        result.columns[2] = columns[2] / scalar;
        result.columns[3] = columns[3] / scalar;
        return result;
    }
    Vec4 operator*(const Vec4& v) const {
        return columns[0] * v.x + columns[1] * v.y + columns[2] * v.z +
               columns[3] * v.w;
    }
    Mat4x4 operator*(const Mat4x4& other) const {
        Mat4x4 result(0.0f);
        result.columns[0] = (*this) * other.columns[0];
        result.columns[1] = (*this) * other.columns[1];
        result.columns[2] = (*this) * other.columns[2];
        result.columns[3] = (*this) * other.columns[3];
        return result;
    }

    Mat4x4& operator+=(const Mat4x4& other) {
        columns[0] += other.columns[0];
        columns[1] += other.columns[1];
        columns[2] += other.columns[2];
        columns[3] += other.columns[3];
        return *this;
    }
    Mat4x4& operator-=(const Mat4x4& other) {
        columns[0] -= other.columns[0];
        columns[1] -= other.columns[1];
        columns[2] -= other.columns[2];
        columns[3] -= other.columns[3];
        return *this;
    }
    Mat4x4& operator*=(f32 scalar) {
        columns[0] *= scalar;
        columns[1] *= scalar;
        columns[2] *= scalar;
        columns[3] *= scalar;
        return *this;
    }
    Mat4x4& operator/=(f32 scalar) {
        columns[0] /= scalar;
        columns[1] /= scalar;
        columns[2] /= scalar;
        columns[3] /= scalar;
        return *this;
    }
    Mat4x4& operator*=(const Mat4x4& other) {
        *this = *this * other;
        return *this;
    }

    void setZero() {
        columns[0] = Vec4(0.0f);
        columns[1] = Vec4(0.0f);
        columns[2] = Vec4(0.0f);
        columns[3] = Vec4(0.0f);
    }
    void identity() {
        setZero();
        columns[0].x = 1.0f;
        columns[1].y = 1.0f;
        columns[2].z = 1.0f;
        columns[3].w = 1.0f;
    }
    Mat4x4 transposed() const {
        Mat4x4 result(0.0f);
        for (usize row = 0; row < 4; ++row) {
            for (usize col = 0; col < 4; ++col) {
                result(row, col) = (*this)(col, row);
            }
        }
        return result;
    }
    void transpose() {
        *this = transposed();
    }
    f32 determinant() const {
        Vec3 c01 = columns[0].xyz().cross(columns[1].xyz());
        Vec3 c23 = columns[2].xyz().cross(columns[3].xyz());
        Vec3 b10 =
            columns[0].xyz() * columns[1].w - columns[1].xyz() * columns[0].w;
        Vec3 b32 =
            columns[2].xyz() * columns[3].w - columns[3].xyz() * columns[2].w;
        return c01.dot(b32) + c23.dot(b10);
    }
    Mat4x4 inversed() const {
        Vec3 c01 = columns[0].xyz().cross(columns[1].xyz());
        Vec3 c23 = columns[2].xyz().cross(columns[3].xyz());
        Vec3 b10 =
            columns[0].xyz() * columns[1].w - columns[1].xyz() * columns[0].w;
        Vec3 b32 =
            columns[2].xyz() * columns[3].w - columns[3].xyz() * columns[2].w;

        f32 inv_det = 1.0f / (c01.dot(b32) + c23.dot(b10));
        c01 *= inv_det;
        c23 *= inv_det;
        b10 *= inv_det;
        b32 *= inv_det;

        Mat4x4 result(0.0f);
        result.columns[0] = Vec4(
            columns[1].xyz().cross(b32) + c23 * columns[1].w,
            -columns[1].xyz().dot(c23)
        );
        result.columns[1] = Vec4(
            b32.cross(columns[0].xyz()) - c23 * columns[0].w,
            columns[0].xyz().dot(c23)
        );
        result.columns[2] = Vec4(
            columns[3].xyz().cross(b10) + c01 * columns[3].w,
            -columns[3].xyz().dot(c01)
        );
        result.columns[3] = Vec4(
            b10.cross(columns[2].xyz()) - c01 * columns[2].w,
            columns[2].xyz().dot(c01)
        );
        return result.transposed();
    }
    void invert() {
        *this = inversed();
    }

    Vec3 transformPoint(const Vec3& p) const {
        Vec4 result = (*this) * Vec4(p, 1.0f);
        return Vec3(result.x, result.y, result.z);
    }
    Vec3 transformVector(const Vec3& v) const {
        Vec4 result = (*this) * Vec4(v, 0.0f);
        return Vec3(result.x, result.y, result.z);
    }

    static Mat4x4 translate(const Vec3& translation) {
        Mat4x4 result;
        result.columns[3].x = translation.x;
        result.columns[3].y = translation.y;
        result.columns[3].z = translation.z;
        return result;
    }
    static Mat4x4 inverseTranslate(const Mat4x4& matrix) {
        Mat4x4 result = matrix;
        result.columns[3].x = -result.columns[3].x;
        result.columns[3].y = -result.columns[3].y;
        result.columns[3].z = -result.columns[3].z;
        return result;
    }

    static Mat4x4 scale(const Vec3& scale) {
        Mat4x4 result;
        result.columns[0].x = scale.x;
        result.columns[1].y = scale.y;
        result.columns[2].z = scale.z;
        return result;
    }
    static Mat4x4 inverseScale(const Mat4x4& matrix) {
        Mat4x4 result = matrix;
        result.columns[0].x = 1.0f / result.columns[0].x;
        result.columns[1].y = 1.0f / result.columns[1].y;
        result.columns[2].z = 1.0f / result.columns[2].z;
        return result;
    }

    static Mat4x4 rotateX(f32 angle) {
        Mat4x4 result;
        f32 c = cosf(angle);
        f32 s = sinf(angle);
        result.columns[1].y = c;
        result.columns[1].z = s;
        result.columns[2].y = -s;
        result.columns[2].z = c;
        return result;
    }
    static Mat4x4 rotateY(f32 angle) {
        Mat4x4 result;
        f32 c = cosf(angle);
        f32 s = sinf(angle);
        result.columns[0].x = c;
        result.columns[0].z = -s;
        result.columns[2].x = s;
        result.columns[2].z = c;
        return result;
    }
    static Mat4x4 rotateZ(f32 angle) {
        Mat4x4 result;
        f32 c = cosf(angle);
        f32 s = sinf(angle);
        result.columns[0].x = c;
        result.columns[0].y = s;
        result.columns[1].x = -s;
        result.columns[1].y = c;
        return result;
    }
    static Mat4x4 rotateRH(f32 angle, Vec3 axis) {
        Mat4x4 result;
        axis = axis.normalized();
        f32 sin_theta = sinf(angle);
        f32 cos_theta = cosf(angle);
        f32 one_minus_cos = 1.0f - cos_theta;

        result(0, 0) = axis.x * axis.x * one_minus_cos + cos_theta;
        result(0, 1) = axis.x * axis.y * one_minus_cos - axis.z * sin_theta;
        result(0, 2) = axis.x * axis.z * one_minus_cos + axis.y * sin_theta;
        result(1, 0) = axis.y * axis.x * one_minus_cos + axis.z * sin_theta;
        result(1, 1) = axis.y * axis.y * one_minus_cos + cos_theta;
        result(1, 2) = axis.y * axis.z * one_minus_cos - axis.x * sin_theta;
        result(2, 0) = axis.z * axis.x * one_minus_cos - axis.y * sin_theta;
        result(2, 1) = axis.z * axis.y * one_minus_cos + axis.x * sin_theta;
        result(2, 2) = axis.z * axis.z * one_minus_cos + cos_theta;
        return result;
    }
    static Mat4x4 rotateLH(f32 angle, const Vec3& axis) {
        return rotateRH(-angle, axis);
    }
    static Mat4x4 inverseRotate(const Mat4x4& matrix) {
        return matrix.transposed();
    }

    static Mat4x4 orthographicRHNO(
        f32 left,
        f32 right,
        f32 bottom,
        f32 top,
        f32 near_z,
        f32 far_z
    ) {
        Mat4x4 result(0.0f);
        result(0, 0) = 2.0f / (right - left);
        result(1, 1) = 2.0f / (top - bottom);
        result(2, 2) = 2.0f / (near_z - far_z);
        result(3, 3) = 1.0f;
        result(0, 3) = (left + right) / (left - right);
        result(1, 3) = (bottom + top) / (bottom - top);
        result(2, 3) = (near_z + far_z) / (near_z - far_z);
        return result;
    }
    static Mat4x4 orthographicRHZO(
        f32 left,
        f32 right,
        f32 bottom,
        f32 top,
        f32 near_z,
        f32 far_z
    ) {
        Mat4x4 result(0.0f);
        result(0, 0) = 2.0f / (right - left);
        result(1, 1) = 2.0f / (top - bottom);
        result(2, 2) = 1.0f / (near_z - far_z);
        result(3, 3) = 1.0f;
        result(0, 3) = (left + right) / (left - right);
        result(1, 3) = (bottom + top) / (bottom - top);
        result(2, 3) = near_z / (near_z - far_z);
        return result;
    }
    static Mat4x4 orthographicLHNO(
        f32 left,
        f32 right,
        f32 bottom,
        f32 top,
        f32 near_z,
        f32 far_z
    ) {
        Mat4x4 result =
            orthographicRHNO(left, right, bottom, top, near_z, far_z);
        result(2, 2) = -result(2, 2);
        return result;
    }
    static Mat4x4 orthographicLHZO(
        f32 left,
        f32 right,
        f32 bottom,
        f32 top,
        f32 near_z,
        f32 far_z
    ) {
        Mat4x4 result =
            orthographicRHZO(left, right, bottom, top, near_z, far_z);
        result(2, 2) = -result(2, 2);
        return result;
    }
    static Mat4x4 inverseOrthographic(const Mat4x4& ortho) {
        Mat4x4 result(0.0f);
        result(0, 0) = 1.0f / ortho(0, 0);
        result(1, 1) = 1.0f / ortho(1, 1);
        result(2, 2) = 1.0f / ortho(2, 2);
        result(3, 3) = 1.0f;
        result(0, 3) = -ortho(0, 3) * result(0, 0);
        result(1, 3) = -ortho(1, 3) * result(1, 1);
        result(2, 3) = -ortho(2, 3) * result(2, 2);
        return result;
    }
    static Mat4x4 orthographicProjection(
        f32 left,
        f32 right,
        f32 top,
        f32 bottom
    ) {
        return orthographicRHZO(left, right, bottom, top, 0.0f, 1.0f);
    }

    static Mat4x4 perspectiveRHNO(
        f32 fov,
        f32 aspect_ratio,
        f32 near_z,
        f32 far_z
    ) {
        Mat4x4 result(0.0f);
        f32 cotangent = 1.0f / tanf(fov * 0.5f);
        result(0, 0) = cotangent / aspect_ratio;
        result(1, 1) = cotangent;
        result(2, 2) = (near_z + far_z) / (near_z - far_z);
        result(2, 3) = (2.0f * near_z * far_z) / (near_z - far_z);
        result(3, 2) = -1.0f;
        return result;
    }
    static Mat4x4 perspectiveRHZO(
        f32 fov,
        f32 aspect_ratio,
        f32 near_z,
        f32 far_z
    ) {
        Mat4x4 result(0.0f);
        f32 cotangent = 1.0f / tanf(fov * 0.5f);
        result(0, 0) = cotangent / aspect_ratio;
        result(1, 1) = cotangent;
        result(2, 2) = far_z / (near_z - far_z);
        result(2, 3) = (near_z * far_z) / (near_z - far_z);
        result(3, 2) = -1.0f;
        return result;
    }
    static Mat4x4 perspectiveLHNO(
        f32 fov,
        f32 aspect_ratio,
        f32 near_z,
        f32 far_z
    ) {
        Mat4x4 result = perspectiveRHNO(fov, aspect_ratio, near_z, far_z);
        result(2, 2) = -result(2, 2);
        result(3, 2) = -result(3, 2);
        return result;
    }
    static Mat4x4 perspectiveLHZO(
        f32 fov,
        f32 aspect_ratio,
        f32 near_z,
        f32 far_z
    ) {
        Mat4x4 result = perspectiveRHZO(fov, aspect_ratio, near_z, far_z);
        result(2, 2) = -result(2, 2);
        result(3, 2) = -result(3, 2);
        return result;
    }
    static Mat4x4 inversePerspectiveRH(const Mat4x4& perspective) {
        Mat4x4 result(0.0f);
        result(0, 0) = 1.0f / perspective(0, 0);
        result(1, 1) = 1.0f / perspective(1, 1);
        result(2, 3) = 1.0f / perspective(3, 2);
        result(3, 2) = perspective(2, 3);
        result(3, 3) = perspective(2, 2) * result(2, 3);
        return result;
    }
    static Mat4x4 inversePerspectiveLH(const Mat4x4& perspective) {
        Mat4x4 result(0.0f);
        result(0, 0) = 1.0f / perspective(0, 0);
        result(1, 1) = 1.0f / perspective(1, 1);
        result(2, 3) = 1.0f / perspective(3, 2);
        result(3, 2) = perspective(2, 3);
        result(3, 3) = -perspective(2, 2) * result(2, 3);
        return result;
    }
    static Mat4x4 perspective(f32 fov, f32 aspect, f32 z_near, f32 z_far) {
        return perspectiveRHNO(fov, aspect, z_near, z_far);
    }

    static Mat4x4 lookAtRH(
        const Vec3& eye,
        const Vec3& center,
        const Vec3& up
    ) {
        Vec3 f = (center - eye).normalized();
        Vec3 s = f.cross(up).normalized();
        Vec3 u = s.cross(f);

        Mat4x4 result;
        result.columns[0] = Vec4(s.x, u.x, -f.x, 0.0f);
        result.columns[1] = Vec4(s.y, u.y, -f.y, 0.0f);
        result.columns[2] = Vec4(s.z, u.z, -f.z, 0.0f);
        result.columns[3] = Vec4(-s.dot(eye), -u.dot(eye), f.dot(eye), 1.0f);
        return result;
    }
    static Mat4x4 lookAtLH(
        const Vec3& eye,
        const Vec3& center,
        const Vec3& up
    ) {
        Vec3 f = (eye - center).normalized();
        Vec3 s = f.cross(up).normalized();
        Vec3 u = s.cross(f);

        Mat4x4 result;
        result.columns[0] = Vec4(s.x, u.x, -f.x, 0.0f);
        result.columns[1] = Vec4(s.y, u.y, -f.y, 0.0f);
        result.columns[2] = Vec4(s.z, u.z, -f.z, 0.0f);
        result.columns[3] = Vec4(-s.dot(eye), -u.dot(eye), f.dot(eye), 1.0f);
        return result;
    }
    static Mat4x4 inverseLookAt(const Mat4x4& matrix) {
        Mat3x3 rotation(0.0f);
        rotation.columns[0] = matrix.columns[0].xyz();
        rotation.columns[1] = matrix.columns[1].xyz();
        rotation.columns[2] = matrix.columns[2].xyz();
        rotation = rotation.transposed();

        Mat4x4 result;
        result.columns[0] = Vec4(rotation.columns[0], 0.0f);
        result.columns[1] = Vec4(rotation.columns[1], 0.0f);
        result.columns[2] = Vec4(rotation.columns[2], 0.0f);
        result.columns[3] = matrix.columns[3] * -1.0f;
        result(0, 3) =
            -matrix(0, 3) / (rotation(0, 0) + rotation(1, 0) + rotation(2, 0));
        result(1, 3) =
            -matrix(1, 3) / (rotation(0, 1) + rotation(1, 1) + rotation(2, 1));
        result(2, 3) =
            -matrix(2, 3) / (rotation(0, 2) + rotation(1, 2) + rotation(2, 2));
        result(3, 3) = 1.0f;
        return result;
    }
    static Mat4x4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        return lookAtRH(eye, center, up);
    }
};

struct Quat {
    union {
        f32 values[4];
        struct {
            f32 x, y, z, w;
        };
    };

    Quat() : x(0), y(0), z(0), w(1) {
    }
    Quat(f32 x, f32 y, f32 z, f32 w) : x(x), y(y), z(z), w(w) {
    }
    Quat(const Vec4& v) : x(v.x), y(v.y), z(v.z), w(v.w) {
    }

    static Quat identity() {
        return Quat(0.0f, 0.0f, 0.0f, 1.0f);
    }

    f32& operator[](usize i) {
        return values[i];
    }
    const f32& operator[](usize i) const {
        return values[i];
    }

    Quat operator+(const Quat& q) const {
        return Quat(x + q.x, y + q.y, z + q.z, w + q.w);
    }
    Quat operator-(const Quat& q) const {
        return Quat(x - q.x, y - q.y, z - q.z, w - q.w);
    }
    Quat operator*(f32 s) const {
        return Quat(x * s, y * s, z * s, w * s);
    }
    Quat operator/(f32 s) const {
        return Quat(x / s, y / s, z / s, w / s);
    }
    Quat operator*(const Quat& q) const {
        return Quat(
            w * q.x + x * q.w + y * q.z - z * q.y,
            w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w,
            w * q.w - x * q.x - y * q.y - z * q.z
        );
    }

    Quat& operator+=(const Quat& q) {
        x += q.x;
        y += q.y;
        z += q.z;
        w += q.w;
        return *this;
    }
    Quat& operator-=(const Quat& q) {
        x -= q.x;
        y -= q.y;
        z -= q.z;
        w -= q.w;
        return *this;
    }
    Quat& operator*=(f32 s) {
        x *= s;
        y *= s;
        z *= s;
        w *= s;
        return *this;
    }
    Quat& operator/=(f32 s) {
        x /= s;
        y /= s;
        z /= s;
        w /= s;
        return *this;
    }
    Quat& operator*=(const Quat& q) {
        *this = *this * q;
        return *this;
    }

    bool operator==(const Quat& q) const {
        return x == q.x && y == q.y && z == q.z && w == q.w;
    }
    bool operator!=(const Quat& q) const {
        return !(*this == q);
    }

    f32 dot(const Quat& q) const {
        return x * q.x + y * q.y + z * q.z + w * q.w;
    }
    f32 lengthSquared() const {
        return dot(*this);
    }
    f32 length() const {
        return sqrtf(lengthSquared());
    }
    Quat normalized() const {
        f32 len = length();
        return len > 0.0f ? *this / len : Quat();
    }
    void normalize() {
        *this = normalized();
    }
    Quat inversed() const {
        return Quat(-x, -y, -z, w) / lengthSquared();
    }
    void invert() {
        *this = inversed();
    }
    Vec4 asVec4() const {
        return Vec4(x, y, z, w);
    }

    Quat nlerp(f32 t, Quat other) const {
        return ((*this) * (1.0f - t) + other * t).normalized();
    }
    Quat slerp(f32 t, Quat other) const {
        f32 cos_theta = dot(other);
        if (cos_theta < 0.0f) {
            cos_theta = -cos_theta;
            other = other * -1.0f;
        }
        if (cos_theta > 0.9995f) {
            return nlerp(t, other);
        }

        f32 angle = acosf(cos_theta);
        f32 left_mix = sinf((1.0f - t) * angle);
        f32 right_mix = sinf(t * angle);
        return ((*this) * left_mix + other * right_mix).normalized();
    }

    Mat4x4 toMat4() const {
        Quat q = normalized();

        f32 xx = q.x * q.x;
        f32 yy = q.y * q.y;
        f32 zz = q.z * q.z;
        f32 xy = q.x * q.y;
        f32 xz = q.x * q.z;
        f32 yz = q.y * q.z;
        f32 wx = q.w * q.x;
        f32 wy = q.w * q.y;
        f32 wz = q.w * q.z;

        Mat4x4 result;
        result(0, 0) = 1.0f - 2.0f * (yy + zz);
        result(0, 1) = 2.0f * (xy - wz);
        result(0, 2) = 2.0f * (xz + wy);
        result(1, 0) = 2.0f * (xy + wz);
        result(1, 1) = 1.0f - 2.0f * (xx + zz);
        result(1, 2) = 2.0f * (yz - wx);
        result(2, 0) = 2.0f * (xz - wy);
        result(2, 1) = 2.0f * (yz + wx);
        result(2, 2) = 1.0f - 2.0f * (xx + yy);
        result(0, 3) = 0.0f;
        result(1, 3) = 0.0f;
        result(2, 3) = 0.0f;
        result(3, 0) = 0.0f;
        result(3, 1) = 0.0f;
        result(3, 2) = 0.0f;
        result(3, 3) = 1.0f;
        return result;
    }

    static Quat fromAxisAngleRH(Vec3 axis, f32 angle) {
        axis = axis.normalized();
        f32 half_angle = angle * 0.5f;
        f32 sin_half = sinf(half_angle);
        f32 cos_half = cosf(half_angle);
        return Quat(
            axis.x * sin_half,
            axis.y * sin_half,
            axis.z * sin_half,
            cos_half
        );
    }
    static Quat fromAxisAngleLH(const Vec3& axis, f32 angle) {
        return fromAxisAngleRH(axis, -angle);
    }
    static Quat fromMat4RH(const Mat4x4& m) {
        f32 t;
        Quat q;

        if (m(2, 2) < 0.0f) {
            if (m(0, 0) > m(1, 1)) {
                t = 1.0f + m(0, 0) - m(1, 1) - m(2, 2);
                q = Quat(
                    t,
                    m(0, 1) + m(1, 0),
                    m(2, 0) + m(0, 2),
                    m(2, 1) - m(1, 2)
                );
            } else {
                t = 1.0f - m(0, 0) + m(1, 1) - m(2, 2);
                q = Quat(
                    m(0, 1) + m(1, 0),
                    t,
                    m(1, 2) + m(2, 1),
                    m(0, 2) - m(2, 0)
                );
            }
        } else {
            if (m(0, 0) < -m(1, 1)) {
                t = 1.0f - m(0, 0) - m(1, 1) + m(2, 2);
                q = Quat(
                    m(2, 0) + m(0, 2),
                    m(1, 2) + m(2, 1),
                    t,
                    m(1, 0) - m(0, 1)
                );
            } else {
                t = 1.0f + m(0, 0) + m(1, 1) + m(2, 2);
                q = Quat(
                    m(2, 1) - m(1, 2),
                    m(0, 2) - m(2, 0),
                    m(1, 0) - m(0, 1),
                    t
                );
            }
        }

        return q * (0.5f / sqrtf(t));
    }
    static Quat fromMat4LH(const Mat4x4& m) {
        f32 t;
        Quat q;

        if (m(2, 2) < 0.0f) {
            if (m(0, 0) > m(1, 1)) {
                t = 1.0f + m(0, 0) - m(1, 1) - m(2, 2);
                q = Quat(
                    t,
                    m(0, 1) + m(1, 0),
                    m(2, 0) + m(0, 2),
                    m(1, 2) - m(2, 1)
                );
            } else {
                t = 1.0f - m(0, 0) + m(1, 1) - m(2, 2);
                q = Quat(
                    m(0, 1) + m(1, 0),
                    t,
                    m(1, 2) + m(2, 1),
                    m(2, 0) - m(0, 2)
                );
            }
        } else {
            if (m(0, 0) < -m(1, 1)) {
                t = 1.0f - m(0, 0) - m(1, 1) + m(2, 2);
                q = Quat(
                    m(2, 0) + m(0, 2),
                    m(1, 2) + m(2, 1),
                    t,
                    m(0, 1) - m(1, 0)
                );
            } else {
                t = 1.0f + m(0, 0) + m(1, 1) + m(2, 2);
                q = Quat(
                    m(1, 2) - m(2, 1),
                    m(2, 0) - m(0, 2),
                    m(0, 1) - m(1, 0),
                    t
                );
            }
        }

        return q * (0.5f / sqrtf(t));
    }
};

inline Vec2 lerp(const Vec2& a, f32 t, const Vec2& b) {
    return a.lerped(t, b);
}
inline Vec3 lerp(const Vec3& a, f32 t, const Vec3& b) {
    return a.lerped(t, b);
}
inline Vec4 lerp(const Vec4& a, f32 t, const Vec4& b) {
    return a.lerped(t, b);
}

inline Vec2 operator*(f32 s, const Vec2& v) {
    return v * s;
}
inline Vec3 operator*(f32 s, const Vec3& v) {
    return v * s;
}
inline Vec4 operator*(f32 s, const Vec4& v) {
    return v * s;
}
inline IVec2 operator*(i32 s, const IVec2& v) {
    return v * s;
}
inline Mat2x2 operator*(f32 s, const Mat2x2& m) {
    return m * s;
}
inline Mat3x3 operator*(f32 s, const Mat3x3& m) {
    return m * s;
}
inline Mat4x4 operator*(f32 s, const Mat4x4& m) {
    return m * s;
}
inline Quat operator*(f32 s, const Quat& q) {
    return q * s;
}
