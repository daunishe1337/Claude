// ============================================================================
//  Renderer.cpp — реализация программного растеризатора (см. Renderer.h).
// ============================================================================
#include "Renderer.h"

#include <cstring>

namespace {

// Матрица Байера 4x4 для упорядоченного дизеринга (значения 0..15).
const int kBayer4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};

inline uint32_t blendRgb(uint32_t dst, uint32_t src, float a) {
    if (a >= 1.0f) return src;
    if (a <= 0.0f) return dst;
    const int ia = static_cast<int>(a * 256.0f);
    const int r = colR(dst) + (((colR(src) - colR(dst)) * ia) >> 8);
    const int g = colG(dst) + (((colG(src) - colG(dst)) * ia) >> 8);
    const int b = colB(dst) + (((colB(src) - colB(dst)) * ia) >> 8);
    return rgb(r, g, b);
}

inline int spriteIndex(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    return -1; // прозрачный
}

} // namespace

// ---------------------------------------------------------------------------
//  Размер и состояние
// ---------------------------------------------------------------------------
void Canvas::resize(int w, int h) {
    w_ = std::max(0, w);
    h_ = std::max(0, h);
    pixels_.assign(static_cast<size_t>(w_) * h_, 0u);
    if (!stencil_.empty()) stencil_.assign(static_cast<size_t>(w_) * h_, 0u);
    resetClip();
    ox_ = oy_ = 0;
}

void Canvas::setClip(int x, int y, int w, int h) {
    clipX0_ = clampi(x, 0, w_);
    clipY0_ = clampi(y, 0, h_);
    clipX1_ = clampi(x + w, 0, w_);
    clipY1_ = clampi(y + h, 0, h_);
}

void Canvas::resetClip() {
    clipX0_ = 0;
    clipY0_ = 0;
    clipX1_ = w_;
    clipY1_ = h_;
}

void Canvas::ensureStencil() {
    if (stencil_.size() != pixels_.size()) stencil_.assign(pixels_.size(), 0u);
}

void Canvas::clearStencil(uint8_t v) {
    ensureStencil();
    std::fill(stencil_.begin(), stencil_.end(), v);
}

uint8_t Canvas::stencilAt(int x, int y) const {
    if (stencil_.empty() || x < 0 || y < 0 || x >= w_ || y >= h_) return 0;
    return stencil_[static_cast<size_t>(y) * w_ + x];
}

inline bool Canvas::passStencil(int x, int y) const {
    if (stencilTest_ == 0) return true;
    return stencil_[static_cast<size_t>(y) * w_ + x] == stencilTest_;
}

inline void Canvas::plotRaw(int x, int y, uint32_t c) {
    if (x < clipX0_ || y < clipY0_ || x >= clipX1_ || y >= clipY1_) return;
    if (!passStencil(x, y)) return;
    const size_t i = static_cast<size_t>(y) * w_ + x;
    pixels_[i] = c;
    if (stencilWrite_ != 0) stencil_[i] = stencilWrite_;
}

// ---------------------------------------------------------------------------
//  Пиксели
// ---------------------------------------------------------------------------
void Canvas::clear(uint32_t c) { std::fill(pixels_.begin(), pixels_.end(), c); }

void Canvas::plot(int x, int y, uint32_t c) { plotRaw(x + ox_, y + oy_, c); }

void Canvas::blendPixel(int x, int y, uint32_t c, float a) {
    x += ox_;
    y += oy_;
    if (x < clipX0_ || y < clipY0_ || x >= clipX1_ || y >= clipY1_) return;
    if (!passStencil(x, y)) return;
    const size_t i = static_cast<size_t>(y) * w_ + x;
    pixels_[i] = blendRgb(pixels_[i], c, a);
    if (stencilWrite_ != 0) stencil_[i] = stencilWrite_;
}

void Canvas::addPixel(int x, int y, uint32_t c, float k) {
    x += ox_;
    y += oy_;
    if (x < clipX0_ || y < clipY0_ || x >= clipX1_ || y >= clipY1_) return;
    if (!passStencil(x, y)) return;
    const size_t i = static_cast<size_t>(y) * w_ + x;
    pixels_[i] = addColor(pixels_[i], scaleColor(c, k));
}

