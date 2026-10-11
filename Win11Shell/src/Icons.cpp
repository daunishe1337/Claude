// Иконки, нарисованные кодом: линии, прямоугольники и контуры GDI+.
// Глифы задаются на сетке 16x16, цветные иконки приложений — на сетке 32x32.

#include "Renderer.h"

namespace
{
inline float F(double v) { return static_cast<float>(v); }

std::vector<Point> Pts(std::initializer_list<double> v)
{
    std::vector<Point> pts;
    pts.reserve(v.size() / 2);
    const double* it = v.begin();
    while (it != v.end())
    {
        const double x = *it++;
        if (it == v.end())
            break;
        const double y = *it++;
        pts.push_back({F(x), F(y)});
    }
    return pts;
}

void Bezier(Gdiplus::GraphicsPath& p, double x1, double y1, double x2, double y2, double x3, double y3, double x4,
            double y4)
{
    p.AddBezier(F(x1), F(y1), F(x2), F(y2), F(x3), F(y3), F(x4), F(y4));
}

void PathLine(Gdiplus::GraphicsPath& p, double x1, double y1, double x2, double y2)
{
    p.AddLine(F(x1), F(y1), F(x2), F(y2));
}

void PathArc(Gdiplus::GraphicsPath& p, double cx, double cy, double r, double start, double sweep)
{
    p.AddArc(F(cx - r), F(cy - r), F(r * 2.0), F(r * 2.0), F(start), F(sweep));
}

// Шестерёнка: зубцы по окружности.
void GearPath(Gdiplus::GraphicsPath& p, float cx, float cy, float rOut, float rIn, int teeth)
{
    std::vector<Gdiplus::PointF> pts;
    const float step = 2.0f * kPi / static_cast<float>(teeth);
    for (int i = 0; i < teeth; ++i)
    {
        const float a = step * static_cast<float>(i) - kPi * 0.5f;
        const float d1 = step * 0.27f, d2 = step * 0.17f;
        const float angs[4] = {a - d1, a - d2, a + d2, a + d1};
        const float rads[4] = {rIn, rOut, rOut, rIn};
        for (int k = 0; k < 4; ++k)
            pts.emplace_back(cx + std::cos(angs[k]) * rads[k], cy + std::sin(angs[k]) * rads[k]);
    }
    p.AddPolygon(pts.data(), static_cast<INT>(pts.size()));
}

float NormDeg(float a)
{
    while (a < 0.0f)
        a += 360.0f;
    while (a >= 360.0f)
        a -= 360.0f;
    return a;
}

// Полумесяц: окружность (cx,cy,r) минус смещённая окружность.
void MoonPath(Gdiplus::GraphicsPath& p, float cx, float cy, float r)
{
    const float c2x = cx + r * 0.55f, c2y = cy - r * 0.45f, r2 = r * 0.82f;
    const float dx = c2x - cx, dy = c2y - cy;
    const float d = std::sqrt(dx * dx + dy * dy);
    const float a = (r * r - r2 * r2 + d * d) / (2.0f * d);
    const float h = std::sqrt(std::max(0.0f, r * r - a * a));
    const float mx = cx + a * dx / d, my = cy + a * dy / d;
    const float i1x = mx - h * dy / d, i1y = my + h * dx / d;
    const float i2x = mx + h * dy / d, i2y = my - h * dx / d;
    const float rad2deg = 180.0f / kPi;
    // Внешняя дуга (не содержит направление на c2).
    const float a1 = NormDeg(std::atan2(i1y - cy, i1x - cx) * rad2deg);
    const float a2 = NormDeg(std::atan2(i2y - cy, i2x - cx) * rad2deg);
    const float beta = NormDeg(std::atan2(dy, dx) * rad2deg);
    float sw = NormDeg(a2 - a1);
    if (NormDeg(beta - a1) < sw)
        sw -= 360.0f;
    p.AddArc(cx - r, cy - r, r * 2.0f, r * 2.0f, a1, sw);
    // Внутренняя дуга второй окружности (содержит направление на центр первой).
    const float g2 = NormDeg(std::atan2(i2y - c2y, i2x - c2x) * rad2deg);
    const float g1 = NormDeg(std::atan2(i1y - c2y, i1x - c2x) * rad2deg);
    const float delta = NormDeg(std::atan2(-dy, -dx) * rad2deg);
    float sw2 = NormDeg(g1 - g2);
    if (NormDeg(delta - g2) >= sw2)
        sw2 -= 360.0f;
    p.AddArc(c2x - r2, c2y - r2, r2 * 2.0f, r2 * 2.0f, g2, sw2);
    p.CloseFigure();
}

// Контур облака (сетка 16), со смещением.
void CloudPath(Gdiplus::GraphicsPath& p, double ox, double oy, double k)
{
    PathArc(p, ox + 4.2 * k, oy + 10.2 * k, 2.3 * k, 90.0, 150.0);
    PathArc(p, ox + 8.0 * k, oy + 7.2 * k, 3.6 * k, 195.0, 150.0);
    PathArc(p, ox + 12.0 * k, oy + 9.8 * k, 2.7 * k, 250.0, 200.0);
    p.CloseFigure();
}
} // namespace

void Renderer::Glyph(Icon icon, const Rect& r, Color c, float stroke)
{
    if (icon == Icon::None || c.a == 0)
        return;
    const float size = std::min(r.w, r.h);
    PushTransform(r.x + (r.w - size) * 0.5f, r.y + (r.h - size) * 0.5f, size / 16.0f);
    GlyphImpl(icon, c, stroke);
    PopTransform();
}

void Renderer::Glyph(Icon icon, float cx, float cy, float size, Color c, float stroke)
{
    Glyph(icon, {cx - size * 0.5f, cy - size * 0.5f, size, size}, c, stroke);
}

void Renderer::DrawAppIcon(AppIcon icon, const Rect& r)
{
    if (icon == AppIcon::None)
        return;
    const float size = std::min(r.w, r.h);
    PushTransform(r.x + (r.w - size) * 0.5f, r.y + (r.h - size) * 0.5f, size / 32.0f);
    AppIconImpl(icon);
    PopTransform();
}

void Renderer::DrawAppIcon(AppIcon icon, float cx, float cy, float size)
{
    DrawAppIcon(icon, {cx - size * 0.5f, cy - size * 0.5f, size, size});
}

void Renderer::BatteryIcon(const Rect& r, Color c, float level, bool charging)
{
    const float size = std::min(r.w, r.h);
    PushTransform(r.x + (r.w - size) * 0.5f, r.y + (r.h - size) * 0.5f, size / 16.0f);
    Gdiplus::GraphicsPath body;
    RoundRectPath(body, {0.8f, 4.3f, 12.6f, 7.4f}, 1.9f);
    StrokePath(body, c, 1.0f);
    FillRoundRect({13.9f, 6.4f, 1.3f, 3.2f}, 0.0f, 0.6f, 0.6f, 0.0f, c);
    const float lv = Saturate(level);
    if (lv > 0.02f)
        FillRoundRect({2.3f, 5.8f, 9.6f * lv, 4.4f}, 0.8f, c);
    if (charging)
    {
        FillPolygon(Pts({8.4, 3.0, 4.6, 8.6, 7.3, 8.6, 6.4, 13.0, 10.4, 7.2, 7.6, 7.2}), c);
    }
    PopTransform();
}

void Renderer::WifiIcon(const Rect& r, Color c, int bars, bool off)
{
    const float size = std::min(r.w, r.h);
    PushTransform(r.x + (r.w - size) * 0.5f, r.y + (r.h - size) * 0.5f, size / 16.0f);
    auto fan = [&](float rad, Color col) {
        Gdiplus::GraphicsPath pp;
        pp.AddPie(8.0f - rad, 13.8f - rad, rad * 2.0f, rad * 2.0f, 225.0f, 90.0f);
        FillPath(pp, col);
    };
    const float radii[4] = {0.0f, 4.4f, 7.4f, 10.4f};
    fan(10.4f, c.MulAlpha(0.32f));
    if (!off)
        fan(radii[Clamp(bars, 0, 3)], c);
    else
    {
        Line(10.5f, 10.0f, 14.5f, 14.0f, c, 1.2f);
        Line(14.5f, 10.0f, 10.5f, 14.0f, c, 1.2f);
    }
    PopTransform();
}

