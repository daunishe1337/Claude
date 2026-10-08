// ============================================================================
//  RealWorldScene.cpp — «реальный мир»: вид с заднего сиденья (см. RealWorldScene.h).
//
//  Порядок отрисовки кадра:
//    1) салон «позади окон»: потолок, стойки, двери (фон, заранее «запечённый»
//       кадр с запасом по краям на тряску) и вмятина от монстра;
//    2) виды наружу: каждое окно заливается своим попиксельным «шейдером» и
//       одновременно помечается в трафарете своим id (лобовое, боковые, люк);
//    3) монстр и стекло (трещины, капли, блики) — строго внутри своего окна
//       (тест трафарета), поэтому ничего не вылезает на обивку;
//    4) передний план салона: уплотнители, приборная панель, зеркало,
//       родители, кресла (неподвижное тоже «запечено»; трафарет помечается
//       как «салон»); в бесконечном режиме родители оборачиваются к ребёнку
//       (facelessTurn) — безликие головы, перегибаясь, ложатся поверх кресел;
//    5) свет: тёплая полоса проезжающего фонаря и холодная — встречной машины;
//    6) пыль с потолка, консоль на коленях, пост-обработка (виньетка + зерно);
//    7) видоискатель, полароид и HUD — поверх всего, без пост-эффектов
//       (в бесконечном режиме вместо пути домой — пройденные километры).
//
//  Вся «косметика» (прокрутка пейзажа, фонари, тряска, пыль, полароид) живёт в
//  State и меняется только в update()/событиях; render() — константный и
//  детерминированный (шум и зерно — хэши от координат и времени).
//
//  Производительность: всё считается на CPU и в отладочной сборке тоже,
//  поэтому горячие циклы пишут прямо в буфер холста, без вызова функции на
//  пиксель, а в кадре не выделяется память.
// ============================================================================
#include "RealWorldScene.h"

#include "ConsoleMiniGame.h"
#include "Hud.h"
#include "Parents.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
//  Экран и трафарет
// ---------------------------------------------------------------------------
constexpr int kW = cfg::kScreenW;
constexpr int kH = cfg::kScreenH;

constexpr uint8_t kStLeft = 1;
constexpr uint8_t kStSunroof = 2;
constexpr uint8_t kStRight = 3;
constexpr uint8_t kStWindshield = 4;
constexpr uint8_t kStInterior = 8;

// ---------------------------------------------------------------------------
//  Геометрия окон (DESIGN.md: «Screen layout») — по ней же работает прицел.
// ---------------------------------------------------------------------------
const Vec2 kPolyLeft[4] = {{0.0f, 22.0f}, {46.0f, 30.0f}, {52.0f, 98.0f}, {0.0f, 104.0f}};
const Vec2 kPolyRight[4] = {{320.0f, 22.0f}, {274.0f, 30.0f}, {268.0f, 98.0f}, {320.0f, 104.0f}};
const Vec2 kPolySunroof[4] = {{112.0f, 0.0f}, {208.0f, 0.0f}, {192.0f, 22.0f}, {128.0f, 22.0f}};
const Vec2 kPolyWindshield[4] = {{92.0f, 34.0f}, {228.0f, 34.0f}, {250.0f, 84.0f}, {70.0f, 84.0f}};

// Те же окна, продлённые за край экрана и под панель, чтобы тряска не
// открывала щелей у границ кадра.
const Vec2 kDrawLeft[4] = {{-8.0f, 20.6f}, {46.0f, 30.0f}, {52.0f, 98.0f}, {-8.0f, 104.9f}};
const Vec2 kDrawRight[4] = {{328.0f, 20.6f}, {274.0f, 30.0f}, {268.0f, 98.0f}, {328.0f, 104.9f}};
const Vec2 kDrawSunroof[4] = {{106.2f, -8.0f}, {213.8f, -8.0f}, {192.0f, 22.0f}, {128.0f, 22.0f}};
const Vec2 kDrawWindshield[4] = {{92.0f, 34.0f}, {228.0f, 34.0f}, {251.8f, 88.0f}, {68.2f, 88.0f}};

// Стойки и передние боковые стёкла (левая сторона; правая — зеркально).
const Vec2 kBPillarL[4] = {{46.0f, 29.0f}, {57.0f, 30.0f}, {62.0f, 100.0f}, {52.0f, 98.0f}};
const Vec2 kFrontGlassL[4] = {{57.0f, 31.0f}, {85.0f, 34.0f}, {64.5f, 82.0f}, {60.5f, 82.0f}};
const Vec2 kAPillarL[4] = {{85.0f, 33.0f}, {92.0f, 33.0f}, {70.0f, 84.0f}, {62.0f, 84.0f}};
const Vec2 kDoorL[5] = {{-8.0f, 104.9f}, {52.0f, 98.0f}, {62.0f, 99.0f}, {68.0f, 188.0f}, {-8.0f, 188.0f}};

// Верхняя кромка левого окна: y на заданном x (рейлинг крыши).
inline float leftEdgeY(float x) { return 22.0f + x * (8.0f / 46.0f); }

// ---------------------------------------------------------------------------
//  Дорога и проекция (вид вперёд через лобовое стекло)
// ---------------------------------------------------------------------------
constexpr float kHorizonY = 50.0f;     // линия горизонта на экране
constexpr float kFocal = 60.0f;        // пикселей на метр на расстоянии 1 м
constexpr float kCamH = 1.2f;          // высота глаз над дорогой, м
constexpr float kCamLat = 1.2f;        // машина в правой полосе: смещение от осевой, м
constexpr float kRoadHalf = 2.4f;      // половина ширины дороги, м
constexpr float kMetersPerSec = 22.0f; // скорость при carSpeed = 1 (≈80 км/ч)

// ---------------------------------------------------------------------------
//  Фонари, встречные машины, тряска
// ---------------------------------------------------------------------------
constexpr int kMaxLamps = 4;
constexpr float kLampLead = 3.0f;       // за сколько секунд до проезда фонарь виден на горизонте
constexpr float kLampTail = 2.6f;       // сколько секунд фонарь ещё виден в зеркале
constexpr float kLampPoleLat = 6.5f;    // столб от осевой, м
constexpr float kLampHeadLat = 5.9f;    // плафон (короткий кронштейн над обочиной)
constexpr float kLampHeight = 5.5f;
constexpr float kCarClosing = 18.0f;    // собственная скорость встречной машины, м/с
constexpr float kShakeK = 900.0f;       // жёсткость «пружины» тряски
constexpr float kShakeDamp = 13.0f;
constexpr float kShakeImpulse = 140.0f; // скорость (пикс/с) от удара силы 1

constexpr int kMaxDust = 128;
constexpr int kMaxScratches = 8;
constexpr int kMaxCrackSegs = 120;
constexpr int kPhotoW = 64;
constexpr int kPhotoH = 36;
constexpr float kPhotoShow = 2.5f;      // сколько полароид висит до затухания
constexpr float kPhotoFade = 0.6f;
constexpr int kSideCols = 96;           // ширина «мира» бокового окна (+ переднее стекло)
constexpr int kGrainSize = 128;

// ---------------------------------------------------------------------------
//  Палитра
// ---------------------------------------------------------------------------
constexpr uint32_t kWarmLight = rgb(255, 176, 92);   // натриевый фонарь
constexpr uint32_t kColdLight = rgb(190, 205, 255);  // фары встречной / луна
constexpr uint32_t kTeal = rgb(40, 196, 176);        // подсветка приборов
constexpr uint32_t kPupilGlow = rgb(170, 255, 150);
constexpr uint32_t kPupilCore = rgb(240, 255, 225);
constexpr uint32_t kSkin = rgb(150, 166, 138);       // бледно-серо-зелёная кожа

// ---------------------------------------------------------------------------
//  Хэши, шум, дизеринг
// ---------------------------------------------------------------------------
// Пороги упорядоченного дизеринга (матрица Байера 4x4), уже нормированные в 0..1.
constexpr float kBayerF[4][4] = {
    {0.5f / 16.0f, 8.5f / 16.0f, 2.5f / 16.0f, 10.5f / 16.0f},
    {12.5f / 16.0f, 4.5f / 16.0f, 14.5f / 16.0f, 6.5f / 16.0f},
    {3.5f / 16.0f, 11.5f / 16.0f, 1.5f / 16.0f, 9.5f / 16.0f},
    {15.5f / 16.0f, 7.5f / 16.0f, 13.5f / 16.0f, 5.5f / 16.0f},
};

inline float bayerAt(int x, int y) {
    return kBayerF[static_cast<unsigned>(y) & 3u][static_cast<unsigned>(x) & 3u];
}

inline uint32_t mix32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}
inline uint32_t hashXY(int x, int y, uint32_t seed) {
    return mix32(static_cast<uint32_t>(x) * 0x8DA6B343u ^ static_cast<uint32_t>(y) * 0xD8163841u ^
                 seed * 0x9E3779B9u);
}
inline float hashXYf(int x, int y, uint32_t seed) {
    return static_cast<float>(hashXY(x, y, seed) >> 8) * (1.0f / 16777216.0f);
}
inline float hashIf(int i, uint32_t seed) { return hashXYf(i, 7919, seed); }

inline int floori(float v) { return static_cast<int>(std::floor(v)); }
inline float fractf(float v) { return v - std::floor(v); }
inline float easeOut3(float t) {
    t = saturate(t);
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

// Гладкий 2D value-noise в [0,1).
float noise2(float x, float y, uint32_t seed) {
    const float fx = std::floor(x), fy = std::floor(y);
    const int ix = static_cast<int>(fx), iy = static_cast<int>(fy);
    float tx = x - fx, ty = y - fy;
    tx = tx * tx * (3.0f - 2.0f * tx);
    ty = ty * ty * (3.0f - 2.0f * ty);
    const float a = hashXYf(ix, iy, seed), b = hashXYf(ix + 1, iy, seed);
    const float c = hashXYf(ix, iy + 1, seed), d = hashXYf(ix + 1, iy + 1, seed);
    return lerpf(lerpf(a, b, tx), lerpf(c, d, tx), ty);
}

// Выбор одного из двух цветов по порогу Байера — «пиксельный» градиент.
inline uint32_t dither2(uint32_t a, uint32_t b, float t, int x, int y) { return t > bayerAt(x, y) ? b : a; }

// Готовая строка градиента: два соседних цвета и доля второго (для быстрых
// попиксельных «шейдеров», где t зависит только от строки).
struct RampRow {
    uint32_t a = 0;
    uint32_t b = 0;
    float t = 0.0f;
};

RampRow rampRow(const uint32_t* ramp, int n, float t) {
    const float f = saturate(t) * static_cast<float>(n - 1);
    const int i = std::min(static_cast<int>(f), n - 2);
    RampRow r;
    r.a = ramp[i];
    r.b = ramp[i + 1];
    r.t = f - static_cast<float>(i);
    return r;
}

inline uint32_t rampPick(const RampRow& r, int x, int y) { return r.t > bayerAt(x, y) ? r.b : r.a; }

// Смешивание цветов в целых числах: t256 = 0 -> a, 256 -> b.
inline uint32_t mix256(uint32_t a, uint32_t b, int t256) {
    const int r = colR(a) + (((colR(b) - colR(a)) * t256) >> 8);
    const int g = colG(a) + (((colG(b) - colG(a)) * t256) >> 8);
    const int bl = colB(a) + (((colB(b) - colB(a)) * t256) >> 8);
    return rgb(r, g, bl);
}

// То же, но «по-пиксельному»: сплошные ступени и узкие дизеринг-переходы
// (так рисуют объёмы вручную; меньше шахматного шума на мелких формах).
uint32_t bandRamp(const uint32_t* ramp, int n, float t, int x, int y) {
    const float f = saturate(t) * static_cast<float>(n - 1);
    const int i = std::min(static_cast<int>(f), n - 2);
    const float fr = saturate((f - static_cast<float>(i) - 0.3f) / 0.4f);
    return dither2(ramp[i], ramp[i + 1], fr, x, y);
}

// Поверхность цвета base под светом цвета light силы k: умножение на свет
// плюс небольшая добавка (свет «видно» даже на почти чёрном).
inline uint32_t litColor(uint32_t base, uint32_t light, float k) {
    if (k <= 0.002f) return base;
    const float m = 2.4f * k * (1.0f / 255.0f);
    const float a = 0.10f * k;
    const float r = static_cast<float>(colR(base)) * (1.0f + m * static_cast<float>(colR(light))) +
                    a * static_cast<float>(colR(light));
    const float g = static_cast<float>(colG(base)) * (1.0f + m * static_cast<float>(colG(light))) +
                    a * static_cast<float>(colG(light));
    const float b = static_cast<float>(colB(base)) * (1.0f + m * static_cast<float>(colB(light))) +
                    a * static_cast<float>(colB(light));
    return rgb(std::min(255, static_cast<int>(r)), std::min(255, static_cast<int>(g)),
               std::min(255, static_cast<int>(b)));
}

// ---------------------------------------------------------------------------
//  Геометрия многоугольников (прицел мышью)
// ---------------------------------------------------------------------------
bool insidePoly(const Vec2* p, int n, float x, float y) {
    bool in = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if ((p[i].y > y) != (p[j].y > y)) {
            const float xc = p[j].x + (y - p[j].y) * (p[i].x - p[j].x) / (p[i].y - p[j].y);
            if (x < xc) in = !in;
        }
    }
    return in;
}

float distToSeg(float px, float py, Vec2 a, Vec2 b) {
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float len2 = dx * dx + dy * dy;
    const float t = len2 > 0.0f ? saturate(((px - a.x) * dx + (py - a.y) * dy) / len2) : 0.0f;
    const float ex = a.x + dx * t - px, ey = a.y + dy * t - py;
    return std::sqrt(ex * ex + ey * ey);
}

// 0 внутри, иначе расстояние до ближайшего ребра.
float distToPoly(const Vec2* p, int n, float x, float y) {
    if (insidePoly(p, n, x, y)) return 0.0f;
    float best = 1e9f;
    for (int i = 0; i < n; ++i) best = std::min(best, distToSeg(x, y, p[i], p[(i + 1) % n]));
    return best;
}

Vec2 centroid(const Vec2* p, int n) {
    Vec2 c;
    for (int i = 0; i < n; ++i) {
        c.x += p[i].x;
        c.y += p[i].y;
    }
    c.x /= static_cast<float>(n);
    c.y /= static_cast<float>(n);
    return c;
}

// Зеркальная копия многоугольника относительно центра экрана (лево <-> право).
template <int N>
std::array<Vec2, N> mirrorPoly(const Vec2 (&p)[N]) {
    std::array<Vec2, N> m{};
    for (int i = 0; i < N; ++i) m[static_cast<size_t>(i)] = Vec2{static_cast<float>(kW) - p[i].x, p[i].y};
    return m;
}

// Горизонтальный отрезок выпуклого многоугольника на строке y (центр пикселя).
bool polySpan(const Vec2* p, int n, float y, float& xl, float& xr) {
    xl = 1e9f;
    xr = -1e9f;
    for (int i = 0; i < n; ++i) {
        const Vec2& a = p[i];
        const Vec2& b = p[(i + 1) % n];
        if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) {
            const float x = a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y);
            xl = std::min(xl, x);
            xr = std::max(xr, x);
        }
    }
    return xr >= xl;
}

// Быстрое аддитивное свечение прямо в буфер (без вызова функции на пиксель).
// clip — необязательный выпуклый многоугольник (координаты сцены), вне
// которого свет не рисуется (например, «только в лобовом стекле»).
void addGlow(Canvas& out, float cx, float cy, float r, uint32_t col, float k, const Vec2* clip = nullptr,
             int clipN = 0) {
    if (r < 1.0f || k <= 0.003f) return;
    const int ox = out.offsetX(), oy = out.offsetY();
    const int W = out.width(), H = out.height();
    const int ya = std::max(0, floori(cy - r) + oy), yb = std::min(H - 1, floori(cy + r) + 1 + oy);
    // Цвет света с учётом силы (в 1/256), спад (1 - d²)² считается в целых.
    const int kr = roundi(static_cast<float>(colR(col)) * k), kg = roundi(static_cast<float>(colG(col)) * k),
              kb = roundi(static_cast<float>(colB(col)) * k);
    const float invR2 = 65536.0f / (r * r);
    uint32_t* px = out.data();
    for (int y = ya; y <= yb; ++y) {
        const float sy = static_cast<float>(y - oy) + 0.5f;
        const float dy = sy - cy;
        const float rem = r * r - dy * dy;
        if (rem <= 0.0f) continue;
        const float half = std::sqrt(rem);
        float xl = cx - half, xr = cx + half;
        if (clip) {
            float pl = 0.0f, pr = 0.0f;
            if (!polySpan(clip, clipN, sy, pl, pr)) continue;
            xl = std::max(xl, pl);
            xr = std::min(xr, pr);
        }
        const int xa = std::max(0, static_cast<int>(std::ceil(xl - 0.5f)) + ox);
        const int xb = std::min(W - 1, static_cast<int>(std::ceil(xr - 0.5f)) - 1 + ox);
        uint32_t* row = px + static_cast<size_t>(y) * static_cast<size_t>(W);
        // f(x) = 1 - (dx² + dy²)/r² в 1/65536; dx растёт на 1 за шаг.
        float dx = static_cast<float>(xa - ox) + 0.5f - cx;
        for (int x = xa; x <= xb; ++x, dx += 1.0f) {
            const int f16 = 65536 - static_cast<int>((dx * dx + dy * dy) * invR2);
            if (f16 <= 0) continue;
            const int f8 = f16 >> 8;
            const int w = (f8 * f8) >> 8; // 0..256
            const uint32_t c = row[x];
            int cr = static_cast<int>((c >> 16) & 255u) + ((kr * w) >> 8);
            int cg = static_cast<int>((c >> 8) & 255u) + ((kg * w) >> 8);
            int cb = static_cast<int>(c & 255u) + ((kb * w) >> 8);
            cr = cr > 255 ? 255 : cr;
            cg = cg > 255 ? 255 : cg;
            cb = cb > 255 ? 255 : cb;
            row[x] = (static_cast<uint32_t>(cr) << 16) | (static_cast<uint32_t>(cg) << 8) |
                     static_cast<uint32_t>(cb);
        }
    }
}

// Заливка выпуклого многоугольника попиксельной функцией fn(x, y, dst)
// (координаты холста). В отличие от Canvas::shadePolygon здесь нет вызова
// std::function на каждый пиксель: трафарет (если stencilId != 0) помечается
// обычной fillPolygon, а цвет пишется прямо в буфер по тем же правилам
// растеризации (центр пикселя внутри).
template <typename Fn>
void shadeConvex(Canvas& out, const Vec2* poly, int n, uint8_t stencilId, Fn&& fn) {
    if (stencilId != 0) {
        out.setStencilWrite(stencilId);
        out.fillPolygon(poly, n, 0u);
        out.setStencilWrite(0);
    }
    const int ox = out.offsetX(), oy = out.offsetY();
    const int W = out.width(), H = out.height();
    float minY = 1e9f, maxY = -1e9f;
    for (int i = 0; i < n; ++i) {
        minY = std::min(minY, poly[i].y);
        maxY = std::max(maxY, poly[i].y);
    }
    const int ya = std::max(0, floori(minY) + oy),
              yb = std::min(H - 1, static_cast<int>(std::ceil(maxY)) + oy);
    uint32_t* px = out.data();
    for (int y = ya; y <= yb; ++y) {
        float xl = 0.0f, xr = 0.0f;
        if (!polySpan(poly, n, static_cast<float>(y - oy) + 0.5f, xl, xr)) continue;
        const int xa = std::max(0, static_cast<int>(std::ceil(xl - 0.5f)) + ox);
        const int xb = std::min(W - 1, static_cast<int>(std::ceil(xr - 0.5f)) - 1 + ox);
        uint32_t* row = px + static_cast<size_t>(y) * static_cast<size_t>(W);
        for (int x = xa; x <= xb; ++x) row[x] = fn(x, y, row[x]);
    }
}

// ---------------------------------------------------------------------------
//  Проекция точки дороги (lat — от осевой, вправо +; height — над асфальтом)
// ---------------------------------------------------------------------------
struct Proj {
    float x = 0.0f;
    float y = 0.0f;
    float ppm = 0.0f; // пикселей на метр на этой дальности
};

inline float curveOffset(float curve, float z) {
    const float zz = std::min(z, 90.0f);
    return curve * zz * zz * 0.5f;
}

inline Proj project(float curve, float lat, float height, float z) {
    Proj p;
    const float zz = std::max(z, 0.4f);
    p.ppm = kFocal / zz;
    p.x = 160.0f + (lat - kCamLat + curveOffset(curve, zz)) * p.ppm;
    p.y = kHorizonY + (kCamH - height) * p.ppm;
    return p;
}

// Яркость света фонаря снаружи: d — секунд до проезда под ним (< 0 — позади).
inline float lampGlowAt(float d) {
    const float w = d >= 0.0f ? 0.42f : 0.30f;
    return std::exp(-(d * d) / (w * w));
}

// ---------------------------------------------------------------------------
//  Рисование «тел»: толстые отрезки, пальцы, ладони
// ---------------------------------------------------------------------------
void thickSeg(Canvas& out, float ax, float ay, float bx, float by, float w0, float w1, uint32_t c) {
    const float dx = bx - ax, dy = by - ay;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.01f) {
        out.fillCircle(roundi(ax), roundi(ay), roundi(w0 * 0.5f), c);
        return;
    }
    const float nx = -dy / len * 0.5f, ny = dx / len * 0.5f;
    const Vec2 q[4] = {{ax + nx * w0, ay + ny * w0}, {bx + nx * w1, by + ny * w1},
                       {bx - nx * w1, by - ny * w1}, {ax - nx * w0, ay - ny * w0}};
    out.fillPolygon(q, 4, c);
    // Центральная линия гарантирует непрерывность очень тонких сегментов.
    out.drawLine(roundi(ax - 0.5f), roundi(ay - 0.5f), roundi(bx - 0.5f), roundi(by - 0.5f), c);
}

// Повёрнутый эллипс (заливка), ang — направление большой оси rx.
void fillRotEllipse(Canvas& out, float cx, float cy, float rx, float ry, float ang, uint32_t c) {
    const float ca = std::cos(ang), sa = std::sin(ang);
    const float ext = std::max(rx, ry) + 1.0f;
    for (int y = floori(cy - ext); y <= floori(cy + ext); ++y) {
        for (int x = floori(cx - ext); x <= floori(cx + ext); ++x) {
            const float dx = static_cast<float>(x) + 0.5f - cx, dy = static_cast<float>(y) + 0.5f - cy;
            const float u = (dx * ca + dy * sa) / rx, v = (-dx * sa + dy * ca) / ry;
            if (u * u + v * v <= 1.0f) out.plot(x, y, c);
        }
    }
}

struct LimbStyle {
    uint32_t skin = kSkin;
    uint32_t dark = rgb(20, 24, 22);
    uint32_t claw = rgb(34, 30, 26);
};

// Длинный палец: три фаланги, сужается к концу, на конце — коготь.
// ang — направление (рад), curl — доворот на каждом суставе.
void drawFinger(Canvas& out, float bx, float by, float ang, float len, float curl, float w,
                const LimbStyle& st) {
    if (len < 0.8f) return;
    const float parts[3] = {0.40f, 0.34f, 0.26f};
    Vec2 p[4];
    p[0] = Vec2{bx, by};
    float a = ang;
    for (int k = 0; k < 3; ++k) {
        p[k + 1] = Vec2{p[k].x + std::cos(a) * len * parts[k], p[k].y + std::sin(a) * len * parts[k]};
        a += curl;
    }
    // Сначала тёмный контур (шире), затем кожа — пальцы не сливаются друг с другом.
    const float widths[4] = {w, w * 0.9f, w * 0.75f, w * 0.55f};
    for (int pass = 0; pass < 2; ++pass) {
        const float extra = pass == 0 ? 1.1f : 0.0f;
        const uint32_t col = pass == 0 ? st.dark : st.skin;
        for (int k = 0; k < 3; ++k)
            thickSeg(out, p[k].x, p[k].y, p[k + 1].x, p[k + 1].y, widths[k] + extra, widths[k + 1] + extra,
                     col);
    }
    // Костяшки — чуть светлее.
    for (int k = 1; k < 3; ++k) out.plot(floori(p[k].x), floori(p[k].y), scaleColor(st.skin, 1.18f));
    // Коготь: острый, костяного цвета, продолжает последнюю фалангу.
    const Vec2 tip = p[3];
    const float cl = std::max(1.5f, len * 0.24f);
    const float ex = tip.x + std::cos(a) * cl, ey = tip.y + std::sin(a) * cl;
    thickSeg(out, tip.x, tip.y, ex, ey, std::max(1.0f, w * 0.7f), 0.3f, st.claw);
}

