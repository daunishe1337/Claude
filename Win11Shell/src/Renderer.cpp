#include "Renderer.h"

ULONG_PTR Renderer::s_gdiplusToken = 0;
Renderer* Renderer::s_instance = nullptr;

namespace
{
inline uint32_t PackRGB(uint32_t r, uint32_t g, uint32_t b) { return 0xFF000000u | (r << 16) | (g << 8) | b; }

inline uint32_t ClampByte(float v)
{
    if (v <= 0.0f)
        return 0u;
    if (v >= 255.0f)
        return 255u;
    return static_cast<uint32_t>(v + 0.5f);
}

// Знаковое расстояние до скруглённого прямоугольника (центр, полуразмеры, радиус).
inline float SdRoundRect(float px, float py, float cx, float cy, float hw, float hh, float r)
{
    const float qx = std::fabs(px - cx) - (hw - r);
    const float qy = std::fabs(py - cy) - (hh - r);
    const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - r;
}

inline uint32_t LerpPixel(uint32_t a, uint32_t b, int k) // k: 0..256 (доля b)
{
    const int ik = 256 - k;
    const uint32_t r = static_cast<uint32_t>((static_cast<int>((a >> 16) & 0xFF) * ik + static_cast<int>((b >> 16) & 0xFF) * k) >> 8);
    const uint32_t g = static_cast<uint32_t>((static_cast<int>((a >> 8) & 0xFF) * ik + static_cast<int>((b >> 8) & 0xFF) * k) >> 8);
    const uint32_t bl = static_cast<uint32_t>((static_cast<int>(a & 0xFF) * ik + static_cast<int>(b & 0xFF) * k) >> 8);
    return PackRGB(r, g, bl);
}

void BoxBlurH(const uint32_t* src, uint32_t* dst, int w, int h, int r)
{
    const int div = 2 * r + 1;
    for (int y = 0; y < h; ++y)
    {
        const uint32_t* s = src + static_cast<size_t>(y) * static_cast<size_t>(w);
        uint32_t* d = dst + static_cast<size_t>(y) * static_cast<size_t>(w);
        int sr = 0, sg = 0, sb = 0;
        for (int i = -r; i <= r; ++i)
        {
            const uint32_t c = s[Clamp(i, 0, w - 1)];
            sr += static_cast<int>((c >> 16) & 0xFF);
            sg += static_cast<int>((c >> 8) & 0xFF);
            sb += static_cast<int>(c & 0xFF);
        }
        for (int x = 0; x < w; ++x)
        {
            d[x] = PackRGB(static_cast<uint32_t>(sr / div), static_cast<uint32_t>(sg / div), static_cast<uint32_t>(sb / div));
            const uint32_t cin = s[std::min(x + r + 1, w - 1)];
            const uint32_t cout = s[std::max(x - r, 0)];
            sr += static_cast<int>((cin >> 16) & 0xFF) - static_cast<int>((cout >> 16) & 0xFF);
            sg += static_cast<int>((cin >> 8) & 0xFF) - static_cast<int>((cout >> 8) & 0xFF);
            sb += static_cast<int>(cin & 0xFF) - static_cast<int>(cout & 0xFF);
        }
    }
}

void BoxBlurV(const uint32_t* src, uint32_t* dst, int w, int h, int r)
{
    const int div = 2 * r + 1;
    const size_t stride = static_cast<size_t>(w);
    for (int x = 0; x < w; ++x)
    {
        int sr = 0, sg = 0, sb = 0;
        for (int i = -r; i <= r; ++i)
        {
            const uint32_t c = src[static_cast<size_t>(Clamp(i, 0, h - 1)) * stride + static_cast<size_t>(x)];
            sr += static_cast<int>((c >> 16) & 0xFF);
            sg += static_cast<int>((c >> 8) & 0xFF);
            sb += static_cast<int>(c & 0xFF);
        }
        for (int y = 0; y < h; ++y)
        {
            dst[static_cast<size_t>(y) * stride + static_cast<size_t>(x)] =
                PackRGB(static_cast<uint32_t>(sr / div), static_cast<uint32_t>(sg / div), static_cast<uint32_t>(sb / div));
            const uint32_t cin = src[static_cast<size_t>(std::min(y + r + 1, h - 1)) * stride + static_cast<size_t>(x)];
            const uint32_t cout = src[static_cast<size_t>(std::max(y - r, 0)) * stride + static_cast<size_t>(x)];
            sr += static_cast<int>((cin >> 16) & 0xFF) - static_cast<int>((cout >> 16) & 0xFF);
            sg += static_cast<int>((cin >> 8) & 0xFF) - static_cast<int>((cout >> 8) & 0xFF);
            sb += static_cast<int>(cin & 0xFF) - static_cast<int>(cout & 0xFF);
        }
    }
}

// Уменьшение изображения усреднением блоков factor x factor.
void Downsample(const uint32_t* src, int srcW, int x0, int y0, int x1, int y1, int factor, std::vector<uint32_t>& out,
                int& outW, int& outH)
{
    outW = (x1 - x0 + factor - 1) / factor;
    outH = (y1 - y0 + factor - 1) / factor;
    out.resize(static_cast<size_t>(outW) * static_cast<size_t>(outH));
    for (int j = 0; j < outH; ++j)
    {
        const int yy0 = y0 + j * factor, yy1 = std::min(yy0 + factor, y1);
        for (int i = 0; i < outW; ++i)
        {
            const int xx0 = x0 + i * factor, xx1 = std::min(xx0 + factor, x1);
            uint32_t sr = 0, sg = 0, sb = 0, n = 0;
            for (int yy = yy0; yy < yy1; ++yy)
            {
                const uint32_t* row = src + static_cast<size_t>(yy) * static_cast<size_t>(srcW);
                for (int xx = xx0; xx < xx1; ++xx)
                {
                    const uint32_t c = row[xx];
                    sr += (c >> 16) & 0xFF;
                    sg += (c >> 8) & 0xFF;
                    sb += c & 0xFF;
                    ++n;
                }
            }
            if (n == 0)
                n = 1;
            out[static_cast<size_t>(j) * static_cast<size_t>(outW) + static_cast<size_t>(i)] = PackRGB(sr / n, sg / n, sb / n);
        }
    }
}
} // namespace