// ---------------------------------------------------------------------------
// Монохромные глифы (сетка 16x16)
// ---------------------------------------------------------------------------
void Renderer::GlyphImpl(Icon icon, Color c, float w)
{
    auto L = [&](double x1, double y1, double x2, double y2) { Line(F(x1), F(y1), F(x2), F(y2), c, w); };
    auto P = [&](std::initializer_list<double> v) { Polyline(Pts(v), c, w, false); };
    auto PC = [&](std::initializer_list<double> v) { Polyline(Pts(v), c, w, true); };
    auto FP = [&](std::initializer_list<double> v) { FillPolygon(Pts(v), c); };
    auto C = [&](double cx, double cy, double rad) { StrokeCircle(F(cx), F(cy), F(rad), c, w); };
    auto FC = [&](double cx, double cy, double rad) { FillCircle(F(cx), F(cy), F(rad), c); };
    auto E = [&](double ex, double ey, double ew, double eh) { StrokeEllipse({F(ex), F(ey), F(ew), F(eh)}, c, w); };
    auto RR = [&](double rx, double ry, double rw, double rh, double rad) {
        Gdiplus::GraphicsPath p;
        RoundRectPath(p, {F(rx), F(ry), F(rw), F(rh)}, F(rad));
        StrokePath(p, c, w);
    };
    auto FRR = [&](double rx, double ry, double rw, double rh, double rad) {
        FillRoundRect({F(rx), F(ry), F(rw), F(rh)}, F(rad), c);
    };
    auto A = [&](double cx, double cy, double rad, double start, double sweep) {
        Arc(F(cx), F(cy), F(rad), F(start), F(sweep), c, w);
    };
    auto SP = [&](const Gdiplus::GraphicsPath& p) { StrokePath(p, c, w); };

    auto speaker = [&]() { PC({2.0, 6.0, 4.8, 6.0, 8.5, 2.8, 8.5, 13.2, 4.8, 10.0, 2.0, 10.0}); };
    auto page = [&]() {
        PC({3.5, 1.5, 9.5, 1.5, 12.5, 4.5, 12.5, 14.5, 3.5, 14.5});
        P({9.5, 1.5, 9.5, 4.5, 12.5, 4.5});
    };
    auto folder = [&]() { PC({1.5, 3.5, 6.0, 3.5, 7.5, 5.0, 14.5, 5.0, 14.5, 12.5, 1.5, 12.5}); };
    auto monitor = [&]() {
        RR(1.5, 2.5, 13.0, 9.0, 1.5);
        L(8.0, 11.5, 8.0, 13.5);
        L(5.0, 13.5, 11.0, 13.5);
    };
    auto magnifier = [&]() {
        C(6.5, 6.5, 4.5);
        L(9.8, 9.8, 13.8, 13.8);
    };
    auto sun = [&](double cx, double cy, double rr, double r1, double r2) {
        C(cx, cy, rr);
        for (int k = 0; k < 8; ++k)
        {
            const double a = k * 3.14159265358979 / 4.0;
            L(cx + std::cos(a) * r1, cy + std::sin(a) * r1, cx + std::cos(a) * r2, cy + std::sin(a) * r2);
        }
    };
    auto globe = [&]() {
        C(8.0, 8.0, 6.5);
        E(5.2, 1.5, 5.6, 13.0);
        L(1.5, 8.0, 14.5, 8.0);
        L(2.6, 4.8, 13.4, 4.8);
        L(2.6, 11.2, 13.4, 11.2);
    };
    auto gear = [&]() {
        Gdiplus::GraphicsPath p;
        GearPath(p, 8.0f, 8.0f, 7.0f, 5.3f, 8);
        SP(p);
        C(8.0, 8.0, 2.3);
    };
    auto cloud = [&](double ox, double oy, double k) {
        Gdiplus::GraphicsPath p;
        CloudPath(p, ox, oy, k);
        SP(p);
    };
    auto moon = [&]() {
        Gdiplus::GraphicsPath p;
        MoonPath(p, 8.0f, 8.4f, 6.0f);
        SP(p);
    };
    auto trash = [&]() {
        L(2.5, 4.0, 13.5, 4.0);
        P({6.0, 4.0, 6.0, 2.5, 10.0, 2.5, 10.0, 4.0});
        P({3.8, 4.0, 4.6, 13.5, 11.4, 13.5, 12.2, 4.0});
        L(6.6, 6.8, 6.8, 11.0);
        L(9.4, 6.8, 9.2, 11.0);
    };
    auto list = [&]() {
        for (double y : {4.0, 8.0, 12.0})
        {
            FC(3.0, y, 0.9);
            L(5.5, y, 13.5, y);
        }
    };

    switch (icon)
    {
    case Icon::None:
        break;
    case Icon::Search:
        magnifier();
        break;
    case Icon::ChevronUp:
        P({3.5, 10.0, 8.0, 5.5, 12.5, 10.0});
        break;
    case Icon::ChevronDown:
        P({3.5, 6.0, 8.0, 10.5, 12.5, 6.0});
        break;
    case Icon::ChevronLeft:
        P({10.0, 3.5, 5.5, 8.0, 10.0, 12.5});
        break;
    case Icon::ChevronRight:
        P({6.0, 3.5, 10.5, 8.0, 6.0, 12.5});
        break;
    case Icon::ArrowLeft:
    case Icon::Back:
        L(2.5, 8.0, 13.5, 8.0);
        P({7.0, 3.5, 2.5, 8.0, 7.0, 12.5});
        break;
    case Icon::ArrowRight:
        L(2.5, 8.0, 13.5, 8.0);
        P({9.0, 3.5, 13.5, 8.0, 9.0, 12.5});
        break;
    case Icon::ArrowUp:
        L(8.0, 13.5, 8.0, 2.5);
        P({3.5, 7.0, 8.0, 2.5, 12.5, 7.0});
        break;
    case Icon::ArrowDown:
        L(8.0, 2.5, 8.0, 13.5);
        P({3.5, 9.0, 8.0, 13.5, 12.5, 9.0});
        break;
    case Icon::Refresh:
    case Icon::Restart:
        A(8.0, 8.5, 5.5, 0.0, 270.0);
        P({6.0, 1.0, 8.3, 3.0, 6.0, 5.0});
        break;
    case Icon::Close:
        L(3.5, 3.5, 12.5, 12.5);
        L(12.5, 3.5, 3.5, 12.5);
        break;
    case Icon::Minimize:
        L(3.0, 8.0, 13.0, 8.0);
        break;
    case Icon::Maximize:
        RR(3.5, 3.5, 9.0, 9.0, 1.5);
        break;
    case Icon::Restore:
        RR(3.5, 5.5, 7.0, 7.0, 1.2);
        P({5.5, 5.5, 5.5, 3.5, 12.5, 3.5, 12.5, 10.5, 10.5, 10.5});
        break;
    case Icon::Add:
        L(8.0, 2.5, 8.0, 13.5);
        L(2.5, 8.0, 13.5, 8.0);
        break;
    case Icon::More:
        FC(3.0, 8.0, 1.1);
        FC(8.0, 8.0, 1.1);
        FC(13.0, 8.0, 1.1);
        break;
    case Icon::MoreVertical:
        FC(8.0, 3.0, 1.1);
        FC(8.0, 8.0, 1.1);
        FC(8.0, 13.0, 1.1);
        break;
    case Icon::Check:
        P({3.0, 8.5, 6.5, 12.0, 13.0, 4.5});
        break;
    case Icon::Hamburger:
        L(2.5, 4.0, 13.5, 4.0);
        L(2.5, 8.0, 13.5, 8.0);
        L(2.5, 12.0, 13.5, 12.0);
        break;
    case Icon::Dot:
        FC(8.0, 8.0, 2.5);
        break;

    case Icon::Settings:
    case Icon::Services:
        gear();
        break;
    case Icon::Edit:
    case Icon::Pencil:
        PC({10.8, 2.2, 13.8, 5.2, 5.5, 13.5, 2.5, 13.5, 2.5, 10.5});
        L(9.2, 3.8, 12.2, 6.8);
        break;
    case Icon::Delete:
    case Icon::RecycleBin:
        trash();
        break;
    case Icon::Cut:
        C(4.5, 11.5, 2.0);
        C(11.5, 11.5, 2.0);
        L(5.9, 10.1, 11.2, 2.5);
        L(10.1, 10.1, 4.8, 2.5);
        break;
    case Icon::Copy:
    case Icon::CopyPath:
        P({2.5, 11.0, 2.5, 3.5, 3.5, 2.5, 10.0, 2.5});
        RR(5.0, 5.0, 8.5, 9.5, 1.5);
        break;
    case Icon::Paste:
        RR(3.0, 3.5, 10.0, 11.0, 1.5);
        RR(5.5, 1.8, 5.0, 3.2, 1.0);
        break;
    case Icon::Rename:
        P({8.0, 4.5, 2.5, 4.5, 1.5, 5.5, 1.5, 10.5, 2.5, 11.5, 8.0, 11.5});
        P({12.0, 4.5, 13.5, 4.5, 14.5, 5.5, 14.5, 10.5, 13.5, 11.5, 12.0, 11.5});
        L(10.0, 2.5, 10.0, 13.5);
        L(8.5, 2.5, 11.5, 2.5);
        L(8.5, 13.5, 11.5, 13.5);
        break;
    case Icon::Share:
    {
        Gdiplus::GraphicsPath p;
        Bezier(p, 3.5, 13.0, 3.5, 8.5, 6.5, 6.0, 13.0, 6.0);
        SP(p);
        P({10.0, 3.0, 13.0, 6.0, 10.0, 9.0});
        break;
    }
    case Icon::Sort:
        L(5.0, 13.5, 5.0, 2.5);
        P({2.5, 5.0, 5.0, 2.5, 7.5, 5.0});
        L(11.0, 2.5, 11.0, 13.5);
        P({8.5, 11.0, 11.0, 13.5, 13.5, 11.0});
        break;
    case Icon::View:
        RR(2.5, 2.5, 11.0, 11.0, 1.5);
        L(2.5, 6.5, 13.5, 6.5);
        L(6.5, 6.5, 6.5, 13.5);
        break;
    case Icon::Filter:
        PC({2.5, 3.5, 13.5, 3.5, 9.5, 8.5, 9.5, 13.0, 6.5, 11.5, 6.5, 8.5});
        break;
    case Icon::Pin:
        PC({5.5, 2.5, 10.5, 2.5, 10.0, 7.0, 12.5, 9.5, 3.5, 9.5, 6.0, 7.0});
        L(8.0, 9.5, 8.0, 14.0);
        break;
    case Icon::Unpin:
        PC({5.5, 2.5, 10.5, 2.5, 10.0, 7.0, 12.5, 9.5, 3.5, 9.5, 6.0, 7.0});
        L(8.0, 9.5, 8.0, 14.0);
        L(2.0, 2.0, 14.0, 14.0);
        break;
    case Icon::Star:
    {
        std::vector<Point> pts;
        for (int i = 0; i < 10; ++i)
        {
            const float a = (-90.0f + 36.0f * static_cast<float>(i)) * kPi / 180.0f;
            const float rr = (i % 2 == 0) ? 6.4f : 2.7f;
            pts.push_back({8.0f + std::cos(a) * rr, 8.6f + std::sin(a) * rr});
        }
        Polyline(pts, c, w, true);
        break;
    }
    case Icon::Undo:
    case Icon::RotateLeft:
    {
        P({5.5, 3.0, 2.5, 6.0, 5.5, 9.0});
        Gdiplus::GraphicsPath p;
        PathLine(p, 2.8, 6.0, 9.5, 6.0);
        Bezier(p, 9.5, 6.0, 12.0, 6.0, 13.5, 7.8, 13.5, 9.5);
        Bezier(p, 13.5, 9.5, 13.5, 11.5, 12.0, 13.0, 9.5, 13.0);
        PathLine(p, 9.5, 13.0, 6.0, 13.0);
        SP(p);
        break;
    }
    case Icon::Redo:
    case Icon::RotateRight:
    {
        P({10.5, 3.0, 13.5, 6.0, 10.5, 9.0});
        Gdiplus::GraphicsPath p;
        PathLine(p, 13.2, 6.0, 6.5, 6.0);
        Bezier(p, 6.5, 6.0, 4.0, 6.0, 2.5, 7.8, 2.5, 9.5);
        Bezier(p, 2.5, 9.5, 2.5, 11.5, 4.0, 13.0, 6.5, 13.0);
        PathLine(p, 6.5, 13.0, 10.0, 13.0);
        SP(p);
        break;
    }
    case Icon::ZoomIn:
        magnifier();
        L(4.5, 6.5, 8.5, 6.5);
        L(6.5, 4.5, 6.5, 8.5);
        break;
    case Icon::ZoomOut:
        magnifier();
        L(4.5, 6.5, 8.5, 6.5);
        break;
    case Icon::Save:
        RR(2.5, 2.5, 11.0, 11.0, 1.5);
        P({5.0, 2.5, 5.0, 5.5, 10.0, 5.5, 10.0, 2.5});
        RR(4.5, 8.5, 7.0, 5.0, 0.8);
        break;
    case Icon::Open:
        P({1.5, 12.5, 1.5, 3.5, 5.5, 3.5, 7.0, 5.0, 12.5, 5.0, 12.5, 7.0});
        PC({1.5, 12.5, 4.0, 7.0, 14.5, 7.0, 12.0, 12.5});
        break;
    case Icon::NewTab:
        RR(1.5, 3.0, 13.0, 10.0, 1.5);
        L(8.0, 5.5, 8.0, 10.5);
        L(5.5, 8.0, 10.5, 8.0);
        break;
    case Icon::Properties:
        RR(2.5, 2.0, 11.0, 12.0, 1.5);
        L(5.0, 5.5, 11.0, 5.5);
        L(5.0, 8.0, 11.0, 8.0);
        L(5.0, 10.5, 9.0, 10.5);
        break;
    case Icon::Print:
    case Icon::Printer:
        P({4.0, 6.0, 4.0, 2.5, 12.0, 2.5, 12.0, 6.0});
        RR(1.5, 6.0, 13.0, 6.0, 1.5);
        RR(4.0, 9.5, 8.0, 4.5, 0.5);
        break;
    case Icon::OpenWith:
    case Icon::Apps:
        RR(2.0, 2.0, 5.0, 5.0, 1.2);
        RR(9.0, 2.0, 5.0, 5.0, 1.2);
        RR(2.0, 9.0, 5.0, 5.0, 1.2);
        RR(9.0, 9.0, 5.0, 5.0, 1.2);
        break;
    case Icon::Select:
    {
        Gdiplus::Pen pen(Gp(c), w);
        pen.SetDashStyle(Gdiplus::DashStyleDash);
        m_g->DrawRectangle(&pen, 2.5f, 2.5f, 11.0f, 11.0f);
        break;
    }
    case Icon::Info:
        C(8.0, 8.0, 6.5);
        L(8.0, 7.0, 8.0, 11.5);
        FC(8.0, 4.8, 0.9);
        break;

    case Icon::Home:
        P({1.8, 7.8, 8.0, 2.3, 14.2, 7.8});
        P({3.5, 6.5, 3.5, 13.5, 12.5, 13.5, 12.5, 6.5});
        P({6.5, 13.5, 6.5, 10.0, 9.5, 10.0, 9.5, 13.5});
        break;
    case Icon::Gallery:
        RR(1.5, 4.0, 10.5, 9.5, 1.5);
        P({4.0, 2.5, 13.0, 2.5, 14.5, 4.0, 14.5, 11.0});
        P({2.0, 12.0, 5.0, 9.0, 7.5, 11.5, 9.0, 10.0, 11.5, 12.5});
        break;
    case Icon::Desktop:
    case Icon::Pc:
    case Icon::Monitor:
    case Icon::Display:
        monitor();
        break;
    case Icon::Downloads:
        L(8.0, 2.0, 8.0, 10.5);
        P({4.5, 7.0, 8.0, 10.5, 11.5, 7.0});
        L(3.0, 13.5, 13.0, 13.5);
        break;
    case Icon::Documents:
        page();
        L(5.5, 8.0, 10.5, 8.0);
        L(5.5, 10.5, 10.5, 10.5);
        L(5.5, 12.5, 8.5, 12.5);
        break;
    case Icon::File:
        page();
        break;
    case Icon::Pictures:
        RR(1.5, 2.5, 13.0, 11.0, 1.5);
        P({2.0, 12.0, 5.5, 8.0, 8.5, 11.0, 10.5, 9.0, 14.0, 12.5});
        C(10.5, 5.8, 1.1);
        break;
    case Icon::Music:
        C(4.5, 12.0, 1.8);
        C(11.5, 10.5, 1.8);
        L(6.3, 12.0, 6.3, 3.5);
        L(13.3, 10.5, 13.3, 2.0);
        L(6.3, 3.5, 13.3, 2.0);
        break;
    case Icon::Videos:
        RR(1.5, 3.5, 9.5, 9.0, 1.5);
        PC({11.0, 6.5, 14.5, 4.5, 14.5, 11.5, 11.0, 9.5});
        break;
    case Icon::Folder:
        folder();
        L(1.5, 6.5, 14.5, 6.5);
        break;
    case Icon::NewFolder:
        PC({1.5, 3.5, 6.0, 3.5, 7.5, 5.0, 14.5, 5.0, 14.5, 8.5});
        P({1.5, 3.5, 1.5, 12.5, 8.5, 12.5});
        L(12.0, 9.0, 12.0, 15.0);
        L(9.0, 12.0, 15.0, 12.0);
        break;
    case Icon::Drive:
    case Icon::Disk:
        RR(1.5, 5.0, 13.0, 6.5, 1.5);
        FC(11.8, 8.25, 0.8);
        L(3.8, 8.25, 7.5, 8.25);
        break;
    case Icon::Storage:
        RR(2.0, 2.5, 12.0, 5.0, 1.2);
        RR(2.0, 8.5, 12.0, 5.0, 1.2);
        FC(11.3, 5.0, 0.6);
        FC(11.3, 11.0, 0.6);
        break;
    case Icon::Network:
    case Icon::Globe:
    case Icon::WorldClock:
    case Icon::Language:
        globe();
        break;

    case Icon::Power:
        A(8.0, 8.5, 5.5, 300.0, 300.0);
        L(8.0, 2.0, 8.0, 8.0);
        break;
    case Icon::Sleep:
    case Icon::Moon:
    case Icon::NightLight:
        moon();
        break;
    case Icon::Lock:
    {
        RR(3.0, 7.0, 10.0, 7.5, 1.5);
        Gdiplus::GraphicsPath p;
        PathLine(p, 5.0, 7.0, 5.0, 5.2);
        p.AddArc(5.0f, 2.2f, 6.0f, 6.0f, 180.0f, 180.0f);
        PathLine(p, 11.0, 5.2, 11.0, 7.0);
        SP(p);
        FC(8.0, 10.6, 0.9);
        break;
    }
    case Icon::SignOut:
        P({9.5, 2.5, 3.5, 2.5, 3.5, 13.5, 9.5, 13.5});
        L(7.0, 8.0, 14.0, 8.0);
        P({11.5, 5.5, 14.0, 8.0, 11.5, 10.5});
        break;
    case Icon::User:
    {
        C(8.0, 5.2, 2.9);
        Gdiplus::GraphicsPath p;
        p.AddArc(2.5f, 9.6f, 11.0f, 9.0f, 180.0f, 180.0f);
        SP(p);
        break;
    }
    case Icon::Bell:
    {
        Gdiplus::GraphicsPath p;
        Bezier(p, 4.0, 11.0, 4.0, 4.0, 5.5, 2.5, 8.0, 2.5);
        Bezier(p, 8.0, 2.5, 10.5, 2.5, 12.0, 4.0, 12.0, 11.0);
        PathLine(p, 12.0, 11.0, 13.5, 12.5);
        PathLine(p, 13.5, 12.5, 2.5, 12.5);
        p.CloseFigure();
        SP(p);
        A(8.0, 12.5, 1.8, 0.0, 180.0);
        break;
    }
    case Icon::Wifi:
        WifiIcon({0.0f, 0.0f, 16.0f, 16.0f}, c, 3, false);
        break;
    case Icon::Wifi2:
        WifiIcon({0.0f, 0.0f, 16.0f, 16.0f}, c, 2, false);
        break;
    case Icon::Wifi1:
        WifiIcon({0.0f, 0.0f, 16.0f, 16.0f}, c, 1, false);
        break;
    case Icon::WifiOff:
        WifiIcon({0.0f, 0.0f, 16.0f, 16.0f}, c, 0, true);
        break;
    case Icon::Ethernet:
        RR(3.0, 2.5, 10.0, 7.0, 1.2);
        L(8.0, 9.5, 8.0, 13.5);
        L(4.0, 13.5, 12.0, 13.5);
        L(5.5, 5.0, 5.5, 7.0);
        L(8.0, 5.0, 8.0, 7.0);
        L(10.5, 5.0, 10.5, 7.0);
        break;
    case Icon::Volume0:
        speaker();
        break;
    case Icon::Volume1:
        speaker();
        A(8.5, 8.0, 2.4, -50.0, 100.0);
        break;
    case Icon::Volume2:
        speaker();
        A(8.5, 8.0, 2.4, -50.0, 100.0);
        A(8.5, 8.0, 4.6, -50.0, 100.0);
        break;
    case Icon::Volume3:
    case Icon::Sound:
        speaker();
        A(8.5, 8.0, 2.4, -50.0, 100.0);
        A(8.5, 8.0, 4.6, -50.0, 100.0);
        A(8.5, 8.0, 6.8, -50.0, 100.0);
        break;
    case Icon::VolumeMute:
        speaker();
        L(11.0, 6.0, 15.0, 10.0);
        L(15.0, 6.0, 11.0, 10.0);
        break;
    case Icon::Battery:
    case Icon::Battery2:
        BatteryIcon({0.0f, 0.0f, 16.0f, 16.0f}, c, 0.8f, false);
        break;
    case Icon::BatteryCharging:
        BatteryIcon({0.0f, 0.0f, 16.0f, 16.0f}, c, 0.8f, true);
        break;
    case Icon::Bluetooth:
        P({4.5, 4.8, 11.5, 11.0, 8.0, 14.0, 8.0, 2.0, 11.5, 5.0, 4.5, 11.2});
        break;
    case Icon::Airplane:
        PC({8.0, 1.5, 9.0, 3.0, 9.0, 6.5, 14.5, 9.5, 14.5, 10.5, 9.0, 9.0, 9.0, 12.0, 11.0, 13.5, 11.0, 14.5, 8.0,
            13.6, 5.0, 14.5, 5.0, 13.5, 7.0, 12.0, 7.0, 9.0, 1.5, 10.5, 1.5, 9.5, 7.0, 6.5, 7.0, 3.0});
        break;
    case Icon::BatterySaver:
    {
        Gdiplus::GraphicsPath p;
        Bezier(p, 3.0, 13.0, 3.0, 6.0, 7.0, 2.5, 13.5, 2.5);
        Bezier(p, 13.5, 2.5, 13.5, 9.0, 10.0, 13.0, 3.0, 13.0);
        p.CloseFigure();
        SP(p);
        L(3.0, 13.0, 9.5, 6.5);
        break;
    }
    case Icon::Accessibility:
        FC(8.0, 3.0, 1.6);
        P({2.5, 5.8, 8.0, 6.8, 13.5, 5.8});
        L(8.0, 6.8, 8.0, 9.8);
        L(8.0, 9.8, 5.2, 14.2);
        L(8.0, 9.8, 10.8, 14.2);
        break;
    case Icon::Brightness:
    case Icon::Sun:
        sun(8.0, 8.0, 3.0, 5.0, 7.0);
        break;
    case Icon::TaskView:
        RR(5.5, 2.0, 8.5, 8.5, 1.5);
        FRR(2.0, 5.5, 8.5, 8.5, 1.5);
        break;
    case Icon::Widgets:
        RR(2.0, 2.0, 5.5, 12.0, 1.5);
        RR(9.5, 2.0, 4.5, 5.5, 1.5);
        RR(9.5, 9.5, 4.5, 4.5, 1.5);
        break;
    case Icon::Terminal:
        RR(1.5, 2.5, 13.0, 11.0, 1.5);
        P({4.5, 6.0, 7.0, 8.0, 4.5, 10.0});
        L(8.5, 10.5, 11.5, 10.5);
        break;
    case Icon::Brush:
    case Icon::Personalize:
    {
        PC({12.5, 1.8, 14.2, 3.5, 8.8, 8.9, 7.1, 7.2});
        Gdiplus::GraphicsPath p;
        Bezier(p, 7.0, 7.3, 4.0, 7.0, 4.6, 11.0, 2.0, 14.0);
        Bezier(p, 2.0, 14.0, 6.0, 13.6, 9.0, 12.0, 8.8, 8.9);
        p.CloseFigure();
        SP(p);
        break;
    }
    case Icon::Palette:
        C(8.0, 8.0, 6.5);
        FC(5.0, 6.0, 1.1);
        FC(8.0, 4.5, 1.1);
        FC(11.0, 6.0, 1.1);
        FC(5.2, 9.8, 1.1);
        C(10.5, 10.5, 1.6);
        break;
    case Icon::Keyboard:
        RR(1.0, 4.0, 14.0, 8.0, 1.5);
        for (double x = 3.5; x < 13.0; x += 2.25)
        {
            FC(x, 6.4, 0.5);
            FC(x, 8.3, 0.5);
        }
        L(5.5, 10.2, 10.5, 10.2);
        break;
    case Icon::Mouse:
        RR(4.5, 1.5, 7.0, 13.0, 3.5);
        L(8.0, 1.5, 8.0, 6.0);
        L(4.5, 6.0, 11.5, 6.0);
        break;
    case Icon::Shield:
    {
        Gdiplus::GraphicsPath p;
        PathLine(p, 8.0, 1.5, 13.5, 3.5);
        PathLine(p, 13.5, 3.5, 13.5, 8.0);
        Bezier(p, 13.5, 8.0, 13.5, 11.5, 11.0, 13.5, 8.0, 14.5);
        Bezier(p, 8.0, 14.5, 5.0, 13.5, 2.5, 11.5, 2.5, 8.0);
        PathLine(p, 2.5, 8.0, 2.5, 3.5);
        p.CloseFigure();
        SP(p);
        break;
    }
    case Icon::Update:
        A(8.0, 8.0, 5.5, 200.0, 140.0);
        P({10.8, 5.6, 13.2, 6.2, 13.6, 3.6});
        A(8.0, 8.0, 5.5, 20.0, 140.0);
        P({5.2, 10.4, 2.8, 9.8, 2.4, 12.4});
        break;
    case Icon::Clock:
        C(8.0, 8.0, 6.5);
        L(8.0, 8.0, 8.0, 4.2);
        L(8.0, 8.0, 10.8, 9.6);
        break;
    case Icon::Calendar:
        RR(2.0, 3.0, 12.0, 11.0, 1.5);
        L(2.0, 6.5, 14.0, 6.5);
        L(5.0, 1.5, 5.0, 4.5);
        L(11.0, 1.5, 11.0, 4.5);
        FC(5.0, 9.0, 0.7);
        FC(8.0, 9.0, 0.7);
        FC(11.0, 9.0, 0.7);
        FC(5.0, 11.5, 0.7);
        FC(8.0, 11.5, 0.7);
        break;
    case Icon::Game:
        RR(1.5, 4.5, 13.0, 8.0, 4.0);
        L(4.0, 8.5, 7.0, 8.5);
        L(5.5, 7.0, 5.5, 10.0);
        FC(10.5, 7.5, 0.8);
        FC(12.0, 9.5, 0.8);
        break;
    case Icon::Cast:
        P({1.5, 5.5, 1.5, 3.5, 2.5, 2.5, 13.5, 2.5, 14.5, 3.5, 14.5, 11.5, 13.5, 12.5, 10.0, 12.5});
        A(1.5, 14.5, 3.0, 270.0, 90.0);
        A(1.5, 14.5, 6.0, 270.0, 90.0);
        FC(1.8, 14.2, 0.9);
        break;
    case Icon::Focus:
        C(8.0, 8.0, 6.0);
        C(8.0, 8.0, 3.0);
        FC(8.0, 8.0, 1.0);
        break;
    case Icon::Location:
    {
        Gdiplus::GraphicsPath p;
        p.AddArc(3.5f, 1.5f, 9.0f, 9.0f, 135.0f, 270.0f);
        PathLine(p, 11.18, 9.18, 8.0, 14.5);
        p.CloseFigure();
        SP(p);
        C(8.0, 6.0, 1.8);
        break;
    }
    case Icon::Camera:
        RR(1.5, 4.5, 13.0, 9.0, 2.0);
        C(8.0, 9.0, 2.5);
        P({5.0, 4.5, 6.0, 2.5, 10.0, 2.5, 11.0, 4.5});
        break;
    case Icon::Mail:
        RR(1.5, 3.5, 13.0, 9.0, 1.5);
        P({2.2, 4.5, 8.0, 9.0, 13.8, 4.5});
        break;
    case Icon::Phone:
        RR(4.0, 1.5, 8.0, 13.0, 1.5);
        L(7.0, 12.5, 9.0, 12.5);
        break;
    case Icon::Multitask:
        RR(1.5, 3.0, 6.0, 10.0, 1.2);
        RR(8.5, 3.0, 6.0, 10.0, 1.2);
        break;

    case Icon::Play:
        FP({5.0, 2.8, 13.2, 8.0, 5.0, 13.2});
        break;
    case Icon::Pause:
        FRR(4.0, 3.0, 2.8, 10.0, 1.0);
        FRR(9.2, 3.0, 2.8, 10.0, 1.0);
        break;
    case Icon::Stop:
        FRR(3.5, 3.5, 9.0, 9.0, 1.5);
        break;
    case Icon::Flag:
        L(3.5, 1.8, 3.5, 14.5);
        PC({3.5, 2.5, 13.0, 2.5, 10.8, 6.0, 13.0, 9.5, 3.5, 9.5});
        break;
    case Icon::Timer:
        C(8.0, 9.0, 5.5);
        L(6.5, 1.5, 9.5, 1.5);
        L(8.0, 1.5, 8.0, 3.5);
        L(8.0, 9.0, 8.0, 6.0);
        break;
    case Icon::Stopwatch:
        C(8.0, 9.0, 5.5);
        L(6.5, 1.5, 9.5, 1.5);
        L(8.0, 1.5, 8.0, 3.5);
        L(8.0, 9.0, 10.0, 6.8);
        L(12.3, 3.8, 13.3, 4.8);
        break;
    case Icon::Alarm:
        C(8.0, 9.0, 5.2);
        L(8.0, 9.0, 8.0, 6.2);
        L(8.0, 9.0, 10.0, 10.0);
        L(2.0, 4.0, 4.0, 2.0);
        L(14.0, 4.0, 12.0, 2.0);
        break;
    case Icon::Eraser:
        PC({6.5, 13.5, 2.0, 9.0, 9.0, 2.0, 14.0, 7.0, 7.5, 13.5});
        L(4.5, 6.5, 9.5, 11.5);
        L(7.5, 13.5, 13.5, 13.5);
        break;
    case Icon::Fill:
        PC({2.5, 7.5, 7.5, 2.5, 12.5, 7.5, 7.5, 12.5});
        L(2.5, 7.5, 12.5, 7.5);
        FC(13.5, 12.0, 1.3);
        break;
    case Icon::Line:
        L(2.5, 13.5, 13.5, 2.5);
        break;
    case Icon::RectShape:
        RR(2.5, 3.5, 11.0, 9.0, 0.5);
        break;
    case Icon::EllipseShape:
        E(2.0, 3.5, 12.0, 9.0);
        break;
    case Icon::TextTool:
        P({3.0, 13.5, 8.0, 2.5, 13.0, 13.5});
        L(5.0, 9.5, 11.0, 9.5);
        break;
    case Icon::Picker:
        P({2.5, 13.5, 2.5, 11.5, 8.5, 5.5});
        L(7.0, 4.0, 12.0, 9.0);
        C(11.8, 4.2, 2.3);
        break;
    case Icon::Crop:
        P({4.0, 1.5, 4.0, 12.0, 14.5, 12.0});
        P({1.5, 4.0, 12.0, 4.0, 12.0, 14.5});
        break;
    case Icon::Heart:
    {
        Gdiplus::GraphicsPath p;
        Bezier(p, 8.0, 13.8, 3.5, 10.5, 1.5, 8.0, 1.8, 5.6);
        Bezier(p, 1.8, 5.6, 2.1, 2.8, 6.0, 1.8, 8.0, 4.6);
        Bezier(p, 8.0, 4.6, 10.0, 1.8, 13.9, 2.8, 14.2, 5.6);
        Bezier(p, 14.2, 5.6, 14.5, 8.0, 12.5, 10.5, 8.0, 13.8);
        p.CloseFigure();
        SP(p);
        break;
    }
    case Icon::Code:
        P({5.0, 4.0, 1.5, 8.0, 5.0, 12.0});
        P({11.0, 4.0, 14.5, 8.0, 11.0, 12.0});
        L(9.5, 2.5, 6.5, 13.5);
        break;
    case Icon::Chart:
    case Icon::Performance:
        P({1.5, 13.0, 5.0, 8.0, 8.0, 10.0, 14.5, 3.5});
        L(1.5, 14.5, 14.5, 14.5);
        break;
    case Icon::Cpu:
        RR(4.0, 4.0, 8.0, 8.0, 1.0);
        RR(6.2, 6.2, 3.6, 3.6, 0.5);
        for (double i : {5.5, 8.0, 10.5})
        {
            L(i, 1.5, i, 4.0);
            L(i, 12.0, i, 14.5);
            L(1.5, i, 4.0, i);
            L(12.0, i, 14.5, i);
        }
        break;
    case Icon::Memory:
        RR(1.5, 4.5, 13.0, 6.0, 1.0);
        for (double x = 3.5; x < 13.0; x += 2.0)
            L(x, 10.5, x, 12.5);
        RR(3.5, 6.0, 2.5, 3.0, 0.3);
        RR(6.75, 6.0, 2.5, 3.0, 0.3);
        RR(10.0, 6.0, 2.5, 3.0, 0.3);
        break;
    case Icon::Gpu:
        RR(1.5, 4.0, 13.0, 8.0, 1.0);
        C(9.5, 8.0, 2.2);
        L(3.5, 12.0, 3.5, 14.0);
        L(6.0, 12.0, 6.0, 14.0);
        break;
    case Icon::Processes:
    case Icon::Details:
    case Icon::List:
        list();
        break;
    case Icon::Startup:
        A(8.0, 10.0, 6.0, 180.0, 180.0);
        L(8.0, 10.0, 11.0, 6.5);
        L(2.0, 10.0, 14.0, 10.0);
        break;
    case Icon::History:
        A(8.0, 8.0, 6.0, 200.0, 320.0);
        P({1.2, 3.6, 2.4, 6.2, 5.0, 5.4});
        L(8.0, 8.0, 8.0, 4.8);
        L(8.0, 8.0, 10.4, 9.4);
        break;
    case Icon::Grid:
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                FRR(2.0 + i * 4.4, 2.0 + j * 4.4, 3.2, 3.2, 0.6);
        break;
    case Icon::Tiles:
        RR(1.5, 2.0, 5.5, 5.5, 1.0);
        RR(1.5, 8.5, 5.5, 5.5, 1.0);
        L(8.5, 3.5, 14.5, 3.5);
        L(8.5, 6.0, 12.5, 6.0);
        L(8.5, 10.0, 14.5, 10.0);
        L(8.5, 12.5, 12.5, 12.5);
        break;
    case Icon::Eye:
    {
        Gdiplus::GraphicsPath p;
        Bezier(p, 1.5, 8.0, 4.0, 3.5, 12.0, 3.5, 14.5, 8.0);
        Bezier(p, 14.5, 8.0, 12.0, 12.5, 4.0, 12.5, 1.5, 8.0);
        p.CloseFigure();
        SP(p);
        C(8.0, 8.0, 2.2);
        break;
    }
    case Icon::Lightbulb:
    {
        Gdiplus::GraphicsPath p;
        p.AddArc(3.2f, 1.5f, 9.6f, 9.6f, 140.0f, 260.0f);
        PathLine(p, 11.68, 9.38, 10.3, 11.5);
        PathLine(p, 10.3, 11.5, 5.7, 11.5);
        p.CloseFigure();
        SP(p);
        L(6.0, 13.2, 10.0, 13.2);
        L(6.8, 14.8, 9.2, 14.8);
        break;
    }
    case Icon::Shuffle:
        P({1.5, 4.0, 4.5, 4.0, 11.0, 12.0, 14.0, 12.0});
        P({1.5, 12.0, 4.5, 12.0, 11.0, 4.0, 14.0, 4.0});
        P({12.0, 2.0, 14.0, 4.0, 12.0, 6.0});
        P({12.0, 10.0, 14.0, 12.0, 12.0, 14.0});
        break;
    case Icon::Repeat:
        RR(2.0, 4.0, 12.0, 8.0, 3.0);
        P({9.0, 2.0, 11.0, 4.0, 9.0, 6.0});
        break;
    case Icon::Previous:
        L(3.5, 3.0, 3.5, 13.0);
        FP({13.0, 3.0, 5.0, 8.0, 13.0, 13.0});
        break;
    case Icon::Next:
        L(12.5, 3.0, 12.5, 13.0);
        FP({3.0, 3.0, 11.0, 8.0, 3.0, 13.0});
        break;
    case Icon::Emoji:
        C(8.0, 8.0, 6.5);
        FC(5.8, 6.5, 0.8);
        FC(10.2, 6.5, 0.8);
        A(8.0, 8.5, 3.2, 20.0, 140.0);
        break;
    case Icon::Cloud:
        cloud(0.0, 0.0, 1.0);
        break;
    case Icon::PartlyCloudy:
        C(5.5, 5.5, 2.4);
        for (int k = 0; k < 5; ++k)
        {
            const double a = 3.14159265358979 * (0.75 + 0.25 * k);
            L(5.5 + std::cos(a) * 3.7, 5.5 + std::sin(a) * 3.7, 5.5 + std::cos(a) * 5.0, 5.5 + std::sin(a) * 5.0);
        }
        cloud(1.5, 2.0, 0.9);
        break;
    case Icon::Rain:
        cloud(0.0, -2.0, 1.0);
        L(5.0, 12.5, 4.0, 14.8);
        L(8.0, 12.5, 7.0, 14.8);
        L(11.0, 12.5, 10.0, 14.8);
        break;
    case Icon::Storm:
        cloud(0.0, -2.0, 1.0);
        P({8.8, 11.0, 7.0, 13.4, 9.0, 13.4, 7.4, 15.6});
        break;
    case Icon::Snow:
        cloud(0.0, -2.0, 1.0);
        FC(5.0, 13.5, 0.75);
        FC(8.0, 14.8, 0.75);
        FC(11.0, 13.5, 0.75);
        break;
    case Icon::Wind:
        P({1.5, 6.5, 11.0, 6.5});
        A(11.0, 4.5, 2.0, 90.0, -240.0);
        P({1.5, 10.0, 12.5, 10.0});
        A(12.5, 12.0, 2.0, 270.0, 240.0);
        break;
    case Icon::Drop:
    {
        Gdiplus::GraphicsPath p;
        Bezier(p, 8.0, 1.8, 6.0, 5.0, 3.5, 7.5, 3.5, 10.2);
        Bezier(p, 3.5, 10.2, 3.5, 12.8, 5.5, 14.5, 8.0, 14.5);
        Bezier(p, 8.0, 14.5, 10.5, 14.5, 12.5, 12.8, 12.5, 10.2);
        Bezier(p, 12.5, 10.2, 12.5, 7.5, 10.0, 5.0, 8.0, 1.8);
        p.CloseFigure();
        SP(p);
        break;
    }
    case Icon::Thermometer:
        P({6.2, 9.8, 6.2, 3.0});
        A(8.0, 3.0, 1.8, 180.0, 180.0);
        P({9.8, 3.0, 9.8, 9.8});
        C(8.0, 12.0, 2.8);
        L(8.0, 5.0, 8.0, 10.5);
        break;
    }
}

