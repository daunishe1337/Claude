// ============================================================================
//  Common.h — общие константы, математика, цвета и генератор случайных чисел.
//  Этот файл не зависит от WinAPI и используется всеми модулями игры.
// ============================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// ---------------------------------------------------------------------------
//  Строки в UTF-8.
//  Весь видимый игроку текст на русском пишется через T8("..."). Макрос
//  гарантирует UTF-8 вне зависимости от кодовой страницы системы и стандарта
//  (в C++17 u8"..." — это const char[], в C++20 — const char8_t[]).
// ---------------------------------------------------------------------------
#define T8(s) reinterpret_cast<const char*>(u8##s)

namespace cfg {
// Внутреннее разрешение программного рендера (16:9).
constexpr int kScreenW = 320;
constexpr int kScreenH = 180;
// Стартовый масштаб окна (320x180 * 4 = 1280x720).
constexpr int kWindowScale = 4;
// Фиксированный шаг логики: 60 обновлений в секунду.
constexpr double kFixedDt = 1.0 / 60.0;
// Экран портативной консоли (как у классического карманного устройства).
constexpr int kConsoleW = 160;
constexpr int kConsoleH = 144;
// Частота дискретизации звука.
constexpr int kSampleRate = 44100;
} // namespace cfg

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTau = 6.28318530717958647692f;

// ---------------------------------------------------------------------------
//  Скалярная математика
// ---------------------------------------------------------------------------
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float saturate(float v) { return clampf(v, 0.0f, 1.0f); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
// Обратная интерполяция: где v между a и b (0..1, без ограничения).
inline float invLerp(float a, float b, float v) { return (b - a) != 0.0f ? (v - a) / (b - a) : 0.0f; }
inline float smoothstep(float e0, float e1, float x) {
    float t = saturate(invLerp(e0, e1, x));
    return t * t * (3.0f - 2.0f * t);
}
// Сдвигает cur к target не более чем на maxDelta.
inline float approach(float cur, float target, float maxDelta) {
    if (cur < target) return std::min(cur + maxDelta, target);
    return std::max(cur - maxDelta, target);
}
// Экспоненциальное сглаживание, не зависящее от частоты кадров.
inline float damp(float cur, float target, float speed, float dt) {
    return lerpf(target, cur, std::exp(-speed * dt));
}
inline int roundi(float v) { return static_cast<int>(std::floor(v + 0.5f)); }

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

// ---------------------------------------------------------------------------
//  Цвета: 0x00RRGGBB — ровно тот формат, который ожидает 32-битный DIB
//  (BI_RGB, порядок байтов в памяти B, G, R, X).
// ---------------------------------------------------------------------------
constexpr uint32_t rgb(int r, int g, int b) {
    return (static_cast<uint32_t>(r & 255) << 16) | (static_cast<uint32_t>(g & 255) << 8) |
           static_cast<uint32_t>(b & 255);
}
constexpr int colR(uint32_t c) { return static_cast<int>((c >> 16) & 255u); }
constexpr int colG(uint32_t c) { return static_cast<int>((c >> 8) & 255u); }
constexpr int colB(uint32_t c) { return static_cast<int>(c & 255u); }

// Линейная смесь двух цветов, t = 0 -> a, t = 1 -> b.
inline uint32_t lerpColor(uint32_t a, uint32_t b, float t) {
    t = saturate(t);
    return rgb(roundi(lerpf(float(colR(a)), float(colR(b)), t)),
               roundi(lerpf(float(colG(a)), float(colG(b)), t)),
               roundi(lerpf(float(colB(a)), float(colB(b)), t)));
}
// Умножение яркости (k может быть > 1, результат насыщается).
inline uint32_t scaleColor(uint32_t c, float k) {
    if (k < 0.0f) k = 0.0f;
    return rgb(clampi(roundi(colR(c) * k), 0, 255), clampi(roundi(colG(c) * k), 0, 255),
               clampi(roundi(colB(c) * k), 0, 255));
}
// Сложение с насыщением (для света и бликов).
inline uint32_t addColor(uint32_t a, uint32_t b) {
    return rgb(std::min(255, colR(a) + colR(b)), std::min(255, colG(a) + colG(b)),
               std::min(255, colB(a) + colB(b)));
}
// Покомпонентное умножение (модуляция цветом света), 255 = 1.0.
inline uint32_t mulColor(uint32_t a, uint32_t b) {
    return rgb(colR(a) * colR(b) / 255, colG(a) * colG(b) / 255, colB(a) * colB(b) / 255);
}
// Яркость 0..1 (для эффектов).
inline float luminance(uint32_t c) {
    return (0.299f * colR(c) + 0.587f * colG(c) + 0.114f * colB(c)) / 255.0f;
}

// ---------------------------------------------------------------------------
//  Rng — быстрый детерминированный генератор (xorshift32).
//  У каждого модуля свой экземпляр, чтобы поведение было воспроизводимым.
// ---------------------------------------------------------------------------
class Rng {
public:
    explicit Rng(uint32_t seed = 0x9E3779B9u) { reseed(seed); }
    void reseed(uint32_t seed) { state_ = seed ? seed : 0x9E3779B9u; }
    uint32_t nextU32() {
        uint32_t x = state_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state_ = x;
        return x;
    }
    // [0, 1)
    float next01() { return static_cast<float>(nextU32() >> 8) * (1.0f / 16777216.0f); }
    // [a, b)
    float range(float a, float b) { return a + (b - a) * next01(); }
    // [a, b] включительно
    int rangeInt(int a, int b) {
        if (b <= a) return a;
        return a + static_cast<int>(nextU32() % static_cast<uint32_t>(b - a + 1));
    }
    bool chance(float p) { return next01() < p; }
    // [-1, 1)
    float signedUnit() { return next01() * 2.0f - 1.0f; }

private:
    uint32_t state_;
};

// Детерминированный шум 1D (значение в [0,1)) — для мерцаний и рельефа.
inline float hash01(uint32_t n) {
    n = (n << 13) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return static_cast<float>(n & 0x00FFFFFFu) * (1.0f / 16777216.0f);
}
// Гладкий 1D value-noise в [0,1).
inline float valueNoise(float x, uint32_t seed = 0) {
    float fl = std::floor(x);
    int i = static_cast<int>(fl);
    float f = x - fl;
    float a = hash01(static_cast<uint32_t>(i) * 2654435761u + seed);
    float b = hash01(static_cast<uint32_t>(i + 1) * 2654435761u + seed);
    float u = f * f * (3.0f - 2.0f * f);
    return lerpf(a, b, u);
}