// Ладонь, прижатая к стеклу: растопыренные пальцы в направлении ang.
void drawPalm(Canvas& out, float cx, float cy, float s, float ang, const LimbStyle& st, float press) {
    for (int i = 0; i < 4; ++i) {
        const float fi = static_cast<float>(i) - 1.5f;
        const float a = ang + fi * 0.36f;
        const float bxp = cx + std::cos(a) * 3.6f * s, byp = cy + std::sin(a) * 3.6f * s;
        const float len = (i == 1 || i == 2 ? 12.0f : 10.0f) * s;
        drawFinger(out, bxp, byp, a, len, -fi * 0.06f, 1.7f * s, st);
    }
    const float ta = ang + 1.45f;
    drawFinger(out, cx + std::cos(ta) * 3.0f * s, cy + std::sin(ta) * 3.0f * s, ta - 0.25f, 6.5f * s, -0.2f,
               2.0f * s, st);
    fillRotEllipse(out, cx, cy, 4.6f * s, 4.0f * s, ang, st.dark);
    fillRotEllipse(out, cx, cy, 3.7f * s, 3.1f * s, ang, st.skin);
    // Прижатая к стеклу кожа светлеет.
    if (press > 0.0f) {
        fillRotEllipse(out, cx + std::cos(ang) * 0.8f, cy + std::sin(ang) * 0.8f, 2.0f * s, 1.6f * s, ang,
                       lerpColor(st.skin, rgb(225, 232, 214), 0.35f * press));
    }
}

// ---------------------------------------------------------------------------
//  Голова монстра: попиксельная «SDF»-картинка в системе головы (масштаб,
//  поворот, вниз головой, свет), чтобы одна функция рисовала и маленькое лицо
//  в окне, и скример на весь экран.
// ---------------------------------------------------------------------------
struct Face {
    float cx = 0.0f, cy = 0.0f;  // центр черепа на экране
    float r = 12.0f;             // радиус черепа, пикс
    float angle = 0.0f;          // поворот (kPi — вниз головой)
    float mouth = 0.0f;          // 0 — сомкнутая ухмылка, 1 — пасть настежь
    float squint = 0.0f;         // прищур (от вспышки)
    float blink = 0.0f;          // веки
    float light = 0.4f;          // освещённость 0..1
    uint32_t lightCol = kColdLight;
    float lightDirX = 0.0f;      // направление НА источник света (экран)
    float lightDirY = -1.0f;
    float lookX = 0.0f;          // взгляд (экран), -1..1
    float lookY = 0.0f;
    float pupil = 1.0f;          // яркость зрачков
    bool detail = false;         // вены и фактура кожи (крупный план)
};

// Ступени цвета кожи (от тени к блику): лицо рисуется дизерингом между ними.
const uint32_t kSkinRamp[6] = {rgb(12, 15, 15), rgb(32, 38, 35), rgb(60, 70, 61),
                               rgb(98, 112, 94), rgb(146, 160, 134), rgb(198, 208, 184)};

// Полуширина челюсти на высоте v (сужается к острому подбородку chinV).
inline float jawHalfWidth(float v, float chinV) {
    const float t = saturate((v + 0.2f) / (chinV + 0.2f));
    const float t2 = t * t * (0.8f + 0.2f * t); // ≈ t^2.2 без pow
    return 0.88f * std::sqrt(std::max(0.0f, 1.0f - t2)) * (1.0f - 0.3f * t);
}

void drawFace(Canvas& out, const Face& f) {
    const float r = std::max(f.r, 2.0f);
    const float ca = std::cos(f.angle), sa = std::sin(f.angle);
    const float inv = 1.0f / r;
    const float chinV = 1.12f + 0.55f * f.mouth;
    const float ext = r * (1.4f + 0.55f * f.mouth);
    const int xa = std::max(floori(f.cx - ext), -out.offsetX());
    const int xb = std::min(floori(f.cx + ext) + 1, out.width() - 1 - out.offsetX());
    const int ya = std::max(floori(f.cy - ext), -out.offsetY());
    const int yb = std::min(floori(f.cy + ext) + 1, out.height() - 1 - out.offsetY());

    // Черты лица в системе головы (v вниз, 1 — радиус черепа).
    const float mouthW = 0.76f;
    const float mouthY = 0.68f + 0.22f * f.mouth;
    // Сомкнутая ухмылка — частые тонкие зубы; открытая пасть — редкие клыки.
    const float teeth = f.mouth < 0.08f ? clampf(r * 0.5f, 5.0f, 14.0f) : clampf(r * 0.3f, 4.0f, 11.0f);
    const float eyeRx = 0.34f, eyeRy0 = 0.23f;
    const float eyeRy = eyeRy0 * (1.0f - 0.82f * f.squint) * (1.0f - f.blink);
    const float tiltC = 0.90f, tiltS = 0.44f; // внешние уголки глаз вверх

    // Направление света (экран -> система головы), z — «на зрителя».
    float lu = f.lightDirX * ca + f.lightDirY * sa;
    float lv = -f.lightDirX * sa + f.lightDirY * ca;
    const float ln = std::sqrt(lu * lu + lv * lv + 0.64f);
    lu /= ln;
    lv /= ln;
    const float lz = 0.8f / ln;
    const float lightScale = 0.68f + 0.5f * f.light;
    const uint32_t tint = lerpColor(rgb(255, 255, 255), f.lightCol, 0.45f);
    uint32_t ramp[6];
    for (int i = 0; i < 6; ++i) ramp[i] = mulColor(kSkinRamp[i], tint);
    const uint32_t toothCol = scaleColor(rgb(230, 224, 196), 0.45f + 0.55f * f.light);
    const float edge = 1.3f * inv;

    // Крупное лицо (скример) считается по сетке 2x2: черты всё равно огромные,
    // а работы вчетверо меньше.
    const int step = r > 40.0f ? 2 : 1;
    const float half = 0.5f * static_cast<float>(step);
    for (int py = ya; py <= yb; py += step) {
        for (int px = xa; px <= xb; px += step) {
            const float dx = (static_cast<float>(px) + half - f.cx) * inv;
            const float dy = (static_cast<float>(py) + half - f.cy) * inv;
            const float u = dx * ca + dy * sa;
            const float v = -dx * sa + dy * ca;
            const float au = std::fabs(u);
            // Силуэт: высокий купол черепа + челюсть, сужающаяся к подбородку.
            const float cv = (v + 0.22f) / 1.08f;
            const float dC = std::sqrt(u * u + cv * cv) - 1.0f;
            float dJ = 1e3f;
            if (v > -0.2f && v < chinV) dJ = (au - jawHalfWidth(v, chinV)) * 0.85f;
            const float d = std::min(dC, dJ);
            if (d > 0.0f) continue;

            // Нормаль: купол — сфера, челюсть — цилиндр.
            float nx, ny;
            if (dC <= dJ) {
                nx = u;
                ny = cv;
            } else {
                const float w = std::max(0.05f, jawHalfWidth(v, chinV));
                nx = clampf(u / w, -1.0f, 1.0f) * 0.9f;
                ny = 0.25f;
            }
            const float nz = std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));
            float shade = 0.32f + 0.75f * std::max(0.0f, nx * lu + ny * lv + nz * lz);
            bool solid = false;
            uint32_t col = 0;

            // Впалые виски.
            if (au > 0.72f && v > -0.45f && v < 0.2f) shade *= 0.78f;
            // Огромные чёрные глаза в глубоких глазницах.
            for (int side = -1; side <= 1; side += 2) {
                const float sx = static_cast<float>(side);
                const float tx = u - 0.42f * sx, ty = v + 0.02f;
                const float ts = tiltS * sx;
                const float exx = tx * tiltC - ty * ts;
                const float eyy = tx * ts + ty * tiltC;
                const float sock =
                    exx * exx * (1.0f / (0.48f * 0.48f)) + eyy * eyy * (1.0f / (0.36f * 0.36f));
                if (sock < 1.0f) shade *= 0.42f + 0.58f * sock;
                if (eyeRy > 0.025f) {
                    const float e = exx * exx / (eyeRx * eyeRx) + eyy * eyy / (eyeRy * eyeRy);
                    if (e < 1.0f) {
                        col = rgb(3, 3, 6);
                        // Влажный блик у верхнего внутреннего края.
                        if (r > 11.0f && e > 0.45f && e < 0.75f && eyy < 0.0f && exx * sx < 0.0f)
                            col = rgb(38, 44, 52);
                        solid = true;
                    }
                } else if (std::fabs(eyy) < 0.04f && std::fabs(exx) < eyeRx) {
                    col = rgb(10, 9, 10);
                    solid = true;
                }
            }
            // Скулы и провалы щёк под ними.
            {
                const float hu = au - 0.50f, hv = v - 0.40f;
                const float q = hu * hu * 2.2f + hv * hv * 3.0f;
                if (q < 0.12f) shade *= 0.55f + 0.45f * q / 0.12f;
                const float bu = au - 0.66f, bv = v - 0.20f;
                if (bu * bu + bv * bv * 2.0f < 0.012f) shade *= 1.15f;
            }
            // Ноздри-щёлки вместо носа.
            if (r > 9.0f) {
                const float nu = (au - 0.08f) / 0.035f, nv = (v - 0.36f) / 0.07f;
                if (nu * nu + nv * nv < 1.0f) shade *= 0.3f;
            }
            // Ухмылка до ушей: тонкие губы, частые тонкие зубы.
            if (!solid && au < mouthW) {
                const float t = u / mouthW;
                const float ym = mouthY - 0.36f * t * t;
                const float hm = 0.075f * (1.0f - 0.55f * t * t) + f.mouth * 0.42f * (1.0f - t * t);
                const float dv = v - ym;
                if (std::fabs(dv) < hm) {
                    const float prof = std::fabs(fractf(u * teeth + 0.5f) - 0.5f) * 2.0f;
                    if (f.mouth < 0.08f) {
                        col = prof < 0.5f ? toothCol : rgb(8, 3, 4);
                    } else {
                        const float tl = std::min(hm, 0.07f + 0.13f * f.mouth);
                        const float fromTop = (dv + hm) / tl, fromBot = (hm - dv) / tl;
                        const bool tooth = (fromTop < 1.0f && prof < 0.85f * (1.0f - fromTop) + 0.1f) ||
                                           (fromBot < 1.0f && prof < 0.85f * (1.0f - fromBot) + 0.1f);
                        if (tooth) {
                            col = scaleColor(toothCol, 0.75f + 0.25f * (1.0f - std::min(fromTop, fromBot)));
                        } else {
                            const float deep = saturate(1.0f - std::fabs(dv) / hm);
                            col = lerpColor(rgb(46, 8, 12), rgb(5, 1, 2), deep);
                        }
                    }
                    solid = true;
                } else if (std::fabs(dv) < hm + 0.07f) {
                    shade *= 0.4f; // тень губ
                }
            }
            // Складки от уголков рта вверх, к скулам — улыбка ещё шире.
            if (!solid && au > mouthW * 0.85f && au < mouthW + 0.12f && v > mouthY - 0.72f &&
                v < mouthY - 0.28f) {
                const float sxs = u < 0.0f ? -1.0f : 1.0f;
                const Vec2 a{sxs * mouthW * 0.97f, mouthY - 0.35f};
                const Vec2 b{sxs * (mouthW + 0.07f), mouthY - 0.66f};
                if (distToSeg(u, v, a, b) < 0.035f) shade *= 0.45f;
            }
            // Крупный план: вены на черепе и неровная кожа.
            if (f.detail) {
                if (v < -0.25f) {
                    // Треугольные волны вместо синусов — дёшево и достаточно «органично».
                    const float bend = std::fabs(fractf(v * 0.8f) - 0.5f) * 4.0f - 1.0f;
                    const float w = std::fabs(fractf(u * 1.1f + bend * 0.3f) - 0.5f);
                    if (w < 0.012f) shade *= 0.72f;
                }
                shade *= 0.92f + 0.16f * hashXYf(floori(u * 18.0f), floori(v * 18.0f), 31u);
            }
            if (!solid) {
                float level = saturate(shade * lightScale);
                // Контур силуэта; со стороны света — светлая кромка.
                if (d > -edge) {
                    const float rim = nx * lu + ny * lv;
                    level = rim > 0.4f && f.light > 0.3f ? std::max(level, 0.85f) : level * 0.35f;
                }
                col = bandRamp(ramp, 6, level, (px + out.offsetX()) / step, (py + out.offsetY()) / step);
            }
            if (step == 1) out.plot(px, py, col);
            else out.fillRect(px, py, 2, 2, col);
        }
    }

    // Зрачки: крошечные светящиеся точки — видны и в полной темноте.
    if (f.squint < 0.6f && f.blink < 0.5f) {
        for (int side = -1; side <= 1; side += 2) {
            const float u = 0.42f * static_cast<float>(side), v = -0.02f;
            float sx = f.cx + (u * ca - v * sa) * r;
            float sy = f.cy + (u * sa + v * ca) * r;
            sx += f.lookX * 0.12f * r;
            sy += f.lookY * eyeRy * 0.35f * r;
            const int ix = floori(sx), iy = floori(sy);
            out.glow(ix, iy, std::max(2, roundi(r * 0.13f)), kPupilGlow, 0.4f * f.pupil);
            if (r < 24.0f) out.plot(ix, iy, kPupilCore);
            else out.fillCircle(ix, iy, std::max(1, roundi(r * 0.03f)), kPupilCore);
        }
    }
}

// ---------------------------------------------------------------------------
//  Трещины, частицы и прочие элементы состояния
// ---------------------------------------------------------------------------
struct CrackSeg {
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
    float t0 = 0.0f, t1 = 1.0f; // при каком значении crack() отрезок начинает и заканчивает расти
};

struct Lamp {
    bool alive = false;
    float d = 0.0f;    // секунд (при полной скорости) до проезда под фонарём; < 0 — позади
    float side = 1.0f; // -1 — слева от дороги, 1 — справа
};

struct Dust {
    bool alive = false;
    float x = 0.0f, y = 0.0f, vx = 0.0f, vy = 0.0f;
    float life = 0.0f, maxLife = 1.0f;
    bool big = false;
};

struct Scratch {
    bool alive = false;
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
    float age = 0.0f;
};

// Участок дороги, где лес подходит вплотную (метры пути).
struct Zone {
    float start = -1.0f, end = -1.0f;
};

// Как выглядит монстр в окне в этом кадре.
enum class Pose { Peek, Break, Recoil, Withdraw };

// Неподвижные части салона «запекаются» один раз при создании сцены и потом
// только копируются. Слой переднего плана хранится отрезками строк.
struct LayerRun {
    int y = 0;
    int x0 = 0;
    int len = 0;
    size_t offset = 0; // начало пикселей отрезка в BakedLayer::pix
};

struct BakedLayer {
    std::vector<LayerRun> runs;
    std::vector<uint32_t> pix;
};

// Биты карты звёзд: звёзды неподвижны относительно экрана (они бесконечно далеко).
constexpr uint8_t kStarFront = 1; // лобовое стекло (редкие, тусклые)
constexpr uint8_t kStarSide = 2;  // боковые окна
constexpr uint8_t kStarUp = 4;    // люк (гуще всего)

// Фон салона хранится целым кадром с запасом по краям на тряску.
constexpr int kBackMargin = 8;
constexpr int kBackW = kW + 2 * kBackMargin;
constexpr int kBackH = kH + 2 * kBackMargin;

// ============================================================================
//  Данные сцены (RealWorldScene::State наследует их, чтобы свободные функции
//  отрисовки могли работать с ними без доступа к закрытым членам класса)
// ============================================================================
struct SceneData {
    Rng rng;
    uint32_t seed = 1;
    float time = 0.0f;
    float speed = 1.0f;
    float dist = 0.0f;  // пройдено, м
    float curve = 0.0f; // кривизна дороги (сглаженная)

    // Фонари и «лес» (длинные тёмные участки).
    std::array<Lamp, kMaxLamps> lamps{};
    float lampTimer = 1.5f;
    bool lampPassed = false;
    std::array<Zone, 4> forest{};
    int forestNext = 0;

    // Встречная машина.
    bool carAlive = false;
    float carZ = 0.0f;
    float carTimer = 14.0f;

    // Освещённость снаружи 0..1 (для монстра и звука).
    float light = 0.0f;

    // Тряска: затухающая пружина + микровибрация двигателя.
    float shX = 0.0f, shY = 0.0f, shVX = 0.0f, shVY = 0.0f;
    float bumpTimer = 2.0f;
    float bump2Timer = -1.0f;
    float bump2Amount = 0.0f;
    int offX = 0, offY = 0;

    // Монстр (для вмятины/пыли/позы).
    float monX = 0.0f, monZ = 0.5f;
    bool monOnRoof = false;
    float dentX = 160.0f, dentY = 0.0f, dentAmt = 0.0f, dentKick = 0.0f;
    float crawlDX = 1.0f, crawlDY = 0.0f;
    Entry poseEntry = Entry::None; // у какого окна монстр был виден перед вспышкой
    float poseAppear = 0.0f;
    bool wasOverSunroof = false;

    std::array<Dust, kMaxDust> dust{};
    std::array<Scratch, kMaxScratches> scratches{};
    int scratchNext = 0;

    // Трещины: Left, Sunroof, Right.
    std::array<std::array<CrackSeg, kMaxCrackSegs>, 3> cracks{};
    std::array<int, 3> crackCount{};
    std::array<Vec2, 3> crackOrigin{};

    // «Ёлочка»-ароматизатор под зеркалом (маятник).
    float pendA = 0.0f, pendV = 0.0f;

    // Полароид.
    std::array<uint32_t, kPhotoW * kPhotoH> photo{};
    bool photoActive = false;
    float photoT = 0.0f;
    bool photoHit = false;

    // «Запечённые» неподвижные части салона.
    std::vector<uint32_t> backPix; // фон: потолок, стойки, двери (kBackW x kBackH)
    BakedLayer dashLayer;          // приборная панель
    BakedLayer seatLayer;          // кресла, тоннель, колени
    BakedLayer seatLayerLit;       // то же, подсвеченное экраном консоли на коленях
    std::vector<uint8_t> stars;    // карта звёзд kW x kH: биты kStar* — в каком небе есть звезда

    // Таблицы пост-обработки и фон скримера (строятся один раз).
    std::vector<uint16_t> vignette;
    std::array<int8_t, kGrainSize * kGrainSize> grain{};
    std::vector<uint32_t> jumpBg;
};


// Доля «леса» в точке пути zw (метры), с мягкими краями.
float forestAmount(const SceneData& s, float zw) {
    float best = 0.0f;
    for (const Zone& z : s.forest) {
        if (z.end <= z.start) continue;
        const float a = smoothstep(z.start - 25.0f, z.start + 10.0f, zw);
        const float b = 1.0f - smoothstep(z.end - 10.0f, z.end + 25.0f, zw);
        best = std::max(best, std::min(a, b));
    }
    return best;
}

// Детерминированный узор трещин: лучи от точки удара, ветки, «паутина».
void buildCracks(SceneData& s, uint32_t seed) {
    const Vec2 origins[3] = {{31.0f, 74.0f}, {160.0f, 11.0f}, {289.0f, 74.0f}};
    for (int w = 0; w < 3; ++w) {
        Rng r(seed * 2654435761u + static_cast<uint32_t>(w) * 977u + 13u);
        auto& segs = s.cracks[static_cast<size_t>(w)];
        int n = 0;
        const Vec2 o = origins[w];
        s.crackOrigin[static_cast<size_t>(w)] = o;
        const float squash = w == 1 ? 0.55f : 1.0f; // люк низкий: трещины расходятся вширь
        auto add = [&](Vec2 a, Vec2 b, float t0, float t1) {
            if (n >= kMaxCrackSegs) return;
            CrackSeg& c = segs[static_cast<size_t>(n++)];
            c.x0 = a.x;
            c.y0 = a.y;
            c.x1 = b.x;
            c.y1 = b.y;
            c.t0 = t0;
            c.t1 = t1;
        };
        const int rays = r.rangeInt(7, 10);
        std::array<std::array<Vec2, 6>, 10> pts{};
        std::array<int, 10> ptCount{};
        for (int i = 0; i < rays; ++i) {
            float ang = (static_cast<float>(i) + r.range(-0.3f, 0.3f)) * kTau / static_cast<float>(rays);
            const float rayT0 = 0.03f + r.next01() * 0.36f;
            Vec2 p = o;
            pts[static_cast<size_t>(i)][0] = p;
            const int nseg = r.rangeInt(3, 5);
            for (int k = 0; k < nseg; ++k) {
                ang += r.range(-0.35f, 0.35f);
                const float len = r.range(4.0f, 8.5f);
                const Vec2 q{p.x + std::cos(ang) * len, p.y + std::sin(ang) * len * squash};
                const float t0 = rayT0 + static_cast<float>(k) * 0.08f;
                add(p, q, t0, t0 + 0.1f);
                if (k >= 1 && r.chance(0.45f)) {
                    float ba = ang + (r.chance(0.5f) ? 0.7f : -0.7f);
                    Vec2 bp = q;
                    for (int b = 0; b < 2; ++b) {
                        const float bl = r.range(2.5f, 5.5f);
                        const Vec2 bq{bp.x + std::cos(ba) * bl, bp.y + std::sin(ba) * bl * squash};
                        const float bt = t0 + 0.14f + static_cast<float>(b) * 0.06f;
                        add(bp, bq, bt, bt + 0.08f);
                        bp = bq;
                        ba += r.range(-0.4f, 0.4f);
                    }
                }
                p = q;
                if (k + 1 < 6) {
                    pts[static_cast<size_t>(i)][static_cast<size_t>(k + 1)] = q;
                    ptCount[static_cast<size_t>(i)] = k + 2;
                }
            }
        }
        // Кольца «паутины» между соседними лучами.
        for (int ring = 1; ring <= 2; ++ring) {
            for (int i = 0; i < rays; ++i) {
                const int j = (i + 1) % rays;
                if (ptCount[static_cast<size_t>(i)] <= ring || ptCount[static_cast<size_t>(j)] <= ring)
                    continue;
                if (!r.chance(0.7f)) continue;
                const float t0 = 0.48f + 0.16f * static_cast<float>(ring) + r.range(0.0f, 0.08f);
                add(pts[static_cast<size_t>(i)][static_cast<size_t>(ring)],
                    pts[static_cast<size_t>(j)][static_cast<size_t>(ring)], t0, t0 + 0.08f);
            }
        }
        s.crackCount[static_cast<size_t>(w)] = n;
    }
}

// ============================================================================
//  Контекст кадра: всё, что считается один раз и нужно многим слоям
// ============================================================================
struct Frame {
    const SceneData* s = nullptr;
    const uint8_t* stars = nullptr; // карта звёзд (см. kStar*)
    const RealWorldView* v = nullptr;
    int ox = 0, oy = 0;
    float time = 0.0f;
    float engine = 1.0f;   // подсветка приборов
    float beams = 1.0f;    // свои фары
    float amb = 1.0f;      // общая освещённость салона
    float light = 0.0f;    // свет фонаря снаружи 0..1
    float frontLight = 0.0f; // фонарь впереди: свет через лобовое стекло
    float sideLightL = 0.0f, sideLightR = 0.0f; // свет снаружи у боковых окон
    float carGlare = 0.0f; // фары встречной (слепят)
    float carSideL = 0.0f; // встречная проносится слева
    float forestHere = 0.0f;
    float houseZ = 1e9f;   // дом при приезде, м
    float monLight = 0.3f; // освещённость монстра
    uint32_t monLightCol = kColdLight;
};

// Есть ли звезда указанного вида в точке сцены (x, y).
inline bool starAt(const Frame& f, int x, int y, uint8_t kind) {
    if (static_cast<unsigned>(x) >= static_cast<unsigned>(kW) ||
        static_cast<unsigned>(y) >= static_cast<unsigned>(kH))
        return false;
    return (f.stars[static_cast<size_t>(y) * kW + static_cast<size_t>(x)] & kind) != 0;
}

// ============================================================================
//  Вид через лобовое стекло
// ============================================================================
// Палитры дороги: по 8 ступеней освещённости фарами (строятся один раз).
struct RoadPalette {
    std::array<uint32_t, 8> asphalt{}, asphaltLo{}, asphaltHi{}, paint{}, edge{}, vergeA{}, vergeB{},
        ground{};
};

RoadPalette makeRoadPalette() {
    RoadPalette p;
    for (int i = 0; i < 8; ++i) {
        const float l = static_cast<float>(i) / 7.0f;
        const size_t k = static_cast<size_t>(i);
        p.asphalt[k] = lerpColor(rgb(16, 16, 22), rgb(122, 110, 92), l);
        p.asphaltLo[k] = scaleColor(p.asphalt[k], 0.9f);
        p.asphaltHi[k] = scaleColor(p.asphalt[k], 1.1f);
        p.paint[k] = lerpColor(rgb(36, 36, 40), rgb(214, 206, 176), std::min(1.0f, l * 1.3f));
        p.edge[k] = scaleColor(p.paint[k], 0.8f);
        p.vergeA[k] = lerpColor(rgb(9, 11, 9), rgb(62, 66, 42), l * 0.65f);
        p.vergeB[k] = lerpColor(rgb(9, 11, 9), rgb(62, 66, 42), l);
        p.ground[k] = lerpColor(rgb(6, 7, 8), rgb(30, 34, 24), l * 0.6f);
    }
    return p;
}

const RoadPalette& roadPalette() {
    static const RoadPalette p = makeRoadPalette();
    return p;
}