// ---------------------------------------------------------------------------
//  Прямоугольники и линии
// ---------------------------------------------------------------------------
void Canvas::fillRect(int x, int y, int w, int h, uint32_t c) {
    x += ox_;
    y += oy_;
    const int x0 = std::max(x, clipX0_), y0 = std::max(y, clipY0_);
    const int x1 = std::min(x + w, clipX1_), y1 = std::min(y + h, clipY1_);
    if (x0 >= x1 || y0 >= y1) return;
    const bool simple = stencilTest_ == 0 && stencilWrite_ == 0;
    for (int yy = y0; yy < y1; ++yy) {
        if (simple) {
            uint32_t* row = pixels_.data() + static_cast<size_t>(yy) * w_;
            std::fill(row + x0, row + x1, c);
        } else {
            for (int xx = x0; xx < x1; ++xx) plotRaw(xx, yy, c);
        }
    }
}

void Canvas::blendRect(int x, int y, int w, int h, uint32_t c, float a) {
    for (int yy = y; yy < y + h; ++yy)
        for (int xx = x; xx < x + w; ++xx) blendPixel(xx, yy, c, a);
}

void Canvas::addRect(int x, int y, int w, int h, uint32_t c, float k) {
    for (int yy = y; yy < y + h; ++yy)
        for (int xx = x; xx < x + w; ++xx) addPixel(xx, yy, c, k);
}

void Canvas::drawRect(int x, int y, int w, int h, uint32_t c) {
    if (w <= 0 || h <= 0) return;
    hline(x, x + w - 1, y, c);
    hline(x, x + w - 1, y + h - 1, c);
    vline(x, y, y + h - 1, c);
    vline(x + w - 1, y, y + h - 1, c);
}

void Canvas::hline(int x0, int x1, int y, uint32_t c) {
    if (x1 < x0) std::swap(x0, x1);
    fillRect(x0, y, x1 - x0 + 1, 1, c);
}

void Canvas::vline(int x, int y0, int y1, uint32_t c) {
    if (y1 < y0) std::swap(y0, y1);
    fillRect(x, y0, 1, y1 - y0 + 1, c);
}

void Canvas::drawLine(int x0, int y0, int x1, int y1, uint32_t c) {
    // Классический алгоритм Брезенхэма.
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int guard = 0; guard < 4096; ++guard) {
        plot(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Canvas::blendLine(int x0, int y0, int x1, int y1, uint32_t c, float a) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int guard = 0; guard < 4096; ++guard) {
        blendPixel(x0, y0, c, a);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// ---------------------------------------------------------------------------
//  Круги и эллипсы
// ---------------------------------------------------------------------------
void Canvas::fillCircle(int cx, int cy, int r, uint32_t c) { fillEllipse(cx, cy, r, r, c); }

void Canvas::blendCircle(int cx, int cy, int r, uint32_t c, float a) { blendEllipse(cx, cy, r, r, c, a); }

void Canvas::drawCircle(int cx, int cy, int r, uint32_t c) {
    if (r <= 0) { plot(cx, cy, c); return; }
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        plot(cx + x, cy + y, c); plot(cx + y, cy + x, c);
        plot(cx - y, cy + x, c); plot(cx - x, cy + y, c);
        plot(cx - x, cy - y, c); plot(cx - y, cy - x, c);
        plot(cx + y, cy - x, c); plot(cx + x, cy - y, c);
        ++y;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            --x;
            err += 2 * (y - x) + 1;
        }
    }
}

void Canvas::fillEllipse(int cx, int cy, int rx, int ry, uint32_t c) {
    if (rx <= 0 || ry <= 0) { if (rx >= 0 && ry >= 0) plot(cx, cy, c); return; }
    const float frx = rx + 0.5f, fry = ry + 0.5f;
    for (int dy = -ry; dy <= ry; ++dy) {
        const float t = 1.0f - (dy * dy) / (fry * fry);
        if (t < 0.0f) continue;
        const int half = static_cast<int>(frx * std::sqrt(t));
        fillRect(cx - half, cy + dy, 2 * half + 1, 1, c);
    }
}

void Canvas::blendEllipse(int cx, int cy, int rx, int ry, uint32_t c, float a) {
    if (rx <= 0 || ry <= 0) { blendPixel(cx, cy, c, a); return; }
    const float frx = rx + 0.5f, fry = ry + 0.5f;
    for (int dy = -ry; dy <= ry; ++dy) {
        const float t = 1.0f - (dy * dy) / (fry * fry);
        if (t < 0.0f) continue;
        const int half = static_cast<int>(frx * std::sqrt(t));
        for (int dx = -half; dx <= half; ++dx) blendPixel(cx + dx, cy + dy, c, a);
    }
}

