// ============================================================================
//  Renderer.h — программный растеризатор поверх буфера uint32_t (0x00RRGGBB).
//
//  Canvas — это просто картинка в памяти плюс набор примитивов:
//  прямоугольники, линии, круги, эллипсы, многоугольники (в т.ч. с
//  попиксельным «шейдером»), градиенты, копирование/масштабирование других
//  холстов, спрайты из символьных массивов и пост-эффекты.
//
//  Возможности, которые нужны сценам:
//    * clip-прямоугольник (рисование только внутри окна);
//    * смещение (offset) — используется для тряски машины;
//    * трафарет (stencil) — 1 байт на пиксель: можно «пометить» окно
//      машины id-шником, а потом рисовать монстра только внутри этого окна.
//
//  Модуль не зависит от WinAPI: вывод на экран делает Platform.
// ============================================================================
#pragma once

#include "Common.h"

#include <functional>
#include <vector>

class Canvas {
public:
    Canvas() = default;
    Canvas(int w, int h) { resize(w, h); }

    void resize(int w, int h);
    int width() const { return w_; }
    int height() const { return h_; }
    uint32_t* data() { return pixels_.data(); }
    const uint32_t* data() const { return pixels_.data(); }

    // Прямой доступ без clip/offset/stencil (с проверкой границ).
    uint32_t get(int x, int y) const {
        return (x >= 0 && y >= 0 && x < w_ && y < h_) ? pixels_[static_cast<size_t>(y) * w_ + x] : 0u;
    }
    void setRaw(int x, int y, uint32_t c) {
        if (x >= 0 && y >= 0 && x < w_ && y < h_) pixels_[static_cast<size_t>(y) * w_ + x] = c;
    }

    // ---- Состояние рисования ------------------------------------------------
    // Clip задаётся в координатах холста (без учёта offset).
    void setClip(int x, int y, int w, int h);
    void resetClip();
    // Смещение, прибавляемое ко всем координатам примитивов.
    void setOffset(int ox, int oy) { ox_ = ox; oy_ = oy; }
    int offsetX() const { return ox_; }
    int offsetY() const { return oy_; }

    // Трафарет: write != 0 — примитивы дополнительно пишут это значение в
    // трафарет; test != 0 — пиксель рисуется только там, где трафарет == test.
    void clearStencil(uint8_t v = 0);
    void setStencilWrite(uint8_t id) { stencilWrite_ = id; ensureStencil(); }
    void setStencilTest(uint8_t id) { stencilTest_ = id; ensureStencil(); }
    void resetStencilModes() { stencilWrite_ = 0; stencilTest_ = 0; }
    uint8_t stencilAt(int x, int y) const;

    // ---- Базовые операции -----------------------------------------------
    void clear(uint32_t c); // игнорирует clip/offset/stencil
    void plot(int x, int y, uint32_t c);                 // с учётом всех режимов
    void blendPixel(int x, int y, uint32_t c, float a);  // альфа-смешивание
    void addPixel(int x, int y, uint32_t c, float k);    // аддитивный свет

    // ---- Примитивы ------------------------------------------------------
    void fillRect(int x, int y, int w, int h, uint32_t c);
    void blendRect(int x, int y, int w, int h, uint32_t c, float a);
    void addRect(int x, int y, int w, int h, uint32_t c, float k);
    void drawRect(int x, int y, int w, int h, uint32_t c); // контур 1px
    void hline(int x0, int x1, int y, uint32_t c);
    void vline(int x, int y0, int y1, uint32_t c);
    void drawLine(int x0, int y0, int x1, int y1, uint32_t c);
    void blendLine(int x0, int y0, int x1, int y1, uint32_t c, float a);
    void fillCircle(int cx, int cy, int r, uint32_t c);
    void blendCircle(int cx, int cy, int r, uint32_t c, float a);
    void drawCircle(int cx, int cy, int r, uint32_t c);
    void fillEllipse(int cx, int cy, int rx, int ry, uint32_t c);
    void blendEllipse(int cx, int cy, int rx, int ry, uint32_t c, float a);
    // Радиальное свечение: аддитивно, яркость k в центре, плавно к нулю на r.
    void glow(int cx, int cy, int r, uint32_t c, float k);
    void fillTriangle(Vec2 a, Vec2 b, Vec2 c, uint32_t col);
    // Многоугольник (любой простой, правило чётности), до 32 вершин.
    void fillPolygon(const Vec2* pts, int n, uint32_t c);
    void blendPolygon(const Vec2* pts, int n, uint32_t c, float a);
    // Многоугольник с шейдером: shader(x, y, текущий_цвет) -> новый цвет.
    // x, y — координаты холста (уже с учётом offset).
    using PixelShader = std::function<uint32_t(int x, int y, uint32_t dst)>;
    void shadePolygon(const Vec2* pts, int n, const PixelShader& shader);
    void shadeRect(int x, int y, int w, int h, const PixelShader& shader);
    // Вертикальный градиент сверху вниз.
    void gradientV(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
    // Упорядоченный дизеринг двух цветов (t = доля цвета b, 0..1) — «пиксельные» переходы.
    void ditherRect(int x, int y, int w, int h, uint32_t a, uint32_t b, float t);

    // ---- Копирование ------------------------------------------------------
    // Копия src в точку (dx, dy) с учётом clip/offset/stencil.
    void blit(const Canvas& src, int dx, int dy);
    // Масштабирование ближайшим соседом в прямоугольник (dx, dy, dw, dh).
    void blitScaled(const Canvas& src, int dx, int dy, int dw, int dh);
    // То же с умножением на яркость и альфой (для «экрана в полумраке»).
    void blitScaledTinted(const Canvas& src, int dx, int dy, int dw, int dh, float brightness,
                          float alpha);

    // ---- Спрайты из символьных массивов ----------------------------------
    // rows — массив строк одинаковой длины. '.' или ' ' — прозрачный пиксель,
    // '0'..'9' и 'A'..'F' (или 'a'..'f') — индекс в палитре pal (16 цветов).
    void drawSprite(const char* const* rows, int rowCount, int x, int y, const uint32_t* pal,
                    bool flipX = false, int scale = 1, bool flipY = false);
    // Силуэт спрайта одним цветом (для теней и вспышек).
    void drawSpriteSolid(const char* const* rows, int rowCount, int x, int y, uint32_t c,
                         bool flipX = false, int scale = 1);

    // ---- Пост-эффекты на весь холст (игнорируют clip/offset/stencil) -----
    void darken(float k);                    // умножение яркости
    void tint(uint32_t c, float amount);      // смесь с цветом
    void vignette(float strength, float radius = 0.75f);
    void grain(Rng& rng, float amount);       // плёночное зерно
    void scanlines(float strength);           // тёмные чётные строки
    void boxBlur(int radius);                 // размытие (для «боковым зрением»)
    void invert();
    // Свести к 4 оттенкам палитры (по яркости) — для стилизации.
    void posterize4(const uint32_t pal[4]);

private:
    bool passStencil(int x, int y) const;
    void plotRaw(int x, int y, uint32_t c); // x, y уже с offset; проверяет clip/stencil
    void ensureStencil();
    void spanShader(int y, int x0, int x1, const PixelShader& shader);

    int w_ = 0;
    int h_ = 0;
    std::vector<uint32_t> pixels_;
    std::vector<uint8_t> stencil_;
    int clipX0_ = 0, clipY0_ = 0, clipX1_ = 0, clipY1_ = 0; // [x0, x1) x [y0, y1)
    int ox_ = 0, oy_ = 0;
    uint8_t stencilWrite_ = 0;
    uint8_t stencilTest_ = 0;
};