// Всё, что для строки лобового стекла считается один раз.
struct WindshieldRow {
    float invPpm = 0.0f;   // метров на пиксель
    float cx = 160.0f;     // осевая на экране
    float beam = 0.0f;     // яркость фар на этой дальности
    float invCone2 = 0.0f; // 1 / ширина_конуса^2
    float lineW = 0.0f;    // полуширина разметки, м
    float edgeW = 0.0f;
    int fog = 0;           // 0..256 — дымка
    int texZ = 0;          // клетка текстуры асфальта по дальности
    int vergeZ = 0;
    bool dash = false;     // на этой дальности есть штрих осевой
    bool tex = false;      // текстура асфальта видна (близко)
    bool yard = false;     // дальше дома: дорога кончилась
    RampRow sky;
};

struct WindshieldShade {
    const Frame* f = nullptr;
    const RoadPalette* pal = nullptr;
    WindshieldRow rows[kH];
    int16_t treeTop[kW] = {};
};

const uint32_t kSkyFront[] = {rgb(9, 10, 25), rgb(13, 14, 33), rgb(19, 19, 42), rgb(28, 26, 51),
                              rgb(40, 33, 58)};
constexpr uint32_t kFogCol = rgb(16, 15, 24);

uint32_t shadeWindshieldPx(const WindshieldShade& w, int x, int y) {
    const Frame& f = *w.f;
    const int lx = x - f.ox, ly = clampi(y - f.oy, 0, kH - 1);
    const WindshieldRow& r = w.rows[static_cast<size_t>(ly)];
    if (ly < static_cast<int>(kHorizonY)) {
        if (lx >= 0 && lx < kW && ly >= w.treeTop[static_cast<size_t>(lx)]) {
            // Лес на горизонте; у самой земли чуть светлее — дымка.
            return ly >= static_cast<int>(kHorizonY) - 1 ? rgb(12, 12, 20) : rgb(6, 8, 12);
        }
        uint32_t c = rampPick(r.sky, x, y);
        // Редкие тусклые звёзды (фары и стекло «съедают» большинство).
        if (ly < 45 && starAt(f, lx, ly, kStarFront)) {
            const float tw = 0.5f + 0.5f * std::sin(f.time * 2.3f + static_cast<float>(lx));
            c = addColor(c, scaleColor(rgb(120, 125, 150), 0.4f + 0.4f * tw));
        }
        return c;
    }
    if (r.yard) return mix256(rgb(12, 13, 14), kFogCol, r.fog);
    const RoadPalette& p = *w.pal;
    const float lat = (static_cast<float>(lx) + 0.5f - r.cx) * r.invPpm;
    const float lc = lat - kCamLat;
    const float q = lc * lc * r.invCone2;
    const float cone = 1.0f / (1.0f + q + 0.5f * q * q); // ≈ exp(-q)
    const float beam = r.beam * (0.15f + 0.85f * cone);
    // Свет фар квантуется с дизерингом — ступенчатый «пиксельный» конус.
    const size_t li = static_cast<size_t>(clampi(static_cast<int>(beam * 7.0f + bayerAt(x, y)), 0, 7));
    const float al = std::fabs(lat);
    uint32_t c;
    if (al <= kRoadHalf) {
        if (al > kRoadHalf - r.edgeW) {
            c = p.edge[li];
        } else if (r.dash && al < r.lineW) {
            c = p.paint[li];
        } else if (r.tex) {
            const uint32_t h = hashXY(r.texZ, floori(lat * 5.0f), 7u) % 3u;
            c = h == 0u ? p.asphaltLo[li] : (h == 1u ? p.asphalt[li] : p.asphaltHi[li]);
        } else {
            c = p.asphalt[li];
        }
    } else if (al < kRoadHalf + 2.4f) {
        c = (hashXY(r.vergeZ, floori(lat * 2.0f), 9u) & 1u) ? p.vergeA[li] : p.vergeB[li];
    } else {
        c = p.ground[li];
    }
    return r.fog > 0 ? mix256(c, kFogCol, r.fog) : c;
}

// Хвойное дерево (силуэт ярусами) в мире: lat от осевой, z — дальность, h — высота.
void drawConifer(Canvas& out, float curve, float lat, float z, float h, uint32_t col, uint32_t litCol,
                 float litK) {
    const Proj base = project(curve, lat, 0.0f, z);
    const Proj top = project(curve, lat, h, z);
    const float w = 1.7f * base.ppm;
    const float H = base.y - top.y;
    if (H < 1.5f) return;
    const float cx = base.x;
    const float ty = top.y;
    const Vec2 pts[11] = {
        {cx, ty},
        {cx + w * 0.42f, ty + H * 0.32f},
        {cx + w * 0.22f, ty + H * 0.30f},
        {cx + w * 0.72f, ty + H * 0.62f},
        {cx + w * 0.40f, ty + H * 0.60f},
        {cx + w * 1.00f, ty + H * 0.92f},
        {cx - w * 1.00f, ty + H * 0.92f},
        {cx - w * 0.40f, ty + H * 0.60f},
        {cx - w * 0.72f, ty + H * 0.62f},
        {cx - w * 0.22f, ty + H * 0.30f},
        {cx - w * 0.42f, ty + H * 0.32f},
    };
    out.fillPolygon(pts, 11, col);
    // Ствол.
    const float tw = std::max(1.0f, 0.18f * base.ppm);
    out.fillRect(roundi(cx - tw * 0.5f), roundi(ty + H * 0.9f), std::max(1, roundi(tw)),
                 std::max(1, roundi(H * 0.1f) + 1), col);
    // Нижние лапы, задетые светом фар.
    if (litK > 0.02f) {
        const Vec2 lowPts[4] = {{cx - w * 1.0f, ty + H * 0.92f},
                                {cx - w * 0.55f, ty + H * 0.72f},
                                {cx + w * 0.55f, ty + H * 0.72f},
                                {cx + w * 1.0f, ty + H * 0.92f}};
        out.blendPolygon(lowPts, 4, litCol, std::min(0.85f, litK));
    }
}

// Пятно света на дороге (аддитивно): центр (lat, z), радиус rad метров.
void drawGroundPool(Canvas& out, float curve, float lat, float z, float rad, uint32_t col, float k) {
    if (k <= 0.01f || z < 0.6f) return;
    const Proj c = project(curve, lat, 0.0f, z);
    const Proj nearP = project(curve, lat, 0.0f, std::max(0.5f, z - rad));
    const Proj farP = project(curve, lat, 0.0f, z + rad);
    const float rx = rad * c.ppm;
    const int yTop = std::max(static_cast<int>(kHorizonY), floori(farP.y));
    const int yBot = std::min(90, floori(nearP.y) + 1);
    const int xL = floori(c.x - rx * 1.6f), xR = floori(c.x + rx * 1.6f) + 1;
    if (yBot <= yTop) return;
    const int ox = out.offsetX(), oy = out.offsetY();
    uint32_t* px = out.data();
    const float kr = static_cast<float>(colR(col)) * k, kg = static_cast<float>(colG(col)) * k,
                kb = static_cast<float>(colB(col)) * k;
    for (int y = yTop; y <= yBot; ++y) {
        const int cyy = y + oy;
        if (cyy < 0 || cyy >= out.height()) continue;
        // Нормированная дальность строки внутри пятна (0 — центр).
        const float zr = (kCamH * kFocal) / std::max(0.5f, static_cast<float>(y) + 0.5f - kHorizonY);
        const float dz = (zr - z) / rad;
        if (std::fabs(dz) >= 1.0f) continue;
        float wl = 0.0f, wr = 0.0f;
        if (!polySpan(kDrawWindshield, 4, static_cast<float>(y) + 0.5f, wl, wr)) continue;
        const Proj rp = project(curve, lat, 0.0f, zr);
        const float rxr = rad * rp.ppm;
        const int xa = std::max({xL, static_cast<int>(std::ceil(wl - 0.5f)), -ox});
        const int xb = std::min({xR, static_cast<int>(std::ceil(wr - 0.5f)) - 1, out.width() - 1 - ox});
        uint32_t* row = px + static_cast<size_t>(cyy) * static_cast<size_t>(out.width());
        for (int x = xa; x <= xb; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f - rp.x) / rxr;
            const float d2 = dx * dx + dz * dz;
            if (d2 >= 1.0f) continue;
            const float w = (1.0f - d2) * (1.0f - d2);
            const uint32_t dst = row[x + ox];
            int cr = static_cast<int>((dst >> 16) & 255u) + static_cast<int>(kr * w);
            int cg = static_cast<int>((dst >> 8) & 255u) + static_cast<int>(kg * w);
            int cb = static_cast<int>(dst & 255u) + static_cast<int>(kb * w);
            cr = cr > 255 ? 255 : cr;
            cg = cg > 255 ? 255 : cg;
            cb = cb > 255 ? 255 : cb;
            row[x + ox] = (static_cast<uint32_t>(cr) << 16) | (static_cast<uint32_t>(cg) << 8) |
                          static_cast<uint32_t>(cb);
        }
    }
}

void drawWindshield(Canvas& out, const Frame& f) {
    const SceneData& s = *f.s;
    WindshieldShade w;
    w.f = &f;
    w.pal = &roadPalette();
    for (int y = 0; y < static_cast<int>(kHorizonY); ++y) {
        w.rows[static_cast<size_t>(y)].sky =
            rampRow(kSkyFront, 5, (static_cast<float>(y) - 33.0f) / (kHorizonY - 33.0f));
    }
    for (int y = static_cast<int>(kHorizonY); y < kH; ++y) {
        WindshieldRow& r = w.rows[static_cast<size_t>(y)];
        const float z = (kCamH * kFocal) / (static_cast<float>(y) + 0.5f - kHorizonY);
        const float ppm = kFocal / z;
        const float zw = z + s.dist;
        const float coneW = 0.9f + 0.30f * z;
        r.invPpm = 1.0f / ppm;
        r.cx = 160.0f + (curveOffset(s.curve, z) - kCamLat) * ppm;
        r.beam = f.beams * smoothstep(1.6f, 4.5f, z) / (1.0f + (z / 15.0f) * (z / 15.0f));
        r.invCone2 = 1.0f / (coneW * coneW);
        r.lineW = std::max(0.07f, 0.6f / ppm);
        r.edgeW = std::max(0.13f, 0.6f / ppm);
        r.fog = roundi(256.0f * smoothstep(38.0f, 120.0f, z));
        r.texZ = floori(zw * 2.5f);
        r.vergeZ = floori(zw * 1.5f);
        r.dash = fractf(zw / 9.0f) < 0.36f;
        r.tex = z < 22.0f;
        r.yard = z > f.houseZ - 1.5f;
    }
    const float shift = s.curve * 1400.0f + s.dist * 0.02f;
    for (int x = 0; x < kW; ++x) {
        const float wx = static_cast<float>(x) + shift;
        float h = 2.5f + 4.0f * valueNoise(wx * 0.06f, 11u) + 2.0f * valueNoise(wx * 0.23f, 12u);
        // Острые макушки елей.
        const float spike = std::fabs(fractf(wx * 0.29f) - 0.5f) * 2.0f;
        h += (1.0f - spike) * 3.0f * hashIf(floori(wx * 0.29f), 13u);
        h += 4.0f * f.forestHere;
        w.treeTop[static_cast<size_t>(x)] = static_cast<int16_t>(roundi(kHorizonY - h));
    }

    shadeConvex(out, kDrawWindshield, 4, kStWindshield,
                [&w](int x, int y, uint32_t) { return shadeWindshieldPx(w, x, y); });
    out.setStencilTest(kStWindshield);

    // Пятна света фонарей и фар встречной на асфальте.
    for (const Lamp& l : s.lamps) {
        if (!l.alive) continue;
        const float z = l.d * kMetersPerSec;
        if (z < 0.8f || z > 75.0f || z > f.houseZ) continue;
        drawGroundPool(out, s.curve, l.side * 3.0f, z, 6.0f, kWarmLight, 0.85f * smoothstep(75.0f, 40.0f, z));
    }
    if (s.carAlive && s.carZ > 2.0f && s.carZ < 90.0f) {
        drawGroundPool(out, s.curve, -1.3f, s.carZ * 0.55f, 0.18f * s.carZ + 2.0f, kColdLight,
                       0.35f * smoothstep(90.0f, 20.0f, s.carZ));
    }

    // Придорожные ели — от дальних к ближним.
    const float spacing = 6.5f;
    const int i0 = floori((s.dist + 2.5f) / spacing);
    const int i1 = floori((s.dist + 78.0f) / spacing);
    for (int i = i1; i >= i0; --i) {
        for (int sideI = 0; sideI < 2; ++sideI) {
            const float side = sideI == 0 ? -1.0f : 1.0f;
            const uint32_t sd = static_cast<uint32_t>(sideI) * 7u + 3u;
            const float zw = static_cast<float>(i) * spacing + hashIf(i, sd) * 4.0f;
            const float z = zw - s.dist;
            if (z < 2.5f || z > 78.0f || z > f.houseZ - 4.0f) continue;
            const float fa = forestAmount(s, zw);
            if (hashIf(i, sd + 1u) > lerpf(0.38f, 0.95f, fa)) continue;
            const float lat = side * (kRoadHalf + 2.6f + hashIf(i, sd + 2u) * lerpf(9.0f, 3.0f, fa));
            const float h = 7.0f + hashIf(i, sd + 3u) * 8.0f;
            const float lc = lat - kCamLat;
            const float litK = f.beams * smoothstep(34.0f, 8.0f, z) * std::exp(-(lc * lc) / 30.0f) * 0.8f;
            const uint32_t col = lerpColor(rgb(5, 7, 9), rgb(10, 12, 16), smoothstep(70.0f, 30.0f, z));
            drawConifer(out, s.curve, lat, z, h, col, rgb(52, 62, 40), litK);
        }
    }

    // Светоотражатели на столбиках вдоль обочин.
    {
        const float step = 25.0f;
        const int j0 = floori((s.dist + 2.5f) / step), j1 = floori((s.dist + 70.0f) / step);
        for (int j = j1; j >= j0; --j) {
            const float z = static_cast<float>(j) * step - s.dist;
            if (z < 2.5f || z > 70.0f || z > f.houseZ - 3.0f) continue;
            for (int sideI = 0; sideI < 2; ++sideI) {
                const float side = sideI == 0 ? -1.0f : 1.0f;
                const Proj p = project(s.curve, side * (kRoadHalf + 0.7f), 0.8f, z);
                const Proj b = project(s.curve, side * (kRoadHalf + 0.7f), 0.0f, z);
                const float lc = side * (kRoadHalf + 0.7f) - kCamLat;
                const float k =
                    f.beams * smoothstep(70.0f, 8.0f, z) * std::exp(-(lc * lc) / (4.0f + 0.2f * z * z));
                if (z < 22.0f)
                    out.drawLine(roundi(b.x), roundi(b.y), roundi(p.x), roundi(p.y),
                                 scaleColor(rgb(70, 70, 66), 0.3f + 0.7f * k));
                const uint32_t rc = sideI == 0 ? rgb(235, 235, 225) : rgb(255, 120, 60);
                out.plot(roundi(p.x), roundi(p.y), scaleColor(rc, 0.12f + 0.88f * k));
                if (k > 0.4f && z < 12.0f) out.plot(roundi(p.x), roundi(p.y) + 1, scaleColor(rc, 0.7f * k));
            }
        }
    }

    // Фонари: столб, кронштейн, плафон со свечением.
    for (int pass = 0; pass < 2; ++pass) {
        for (const Lamp& l : s.lamps) {
            if (!l.alive) continue;
            const float z = l.d * kMetersPerSec;
            if (z < 1.0f || z > 72.0f || z > f.houseZ) continue;
            const float poleLat = l.side * kLampPoleLat;
            const Proj b = project(s.curve, poleLat, 0.0f, z);
            const Proj t = project(s.curve, poleLat, kLampHeight, z);
            const Proj hd = project(s.curve, l.side * kLampHeadLat, kLampHeight - 0.1f, z);
            if (pass == 0) {
                const float pw = std::max(1.0f, 0.16f * b.ppm);
                const uint32_t pc = lerpColor(rgb(12, 12, 16), rgb(70, 58, 44), lampGlowAt(l.d) * 0.6f);
                thickSeg(out, b.x, b.y, t.x, t.y, pw, pw * 0.8f, pc);
                thickSeg(out, t.x, t.y, hd.x, hd.y, pw * 0.7f, pw * 0.6f, pc);
            } else {
                const float fade = smoothstep(72.0f, 50.0f, z);
                const int gr = clampi(roundi(2.5f + 2.2f * hd.ppm), 3, 18);
                addGlow(out, hd.x, hd.y, static_cast<float>(gr * 2), kWarmLight, 0.18f * fade,
                        kDrawWindshield, 4);
                addGlow(out, hd.x, hd.y, static_cast<float>(gr), rgb(255, 200, 130), 0.55f * fade,
                        kDrawWindshield, 4);
                const int core = std::max(0, roundi(0.18f * hd.ppm));
                out.fillCircle(roundi(hd.x), roundi(hd.y), core, rgb(255, 236, 190));
            }
        }
    }

    // Встречная машина: тёмный силуэт и две фары с бликом-«звездой».
    if (s.carAlive && s.carZ > 1.2f && s.carZ < 130.0f) {
        const float z = s.carZ;
        const Proj c = project(s.curve, -1.25f, 0.65f, z);
        const float sep = 0.75f * c.ppm;
        if (z < 35.0f) {
            const Proj bl = project(s.curve, -2.15f, 0.0f, z);
            const Proj tr = project(s.curve, -0.35f, 1.45f, z);
            const Proj roof = project(s.curve, -1.25f, 1.45f, z);
            out.fillRect(roundi(bl.x), roundi(tr.y), std::max(1, roundi(tr.x - bl.x)),
                         std::max(1, roundi(bl.y - tr.y)), rgb(9, 9, 12));
            out.fillRect(roundi(bl.x + (tr.x - bl.x) * 0.18f), roundi(roof.y - 0.5f * c.ppm),
                         std::max(1, roundi((tr.x - bl.x) * 0.64f)), std::max(1, roundi(0.5f * c.ppm)),
                         rgb(9, 9, 12));
        }
        const float k = smoothstep(130.0f, 25.0f, z);
        const int gr = clampi(roundi(3.0f + 1.8f * c.ppm), 3, 40);
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            const int hx = roundi(c.x + static_cast<float>(sgn) * sep), hy = roundi(c.y);
            addGlow(out, static_cast<float>(hx), static_cast<float>(hy), static_cast<float>(gr), kColdLight,
                    0.35f + 0.5f * k, kDrawWindshield, 4);
            out.fillCircle(hx, hy, std::max(0, roundi(0.12f * c.ppm)), rgb(255, 252, 240));
        }
        // Горизонтальный блик по стеклу.
        const int streak = roundi(static_cast<float>(gr) * 2.5f);
        for (int dx = -streak; dx <= streak; ++dx) {
            const float a = 1.0f - std::fabs(static_cast<float>(dx)) / static_cast<float>(streak + 1);
            out.addPixel(roundi(c.x) + dx, roundi(c.y), kColdLight, 0.35f * k * a * a);
        }
    }

    // Приезд: дом с тёплыми окнами в конце дороги.
    if (f.houseZ < 200.0f) {
        const float z = f.houseZ;
        const float hl = 2.2f;
        const Proj bl = project(s.curve, hl - 5.0f, 0.0f, z);
        const Proj br = project(s.curve, hl + 5.0f, 0.0f, z);
        const Proj wl = project(s.curve, hl - 5.0f, 3.2f, z);
        const Proj peak = project(s.curve, hl, 6.4f, z);
        const float beamK = f.beams * smoothstep(60.0f, 12.0f, z) * 0.7f;
        const uint32_t wall = lerpColor(rgb(20, 22, 32), rgb(70, 66, 64), beamK);
        out.fillRect(roundi(bl.x), roundi(wl.y), std::max(1, roundi(br.x - bl.x)),
                     std::max(1, roundi(bl.y - wl.y)), wall);
        const Vec2 roof[3] = {
            {bl.x - 0.6f * bl.ppm, wl.y + 0.5f}, {peak.x, peak.y}, {br.x + 0.6f * bl.ppm, wl.y + 0.5f}};
        out.fillPolygon(roof, 3, rgb(12, 12, 18));
        const Proj ch = project(s.curve, hl + 2.6f, 6.2f, z);
        out.fillRect(roundi(ch.x), roundi(ch.y), std::max(1, roundi(0.7f * ch.ppm)),
                     std::max(1, roundi(1.6f * ch.ppm)), rgb(12, 12, 18));
        // Окна и дверь с фонарём над ней.
        const float winLat[3] = {hl - 4.0f, hl - 2.0f, hl + 3.0f};
        for (int i = 0; i < 3; ++i) {
            const Proj a = project(s.curve, winLat[i], 1.9f, z);
            const Proj b = project(s.curve, winLat[i] + 1.3f, 0.75f, z);
            const int ww = std::max(1, roundi(b.x - a.x)), wh = std::max(1, roundi(b.y - a.y));
            out.fillRect(roundi(a.x), roundi(a.y), ww, wh, rgb(255, 196, 100));
            if (ww >= 4) {
                out.fillRect(roundi(a.x) + ww / 2, roundi(a.y), 1, wh, rgb(120, 70, 30));
                out.fillRect(roundi(a.x), roundi(a.y) + wh / 2, ww, 1, rgb(120, 70, 30));
            }
            addGlow(out, a.x + static_cast<float>(ww) * 0.5f, a.y + static_cast<float>(wh) * 0.5f,
                    static_cast<float>(std::max(3, ww * 2)), rgb(255, 170, 70), 0.35f, kDrawWindshield, 4);
        }
        const Proj door = project(s.curve, hl + 1.0f, 2.1f, z);
        const Proj doorB = project(s.curve, hl + 1.9f, 0.0f, z);
        out.fillRect(roundi(door.x), roundi(door.y), std::max(1, roundi(doorB.x - door.x)),
                     std::max(1, roundi(doorB.y - door.y)), rgb(60, 36, 22));
        const Proj porch = project(s.curve, hl + 1.45f, 2.6f, z);
        addGlow(out, porch.x, porch.y, clampf(4.0f * porch.ppm, 3.0f, 30.0f), rgb(255, 190, 110), 0.5f,
                kDrawWindshield, 4);
    }
    out.resetStencilModes();
}

// ============================================================================
//  Боковые окна (и узкие передние стёкла): параллакс леса, столбы, провода
// ============================================================================
constexpr int kSideRows = 128; // строки -8..119 (с запасом на тряску)

struct SideShade {
    const Frame* f = nullptr;
    bool right = false;
    RampRow sky[kSideRows];
    float lightK = 0.0f;   // тёплый свет фонаря снаружи
    float coldK = 0.0f;    // холодный свет встречных фар
    float dim = 1.0f;      // передние стёкла темнее
    int16_t farTop[kSideCols] = {};
    int16_t midTop[kSideCols] = {};
    uint8_t pole[kSideCols] = {};  // 0 — нет, 1 — столб ЛЭП, 2 — столб фонаря
    uint8_t birch[kSideCols] = {}; // берёзовый ствол
    int16_t wire[kSideCols] = {};  // верхний провод (-1 — нет)
};

constexpr int kSideHorizon = 70;
const uint32_t kSkySide[] = {rgb(6, 8, 22), rgb(10, 12, 29), rgb(16, 17, 38), rgb(24, 24, 48),
                             rgb(31, 29, 55)};

void buildSideColumns(SideShade& sd, const SceneData& s, bool right) {
    const uint32_t seed = right ? 500u : 100u;
    const float dist = s.dist;
    for (int u = 0; u < kSideCols; ++u) {
        const size_t i = static_cast<size_t>(u);
        const float fu = static_cast<float>(u);
        // Дальняя линия леса/холмов.
        const float wf = fu + dist * 0.15f;
        const float hf =
            7.0f + 9.0f * valueNoise(wf * 0.03f, seed) + 3.0f * valueNoise(wf * 0.11f, seed + 1u);
        sd.farTop[i] = static_cast<int16_t>(kSideHorizon - roundi(hf));
        // Средний план: ели.
        const float wm = fu + dist * 1.1f;
        const float spacing = 10.0f;
        const int c0 = floori(wm / spacing);
        float top = 999.0f;
        const float dens = right ? 0.55f : 0.8f;
        for (int c = c0 - 2; c <= c0 + 2; ++c) {
            if (hashIf(c, seed + 2u) > dens) continue;
            const float centre =
                (static_cast<float>(c) + 0.5f) * spacing + (hashIf(c, seed + 3u) - 0.5f) * 5.0f;
            const float H = 16.0f + hashIf(c, seed + 4u) * 22.0f;
            const float half = H * 0.30f;
            const float dx = std::fabs(wm - centre);
            if (dx > half) continue;
            // Ярусы веток: зубчатый край.
            const float tier = fractf(dx * 0.5f + static_cast<float>(c) * 0.37f) < 0.5f ? 1.5f : 0.0f;
            const float yTop = static_cast<float>(kSideHorizon + 4) - H + dx * (H / half) + tier;
            top = std::min(top, yTop);
        }
        sd.midTop[i] = static_cast<int16_t>(std::min(999, roundi(top)));
        // Ближний план.
        const float wn = fu + dist * 4.2f;
        sd.pole[i] = 0;
        sd.birch[i] = 0;
        sd.wire[i] = -1;
        if (right) {
            // Линия электропередачи: столбы и провисающие провода.
            const float span = 64.0f;
            const float pos = wn - std::floor(wn / span) * span;
            const float blur = 2.0f + s.speed * 2.0f;
            if (pos < blur) sd.pole[i] = 1;
            const float t = pos / span;
            sd.wire[i] = static_cast<int16_t>(roundi(33.0f + 9.0f * 4.0f * t * (1.0f - t)));
        } else {
            // Берёзы у самой дороги — светлые стволы.
            const float span = 23.0f;
            const int cell = floori(wn / span);
            const float pos = wn - static_cast<float>(cell) * span;
            if (hashIf(cell, seed + 7u) < 0.4f && pos < 1.5f + s.speed) sd.birch[i] = 1;
        }
    }
    // Столб фонаря проносится мимо своего окна в момент проезда.
    for (const Lamp& l : s.lamps) {
        if (!l.alive || (l.side > 0.0f) != right) continue;
        if (l.d < -0.08f || l.d > 0.08f) continue;
        const float pu = lerpf(58.0f, -6.0f, (0.08f - l.d) / 0.16f);
        for (int u = floori(pu); u <= floori(pu) + 3; ++u) {
            if (u >= 0 && u < kSideCols) sd.pole[static_cast<size_t>(u)] = 2;
        }
    }
}