void Canvas::glow(int cx, int cy, int r, uint32_t c, float k) {
    if (r <= 0 || k <= 0.0f) return;
    const float inv = 1.0f / static_cast<float>(r * r);
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            const float d2 = static_cast<float>(dx * dx + dy * dy) * inv;
            if (d2 >= 1.0f) continue;
            const float f = (1.0f - d2);
            addPixel(cx + dx, cy + dy, c, k * f * f);
        }
    }
}

// ---------------------------------------------------------------------------
//  Многоугольники (сканирующая строка, правило чётности)
// ---------------------------------------------------------------------------
namespace {

// Для строки y (центр пикселя y + 0.5) находит пересечения с рёбрами.
int scanIntersections(const Vec2* pts, int n, float yc, float* xs, int maxXs) {
    int count = 0;
    for (int i = 0; i < n; ++i) {
        const Vec2& a = pts[i];
        const Vec2& b = pts[(i + 1) % n];
        if ((a.y <= yc && b.y > yc) || (b.y <= yc && a.y > yc)) {
            const float t = (yc - a.y) / (b.y - a.y);
            if (count < maxXs) xs[count++] = a.x + t * (b.x - a.x);
        }
    }
    std::sort(xs, xs + count);
    return count;
}

template <typename SpanFn>
void rasterPolygon(const Vec2* pts, int n, int offX, int offY, int clipY0, int clipY1, SpanFn span) {
    if (n < 3) return;
    n = std::min(n, 32);
    Vec2 p[32];
    float minY = 1e9f, maxY = -1e9f;
    for (int i = 0; i < n; ++i) {
        p[i].x = pts[i].x + static_cast<float>(offX);
        p[i].y = pts[i].y + static_cast<float>(offY);
        minY = std::min(minY, p[i].y);
        maxY = std::max(maxY, p[i].y);
    }
    const int y0 = std::max(clipY0, static_cast<int>(std::floor(minY)));
    const int y1 = std::min(clipY1 - 1, static_cast<int>(std::ceil(maxY)));
    float xs[64];
    for (int y = y0; y <= y1; ++y) {
        const int cnt = scanIntersections(p, n, static_cast<float>(y) + 0.5f, xs, 64);
        for (int k = 0; k + 1 < cnt; k += 2) {
            const int xa = static_cast<int>(std::ceil(xs[k] - 0.5f));
            const int xb = static_cast<int>(std::ceil(xs[k + 1] - 0.5f)) - 1;
            if (xb >= xa) span(y, xa, xb);
        }
    }
}

} // namespace

void Canvas::fillTriangle(Vec2 a, Vec2 b, Vec2 c, uint32_t col) {
    const Vec2 pts[3] = {a, b, c};
    fillPolygon(pts, 3, col);
}

void Canvas::fillPolygon(const Vec2* pts, int n, uint32_t c) {
    rasterPolygon(pts, n, ox_, oy_, clipY0_, clipY1_, [&](int y, int xa, int xb) {
        const int x0 = std::max(xa, clipX0_), x1 = std::min(xb, clipX1_ - 1);
        for (int x = x0; x <= x1; ++x) plotRaw(x, y, c);
    });
}

void Canvas::blendPolygon(const Vec2* pts, int n, uint32_t c, float a) {
    rasterPolygon(pts, n, ox_, oy_, clipY0_, clipY1_, [&](int y, int xa, int xb) {
        const int x0 = std::max(xa, clipX0_), x1 = std::min(xb, clipX1_ - 1);
        for (int x = x0; x <= x1; ++x) {
            if (!passStencil(x, y)) continue;
            const size_t i = static_cast<size_t>(y) * w_ + x;
            pixels_[i] = blendRgb(pixels_[i], c, a);
            if (stencilWrite_ != 0) stencil_[i] = stencilWrite_;
        }
    });
}

void Canvas::spanShader(int y, int x0, int x1, const PixelShader& shader) {
    x0 = std::max(x0, clipX0_);
    x1 = std::min(x1, clipX1_ - 1);
    if (y < clipY0_ || y >= clipY1_) return;
    for (int x = x0; x <= x1; ++x) {
        if (!passStencil(x, y)) continue;
        const size_t i = static_cast<size_t>(y) * w_ + x;
        pixels_[i] = shader(x, y, pixels_[i]);
        if (stencilWrite_ != 0) stencil_[i] = stencilWrite_;
    }
}

