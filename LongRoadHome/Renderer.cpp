// Renderer.cpp
#include "Renderer.h"
#include <cctype>
#include <cstring>

namespace lrh {

namespace {
struct Glyph { char c; uint8_t col[5]; };  // столбцы, младший бит = верхняя строка
const Glyph kGlyphs[] = {
    {' ', {0x00,0x00,0x00,0x00,0x00}}, {'!', {0x00,0x00,0x5F,0x00,0x00}},
    {'\'',{0x00,0x05,0x03,0x00,0x00}}, {'%', {0x23,0x13,0x08,0x64,0x62}},
    {'(', {0x00,0x1C,0x22,0x41,0x00}}, {')', {0x00,0x41,0x22,0x1C,0x00}},
    {'*', {0x14,0x08,0x3E,0x08,0x14}}, {'+', {0x08,0x08,0x3E,0x08,0x08}},
    {',', {0x00,0x80,0x60,0x00,0x00}}, {'-', {0x08,0x08,0x08,0x08,0x08}},
    {'.', {0x00,0x60,0x60,0x00,0x00}}, {'/', {0x20,0x10,0x08,0x04,0x02}},
    {'0', {0x3E,0x51,0x49,0x45,0x3E}}, {'1', {0x00,0x42,0x7F,0x40,0x00}},
    {'2', {0x42,0x61,0x51,0x49,0x46}}, {'3', {0x21,0x41,0x45,0x4B,0x31}},
    {'4', {0x18,0x14,0x12,0x7F,0x10}}, {'5', {0x27,0x45,0x45,0x45,0x39}},
    {'6', {0x3C,0x4A,0x49,0x49,0x30}}, {'7', {0x01,0x71,0x09,0x05,0x03}},
    {'8', {0x36,0x49,0x49,0x49,0x36}}, {'9', {0x06,0x49,0x49,0x29,0x1E}},
    {':', {0x00,0x36,0x36,0x00,0x00}}, {'<', {0x08,0x14,0x22,0x41,0x00}},
    {'=', {0x14,0x14,0x14,0x14,0x14}}, {'>', {0x00,0x41,0x22,0x14,0x08}},
    {'?', {0x02,0x01,0x51,0x09,0x06}},
    {'A', {0x7E,0x11,0x11,0x11,0x7E}}, {'B', {0x7F,0x49,0x49,0x49,0x36}},
    {'C', {0x3E,0x41,0x41,0x41,0x22}}, {'D', {0x7F,0x41,0x41,0x22,0x1C}},
    {'E', {0x7F,0x49,0x49,0x49,0x41}}, {'F', {0x7F,0x09,0x09,0x09,0x01}},
    {'G', {0x3E,0x41,0x49,0x49,0x7A}}, {'H', {0x7F,0x08,0x08,0x08,0x7F}},
    {'I', {0x00,0x41,0x7F,0x41,0x00}}, {'J', {0x20,0x40,0x41,0x3F,0x01}},
    {'K', {0x7F,0x08,0x14,0x22,0x41}}, {'L', {0x7F,0x40,0x40,0x40,0x40}},
    {'M', {0x7F,0x02,0x0C,0x02,0x7F}}, {'N', {0x7F,0x04,0x08,0x10,0x7F}},
    {'O', {0x3E,0x41,0x41,0x41,0x3E}}, {'P', {0x7F,0x09,0x09,0x09,0x06}},
    {'Q', {0x3E,0x41,0x51,0x21,0x5E}}, {'R', {0x7F,0x09,0x19,0x29,0x46}},
    {'S', {0x46,0x49,0x49,0x49,0x31}}, {'T', {0x01,0x01,0x7F,0x01,0x01}},
    {'U', {0x3F,0x40,0x40,0x40,0x3F}}, {'V', {0x1F,0x20,0x40,0x20,0x1F}},
    {'W', {0x3F,0x40,0x38,0x40,0x3F}}, {'X', {0x63,0x14,0x08,0x14,0x63}},
    {'Y', {0x07,0x08,0x70,0x08,0x07}}, {'Z', {0x61,0x51,0x49,0x45,0x43}},
};
}  // namespace

Renderer::Renderer() : buf_(size_t(kW) * kH, 0), tmp_(size_t(kW) * kH, 0), vig_(size_t(kW) * kH, 0) {
    std::memset(font_, 0, sizeof(font_));
    for (const Glyph& g : kGlyphs) std::memcpy(font_[int(g.c)], g.col, 5);
    // Таблица виньетки: 0..255, чем дальше от центра, тем больше.
    for (int y = 0; y < kH; ++y)
        for (int x = 0; x < kW; ++x) {
            float dx = (x - kW * 0.5f) / (kW * 0.5f), dy = (y - kH * 0.5f) / (kH * 0.5f);
            float d = saturate((dx * dx * 0.8f + dy * dy * 1.1f - 0.25f) / 1.1f);
            vig_[size_t(y) * kW + x] = uint8_t(d * d * 255.0f);
        }
}

void Renderer::setClip(int x, int y, int w, int h) {
    cx0_ = std::max(0, x); cy0_ = std::max(0, y);
    cx1_ = std::min(kW, x + w); cy1_ = std::min(kH, y + h);
}
void Renderer::clearClip() { cx0_ = 0; cy0_ = 0; cx1_ = kW; cy1_ = kH; }

void Renderer::clear(uint32_t c) { std::fill(buf_.begin(), buf_.end(), c); }

void Renderer::put(int x, int y, uint32_t c) {
    if (x < cx0_ || x >= cx1_ || y < cy0_ || y >= cy1_) return;
    buf_[size_t(y) * kW + x] = c;
}

void Renderer::blendPut(int x, int y, uint32_t c, float a) {
    if (x < cx0_ || x >= cx1_ || y < cy0_ || y >= cy1_) return;
    if (a >= 0.999f) { buf_[size_t(y) * kW + x] = c; return; }
    int ia = int(a * 256.0f);
    if (ia <= 0) return;
    uint32_t& d = buf_[size_t(y) * kW + x];
    int r = (((c >> 16) & 255) * ia + ((d >> 16) & 255) * (256 - ia)) >> 8;
    int g = (((c >> 8) & 255) * ia + ((d >> 8) & 255) * (256 - ia)) >> 8;
    int b = ((c & 255) * ia + (d & 255) * (256 - ia)) >> 8;
    d = (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

void Renderer::rect(int x, int y, int w, int h, uint32_t c, float a) {
    int x0 = std::max(x, cx0_), x1 = std::min(x + w, cx1_);
    int y0 = std::max(y, cy0_), y1 = std::min(y + h, cy1_);
    for (int j = y0; j < y1; ++j)
        for (int i = x0; i < x1; ++i) blendPut(i, j, c, a);
}

void Renderer::rectOutline(int x, int y, int w, int h, uint32_t c) {
    rect(x, y, w, 1, c); rect(x, y + h - 1, w, 1, c);
    rect(x, y, 1, h, c); rect(x + w - 1, y, 1, h, c);
}

void Renderer::vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    for (int j = 0; j < h; ++j) rect(x, y + j, w, 1, mixColor(top, bottom, h > 1 ? float(j) / float(h - 1) : 0.0f));
}

void Renderer::line(int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        put(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Renderer::thickLine(float x0, float y0, float x1, float y1, float w0, float w1, uint32_t c) {
    float dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) { ellipse(x0, y0, w0 * 0.5f, w0 * 0.5f, c); return; }
    float nx = -dy / len, ny = dx / len;
    Pt p[4] = {{x0 + nx * w0 * 0.5f, y0 + ny * w0 * 0.5f}, {x1 + nx * w1 * 0.5f, y1 + ny * w1 * 0.5f},
               {x1 - nx * w1 * 0.5f, y1 - ny * w1 * 0.5f}, {x0 - nx * w0 * 0.5f, y0 - ny * w0 * 0.5f}};
    poly(p, 4, c);
}

void Renderer::ellipse(float cx, float cy, float rx, float ry, uint32_t c, float a) {
    if (rx <= 0.0f || ry <= 0.0f) return;
    int y0 = std::max(int(std::floor(cy - ry)), cy0_), y1 = std::min(int(std::ceil(cy + ry)), cy1_ - 1);
    for (int y = y0; y <= y1; ++y) {
        float dy = (float(y) + 0.5f - cy) / ry;
        if (dy < -1.0f || dy > 1.0f) continue;
        float hw = rx * std::sqrt(1.0f - dy * dy);
        int xa = int(std::ceil(cx - hw - 0.5f)), xb = int(std::floor(cx + hw - 0.5f));
        for (int x = std::max(xa, cx0_); x <= std::min(xb, cx1_ - 1); ++x) blendPut(x, y, c, a);
    }
}

void Renderer::poly(const Pt* p, int n, uint32_t c, float a) {
    if (n < 3) return;
    float minY = p[0].y, maxY = p[0].y;
    for (int i = 1; i < n; ++i) { minY = std::min(minY, p[i].y); maxY = std::max(maxY, p[i].y); }
    int y0 = std::max(int(std::floor(minY)), cy0_), y1 = std::min(int(std::ceil(maxY)), cy1_ - 1);
    float xs[16];
    for (int y = y0; y <= y1; ++y) {
        float sy = float(y) + 0.5f;
        int cnt = 0;
        for (int i = 0; i < n && cnt < 16; ++i) {
            const Pt& a0 = p[i];
            const Pt& b0 = p[(i + 1) % n];
            if ((a0.y <= sy && b0.y > sy) || (b0.y <= sy && a0.y > sy))
                xs[cnt++] = a0.x + (sy - a0.y) / (b0.y - a0.y) * (b0.x - a0.x);
        }
        std::sort(xs, xs + cnt);
        for (int k = 0; k + 1 < cnt; k += 2) {
            int xa = int(std::ceil(xs[k] - 0.5f)), xb = int(std::ceil(xs[k + 1] - 0.5f)) - 1;
            for (int x = std::max(xa, cx0_); x <= std::min(xb, cx1_ - 1); ++x) blendPut(x, y, c, a);
        }
    }
}

int Renderer::textWidth(const char* s, int scale) {
    int n = 0;
    for (; *s; ++s) ++n;
    return n > 0 ? (n * 6 - 1) * scale : 0;
}

void Renderer::text(int x, int y, const char* s, uint32_t c, int scale) {
    for (; *s; ++s, x += 6 * scale) {
        int ch = std::toupper(static_cast<unsigned char>(*s));
        if (ch < 0 || ch > 127) continue;
        for (int col = 0; col < 5; ++col) {
            uint8_t bits = font_[ch][col];
            for (int row = 0; row < 8; ++row)
                if (bits & (1 << row)) rect(x + col * scale, y + row * scale, scale, scale, c);
        }
    }
}

void Renderer::textCentered(int cx, int y, const char* s, uint32_t c, int scale) {
    text(cx - textWidth(s, scale) / 2, y, s, c, scale);
}

void Renderer::tint(uint32_t c, float a) {
    int ia = int(saturate(a) * 256.0f);
    if (ia <= 0) return;
    int cr = (c >> 16) & 255, cg = (c >> 8) & 255, cb = c & 255;
    for (uint32_t& d : buf_) {
        int r = (cr * ia + int((d >> 16) & 255) * (256 - ia)) >> 8;
        int g = (cg * ia + int((d >> 8) & 255) * (256 - ia)) >> 8;
        int b = (cb * ia + int(d & 255) * (256 - ia)) >> 8;
        d = (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
    }
}

void Renderer::addLightX(float amount, uint32_t c, float cx, float spread) {
    if (amount <= 0.001f) return;
    int cr = (c >> 16) & 255, cg = (c >> 8) & 255, cb = c & 255;
    for (int x = 0; x < kW; ++x) {
        float w = amount * saturate(1.0f - std::fabs(float(x) - cx) / spread);
        if (w <= 0.0f) continue;
        int ar = int(cr * w), ag = int(cg * w), ab = int(cb * w);
        for (int y = 0; y < kH; ++y) {
            uint32_t& d = buf_[size_t(y) * kW + x];
            int r = std::min(255, int((d >> 16) & 255) + ar);
            int g = std::min(255, int((d >> 8) & 255) + ag);
            int b = std::min(255, int(d & 255) + ab);
            d = (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
        }
    }
}

void Renderer::shift(int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    std::fill(tmp_.begin(), tmp_.end(), 0u);
    for (int y = 0; y < kH; ++y) {
        int sy = y - dy;
        if (sy < 0 || sy >= kH) continue;
        for (int x = 0; x < kW; ++x) {
            int sx = x - dx;
            if (sx < 0 || sx >= kW) continue;
            tmp_[size_t(y) * kW + x] = buf_[size_t(sy) * kW + sx];
        }
    }
    buf_.swap(tmp_);
}

void Renderer::vignette(float strength) {
    int s = int(saturate(strength) * 256.0f);
    for (size_t i = 0; i < buf_.size(); ++i) {
        int k = 256 - ((int(vig_[i]) * s) >> 8);
        uint32_t d = buf_[i];
        int r = (int((d >> 16) & 255) * k) >> 8, g = (int((d >> 8) & 255) * k) >> 8, b = (int(d & 255) * k) >> 8;
        buf_[i] = (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
    }
}

void Renderer::grain(int amount) {
    if (amount <= 0) return;
    for (uint32_t& d : buf_) {
        int n = int(g_rng.nextU() % uint32_t(amount * 2 + 1)) - amount;
        int r = std::max(0, std::min(255, int((d >> 16) & 255) + n));
        int g = std::max(0, std::min(255, int((d >> 8) & 255) + n));
        int b = std::max(0, std::min(255, int(d & 255) + n));
        d = (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
    }
}

}  // namespace lrh