uint32_t shadeSidePx(const SideShade& sd, int x, int y) {
    const Frame& f = *sd.f;
    const int lx = x - f.ox, ly = y - f.oy;
    const int u = sd.right ? (kW - 1 - lx) : lx;
    if (u < 0 || u >= kSideCols) return rgb(6, 7, 12);
    const size_t i = static_cast<size_t>(u);
    uint32_t c;
    float lightMul = 0.0f; // насколько этот слой «ловит» свет фонаря
    if (sd.pole[i] == 2) {
        c = lerpColor(rgb(30, 26, 22), rgb(150, 110, 70), sd.lightK);
    } else if (sd.pole[i] == 1 && ly > 26) {
        c = rgb(8, 8, 10);
        lightMul = 0.6f;
    } else if (sd.birch[i] && ly > 30) {
        const bool mark = hashXY(u / 3, ly / 2, 33u) % 5u == 0u;
        c = mark ? rgb(8, 8, 8) : rgb(26, 26, 25);
        lightMul = 1.2f;
    } else if (ly >= kSideHorizon) {
        c = rgb(6, 7, 8);
        lightMul = 0.25f + 0.9f * saturate(static_cast<float>(ly - kSideHorizon) / 30.0f);
    } else if (ly >= sd.midTop[i]) {
        c = rgb(5, 6, 10);
        lightMul = 0.7f;
    } else if (ly >= sd.farTop[i]) {
        c = rgb(11, 13, 22);
        lightMul = 0.15f;
    } else if (sd.wire[i] >= 0 && (ly == sd.wire[i] || ly == sd.wire[i] + 5)) {
        c = rgb(4, 4, 7);
    } else {
        c = rampPick(sd.sky[static_cast<size_t>(clampi(ly + 8, 0, kSideRows - 1))], x, y);
        // Луна слева: свечение неба.
        if (!sd.right) {
            const float mdx = static_cast<float>(u) - 17.0f, mdy = static_cast<float>(ly) - 41.0f;
            const float md = std::sqrt(mdx * mdx + mdy * mdy);
            if (md < 26.0f) {
                const float g = 1.0f - md / 26.0f;
                c = addColor(c, scaleColor(rgb(60, 66, 90), g * g));
            }
            if (md < 4.6f) {
                // Месяц: светлый диск без «откушенной» части.
                const float cdx = mdx - 2.2f, cdy = mdy + 1.0f;
                c = cdx * cdx + cdy * cdy < 13.0f ? addColor(c, rgb(20, 22, 34)) : rgb(214, 220, 232);
            }
        }
        // Звёзды.
        if (starAt(f, lx, ly, kStarSide)) {
            const float tw = 0.55f + 0.45f * std::sin(f.time * 1.7f + static_cast<float>(u * 7 + ly));
            c = addColor(c, scaleColor(rgb(170, 175, 200), tw));
        }
    }
    if (lightMul > 0.0f && sd.lightK > 0.01f) c = litColor(c, kWarmLight, sd.lightK * lightMul * 0.9f);
    if (sd.coldK > 0.01f) c = litColor(c, kColdLight, sd.coldK * (0.3f + lightMul));
    if (sd.dim < 1.0f) c = scaleColor(c, sd.dim);
    return c;
}

void drawSideWindows(Canvas& out, const Frame& f) {
    const SceneData& s = *f.s;
    for (int side = 0; side < 2; ++side) {
        const bool right = side == 1;
        SideShade sd;
        sd.f = &f;
        sd.right = right;
        sd.lightK = right ? f.sideLightR : f.sideLightL;
        sd.coldK = right ? 0.0f : f.carSideL;
        for (int r = 0; r < kSideRows; ++r) {
            sd.sky[static_cast<size_t>(r)] = rampRow(
                kSkySide, 5, (static_cast<float>(r - 8) - 20.0f) / static_cast<float>(kSideHorizon - 20));
        }
        buildSideColumns(sd, s, right);
        auto shader = [&sd](int x, int y, uint32_t) { return shadeSidePx(sd, x, y); };
        shadeConvex(out, right ? kDrawRight : kDrawLeft, 4, right ? kStRight : kStLeft, shader);
        // Переднее боковое стекло — то же «кино», только темнее и дальше.
        sd.dim = 0.6f;
        if (right) {
            const auto poly = mirrorPoly(kFrontGlassL);
            shadeConvex(out, poly.data(), 4, 0, shader);
        } else {
            shadeConvex(out, kFrontGlassL, 4, 0, shader);
        }
    }
}

// ============================================================================
//  Люк: глубокое небо, облака, ветви в лесу, фонарь над головой
// ============================================================================
struct SunroofShade {
    const Frame* f = nullptr;
    RampRow sky[48]; // строки -12..35
    float clouds = 0.0f;
    float branches = 0.0f;
    float branchScroll = 0.0f;
    float lampK = 0.0f;
};

const uint32_t kSkyUp[] = {rgb(4, 5, 15), rgb(7, 8, 21), rgb(11, 12, 28), rgb(15, 16, 34)};

uint32_t shadeSunroofPx(const SunroofShade& sr, int x, int y) {
    const Frame& f = *sr.f;
    const int lx = x - f.ox, ly = y - f.oy;
    uint32_t c = rampPick(sr.sky[static_cast<size_t>(clampi(ly + 12, 0, 47))], x, y);
    const float fx = static_cast<float>(lx), fy = static_cast<float>(ly);
    const float cl = noise2(fx * 0.06f + sr.clouds, fy * 0.13f + sr.clouds * 0.3f, 3u);
    const float cloudA = smoothstep(0.52f, 0.8f, cl);
    if (cloudA < 0.6f && starAt(f, lx, ly, kStarUp)) {
        const float tw = 0.6f + 0.4f * std::sin(f.time * 2.1f + static_cast<float>(lx * 3 + ly * 5));
        c = addColor(c, scaleColor(rgb(190, 195, 220), tw * (1.0f - cloudA / 0.6f)));
    }
    if (cloudA > 0.0f) c = lerpColor(c, rgb(24, 26, 42), cloudA * 0.85f);
    if (sr.branches > 0.02f) {
        // Ветви над дорогой: в каждом 36-пиксельном отрезке «мира» — сук от
        // края к центру с парой сучков; проносятся снизу вверх.
        const float wy = fy + sr.branchScroll;
        const int seg = floori(wy / 36.0f);
        for (int k = seg - 1; k <= seg; ++k) {
            if (hashIf(k, 61u) > sr.branches * 0.8f) continue;
            const float baseY = static_cast<float>(k) * 36.0f;
            const bool fromLeft = hashIf(k, 62u) < 0.5f;
            const Vec2 a{fromLeft ? 104.0f : 216.0f, baseY + hashIf(k, 63u) * 20.0f};
            const Vec2 b{160.0f + (hashIf(k, 64u) - 0.5f) * 50.0f, a.y + 8.0f + hashIf(k, 65u) * 18.0f};
            const Vec2 m{lerpf(a.x, b.x, 0.55f), lerpf(a.y, b.y, 0.55f)};
            const Vec2 n{lerpf(a.x, b.x, 0.8f), lerpf(a.y, b.y, 0.8f)};
            const Vec2 t1{m.x + (fromLeft ? 4.0f : -4.0f), m.y - 9.0f};
            const Vec2 t2{n.x, n.y + 7.0f};
            const float along = saturate((fromLeft ? fx - a.x : a.x - fx) / std::fabs(b.x - a.x));
            const float thick = lerpf(1.6f, 0.5f, along);
            if (distToSeg(fx, wy, a, b) < thick || distToSeg(fx, wy, m, t1) < 0.6f ||
                distToSeg(fx, wy, n, t2) < 0.5f)
                c = rgb(3, 3, 6);
        }
    }
    if (sr.lampK > 0.01f) c = litColor(c, kWarmLight, sr.lampK * 0.5f);
    return c;
}

// Силуэт монстра, ползущего по стеклу люка (виден снизу).
void drawRoofSilhouette(Canvas& out, const Frame& f, float px, float py, float dirX, float dirY, float jerk) {
    const float len = std::sqrt(dirX * dirX + dirY * dirY);
    const float dx = len > 0.01f ? dirX / len : 1.0f, dy = len > 0.01f ? dirY / len : 0.0f;
    const float nx = -dy, ny = dx;
    const float ang = std::atan2(dy, dx);
    const uint32_t body = rgb(5, 5, 9);
    const uint32_t rim = scaleColor(rgb(60, 66, 88), 0.4f + 0.6f * f.monLight);
    const float gait = f.time * 6.0f;
    // Длинные конечности, широко расставленные.
    for (int k = 0; k < 4; ++k) {
        const float fore = k < 2 ? 1.0f : -1.0f;
        const float sgn = (k % 2 == 0) ? 1.0f : -1.0f;
        const float ph = std::sin(gait + static_cast<float>(k) * 1.7f) * 3.0f;
        const float sx = px + dx * 8.0f * fore + nx * 3.0f * sgn;
        const float sy = py + dy * 8.0f * fore + ny * 3.0f * sgn;
        const float kx = sx + nx * 10.0f * sgn + dx * (4.0f * fore + ph);
        const float ky = sy + ny * 10.0f * sgn + dy * (4.0f * fore + ph);
        const float fx = kx + nx * 6.0f * sgn + dx * (6.0f * fore - ph);
        const float fy = ky + ny * 6.0f * sgn + dy * (6.0f * fore - ph);
        thickSeg(out, sx, sy, kx, ky, 3.0f, 2.2f, body);
        thickSeg(out, kx, ky, fx, fy, 2.2f, 1.2f, body);
        // Коготь, скребущий стекло.
        out.plot(floori(fx + dx * 2.0f), floori(fy + dy * 2.0f), rgb(30, 30, 36));
    }
    fillRotEllipse(out, px + nx * 0.6f, py + ny * 0.6f, 13.5f, 5.5f, ang, rim);
    fillRotEllipse(out, px, py, 13.0f, 5.0f, ang, body);
    // Голова впереди; глаза смотрят вниз — в салон.
    const float hx = px + dx * 15.0f + nx * jerk, hy = py + dy * 15.0f + ny * jerk;
    out.fillCircle(roundi(hx), roundi(hy), 5, body);
    for (int sgn = -1; sgn <= 1; sgn += 2) {
        const int ex = roundi(hx + nx * 2.0f * static_cast<float>(sgn) + dx),
                  ey = roundi(hy + ny * 2.0f * static_cast<float>(sgn) + dy);
        out.glow(ex, ey, 3, kPupilGlow, 0.3f);
        out.plot(ex, ey, kPupilCore);
    }
}

void drawSunroof(Canvas& out, const Frame& f, const Monster& m) {
    const SceneData& s = *f.s;
    SunroofShade sr;
    sr.f = &f;
    sr.clouds = f.time * 0.35f + s.dist * 0.002f;
    sr.branches = f.forestHere;
    sr.branchScroll = s.dist * 6.0f;
    sr.lampK = f.light;
    for (int r = 0; r < 48; ++r)
        sr.sky[static_cast<size_t>(r)] = rampRow(kSkyUp, 4, (static_cast<float>(r - 12) + 8.0f) / 30.0f);
    shadeConvex(out, kDrawSunroof, 4, kStSunroof,
                [&sr](int x, int y, uint32_t) { return shadeSunroofPx(sr, x, y); });
    out.setStencilTest(kStSunroof);
    // Фонарь проплывает над головой (снизу экрана — вперёд, вверх — назад).
    for (const Lamp& l : s.lamps) {
        if (!l.alive || std::fabs(l.d) > 0.16f) continue;
        const float t = (0.16f - l.d) / 0.32f;
        const int lx = roundi(160.0f + l.side * 34.0f);
        const int ly = roundi(lerpf(26.0f, -10.0f, t));
        addGlow(out, static_cast<float>(lx), static_cast<float>(ly), 18.0f, kWarmLight, 0.5f, kDrawSunroof,
                4);
        out.fillRect(lx - 3, ly - 1, 7, 3, rgb(255, 230, 180));
    }
    // Тень прыжка на крышу.
    const MonsterState ms = m.state();
    if (ms == MonsterState::Landing && m.stateTime() < 0.25f) {
        const float k = 1.0f - m.stateTime() / 0.25f;
        const int by = roundi(lerpf(-14.0f, 30.0f, m.stateTime() / 0.25f));
        out.blendRect(100, by - 10, 120, 20, rgb(2, 2, 4), 0.85f * k);
    }
    // Монстр ползёт прямо по стеклу люка.
    const bool crawling =
        ms == MonsterState::Crawling || ms == MonsterState::Retreating || ms == MonsterState::Landing;
    if (crawling && m.onRoof()) {
        const float px = 160.0f + m.roofX() * 140.0f;
        const float py = lerpf(-6.0f, 30.0f, m.roofZ());
        if (px > 90.0f && px < 230.0f && py > -22.0f && py < 44.0f)
            drawRoofSilhouette(out, f, px, py, s.crawlDX, s.crawlDY, 0.0f);
    }
    if (ms == MonsterState::Repelled && s.poseEntry == Entry::None && s.wasOverSunroof) {
        // Отпугнули прямо над люком: силуэт дёргается и уползает.
        const float k = easeOut3(m.stateTime() / 0.4f);
        const float px = 160.0f + m.roofX() * 140.0f;
        const float py = lerpf(-6.0f, 30.0f, m.roofZ()) - 18.0f * k;
        const float jerk = std::sin(f.time * 60.0f) * 2.0f * (1.0f - k);
        drawRoofSilhouette(out, f, px, py, s.crawlDX, s.crawlDY, jerk);
    }
    out.resetStencilModes();
}

// ============================================================================
//  Монстр в окне
// ============================================================================
LimbStyle monsterLimbStyle(const Frame& f) {
    LimbStyle st;
    const float k = 0.62f + 0.45f * f.monLight;
    const uint32_t tint = lerpColor(rgb(255, 255, 255), f.monLightCol, 0.4f);
    st.skin = scaleColor(mulColor(rgb(166, 180, 150), tint), k);
    st.dark = rgb(12, 14, 14);
    st.claw = scaleColor(mulColor(rgb(206, 196, 160), tint), 0.5f + 0.5f * f.monLight);
    return st;
}

// Монстр у бокового окна: пальцы через верхнюю кромку, затем голова вниз
// головой. Всё считается для левого окна и зеркалится для правого.
void drawSidePose(Canvas& out, const Frame& f, bool right, Pose pose, float ap, float poseT, float bang) {
    auto X = [right](float x) { return right ? static_cast<float>(kW) - x : x; };
    const float mir = right ? -1.0f : 1.0f;
    const float t = f.time;
    const LimbStyle st = monsterLimbStyle(f);

    float recoil = 0.0f;
    if (pose == Pose::Recoil) recoil = easeOut3(poseT / 0.32f);
    const float fingerP = smoothstep(0.0f, 0.3f, ap) * (1.0f - recoil);
    const float headP = smoothstep(0.4f, 1.0f, ap);
    const float jitter =
        pose == Pose::Break || pose == Pose::Recoil
            ? (hashXYf(floori(t * 30.0f), 5, 9u) - 0.5f) * (pose == Pose::Recoil ? 3.0f : 2.0f)
            : 0.0f;

    float r = 16.0f;
    float cx = 25.0f + std::sin(t * 1.1f) * 1.2f + jitter;
    float cy = lerpf(leftEdgeY(25.0f) - r * 1.3f, 52.0f, headP);
    float mouth = 0.14f + 0.12f * std::pow(std::max(0.0f, std::sin(t * 0.5f)), 8.0f);
    float squint = 0.0f;
    float blink = fractf(t * 0.21f + 0.3f) < 0.035f ? 1.0f : 0.0f;
    float tilt = std::sin(t * 0.7f) * 0.10f + 0.30f;
    if (pose == Pose::Break) {
        r *= 1.0f + 0.2f * bang;
        cy += 5.0f * bang;
        mouth = 0.45f + 0.55f * bang;
        blink = 0.0f;
        tilt = 0.2f + std::sin(t * 9.0f) * 0.12f;
    } else if (pose == Pose::Recoil) {
        cy -= 38.0f * recoil;
        mouth = 0.85f;
        squint = 1.0f;
        blink = 0.0f;
        tilt = 0.25f * recoil;
    } else if (pose == Pose::Withdraw) {
        blink = 0.0f;
    }

    // Шея уходит вверх, к крыше.
    if (headP > 0.02f || pose == Pose::Recoil) {
        // Подбородок (вниз головой — сверху), с учётом наклона головы.
        const float chinX = cx + std::sin(tilt) * r * 0.95f;
        const float chinY = cy - std::cos(tilt) * r * 0.95f;
        thickSeg(out, X(chinX), chinY + 3.0f, X(cx + 4.0f), leftEdgeY(cx) - 6.0f, r * 0.36f + 1.0f,
                 r * 0.5f + 1.0f, st.dark);
        thickSeg(out, X(chinX), chinY + 3.0f, X(cx + 4.0f), leftEdgeY(cx) - 6.0f, r * 0.36f, r * 0.5f,
                 scaleColor(st.skin, 0.38f));
    }

    // Пальцы, вцепившиеся в кромку крыши (по четыре на руку).
    const float hands[2] = {8.0f, 38.0f};
    for (int h = 0; h < 2; ++h) {
        if (pose == Pose::Break && h == 1) continue; // эта рука бьёт по стеклу
        const float hx = hands[h];
        const float inward = h == 0 ? 1.0f : -1.0f;
        for (int k = 0; k < 4; ++k) {
            const float fk = static_cast<float>(k) - 1.5f;
            const float bx = hx + fk * 4.3f;
            const float by = leftEdgeY(bx) - 3.0f;
            const float len = (18.0f + (k == 1 || k == 2 ? 4.0f : 0.0f)) * fingerP;
            const float wig =
                std::sin(t * 2.3f + static_cast<float>(k) + static_cast<float>(h) * 2.0f) * 0.07f;
            const float ang = kPi * 0.5f + (fk * 0.11f - inward * 0.10f + wig) * mir;
            drawFinger(out, X(bx), by, ang, len, inward * 0.24f * mir, 1.9f, st);
        }
    }

    // Ладонь, бьющая по стеклу.
    if (pose == Pose::Break) {
        const float s = 1.3f + 0.3f * bang;
        drawPalm(out, X(37.0f + jitter), 80.0f - 4.0f * bang, s, -kPi * 0.5f - 0.25f * mir, st,
                 0.5f + 0.5f * bang);
    }

    // Голова вниз головой.
    if (headP > 0.01f || pose == Pose::Recoil) {
        Face fc;
        fc.cx = X(cx);
        fc.cy = cy;
        fc.r = r;
        fc.angle = kPi + tilt * mir;
        fc.mouth = mouth;
        fc.squint = squint;
        fc.blink = blink;
        fc.light = f.monLight;
        fc.lightCol = f.monLightCol;
        fc.lightDirX = right ? -0.3f : 0.5f;
        fc.lightDirY = -1.0f;
        fc.lookX = 0.8f * mir;
        fc.lookY = 0.3f;
        fc.pupil = pose == Pose::Break ? 1.3f : 1.0f;
        drawFace(out, fc);
        // Дыхание туманит стекло у пасти.
        if (pose != Pose::Recoil && headP > 0.7f) {
            const float br = 0.5f + 0.5f * std::sin(t * (pose == Pose::Break ? 9.0f : 2.2f));
            out.blendEllipse(roundi(X(cx)), roundi(cy - r * 0.62f), roundi(r * 0.55f), roundi(r * 0.25f),
                             rgb(170, 180, 190), 0.05f + 0.07f * br);
        }
    }
}

// Монстр в люке: ладони распластаны по стеклу, лицо смотрит вниз.
void drawSunroofPose(Canvas& out, const Frame& f, Pose pose, float ap, float poseT, float bang) {
    const float t = f.time;
    const LimbStyle st = monsterLimbStyle(f);
    float recoil = 0.0f;
    if (pose == Pose::Recoil) recoil = easeOut3(poseT / 0.32f);
    const float palmP = smoothstep(0.0f, 0.4f, ap) * (1.0f - recoil);
    const float headP = smoothstep(0.4f, 1.0f, ap);
    const float jitter = pose == Pose::Break ? (hashXYf(floori(t * 30.0f), 6, 9u) - 0.5f) * 2.0f : 0.0f;

    float r = 16.0f;
    float cy = lerpf(-32.0f, 9.0f, headP);
    float mouth = 0.15f;
    float squint = 0.0f;
    if (pose == Pose::Break) {
        r *= 1.0f + 0.16f * bang;
        mouth = 0.4f + 0.6f * bang;
    } else if (pose == Pose::Recoil) {
        cy -= 32.0f * recoil;
        mouth = 0.85f;
        squint = 1.0f;
    }
    Face fc;
    fc.cx = 160.0f + std::sin(t * 0.9f) * 1.5f + jitter;
    fc.cy = cy;
    fc.r = r;
    fc.angle = kPi + std::sin(t * 0.6f) * 0.08f;
    fc.mouth = mouth;
    fc.squint = squint;
    fc.blink = pose == Pose::Peek && fractf(t * 0.19f) < 0.03f ? 1.0f : 0.0f;
    fc.light = f.monLight;
    fc.lightCol = f.monLightCol;
    fc.lightDirX = 0.3f;
    fc.lightDirY = 1.0f;
    fc.lookY = 1.0f;
    fc.pupil = 1.1f;
    if (headP > 0.01f || pose == Pose::Recoil) drawFace(out, fc);

    if (palmP > 0.05f && recoil < 0.6f) {
        // Шлепок: ладонь сначала чуть больше, затем «прилипает»; при испуге —
        // отдёргивается (уменьшается, уходя от стекла).
        const float slap = pose == Pose::Recoil ? 1.0f - 0.5f * recoil : 1.0f + 0.3f * (1.0f - palmP);
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            const float fs = static_cast<float>(sgn);
            const float pound = pose == Pose::Break ? bang * (sgn < 0 ? 1.0f : 0.6f) : 0.0f;
            const float s = (1.2f + 0.3f * pound) * slap;
            const float px = 160.0f + fs * 31.0f + (pose == Pose::Break ? jitter : 0.0f);
            drawPalm(out, px, 15.0f, s, -kPi * 0.5f + fs * 0.55f, st, palmP);
        }
    }
}

