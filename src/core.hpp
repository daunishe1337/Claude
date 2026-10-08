// Базовые типы и математика (без сторонних библиотек)
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

constexpr float kPi = 3.14159265358979f;
constexpr float kDeg = kPi / 180.0f;

struct Color {
    uint8_t r = 255, g = 255, b = 255, a = 255;
};
constexpr Color rgba(int r, int g, int b, int a = 255) {
    return Color{(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
}
inline Color withAlpha(Color c, int a) {
    c.a = (uint8_t)a;
    return c;
}
inline Color lerpColor(Color a, Color b, float t) {
    return Color{(uint8_t)(a.r + (b.r - a.r) * t), (uint8_t)(a.g + (b.g - a.g) * t),
                 (uint8_t)(a.b + (b.b - a.b) * t), (uint8_t)(a.a + (b.a - a.a) * t)};
}
Color hsv(float h, float s, float v);

namespace col {
constexpr Color white = rgba(255, 255, 255), black = rgba(0, 0, 0), red = rgba(230, 60, 60),
                green = rgba(70, 200, 90), yellow = rgba(250, 210, 60), gray = rgba(130, 130, 130),
                orange = rgba(255, 150, 40), blue = rgba(60, 130, 240);
}

struct Vec2 {
    float x = 0, y = 0;
};

struct Vec3 {
    float x = 0, y = 0, z = 0;
};
inline Vec3 v3(float x, float y, float z) { return Vec3{x, y, z}; }
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3& operator+=(Vec3& a, Vec3 b) { return a = a + b; }
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(Vec3 a) {
    float l = length(a);
    return l > 1e-8f ? a * (1.0f / l) : Vec3{0, 0, 0};
}

// Матрица 4x4, по столбцам (как в OpenGL). Векторы-столбцы: p' = M * p.
struct Mat4 {
    float m[16];
};
Mat4 identity();
Mat4 operator*(const Mat4& a, const Mat4& b);  // сначала b, потом a
Mat4 translate(float x, float y, float z);
Mat4 rotX(float deg);
Mat4 rotY(float deg);
Mat4 rotZ(float deg);
Mat4 scale(float x, float y, float z);
Mat4 perspective(float fovyDeg, float aspect, float n, float f);
Mat4 ortho(float l, float r, float b, float t, float n, float f);
Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up);
Vec3 xformPoint(const Mat4& m, Vec3 p);
Vec3 xformDir(const Mat4& m, Vec3 d);

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(Vec2 p) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
};

struct BBox {
    Vec3 min{0, 0, 0}, max{0, 0, 0};
};

struct Ray {
    Vec3 pos, dir;
};
bool rayBox(const Ray& r, const BBox& b, float& t);

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float approach(float cur, float target, float rate, float dt) {
    return cur + (target - cur) * (1.0f - std::exp(-rate * dt));
}

// Простой ГПСЧ
uint32_t rnd();
float rndf();               // 0..1
int rndi(int lo, int hi);   // включительно

std::string fmt(const char* f, ...);
std::string money(double v);
