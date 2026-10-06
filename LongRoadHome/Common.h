// Common.h - общие константы, математика, ГСЧ и разделяемое состояние забега.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace lrh {

constexpr int   kW  = 320;            // внутреннее разрешение
constexpr int   kH  = 180;
constexpr float kPi = 3.14159265f;
constexpr float kDt = 1.0f / 60.0f;   // фиксированный шаг логики

// ---- Баланс ----------------------------------------------------------------
constexpr float kTripSeconds = 210.0f;   // длина пути при полной скорости
constexpr float kFuelDrain   = 0.0068f;  // расход топлива в секунду
constexpr float kFuelCan     = 0.22f;    // сколько даёт одна канистра
constexpr int   kMaxFilm     = 4;
constexpr float kCamCooldown = 2.6f;     // «перезарядка» вспышки

// ---- Математика ------------------------------------------------------------
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float saturate(float v) { return clampf(v, 0.0f, 1.0f); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep(float t) { t = saturate(t); return t * t * (3.0f - 2.0f * t); }
inline float fract(float v) { return v - std::floor(v); }
inline uint32_t rgb(int r, int g, int b) {
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);
    return (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}
inline uint32_t scaleColor(uint32_t c, float k) {
    return rgb(int(((c >> 16) & 255) * k), int(((c >> 8) & 255) * k), int((c & 255) * k));
}
inline uint32_t mixColor(uint32_t a, uint32_t b, float t) {
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    return rgb(int(lerp(float(ar), float(br), t)), int(lerp(float(ag), float(bg), t)),
               int(lerp(float(ab), float(bb), t)));
}

// Небольшой быстрый ГСЧ (xorshift32).
struct Rng {
    uint32_t s = 0x1234567u;
    uint32_t nextU() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float next01() { return float(nextU() & 0xFFFFFF) / float(0x1000000); }
    float range(float a, float b) { return a + (b - a) * next01(); }
    int   irange(int a, int b) { return a + int(nextU() % uint32_t(b - a + 1)); }  // [a,b]
    bool  chance(float p) { return next01() < p; }
};
inline Rng g_rng;  // ГСЧ игрового потока (аудио-поток имеет свой)

// ---- Состояние забега, разделяемое между сценами -----------------------------
struct SharedState {
    float time      = 0.0f;   // секунд с начала забега
    float fuel      = 0.75f;  // 0..1
    float progress  = 0.0f;   // 0..1 путь до дома
    int   film      = 2;      // кадры камеры
    float camCooldown = 0.0f;
    float carSpeed  = 1.0f;   // множитель скорости машины (0..1)
    bool  stalled   = false;  // заглох
    float threat    = 0.0f;   // сглаженная шкала угрозы 0..1
    float bump      = 0.0f;   // импульс тряски (удар в мини-игре)

    void reset() { *this = SharedState(); }
};

}  // namespace lrh