void drawMonster(Canvas& out, const Frame& f, const Monster& m) {
    const SceneData& s = *f.s;
    const MonsterState ms = m.state();
    const Entry tgt = m.target();
    const float ap = saturate(m.appear());
    Pose pose = Pose::Peek;
    Entry where = Entry::None;
    float apUse = ap;
    switch (ms) {
    case MonsterState::Peeking:
        pose = Pose::Peek;
        where = tgt;
        break;
    case MonsterState::BreakingIn:
    case MonsterState::Entered:
        pose = Pose::Break;
        where = tgt;
        apUse = std::max(ap, ms == MonsterState::Entered ? 1.0f : ap);
        break;
    case MonsterState::Repelled:
        if (s.poseEntry != Entry::None) {
            pose = Pose::Recoil;
            where = s.poseEntry;
            apUse = std::max(ap, s.poseAppear);
        }
        break;
    case MonsterState::Retreating:
        if (s.poseEntry != Entry::None && ap > 0.01f) {
            pose = Pose::Withdraw;
            where = s.poseEntry;
        }
        break;
    default:
        break;
    }

    // Тень у бокового окна: монстр подползает к краю крыши.
    if (ms == MonsterState::Crawling && (tgt == Entry::Left || tgt == Entry::Right)) {
        const bool right = tgt == Entry::Right;
        const float edge = right ? m.roofX() : -m.roofX();
        const float k = smoothstep(0.45f, 0.95f, edge);
        if (k > 0.01f) {
            // Тень «головы и плеч» свешивается с кромки крыши; под фонарём —
            // резче и темнее.
            out.setStencilTest(right ? kStRight : kStLeft);
            const float sx = 24.0f + (m.roofZ() - 0.5f) * 24.0f;
            const float depth = 30.0f * k + 1.0f;
            const float dark = std::min(0.95f, (0.75f + 0.3f * f.light) * k);
            for (int y = 18; y < 64; ++y) {
                for (int x = -6; x < 56; ++x) {
                    const float fx = static_cast<float>(x);
                    const float ey = (static_cast<float>(y) - leftEdgeY(fx)) / depth;
                    if (ey < 0.0f) continue;
                    // Шире у кромки (плечи), уже внизу (голова).
                    const float halfW = lerpf(30.0f, 13.0f, saturate(ey * 1.3f));
                    const float ex = (fx - sx) / halfW;
                    const float d = ex * ex + ey * ey;
                    if (d >= 1.0f) continue;
                    const float a = dark * std::min(1.0f, (1.0f - d) * (2.0f + 2.0f * f.light));
                    out.blendPixel(right ? kW - 1 - x : x, y, rgb(1, 1, 3), a);
                }
            }
            out.resetStencilModes();
        }
    }

    if (where == Entry::None || apUse <= 0.01f) return;
    const float poseT = m.stateTime();
    const float bang = saturate(m.bangPulse());
    if (where == Entry::Sunroof) {
        out.setStencilTest(kStSunroof);
        drawSunroofPose(out, f, pose, apUse, poseT, bang);
    } else {
        const bool right = where == Entry::Right;
        out.setStencilTest(right ? kStRight : kStLeft);
        drawSidePose(out, f, right, pose, apUse, poseT, bang);
    }
    out.resetStencilModes();
}

// ============================================================================
//  Стекло: блики, капли, царапины, трещины
// ============================================================================
void drawCracks(Canvas& out, const SceneData& s, int w, float crack, float light) {
    if (crack <= 0.01f) return;
    const auto& segs = s.cracks[static_cast<size_t>(w)];
    const int n = s.crackCount[static_cast<size_t>(w)];
    const uint32_t col = rgb(214, 224, 236);
    const float a = 0.45f + 0.35f * light;
    for (int i = 0; i < n; ++i) {
        const CrackSeg& c = segs[static_cast<size_t>(i)];
        if (crack <= c.t0) continue;
        const float k = saturate((crack - c.t0) / std::max(0.01f, c.t1 - c.t0));
        const float ex = lerpf(c.x0, c.x1, k), ey = lerpf(c.y0, c.y1, k);
        out.blendLine(roundi(c.x0 + 1.0f), roundi(c.y0 + 1.0f), roundi(ex + 1.0f), roundi(ey + 1.0f),
                      rgb(0, 0, 0), 0.25f);
        out.blendLine(roundi(c.x0), roundi(c.y0), roundi(ex), roundi(ey), col, a);
    }
    // Точка удара: выбитая «звёздочка».
    const Vec2 o = s.crackOrigin[static_cast<size_t>(w)];
    const int ox = roundi(o.x), oy = roundi(o.y);
    out.blendCircle(ox, oy, 1 + roundi(crack * 2.0f), col, 0.25f + 0.3f * crack);
    out.plot(ox, oy, rgb(240, 245, 250));
}

void drawGlass(Canvas& out, const Frame& f, const Monster& m) {
    const SceneData& s = *f.s;
    // Лобовое: два косых блика.
    out.setStencilTest(kStWindshield);
    for (int i = 0; i < 2; ++i) {
        const int x0 = 96 + i * 7;
        out.blendLine(x0, 36, x0 - 22, 83, rgb(150, 160, 190), 0.05f + 0.05f * f.light);
    }
    out.blendLine(214, 36, 236, 83, rgb(150, 160, 190), 0.04f + 0.04f * f.light);

    // Боковые окна и люк: капли, блики, царапины, трещины.
    const uint8_t ids[3] = {kStLeft, kStSunroof, kStRight};
    const Entry entries[3] = {Entry::Left, Entry::Sunroof, Entry::Right};
    for (int w = 0; w < 3; ++w) {
        out.setStencilTest(ids[w]);
        const float wl = w == 0 ? f.sideLightL : (w == 2 ? f.sideLightR : f.light);
        // Капли: светлая точка и тёмная тень под ней; ползут назад по стеклу.
        for (int i = 0; i < 18; ++i) {
            const uint32_t sd = static_cast<uint32_t>(w * 100 + i);
            float x, y;
            if (w == 1) {
                x = 112.0f + hashIf(i, sd) * 96.0f;
                y = hashIf(i, sd + 1u) * 22.0f;
            } else {
                const float drift =
                    fractf(hashIf(i, sd + 2u) + s.dist * 0.0025f * (0.5f + hashIf(i, sd + 3u)));
                const float lx = drift * 54.0f;
                x = w == 0 ? 54.0f - lx : 266.0f + lx;
                y = 26.0f + hashIf(i, sd + 1u) * 76.0f;
            }
            const int ix = floori(x), iy = floori(y);
            out.blendPixel(ix, iy, rgb(200, 210, 230), 0.18f + 0.5f * wl);
            out.blendPixel(ix, iy + 1, rgb(0, 0, 0), 0.25f);
        }
        if (w != 1) {
            const int bx = w == 0 ? 10 : 310;
            const int dir = w == 0 ? 1 : -1;
            out.blendLine(bx, 34, bx + dir * 30, 96, rgb(150, 160, 190), 0.035f + 0.05f * wl);
        }
        if (w == 1) {
            for (const Scratch& sc : s.scratches) {
                if (!sc.alive) continue;
                const float a = 0.5f * (1.0f - sc.age / 25.0f);
                out.blendLine(roundi(sc.x0), roundi(sc.y0), roundi(sc.x1), roundi(sc.y1), rgb(200, 205, 215),
                              a);
            }
        }
        drawCracks(out, s, w, m.crack(entries[w]), wl);
    }
    out.resetStencilModes();
}

// ============================================================================
//  Салон: фон (до окон)
// ============================================================================
const uint32_t kHeadliner[] = {rgb(13, 12, 19), rgb(17, 16, 25), rgb(22, 21, 31), rgb(29, 27, 39)};

void fillPolyMirror(Canvas& out, const Vec2* p, int n, bool right, uint32_t c) {
    std::array<Vec2, 8> q{};
    const int m = std::min(n, 8);
    for (int i = 0; i < m; ++i)
        q[static_cast<size_t>(i)] = Vec2{right ? static_cast<float>(kW) - p[i].x : p[i].x, p[i].y};
    out.fillPolygon(q.data(), m, c);
}

// Неподвижный фон салона (рисуется один раз в запасённый кадр).
void drawCabinStatic(Canvas& out) {
    // Потолок: дизеринг между тонами, светлее к лобовому стеклу.
    for (int y = -kBackMargin; y < 46; ++y) {
        const RampRow rr = rampRow(kHeadliner, 4, (static_cast<float>(y) + 6.0f) / 44.0f);
        for (int x = -kBackMargin; x < kW + kBackMargin; ++x) out.plot(x, y, rampPick(rr, x, y));
    }
    // Швы обивки сходятся к лобовому стеклу.
    const uint32_t seam = rgb(11, 10, 16);
    out.drawLine(64, -4, 102, 33, seam);
    out.drawLine(256, -4, 218, 33, seam);
    out.drawLine(40, 12, 104, 14, seam);
    out.drawLine(216, 14, 280, 12, seam);

    // Рамка люка (тёмный проём; стекло дорисуется сверху).
    {
        const Vec2 frame[4] = {{105.0f, -8.0f}, {215.0f, -8.0f}, {196.0f, 25.0f}, {124.0f, 25.0f}};
        out.fillPolygon(frame, 4, rgb(11, 10, 16));
    }
    // Плафон освещения (выключен).
    out.fillRect(150, 26, 20, 6, rgb(12, 12, 16));
    out.fillRect(151, 27, 18, 4, rgb(36, 34, 42));
    out.fillRect(152, 27, 16, 1, rgb(52, 50, 60));

    for (int side = 0; side < 2; ++side) {
        const bool right = side == 1;
        auto X = [right](int x) { return right ? kW - x : x; };
        // Ручка над дверью.
        const uint32_t hc = rgb(36, 34, 46);
        out.drawLine(X(6), 16, X(9), 19, hc);
        out.drawLine(X(9), 19, X(26), 22, hc);
        out.drawLine(X(26), 22, X(29), 19, hc);
        out.drawLine(X(9), 18, X(26), 21, rgb(50, 48, 62));
        // Задняя дверь.
        fillPolyMirror(out, kDoorL, 5, right, rgb(21, 20, 29));
        const Vec2 sill[4] = {{-8.0f, 104.9f}, {52.0f, 98.0f}, {53.0f, 102.0f}, {-8.0f, 109.0f}};
        fillPolyMirror(out, sill, 4, right, rgb(30, 29, 40));
        const Vec2 arm[4] = {{-8.0f, 127.0f}, {58.0f, 120.0f}, {60.0f, 129.0f}, {-8.0f, 137.0f}};
        fillPolyMirror(out, arm, 4, right, rgb(31, 30, 41));
        const Vec2 armTop[4] = {{-8.0f, 127.0f}, {58.0f, 120.0f}, {58.0f, 121.0f}, {-8.0f, 128.0f}};
        fillPolyMirror(out, armTop, 4, right, rgb(46, 44, 58));
        // Ручка двери (хромированный рычажок в нише).
        const int hx = right ? kW - 46 : 34;
        out.fillRect(hx, 111, 12, 5, rgb(12, 12, 17));
        out.fillRect(hx + 2, 113, 8, 1, rgb(110, 110, 124));
        // Ручка стеклоподъёмника.
        const int cx = right ? kW - 44 : 44;
        const int dir = right ? -1 : 1;
        out.fillCircle(cx, 144, 2, rgb(34, 33, 44));
        out.drawLine(cx, 144, cx - dir * 6, 150, rgb(40, 39, 52));
        out.fillCircle(cx - dir * 7, 151, 1, rgb(60, 58, 72));
        // Решётка динамика.
        const int gx = right ? kW - 38 : 12;
        for (int gy = 156; gy < 172; gy += 3) {
            for (int gxx = 0; gxx < 24; gxx += 3) out.plot(gx + gxx + ((gy / 3) & 1), gy, rgb(12, 12, 16));
        }
        // Средняя стойка с ремнём безопасности, передняя стойка.
        fillPolyMirror(out, kBPillarL, 4, right, rgb(23, 22, 31));
        out.drawLine(X(46), 30, X(52), 97, rgb(44, 44, 60));
        const uint32_t belt = rgb(33, 33, 40);
        for (int k = 0; k < 3; ++k) out.drawLine(X(53 + k), 34, X(58 + k), 92, belt);
        fillPolyMirror(out, kAPillarL, 4, right, rgb(18, 17, 25));
    }
}

// Вмятина в потолке там, где стоит монстр: обивка выгибается вниз —
// тёмный полумесяц сверху, светлая кромка снизу и складки ткани.
void drawDent(Canvas& out, const Frame& f, const Monster& m) {
    const SceneData& s = *f.s;
    if (s.dentAmt > 0.01f) {
        const float pulse = std::min(1.0f, saturate(m.stepPulse()) * 0.6f + s.dentKick);
        const float rx = lerpf(36.0f, 18.0f, saturate(m.roofZ())) * (1.0f + 0.25f * pulse);
        const float ry = rx * 0.45f;
        const float k = s.dentAmt * (0.75f + 0.25f * pulse);
        const float dcx = s.dentX, dcy = s.dentY;
        const uint32_t hi = scaleColor(rgb(74, 70, 90), f.amb);
        for (int y = floori(dcy - ry - 2.0f); y <= floori(dcy + ry + 3.0f); ++y) {
            for (int x = floori(dcx - rx - 1.0f); x <= floori(dcx + rx + 1.0f); ++x) {
                const float ex = (static_cast<float>(x) + 0.5f - dcx) / rx;
                const float ey = (static_cast<float>(y) + 0.5f - dcy) / ry;
                const float d = ex * ex + ey * ey;
                if (d >= 1.0f) continue;
                const float edgeK = 1.0f - d;
                if (ey < 0.25f) {
                    out.blendPixel(x, y, rgb(1, 1, 3), 0.95f * k * std::sqrt(edgeK) * saturate(0.7f - ey));
                } else {
                    const float rim = saturate((d - 0.3f) / 0.45f) * saturate((ey - 0.25f) * 2.5f);
                    out.blendPixel(x, y, hi, 0.9f * k * rim);
                }
            }
        }
        // Складки: короткие тёмные лучи от центра.
        for (int i = 0; i < 6; ++i) {
            const float a = (static_cast<float>(i) + 0.5f) * kTau / 6.0f + 0.3f;
            const float r0 = 0.25f, r1 = 0.8f + 0.15f * pulse;
            out.blendLine(roundi(dcx + std::cos(a) * rx * r0), roundi(dcy + std::sin(a) * ry * r0),
                          roundi(dcx + std::cos(a) * rx * r1), roundi(dcy + std::sin(a) * ry * r1),
                          rgb(4, 4, 7), 0.55f * k);
        }
    }

}

// ============================================================================
//  Салон: передний план (после окон)
// ============================================================================
void fillRoundRect(Canvas& out, int x, int y, int w, int h, int r, uint32_t c) {
    r = std::min(r, std::min(w, h) / 2);
    out.fillRect(x + r, y, w - 2 * r, h, c);
    out.fillRect(x, y + r, r, h - 2 * r, c);
    out.fillRect(x + w - r, y + r, r, h - 2 * r, c);
    out.fillCircle(x + r, y + r, r, c);
    out.fillCircle(x + w - 1 - r, y + r, r, c);
    out.fillCircle(x + r, y + h - 1 - r, r, c);
    out.fillCircle(x + w - 1 - r, y + h - 1 - r, r, c);
}

void drawTrims(Canvas& out, const Frame& f) {
    const uint32_t rubber = rgb(8, 8, 12);
    const uint32_t lip = scaleColor(rgb(40, 38, 52), f.amb);
    for (int side = 0; side < 2; ++side) {
        const bool right = side == 1;
        auto X = [right](float x) { return right ? static_cast<float>(kW) - x : x; };
        // Верхняя кромка бокового окна (за ней прячутся «корни» пальцев).
        thickSeg(out, X(-8.0f), 20.0f, X(46.0f), 29.5f, 3.0f, 3.0f, rubber);
        out.drawLine(roundi(X(-8.0f)), 22, roundi(X(46.0f)), 31, lip);
        thickSeg(out, X(46.0f), 30.0f, X(52.0f), 98.0f, 2.0f, 2.0f, rubber);
        thickSeg(out, X(-8.0f), 105.0f, X(52.0f), 98.0f, 2.0f, 2.0f, rubber);
        // Переднее стекло — тонкая рамка.
        thickSeg(out, X(57.0f), 31.0f, X(85.0f), 34.0f, 1.5f, 1.5f, rubber);
    }
    // Люк.
    thickSeg(out, 112.0f, -1.0f, 128.0f, 22.0f, 2.5f, 2.5f, rubber);
    thickSeg(out, 208.0f, -1.0f, 192.0f, 22.0f, 2.5f, 2.5f, rubber);
    thickSeg(out, 127.0f, 22.5f, 193.0f, 22.5f, 2.5f, 2.5f, rubber);
    out.drawLine(128, 24, 192, 24, scaleColor(rgb(46, 44, 58), f.amb));
    // Лобовое: кромка обивки сверху.
    out.fillRect(88, 32, 144, 2, rubber);
    out.fillRect(92, 31, 136, 1, scaleColor(rgb(34, 32, 44), f.amb));
    thickSeg(out, 92.0f, 34.0f, 70.0f, 84.0f, 1.6f, 1.6f, rubber);
    thickSeg(out, 228.0f, 34.0f, 250.0f, 84.0f, 1.6f, 1.6f, rubber);
}

// Неподвижная часть приборной панели («запекается» один раз).
void drawDashStatic(Canvas& out) {
    const Vec2 dash[4] = {{56.0f, 84.0f}, {264.0f, 84.0f}, {270.0f, 110.0f}, {50.0f, 110.0f}};
    out.fillPolygon(dash, 4, rgb(15, 14, 21));
    out.fillRect(60, 84, 200, 1, rgb(44, 42, 52));
    out.fillRect(60, 85, 200, 5, rgb(20, 19, 27));
    for (int x = 96; x < 228; x += 4) out.plot(x, 87, rgb(9, 9, 13));
    // Центральная консоль с магнитолой (видна между креслами).
    out.fillRect(146, 89, 28, 22, rgb(17, 17, 24));
    out.fillRect(149, 89, 22, 3, rgb(7, 10, 10));
    for (int i = 0; i < 6; ++i) out.plot(149 + i * 4, 94, rgb(30, 30, 40));
}

// Подсветка приборов и дисплей магнитолы (гаснут с двигателем).
void drawDashLights(Canvas& out, const Frame& f) {
    if (f.engine > 0.02f) {
        // Частота радиостанции: сегменты дисплея.
        const uint32_t lcd = scaleColor(rgb(60, 200, 170), 0.45f * f.engine);
        for (int i = 0; i < 18; ++i) {
            if ((i % 5) != 4 && (i * 7 + 3) % 6 != 0) out.plot(151 + i, 90, lcd);
        }
        addGlow(out, 160.0f, 90.0f, 8.0f, kTeal, 0.06f * f.engine);
    }
    // Подсветка приборов со стороны водителя: бирюзовый ореол за папой.
    if (f.engine > 0.02f) {
        addGlow(out, 124.0f, 86.0f, 24.0f, kTeal, 0.16f * f.engine);
        addGlow(out, 126.0f, 88.0f, 10.0f, rgb(90, 230, 210), 0.14f * f.engine);
    }
}

void drawMirror(Canvas& out, const Frame& f, const Monster& m) {
    const SceneData& s = *f.s;
    out.fillRect(159, 33, 2, 4, rgb(10, 10, 14));
    fillRoundRect(out, 145, 36, 30, 10, 3, rgb(9, 9, 12));
    // Отражение: тёмная дорога позади, отсветы своих стоп-сигналов.
    out.setClip(147 + f.ox, 38 + f.oy, 26, 6);
    out.fillRect(147, 38, 26, 6, rgb(10, 11, 22));
    const Vec2 road[4] = {{160.0f, 41.0f}, {161.0f, 41.0f}, {178.0f, 44.0f}, {142.0f, 44.0f}};
    out.fillPolygon(road, 4, rgb(16, 15, 20));
    out.glow(152, 44, 4, rgb(200, 20, 16), 0.14f);
    out.glow(168, 44, 4, rgb(200, 20, 16), 0.14f);
    // Фонари, которые уже позади, удаляются к горизонту.
    for (const Lamp& l : s.lamps) {
        if (!l.alive || l.d > -0.05f) continue;
        const float k = saturate(-l.d / kLampTail);
        const int lx = roundi(160.0f - l.side * lerpf(16.0f, 2.0f, std::sqrt(k)));
        const int ly = roundi(lerpf(36.0f, 41.0f, std::sqrt(k)));
        out.glow(lx, ly, 4, kWarmLight, 0.6f * (1.0f - k));
        out.plot(lx, ly, scaleColor(rgb(255, 220, 160), 1.0f - k));
    }
    // Монстр на задней части крыши: в зеркале мелькают глаза.
    const MonsterState ms = m.state();
    if ((ms == MonsterState::Landing || ms == MonsterState::Retreating) && m.roofZ() < 0.25f) {
        const float k = saturate((0.25f - m.roofZ()) / 0.2f);
        const float mx = 160.0f - m.roofX() * 8.0f;
        out.blendEllipse(roundi(mx), 38, 6, 3, rgb(2, 2, 4), 0.9f * k);
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            const int ex = roundi(mx) + sgn * 2;
            out.glow(ex, 39, 2, kPupilGlow, 0.4f * k);
            out.blendPixel(ex, 39, kPupilCore, k);
        }
    }
    out.blendLine(149, 43, 154, 38, rgb(170, 180, 210), 0.12f);
    out.resetClip();

    // «Ёлочка»-ароматизатор на нитке.
    const float a = s.pendA;
    const float tx = 167.0f + std::sin(a) * 5.0f, ty = 46.0f + std::cos(a) * 5.0f;
    out.drawLine(167, 46, roundi(tx), roundi(ty), rgb(40, 40, 46));
    const uint32_t g0 = litColor(rgb(16, 40, 26), kWarmLight, f.light * 0.6f);
    const uint32_t g1 = litColor(rgb(28, 64, 40), kWarmLight, f.light * 0.6f);
    const int ix = roundi(tx), iy = roundi(ty);
    out.plot(ix, iy + 1, g1);
    out.fillRect(ix - 1, iy + 2, 3, 1, g0);
    out.plot(ix, iy + 2, g1);
    out.fillRect(ix - 1, iy + 3, 3, 1, g0);
    out.fillRect(ix - 2, iy + 4, 5, 1, g0);
    out.plot(ix - 1, iy + 4, g1);
    out.fillRect(ix - 2, iy + 5, 5, 1, g0);
    out.plot(ix, iy + 6, rgb(30, 22, 16));
}

// Голова родителя со спины: силуэт с подсвеченной кромкой.
struct HeadLook {
    float cx = 0.0f, cy = 0.0f;
    float rx = 12.0f, ry = 14.0f;
    float turn = 0.0f;
    uint32_t hair = rgb(14, 12, 12);
    uint32_t hairHi = rgb(30, 26, 24); // блик на макушке
    uint32_t skin = rgb(46, 36, 34);
    uint32_t rimL = rgb(70, 64, 60);   // кромка слева (луна)
    uint32_t rimR = rgb(70, 64, 60);   // кромка справа
};

// Эллипс волос построчно: по бокам — кромка света, сильнее внизу (там за
// головой яркая дорога), на макушке — мягкий блик.
void drawHairShape(Canvas& out, float cx, float cy, float rx, float ry, const HeadLook& h) {
    const int top = floori(cy - ry), bot = floori(cy + ry);
    for (int y = top; y <= bot; ++y) {
        const float dy = (static_cast<float>(y) + 0.5f - cy) / ry;
        if (std::fabs(dy) >= 1.0f) continue;
        const float half = rx * std::sqrt(1.0f - dy * dy);
        const int xl = roundi(cx - half), xr = roundi(cx + half) - 1;
        if (xr < xl) continue;
        out.fillRect(xl, y, xr - xl + 1, 1, h.hair);
        const float k = smoothstep(-0.5f, 0.6f, dy);
        out.plot(xl, y, lerpColor(h.hair, h.rimL, k));
        out.plot(xr, y, lerpColor(h.hair, h.rimR, k));
        if (dy < -0.55f && dy > -0.9f) {
            const int hl = roundi(cx - half * 0.35f), hr = roundi(cx + half * 0.1f);
            out.fillRect(hl, y, std::max(1, hr - hl), 1, lerpColor(h.hair, h.hairHi, 0.6f));
        }
    }
}

void drawHeadBack(Canvas& out, const HeadLook& h, bool bun) {
    const int cy = roundi(h.cy);
    // Уши: при повороте одно уходит за голову, другое смещается к центру.
    for (int sgn = -1; sgn <= 1; sgn += 2) {
        const float fs = static_cast<float>(sgn);
        if (h.turn * fs > 0.5f) continue;
        const int ex = roundi(h.cx + fs * (h.rx - 0.5f) - h.turn * 3.0f);
        out.fillEllipse(ex, cy + 4, 2, 3, h.skin);
        out.plot(ex + sgn * 2, cy + 4, sgn < 0 ? h.rimL : h.rimR);
    }
    // Профиль (нос/скула) со стороны поворота.
    if (std::fabs(h.turn) > 0.3f) {
        const float fs = h.turn > 0.0f ? 1.0f : -1.0f;
        const float k = smoothstep(0.3f, 0.9f, std::fabs(h.turn));
        const int px = roundi(h.cx + fs * (h.rx - 1.0f + 2.5f * k));
        out.fillEllipse(px, cy + 3, 2, 3, h.skin);
        out.plot(px + roundi(fs * 2.0f), cy + 3, h.skin);
        out.plot(px + roundi(fs * 2.0f), cy + 2, fs < 0.0f ? h.rimL : h.rimR);
    }
    if (bun) {
        // Пучок на затылке, перехваченный резинкой.
        HeadLook b = h;
        drawHairShape(out, h.cx + 1.0f, h.cy - h.ry + 1.5f, 5.5f, 5.0f, b);
        out.fillRect(roundi(h.cx) - 3, roundi(h.cy - h.ry) + 5, 8, 1, rgb(70, 44, 34));
    }
    drawHairShape(out, h.cx, h.cy, h.rx, h.ry, h);
    if (bun) {
        // Выбившиеся пряди.
        out.plot(roundi(h.cx) - 5, roundi(h.cy - h.ry) + 6, h.hairHi);
        out.plot(roundi(h.cx) + 6, roundi(h.cy - h.ry) + 8, h.hairHi);
    }
}

