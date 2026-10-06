#pragma once
// Renderer.h - программный пиксельный буфер и вывод в окно через StretchDIBits.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>
#include <functional>
#include <vector>
#include "Map.h"
#include "Player.h"

// Цвет в формате 0x00RRGGBB (формат буфера BI_RGB 32 bit)
constexpr uint32_t Rgb(int r, int g, int b) {
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

// Смешивание двух цветов: t = 0 -> a, t = 1 -> b
inline uint32_t MixColor(uint32_t a, uint32_t b, double t) {
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    return Rgb(static_cast<int>(ar + (br - ar) * t), static_cast<int>(ag + (bg - ag) * t),
               static_cast<int>(ab + (bb - ab) * t));
}

class Renderer {
public:
    static constexpr int kWidth = 480;  // внутреннее разрешение буфера
    static constexpr int kHeight = 300;

    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void Clear(uint32_t color);
    void SetPixel(int x, int y, uint32_t color);
    void FillRect(int x, int y, int w, int h, uint32_t color);
    void DrawLine(int x0, int y0, int x1, int y1, uint32_t color);

    void FillCircle(int cx, int cy, int radius, uint32_t color);
    void FillGradientV(int x, int y, int w, int h, uint32_t top, uint32_t bottom);

    // Постобработка для атмосферы: затемнение по краям и плавное смешивание с цветом
    void ApplyVignette(double strength);
    void Fade(uint32_t color, double amount);

    // Этап 2: рейкастер - стены (DDA), пол, потолок, затухание по расстоянию.
    // flicker - множитель яркости света (мерцание лампы/фонаря), около 1.0.
    void DrawWorld(const Map& map, const Player& player, double flicker);

    // Расстояние до стены для каждой колонки экрана (пригодится спрайтам на этапе 3)
    const std::vector<double>& ZBuffer() const { return zbuffer_; }

    // Маленькая карта в правом верхнем углу (включается клавишей M)
    void DrawMiniMap(const Map& map, const Player& player);

    // Вид сверху (отладочный) (карта и игрок) вместо 3D. На этапе 2 заменим рейкастером.
    // lightRadius > 0 - "фонарик": клетки далеко от игрока тонут во тьме.
    void DrawTopDown(const Map& map, const Player& player, double lightRadius);

    // Выводит буфер в окно (с растяжением до w x h). overlay рисует GDI-текст поверх.
    void Present(HDC target, int w, int h, const std::function<void(HDC)>& overlay);

private:
    void EnsureBackBuffer(HDC target, int w, int h);
    void ReleaseBackBuffer();

    std::vector<uint32_t> pixels_;
    std::vector<float> vignette_;
    std::vector<double> zbuffer_;
    BITMAPINFO bmi_;
    HDC memDc_ = nullptr;
    HBITMAP memBmp_ = nullptr;
    HBITMAP oldBmp_ = nullptr;
    int bw_ = 0, bh_ = 0;
};
