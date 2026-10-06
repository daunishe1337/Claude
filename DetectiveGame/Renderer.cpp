// Renderer.cpp
#include "Renderer.h"
#include <cmath>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

Renderer::Renderer() : pixels_(kWidth * kHeight, 0), vignette_(kWidth * kHeight, 0.0f) {
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            double nx = (x - kWidth / 2.0) / (kWidth / 2.0);
            double ny = (y - kHeight / 2.0) / (kHeight / 2.0);
            double d = (nx * nx + ny * ny) / 2.0; // 0 в центре, 1 в углах
            vignette_[y * kWidth + x] = static_cast<float>(d);
        }
    }
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

void Renderer::FillCircle(int cx, int cy, int radius, uint32_t color) {
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx)
            if (dx * dx + dy * dy <= radius * radius) SetPixel(cx + dx, cy + dy, color);
}

void Renderer::FillGradientV(int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    for (int j = 0; j < h; ++j) {
        uint32_t c = MixColor(top, bottom, h > 1 ? static_cast<double>(j) / (h - 1) : 0.0);
        FillRect(x, y + j, w, 1, c);
    }
}

void Renderer::ApplyVignette(double strength) {
    for (int i = 0; i < kWidth * kHeight; ++i) {
        double f = 1.0 - strength * vignette_[i];
        if (f < 0.0) f = 0.0;
        uint32_t p = pixels_[i];
        int r = static_cast<int>(((p >> 16) & 255) * f);
        int g = static_cast<int>(((p >> 8) & 255) * f);
        int b = static_cast<int>((p & 255) * f);
        pixels_[i] = Rgb(r, g, b);
    }
}

void Renderer::Fade(uint32_t color, double amount) {
    if (amount <= 0.0) return;
    for (uint32_t& p : pixels_) p = MixColor(p, color, amount);
}

void Renderer::DrawTopDown(const Map& map, const Player& player, double lightRadius) {
    // Масштаб подбираем так, чтобы любая карта помещалась в буфер
    int cell = 12;
    while (cell > 4 && (map.Width() * cell > kWidth - 20 || map.Height() * cell > kHeight - 20)) --cell;
    const int ox = 10, oy = 10;
    Clear(Rgb(6, 6, 8));

    for (int y = 0; y < map.Height(); ++y) {
        for (int x = 0; x < map.Width(); ++x) {
            uint32_t c;
            switch (map.Cell(x, y)) {
            case '1': c = Rgb(160, 80, 64);   break; // кирпич
            case '2': c = Rgb(139, 90, 43);   break; // дерево
            case '3': c = Rgb(128, 136, 144); break; // камень
            case '4': c = Rgb(79, 127, 95);   break; // обои
            case '5': c = Rgb(200, 160, 64);  break; // мебель
            case '6': c = Rgb(110, 115, 125); break; // офисный бетон
            case '7': c = Rgb(90, 140, 180);  break; // стекло
            default:  c = Rgb(40, 40, 48);    break; // пол
            }
            if (lightRadius > 0.0) {
                double dx = x + 0.5 - player.x, dy = y + 0.5 - player.y;
                double f = 1.0 - std::sqrt(dx * dx + dy * dy) / lightRadius;
                if (f < 0.0) f = 0.0;
                c = MixColor(Rgb(0, 0, 0), c, f * f);
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