// ============================================================================
//  Безликие родители (бесконечный режим)
// ============================================================================
// Родители медленно оборачиваются к ребёнку: затылок -> профиль -> лицо. Лица
// нет: гладкая бледная кожа и лишь едва заметные впадины там, где должны быть
// глаза, нос и рот. Голова рисуется попиксельно как повёрнутый эллипсоид:
// для каждого пикселя считается нормаль, она переводится в координаты головы
// (a — вбок, b — вниз, c — «вперёд лица»), и по ним решается, волосы это,
// кожа или ухо.

constexpr int kHeadrestTopY = 57; // выше этой строки спинки кресел ничего не закрывают

// Проход отрисовки: 0 — за спинками кресел (как обычно), 1 — поверх них, когда
// родитель перегибается к ребёнку. Во втором проходе граница «перед/за
// подголовником» опускается сверху вниз по frontK — голова как будто
// переползает через верх подголовника, без резкой смены порядка.
struct HeadPass {
    int pass = 0;
    float frontK = 0.0f;
};

inline bool headPassAllows(const HeadPass& hp, int y) {
    if (hp.pass == 0) return true;
    return y >= kHeadrestTopY && static_cast<float>(y - kHeadrestTopY) < hp.frontK * 32.0f;
}

// Дешёвый «колокол» вместо exp(-q): (1 - q/4)^4, ноль при q >= 4.
inline float softBump(float q) {
    if (q >= 4.0f) return 0.0f;
    const float u = 1.0f - 0.25f * q;
    const float u2 = u * u;
    return u2 * u2;
}

struct FacelessLook {
    float cx = 0.0f, cy = 0.0f, rx = 12.0f, ry = 13.5f;
    float roll = 0.0f;   // наклон головы набок, рад
    float yaw = 0.0f;    // 0 — затылок к зрителю, 1 — лицо к зрителю
    float side = 1.0f;   // куда идёт поворот: +1 — вправо (папа), -1 — влево (мама)
    bool mom = false;
    uint32_t hair = 0, hairHi = 0, rimL = 0, rimR = 0;
    uint32_t earSkin = 0; // цвет ушей со спины (как в обычной позе)
    uint32_t rim = 0;    // холодный контровой свет (от лобового стекла)
    float rimK = 0.0f;
    float fill = 0.0f;   // рассеянный свет со стороны ребёнка
    float glow = 0.0f;   // экран консоли снизу
    float amb = 1.0f;
    std::array<uint32_t, 6> skin{}; // рампа кожи от тени к свету
};

// Бледная кожа: от почти чёрного к «восковому» светлому.
const uint32_t kPaleSkin[6] = {rgb(10, 10, 16),   rgb(34, 33, 42),    rgb(66, 64, 72),
                               rgb(104, 101, 104), rgb(146, 142, 138), rgb(194, 188, 176)};

// Кусок волос (эллипс) с кромкой света, как у drawHairShape, но с маской прохода.
void drawHairBlobMasked(Canvas& out, float cx, float cy, float rx, float ry, const FacelessLook& h,
                        const HeadPass& hp) {
    const int top = floori(cy - ry), bot = floori(cy + ry);
    for (int y = top; y <= bot; ++y) {
        const float dy = (static_cast<float>(y) + 0.5f - cy) / ry;
        if (std::fabs(dy) >= 1.0f || !headPassAllows(hp, y)) continue;
        const float half = rx * std::sqrt(1.0f - dy * dy);
        const int xl = roundi(cx - half), xr = roundi(cx + half) - 1;
        const float k = smoothstep(-0.5f, 0.6f, dy);
        for (int x = xl; x <= xr; ++x) {
            uint32_t c = h.hair;
            if (x == xl) c = lerpColor(h.hair, h.rimL, k);
            else if (x == xr) c = lerpColor(h.hair, h.rimR, k);
            out.plot(x, y, c);
        }
    }
}

// Голова, повёрнутая на yaw. Рисует уши, пучок (у мамы) и саму голову.
void drawFacelessHead(Canvas& out, const FacelessLook& h, const HeadPass& hp) {
    const float phi = h.yaw * kPi;
    // Направление «лица» и «вбок» в координатах экрана (z — к зрителю).
    const float fx = std::sin(phi) * h.side, fz = -std::cos(phi);
    const float ux = std::cos(phi), uz = std::sin(phi) * h.side;
    const float cr = std::cos(h.roll), sr = std::sin(h.roll);
    // Точка головы (в долях радиусов, локальные a/b/c) -> экран.
    auto project = [&](float a, float b, float c, float& sx, float& sy, float& sz) {
        const float lx = a * ux + c * fx, lz = a * uz + c * fz;
        const float px = lx * h.rx, py = b * h.ry;
        sx = h.cx + px * cr - py * sr;
        sy = h.cy + px * sr + py * cr;
        sz = lz;
    };

    // ---- Уши: торчат из-за контура головы, а в профиле ближнее ухо лежит
    // поверх головы (у мамы уши скрываются под волосами) ----
    auto drawEars = [&](bool nearSide) {
        if (h.mom && h.yaw >= 0.3f) return;
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            const float fs = static_cast<float>(sgn);
            float ex = 0.0f, ey = 0.0f, ez = 0.0f;
            project(fs * 0.98f, 0.28f, -0.05f, ex, ey, ez);
            if (ez < -0.45f || (ez > 0.35f) != nearSide) continue;
            const int ix = roundi(ex), iy = roundi(ey);
            const uint32_t earC = lerpColor(h.earSkin, h.skin[2], smoothstep(0.2f, 0.8f, h.yaw));
            for (int yy = -3; yy <= 3; ++yy) {
                for (int xx = -2; xx <= 2; ++xx) {
                    if (xx * xx * 9 + yy * yy * 4 > 36) continue;
                    if (!headPassAllows(hp, iy + yy)) continue;
                    const bool edge = (ex < h.cx) ? xx == -2 || (xx == -1 && std::abs(yy) == 2)
                                                  : xx == 2 || (xx == 1 && std::abs(yy) == 2);
                    out.plot(ix + xx, iy + yy, edge ? lerpColor(earC, h.rim, 0.5f * h.rimK + 0.2f) : earC);
                }
            }
        }
    };
    drawEars(false);

    // ---- Пучок мамы: на затылке; когда она обернулась — за головой ----
    float bx = 0.0f, by = 0.0f, bz = 0.0f;
    if (h.mom) {
        project(0.0f, -0.88f, -0.5f, bx, by, bz);
        if (bz <= 0.0f) drawHairBlobMasked(out, bx, by, 5.5f, 5.0f, h, hp);
    }

    // ---- Сама голова ----
    const float ext = std::max(h.rx, h.ry) + 1.0f;
    const int xa = floori(h.cx - ext), xb = floori(h.cx + ext) + 1;
    const int ya = floori(h.cy - ext), yb = floori(h.cy + ext) + 1;
    const float hairTop = h.mom ? -0.50f : -0.42f;
    const float faceHalfW = h.mom ? 0.64f : 0.80f;
    for (int y = ya; y <= yb; ++y) {
        for (int x = xa; x <= xb; ++x) {
            if (!headPassAllows(hp, y)) continue;
            const float dx = static_cast<float>(x) + 0.5f - h.cx, dy = static_cast<float>(y) + 0.5f - h.cy;
            const float ly = (-dx * sr + dy * cr) / h.ry;
            // Голова сужается к подбородку (у лица сильнее, чем у затылка).
            const float jaw = ly > 0.0f ? 1.0f - (0.10f + 0.10f * h.yaw) * ly * ly : 1.0f;
            const float lx = (dx * cr + dy * sr) / (h.rx * jaw);
            const float r2 = lx * lx + ly * ly;
            if (r2 >= 1.0f) continue;
            const float nz = std::sqrt(1.0f - r2);
            const float a = lx * ux + nz * uz;
            const float b = ly;
            const float c = lx * fx + nz * fz;
            const float hairline = hairTop + 0.32f * a * a;
            const bool face = c > 0.14f && b > hairline && std::fabs(a) < faceHalfW;
            uint32_t col;
            if (face) {
                // Освещение: рассеянный свет от ребёнка + экран консоли снизу.
                const float lc = std::max(0.0f, 0.5f * ly + 0.866f * nz);
                float level = h.amb * (0.08f + h.fill * (0.25f + 0.85f * nz * nz) + h.glow * lc * lc);
                // Едва заметный рельеф: мягкие впадины глазниц, намёк на
                // переносицу, тень под ней и там, где должен быть рот.
                float relief = 0.0f;
                for (int sgn = -1; sgn <= 1; sgn += 2) {
                    const float da = a - static_cast<float>(sgn) * 0.33f, db = b + 0.02f;
                    relief -= 0.30f * softBump((da * da + db * db * 1.3f) / 0.028f);
                }
                relief += 0.08f * softBump((a * a) / 0.006f) * smoothstep(-0.10f, 0.05f, b) *
                          smoothstep(0.40f, 0.28f, b);
                relief -= 0.14f * softBump(a * a / 0.012f + (b - 0.43f) * (b - 0.43f) / 0.004f);
                relief -= 0.12f * softBump(a * a / 0.05f + (b - 0.63f) * (b - 0.63f) / 0.005f);
                // Тень от волос у линии роста и на висках.
                if (b - hairline < 0.08f) relief -= 0.15f;
                level *= 1.0f + relief;
                // Кожа гладкая: сплошной упорядоченный дизеринг между ступенями
                // рампы (без резких полос, которые читались бы как черты лица).
                col = rampPick(rampRow(h.skin.data(), 6, level), x, y);
                // Холодный контровой свет из лобового стекла — тонкая кромка, сильнее сверху.
                const float rimE = smoothstep(0.80f, 0.98f, r2) * h.rimK * (0.65f - 0.35f * ly);
                if (rimE > 0.05f) col = lerpColor(col, h.rim, std::min(0.7f, rimE));
            } else {
                // Волосы: кромка света по контуру (сильнее внизу) и блик на макушке.
                col = h.hair;
                if (c < -0.2f && b < -0.5f && b > -0.92f && std::fabs(a) < 0.4f)
                    col = lerpColor(h.hair, h.hairHi, 0.6f);
                // Волосы прядями: от макушки вниз чуть светлее и темнее полосами.
                if (c > 0.0f) {
                    const float fan = a / (1.4f - b) * (h.mom ? 3.5f : 4.8f);
                    if (fan - std::floor(fan) > 0.8f) col = lerpColor(col, h.hairHi, 0.35f);
                    // Пробор у мамы — тонкая светлая линия посередине.
                    if (h.mom && std::fabs(a) < 0.05f && b < hairline)
                        col = lerpColor(h.hair, h.hairHi, 0.9f);
                }
                if (r2 > 0.80f) {
                    const float k = smoothstep(-0.5f, 0.6f, ly) * smoothstep(0.80f, 0.98f, r2);
                    col = lerpColor(col, lx < 0.0f ? h.rimL : h.rimR, k);
                    const float rimE = smoothstep(0.88f, 0.99f, r2) * h.rimK * 0.45f * (0.5f - 0.5f * ly);
                    if (rimE > 0.02f) col = lerpColor(col, h.rim, rimE);
                }
            }
            out.plot(x, y, col);
        }
    }
    drawEars(true);
    if (h.mom && bz > 0.0f) {
        drawHairBlobMasked(out, bx, by, 5.5f, 5.0f, h, hp);
        if (hp.pass == 0) out.fillRect(roundi(bx) - 4, roundi(by) + 3, 8, 1, rgb(70, 44, 34));
    }
}

// Шея и плечи родителя, перегнувшегося через спинку к ребёнку (второй проход).
void drawLeanBody(Canvas& out, const FacelessLook& h, float lean, uint32_t cloth, const HeadPass& hp) {
    if (lean <= 0.01f) return;
    const float topY = h.cy + h.ry * 0.55f, botY = 81.0f;
    const float topX = h.cx, botX = h.cx - h.side * 4.0f;
    // Плечи низким горбом поднимаются над верхом спинки; кромку ловит свет.
    const float shY = lerpf(92.0f, 86.0f, lean), shRx = h.rx * 1.35f, shRy = 7.0f;
    const int shTop = floori(shY - shRy);
    for (int y = shTop; y <= 82; ++y) {
        const float dy = (static_cast<float>(y) + 0.5f - shY) / shRy;
        if (std::fabs(dy) >= 1.0f) continue;
        const float half = shRx * std::sqrt(1.0f - dy * dy);
        const int xl = roundi(botX - half), xr = roundi(botX + half) - 1;
        for (int x = xl; x <= xr; ++x) {
            if (!headPassAllows(hp, y)) continue;
            const bool edge = y == shTop + 1 || ((x == xl || x == xr) && y < shTop + 4);
            out.plot(x, y, edge ? lerpColor(cloth, h.rim, 0.15f + 0.2f * h.rimK) : cloth);
        }
    }
    // Шея: от подбородка вниз, к основанию сдвигается к своему креслу.
    const int ya = floori(topY), yb = floori(botY);
    for (int y = ya; y <= yb; ++y) {
        const float t = saturate((static_cast<float>(y) - topY) / (botY - topY));
        const float cx = lerpf(topX, botX, t), hw = h.rx * lerpf(0.27f, 0.33f, t);
        // Под подбородком — глубокая тень, ниже шею чуть подсвечивает консоль.
        const float level = h.amb * (0.03f + h.fill * 0.18f + h.glow * 0.30f * t);
        const int xl = roundi(cx - hw), xr = roundi(cx + hw) - 1;
        for (int x = xl; x <= xr; ++x) {
            if (!headPassAllows(hp, y)) continue;
            uint32_t c = rampPick(rampRow(h.skin.data(), 6, level), x, y);
            if (x == xl || x == xr) c = lerpColor(c, h.rim, 0.12f * h.rimK);
            out.plot(x, y, c);
        }
    }
}

// Пальцы, вцепившиеся в верх спинки со стороны родителя (видны поверх кромки).
void drawGripHand(Canvas& out, int x0, float k, const FacelessLook& h) {
    if (k <= 0.02f) return;
    const int len = clampi(roundi(1.0f + 3.0f * k), 1, 4);
    const uint32_t knuckle = bandRamp(h.skin.data(), 6, h.amb * (0.10f + h.fill * 0.6f), x0, 79);
    const float fingerL = h.amb * (0.16f + h.fill * 0.7f + h.glow * 0.4f);
    const uint32_t finger = bandRamp(h.skin.data(), 6, fingerL, x0, 80);
    out.fillRect(x0, 79, 7, 1, knuckle);
    for (int i = 0; i < 4; ++i) {
        const int fx = x0 + i * 2;
        const int l = std::max(1, len - (i == 0 || i == 3 ? 1 : 0));
        out.fillRect(fx, 80, 1, l, finger);
        out.plot(fx, 80 + l - 1, lerpColor(finger, h.rim, 0.3f)); // ноготь ловит свет
        if (i < 3) out.fillRect(fx + 1, 80, 1, std::max(1, l - 1), rgb(6, 6, 10));
    }
}

const uint32_t kSeatRamp[5] = {rgb(9, 9, 14), rgb(15, 15, 22), rgb(22, 22, 31), rgb(30, 30, 41),
                               rgb(40, 40, 53)};

// Спинка кресла с подголовником (рисуется один раз при создании сцены).
void drawSeat(Canvas& out, int x, int headX) {
    const int w = 86, top = 80, bottom = 188;
    const uint32_t* ramp = kSeatRamp;
    // Подголовник на двух штырях (за ним — голова, перед ним — ничего).
    const uint32_t post = rgb(52, 52, 62);
    out.fillRect(headX - 9, 77, 2, 5, post);
    out.fillRect(headX + 8, 77, 2, 5, post);
    out.plot(headX - 9, 77, rgb(90, 90, 104));
    out.plot(headX + 8, 77, rgb(90, 90, 104));
    for (int y = 58; y < 79; ++y) {
        const float ty = static_cast<float>(y - 58) / 20.0f;
        for (int xx = headX - 21; xx <= headX + 21; ++xx) {
            // Скруглённые углы.
            const int ex = std::max(0, std::abs(xx - headX) - 15),
                      ey = y < 64 ? 64 - y : (y > 72 ? y - 72 : 0);
            if (ex * ex + ey * ey > 36) continue;
            const float tx = static_cast<float>(xx - headX) / 21.0f;
            float l = 0.62f - 0.35f * ty - 0.25f * tx * tx;
            if (y == 58 || (y == 59 && std::abs(xx - headX) > 12)) l += 0.25f;
            if (y == 68) l -= 0.12f;
            out.plot(xx, y, bandRamp(ramp, 5, l, xx, y));
        }
    }
    // Спинка: светлее вверху, к низу уходит в темноту; боковины выпуклые.
    for (int y = top; y < bottom; ++y) {
        const float ty = static_cast<float>(y - top) / static_cast<float>(bottom - top);
        for (int xx = x; xx < x + w; ++xx) {
            const int lx = xx - x;
            // Скруглённые «плечи» спинки.
            const int cxr = lx < 11 ? 11 - lx : (lx > w - 12 ? lx - (w - 12) : 0);
            const int cyr = y < top + 11 ? top + 11 - y : 0;
            if (cxr * cxr + cyr * cyr > 121) continue;
            float l = 0.58f - 0.5f * ty;
            const bool bolster = lx < 12 || lx >= w - 12;
            if (bolster) {
                const float bx =
                    lx < 12 ? static_cast<float>(lx) / 12.0f : static_cast<float>(w - 1 - lx) / 12.0f;
                l += 0.18f * std::sin(bx * kPi) - 0.12f;
            } else if ((lx % 3) == 0) {
                l -= 0.05f; // рубчик ткани
            }
            if (lx == 12 || lx == w - 13) l -= 0.2f;              // шов боковины
            if (y == top + 1 || (y < top + 4 && cxr > 0)) l += 0.25f; // кромка сверху ловит свет
            if (y >= 134 && !bolster) {
                l -= 0.12f;                                        // карман
                if (y < 136) l += 0.3f;                            // резинка кармана
            }
            out.plot(xx, y, bandRamp(ramp, 5, l, xx, y));
        }
    }
}

// Кресла, тоннель между ними и колени ребёнка (рисуются один раз).
void drawSeatsStatic(Canvas& out) {
    drawSeat(out, 62, 105);
    drawSeat(out, 172, 215);
    // Тоннель между креслами и рычаг КПП (почти весь скрыт консолью).
    out.fillRect(148, 104, 24, 84, rgb(13, 13, 18));
    const Vec2 boot[4] = {{153.0f, 104.0f}, {167.0f, 104.0f}, {170.0f, 120.0f}, {150.0f, 120.0f}};
    out.fillPolygon(boot, 4, rgb(18, 18, 24));
    out.fillRect(157, 104, 6, 3, rgb(22, 22, 28));
    out.fillRect(158, 104, 3, 1, rgb(54, 54, 66));
    // Колени в джинсах.
    out.fillEllipse(86, 186, 26, 16, rgb(24, 30, 48));
    out.fillEllipse(234, 186, 26, 16, rgb(24, 30, 48));
    out.drawLine(66, 175, 96, 171, rgb(38, 46, 70));
    out.drawLine(224, 171, 254, 175, rgb(38, 46, 70));
}

// Рисует draw() на пустом холсте и сохраняет нарисованное отрезками строк.
template <typename DrawFn>
void bakeLayer(BakedLayer& layer, DrawFn draw) {
    constexpr uint32_t kEmpty = 0xFF000000u; // старший байт: пиксель не нарисован
    Canvas tmp(kW, kH);
    tmp.clear(kEmpty);
    draw(tmp);
    layer.runs.clear();
    layer.pix.clear();
    for (int y = 0; y < kH; ++y) {
        int x = 0;
        while (x < kW) {
            if (tmp.get(x, y) == kEmpty) {
                ++x;
                continue;
            }
            LayerRun r;
            r.y = y;
            r.x0 = x;
            r.offset = layer.pix.size();
            while (x < kW && tmp.get(x, y) != kEmpty) layer.pix.push_back(tmp.get(x++, y));
            r.len = x - r.x0;
            layer.runs.push_back(r);
        }
    }
}

// Умножение цвета на яркость a (0..256) — красный и синий одной операцией.
inline uint32_t scale256(uint32_t c, uint32_t a) {
    return ((((c & 0xFF00FFu) * a) >> 8) & 0xFF00FFu) | ((((c & 0x00FF00u) * a) >> 8) & 0x00FF00u);
}

// Копирует слой с учётом тряски. Строки выше markBelowY могут лежать поверх
// стекла — для них трафарет помечается через fillRect (режим записи
// трафарета выставлен вызывающим кодом).
void blitLayer(Canvas& out, const BakedLayer& layer, float amb, int markBelowY) {
    const int ox = out.offsetX(), oy = out.offsetY();
    const uint32_t a = static_cast<uint32_t>(clampi(roundi(amb * 256.0f), 0, 256));
    uint32_t* px = out.data();
    for (const LayerRun& r : layer.runs) {
        if (r.y < markBelowY) out.fillRect(r.x0, r.y, r.len, 1, 0u);
        const int cy = r.y + oy;
        if (cy < 0 || cy >= kH) continue;
        const int start = r.x0 + ox;
        const int xa = std::max(0, start), xb = std::min(kW, start + r.len);
        uint32_t* row = px + static_cast<size_t>(cy) * kW;
        const uint32_t* src = layer.pix.data() + r.offset + static_cast<size_t>(std::max(0, xa - start));
        for (int x = xa; x < xb; ++x) row[x] = a >= 256u ? *src++ : scale256(*src++, a);
    }
}

// Фон салона: копия запасённого кадра со сдвигом тряски.
void blitBack(Canvas& out, const SceneData& s, float amb) {
    if (s.backPix.size() != static_cast<size_t>(kBackW * kBackH)) return;
    const int ox = out.offsetX(), oy = out.offsetY();
    const uint32_t a = static_cast<uint32_t>(clampi(roundi(amb * 256.0f), 0, 256));
    uint32_t* px = out.data();
    for (int y = 0; y < kH; ++y) {
        const int sy = clampi(y - oy + kBackMargin, 0, kBackH - 1);
        const uint32_t* src = s.backPix.data() + static_cast<size_t>(sy) * kBackW +
                              static_cast<size_t>(clampi(kBackMargin - ox, 0, 2 * kBackMargin));
        uint32_t* row = px + static_cast<size_t>(y) * kW;
        if (a >= 256u) std::copy(src, src + kW, row);
        else for (int x = 0; x < kW; ++x) row[x] = scale256(src[x], a);
    }
}

// Параметры безликой головы для текущего кадра (общие для обоих родителей).
FacelessLook facelessBase(const Frame& f, float backLight) {
    FacelessLook h;
    h.amb = f.amb;
    // Свет со стороны ребёнка: луна в заднем стекле плюс фонарь за боковыми окнами.
    h.fill = 0.26f + 0.06f * f.light;
    h.glow = f.v->showConsole ? 0.30f : 0.06f;
    h.rim = rgb(150, 172, 226);
    h.rimK = 0.50f + 0.35f * backLight;
    // Экран консоли на коленях слегка зеленит кожу.
    const float tealK = f.v->showConsole ? 0.10f : 0.0f;
    for (size_t i = 0; i < h.skin.size(); ++i) h.skin[i] = litColor(kPaleSkin[i], kTeal, tealK);
    return h;
}

