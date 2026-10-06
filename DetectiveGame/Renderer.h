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

    // Этап 1: вид сверху (карта и игрок) вместо 3D. На этапе 2 заменим рейкастером.
    void DrawTopDown(const Map& map, const Player& player);

    // Выводит буфер в окно (с растяжением до w x h). overlay рисует GDI-текст поверх.
    void Present(HDC target, int w, int h, const std::function<void(HDC)>& overlay);

private:
    void EnsureBackBuffer(HDC target, int w, int h);
    void ReleaseBackBuffer();

    std::vector<uint32_t> pixels_;
    BITMAPINFO bmi_;
    HDC memDc_ = nullptr;
    HBITMAP memBmp_ = nullptr;
    HBITMAP oldBmp_ = nullptr;
    int bw_ = 0, bh_ = 0;
};
