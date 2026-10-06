// Renderer.cpp
#include "Renderer.h"
#include <cmath>
#include "Textures.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

Renderer::Renderer() : pixels_(kWidth * kHeight, 0), vignette_(kWidth * kHeight, 0.0f), zbuffer_(kWidth, 0.0) {
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

namespace {

// Освещённость по расстоянию: тёплый "фонарь", быстро тонущий во тьме
double LightAt(double dist, double flicker) {
    double l = flicker / (1.0 + dist * dist * 0.05);
    return l < 0.025 ? 0.025 : l;
}

// Применяет яркость k и тёплый оттенок света к цвету
uint32_t Lit(uint32_t c, double k) {
    int r = static_cast<int>(((c >> 16) & 255) * k);
    int g = static_cast<int>(((c >> 8) & 255) * k * 0.95);
    int b = static_cast<int>((c & 255) * k * 0.85);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return Rgb(r, g, b);
}

} // namespace

void Renderer::DrawWorld(const Map& map, const Player& player, double flicker) {
    const int W = kWidth, H = kHeight, half = H / 2;
    const double px = player.x, py = player.y;
    const double dirX = player.DirX(), dirY = player.DirY();
    // Плоскость камеры перпендикулярна взгляду; длина 0.66 даёт угол обзора около 66 градусов.
    // (вправо на экране = (-dirY, dirX), так как ось Y направлена вниз)
    const double planeX = -dirY * 0.66, planeY = dirX * 0.66;

    const Texture& floorTex = Textures::Floor(map.LevelId());
    const Texture& ceilTex = Textures::Ceiling(map.LevelId());

    // ---- Пол и потолок: строка за строкой ----
    const double rx0 = dirX - planeX, ry0 = dirY - planeY; // левый луч
    const double rx1 = dirX + planeX, ry1 = dirY + planeY; // правый луч
    for (int y = 0; y < half; ++y) {
        // Горизонтальная полоса экрана: потолок в строке y, пол - в зеркальной строке
        int p = half - y; // расстояние от горизонта в пикселях
        double rowDist = (0.5 * H) / p;
        double stepX = rowDist * (rx1 - rx0) / W;
        double stepY = rowDist * (ry1 - ry0) / W;
        double fx = px + rowDist * rx0;
        double fy = py + rowDist * ry0;
        double light = LightAt(rowDist, flicker);
        uint32_t* ceilRow = &pixels_[y * W];
        uint32_t* floorRow = &pixels_[(H - 1 - y) * W];
        for (int x = 0; x < W; ++x) {
            int tx = static_cast<int>(std::floor(fx * Texture::kSize));
            int ty = static_cast<int>(std::floor(fy * Texture::kSize));
            floorRow[x] = Lit(floorTex.At(tx, ty), light);
            ceilRow[x] = Lit(ceilTex.At(tx, ty), light);
            fx += stepX;
            fy += stepY;
        }
    }

    // ---- Стены: по лучу на каждую колонку (алгоритм DDA) ----
    for (int x = 0; x < W; ++x) {
        double cameraX = 2.0 * x / W - 1.0;
        double rayX = dirX + planeX * cameraX;
        double rayY = dirY + planeY * cameraX;

        int mapX = static_cast<int>(std::floor(px));
        int mapY = static_cast<int>(std::floor(py));
        double deltaX = rayX == 0.0 ? 1e30 : std::fabs(1.0 / rayX);
        double deltaY = rayY == 0.0 ? 1e30 : std::fabs(1.0 / rayY);
        int stepMapX, stepMapY;
        double sideX, sideY;
        if (rayX < 0) { stepMapX = -1; sideX = (px - mapX) * deltaX; }
        else          { stepMapX = 1;  sideX = (mapX + 1.0 - px) * deltaX; }
        if (rayY < 0) { stepMapY = -1; sideY = (py - mapY) * deltaY; }
        else          { stepMapY = 1;  sideY = (mapY + 1.0 - py) * deltaY; }

        int side = 0;
        for (int i = 0; i < 128; ++i) { // ограничение на всякий случай
            if (sideX < sideY) { sideX += deltaX; mapX += stepMapX; side = 0; }
            else               { sideY += deltaY; mapY += stepMapY; side = 1; }
            if (map.IsWall(mapX, mapY)) break;
        }

        // Перпендикулярное расстояние (без "рыбьего глаза")
        double perp = side == 0 ? sideX - deltaX : sideY - deltaY;
        if (perp < 0.05) perp = 0.05;
        zbuffer_[x] = perp;

        int lineH = static_cast<int>(H / perp);
        int drawStart = -lineH / 2 + half;
        int drawEnd = lineH / 2 + half;

        // Какой столбец текстуры попал под луч
        double wallX = side == 0 ? py + perp * rayY : px + perp * rayX;
        wallX -= std::floor(wallX);
        int texX = static_cast<int>(wallX * Texture::kSize);
        if ((side == 0 && rayX > 0) || (side == 1 && rayY < 0)) texX = Texture::kSize - 1 - texX;

        const Texture& tex = Textures::Wall(map.Cell(mapX, mapY));
        double texStep = static_cast<double>(Texture::kSize) / lineH;
        double texPos = (drawStart < 0 ? -drawStart : 0) * texStep;

        double light = LightAt(perp, flicker);
        if (side == 1) light *= 0.72; // грани по оси Y темнее: объём

        int y0 = drawStart < 0 ? 0 : drawStart;
        int y1 = drawEnd >= H ? H - 1 : drawEnd;
        for (int y = y0; y <= y1; ++y) {
            int texY = static_cast<int>(texPos);
            texPos += texStep;
            pixels_[y * W + x] = Lit(tex.At(texX, texY), light);
        }
    }
}

void Renderer::DrawMiniMap(const Map& map, const Player& player) {
    const int cell = 4;
    const int ox = kWidth - map.Width() * cell - 8, oy = 8;
    FillRect(ox - 2, oy - 2, map.Width() * cell + 4, map.Height() * cell + 4, Rgb(0, 0, 0));
    for (int y = 0; y < map.Height(); ++y)
        for (int x = 0; x < map.Width(); ++x)
            FillRect(ox + x * cell, oy + y * cell, cell, cell,
                     map.IsWall(x, y) ? Rgb(150, 130, 110) : Rgb(30, 30, 36));
    int px = ox + static_cast<int>(player.x * cell), py = oy + static_cast<int>(player.y * cell);
    FillRect(px - 1, py - 1, 3, 3, Rgb(255, 230, 80));
    DrawLine(px, py, px + static_cast<int>(player.DirX() * 8), py + static_cast<int>(player.DirY() * 8),
             Rgb(255, 80, 80));
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