void drawParentsAndSeats(Canvas& out, const Frame& f, const Parents& p) {
    const float t = f.time;
    const float backLight = 0.35f * f.beams + 0.6f * f.frontLight + 0.5f * f.carGlare;
    // Безликий поворот: пока он идёт, родители молчат и не двигаются — их
    // обычная анимация (кивки, повороты, дрожь) за первые проценты гаснет.
    const float turnT = saturate(f.v->facelessTurn);
    const float alive = 1.0f - smoothstep(0.0f, 0.08f, turnT);
    const float tension = saturate(p.tension()) * alive;
    const float yaw = smoothstep(0.06f, 0.85f, turnT);
    const float lean = smoothstep(0.45f, 1.0f, turnT);
    const float twist = smoothstep(0.10f, 0.80f, turnT);
    HeadPass front;
    front.pass = 1;
    front.frontK = smoothstep(0.50f, 0.85f, turnT);
    FacelessLook dadF, momF;
    uint32_t dadCloth = 0, momCloth = 0;
    // Длинные волосы мамы ниже плеч: обрамляют лицо, когда она обернулась.
    auto momLongHair = [&](const HeadPass& hp) {
        const float sc = 1.0f + 0.12f * lean;
        drawHairBlobMasked(out, momF.cx, momF.cy + (6.0f + 2.0f * yaw) * sc, (13.0f - 0.5f * yaw) * sc,
                           (9.0f + 2.0f * yaw) * sc, momF, hp);
    };
    // ---- Папа (слева, водитель): короткая стрижка, широкие плечи ----
    {
        const float talk = saturate(p.talk(Speaker::Dad)) * alive;
        const float turn = clampf(p.headTurn(Speaker::Dad), -1.0f, 1.0f) * alive;
        const float bob = -std::fabs(std::sin(t * 7.3f)) * talk * 1.4f;
        const float shake = tension > 0.6f ? std::sin(t * 13.0f) * 0.6f * talk : 0.0f;
        HeadLook h;
        h.cx = 105.0f + turn * 3.0f + shake;
        h.cy = 49.0f + bob;
        h.rx = 12.0f;
        h.ry = 13.5f;
        h.turn = turn;
        h.hair = scaleColor(rgb(13, 11, 12), f.amb);
        h.hairHi = scaleColor(rgb(30, 28, 32), f.amb);
        h.skin = litColor(scaleColor(rgb(40, 31, 30), f.amb), kTeal, 0.25f * f.engine);
        h.rimL = lerpColor(h.hair, rgb(96, 104, 130), 0.5f + 0.3f * backLight);
        h.rimR =
            lerpColor(h.hair, litColor(rgb(70, 66, 62), kTeal, 0.25f * f.engine), 0.35f + 0.45f * backLight);
        const uint32_t jacket = scaleColor(rgb(25, 26, 34), f.amb);
        dadCloth = jacket;
        // Плечи шире спинки кресла; при повороте внутреннее плечо поднимается.
        out.fillEllipse(63 + roundi(3.0f * twist), 89 + roundi(2.0f * twist), 10, 9, jacket);
        out.fillEllipse(147 + roundi(2.0f * twist), 89 - roundi(4.0f * twist), 10 + roundi(twist), 9, jacket);
        out.drawLine(55, 83, 60, 80, litColor(jacket, kColdLight, 0.5f));
        if (turnT <= 0.0f) {
            drawHeadBack(out, h, false);
        } else {
            dadF = facelessBase(f, backLight);
            const float sc = 1.0f + 0.12f * lean;
            dadF.cx = h.cx + 12.0f * lean;
            dadF.cy = h.cy + 6.0f * lean;
            dadF.rx = h.rx * sc;
            dadF.ry = h.ry * sc;
            dadF.roll = 0.14f * lean;
            dadF.yaw = yaw;
            dadF.side = 1.0f;
            dadF.hair = h.hair;
            dadF.hairHi = h.hairHi;
            dadF.rimL = h.rimL;
            dadF.rimR = h.rimR;
            dadF.earSkin = h.skin;
            drawFacelessHead(out, dadF, HeadPass{});
        }
    }
    // ---- Мама (справа): пучок волос ----
    {
        const float talk = saturate(p.talk(Speaker::Mom)) * alive;
        const float turn = clampf(p.headTurn(Speaker::Mom), -1.0f, 1.0f) * alive;
        const float bob = -std::fabs(std::sin(t * 8.1f + 1.3f)) * talk * 1.3f;
        HeadLook h;
        h.cx = 215.0f + turn * 3.0f;
        h.cy = 52.0f + bob;
        h.rx = 11.0f;
        h.ry = 12.5f;
        h.turn = turn;
        h.hair = scaleColor(rgb(32, 19, 16), f.amb);
        h.hairHi = scaleColor(rgb(62, 40, 30), f.amb);
        h.skin = scaleColor(rgb(46, 36, 34), f.amb);
        h.rimL = lerpColor(h.hair, rgb(120, 96, 80), 0.35f + 0.5f * backLight);
        h.rimR = lerpColor(h.hair, rgb(140, 110, 88), 0.35f + 0.5f * backLight);
        momCloth = scaleColor(rgb(38, 26, 34), f.amb);
        if (turnT <= 0.0f) {
            // Пышные волосы по бокам.
            drawHairShape(out, h.cx, h.cy + 6.0f, 13.0f, 9.0f, h);
            drawHeadBack(out, h, true);
        } else {
            momF = facelessBase(f, backLight);
            const float sc = 1.0f + 0.12f * lean;
            momF.cx = h.cx - 12.0f * lean;
            momF.cy = h.cy + 5.0f * lean;
            momF.rx = h.rx * sc;
            momF.ry = h.ry * sc;
            momF.roll = -0.18f * lean;
            momF.yaw = yaw;
            momF.side = -1.0f;
            momF.mom = true;
            momF.hair = h.hair;
            momF.hairHi = h.hairHi;
            momF.rimL = h.rimL;
            momF.rimR = h.rimR;
            momF.earSkin = h.skin;
            momLongHair(HeadPass{});
            drawFacelessHead(out, momF, HeadPass{});
        }
    }
    blitLayer(out, f.v->showConsole ? f.s->seatLayerLit : f.s->seatLayer, f.amb, 96);
    if (turnT > 0.0f && front.frontK > 0.0f) {
        // Второй проход: родители перегибаются через спинки — поверх кресел.
        drawLeanBody(out, dadF, lean, dadCloth, front);
        drawFacelessHead(out, dadF, front);
        drawLeanBody(out, momF, lean, momCloth, front);
        momLongHair(front);
        drawFacelessHead(out, momF, front);
        const float grip = smoothstep(0.72f, 1.0f, turnT);
        drawGripHand(out, 129, grip, dadF);
        drawGripHand(out, 185, grip, momF);
    }
}

// ============================================================================
//  Свет: полосы фонаря и встречной машины по салону
// ============================================================================
// Горизонтальная полоса света (центр centerY, полувысота halfH), сила k,
// по ширине модулируется: side < 0 — сильнее слева, > 0 — справа.
void applyLightBand(Canvas& out, float centerY, float halfH, uint32_t col, float k, float side) {
    if (k <= 0.01f) return;
    const int ya = std::max(0, floori(centerY - halfH));
    const int yb = std::min(kH - 1, floori(centerY + halfH) + 1);
    if (yb < ya) return;
    int colK[kW];
    for (int x = 0; x < kW; ++x) {
        const float nx = (static_cast<float>(x) - 160.0f) / 160.0f;
        colK[x] = static_cast<int>(256.0f * saturate(0.75f + 0.45f * nx * side));
    }
    const int lr = colR(col), lg = colG(col), lb = colB(col);
    const int ox = out.offsetX(), oy = out.offsetY();
    // Стёкла (а поверх них может лежать и салон) — только там спрашиваем
    // трафарет; остальное заведомо салон.
    const Vec2* const windows[4] = {kDrawLeft, kDrawRight, kDrawWindshield, kDrawSunroof};
    uint8_t ask[kW];
    uint32_t* px = out.data();
    for (int y = ya; y <= yb; ++y) {
        const float dy = (static_cast<float>(y) + 0.5f - centerY) / halfH;
        if (std::fabs(dy) >= 1.0f) continue;
        const float wgt = (1.0f - dy * dy) * (1.0f - dy * dy);
        const int rowK = static_cast<int>(256.0f * k * wgt);
        if (rowK <= 0) continue;
        std::fill(ask, ask + kW, static_cast<uint8_t>(0));
        for (const Vec2* w : windows) {
            float xl = 0.0f, xr = 0.0f;
            if (!polySpan(w, 4, static_cast<float>(y - oy) + 0.5f, xl, xr)) continue;
            const int xa = std::max(0, floori(xl) - 1 + ox),
                      xb = std::min(kW - 1, static_cast<int>(std::ceil(xr)) + 1 + ox);
            if (xb >= xa) std::fill(ask + xa, ask + xb + 1, static_cast<uint8_t>(1));
        }
        uint32_t* row = px + static_cast<size_t>(y) * kW;
        for (int x = 0; x < kW; ++x) {
            if (ask[x]) {
                const uint8_t st = out.stencilAt(x, y);
                if (st != 0 && st != kStInterior) continue;
            }
            const int kk = (rowK * colK[x]) >> 8;
            const uint32_t c = row[x];
            const int cr = static_cast<int>((c >> 16) & 255u), cg = static_cast<int>((c >> 8) & 255u),
                      cb = static_cast<int>(c & 255u);
            // Умножение на свет + небольшая добавка (как litColor, в целых числах).
            int r = cr + ((cr * lr * kk * 5) >> 17) + ((lr * kk) >> 11);
            int g = cg + ((cg * lg * kk * 5) >> 17) + ((lg * kk) >> 11);
            int b = cb + ((cb * lb * kk * 5) >> 17) + ((lb * kk) >> 11);
            r = r > 255 ? 255 : r;
            g = g > 255 ? 255 : g;
            b = b > 255 ? 255 : b;
            row[x] =
                (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
        }
    }
}

// Свет спереди (фонарь ещё впереди): потолок у лобового и кромки салона.
void applyFrontLight(Canvas& out, float k, uint32_t col, float side) {
    if (k <= 0.01f) return;
    applyLightBand(out, 30.0f, 34.0f, col, k * 0.6f, side);
}

void applyLighting(Canvas& out, const Frame& f) {
    const SceneData& s = *f.s;
    for (const Lamp& l : s.lamps) {
        if (!l.alive) continue;
        if (l.d > 0.0f && l.d < 1.0f) applyFrontLight(out, lampGlowAt(l.d) * 0.8f, kWarmLight, l.side);
        if (l.d <= 0.25f && l.d > -0.95f) {
            const float p = (0.25f - l.d) / 1.2f;
            const float k = 0.7f * std::pow(std::sin(p * kPi), 0.7f);
            applyLightBand(out, lerpf(-20.0f, 205.0f, p), 42.0f, kWarmLight, k, l.side);
        }
    }
    if (f.carGlare > 0.01f) applyFrontLight(out, f.carGlare * 0.5f, kColdLight, -1.0f);
    if (s.carAlive && s.carZ < 2.0f && s.carZ > -10.0f) {
        const float p = (2.0f - s.carZ) / 12.0f;
        applyLightBand(out, lerpf(-10.0f, 190.0f, p), 36.0f, kColdLight, 0.35f * std::sin(p * kPi), -1.0f);
    }
}

// ============================================================================
//  Пыль, полароид, видоискатель, пост-обработка
// ============================================================================
void drawDust(Canvas& out, const Frame& f) {
    const SceneData& s = *f.s;
    const uint32_t col = rgb(170, 160, 140);
    for (const Dust& d : s.dust) {
        if (!d.alive) continue;
        const float a = saturate(d.life / d.maxLife * 2.0f) * (0.35f + 0.6f * f.light + 0.2f * f.engine);
        const int x = floori(d.x), y = floori(d.y);
        out.blendPixel(x, y, col, std::min(0.85f, a));
        if (d.big) out.blendPixel(x + 1, y, col, std::min(0.6f, a * 0.7f));
    }
}

void drawPolaroid(Canvas& out, const SceneData& s) {
    if (!s.photoActive) return;
    const float t = s.photoT;
    const float alpha = 1.0f - saturate((t - kPhotoShow) / kPhotoFade);
    if (alpha <= 0.0f) return;
    const float slide = easeOut3(t / 0.35f);
    const float cx = 84.0f, cy = lerpf(220.0f, 124.0f, slide);
    const float ang = -0.07f;
    const float ca = std::cos(ang), sa = std::sin(ang);
    const int cw = 70, ch = 48; // карточка: фото 64x36 + поля (снизу шире)
    const float develop = smoothstep(0.1f, 1.2f, t); // «проявление»
    for (int y = -30; y <= 30; ++y) {
        for (int x = -40; x <= 40; ++x) {
            // Обратный поворот: координаты на карточке.
            const float fx = static_cast<float>(x) * ca + static_cast<float>(y) * sa;
            const float fy = -static_cast<float>(x) * sa + static_cast<float>(y) * ca;
            const float u = fx + static_cast<float>(cw) * 0.5f, v = fy + static_cast<float>(ch) * 0.5f;
            const int px = roundi(cx) + x, py = roundi(cy) + y;
            if (u < 0.0f || v < 0.0f || u >= static_cast<float>(cw) || v >= static_cast<float>(ch)) {
                // Мягкая тень под карточкой.
                if (u > 1.0f && v > 1.0f && u < static_cast<float>(cw) + 2.0f &&
                    v < static_cast<float>(ch) + 2.0f)
                    out.blendPixel(px, py, rgb(0, 0, 0), 0.4f * alpha);
                continue;
            }
            const int iu = static_cast<int>(u), iv = static_cast<int>(v);
            uint32_t c = rgb(232, 228, 214);
            if (iu >= 3 && iu < 3 + kPhotoW && iv >= 3 && iv < 3 + kPhotoH) {
                const uint32_t ph = s.photo[static_cast<size_t>((iv - 3) * kPhotoW + (iu - 3))];
                c = lerpColor(rgb(214, 214, 206), ph, develop);
            } else if (iv >= ch - 1 || iu == 0 || iu == cw - 1 || iv == 0) {
                c = rgb(196, 192, 180);
            } else if (s.photoHit && t > 0.5f && iv >= 40 && iv <= 46 && iv != 45) {
                // Подпись маркером на нижнем поле: «!!!» с лёгким наклоном.
                const int row = iv - 40;
                for (int k = 0; k < 3; ++k) {
                    const int sx = 29 + k * 6 + (row < 3 ? 1 : 0);
                    if (iu == sx || iu == sx + 1) c = rgb(186, 32, 30);
                }
            }
            out.blendPixel(px, py, c, alpha);
        }
    }
}

// Уголки видоискателя вокруг многоугольника окна.
void drawBrackets(Canvas& out, const Vec2* poly, int n, float inset, float len, uint32_t c) {
    const Vec2 ctr = centroid(poly, n);
    for (int i = 0; i < n; ++i) {
        const Vec2 p = poly[i];
        const Vec2 a = poly[(i + n - 1) % n], b = poly[(i + 1) % n];
        float dx = ctr.x - p.x, dy = ctr.y - p.y;
        const float dl = std::sqrt(dx * dx + dy * dy);
        dx /= dl;
        dy /= dl;
        const Vec2 q{p.x + dx * inset, p.y + dy * inset};
        for (const Vec2& o : {a, b}) {
            float ex = o.x - p.x, ey = o.y - p.y;
            const float el = std::sqrt(ex * ex + ey * ey);
            ex /= el;
            ey /= el;
            out.drawLine(roundi(q.x), roundi(q.y), roundi(q.x + ex * len), roundi(q.y + ey * len), c);
        }
    }
}

void drawChargeArc(Canvas& out, int cx, int cy, int rad, float charge, uint32_t on, uint32_t off) {
    for (int i = 0; i < 24; ++i) {
        const float a = (static_cast<float>(i) + 0.5f) / 24.0f;
        const float ang = a * kTau - kPi * 0.5f;
        out.plot(cx + roundi(std::cos(ang) * static_cast<float>(rad)),
                 cy + roundi(std::sin(ang) * static_cast<float>(rad)), a <= charge ? on : off);
    }
}

void drawViewfinder(Canvas& out, const RealWorldView& v) {
    const bool ready = v.cameraCharge >= 0.999f;
    const uint32_t col = !ready ? rgb(96, 96, 116) : (v.superFlash ? rgb(255, 214, 110) : rgb(226, 232, 255));
    const bool rec = std::fmod(v.time, 1.0f) < 0.6f;
    const Vec2* poly = nullptr;
    switch (v.aim) {
    case Entry::Left: poly = kPolyLeft; break;
    case Entry::Right: poly = kPolyRight; break;
    case Entry::Sunroof: poly = kPolySunroof; break;
    default: break;
    }
    Vec2 ctr{160.0f, 60.0f};
    if (poly) {
        const bool tall = v.aim != Entry::Sunroof;
        drawBrackets(out, poly, 4, 4.0f, tall ? 8.0f : 6.0f, col);
        if (v.superFlash && ready)
            drawBrackets(out, poly, 4, 1.0f, tall ? 6.0f : 5.0f, scaleColor(col, 0.6f));
        ctr = centroid(poly, 4);
        if (v.aim == Entry::Sunroof) ctr.y = 12.0f;
    } else {
        // Прямо: маленькая рамка в центре лобового стекла.
        const int x0 = 148, y0 = 52, x1 = 172, y1 = 68;
        for (int k = 0; k < 2; ++k) {
            const int yy = k == 0 ? y0 : y1;
            const int dy = k == 0 ? 1 : -1;
            out.hline(x0, x0 + 4, yy, col);
            out.hline(x1 - 4, x1, yy, col);
            out.vline(x0, yy, yy + dy * 3, col);
            out.vline(x1, yy, yy + dy * 3, col);
        }
    }
    const int cx = roundi(ctr.x), cy = roundi(ctr.y);
    if (!ready) {
        drawChargeArc(out, cx, cy, 5, v.cameraCharge, rgb(170, 176, 200), rgb(46, 46, 58));
    } else {
        out.plot(cx, cy, col);
        out.plot(cx - 2, cy, scaleColor(col, 0.6f));
        out.plot(cx + 2, cy, scaleColor(col, 0.6f));
        out.plot(cx, cy - 2, scaleColor(col, 0.6f));
        out.plot(cx, cy + 2, scaleColor(col, 0.6f));
    }
    // Мигающая точка записи.
    if (rec && ready) {
        int rx = cx - 14, ry = cy - 14;
        if (v.aim == Entry::Sunroof) {
            rx = 132;
            ry = 3;
        } else if (v.aim == Entry::None) {
            rx = 151;
            ry = 55;
        } else if (v.aim == Entry::Left) {
            rx = 8;
            ry = 38;
        } else {
            rx = kW - 10;
            ry = 38;
        }
        out.fillRect(rx, ry, 2, 2, rgb(235, 40, 40));
    }
}

void postProcess(Canvas& out, const SceneData& s, float time, float strength) {
    if (s.vignette.size() != static_cast<size_t>(kW * kH)) return;
    // Номер «кадра плёнки» (24 к/с); время ограничено, чтобы приведение было определено.
    const uint32_t fr = static_cast<uint32_t>(std::fmod(std::max(0.0f, time), 100000.0f) * 24.0f);
    const uint32_t h = mix32(fr * 2654435761u + 17u);
    const int gx = static_cast<int>(h & 127u), gy = static_cast<int>((h >> 8) & 127u);
    const int str = static_cast<int>(256.0f * clampf(strength, 0.0f, 1.5f));
    uint32_t* px = out.data();
    for (int y = 0; y < kH; ++y) {
        uint32_t* row = px + static_cast<size_t>(y) * kW;
        const uint16_t* vr = s.vignette.data() + static_cast<size_t>(y) * kW;
        const int8_t* gr = s.grain.data() + static_cast<size_t>((y + gy) & (kGrainSize - 1)) * kGrainSize;
        // Внутренний цикл без вызовов функций (в отладочной сборке это важно).
        // Красный и синий обрабатываются вместе в одном 32-битном слове: между
        // каналами есть свободный бит-«охранник» для переноса/заёма.
        for (int x = 0; x < kW; ++x) {
            int fi = 256 - (((256 - static_cast<int>(vr[x])) * str) >> 8);
            fi = fi < 0 ? 0 : fi;
            const uint32_t f = static_cast<uint32_t>(fi);
            const int n = gr[(x + gx) & (kGrainSize - 1)];
            const uint32_t c = row[x];
            uint32_t rb = (((c & 0xFF00FFu) * f) >> 8) & 0xFF00FFu;
            int g = static_cast<int>((((c >> 8) & 0xFFu) * f) >> 8) + n;
            g = g < 0 ? 0 : (g > 255 ? 255 : g);
            if (n >= 0) {
                rb += static_cast<uint32_t>(n) * 0x00010001u;
                const uint32_t over = rb & 0x01000100u; // канал перевалил за 255
                rb = (rb | (over - (over >> 8))) & 0x00FF00FFu;
            } else {
                const uint32_t t = (rb | 0x01000100u) - static_cast<uint32_t>(-n) * 0x00010001u;
                const uint32_t ok = t & 0x01000100u; // охранник цел — канал не ушёл ниже нуля
                rb = t & (ok - (ok >> 8)) & 0x00FF00FFu;
            }
            row[x] = rb | (static_cast<uint32_t>(g) << 8);
        }
    }
}

// Пыль с потолка в точке (x, y).
void spawnDust(SceneData& s, float x, float y, int count, float spread) {
    for (int i = 0; i < count; ++i) {
        for (Dust& d : s.dust) {
            if (d.alive) continue;
            d.alive = true;
            d.x = x + s.rng.signedUnit() * spread;
            d.y = std::max(1.0f, y + s.rng.range(0.0f, 3.0f));
            d.vx = s.rng.signedUnit() * 7.0f;
            d.vy = s.rng.range(2.0f, 14.0f);
            d.maxLife = s.rng.range(1.6f, 3.4f);
            d.life = d.maxLife;
            d.big = s.rng.chance(0.2f);
            break;
        }
    }
}

} // namespace

struct RealWorldScene::State : SceneData {};

// ============================================================================
//  RealWorldScene
// ============================================================================
RealWorldScene::RealWorldScene() : st_(std::make_unique<State>()) {
    State& s = *st_;
    // Виньетка: таблица множителей (0..256), затемнение к краям кадра.
    s.vignette.resize(static_cast<size_t>(kW * kH));
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const float nx = (static_cast<float>(x) + 0.5f - 160.0f) / 160.0f;
            const float ny = (static_cast<float>(y) + 0.5f - 90.0f) / 90.0f;
            const float d = std::sqrt(nx * nx * 0.8f + ny * ny * 0.7f);
            const float v = smoothstep(0.55f, 1.25f, d);
            s.vignette[static_cast<size_t>(y * kW + x)] =
                static_cast<uint16_t>(roundi(256.0f * (1.0f - 0.72f * v)));
        }
    }
    // Зерно: треугольное распределение -5..5.
    for (int i = 0; i < kGrainSize * kGrainSize; ++i) {
        const float a = hashXYf(i, 1, 4242u), b = hashXYf(i, 2, 4242u);
        s.grain[static_cast<size_t>(i)] = static_cast<int8_t>(roundi((a + b - 1.0f) * 5.0f));
    }
    bakeLayer(s.dashLayer, [](Canvas& c) { drawDashStatic(c); });
    bakeLayer(s.seatLayer, [](Canvas& c) { drawSeatsStatic(c); });
    bakeLayer(s.seatLayerLit, [](Canvas& c) {
        drawSeatsStatic(c);
        // Свет экрана консоли на коленях и спинках кресел (только по нарисованному).
        for (int y = 80; y < kH; ++y) {
            for (int x = 90; x < 231; ++x) {
                const uint32_t p = c.get(x, y);
                if ((p >> 24) != 0u) continue;
                const float dx = static_cast<float>(x - 160), dy = static_cast<float>(y - 150);
                const float fall = std::max(0.0f, 1.0f - (dx * dx + dy * dy) / (70.0f * 70.0f));
                c.setRaw(x, y, addColor(p, scaleColor(rgb(90, 200, 150), 0.10f * fall * fall)));
            }
        }
    });
    s.stars.assign(static_cast<size_t>(kW * kH), 0u);
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            uint8_t v = 0;
            if (hashXY(x, y, 101u) % 997u < 4u) v |= kStarFront;
            if (hashXY(x, y, 55u) % 409u < 3u) v |= kStarSide;
            if (hashXY(x, y, 201u) % 157u < 3u) v |= kStarUp;
            s.stars[static_cast<size_t>(y * kW + x)] = v;
        }
    }
    {
        Canvas back(kBackW, kBackH);
        back.clear(rgb(12, 11, 17));
        back.setOffset(kBackMargin, kBackMargin);
        drawCabinStatic(back);
        s.backPix.assign(back.data(), back.data() + static_cast<size_t>(kBackW * kBackH));
    }
    // Фон скримера: тёмно-красное зарево из центра.
    s.jumpBg.resize(static_cast<size_t>(kW * kH));
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const float dx = static_cast<float>(x) + 0.5f - 160.0f, dy = static_cast<float>(y) + 0.5f - 96.0f;
            const float f = std::max(0.0f, 1.0f - (dx * dx + dy * dy) / (190.0f * 190.0f));
            const float w = f * f * 0.75f;
            s.jumpBg[static_cast<size_t>(y * kW + x)] =
                rgb(5 + roundi(110.0f * w), 1 + roundi(6.0f * w), 2 + roundi(8.0f * w));
        }
    }
    reset(1u);
}

RealWorldScene::~RealWorldScene() = default;

void RealWorldScene::reset(uint32_t seed) {
    State& s = *st_;
    s.seed = seed;
    s.rng.reseed(seed ^ 0x5CE7E5u);
    s.time = 0.0f;
    s.speed = 1.0f;
    s.dist = 0.0f;
    s.curve = 0.0f;
    for (Lamp& l : s.lamps) l = Lamp{};
    s.lampTimer = 1.2f;
    s.lampPassed = false;
    for (Zone& z : s.forest) z = Zone{};
    s.forestNext = 0;
    s.carAlive = false;
    s.carZ = 0.0f;
    s.carTimer = s.rng.range(12.0f, 22.0f);
    s.light = 0.0f;
    s.shX = s.shY = s.shVX = s.shVY = 0.0f;
    s.bumpTimer = 2.0f;
    s.bump2Timer = -1.0f;
    s.bump2Amount = 0.0f;
    s.offX = s.offY = 0;
    s.monX = 0.0f;
    s.monZ = 0.5f;
    s.monOnRoof = false;
    s.dentX = 160.0f;
    s.dentY = 0.0f;
    s.dentAmt = 0.0f;
    s.dentKick = 0.0f;
    s.crawlDX = 1.0f;
    s.crawlDY = 0.0f;
    s.poseEntry = Entry::None;
    s.poseAppear = 0.0f;
    s.wasOverSunroof = false;
    for (Dust& d : s.dust) d = Dust{};
    for (Scratch& sc : s.scratches) sc = Scratch{};
    s.scratchNext = 0;
    buildCracks(s, seed);
    s.pendA = 0.0f;
    s.pendV = 0.0f;
    s.photo.fill(0u);
    s.photoActive = false;
    s.photoT = 0.0f;
    s.photoHit = false;
}

