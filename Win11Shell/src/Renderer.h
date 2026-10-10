#pragma once
// Обёртка над GDI+: двойная буферизация (DIB-секция), скруглённые прямоугольники, мягкие тени,
// материалы Mica/Acrylic (размытие участка фона), текст Segoe UI с ClearType и иконки.

#include "Common.h"
#include "Icons.h"
#include "Theme.h"

enum class FontWeight
{
    Light,
    Semilight,
    Regular,
    Semibold,
    Bold,
};

enum class FontFace
{
    UI,       // Segoe UI
    Mono,     // Consolas
    Terminal, // Cascadia Mono / Consolas
};

namespace TextFlags
{
constexpr unsigned Left = 0;
constexpr unsigned Center = 1;
constexpr unsigned Right = 2;
constexpr unsigned Top = 0;
constexpr unsigned VCenter = 4;
constexpr unsigned Bottom = 8;
constexpr unsigned Wrap = 16;
constexpr unsigned NoEllipsis = 32;
constexpr unsigned Middle = Center | VCenter;
} // namespace TextFlags

class Renderer
{
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    static bool Startup();
    static void Shutdown();
    static Renderer& Get() { return *s_instance; } // единственный экземпляр (для измерения текста)

    // ---- кадр -------------------------------------------------------------
    void Resize(int pxWidth, int pxHeight, float scale);
    void BeginFrame();
    void EndFrame(HDC target);
    void Flush();
    float Scale() const { return m_scale; }
    float Width() const { return static_cast<float>(m_pxW) / m_scale; }
    float Height() const { return static_cast<float>(m_pxH) / m_scale; }
    int PixelWidth() const { return m_pxW; }
    int PixelHeight() const { return m_pxH; }
    Gdiplus::Graphics& G() { return *m_g; }
    const uint32_t* Bits() const { return m_bits; }
    HDC MemDC() const { return m_memDC; }
    bool Ready() const { return m_g != nullptr; }

    // ---- стеки состояния ---------------------------------------------------
    void PushTransform(float dx, float dy, float scale = 1.0f);
    void PopTransform();
    void PushClip(const Rect& r);
    void PopClip();
    void PushOpacity(float opacity);
    void PopOpacity();
    float Opacity() const { return m_opacity; }
    float DeviceScale() const { return m_xf.s; }
    Rect ToDevice(const Rect& r) const;
    Point ToDevice(Point p) const;

    // ---- примитивы (в DIP) -------------------------------------------------
    Gdiplus::Color Gp(Color c) const;
    void Clear(Color c);
    void FillRect(const Rect& r, Color c);
    void StrokeRect(const Rect& r, Color c, float width = 1.0f);
    void FillRoundRect(const Rect& r, float radius, Color c);
    void FillRoundRect(const Rect& r, float tl, float tr, float br, float bl, Color c);
    void StrokeRoundRect(const Rect& r, float radius, Color c, float width = 1.0f);
    void FillGradientV(const Rect& r, float radius, Color top, Color bottom);
    void FillGradientH(const Rect& r, float radius, Color left, Color right);
    void FillEllipse(const Rect& r, Color c);
    void StrokeEllipse(const Rect& r, Color c, float width = 1.0f);
    void FillCircle(float cx, float cy, float radius, Color c);
    void StrokeCircle(float cx, float cy, float radius, Color c, float width = 1.0f);
    void Line(float x1, float y1, float x2, float y2, Color c, float width = 1.0f, bool roundCaps = true);
    void Polyline(std::initializer_list<Point> pts, Color c, float width = 1.0f, bool closed = false);
    void Polyline(const std::vector<Point>& pts, Color c, float width = 1.0f, bool closed = false);
    void FillPolygon(std::initializer_list<Point> pts, Color c);
    void FillPolygon(const std::vector<Point>& pts, Color c);
    void Arc(float cx, float cy, float radius, float startDeg, float sweepDeg, Color c, float width = 1.0f);
    void FillPath(const Gdiplus::GraphicsPath& path, Color c);
    void FillPath(const Gdiplus::GraphicsPath& path, const Gdiplus::Brush& brush);
    void StrokePath(const Gdiplus::GraphicsPath& path, Color c, float width = 1.0f);
    static void RoundRectPath(Gdiplus::GraphicsPath& path, const Rect& r, float radius);
    static void RoundRectPath(Gdiplus::GraphicsPath& path, const Rect& r, float tl, float tr, float br, float bl);

    // ---- изображения -------------------------------------------------------
    void Image(Gdiplus::Image* img, const Rect& dst);
    void Image(Gdiplus::Image* img, const Rect& dst, const Rect& src);
    void ImageRounded(Gdiplus::Image* img, const Rect& dst, float radius);

    // ---- эффекты (работают с пикселями буфера, учитывают transform и opacity) ---
    void Shadow(const Rect& r, float radius, float blur, float offsetY, float opacity);
    void Acrylic(const Rect& r, float radius, Color tint, float amount, float blur = 30.0f);
    void Mica(const Rect& r);
    void BeginRounded(const Rect& r, float radius); // запомнить углы под окном
    void EndRounded();                              // сгладить углы (антиалиасинг)
    void Dim(Color c);

