// Renderer.h - программный рендер в буфер uint32_t (0x00RRGGBB), 320x180.
// Не зависит от WinAPI: вывод на экран делает main.cpp.
#pragma once
#include <cstdint>
#include <vector>
#include "Common.h"

namespace lrh {

struct Pt { float x, y; };

class Renderer {
public:
    Renderer();

    const uint32_t* pixels() const { return buf_.data(); }
    uint32_t* pixels() { return buf_.data(); }

    // --- примитивы ---
    void setClip(int x, int y, int w, int h);
    void clearClip();
    void clear(uint32_t c);
    void put(int x, int y, uint32_t c);
    void blendPut(int x, int y, uint32_t c, float a);
    void rect(int x, int y, int w, int h, uint32_t c, float a = 1.0f);
    void rectOutline(int x, int y, int w, int h, uint32_t c);
    void vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
    void line(int x0, int y0, int x1, int y1, uint32_t c);
    void thickLine(float x0, float y0, float x1, float y1, float w0, float w1, uint32_t c);
    void ellipse(float cx, float cy, float rx, float ry, uint32_t c, float a = 1.0f);
    void poly(const Pt* p, int n, uint32_t c, float a = 1.0f);  // заливка (even-odd)

    // --- текст (шрифт 5x7 зашит в код) ---
    void text(int x, int y, const char* s, uint32_t c, int scale = 1);
    void textCentered(int cx, int y, const char* s, uint32_t c, int scale = 1);
    static int textWidth(const char* s, int scale = 1);

    // --- пост-эффекты на весь кадр ---
    void tint(uint32_t c, float a);                 // смешивание с цветом
    void addLightX(float amount, uint32_t c, float cx, float spread);  // аддитивный свет с градиентом по X
    void shift(int dx, int dy);                     // тряска кадра
    void vignette(float strength);
    void grain(int amount);

private:
    std::vector<uint32_t> buf_;
    std::vector<uint32_t> tmp_;
    std::vector<uint8_t>  vig_;
    int cx0_ = 0, cy0_ = 0, cx1_ = kW, cy1_ = kH;  // clip [x0,x1) [y0,y1)
    uint8_t font_[128][5];
};

}  // namespace lrh
