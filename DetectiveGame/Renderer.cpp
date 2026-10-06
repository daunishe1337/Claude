// Renderer.cpp
#include "Renderer.h"
#include <cmath>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

Renderer::Renderer() : pixels_(kWidth * kHeight, 0) {
    ZeroMemory(&bmi_, sizeof(bmi_));
    bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi_.bmiHeader.biWidth = kWidth;
    bmi_.bmiHeader.biHeight = -kHeight; // минус = строки сверху вниз
    bmi_.bmiHeader.biPlanes = 1;
    bmi_.bmiHeader.biBitCount = 32;
    bmi_.bmiHeader.biCompression = BI_RGB;
}

Renderer::~Renderer() { ReleaseBackBuffer(); }

void Renderer::Clear(uint32_t color) {
    for (uint32_t& p : pixels_) p = color;
}

void Renderer::SetPixel(int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return;
    pixels_[y * kWidth + x] = color;
}

void Renderer::FillRect(int x, int y, int w, int h, uint32_t color) {
    for (int j = y; j < y + h; ++j)
        for (int i = x; i < x + w; ++i)
            SetPixel(i, j, color);
}

// Линия Брезенхема
void Renderer::DrawLine(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        SetPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Renderer::DrawTopDown(const Map& map, const Player& player) {
    const int cell = 12, ox = 10, oy = 10;
    Clear(Rgb(16, 16, 20));

    for (int y = 0; y < map.Height(); ++y) {
        for (int x = 0; x < map.Width(); ++x) {
            uint32_t c;
            switch (map.Cell(x, y)) {
            case '1': c = Rgb(160, 80, 64);  break; // кирпич
            case '2': c = Rgb(139, 90, 43);  break; // дерево
            case '3': c = Rgb(128, 136, 144); break; // камень
            case '4': c = Rgb(79, 127, 95);  break; // обои
            case '5': c = Rgb(200, 160, 64); break; // мебель
            default:  c = Rgb(40, 40, 48);   break; // пол
            }
            FillRect(ox + x * cell, oy + y * cell, cell - 1, cell - 1, c);
        }
    }

    // Игрок: точка и линия направления
    int px = ox + static_cast<int>(player.x * cell);
    int py = oy + static_cast<int>(player.y * cell);
    FillRect(px - 2, py - 2, 5, 5, Rgb(255, 230, 80));
    DrawLine(px, py, px + static_cast<int>(player.DirX() * cell * 2),
                     py + static_cast<int>(player.DirY() * cell * 2), Rgb(255, 80, 80));
}

void Renderer::EnsureBackBuffer(HDC target, int w, int h) {
    if (memDc_ && bw_ == w && bh_ == h) return;
    ReleaseBackBuffer();
    memDc_ = CreateCompatibleDC(target);
    memBmp_ = CreateCompatibleBitmap(target, w, h);
    oldBmp_ = static_cast<HBITMAP>(SelectObject(memDc_, memBmp_));
    bw_ = w;
    bh_ = h;
}

void Renderer::ReleaseBackBuffer() {
    if (memDc_) {
        SelectObject(memDc_, oldBmp_);
        DeleteObject(memBmp_);
        DeleteDC(memDc_);
    }
    memDc_ = nullptr; memBmp_ = nullptr; oldBmp_ = nullptr;
    bw_ = bh_ = 0;
}

void Renderer::Present(HDC target, int w, int h, const std::function<void(HDC)>& overlay) {
    if (w <= 0 || h <= 0) return;
    EnsureBackBuffer(target, w, h);
    SetStretchBltMode(memDc_, COLORONCOLOR);
    StretchDIBits(memDc_, 0, 0, w, h, 0, 0, kWidth, kHeight,
                  pixels_.data(), &bmi_, DIB_RGB_COLORS, SRCCOPY);
    if (overlay) overlay(memDc_);          // GDI-текст поверх буфера
    BitBlt(target, 0, 0, w, h, memDc_, 0, 0, SRCCOPY); // без мерцания
}