void BoxBlur(std::vector<uint32_t>& px, std::vector<uint32_t>& tmp, int w, int h, int radius, int passes)
{
    if (w <= 0 || h <= 0 || radius <= 0)
        return;
    tmp.resize(px.size());
    for (int i = 0; i < passes; ++i)
    {
        BoxBlurH(px.data(), tmp.data(), w, h, radius);
        BoxBlurV(tmp.data(), px.data(), w, h, radius);
    }
}

// ---------------------------------------------------------------------------
// Жизненный цикл
// ---------------------------------------------------------------------------
bool Renderer::Startup()
{
    Gdiplus::GdiplusStartupInput input;
    return Gdiplus::GdiplusStartup(&s_gdiplusToken, &input, nullptr) == Gdiplus::Ok;
}

void Renderer::Shutdown()
{
    if (s_gdiplusToken)
    {
        Gdiplus::GdiplusShutdown(s_gdiplusToken);
        s_gdiplusToken = 0;
    }
}

Renderer::Renderer() { s_instance = this; }

Renderer::~Renderer()
{
    if (s_instance == this)
        s_instance = nullptr;
    m_fonts.clear();
    for (auto& f : m_formats)
        f.reset();
    m_micaBmp.reset();
    m_wallThumbBmp.reset();
    m_g.reset();
    if (m_memDC)
    {
        SelectObject(m_memDC, m_oldBmp);
        DeleteObject(m_dib);
        DeleteDC(m_memDC);
    }
}