void Canvas::shadePolygon(const Vec2* pts, int n, const PixelShader& shader) {
    rasterPolygon(pts, n, ox_, oy_, clipY0_, clipY1_,
                  [&](int y, int xa, int xb) { spanShader(y, xa, xb, shader); });
}

void Canvas::shadeRect(int x, int y, int w, int h, const PixelShader& shader) {
    x += ox_;
    y += oy_;
    for (int yy = std::max(y, clipY0_); yy < std::min(y + h, clipY1_); ++yy)
        spanShader(yy, x, x + w - 1, shader);
}

void Canvas::gradientV(int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    if (h <= 0) return;
    for (int i = 0; i < h; ++i) {
        const float t = h > 1 ? static_cast<float>(i) / static_cast<float>(h - 1) : 0.0f;
        fillRect(x, y + i, w, 1, lerpColor(top, bottom, t));
    }
}

void Canvas::ditherRect(int x, int y, int w, int h, uint32_t a, uint32_t b, float t) {
    const int level = clampi(static_cast<int>(t * 16.0f + 0.5f), 0, 16);
    for (int yy = y; yy < y + h; ++yy) {
        for (int xx = x; xx < x + w; ++xx) {
            const int sx = xx + ox_, sy = yy + oy_;
            const bool useB = kBayer4[sy & 3][sx & 3] < level;
            plot(xx, yy, useB ? b : a);
        }
    }
}

// ---------------------------------------------------------------------------
//  Копирование холстов
// ---------------------------------------------------------------------------
void Canvas::blit(const Canvas& src, int dx, int dy) {
    for (int y = 0; y < src.height(); ++y)
        for (int x = 0; x < src.width(); ++x) plot(dx + x, dy + y, src.get(x, y));
}

void Canvas::blitScaled(const Canvas& src, int dx, int dy, int dw, int dh) {
    if (dw <= 0 || dh <= 0 || src.width() <= 0 || src.height() <= 0) return;
    for (int y = 0; y < dh; ++y) {
        const int sy = y * src.height() / dh;
        for (int x = 0; x < dw; ++x) {
            const int sx = x * src.width() / dw;
            plot(dx + x, dy + y, src.get(sx, sy));
        }
    }
}

void Canvas::blitScaledTinted(const Canvas& src, int dx, int dy, int dw, int dh, float brightness,
                              float alpha) {
    if (dw <= 0 || dh <= 0 || src.width() <= 0 || src.height() <= 0) return;
    for (int y = 0; y < dh; ++y) {
        const int sy = y * src.height() / dh;
        for (int x = 0; x < dw; ++x) {
            const int sx = x * src.width() / dw;
            blendPixel(dx + x, dy + y, scaleColor(src.get(sx, sy), brightness), alpha);
        }
    }
}

// ---------------------------------------------------------------------------
//  Спрайты
// ---------------------------------------------------------------------------
void Canvas::drawSprite(const char* const* rows, int rowCount, int x, int y, const uint32_t* pal,
                        bool flipX, int scale, bool flipY) {
    if (!rows || rowCount <= 0 || !pal) return;
    if (scale < 1) scale = 1;
    const int w = static_cast<int>(std::strlen(rows[0]));
    for (int r = 0; r < rowCount; ++r) {
        const char* row = rows[flipY ? (rowCount - 1 - r) : r];
        for (int col = 0; col < w; ++col) {
            const int idx = spriteIndex(row[flipX ? (w - 1 - col) : col]);
            if (idx < 0) continue;
            if (scale == 1) plot(x + col, y + r, pal[idx]);
            else fillRect(x + col * scale, y + r * scale, scale, scale, pal[idx]);
        }
    }
}

void Canvas::drawSpriteSolid(const char* const* rows, int rowCount, int x, int y, uint32_t c,
                             bool flipX, int scale) {
    if (!rows || rowCount <= 0) return;
    if (scale < 1) scale = 1;
    const int w = static_cast<int>(std::strlen(rows[0]));
    for (int r = 0; r < rowCount; ++r) {
        for (int col = 0; col < w; ++col) {
            if (spriteIndex(rows[r][flipX ? (w - 1 - col) : col]) < 0) continue;
            if (scale == 1) plot(x + col, y + r, c);
            else fillRect(x + col * scale, y + r * scale, scale, scale, c);
        }
    }
}

// ---------------------------------------------------------------------------
//  Пост-эффекты
// ---------------------------------------------------------------------------
void Canvas::darken(float k) {
    for (uint32_t& p : pixels_) p = scaleColor(p, k);
}