void RealWorldScene::update(float dt, float carSpeed, const Monster& monster) {
    State& s = *st_;
    if (dt <= 0.0f) return;
    carSpeed = saturate(carSpeed);
    s.time += dt;
    s.speed = carSpeed;
    const float metres = dt * carSpeed * kMetersPerSec;
    s.dist += metres;
    // Плавные повороты дороги.
    const float targetCurve = (valueNoise(s.dist * 0.004f, s.seed) - 0.5f) * 0.012f;
    s.curve = damp(s.curve, targetCurve, 1.5f, dt);

    // ---- Фонари ----
    const float adv = dt * carSpeed;
    for (Lamp& l : s.lamps) {
        if (!l.alive) continue;
        const float prev = l.d;
        l.d -= adv;
        if (prev > 0.0f && l.d <= 0.0f) s.lampPassed = true;
        if (l.d < -kLampTail) l.alive = false;
    }
    s.lampTimer -= adv;
    if (s.lampTimer <= 0.0f) {
        for (Lamp& l : s.lamps) {
            if (l.alive) continue;
            l.alive = true;
            l.d = kLampLead;
            l.side = s.rng.chance(0.7f) ? 1.0f : -1.0f;
            break;
        }
        if (s.rng.chance(0.14f)) {
            // Длинный тёмный участок: лес подступает к дороге, фонарей нет.
            const float gap = s.rng.range(10.0f, 16.0f);
            s.lampTimer = gap;
            Zone& z = s.forest[static_cast<size_t>(s.forestNext)];
            z.start = s.dist + kLampLead * kMetersPerSec + 12.0f;
            z.end = s.dist + (kLampLead + gap - 1.5f) * kMetersPerSec;
            s.forestNext = (s.forestNext + 1) % static_cast<int>(s.forest.size());
        } else {
            s.lampTimer = s.rng.range(2.5f, 7.0f);
        }
    }

    // ---- Встречная машина ----
    if (s.carAlive) {
        s.carZ -= dt * (carSpeed * kMetersPerSec + kCarClosing);
        if (s.carZ < -12.0f) s.carAlive = false;
    } else {
        s.carTimer -= adv;
        if (s.carTimer <= 0.0f) {
            s.carAlive = true;
            s.carZ = 140.0f;
            s.carTimer = s.rng.range(16.0f, 40.0f);
        }
    }

    // ---- Освещённость снаружи ----
    float light = 0.0f;
    for (const Lamp& l : s.lamps) {
        if (l.alive) light = std::max(light, lampGlowAt(l.d));
    }
    if (s.carAlive && s.carZ < 30.0f && s.carZ > -5.0f)
        light = std::max(light, 0.35f * smoothstep(30.0f, 3.0f, s.carZ));
    s.light = light;

    // ---- Тряска: кочки (передняя, затем задняя ось) ----
    s.bumpTimer -= adv;
    if (s.bumpTimer <= 0.0f) {
        const float a = s.rng.range(0.15f, 0.42f);
        addShake(a);
        s.bump2Timer = 0.11f;
        s.bump2Amount = a * 0.7f;
        s.bumpTimer = s.rng.range(1.2f, 4.5f);
    }
    if (s.bump2Timer > 0.0f) {
        s.bump2Timer -= dt;
        if (s.bump2Timer <= 0.0f) addShake(s.bump2Amount);
    }
    // Затухающая пружина (полунеявный Эйлер). Шаг не длиннее 1/60 с —
    // иначе жёсткая пружина «взрывается» при подвисании кадра.
    const int springSteps = clampi(static_cast<int>(std::ceil(dt * 60.0f - 0.001f)), 1, 8);
    const float sdt = std::min(dt, 8.0f / 60.0f) / static_cast<float>(springSteps);
    for (int i = 0; i < springSteps; ++i) {
        s.shVX += (-kShakeK * s.shX - kShakeDamp * s.shVX) * sdt;
        s.shVY += (-kShakeK * s.shY - kShakeDamp * s.shVY) * sdt;
        s.shX += s.shVX * sdt;
        s.shY += s.shVY * sdt;
    }
    // Микровибрация двигателя (сильнее на скорости).
    const float vib = (valueNoise(s.time * 11.0f, 77u) - 0.5f) * (0.45f + 0.7f * carSpeed);
    const float vibX = (valueNoise(s.time * 7.0f, 78u) - 0.5f) * 0.5f * carSpeed;
    s.offX = clampi(roundi(s.shX + vibX), -6, 6);
    s.offY = clampi(roundi(s.shY + vib), -6, 6);

    // ---- Монстр: вмятина, направление движения, поза ----
    const MonsterState ms = monster.state();
    const bool onRoof = monster.onRoof() && ms != MonsterState::Entered;
    const float mx = monster.roofX(), mz = monster.roofZ();
    const float tx = 160.0f + mx * 140.0f, ty = lerpf(-6.0f, 30.0f, mz);
    if (onRoof && !s.monOnRoof) {
        s.dentX = tx;
        s.dentY = ty;
    }
    const float ddx = (mx - s.monX) * 140.0f, ddy = (mz - s.monZ) * 36.0f;
    if (onRoof && (std::fabs(ddx) + std::fabs(ddy)) > 0.001f) {
        s.crawlDX = damp(s.crawlDX, ddx, 6.0f, dt);
        s.crawlDY = damp(s.crawlDY, ddy, 6.0f, dt);
    }
    s.monX = mx;
    s.monZ = mz;
    s.monOnRoof = onRoof;
    s.dentX = damp(s.dentX, tx, 10.0f, dt);
    s.dentY = damp(s.dentY, ty, 10.0f, dt);
    s.dentAmt = approach(s.dentAmt, onRoof ? 1.0f : 0.0f, dt * 2.5f);
    s.dentKick = approach(s.dentKick, 0.0f, dt * 3.0f);
    switch (ms) {
    case MonsterState::Peeking:
    case MonsterState::BreakingIn:
    case MonsterState::Entered:
        s.poseEntry = monster.target();
        s.poseAppear = monster.appear();
        s.wasOverSunroof = false;
        break;
    case MonsterState::Crawling:
        s.poseEntry = Entry::None;
        s.wasOverSunroof = std::fabs(mx) < 0.3f && mz > 0.35f && mz < 0.75f;
        break;
    case MonsterState::Dormant:
    case MonsterState::Landing:
        s.poseEntry = Entry::None;
        s.wasOverSunroof = false;
        break;
    default:
        break; // Repelled/Retreating: помним, где он был
    }

    // ---- Пыль ----
    for (Dust& d : s.dust) {
        if (!d.alive) continue;
        d.vy += 16.0f * dt;
        d.vx *= 1.0f - 1.5f * dt;
        d.vx += std::sin(s.time * 3.0f + d.y * 0.2f) * 6.0f * dt;
        d.x += d.vx * dt;
        d.y += d.vy * dt;
        d.life -= dt;
        if (d.life <= 0.0f || d.y > static_cast<float>(kH + 2)) d.alive = false;
    }
    for (Scratch& sc : s.scratches) {
        if (!sc.alive) continue;
        sc.age += dt;
        if (sc.age > 25.0f) sc.alive = false;
    }

    // ---- «Ёлочка»: маятник, раскачиваемый поворотами и тряской ----
    const float lateral = s.curve * kMetersPerSec * kMetersPerSec * carSpeed * carSpeed;
    for (int i = 0; i < springSteps; ++i) {
        const float acc = -120.0f * std::sin(s.pendA) - 2.2f * s.pendV - lateral * 12.0f * std::cos(s.pendA) -
                          s.shVX * 0.8f;
        s.pendV += acc * sdt;
        s.pendA = clampf(s.pendA + s.pendV * sdt, -1.2f, 1.2f);
    }

    // ---- Полароид ----
    if (s.photoActive) {
        s.photoT += dt;
        if (s.photoT > kPhotoShow + kPhotoFade) s.photoActive = false;
    }
}

void RealWorldScene::onMonsterEvent(const MonsterEventInfo& e) {
    State& s = *st_;
    const float x = 160.0f + s.monX * 140.0f;
    const float y = lerpf(-6.0f, 30.0f, s.monZ);
    const float k = saturate(e.intensity);
    switch (e.type) {
    case MonsterEvent::Landed:
        addShake(1.0f * k);
        s.dentKick = 1.0f;
        s.dentX = x;
        s.dentY = y;
        spawnDust(s, x, y, 26, 14.0f);
        break;
    case MonsterEvent::Step:
        addShake(0.15f * k);
        s.dentKick = std::max(s.dentKick, 0.5f);
        spawnDust(s, x, y, 4, 8.0f);
        break;
    case MonsterEvent::Scrape:
        spawnDust(s, x, y, 6, 12.0f);
        if (std::fabs(s.monX) < 0.3f && s.monZ > 0.3f && s.monZ < 0.8f) {
            // Коготь царапает стекло люка — след остаётся надолго.
            Scratch& sc = s.scratches[static_cast<size_t>(s.scratchNext)];
            s.scratchNext = (s.scratchNext + 1) % kMaxScratches;
            sc.alive = true;
            sc.age = 0.0f;
            sc.x0 = x + s.rng.signedUnit() * 10.0f;
            sc.y0 = clampf(y + s.rng.signedUnit() * 6.0f, 2.0f, 20.0f);
            sc.x1 = sc.x0 + s.rng.range(6.0f, 14.0f) * (s.rng.chance(0.5f) ? 1.0f : -1.0f);
            sc.y1 = clampf(sc.y0 + s.rng.signedUnit() * 4.0f, 1.0f, 21.0f);
        }
        break;
    case MonsterEvent::Knock:
        addShake(0.08f * k);
        break;
    case MonsterEvent::Bang:
        addShake(0.5f * k);
        if (e.entry == Entry::Left || e.entry == Entry::Right) {
            spawnDust(s, e.entry == Entry::Left ? 26.0f : 294.0f, 24.0f, 3, 10.0f);
        } else {
            spawnDust(s, x, y, 3, 10.0f);
        }
        break;
    case MonsterEvent::LeftRoof:
        addShake(0.35f * k);
        spawnDust(s, x, y, 8, 12.0f);
        break;
    case MonsterEvent::BrokeIn:
        addShake(1.0f);
        break;
    default:
        break;
    }
}

void RealWorldScene::addShake(float amount) {
    State& s = *st_;
    amount = clampf(amount, 0.0f, 1.5f);
    s.shVY += amount * kShakeImpulse;
    s.shVX += amount * kShakeImpulse * 0.35f * s.rng.signedUnit();
    s.pendV += amount * 4.0f * s.rng.signedUnit();
}

void RealWorldScene::capturePhoto(const Canvas& frame, bool hit) {
    State& s = *st_;
    const int fw = frame.width(), fh = frame.height();
    if (fw <= 0 || fh <= 0) return;
    for (int y = 0; y < kPhotoH; ++y) {
        const int sy0 = y * fh / kPhotoH, sy1 = std::max(sy0 + 1, (y + 1) * fh / kPhotoH);
        for (int x = 0; x < kPhotoW; ++x) {
            const int sx0 = x * fw / kPhotoW, sx1 = std::max(sx0 + 1, (x + 1) * fw / kPhotoW);
            int r = 0, g = 0, b = 0, n = 0;
            for (int yy = sy0; yy < sy1; ++yy) {
                for (int xx = sx0; xx < sx1; ++xx) {
                    const uint32_t c = frame.get(xx, yy);
                    r += colR(c);
                    g += colG(c);
                    b += colB(c);
                    ++n;
                }
            }
            n = std::max(n, 1);
            // «Вспышка»: тёмное вытягивается, лёгкий тёплый сдвиг и выцветшие чёрные.
            auto tone = [](int v, float warm) {
                float f = std::pow(static_cast<float>(v) / 255.0f, 0.62f);
                f = (f - 0.45f) * 1.25f + 0.45f;
                return clampi(roundi(14.0f + 235.0f * saturate(f) * warm), 0, 255);
            };
            s.photo[static_cast<size_t>(y * kPhotoW + x)] =
                rgb(tone(r / n, 1.04f), tone(g / n, 1.0f), tone(b / n, 0.92f));
        }
    }
    s.photoActive = true;
    s.photoT = 0.0f;
    s.photoHit = hit;
}

void RealWorldScene::render(Canvas& out, const RealWorldView& view, const Monster& monster,
                            const Parents& parents, const Canvas& consoleScreen) const {
    const State& s = *st_;
    // Сцена пишет в буфер напрямую и рассчитана ровно на 320x180.
    if (out.width() != kW || out.height() != kH) out.resize(kW, kH);
    out.resetClip();
    out.resetStencilModes();
    out.setOffset(0, 0);
    out.clearStencil(0);

    // ---- Параметры кадра ----
    Frame f;
    f.s = &s;
    f.stars = s.stars.data();
    f.v = &view;
    f.ox = s.offX;
    f.oy = s.offY;
    f.time = view.time;
    f.engine = saturate(view.engineAlive);
    f.beams = f.engine;
    f.amb = 0.78f + 0.22f * f.engine;
    f.light = s.light;
    f.forestHere = forestAmount(s, s.dist + 10.0f);
    for (const Lamp& l : s.lamps) {
        if (!l.alive) continue;
        const float g = lampGlowAt(l.d);
        if (l.d > 0.0f) f.frontLight = std::max(f.frontLight, g);
        f.sideLightL = std::max(f.sideLightL, g * (l.side < 0.0f ? 1.0f : 0.45f));
        f.sideLightR = std::max(f.sideLightR, g * (l.side > 0.0f ? 1.0f : 0.45f));
    }
    if (s.carAlive) {
        f.carGlare = smoothstep(60.0f, 6.0f, s.carZ) * (s.carZ > 1.0f ? 1.0f : 0.0f);
        f.carSideL = s.carZ < 6.0f && s.carZ > -10.0f ? std::exp(-(s.carZ * s.carZ) / 20.0f) * 0.7f : 0.0f;
    }
    if (view.arrive > 0.0f) {
        f.houseZ = 110.0f - 103.5f * smoothstep(0.0f, 1.0f, view.arrive);
        f.light = std::max(f.light, 0.6f * view.arrive);
    }
    f.monLight = clampf(0.30f + 0.75f * f.light, 0.0f, 1.0f);
    f.monLightCol = lerpColor(kColdLight, kWarmLight, saturate(f.light * 1.4f));
    out.setOffset(f.ox, f.oy);

    // ---- 1) Фон салона ----
    blitBack(out, s, f.amb);
    drawDent(out, f, monster);
    // ---- 2) Виды наружу ----
    drawWindshield(out, f);
    drawSideWindows(out, f);
    drawSunroof(out, f, monster);
    // ---- 3) Монстр и стекло ----
    drawMonster(out, f, monster);
    drawGlass(out, f, monster);
    // ---- 4) Передний план салона ----
    out.setStencilWrite(kStInterior);
    drawTrims(out, f);
    blitLayer(out, s.dashLayer, f.amb, 96);
    drawDashLights(out, f);
    drawMirror(out, f, monster);
    drawParentsAndSeats(out, f, parents);
    out.setStencilWrite(0);
    // ---- 5) Свет ----
    applyLighting(out, f);
    // ---- 6) Пыль, консоль, пост-обработка ----
    drawDust(out, f);
    if (view.showConsole) {
        const float brightness = clampf(0.3f + 0.45f * f.light + 0.15f * f.engine, 0.0f, 1.0f);
        ConsoleMiniGame::drawHandheld(out, consoleScreen, 160, 150, 0.5f, view.consoleDpad, brightness,
                                      0.55f);
    }
    out.setOffset(0, 0);
    const float threatK = saturate(view.threat / 100.0f);
    float vig = 0.85f + 0.25f * threatK;
    if (threatK > 0.7f) vig += 0.12f * std::pow(std::max(0.0f, std::sin(view.time * 7.5f)), 6.0f);
    postProcess(out, s, view.time, vig);

    // ---- 7) Видоискатель, полароид, HUD ----
    if (view.showHud && view.hasCamera && view.showAim) {
        out.setOffset(f.ox, f.oy);
        drawViewfinder(out, view);
        out.setOffset(0, 0);
    }
    drawPolaroid(out, s);
    if (view.showHud) {
        hud::drawFuelGauge(out, 4, 4, view.fuel / 100.0f, view.lowFuelWarning, view.time);
        hud::drawThreatBar(out, 4, 20, threatK, view.time);
        if (view.endless) hud::drawEndlessBar(out, 246, 4, 70, view.kmDriven, view.time);
        else hud::drawTripBar(out, 246, 4, 70, view.tripProgress, view.kmLeft);
        hud::drawCameraStatus(out, 272, 148, view.hasCamera, view.cameraCharge, view.superFlash, view.time);
        if (view.lockActive) hud::drawLockStatus(out, 258, 134, view.lockLeft);
        int hy = 170;
        hud::drawKeyHint(out, 4, hy, "TAB", T8("КОНСОЛЬ"), 0.85f);
        if (view.hasCamera) {
            hy -= 11;
            hud::drawKeyHint(out, 4, hy, "F", T8("ФОТО"), 0.85f);
            hy -= 11;
            hud::drawKeyHint(out, 4, hy, "A/W/D", T8("ЦЕЛЬ"), 0.85f);
        }
    }
    out.resetStencilModes();
    out.resetClip();
    out.setOffset(0, 0);
}

void RealWorldScene::renderJumpscare(Canvas& out, float t) const {
    const State& s = *st_;
    if (out.width() != kW || out.height() != kH) out.resize(kW, kH);
    out.resetClip();
    out.resetStencilModes();
    out.setOffset(0, 0);
    t = std::max(0.0f, t);
    const int frame = static_cast<int>(std::min(t, 60.0f) * 60.0f);
    const bool fullSize = s.jumpBg.size() == static_cast<size_t>(kW * kH);
    // Не больше двух ярких кадров (светочувствительность).
    const bool bright = frame < 2;
    if (bright) out.clear(frame == 0 ? rgb(255, 232, 222) : rgb(176, 18, 18));
    else if (fullSize) std::copy(s.jumpBg.begin(), s.jumpBg.end(), out.data());
    else out.clear(rgb(5, 1, 2));

    // Лицо несётся на камеру, кадр трясётся.
    const float zoom = t < 0.25f ? lerpf(0.6f, 1.2f, easeOut3(t / 0.25f)) : 1.2f + (t - 0.25f) * 0.06f;
    const float amp = 7.0f * (1.0f - smoothstep(0.0f, 1.1f, t)) + 1.0f;
    const int sx = roundi((hashXYf(frame, 1, 77u) - 0.5f) * 2.0f * amp);
    const int sy = roundi((hashXYf(frame, 2, 77u) - 0.5f) * 2.0f * amp);
    out.setOffset(sx, sy);
    // Плечи и шея — тёмная масса под головой.
    const float zk = (zoom - 0.6f) / 0.6f;
    const float headY = lerpf(86.0f, 58.0f, saturate(zk));
    const int shoulderY = roundi(headY + 120.0f * zoom);
    out.fillEllipse(160, shoulderY, roundi(170.0f * zoom), roundi(50.0f * zoom), rgb(8, 7, 8));
    out.fillRect(160 - roundi(26.0f * zoom), roundi(headY + 60.0f * zoom), roundi(52.0f * zoom),
                 roundi(60.0f * zoom), rgb(14, 14, 14));
    Face fc;
    fc.cx = 160.0f;
    fc.cy = headY;
    fc.r = 92.0f * zoom;
    fc.angle = std::sin(t * 9.0f) * 0.03f;
    fc.mouth = smoothstep(0.0f, 0.22f, t);
    fc.light = bright ? 1.0f : 0.8f;
    fc.lightCol = bright ? rgb(255, 255, 255) : rgb(255, 196, 180);
    fc.lightDirX = 0.0f;
    fc.lightDirY = 0.7f;
    fc.pupil = 1.8f;
    fc.detail = true;
    drawFace(out, fc);
    // Тянущиеся к камере руки с когтями (после первых кадров).
    if (t > 0.08f) {
        const float reach = easeOut3((t - 0.08f) / 0.3f);
        LimbStyle st;
        st.skin = rgb(170, 150, 136);
        st.dark = rgb(14, 8, 8);
        st.claw = rgb(214, 200, 170);
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            const float fs = static_cast<float>(sgn);
            const float hx = 160.0f + fs * lerpf(230.0f, 150.0f, reach);
            const float hy = lerpf(230.0f, 160.0f, reach);
            for (int k = 0; k < 4; ++k) {
                const float fk = static_cast<float>(k) - 1.5f;
                const float a = -kPi * 0.5f - fs * (0.55f - fk * 0.16f);
                drawFinger(out, hx + fk * 9.0f * fs, hy + std::fabs(fk) * 4.0f, a,
                           70.0f + (k == 1 || k == 2 ? 14.0f : 0.0f), fs * 0.18f, 7.0f, st);
            }
        }
    }
    // Мгновение рывка: тёмные «линии скорости» к краям.
    if (t < 0.3f && !bright) {
        const float k = 1.0f - t / 0.3f;
        for (int i = 0; i < 28; ++i) {
            const float a = hashIf(i, 91u) * kTau;
            const float r0 = 120.0f + hashIf(i, 92u) * 60.0f;
            const float r1 = r0 + 80.0f;
            out.blendLine(160 + roundi(std::cos(a) * r0), 90 + roundi(std::sin(a) * r0 * 0.6f),
                          160 + roundi(std::cos(a) * r1), 90 + roundi(std::sin(a) * r1 * 0.6f), rgb(0, 0, 0),
                          0.6f * k);
        }
    }
    out.setOffset(0, 0);
    if (!fullSize) return;

    // Помехи (затухают), красная виньетка, в конце — уход в черноту.
    const int noise = bright ? 0 : roundi(64.0f * (1.0f - smoothstep(0.05f, 0.9f, t))) + 12;
    const int fade = roundi(256.0f * (1.0f - smoothstep(1.3f, 1.7f, t)));
    const uint32_t h = mix32(static_cast<uint32_t>(frame) * 2654435761u + 99u);
    const int gx = static_cast<int>(h & 127u), gy = static_cast<int>((h >> 8) & 127u);
    uint32_t* px = out.data();
    for (int y = 0; y < kH; ++y) {
        uint32_t* row = px + static_cast<size_t>(y) * kW;
        const uint16_t* vr = s.vignette.data() + static_cast<size_t>(y) * kW;
        const int8_t* gr = s.grain.data() + static_cast<size_t>((y + gy) & (kGrainSize - 1)) * kGrainSize;
        for (int x = 0; x < kW; ++x) {
            const int n = (gr[(x + gx) & (kGrainSize - 1)] * noise) >> 4;
            const uint32_t c = row[x];
            int r = colR(c) + n, g = colG(c) + n, b = colB(c) + n;
            if (!bright) {
                // v: 0 в центре .. 256 в углах.
                const int v = std::min(256, ((256 - static_cast<int>(vr[x])) * 355) >> 8);
                r = ((r * (256 - ((v * 90) >> 8))) >> 8) + ((40 * v) >> 8);
                g = (g * (256 - ((v * 180) >> 8))) >> 8;
                b = (b * (256 - ((v * 180) >> 8))) >> 8;
            }
            row[x] = rgb(clampi((r * fade) >> 8, 0, 255), clampi((g * fade) >> 8, 0, 255),
                         clampi((b * fade) >> 8, 0, 255));
        }
    }
}

Entry RealWorldScene::aimFromPoint(int x, int y) const {
    const float px = static_cast<float>(x) + 0.5f, py = static_cast<float>(y) + 0.5f;
    const struct {
        Entry e;
        const Vec2* poly;
    } cands[3] = {{Entry::Left, kPolyLeft}, {Entry::Sunroof, kPolySunroof}, {Entry::Right, kPolyRight}};
    Entry best = Entry::None;
    float bestD = 6.0f;
    for (const auto& c : cands) {
        const float d = distToPoly(c.poly, 4, px, py);
        if (d <= bestD) {
            bestD = d;
            best = c.e;
        }
    }
    return best;
}

Vec2 RealWorldScene::entryCenter(Entry e) const {
    switch (e) {
    case Entry::Left: return centroid(kPolyLeft, 4);
    case Entry::Right: return centroid(kPolyRight, 4);
    case Entry::Sunroof: return Vec2{160.0f, 11.0f};
    default: return centroid(kPolyWindshield, 4);
    }
}

float RealWorldScene::lightLevel() const { return st_->light; }

bool RealWorldScene::takeStreetlightPassed() {
    const bool r = st_->lampPassed;
    st_->lampPassed = false;
    return r;
}

int RealWorldScene::shakeX() const { return st_->offX; }
int RealWorldScene::shakeY() const { return st_->offY; }