    // Обои: полный размер (пиксели устройства) — для фона и Mica.
    void SetWallpaper(std::vector<uint32_t>&& pixels, int width, int height);
    bool HasWallpaper() const { return !m_wallpaper.empty(); }
    void DrawWallpaper();
    void DrawWallpaperThumb(const Rect& dst, float radius);
    void RebuildMica();
    Gdiplus::Bitmap* WallpaperThumb() { return m_wallThumbBmp.get(); }

    // ---- текст --------------------------------------------------------------
    Gdiplus::Font* Font(float size, FontWeight weight = FontWeight::Regular, FontFace face = FontFace::UI);
    void Text(const std::wstring& s, const Rect& r, Color c, float size, FontWeight weight = FontWeight::Regular,
              unsigned flags = 0, FontFace face = FontFace::UI);
    void Text(const wchar_t* s, int len, const Rect& r, Color c, float size, FontWeight weight, unsigned flags,
              FontFace face);
    float TextWidth(const std::wstring& s, float size, FontWeight weight = FontWeight::Regular,
                    FontFace face = FontFace::UI);
    float TextWidth(const wchar_t* s, int len, float size, FontWeight weight, FontFace face);
    float TextHeight(const std::wstring& s, float size, FontWeight weight, float maxWidth,
                     FontFace face = FontFace::UI);
    float LineHeight(float size, FontWeight weight = FontWeight::Regular, FontFace face = FontFace::UI);
    // Индекс символа, ближайшего к x (для позиционирования каретки).
    int HitTestText(const std::wstring& s, float x, float size, FontWeight weight = FontWeight::Regular,
                    FontFace face = FontFace::UI);

    // ---- иконки (Icons.cpp) ------------------------------------------------
    void Glyph(Icon icon, const Rect& r, Color c, float stroke = 1.0f);
    void Glyph(Icon icon, float cx, float cy, float size, Color c, float stroke = 1.0f);
    void DrawAppIcon(AppIcon icon, const Rect& r);
    void DrawAppIcon(AppIcon icon, float cx, float cy, float size);
    void BatteryIcon(const Rect& r, Color c, float level, bool charging);
    void WifiIcon(const Rect& r, Color c, int bars, bool off);

    // Отметка «на кадре нарисована каретка» — чтобы таймер мигания перерисовывал экран.
    void NoteCaret() { m_caretDrawn = true; }
    bool TakeCaretNote()
    {
        const bool r = m_caretDrawn;
        m_caretDrawn = false;
        return r;
    }

private:
    struct Xform
    {
        float s = 1.0f;
        float tx = 0.0f;
        float ty = 0.0f;
    };
    struct CornerSave
    {
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        int n = 0;
        float radius = 0.0f;
        std::vector<uint32_t> px; // 4 квадрата n*n
    };

    void ApplyTransform();
    Gdiplus::StringFormat* Format(unsigned flags);
    void DeviceRect(const Rect& r, int& x0, int& y0, int& x1, int& y1) const;
    void SaveCorners(int x0, int y0, int x1, int y1, float radiusPx);
    void BlitSmall(const uint32_t* small, int sw, int sh, float dstX, float dstY, float cell, int clipX0,
                   int clipY0, int clipX1, int clipY1);
    void GlyphImpl(Icon icon, Color c, float stroke);
    void AppIconImpl(AppIcon icon);

    // Буфер
    HDC m_memDC = nullptr;
    HBITMAP m_dib = nullptr;
    HGDIOBJ m_oldBmp = nullptr;
    uint32_t* m_bits = nullptr;
    int m_pxW = 0;
    int m_pxH = 0;
    float m_scale = 1.0f;
    std::unique_ptr<Gdiplus::Graphics> m_g;

    // Состояние
    Xform m_xf;
    std::vector<Xform> m_xfStack;
    std::vector<Gdiplus::GraphicsState> m_clipStack;
    std::vector<float> m_opacityStack;
    float m_opacity = 1.0f;
    std::vector<CornerSave> m_corners;
    bool m_caretDrawn = false;

    // Ресурсы
    std::map<int, std::unique_ptr<Gdiplus::Font>> m_fonts;
    std::unique_ptr<Gdiplus::StringFormat> m_formats[64];
    std::vector<uint32_t> m_small;  // рабочий буфер размытия
    std::vector<uint32_t> m_small2;

    // Обои и Mica
    std::vector<uint32_t> m_wallpaper;
    int m_wallW = 0;
    int m_wallH = 0;
    std::vector<uint32_t> m_wallThumb;
    std::unique_ptr<Gdiplus::Bitmap> m_wallThumbBmp;
    std::vector<uint32_t> m_mica;
    int m_micaW = 0;
    int m_micaH = 0;
    std::unique_ptr<Gdiplus::Bitmap> m_micaBmp;
    unsigned m_micaVersion = 0;

    static ULONG_PTR s_gdiplusToken;
    static Renderer* s_instance;
};

// Размытие (3 прохода box blur) буфера BGRX на месте.
void BoxBlur(std::vector<uint32_t>& px, std::vector<uint32_t>& tmp, int w, int h, int radius, int passes = 3);