void Canvas::tint(uint32_t c, float amount) {
    if (amount <= 0.0f) return;
    for (uint32_t& p : pixels_) p = blendRgb(p, c, amount);
}

void Canvas::vignette(float strength, float radius) {
    if (strength <= 0.0f || w_ == 0 || h_ == 0) return;
    const float cx = w_ * 0.5f, cy = h_ * 0.5f;
    const float aspect = static_cast<float>(w_) / static_cast<float>(h_);
    for (int y = 0; y < h_; ++y) {
        for (int x = 0; x < w_; ++x) {
            const float nx = (x + 0.5f - cx) / cx;
            const float ny = (y + 0.5f - cy) / cy / aspect * 1.6f;
            const float d = std::sqrt(nx * nx + ny * ny);
            const float v = smoothstep(radius, radius + 0.6f, d) * strength;
            if (v > 0.0f) {
                uint32_t& p = pixels_[static_cast<size_t>(y) * w_ + x];
                p = scaleColor(p, 1.0f - saturate(v));
            }
        }
    }
}

void Canvas::grain(Rng& rng, float amount) {
    if (amount <= 0.0f) return;
    const int a = static_cast<int>(amount * 255.0f);
    if (a <= 0) return;
    for (uint32_t& p : pixels_) {
        const int n = static_cast<int>(rng.nextU32() % static_cast<uint32_t>(2 * a + 1)) - a;
        p = rgb(clampi(colR(p) + n, 0, 255), clampi(colG(p) + n, 0, 255), clampi(colB(p) + n, 0, 255));
    }
}

void Canvas::scanlines(float strength) {
    if (strength <= 0.0f) return;
    const float k = 1.0f - saturate(strength);
    for (int y = 1; y < h_; y += 2) {
        uint32_t* row = pixels_.data() + static_cast<size_t>(y) * w_;
        for (int x = 0; x < w_; ++x) row[x] = scaleColor(row[x], k);
    }
}

void Canvas::boxBlur(int radius) {
    if (radius <= 0 || w_ == 0 || h_ == 0) return;
    std::vector<uint32_t> tmp(pixels_.size());
    // Горизонтальный проход (скользящее окно).
    for (int y = 0; y < h_; ++y) {
        int sr = 0, sg = 0, sb = 0;
        const uint32_t* row = pixels_.data() + static_cast<size_t>(y) * w_;
        for (int x = -radius; x <= radius; ++x) {
            const uint32_t c = row[clampi(x, 0, w_ - 1)];
            sr += colR(c); sg += colG(c); sb += colB(c);
        }
        const int n = 2 * radius + 1;
        for (int x = 0; x < w_; ++x) {
            tmp[static_cast<size_t>(y) * w_ + x] = rgb(sr / n, sg / n, sb / n);
            const uint32_t out = row[clampi(x - radius, 0, w_ - 1)];
            const uint32_t in = row[clampi(x + radius + 1, 0, w_ - 1)];
            sr += colR(in) - colR(out);
            sg += colG(in) - colG(out);
            sb += colB(in) - colB(out);
        }
    }
    // Вертикальный проход.
    for (int x = 0; x < w_; ++x) {
        int sr = 0, sg = 0, sb = 0;
        for (int y = -radius; y <= radius; ++y) {
            const uint32_t c = tmp[static_cast<size_t>(clampi(y, 0, h_ - 1)) * w_ + x];
            sr += colR(c); sg += colG(c); sb += colB(c);
        }
        const int n = 2 * radius + 1;
        for (int y = 0; y < h_; ++y) {
            pixels_[static_cast<size_t>(y) * w_ + x] = rgb(sr / n, sg / n, sb / n);
            const uint32_t out = tmp[static_cast<size_t>(clampi(y - radius, 0, h_ - 1)) * w_ + x];
            const uint32_t in = tmp[static_cast<size_t>(clampi(y + radius + 1, 0, h_ - 1)) * w_ + x];
            sr += colR(in) - colR(out);
            sg += colG(in) - colG(out);
            sb += colB(in) - colB(out);
        }
    }
}

void Canvas::invert() {
    for (uint32_t& p : pixels_) p = (~p) & 0x00FFFFFFu;
}

void Canvas::posterize4(const uint32_t pal[4]) {
    for (uint32_t& p : pixels_) {
        const int i = clampi(static_cast<int>(luminance(p) * 4.0f), 0, 3);
        p = pal[i];
    }
}