void Renderer::Resize(int pxWidth, int pxHeight, float scale)
{
    pxWidth = std::max(1, pxWidth);
    pxHeight = std::max(1, pxHeight);
    if (m_g && pxWidth == m_pxW && pxHeight == m_pxH && scale == m_scale)
        return;

    m_g.reset();
    if (m_memDC)
    {
        SelectObject(m_memDC, m_oldBmp);
        DeleteObject(m_dib);
        DeleteDC(m_memDC);
        m_memDC = nullptr;
    }

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = pxWidth;
    bi.bmiHeader.biHeight = -pxHeight; // сверху вниз
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    m_memDC = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    void* bits = nullptr;
    m_dib = CreateDIBSection(m_memDC, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    m_bits = static_cast<uint32_t*>(bits);
    m_oldBmp = SelectObject(m_memDC, m_dib);

    m_pxW = pxWidth;
    m_pxH = pxHeight;
    m_scale = scale;

    m_g = std::make_unique<Gdiplus::Graphics>(m_memDC);
    m_g->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    m_g->SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    m_g->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    m_g->SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
    m_g->SetCompositingQuality(Gdiplus::CompositingQualityHighSpeed);

    m_xfStack.clear();
    m_clipStack.clear();
    m_xf = Xform{scale, 0.0f, 0.0f};
    ApplyTransform();
}

void Renderer::BeginFrame()
{
    m_xfStack.clear();
    m_clipStack.clear();
    m_opacityStack.clear();
    m_corners.clear();
    m_opacity = 1.0f;
    m_g->ResetClip();
    m_xf = Xform{m_scale, 0.0f, 0.0f};
    ApplyTransform();
}

void Renderer::EndFrame(HDC target)
{
    Flush();
    BitBlt(target, 0, 0, m_pxW, m_pxH, m_memDC, 0, 0, SRCCOPY);
}

void Renderer::Flush()
{
    if (m_g)
        m_g->Flush(Gdiplus::FlushIntentionSync);
    GdiFlush();
}

// ---------------------------------------------------------------------------
// Состояние
// ---------------------------------------------------------------------------
void Renderer::ApplyTransform()
{
    Gdiplus::Matrix m(m_xf.s, 0.0f, 0.0f, m_xf.s, m_xf.tx, m_xf.ty);
    m_g->SetTransform(&m);
}

void Renderer::PushTransform(float dx, float dy, float scale)
{
    m_xfStack.push_back(m_xf);
    m_xf.tx += dx * m_xf.s;
    m_xf.ty += dy * m_xf.s;
    m_xf.s *= scale;
    ApplyTransform();
}

void Renderer::PopTransform()
{
    if (m_xfStack.empty())
        return;
    m_xf = m_xfStack.back();
    m_xfStack.pop_back();
    ApplyTransform();
}

void Renderer::PushClip(const Rect& r)
{
    m_clipStack.push_back(m_g->Save());
    m_g->IntersectClip(Gdiplus::RectF(r.x, r.y, r.w, r.h));
}

void Renderer::PopClip()
{
    if (m_clipStack.empty())
        return;
    m_g->Restore(m_clipStack.back());
    m_clipStack.pop_back();
}

void Renderer::PushOpacity(float opacity)
{
    m_opacityStack.push_back(m_opacity);
    m_opacity *= Saturate(opacity);
}

void Renderer::PopOpacity()
{
    if (m_opacityStack.empty())
        return;
    m_opacity = m_opacityStack.back();
    m_opacityStack.pop_back();
}

Rect Renderer::ToDevice(const Rect& r) const
{
    return {r.x * m_xf.s + m_xf.tx, r.y * m_xf.s + m_xf.ty, r.w * m_xf.s, r.h * m_xf.s};
}

Point Renderer::ToDevice(Point p) const { return {p.x * m_xf.s + m_xf.tx, p.y * m_xf.s + m_xf.ty}; }

void Renderer::DeviceRect(const Rect& r, int& x0, int& y0, int& x1, int& y1) const
{
    const Rect d = ToDevice(r);
    x0 = RoundToInt(d.x);
    y0 = RoundToInt(d.y);
    x1 = RoundToInt(d.x + d.w);
    y1 = RoundToInt(d.y + d.h);
}

// ---------------------------------------------------------------------------
// Примитивы
// ---------------------------------------------------------------------------
Gdiplus::Color Renderer::Gp(Color c) const
{
    const float a = static_cast<float>(c.a) * m_opacity;
    return Gdiplus::Color(static_cast<BYTE>(Clamp(a + 0.5f, 0.0f, 255.0f)), c.r, c.g, c.b);
}

void Renderer::Clear(Color c) { m_g->Clear(Gdiplus::Color(255, c.r, c.g, c.b)); }

void Renderer::FillRect(const Rect& r, Color c)
{
    if (c.a == 0 || r.Empty())
        return;
    Gdiplus::SolidBrush b(Gp(c));
    m_g->FillRectangle(&b, r.x, r.y, r.w, r.h);
}

void Renderer::StrokeRect(const Rect& r, Color c, float width)
{
    if (c.a == 0)
        return;
    Gdiplus::Pen pen(Gp(c), width);
    const float hw = width * 0.5f;
    m_g->DrawRectangle(&pen, r.x + hw, r.y + hw, r.w - width, r.h - width);
}

void Renderer::RoundRectPath(Gdiplus::GraphicsPath& path, const Rect& r, float radius)
{
    RoundRectPath(path, r, radius, radius, radius, radius);
}

void Renderer::RoundRectPath(Gdiplus::GraphicsPath& path, const Rect& r, float tl, float tr, float br, float bl)
{
    const float maxR = std::min(r.w, r.h) * 0.5f;
    tl = std::min(tl, maxR);
    tr = std::min(tr, maxR);
    br = std::min(br, maxR);
    bl = std::min(bl, maxR);
    if (tl > 0.05f)
        path.AddArc(r.x, r.y, tl * 2.0f, tl * 2.0f, 180.0f, 90.0f);
    else
        path.AddLine(r.x, r.y, r.x, r.y);
    if (tr > 0.05f)
        path.AddArc(r.Right() - tr * 2.0f, r.y, tr * 2.0f, tr * 2.0f, 270.0f, 90.0f);
    else
        path.AddLine(r.Right(), r.y, r.Right(), r.y);
    if (br > 0.05f)
        path.AddArc(r.Right() - br * 2.0f, r.Bottom() - br * 2.0f, br * 2.0f, br * 2.0f, 0.0f, 90.0f);
    else
        path.AddLine(r.Right(), r.Bottom(), r.Right(), r.Bottom());
    if (bl > 0.05f)
        path.AddArc(r.x, r.Bottom() - bl * 2.0f, bl * 2.0f, bl * 2.0f, 90.0f, 90.0f);
    else
        path.AddLine(r.x, r.Bottom(), r.x, r.Bottom());
    path.CloseFigure();
}

void Renderer::FillRoundRect(const Rect& r, float radius, Color c)
{
    if (c.a == 0 || r.Empty())
        return;
    if (radius <= 0.25f)
    {
        FillRect(r, c);
        return;
    }
    Gdiplus::GraphicsPath path;
    RoundRectPath(path, r, radius);
    Gdiplus::SolidBrush b(Gp(c));
    m_g->FillPath(&b, &path);
}

void Renderer::FillRoundRect(const Rect& r, float tl, float tr, float br, float bl, Color c)
{
    if (c.a == 0 || r.Empty())
        return;
    Gdiplus::GraphicsPath path;
    RoundRectPath(path, r, tl, tr, br, bl);
    Gdiplus::SolidBrush b(Gp(c));
    m_g->FillPath(&b, &path);
}

void Renderer::StrokeRoundRect(const Rect& r, float radius, Color c, float width)
{
    if (c.a == 0 || r.Empty())
        return;
    const Rect ir = r.Inflated(-width * 0.5f);
    Gdiplus::GraphicsPath path;
    RoundRectPath(path, ir, std::max(0.0f, radius - width * 0.5f));
    Gdiplus::Pen pen(Gp(c), width);
    m_g->DrawPath(&pen, &path);
}

void Renderer::FillGradientV(const Rect& r, float radius, Color top, Color bottom)
{
    if (r.Empty())
        return;
    Gdiplus::LinearGradientBrush br(Gdiplus::PointF(r.x, r.y - 1.0f), Gdiplus::PointF(r.x, r.Bottom() + 1.0f), Gp(top),
                                    Gp(bottom));
    Gdiplus::GraphicsPath path;
    RoundRectPath(path, r, radius);
    m_g->FillPath(&br, &path);
}

void Renderer::FillGradientH(const Rect& r, float radius, Color left, Color right)
{
    if (r.Empty())
        return;
    Gdiplus::LinearGradientBrush br(Gdiplus::PointF(r.x - 1.0f, r.y), Gdiplus::PointF(r.Right() + 1.0f, r.y), Gp(left),
                                    Gp(right));
    Gdiplus::GraphicsPath path;
    RoundRectPath(path, r, radius);
    m_g->FillPath(&br, &path);
}

void Renderer::FillEllipse(const Rect& r, Color c)
{
    if (c.a == 0)
        return;
    Gdiplus::SolidBrush b(Gp(c));
    m_g->FillEllipse(&b, r.x, r.y, r.w, r.h);
}

void Renderer::StrokeEllipse(const Rect& r, Color c, float width)
{
    if (c.a == 0)
        return;
    Gdiplus::Pen pen(Gp(c), width);
    m_g->DrawEllipse(&pen, r.x, r.y, r.w, r.h);
}

void Renderer::FillCircle(float cx, float cy, float radius, Color c)
{
    FillEllipse({cx - radius, cy - radius, radius * 2.0f, radius * 2.0f}, c);
}

void Renderer::StrokeCircle(float cx, float cy, float radius, Color c, float width)
{
    StrokeEllipse({cx - radius, cy - radius, radius * 2.0f, radius * 2.0f}, c, width);
}

void Renderer::Line(float x1, float y1, float x2, float y2, Color c, float width, bool roundCaps)
{
    if (c.a == 0)
        return;
    Gdiplus::Pen pen(Gp(c), width);
    if (roundCaps)
    {
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
    }
    m_g->DrawLine(&pen, x1, y1, x2, y2);
}

void Renderer::Polyline(std::initializer_list<Point> pts, Color c, float width, bool closed)
{
    Polyline(std::vector<Point>(pts), c, width, closed);
}

void Renderer::Polyline(const std::vector<Point>& pts, Color c, float width, bool closed)
{
    if (c.a == 0 || pts.size() < 2)
        return;
    std::vector<Gdiplus::PointF> gp;
    gp.reserve(pts.size());
    for (const Point& p : pts)
        gp.emplace_back(p.x, p.y);
    Gdiplus::Pen pen(Gp(c), width);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    if (closed)
        m_g->DrawPolygon(&pen, gp.data(), static_cast<INT>(gp.size()));
    else
        m_g->DrawLines(&pen, gp.data(), static_cast<INT>(gp.size()));
}

void Renderer::FillPolygon(std::initializer_list<Point> pts, Color c) { FillPolygon(std::vector<Point>(pts), c); }

void Renderer::FillPolygon(const std::vector<Point>& pts, Color c)
{
    if (c.a == 0 || pts.size() < 3)
        return;
    std::vector<Gdiplus::PointF> gp;
    gp.reserve(pts.size());
    for (const Point& p : pts)
        gp.emplace_back(p.x, p.y);
    Gdiplus::SolidBrush b(Gp(c));
    m_g->FillPolygon(&b, gp.data(), static_cast<INT>(gp.size()));
}

void Renderer::Arc(float cx, float cy, float radius, float startDeg, float sweepDeg, Color c, float width)
{
    if (c.a == 0 || radius <= 0.0f)
        return;
    Gdiplus::Pen pen(Gp(c), width);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    m_g->DrawArc(&pen, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, startDeg, sweepDeg);
}

void Renderer::FillPath(const Gdiplus::GraphicsPath& path, Color c)
{
    if (c.a == 0)
        return;
    Gdiplus::SolidBrush b(Gp(c));
    m_g->FillPath(&b, &path);
}

void Renderer::FillPath(const Gdiplus::GraphicsPath& path, const Gdiplus::Brush& brush) { m_g->FillPath(&brush, &path); }

void Renderer::StrokePath(const Gdiplus::GraphicsPath& path, Color c, float width)
{
    if (c.a == 0)
        return;
    Gdiplus::Pen pen(Gp(c), width);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    m_g->DrawPath(&pen, &path);
}

// ---------------------------------------------------------------------------
// Изображения
// ---------------------------------------------------------------------------
void Renderer::Image(Gdiplus::Image* img, const Rect& dst)
{
    if (!img)
        return;
    Image(img, dst, {0.0f, 0.0f, static_cast<float>(img->GetWidth()), static_cast<float>(img->GetHeight())});
}

void Renderer::Image(Gdiplus::Image* img, const Rect& dst, const Rect& src)
{
    if (!img || dst.Empty())
        return;
    Gdiplus::ImageAttributes attr;
    attr.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
    if (m_opacity < 0.999f)
    {
        Gdiplus::ColorMatrix cm{};
        for (int i = 0; i < 5; ++i)
            cm.m[i][i] = 1.0f;
        cm.m[3][3] = m_opacity;
        attr.SetColorMatrix(&cm);
    }
    m_g->DrawImage(img, Gdiplus::RectF(dst.x, dst.y, dst.w, dst.h), src.x, src.y, src.w, src.h, Gdiplus::UnitPixel,
                   &attr);
}

void Renderer::ImageRounded(Gdiplus::Image* img, const Rect& dst, float radius)
{
    if (!img || dst.Empty())
        return;
    if (radius <= 0.25f || m_opacity < 0.999f)
    {
        PushClip(dst);
        Image(img, dst);
        PopClip();
        return;
    }
    Gdiplus::TextureBrush brush(img, Gdiplus::WrapModeClamp);
    brush.TranslateTransform(dst.x, dst.y);
    brush.ScaleTransform(dst.w / static_cast<float>(img->GetWidth()), dst.h / static_cast<float>(img->GetHeight()));
    Gdiplus::GraphicsPath path;
    RoundRectPath(path, dst, radius);
    m_g->FillPath(&brush, &path);
}

// ---------------------------------------------------------------------------
// Эффекты
// ---------------------------------------------------------------------------
void Renderer::Shadow(const Rect& r, float radius, float blur, float offsetY, float opacity)
{
    const float alphaMul = opacity * m_opacity;
    if (alphaMul <= 0.003f || !m_bits || r.Empty())
        return;
    Flush();
    const float s = m_xf.s;
    const float px0 = r.x * s + m_xf.tx, py0 = r.y * s + m_xf.ty;
    const float px1 = px0 + r.w * s, py1 = py0 + r.h * s;
    const float off = offsetY * s;
    const float sy0 = py0 + off, sy1 = py1 + off;
    const float rad = std::max(0.0f, radius * s);
    const float sigma = std::max(0.6f, blur * s * 0.5f);
    const float ext = sigma * 2.6f;

    const int bx0 = std::max(0, static_cast<int>(std::floor(px0 - ext)));
    const int by0 = std::max(0, static_cast<int>(std::floor(std::min(py0, sy0) - ext)));
    const int bx1 = std::min(m_pxW, static_cast<int>(std::ceil(px1 + ext)));
    const int by1 = std::min(m_pxH, static_cast<int>(std::ceil(sy1 + ext)));
    if (bx0 >= bx1 || by0 >= by1)
        return;

    float lut[257];
    for (int i = 0; i <= 256; ++i)
    {
        const float d = -ext + 2.0f * ext * static_cast<float>(i) / 256.0f;
        lut[i] = 0.5f * std::erfc(d / (sigma * 1.41421356f));
    }
    const float lutScale = 256.0f / (2.0f * ext);
    const float cx = (px0 + px1) * 0.5f, cy = (sy0 + sy1) * 0.5f;
    const float hw = (px1 - px0) * 0.5f, hh = (sy1 - sy0) * 0.5f;
    const float pcy = (py0 + py1) * 0.5f;

    for (int y = by0; y < by1; ++y)
    {
        const float fy = static_cast<float>(y) + 0.5f;
        uint32_t* row = m_bits + static_cast<size_t>(y) * static_cast<size_t>(m_pxW);
        int skipL = bx1, skipR = bx1;
        if (fy > py0 + rad + 1.0f && fy < py1 - rad - 1.0f)
        {
            skipL = static_cast<int>(std::ceil(px0 + 1.0f));
            skipR = static_cast<int>(std::floor(px1 - 1.0f));
        }
        for (int x = bx0; x < bx1; ++x)
        {
            if (x >= skipL && x < skipR)
            {
                x = skipR - 1;
                continue;
            }
            const float fx = static_cast<float>(x) + 0.5f;
            if (SdRoundRect(fx, fy, cx, pcy, hw, (py1 - py0) * 0.5f, rad) < -1.0f)
                continue; // под самой панелью тень не нужна
            const float d = SdRoundRect(fx, fy, cx, cy, hw, hh, rad);
            if (d >= ext)
                continue;
            const float a = d <= -ext ? 1.0f : lut[static_cast<int>((d + ext) * lutScale)];
            const int k = static_cast<int>(a * alphaMul * 256.0f);
            if (k <= 0)
                continue;
            row[x] = LerpPixel(row[x], 0xFF000000u, std::min(k, 256));
        }
    }
}

void Renderer::BlitSmall(const uint32_t* small, int sw, int sh, float dstX, float dstY, float cell, int clipX0,
                         int clipY0, int clipX1, int clipY1)
{
    Gdiplus::Bitmap bmp(sw, sh, sw * 4, PixelFormat32bppRGB, reinterpret_cast<BYTE*>(const_cast<uint32_t*>(small)));
    const Gdiplus::GraphicsState st = m_g->Save();
    m_g->ResetTransform();
    m_g->SetClip(Gdiplus::Rect(clipX0, clipY0, clipX1 - clipX0, clipY1 - clipY0), Gdiplus::CombineModeIntersect);
    m_g->SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
    m_g->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    Gdiplus::ImageAttributes attr;
    attr.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
    if (m_opacity < 0.999f)
    {
        Gdiplus::ColorMatrix cm{};
        for (int i = 0; i < 5; ++i)
            cm.m[i][i] = 1.0f;
        cm.m[3][3] = m_opacity;
        attr.SetColorMatrix(&cm);
    }
    m_g->DrawImage(&bmp, Gdiplus::RectF(dstX, dstY, static_cast<float>(sw) * cell, static_cast<float>(sh) * cell), 0.0f,
                   0.0f, static_cast<float>(sw), static_cast<float>(sh), Gdiplus::UnitPixel, &attr);
    m_g->Restore(st);
}

void Renderer::Acrylic(const Rect& r, float radius, Color tint, float amount, float blur)
{
    if (!m_bits || r.Empty())
        return;
    if (!Theme::Transparency())
    {
        FillRoundRect(r, radius, tint.WithAlpha(255));
        return;
    }
    Flush();
    int x0, y0, x1, y1;
    DeviceRect(r, x0, y0, x1, y1);
    const int cx0 = std::max(0, x0), cy0 = std::max(0, y0);
    const int cx1 = std::min(m_pxW, x1), cy1 = std::min(m_pxH, y1);
    if (cx0 >= cx1 || cy0 >= cy1)
        return;

    const int D = 4;
    const float blurPx = blur * m_xf.s;
    const int k = std::max(1, RoundToInt(blurPx / (static_cast<float>(D) * 2.2f)));
    const int margin = (k * 3 + 1) * D;
    const int ax0 = std::max(0, cx0 - margin), ay0 = std::max(0, cy0 - margin);
    const int ax1 = std::min(m_pxW, cx1 + margin), ay1 = std::min(m_pxH, cy1 + margin);
    int sw = 0, sh = 0;
    Downsample(m_bits, m_pxW, ax0, ay0, ax1, ay1, D, m_small, sw, sh);
    BoxBlur(m_small, m_small2, sw, sh, k, 3);

    // Насыщенность + тонировка (упрощённая модель Acrylic: luminosity + tint).
    const float sat = 1.3f;
    const float t = Saturate(amount);
    const float tr = static_cast<float>(tint.r), tg = static_cast<float>(tint.g), tb = static_cast<float>(tint.b);
    for (uint32_t& c : m_small)
    {
        float rr = static_cast<float>((c >> 16) & 0xFF), gg = static_cast<float>((c >> 8) & 0xFF),
              bb = static_cast<float>(c & 0xFF);
        const float lum = rr * 0.299f + gg * 0.587f + bb * 0.114f;
        rr = lum + (rr - lum) * sat;
        gg = lum + (gg - lum) * sat;
        bb = lum + (bb - lum) * sat;
        rr += (tr - rr) * t;
        gg += (tg - gg) * t;
        bb += (tb - bb) * t;
        c = PackRGB(ClampByte(rr), ClampByte(gg), ClampByte(bb));
    }

    // Скругление углов через маску (сохраняем углы до вывода картинки).
    SaveCorners(x0, y0, x1, y1, radius * m_xf.s);
    BlitSmall(m_small.data(), sw, sh, static_cast<float>(ax0), static_cast<float>(ay0), static_cast<float>(D), cx0, cy0,
              cx1, cy1);
    EndRounded();
}

void Renderer::Mica(const Rect& r)
{
    const Palette& p = Theme::P();
    if (!Theme::Transparency() || m_wallpaper.empty())
    {
        FillRect(r, p.micaTint);
        return;
    }
    if (m_micaVersion != Theme::Version() || !m_micaBmp)
        RebuildMica();
    if (!m_micaBmp)
    {
        FillRect(r, p.micaTint);
        return;
    }
    int x0, y0, x1, y1;
    DeviceRect(r, x0, y0, x1, y1);
    const int cx0 = std::max(0, x0), cy0 = std::max(0, y0);
    const int cx1 = std::min(m_pxW, x1), cy1 = std::min(m_pxH, y1);
    if (cx0 >= cx1 || cy0 >= cy1)
        return;
    const float cellX = static_cast<float>(m_pxW) / static_cast<float>(m_micaW);
    const float cellY = static_cast<float>(m_pxH) / static_cast<float>(m_micaH);
    const int sx0 = std::max(0, static_cast<int>(static_cast<float>(cx0) / cellX) - 1);
    const int sy0 = std::max(0, static_cast<int>(static_cast<float>(cy0) / cellY) - 1);
    const int sx1 = std::min(m_micaW, static_cast<int>(static_cast<float>(cx1) / cellX) + 2);
    const int sy1 = std::min(m_micaH, static_cast<int>(static_cast<float>(cy1) / cellY) + 2);

    const Gdiplus::GraphicsState st = m_g->Save();
    m_g->ResetTransform();
    m_g->SetClip(Gdiplus::Rect(cx0, cy0, cx1 - cx0, cy1 - cy0), Gdiplus::CombineModeIntersect);
    m_g->SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
    Gdiplus::ImageAttributes attr;
    attr.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
    if (m_opacity < 0.999f)
    {
        Gdiplus::ColorMatrix cm{};
        for (int i = 0; i < 5; ++i)
            cm.m[i][i] = 1.0f;
        cm.m[3][3] = m_opacity;
        attr.SetColorMatrix(&cm);
    }
    m_g->DrawImage(m_micaBmp.get(),
                   Gdiplus::RectF(static_cast<float>(sx0) * cellX, static_cast<float>(sy0) * cellY,
                                  static_cast<float>(sx1 - sx0) * cellX, static_cast<float>(sy1 - sy0) * cellY),
                   static_cast<float>(sx0), static_cast<float>(sy0), static_cast<float>(sx1 - sx0),
                   static_cast<float>(sy1 - sy0), Gdiplus::UnitPixel, &attr);
    m_g->Restore(st);
}

void Renderer::BeginRounded(const Rect& r, float radius)
{
    Flush();
    int x0, y0, x1, y1;
    DeviceRect(r, x0, y0, x1, y1);
    SaveCorners(x0, y0, x1, y1, radius * m_xf.s);
}

void Renderer::SaveCorners(int x0, int y0, int x1, int y1, float radiusPx)
{
    CornerSave cs;
    cs.x0 = x0;
    cs.y0 = y0;
    cs.x1 = x1;
    cs.y1 = y1;
    cs.radius = radiusPx;
    cs.n = std::min({static_cast<int>(std::ceil(cs.radius)) + 1, (cs.x1 - cs.x0) / 2, (cs.y1 - cs.y0) / 2});
    if (cs.n > 0 && cs.radius > 0.5f && m_bits)
    {
        const int n = cs.n;
        cs.px.assign(static_cast<size_t>(4 * n * n), 0u);
        for (int q = 0; q < 4; ++q)
        {
            const int ox = (q & 1) ? cs.x1 - n : cs.x0;
            const int oy = (q & 2) ? cs.y1 - n : cs.y0;
            for (int j = 0; j < n; ++j)
            {
                for (int i = 0; i < n; ++i)
                {
                    const int xx = ox + i, yy = oy + j;
                    if (xx >= 0 && yy >= 0 && xx < m_pxW && yy < m_pxH)
                        cs.px[static_cast<size_t>(q * n * n + j * n + i)] =
                            m_bits[static_cast<size_t>(yy) * static_cast<size_t>(m_pxW) + static_cast<size_t>(xx)];
                }
            }
        }
    }
    else
        cs.n = 0;
    m_corners.push_back(std::move(cs));
}

void Renderer::EndRounded()
{
    if (m_corners.empty())
        return;
    Flush();
    CornerSave cs = std::move(m_corners.back());
    m_corners.pop_back();
    if (cs.n <= 0 || !m_bits)
        return;
    const int n = cs.n;
    const float cx = static_cast<float>(cs.x0 + cs.x1) * 0.5f, cy = static_cast<float>(cs.y0 + cs.y1) * 0.5f;
    const float hw = static_cast<float>(cs.x1 - cs.x0) * 0.5f, hh = static_cast<float>(cs.y1 - cs.y0) * 0.5f;
    for (int q = 0; q < 4; ++q)
    {
        const int ox = (q & 1) ? cs.x1 - n : cs.x0;
        const int oy = (q & 2) ? cs.y1 - n : cs.y0;
        for (int j = 0; j < n; ++j)
        {
            for (int i = 0; i < n; ++i)
            {
                const int xx = ox + i, yy = oy + j;
                if (xx < 0 || yy < 0 || xx >= m_pxW || yy >= m_pxH)
                    continue;
                const float d = SdRoundRect(static_cast<float>(xx) + 0.5f, static_cast<float>(yy) + 0.5f, cx, cy, hw, hh,
                                            cs.radius);
                const float cov = Saturate(0.5f - d);
                if (cov >= 0.999f)
                    continue;
                uint32_t& dst = m_bits[static_cast<size_t>(yy) * static_cast<size_t>(m_pxW) + static_cast<size_t>(xx)];
                dst = LerpPixel(cs.px[static_cast<size_t>(q * n * n + j * n + i)], dst, static_cast<int>(cov * 256.0f));
            }
        }
    }
}

void Renderer::Dim(Color c) { FillRect({-1.0f, -1.0f, Width() + 2.0f, Height() + 2.0f}, c); }

// ---------------------------------------------------------------------------
// Обои и Mica
// ---------------------------------------------------------------------------
void Renderer::SetWallpaper(std::vector<uint32_t>&& pixels, int width, int height)
{
    m_wallpaper = std::move(pixels);
    m_wallW = width;
    m_wallH = height;
    // Уменьшенная копия для миниатюр.
    int tw = 0, th = 0;
    m_wallThumbBmp.reset();
    if (!m_wallpaper.empty())
    {
        Downsample(m_wallpaper.data(), m_wallW, 0, 0, m_wallW, m_wallH, 4, m_wallThumb, tw, th);
        m_wallThumbBmp = std::make_unique<Gdiplus::Bitmap>(tw, th, tw * 4, PixelFormat32bppRGB,
                                                           reinterpret_cast<BYTE*>(m_wallThumb.data()));
    }
    RebuildMica();
}

void Renderer::DrawWallpaper()
{
    if (!m_bits)
        return;
    Flush();
    if (m_wallpaper.empty() || m_wallW != m_pxW || m_wallH != m_pxH)
    {
        const uint32_t c = 0xFF0A3D91u;
        std::fill(m_bits, m_bits + static_cast<size_t>(m_pxW) * static_cast<size_t>(m_pxH), c);
        return;
    }
    std::memcpy(m_bits, m_wallpaper.data(), m_wallpaper.size() * sizeof(uint32_t));
}

void Renderer::DrawWallpaperThumb(const Rect& dst, float radius)
{
    if (!m_wallThumbBmp)
    {
        FillRoundRect(dst, radius, Color::Hex(0x0A3D91));
        return;
    }
    ImageRounded(m_wallThumbBmp.get(), dst, radius);
}

void Renderer::RebuildMica()
{
    m_micaVersion = Theme::Version();
    m_micaBmp.reset();
    if (m_wallpaper.empty())
        return;
    const int factor = 8;
    Downsample(m_wallpaper.data(), m_wallW, 0, 0, m_wallW, m_wallH, factor, m_mica, m_micaW, m_micaH);
    BoxBlur(m_mica, m_small2, m_micaW, m_micaH, 7, 3);
    const Palette& p = Theme::P();
    const float tr = static_cast<float>(p.micaTint.r), tg = static_cast<float>(p.micaTint.g),
                tb = static_cast<float>(p.micaTint.b);
    const float k = p.micaChroma;
    const float lk = p.dark ? 0.035f : 0.05f;
    for (uint32_t& c : m_mica)
    {
        const float rr = static_cast<float>((c >> 16) & 0xFF), gg = static_cast<float>((c >> 8) & 0xFF),
                    bb = static_cast<float>(c & 0xFF);
        const float lum = rr * 0.299f + gg * 0.587f + bb * 0.114f;
        const float dl = (lum - 128.0f) * lk;
        c = PackRGB(ClampByte(tr + (rr - lum) * k + dl), ClampByte(tg + (gg - lum) * k + dl),
                    ClampByte(tb + (bb - lum) * k + dl));
    }
    m_micaBmp = std::make_unique<Gdiplus::Bitmap>(m_micaW, m_micaH, m_micaW * 4, PixelFormat32bppRGB,
                                                  reinterpret_cast<BYTE*>(m_mica.data()));
}

// ---------------------------------------------------------------------------
// Текст
// ---------------------------------------------------------------------------
Gdiplus::Font* Renderer::Font(float size, FontWeight weight, FontFace face)
{
    const int key = (static_cast<int>(face) << 24) | (static_cast<int>(weight) << 20) |
                    std::min(0xFFFFF, static_cast<int>(size * 16.0f + 0.5f));
    auto it = m_fonts.find(key);
    if (it != m_fonts.end())
        return it->second.get();

    struct Candidate
    {
        const wchar_t* family;
        INT style;
    };
    Candidate cands[4] = {};
    int count = 0;
    auto push = [&](const wchar_t* family, INT style) {
        if (count < 4)
            cands[count++] = Candidate{family, style};
    };
    const INT boldIfHeavy = weight >= FontWeight::Semibold ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular;
    if (face == FontFace::Mono)
    {
        push(L"Consolas", boldIfHeavy);
        push(L"Courier New", boldIfHeavy);
    }
    else if (face == FontFace::Terminal)
    {
        push(L"Cascadia Mono", boldIfHeavy);
        push(L"Consolas", boldIfHeavy);
        push(L"Courier New", boldIfHeavy);
    }
    else
    {
        switch (weight)
        {
        case FontWeight::Light:
            push(L"Segoe UI Light", Gdiplus::FontStyleRegular);
            push(L"Segoe UI", Gdiplus::FontStyleRegular);
            break;
        case FontWeight::Semilight:
            push(L"Segoe UI Semilight", Gdiplus::FontStyleRegular);
            push(L"Segoe UI", Gdiplus::FontStyleRegular);
            break;
        case FontWeight::Regular:
            push(L"Segoe UI", Gdiplus::FontStyleRegular);
            break;
        case FontWeight::Semibold:
            push(L"Segoe UI Semibold", Gdiplus::FontStyleRegular);
            push(L"Segoe UI", Gdiplus::FontStyleBold);
            break;
        case FontWeight::Bold:
            push(L"Segoe UI", Gdiplus::FontStyleBold);
            break;
        }
        push(L"Tahoma", boldIfHeavy);
    }

    std::unique_ptr<Gdiplus::Font> font;
    for (int ci = 0; ci < count; ++ci)
    {
        const Candidate& c = cands[ci];
        Gdiplus::FontFamily fam(c.family);
        if (fam.GetLastStatus() != Gdiplus::Ok || !fam.IsStyleAvailable(c.style))
            continue;
        font = std::make_unique<Gdiplus::Font>(&fam, size, c.style, Gdiplus::UnitPixel);
        if (font->GetLastStatus() == Gdiplus::Ok)
            break;
        font.reset();
    }
    if (!font)
        font = std::make_unique<Gdiplus::Font>(Gdiplus::FontFamily::GenericSansSerif(), size, Gdiplus::FontStyleRegular,
                                               Gdiplus::UnitPixel);
    Gdiplus::Font* raw = font.get();
    m_fonts[key] = std::move(font);
    return raw;
}

Gdiplus::StringFormat* Renderer::Format(unsigned flags)
{
    const unsigned idx = flags & 63u;
    std::unique_ptr<Gdiplus::StringFormat>& slot = m_formats[idx];
    if (!slot)
    {
        slot.reset(Gdiplus::StringFormat::GenericTypographic()->Clone());
        INT ff = slot->GetFormatFlags();
        ff |= Gdiplus::StringFormatFlagsMeasureTrailingSpaces;
        if (!(flags & TextFlags::Wrap))
            ff |= Gdiplus::StringFormatFlagsNoWrap;
        slot->SetFormatFlags(ff);
        if (flags & TextFlags::Center)
            slot->SetAlignment(Gdiplus::StringAlignmentCenter);
        else if (flags & TextFlags::Right)
            slot->SetAlignment(Gdiplus::StringAlignmentFar);
        else
            slot->SetAlignment(Gdiplus::StringAlignmentNear);
        if (flags & TextFlags::VCenter)
            slot->SetLineAlignment(Gdiplus::StringAlignmentCenter);
        else if (flags & TextFlags::Bottom)
            slot->SetLineAlignment(Gdiplus::StringAlignmentFar);
        else
            slot->SetLineAlignment(Gdiplus::StringAlignmentNear);
        slot->SetTrimming((flags & TextFlags::NoEllipsis) ? Gdiplus::StringTrimmingNone
                                                          : Gdiplus::StringTrimmingEllipsisCharacter);
    }
    return slot.get();
}

void Renderer::Text(const std::wstring& s, const Rect& r, Color c, float size, FontWeight weight, unsigned flags,
                    FontFace face)
{
    Text(s.c_str(), static_cast<int>(s.size()), r, c, size, weight, flags, face);
}

void Renderer::Text(const wchar_t* s, int len, const Rect& r, Color c, float size, FontWeight weight, unsigned flags,
                    FontFace face)
{
    if (!s || len <= 0 || c.a == 0 || r.w <= 0.0f)
        return;
    Gdiplus::SolidBrush brush(Gp(c));
    m_g->DrawString(s, len, Font(size, weight, face), Gdiplus::RectF(r.x, r.y, r.w, r.h), Format(flags), &brush);
}

float Renderer::TextWidth(const std::wstring& s, float size, FontWeight weight, FontFace face)
{
    return TextWidth(s.c_str(), static_cast<int>(s.size()), size, weight, face);
}

float Renderer::TextWidth(const wchar_t* s, int len, float size, FontWeight weight, FontFace face)
{
    if (!s || len <= 0 || !m_g)
        return 0.0f;
    Gdiplus::RectF out;
    m_g->MeasureString(s, len, Font(size, weight, face), Gdiplus::PointF(0.0f, 0.0f), Format(TextFlags::NoEllipsis),
                       &out);
    return out.Width;
}

float Renderer::TextHeight(const std::wstring& s, float size, FontWeight weight, float maxWidth, FontFace face)
{
    if (s.empty() || !m_g)
        return 0.0f;
    Gdiplus::RectF out;
    m_g->MeasureString(s.c_str(), static_cast<INT>(s.size()), Font(size, weight, face),
                       Gdiplus::RectF(0.0f, 0.0f, maxWidth, 100000.0f), Format(TextFlags::Wrap | TextFlags::NoEllipsis),
                       &out);
    return out.Height;
}

float Renderer::LineHeight(float size, FontWeight weight, FontFace face)
{
    Gdiplus::Font* f = Font(size, weight, face);
    Gdiplus::FontFamily fam;
    f->GetFamily(&fam);
    const INT style = f->GetStyle();
    const float em = static_cast<float>(fam.GetEmHeight(style));
    if (em <= 0.0f)
        return size * 1.33f;
    return size * static_cast<float>(fam.GetLineSpacing(style)) / em;
}

int Renderer::HitTestText(const std::wstring& s, float x, float size, FontWeight weight, FontFace face)
{
    const int n = static_cast<int>(s.size());
    if (n == 0 || x <= 0.0f)
        return 0;
    if (x >= TextWidth(s, size, weight, face))
        return n;
    // Двоичный поиск по ширине префикса.
    int lo = 0, hi = n;
    while (hi - lo > 1)
    {
        const int mid = (lo + hi) / 2;
        if (TextWidth(s.c_str(), mid, size, weight, face) <= x)
            lo = mid;
        else
            hi = mid;
    }
    const float wl = TextWidth(s.c_str(), lo, size, weight, face);
    const float wh = TextWidth(s.c_str(), hi, size, weight, face);
    return (x - wl) < (wh - x) ? lo : hi;
}
