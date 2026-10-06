// Scenes.cpp
#include "Scenes.h"
#include <cmath>

namespace {

const int W = Renderer::kWidth;

// Псевдослучайное число 0..1 по целому ключу (одинаковое между кадрами)
double Hash(int n) {
    double s = std::sin(n * 12.9898) * 43758.5453;
    return s - std::floor(s);
}

double Clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

// Силуэт человека
void DrawFigure(Renderer& r, int x, int groundY, uint32_t c) {
    r.FillCircle(x, groundY - 25, 4, c);
    r.FillRect(x - 4, groundY - 21, 9, 15, c);
    r.FillRect(x - 4, groundY - 6, 3, 6, c);
    r.FillRect(x + 2, groundY - 6, 3, 6, c);
}

// Крыша-треугольник
void DrawRoof(Renderer& r, int cx, int baseY, int halfW, int height, uint32_t c) {
    for (int row = 0; row < height; ++row) {
        int half = halfW * row / height;
        r.FillRect(cx - half, baseY - height + row, half * 2 + 1, 1, c);
    }
}

} // namespace

namespace Scenes {

void DrawDayStreet(Renderer& r, double t) {
    r.FillGradientV(0, 0, W, 230, Rgb(110, 165, 225), Rgb(205, 225, 245));
    r.FillCircle(400, 60, 26, Rgb(255, 240, 180));

    for (int i = 0; i < 9; ++i) {
        int bw = 40 + static_cast<int>(Hash(i) * 30);
        int bh = 70 + static_cast<int>(Hash(i + 50) * 90);
        int bx = i * 56 - 10;
        int shade = 110 + static_cast<int>(Hash(i + 9) * 50);
        r.FillRect(bx, 230 - bh, bw, bh, Rgb(shade, shade, shade + 10));
        for (int wy = 230 - bh + 8; wy < 222; wy += 14)
            for (int wx = bx + 6; wx < bx + bw - 8; wx += 12) {
                bool lit = Hash(i * 1000 + wx * 7 + wy) > 0.5;
                r.FillRect(wx, wy, 5, 7, lit ? Rgb(240, 230, 160) : Rgb(60, 70, 90));
            }
    }
    r.FillRect(0, 230, W, 70, Rgb(55, 55, 60));
    r.FillRect(0, 230, W, 8, Rgb(140, 140, 140));
    DrawFigure(r, 60 + static_cast<int>(t * 30), 246, Rgb(20, 20, 30));
}

void DrawDusk(Renderer& r, double t) {
    r.FillGradientV(0, 0, W, 230, Rgb(35, 30, 70), Rgb(235, 125, 60));
    r.FillCircle(90, 215, 30, Rgb(255, 170, 80));
    r.FillRect(0, 230, W, 70, Rgb(16, 14, 22));

    // Силуэты домов
    for (int i = 0; i < 7; ++i) {
        int cx = 30 + i * 62;
        int bh = 38 + static_cast<int>(Hash(i + 3) * 22);
        r.FillRect(cx - 24, 230 - bh, 48, bh, Rgb(20, 18, 28));
        DrawRoof(r, cx, 230 - bh, 30, 16, Rgb(18, 16, 24));
        if (Hash(i + 20) > 0.4) r.FillRect(cx - 6, 230 - bh + 10, 8, 10, Rgb(255, 190, 90));
    }
    // Дом героя (справа): тёплый свет в окнах
    r.FillRect(380, 165, 80, 65, Rgb(26, 22, 30));
    DrawRoof(r, 420, 165, 52, 26, Rgb(20, 16, 22));
    r.FillRect(394, 182, 12, 14, Rgb(255, 190, 90));
    r.FillRect(432, 182, 12, 14, Rgb(255, 190, 90));
    r.FillRect(414, 205, 12, 25, Rgb(10, 8, 12));

    // Фонарь
    r.FillRect(200, 170, 3, 60, Rgb(10, 10, 14));
    r.FillCircle(201, 168, 5, Rgb(255, 220, 140));

    DrawFigure(r, 40 + static_cast<int>(t * 40), 248, Rgb(4, 4, 8));
}

void DrawBurningHouse(Renderer& r, double t) {
    const int ground = 232;
    r.FillGradientV(0, 0, W, ground, Rgb(8, 6, 14), Rgb(95, 32, 12));
    r.FillRect(0, ground, W, 300 - ground, Rgb(20, 12, 10));

    // Дом
    r.FillRect(170, 140, 140, ground - 140, Rgb(45, 30, 28));
    DrawRoof(r, 240, 140, 90, 48, Rgb(30, 20, 20));
    for (int i = 0; i < 3; ++i) {
        double flick = 0.6 + 0.4 * std::sin(t * 9.0 + i * 2.1);
        r.FillRect(190 + i * 45, 165, 22, 26, MixColor(Rgb(90, 30, 10), Rgb(255, 170, 40), flick));
    }
    r.FillRect(232, 195, 16, ground - 195, Rgb(14, 8, 8));

    // Пламя: высота зависит от расстояния до центра дома и "шума"
    for (int x = 130; x < 350; ++x) {
        double e = 1.0 - std::fabs(x - 240) / 115.0;
        if (e <= 0.0) continue;
        double n = 0.55 + 0.25 * std::sin(x * 0.31 + t * 8.0) + 0.2 * std::sin(x * 0.11 - t * 5.0);
        double h = (25.0 + 125.0 * e) * n;
        if (h < 2.0) continue;
        for (int y = ground; y > ground - static_cast<int>(h); --y) {
            double k = (ground - y) / h; // 0 внизу, 1 на кончике
            uint32_t c;
            if (k < 0.35)      c = MixColor(Rgb(255, 235, 120), Rgb(255, 150, 30), k / 0.35);
            else if (k < 0.75) c = MixColor(Rgb(255, 150, 30), Rgb(200, 40, 10), (k - 0.35) / 0.4);
            else               c = MixColor(Rgb(200, 40, 10), Rgb(60, 12, 8), (k - 0.75) / 0.25);
            r.SetPixel(x, y, c);
        }
    }

    // Искры
    for (int i = 0; i < 50; ++i) {
        double speed = 25.0 + Hash(i + 100) * 50.0;
        double y = ground - std::fmod(t * speed + Hash(i + 200) * 220.0, 230.0);
        double x = 240 + (Hash(i) - 0.5) * 170.0 + std::sin(t * 2.0 + i) * 10.0;
        r.FillRect(static_cast<int>(x), static_cast<int>(y), 2, 2, Rgb(255, 210, 90));
    }

    // Герой спиной к нам
    DrawFigure(r, 70, 268, Rgb(3, 3, 5));
}

void DrawCarView(Renderer& r, double t, double glow, double speed) {
    const int horizon = 100, dashY = 190;
    double phase = t * speed * 6.0;

    // Небо; при glow на горизонте появляется зарево
    r.FillGradientV(0, 0, W, horizon, Rgb(6, 8, 14), MixColor(Rgb(14, 16, 26), Rgb(120, 45, 18), glow * 0.7));
    // Силуэты далёких зданий
    for (int i = 0; i < 24; ++i) {
        int h = 10 + static_cast<int>(Hash(i) * 30);
        r.FillRect(i * 20, horizon - h, 20, h, Rgb(10, 11, 16));
    }
    // Земля по бокам и дорога
    for (int y = horizon; y < dashY; ++y) {
        double p = (y - horizon) / static_cast<double>(dashY - horizon);
        r.FillRect(0, y, W, 1, MixColor(Rgb(8, 9, 12), Rgb(16, 18, 22), p));
        int half = 6 + static_cast<int>(p * 300);
        r.FillRect(W / 2 - half, y, half * 2, 1, MixColor(Rgb(14, 15, 20), Rgb(32, 34, 42), p));
        // Центральная разметка (штрихи в перспективе)
        double z = 1.0 / (p + 0.03);
        if (static_cast<long long>(std::floor(z * 1.5 - phase)) % 2 == 0)
            r.FillRect(W / 2 - static_cast<int>(p * 3), y, 1 + static_cast<int>(p * 6), 1, Rgb(150, 150, 120));
    }
    // Столбы вдоль дороги (только при движении)
    if (speed > 0.0) {
        for (int k = 0; k < 8; ++k) {
            double zz = std::fmod(k / 8.0 + phase * 0.08, 1.0);
            double pp = zz * zz;
            int y = horizon + static_cast<int>(pp * 90);
            int off = 12 + static_cast<int>(pp * 330);
            int ph = 6 + static_cast<int>(pp * 70);
            int pw = 1 + static_cast<int>(pp * 5);
            r.FillRect(W / 2 - off, y - ph, pw, ph, Rgb(35, 38, 48));
            r.FillRect(W / 2 + off, y - ph, pw, ph, Rgb(35, 38, 48));
            r.FillCircle(W / 2 - off, y - ph, 1 + static_cast<int>(pp * 4), Rgb(190, 170, 110));
            r.FillCircle(W / 2 + off, y - ph, 1 + static_cast<int>(pp * 4), Rgb(190, 170, 110));
        }
    }
    // Дождь
    for (int i = 0; i < 70; ++i) {
        double rate = 300.0 + Hash(i + 700) * 200.0;
        double y = std::fmod(Hash(i + 300) * dashY + t * rate, static_cast<double>(dashY));
        int x = static_cast<int>(Hash(i) * W);
        r.DrawLine(x, static_cast<int>(y), x - 1, static_cast<int>(y) + 6, Rgb(80, 90, 120));
    }

    // Зеркало заднего вида: в нём отблеск пожара
    r.FillRect(200, 4, 80, 20, Rgb(10, 10, 12));
    double flick = 0.7 + 0.3 * std::sin(t * 9.0);
    r.FillRect(203, 7, 74, 14, MixColor(Rgb(8, 10, 16), Rgb(210, 75, 20), glow * flick));

    // Приборная панель и руль
    r.FillRect(0, dashY, W, 300 - dashY, Rgb(12, 12, 14));
    r.FillRect(0, dashY, W, 3, Rgb(40, 40, 46));
    r.FillCircle(210, 215, 16, Rgb(8, 8, 10));
    r.FillCircle(270, 215, 16, Rgb(8, 8, 10));
    r.FillCircle(210, 215, 12, Rgb(20, 40, 30));
    r.FillCircle(270, 215, 12, Rgb(40, 22, 18));
    r.FillCircle(240, 275, 68, Rgb(30, 30, 34));
    r.FillCircle(240, 275, 56, Rgb(12, 12, 14));
    r.FillRect(172, 270, 136, 9, Rgb(30, 30, 34));
    r.FillRect(236, 275, 9, 28, Rgb(30, 30, 34));
    // Телефон на приборке: слабое голубое свечение
    r.FillRect(350, 205, 28, 46, Rgb(22, 30, 44));
    r.FillRect(353, 208, 22, 40, Rgb(40, 70, 110));
}

} // namespace Scenes