// ---------------------------------------------------------------------------
// Цветные иконки приложений, папок и файлов (сетка 32x32)
// ---------------------------------------------------------------------------
void Renderer::AppIconImpl(AppIcon icon)
{
    auto RC = [](double x, double y, double w, double h) { return Rect{F(x), F(y), F(w), F(h)}; };
    auto GradV = [&](const Rect& r, double rad, Color a, Color b) { FillGradientV(r, F(rad), a, b); };
    auto GradD = [&](const Gdiplus::GraphicsPath& path, const Rect& r, Color a, Color b) {
        Gdiplus::LinearGradientBrush br(Gdiplus::PointF(r.x, r.y), Gdiplus::PointF(r.Right(), r.Bottom()), Gp(a), Gp(b));
        FillPath(path, br);
    };
    auto GradPathV = [&](const Gdiplus::GraphicsPath& path, float y0, float y1, Color a, Color b) {
        Gdiplus::LinearGradientBrush br(Gdiplus::PointF(0.0f, y0 - 0.5f), Gdiplus::PointF(0.0f, y1 + 0.5f), Gp(a), Gp(b));
        FillPath(path, br);
    };
    auto FRR = [&](double x, double y, double w, double h, double rad, Color c) { FillRoundRect(RC(x, y, w, h), F(rad), c); };
    auto FC = [&](double cx, double cy, double rad, Color c) { FillCircle(F(cx), F(cy), F(rad), c); };
    auto Ln = [&](double x1, double y1, double x2, double y2, Color c, double w) { Line(F(x1), F(y1), F(x2), F(y2), c, F(w)); };
    auto FP = [&](std::initializer_list<double> v, Color c) { FillPolygon(Pts(v), c); };
    auto PL = [&](std::initializer_list<double> v, Color c, double w) { Polyline(Pts(v), c, F(w), false); };
    auto CircleG = [&](double cx, double cy, double rad, Color a, Color b) {
        Gdiplus::GraphicsPath p;
        p.AddEllipse(F(cx - rad), F(cy - rad), F(rad * 2.0), F(rad * 2.0));
        GradD(p, RC(cx - rad, cy - rad, rad * 2.0, rad * 2.0), a, b);
    };

    const Color white(255, 255, 255);

    // Папка: задняя стенка с вкладкой и передняя панель.
    auto folder = [&](bool explorer) {
        Gdiplus::GraphicsPath back;
        RoundRectPath(back, RC(2, 4, 12, 8), 2.0f);
        RoundRectPath(back, RC(2, 6.5, 28, 20), 2.5f);
        back.SetFillMode(Gdiplus::FillModeWinding);
        GradPathV(back, 4.0f, 26.5f, Color::Hex(0xE8A317), Color::Hex(0xD18B00));
        GradV(RC(2, 10, 28, 18), 2.5, Color::Hex(0xFFD966), Color::Hex(0xFFBF1F));
        FillRect(RC(4, 10.4, 24, 0.8), white.WithAlpha(110));
        if (explorer)
        {
            Gdiplus::GraphicsPath band;
            RoundRectPath(band, RC(2, 21, 28, 7), 0.0f, 0.0f, 2.5f, 2.5f);
            GradPathV(band, 21.0f, 28.0f, Color::Hex(0x3AA0F3), Color::Hex(0x1366D6));
        }
    };
    auto folderBadge = [&](Color bg, Icon glyph) {
        folder(false);
        FRR(15.5, 14.5, 12, 12, 3, bg);
        Glyph(glyph, RC(17.5, 16.5, 8, 8), white, 1.7f);
    };
    // Лист документа с загнутым углом.
    auto page = [&]() {
        Gdiplus::GraphicsPath p;
        p.AddLine(7.0f, 2.0f, 20.0f, 2.0f);
        p.AddLine(20.0f, 2.0f, 26.0f, 8.0f);
        p.AddLine(26.0f, 8.0f, 26.0f, 30.0f);
        p.AddLine(26.0f, 30.0f, 7.0f, 30.0f);
        p.CloseFigure();
        FillPath(p, Color::Hex(0xFFFFFF));
        StrokePath(p, Color::Hex(0xB8BFC7), 1.0f);
        FP({20, 2, 20, 8, 26, 8}, Color::Hex(0xE4E8EC));
        PL({20, 2, 20, 8, 26, 8}, Color::Hex(0xB8BFC7), 1.0);
    };
    auto drive = [&](bool system) {
        GradV(RC(2, 10, 28, 13), 2.5, Color::Hex(0xEEF1F4), Color::Hex(0xBAC2CA));
        StrokeRoundRect(RC(2, 10, 28, 13), 2.5f, Color::Hex(0x8C969F), 1.0f);
        FRR(4, 19.5, 24, 1, 0.5, Color::Hex(0x9AA4AD));
        FRR(21, 15, 5, 1.8, 0.9, Color::Hex(0x34C759));
        if (system)
        {
            FRR(5, 13, 6, 5, 1.2, Color::Hex(0x2B7BD6));
            FRR(6.4, 14.3, 3.2, 2.4, 0.5, Color::Hex(0x9ED0FF));
        }
    };

    switch (icon)
    {
    case AppIcon::None:
        break;
    case AppIcon::Explorer:
        folder(true);
        break;
    case AppIcon::Folder:
        folder(false);
        break;
    case AppIcon::FolderDesktop:
        folderBadge(Color::Hex(0x2F7FE0), Icon::Monitor);
        break;
    case AppIcon::FolderDocuments:
        folderBadge(Color::Hex(0x2B6FCC), Icon::Documents);
        break;
    case AppIcon::FolderDownloads:
        folderBadge(Color::Hex(0x1E9E52), Icon::Downloads);
        break;
    case AppIcon::FolderPictures:
        folderBadge(Color::Hex(0x1A8FD0), Icon::Pictures);
        break;
    case AppIcon::FolderMusic:
        folderBadge(Color::Hex(0xEE6A1F), Icon::Music);
        break;
    case AppIcon::FolderVideos:
        folderBadge(Color::Hex(0x8452E0), Icon::Videos);
        break;
    case AppIcon::Notepad:
        GradV(RC(5, 3, 22, 27), 3, Color::Hex(0x5CB8F7), Color::Hex(0x1E6CD6));
        FRR(8, 7.5, 16, 19.5, 1.5, Color::Hex(0xF5FAFF));
        for (double y : {11.8, 15.3, 18.8, 22.3})
            Ln(10.5, y, 21.5, y, Color::Hex(0x2B7DE0, 190), 1.4);
        for (double x : {10.5, 14.5, 18.5, 22.5})
            FRR(x - 1.0, 1.5, 2.0, 5.0, 1.0, Color::Hex(0x0D4FA8));
        break;
    case AppIcon::Settings:
    {
        Gdiplus::GraphicsPath p;
        GearPath(p, 16.0f, 16.0f, 14.0f, 11.0f, 8);
        p.AddEllipse(11.0f, 11.0f, 10.0f, 10.0f);
        GradD(p, RC(2, 2, 28, 28), Color::Hex(0xA3ADB8), Color::Hex(0x56616D));
        CircleG(16, 16, 4.2, Color::Hex(0x5CC0FF), Color::Hex(0x1C72DC));
        break;
    }
    case AppIcon::Calculator:
        GradV(RC(4, 2, 24, 28), 3.5, Color::Hex(0x5E5E5E), Color::Hex(0x2F2F2F));
        FRR(7, 5, 18, 6.5, 1.2, Color::Hex(0xD3EAFF));
        for (int row = 0; row < 3; ++row)
        {
            for (int col = 0; col < 3; ++col)
            {
                const Color c = col == 2 ? Color::Hex(0x5BB8FF) : Color::Hex(0xC4C4C4);
                FRR(7.0 + col * 6.5, 14.0 + row * 5.0, 5.0, 3.8, 0.9, c);
            }
        }
        break;
    case AppIcon::Terminal:
        GradV(RC(2, 4, 28, 24), 3.5, Color::Hex(0x4A4A4A), Color::Hex(0x1B1B1B));
        FillRoundRect(RC(2, 4, 28, 5), 3.5f, 3.5f, 0.0f, 0.0f, Color::Hex(0x5E5E5E));
        PL({8, 14, 12, 17.5, 8, 21}, white, 2.2);
        Ln(14, 21.5, 21, 21.5, white, 2.2);
        break;
    case AppIcon::TaskManager:
        GradV(RC(3, 3, 26, 26), 4, Color::Hex(0x2BC4B4), Color::Hex(0x0A7D76));
        PL({7, 18, 11, 18, 13.5, 11, 16.5, 23, 19.5, 14, 21.5, 18, 25, 18}, white, 2.0);
        break;
    case AppIcon::Paint:
    {
        Gdiplus::GraphicsPath p;
        p.AddEllipse(3.0f, 5.0f, 26.0f, 22.0f);
        p.AddEllipse(18.0f, 17.0f, 6.0f, 6.0f);
        GradD(p, RC(3, 5, 26, 22), Color::Hex(0xFFFFFF), Color::Hex(0xE2E2E2));
        StrokePath(p, Color::Hex(0xB9B9B9), 0.8f);
        FC(9.5, 12.5, 2.5, Color::Hex(0xE53935));
        FC(15.5, 9.5, 2.5, Color::Hex(0xFDD835));
        FC(21.5, 11.5, 2.5, Color::Hex(0x43A047));
        FC(9.5, 19.5, 2.5, Color::Hex(0x1E88E5));
        Ln(29, 3, 20.5, 14.5, Color::Hex(0x8D5A2B), 2.6);
        FC(20.0, 15.2, 1.7, Color::Hex(0x303030));
        break;
    }
    case AppIcon::Photos:
    {
        GradV(RC(3, 5, 26, 22), 3.5, Color::Hex(0x47AEF7), Color::Hex(0x1A60D0));
        FC(22, 11, 2.6, Color::Hex(0xFFE36E));
        FP({4, 25.5, 12, 15, 17.5, 21, 21.5, 16.5, 28, 25.5}, Color::Hex(0x0E3F8E, 230));
        FP({4, 25.5, 9, 20, 13, 24, 15, 22.5, 18, 25.5}, Color::Hex(0x2D7BE3, 230));
        break;
    }
    case AppIcon::Clock:
        CircleG(16, 16, 14, Color::Hex(0x45A2EE), Color::Hex(0x1B60C6));
        FC(16, 16, 11, white);
        for (int k = 0; k < 12; ++k)
        {
            const double a = k * 3.14159265358979 / 6.0;
            const double r1 = (k % 3 == 0) ? 7.8 : 8.8;
            Ln(16 + std::cos(a) * r1, 16 + std::sin(a) * r1, 16 + std::cos(a) * 9.8, 16 + std::sin(a) * 9.8,
               Color::Hex(0x5A6470), k % 3 == 0 ? 1.4 : 0.8);
        }
        Ln(16, 16, 16, 9, Color::Hex(0x1F1F1F), 2.0);
        Ln(16, 16, 21, 18.5, Color::Hex(0x1F1F1F), 2.0);
        FC(16, 16, 1.5, Color::Hex(0x1B60C6));
        break;
    case AppIcon::Weather:
    {
        CircleG(12, 12, 7.5, Color::Hex(0xFFDA55), Color::Hex(0xFF9F0F));
        Gdiplus::GraphicsPath cl;
        cl.SetFillMode(Gdiplus::FillModeWinding);
        cl.AddEllipse(8.5f, 16.0f, 11.0f, 11.0f);
        cl.AddEllipse(13.5f, 11.0f, 13.0f, 13.0f);
        cl.AddEllipse(21.5f, 17.5f, 9.0f, 9.0f);
        RoundRectPath(cl, RC(12, 20, 15, 7), 3.5f);
        GradPathV(cl, 11.0f, 27.0f, Color::Hex(0xFFFFFF), Color::Hex(0xD5E1EE));
        break;
    }
    case AppIcon::Browser:
        CircleG(16, 16, 14, Color::Hex(0x3BD0F2), Color::Hex(0x1257C2));
        StrokeEllipse(RC(10.3, 2.5, 11.4, 27), white.WithAlpha(150), 1.3f);
        Ln(2.5, 16, 29.5, 16, white.WithAlpha(150), 1.3);
        Ln(4.6, 9.5, 27.4, 9.5, white.WithAlpha(120), 1.1);
        Ln(4.6, 22.5, 27.4, 22.5, white.WithAlpha(120), 1.1);
        break;
    case AppIcon::Mail:
        GradV(RC(2, 7, 28, 19), 2.5, Color::Hex(0x4CAAF8), Color::Hex(0x1C6CD6));
        FP({3, 8.5, 16, 18.5, 29, 8.5}, Color::Hex(0x9AD2FF, 220));
        PL({3.5, 9, 16, 18.5, 28.5, 9}, white.WithAlpha(200), 1.2);
        break;
    case AppIcon::Calendar:
        FRR(3, 4, 26, 25, 3, Color::Hex(0xFFFFFF));
        StrokeRoundRect(RC(3, 4, 26, 25), 3.0f, Color::Hex(0xC9CED4), 1.0f);
        FillRoundRect(RC(3, 4, 26, 7), 3.0f, 3.0f, 0.0f, 0.0f, Color::Hex(0xE5484D));
        FRR(9, 1.8, 2.6, 5.4, 1.3, Color::Hex(0x9E2B2E));
        FRR(20.4, 1.8, 2.6, 5.4, 1.3, Color::Hex(0x9E2B2E));
        for (int j = 0; j < 3; ++j)
            for (int i = 0; i < 4; ++i)
                FRR(6.8 + i * 5.0, 14.0 + j * 4.8, 3.0, 3.0, 0.7,
                    (i == 2 && j == 1) ? Color::Hex(0x2B7BD6) : Color::Hex(0xA3AAB2));
        break;
    case AppIcon::Store:
        GradV(RC(5, 10, 22, 19), 3, Color::Hex(0x56B4F6), Color::Hex(0x226FD6));
        Arc(16.0f, 11.0f, 5.5f, 180.0f, 180.0f, Color::Hex(0x1A4C99), 2.2f);
        FRR(9, 14, 14, 2, 1, white.WithAlpha(90));
        break;
    case AppIcon::Camera:
        FRR(10, 6, 12, 5, 2, Color::Hex(0x4F4F4F));
        GradV(RC(3, 9, 26, 18), 4, Color::Hex(0x6E6E6E), Color::Hex(0x2E2E2E));
        FC(16, 18, 6.6, Color::Hex(0x161616));
        CircleG(16, 18, 5.0, Color::Hex(0x93D4FF), Color::Hex(0x1A55B0));
        FC(14.3, 16.3, 1.3, white.WithAlpha(190));
        break;
    case AppIcon::MediaPlayer:
        CircleG(16, 16, 14, Color::Hex(0xFF9A52), Color::Hex(0xE2401A));
        FP({13, 10, 23, 16, 13, 22}, white);
        break;
    case AppIcon::Maps:
        FP({4, 8, 11, 5, 11, 25, 4, 28}, Color::Hex(0x43B868));
        FP({11, 5, 21, 8, 21, 28, 11, 25}, Color::Hex(0x2E9150));
        FP({21, 8, 28, 5, 28, 25, 21, 28}, Color::Hex(0x43B868));
        FC(18.5, 11.5, 4.2, Color::Hex(0xE53935));
        FP({15, 13.5, 22, 13.5, 18.5, 20}, Color::Hex(0xE53935));
        FC(18.5, 11.5, 1.6, white);
        break;
    case AppIcon::Tips:
        CircleG(16, 13, 9, Color::Hex(0xFFE57A), Color::Hex(0xFFAE00));
        FRR(11.5, 21, 9, 3, 1, Color::Hex(0xA0A0A0));
        FRR(12.5, 24.5, 7, 3, 1.5, Color::Hex(0x787878));
        PL({13, 15, 16, 12, 19, 15}, white.WithAlpha(200), 1.2);
        break;
    case AppIcon::ThisPC:
        FRR(2, 4, 28, 19, 2.5, Color::Hex(0x3A3F45));
        GradV(RC(3.5, 5.5, 25, 16), 1.2, Color::Hex(0x2B86E6), Color::Hex(0x6CC4FF));
        FP({13, 23, 19, 23, 20, 27, 12, 27}, Color::Hex(0x8A9099));
        FRR(9, 26.5, 14, 2.5, 1.2, Color::Hex(0x6E757E));
        break;
    case AppIcon::RecycleBinEmpty:
    case AppIcon::RecycleBinFull:
    {
        if (icon == AppIcon::RecycleBinFull)
        {
            FP({9, 10, 13.5, 3.5, 18, 10}, Color::Hex(0x4A9BEF));
            FP({14.5, 10, 20.5, 4.5, 23.5, 10}, Color::Hex(0xF5C542));
            FP({11, 10, 16, 6, 21, 10}, Color::Hex(0xFFFFFF));
        }
        Gdiplus::GraphicsPath body;
        body.AddLine(6.5f, 9.0f, 25.5f, 9.0f);
        body.AddLine(25.5f, 9.0f, 23.5f, 29.0f);
        body.AddLine(23.5f, 29.0f, 8.5f, 29.0f);
        body.CloseFigure();
        GradPathV(body, 9.0f, 29.0f, Color::Hex(0xF1F6FB, 235), Color::Hex(0xC4D3E3, 235));
        StrokePath(body, Color::Hex(0x8DA2B8), 1.0f);
        for (double x : {12.5, 16.0, 19.5})
            Ln(x, 12.5, x - (x - 16.0) * 0.12, 26.0, Color::Hex(0xA3B4C6), 1.0);
        FRR(5, 6.5, 22, 3, 1.5, Color::Hex(0xAFC0D2));
        break;
    }
    case AppIcon::FileText:
        page();
        for (double y : {13.0, 16.5, 20.0, 23.5})
            Ln(10, y, 23, y, Color::Hex(0x8EB8E5), 1.4);
        break;
    case AppIcon::FileImage:
        page();
        GradV(RC(9.5, 12, 14, 12), 1, Color::Hex(0x5DB5F5), Color::Hex(0x2F80E8));
        FP({9.5, 24, 14, 18, 17, 21, 19.5, 19, 23.5, 24}, Color::Hex(0x3C9D4E));
        FC(20, 15, 1.6, Color::Hex(0xFFE36E));
        break;
    case AppIcon::FileAudio:
        page();
        Glyph(Icon::Music, RC(10, 13, 13, 13), Color::Hex(0xF0742A), 1.7f);
        break;
    case AppIcon::FileVideo:
        page();
        FC(16.5, 19, 5.5, Color::Hex(0x8957E5));
        FP({15, 16.2, 19.5, 19, 15, 21.8}, white);
        break;
    case AppIcon::FileGeneric:
        page();
        break;
    case AppIcon::FileDoc:
        page();
        FRR(9.5, 12, 14, 3, 0.8, Color::Hex(0x2B7BD6));
        for (double y : {18.0, 21.0, 24.0})
            Ln(10, y, 23, y, Color::Hex(0xA0A8B0), 1.2);
        break;
    case AppIcon::FileArchive:
        page();
        for (int i = 0; i < 6; ++i)
            FRR(i % 2 == 0 ? 15.0 : 16.5, 10.0 + i * 2.6, 1.5, 2.0, 0.3, Color::Hex(0xB8901E));
        FRR(14.5, 25, 3.5, 3.5, 0.8, Color::Hex(0xB8901E));
        break;
    case AppIcon::FileApp:
        FRR(4, 6, 24, 20, 2.5, Color::Hex(0xFFFFFF));
        StrokeRoundRect(RC(4, 6, 24, 20), 2.5f, Color::Hex(0xB9C0C8), 1.0f);
        FillRoundRect(RC(4, 6, 24, 5), 2.5f, 2.5f, 0.0f, 0.0f, Color::Hex(0x2B7BD6));
        break;
    case AppIcon::DriveSystem:
        drive(true);
        break;
    case AppIcon::Drive:
        drive(false);
        break;
    case AppIcon::Network:
        CircleG(16, 16, 13, Color::Hex(0x4FC3F7), Color::Hex(0x1976D2));
        StrokeEllipse(RC(11, 3, 10, 26), white.WithAlpha(150), 1.2f);
        Ln(3, 16, 29, 16, white.WithAlpha(150), 1.2);
        FRR(17, 18, 13, 9, 1.5, Color::Hex(0x3A3F45));
        FRR(18.2, 19.2, 10.6, 6.2, 0.8, Color::Hex(0x7FD0FF));
        break;
    case AppIcon::Gallery:
        FRR(9, 4, 20, 16, 2.5, Color::Hex(0xBFD9F5));
        GradV(RC(3, 9, 22, 18), 2.5, Color::Hex(0x47AEF7), Color::Hex(0x1A60D0));
        FC(19, 14, 2.0, Color::Hex(0xFFE36E));
        FP({4, 25.5, 10.5, 17, 15, 22, 18, 19, 24, 25.5}, Color::Hex(0x0E3F8E, 230));
        break;
    case AppIcon::Home:
        GradV(RC(6, 14, 20, 15), 1.5, Color::Hex(0xFFFFFF), Color::Hex(0xDDE6F0));
        StrokeRoundRect(RC(6, 14, 20, 15), 1.5f, Color::Hex(0xA9B6C4), 1.0f);
        FP({16, 3, 30, 15.5, 2, 15.5}, Color::Hex(0x2B7BD6));
        FRR(13.5, 20, 5, 9, 1, Color::Hex(0xF5A623));
        break;
    case AppIcon::User:
    {
        CircleG(16, 16, 15, Color::Hex(0xD9DFE6), Color::Hex(0x9CA7B3));
        FC(16, 12.5, 5.2, white.WithAlpha(245));
        Gdiplus::GraphicsPath p;
        p.AddArc(7.0f, 19.5f, 18.0f, 16.0f, 180.0f, 180.0f);
        p.AddArc(1.0f, 1.0f, 30.0f, 30.0f, 52.0f, 76.0f);
        p.CloseFigure();
        FillPath(p, white.WithAlpha(245));
        break;
    }
    }
}
