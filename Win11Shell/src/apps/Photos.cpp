// Приложение «Фотографии»: галерея и просмотр изображений, плюс генератор пейзажей.

#include "Photos.h"

#include <random>

#include "../Shell.h"
#include "AppFactories.h"

// ===========================================================================
// Генератор изображений
// ===========================================================================
namespace
{
std::map<int, std::shared_ptr<Gdiplus::Bitmap>>& Cache()
{
    static std::map<int, std::shared_ptr<Gdiplus::Bitmap>> cache;
    return cache;
}

Gdiplus::Color C(uint32_t rgb, BYTE a = 255)
{
    return Gdiplus::Color(a, static_cast<BYTE>((rgb >> 16) & 0xFF), static_cast<BYTE>((rgb >> 8) & 0xFF),
                          static_cast<BYTE>(rgb & 0xFF));
}

void Sky(Gdiplus::Graphics& g, float w, float h, uint32_t top, uint32_t bottom)
{
    Gdiplus::LinearGradientBrush br(Gdiplus::PointF(0.0f, -1.0f), Gdiplus::PointF(0.0f, h + 1.0f), C(top), C(bottom));
    g.FillRectangle(&br, 0.0f, 0.0f, w, h);
}

void Glow(Gdiplus::Graphics& g, float cx, float cy, float r, uint32_t color, int layers, BYTE alpha)
{
    for (int i = layers; i >= 1; --i)
    {
        const float rr = r * (1.0f + static_cast<float>(i) * 0.6f);
        Gdiplus::SolidBrush b(C(color, static_cast<BYTE>(alpha / static_cast<BYTE>(i + 1))));
        g.FillEllipse(&b, cx - rr, cy - rr, rr * 2.0f, rr * 2.0f);
    }
    Gdiplus::SolidBrush core(C(color));
    g.FillEllipse(&core, cx - r, cy - r, r * 2.0f, r * 2.0f);
}

// Силуэт гор/холмов: сумма синусоид со случайными фазами.
void Ridge(Gdiplus::Graphics& g, std::mt19937& rng, float w, float h, float base, float amp, float freq, uint32_t top,
           uint32_t bottom, bool sharp = false)
{
    std::uniform_real_distribution<float> ph(0.0f, 6.28f);
    const float p1 = ph(rng), p2 = ph(rng), p3 = ph(rng);
    std::vector<Gdiplus::PointF> pts;
    pts.emplace_back(0.0f, h);
    for (int i = 0; i <= 64; ++i)
    {
        const float x = w * static_cast<float>(i) / 64.0f;
        const float u = x / w * freq;
        float y = std::sin(u * 6.28f + p1) * 0.55f + std::sin(u * 13.0f + p2) * 0.3f + std::sin(u * 29.0f + p3) * 0.15f;
        if (sharp)
            y = 1.0f - std::fabs(y) * 1.6f;
        pts.emplace_back(x, base - y * amp);
    }
    pts.emplace_back(w, h);
    Gdiplus::LinearGradientBrush br(Gdiplus::PointF(0.0f, base - amp - 1.0f), Gdiplus::PointF(0.0f, h + 1.0f), C(top), C(bottom));
    g.FillPolygon(&br, pts.data(), static_cast<INT>(pts.size()));
}

void Pine(Gdiplus::Graphics& g, float x, float baseY, float hgt, uint32_t color)
{
    Gdiplus::SolidBrush b(C(color));
    for (int k = 0; k < 3; ++k)
    {
        const float kf = static_cast<float>(k);
        const float top = baseY - hgt + kf * hgt * 0.22f;
        const float wid = hgt * (0.22f + kf * 0.08f);
        Gdiplus::PointF tri[3] = {{x, top}, {x - wid, top + hgt * 0.42f}, {x + wid, top + hgt * 0.42f}};
        g.FillPolygon(&b, tri, 3);
    }
    Gdiplus::SolidBrush trunk(C(0x3B2A1E));
    g.FillRectangle(&trunk, x - hgt * 0.03f, baseY - hgt * 0.1f, hgt * 0.06f, hgt * 0.1f);
}

void Cloud(Gdiplus::Graphics& g, float x, float y, float s, BYTE a)
{
    Gdiplus::SolidBrush b(C(0xFFFFFF, a));
    g.FillEllipse(&b, x, y, s * 1.6f, s * 0.7f);
    g.FillEllipse(&b, x + s * 0.4f, y - s * 0.35f, s * 1.0f, s * 0.9f);
    g.FillEllipse(&b, x + s * 1.0f, y - s * 0.15f, s * 1.2f, s * 0.8f);
}

void Water(Gdiplus::Graphics& g, std::mt19937& rng, float w, float y0, float h, uint32_t top, uint32_t bottom)
{
    Gdiplus::LinearGradientBrush br(Gdiplus::PointF(0.0f, y0 - 1.0f), Gdiplus::PointF(0.0f, h + 1.0f), C(top), C(bottom));
    g.FillRectangle(&br, 0.0f, y0, w, h - y0);
    std::uniform_real_distribution<float> rx(0.0f, w), ry(y0 + 4.0f, h), rl(10.0f, 60.0f);
    Gdiplus::Pen pen(C(0xFFFFFF, 50), 1.5f);
    for (int i = 0; i < 40; ++i)
    {
        const float x = rx(rng), y = ry(rng), l = rl(rng) * (y - y0) / (h - y0) + 6.0f;
        g.DrawLine(&pen, x, y, x + l, y);
    }
}
} // namespace

std::shared_ptr<Gdiplus::Bitmap> PhotoBitmap(int seed)
{
    auto& cache = Cache();
    auto it = cache.find(seed);
    if (it != cache.end())
        return it->second;
    const int W = 720, H = 480;
    const float w = static_cast<float>(W), h = static_cast<float>(H);
    auto bmp = std::make_shared<Gdiplus::Bitmap>(W, H, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(bmp.get());
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        std::mt19937 rng(static_cast<unsigned>(seed * 7919 + 13));
        std::uniform_real_distribution<float> uni(0.0f, 1.0f);
        const int scene = ((seed - 1) % 8 + 8) % 8;
        switch (scene)
        {
        case 0: // горы
            Sky(g, w, h, 0x6FAEEB, 0xDCEEFB);
            Glow(g, w * 0.78f, h * 0.2f, 26.0f, 0xFFF4CF, 3, 90);
            Cloud(g, w * 0.1f, h * 0.18f, 60.0f, 200);
            Ridge(g, rng, w, h, h * 0.52f, h * 0.16f, 1.6f, 0x9BB4D1, 0x7F98B8, true);
            Ridge(g, rng, w, h, h * 0.62f, h * 0.18f, 2.3f, 0x5C7DA3, 0x3F5E82, true);
            Ridge(g, rng, w, h, h * 0.78f, h * 0.06f, 1.2f, 0x4C7A4F, 0x2F5A36);
            Ridge(g, rng, w, h, h * 0.9f, h * 0.04f, 2.0f, 0x3C6A3F, 0x24452A);
            break;
        case 1: // озеро
            Sky(g, w, h, 0x8CC3F2, 0xEAF5FF);
            Cloud(g, w * 0.55f, h * 0.15f, 70.0f, 210);
            Ridge(g, rng, w, h, h * 0.5f, h * 0.08f, 1.5f, 0x6D8FA8, 0x56778F);
            Ridge(g, rng, w, h, h * 0.56f, h * 0.06f, 3.0f, 0x3F6E4D, 0x2E5639);
            Water(g, rng, w, h * 0.6f, h, 0x6AA6D6, 0x2A5F94);
            for (int i = 0; i < 14; ++i)
                Pine(g, w * (0.02f + 0.07f * static_cast<float>(i)) + uni(rng) * 10.0f, h * 0.62f, 40.0f + uni(rng) * 40.0f, 0x234A30);
            break;
        case 2: // закат
        {
            Gdiplus::LinearGradientBrush sky(Gdiplus::PointF(0.0f, 0.0f), Gdiplus::PointF(0.0f, h * 0.62f), C(0x2A1A50), C(0xFFB86E));
            Gdiplus::Color colors[3] = {C(0x2A1A50), C(0xE2626A), C(0xFFC47E)};
            Gdiplus::REAL pos[3] = {0.0f, 0.6f, 1.0f};
            sky.SetInterpolationColors(colors, pos, 3);
            g.FillRectangle(&sky, 0.0f, 0.0f, w, h * 0.62f + 1.0f);
            Glow(g, w * 0.5f, h * 0.58f, 34.0f, 0xFFE7B0, 4, 110);
            Water(g, rng, w, h * 0.62f, h, 0x6E3F63, 0x1E1533);
            Gdiplus::SolidBrush streak(C(0xFFD49A, 120));
            for (int i = 0; i < 18; ++i)
            {
                const float y = h * 0.64f + static_cast<float>(i) * 9.0f;
                const float hw = 50.0f - static_cast<float>(i) * 2.0f + uni(rng) * 12.0f;
                g.FillRectangle(&streak, w * 0.5f - hw, y, hw * 2.0f, 2.5f);
            }
            Ridge(g, rng, w * 0.4f, h * 0.66f, h * 0.62f, h * 0.05f, 1.0f, 0x24183A, 0x1A1029);
            break;
        }
        case 3: // лес
            Sky(g, w, h, 0xB5DDF4, 0xF3FBFF);
            Glow(g, w * 0.2f, h * 0.16f, 22.0f, 0xFFFBE6, 3, 80);
            for (int layer = 0; layer < 3; ++layer)
            {
                const uint32_t cols[3] = {0x5E8F72, 0x2F6B45, 0x1C4A2D};
                const float base = h * (0.62f + 0.14f * static_cast<float>(layer));
                const float size = 70.0f + 50.0f * static_cast<float>(layer);
                for (float x = -20.0f; x < w + 40.0f; x += size * 0.45f)
                    Pine(g, x + uni(rng) * 20.0f, base + uni(rng) * 10.0f, size * (0.8f + uni(rng) * 0.4f), cols[layer]);
            }
            {
                Gdiplus::SolidBrush ground(C(0x1F3F25));
                g.FillRectangle(&ground, 0.0f, h * 0.93f, w, h * 0.07f);
            }
            break;
        case 4: // город ночью
        {
            Sky(g, w, h, 0x070F2A, 0x23366A);
            Glow(g, w * 0.82f, h * 0.18f, 18.0f, 0xF6F2DA, 3, 60);
            Gdiplus::SolidBrush star(C(0xFFFFFF, 180));
            for (int i = 0; i < 60; ++i)
                g.FillEllipse(&star, uni(rng) * w, uni(rng) * h * 0.45f, 1.6f, 1.6f);
            Gdiplus::SolidBrush bld(C(0x10182E));
            Gdiplus::SolidBrush win(C(0xFFD27A, 210));
            float x = 0.0f;
            while (x < w)
            {
                const float bw = 30.0f + uni(rng) * 50.0f, bh = h * (0.18f + uni(rng) * 0.38f);
                const float top = h * 0.72f - bh;
                g.FillRectangle(&bld, x, top, bw, bh + 2.0f);
                for (float wy = top + 8.0f; wy < h * 0.7f; wy += 12.0f)
                    for (float wx = x + 5.0f; wx < x + bw - 6.0f; wx += 9.0f)
                        if (uni(rng) < 0.45f)
                            g.FillRectangle(&win, wx, wy, 4.0f, 6.0f);
                x += bw + 2.0f;
            }
            Water(g, rng, w, h * 0.72f, h, 0x1A2A55, 0x070D20);
            Gdiplus::SolidBrush refl(C(0xFFD27A, 60));
            for (int i = 0; i < 80; ++i)
                g.FillRectangle(&refl, uni(rng) * w, h * 0.73f + uni(rng) * h * 0.25f, 3.0f + uni(rng) * 10.0f, 1.5f);
            break;
        }
        case 5: // море
            Sky(g, w, h, 0x4FA9EE, 0xCDEBFF);
            Cloud(g, w * 0.15f, h * 0.2f, 70.0f, 230);
            Cloud(g, w * 0.62f, h * 0.12f, 50.0f, 200);
            Water(g, rng, w, h * 0.5f, h, 0x2B8BD0, 0x0B4C87);
            {
                Gdiplus::GraphicsPath beach;
                beach.AddBezier(0.0f, h * 0.8f, w * 0.3f, h * 0.72f, w * 0.6f, h * 0.9f, w, h * 0.82f);
                beach.AddLine(w, h * 0.82f, w, h);
                beach.AddLine(w, h, 0.0f, h);
                beach.CloseFigure();
                Gdiplus::LinearGradientBrush sand(Gdiplus::PointF(0.0f, h * 0.72f), Gdiplus::PointF(0.0f, h), C(0xF6E2AE), C(0xE2C27E));
                g.FillPath(&sand, &beach);
                Gdiplus::Pen foam(C(0xFFFFFF, 200), 3.0f);
                g.DrawBezier(&foam, 0.0f, h * 0.8f, w * 0.3f, h * 0.72f, w * 0.6f, h * 0.9f, w, h * 0.82f);
            }
            break;
        case 6: // поле
            Sky(g, w, h, 0x7DC0F5, 0xE6F5FF);
            Cloud(g, w * 0.3f, h * 0.16f, 64.0f, 220);
            Ridge(g, rng, w, h, h * 0.6f, h * 0.05f, 1.1f, 0x8DBD55, 0x6FA03D);
            Ridge(g, rng, w, h, h * 0.75f, h * 0.06f, 0.8f, 0xC9B44A, 0xA48A28);
            {
                Gdiplus::Pen row(C(0x8A7020, 90), 2.0f);
                for (int i = 0; i < 16; ++i)
                {
                    const float fx = w * static_cast<float>(i) / 15.0f;
                    g.DrawLine(&row, w * 0.5f + (fx - w * 0.5f) * 0.2f, h * 0.76f, fx * 1.4f - w * 0.2f, h);
                }
                Gdiplus::SolidBrush crown(C(0x2F6B35));
                Gdiplus::SolidBrush trunk(C(0x4A3322));
                g.FillRectangle(&trunk, w * 0.72f, h * 0.48f, 8.0f, 50.0f);
                g.FillEllipse(&crown, w * 0.72f - 36.0f, h * 0.36f, 80.0f, 66.0f);
            }
            break;
        case 7: // северное сияние
        {
            Sky(g, w, h, 0x02081A, 0x0B2238);
            Gdiplus::SolidBrush star(C(0xFFFFFF, 200));
            for (int i = 0; i < 90; ++i)
                g.FillEllipse(&star, uni(rng) * w, uni(rng) * h * 0.6f, 1.5f, 1.5f);
            for (int band = 0; band < 2; ++band)
            {
                Gdiplus::GraphicsPath p;
                const float bf = static_cast<float>(band);
                const float y0 = h * (0.25f + 0.12f * bf);
                p.AddBezier(0.0f, y0 + 40.0f, w * 0.3f, y0 - 50.0f, w * 0.6f, y0 + 60.0f, w, y0 - 10.0f);
                p.AddLine(w, y0 - 10.0f, w, y0 + 70.0f);
                p.AddBezier(w, y0 + 70.0f, w * 0.6f, y0 + 120.0f, w * 0.3f, y0 + 20.0f, 0.0f, y0 + 110.0f);
                p.CloseFigure();
                Gdiplus::LinearGradientBrush br(Gdiplus::PointF(0.0f, y0 - 60.0f), Gdiplus::PointF(0.0f, y0 + 120.0f),
                                                C(band == 0 ? 0x8A4BE8 : 0x2CF2A0, 0), C(band == 0 ? 0x2CF29A : 0x2CF2C8, 150));
                g.FillPath(&br, &p);
            }
            Ridge(g, rng, w, h, h * 0.8f, h * 0.05f, 1.3f, 0xDDE8F3, 0xAFC2D6);
            for (int i = 0; i < 12; ++i)
                Pine(g, w * (0.04f + 0.08f * static_cast<float>(i)) + uni(rng) * 12.0f, h * 0.86f + uni(rng) * 8.0f,
                     50.0f + uni(rng) * 40.0f, 0x05101C);
            break;
        }
        default:
            break;
        }
    }
    cache[seed] = bmp;
    return bmp;
}

std::shared_ptr<Gdiplus::Bitmap> NodeBitmap(const FsNode* node)
{
    if (!node)
        return nullptr;
    if (node->bitmap)
        return node->bitmap;
    return PhotoBitmap(node->seed > 0 ? node->seed : static_cast<int>(node->id % 8U) + 1);
}

void ClearPhotoCache() { Cache().clear(); }

// ===========================================================================
// Приложение «Фотографии»
// ===========================================================================
namespace
{
class PhotosWindow : public Window
{
public:
    explicit PhotosWindow(const std::wstring& arg) : Window(AppId::Photos, L"Фотографии")
    {
        SetInitialSize(1080.0f, 720.0f);
        m_minSize = {520.0f, 380.0f};
        VirtualFS& fs = GetShell().FS();
        FsNode* n = fs.FromArg(arg);
        if (n && n->kind == FileKind::Image)
            OpenViewer(n);
        BuildToolbar();
    }

    AppIcon WindowIcon() const override { return AppIcon::Photos; }

    void CollectImages(FsNode* dir, std::vector<FsNode*>& out)
    {
        for (auto& c : dir->children)
        {
            if (c->kind == FileKind::Image)
                out.push_back(c.get());
            else if (c->IsFolder())
                CollectImages(c.get(), out);
        }
    }

    std::vector<FsNode*> Gallery()
    {
        std::vector<FsNode*> out;
        CollectImages(GetShell().FS().Pictures(), out);
        return out;
    }

    void OpenViewer(FsNode* n)
    {
        m_viewer = true;
        m_current = n;
        m_zoom = 1.0f;
        m_panX = m_panY = 0.0f;
        m_rot = 0;
        m_siblings.clear();
        if (n->parent)
        {
            for (auto& c : n->parent->children)
            {
                if (c->kind == FileKind::Image)
                    m_siblings.push_back(c.get());
            }
        }
        SetTitle(n->name + L" — Фотографии");
        BuildToolbar();
    }

    void CloseViewer()
    {
        m_viewer = false;
        m_current = nullptr;
        SetTitle(L"Фотографии");
        BuildToolbar();
    }

    void Step(int dir)
    {
        if (m_siblings.empty() || !m_current)
            return;
        auto it = std::find(m_siblings.begin(), m_siblings.end(), m_current);
        int idx = it == m_siblings.end() ? 0 : static_cast<int>(it - m_siblings.begin());
        idx = (idx + dir + static_cast<int>(m_siblings.size())) % static_cast<int>(m_siblings.size());
        OpenViewer(m_siblings[static_cast<size_t>(idx)]);
    }

    void BuildToolbar()
    {
        m_ui.Clear();
        const float w = Width() > 0.0f ? Width() : Frame().w;
        if (!m_viewer)
            return;
        auto add = [&](Icon ic, const wchar_t* tip, float x, std::function<void()> fn) {
            auto* b = m_ui.Add<Button>(L"", ic, ButtonStyle::Subtle);
            b->SetBounds({x, 36.0f, 40.0f, 36.0f});
            b->SetTooltip(tip);
            b->onClick = std::move(fn);
            return b;
        };
        auto* back = m_ui.Add<Button>(L"Все фотографии", Icon::ArrowLeft, ButtonStyle::Subtle);
        back->SetBounds({8.0f, 36.0f, 160.0f, 36.0f});
        back->onClick = [this]() { CloseViewer(); };
        float x = w - 12.0f - 40.0f;
        add(Icon::Info, L"Сведения о файле", x, [this]() { m_info = !m_info; });
        x -= 44.0f;
        add(Icon::Delete, L"Удалить", x, [this]() {
            if (!m_current)
                return;
            FsNode* del = m_current;
            Step(1);
            GetShell().FS().Recycle(del);
            m_siblings.erase(std::remove(m_siblings.begin(), m_siblings.end(), del), m_siblings.end());
            if (m_siblings.empty() || m_current == del)
                CloseViewer();
        });
        x -= 44.0f;
        add(Icon::Heart, L"Добавить в избранное", x, [this]() { m_fav = !m_fav; });
        x -= 44.0f;
        add(Icon::RotateRight, L"Повернуть", x, [this]() { m_rot = (m_rot + 1) % 4; });
        x -= 52.0f;
        add(Icon::ZoomIn, L"Увеличить", x, [this]() { m_zoom = std::min(8.0f, m_zoom * 1.25f); });
        x -= 60.0f;
        add(Icon::ZoomOut, L"Уменьшить", x, [this]() {
            m_zoom = std::max(1.0f, m_zoom / 1.25f);
            if (m_zoom <= 1.0f)
                m_panX = m_panY = 0.0f;
        });
    }

    void OnResize() override { BuildToolbar(); }

    Rect ImageArea() const
    {
        const float infoW = m_info ? 300.0f : 0.0f;
        return {0.0f, 80.0f, Width() - infoW, Height() - 80.0f - 84.0f};
    }

    Rect FitRect(Gdiplus::Bitmap* bmp, const Rect& area) const
    {
        float iw = static_cast<float>(bmp->GetWidth()), ih = static_cast<float>(bmp->GetHeight());
        if (m_rot % 2 == 1)
            std::swap(iw, ih);
        const float s = std::min((area.w - 48.0f) / iw, (area.h - 24.0f) / ih) * m_zoom;
        const float w = iw * s, h = ih * s;
        return {area.CenterX() - w * 0.5f + m_panX, area.CenterY() - h * 0.5f + m_panY, w, h};
    }

    void DrawContent(Renderer& r, const Rect& client) override
    {
        const Palette& p = Theme::P();
        if (!m_viewer)
        {
            DrawGallery(r, client);
            return;
        }
        if (!m_current || !GetShell().FS().IsAlive(m_current))
        {
            CloseViewer();
            DrawGallery(r, client);
            return;
        }
        r.Text(m_current->name, {176.0f, 36.0f, Width() - 176.0f - 320.0f, 36.0f}, p.text, 14.0f, FontWeight::Regular,
               TextFlags::VCenter);
        const Rect area = ImageArea();
        r.FillRect(area, p.dark ? Color(0, 0, 0, 0x30) : Color(255, 255, 255, 0x50));
        std::shared_ptr<Gdiplus::Bitmap> bmp = NodeBitmap(m_current);
        if (bmp)
        {
            r.PushClip(area);
            const Rect fr = FitRect(bmp.get(), area);
            const Gdiplus::GraphicsState st = r.G().Save();
            r.G().TranslateTransform(fr.CenterX(), fr.CenterY());
            r.G().RotateTransform(90.0f * static_cast<float>(m_rot));
            r.G().SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            const float dw = (m_rot % 2 == 1) ? fr.h : fr.w, dh = (m_rot % 2 == 1) ? fr.w : fr.h;
            r.Image(bmp.get(), {-dw * 0.5f, -dh * 0.5f, dw, dh});
            r.G().Restore(st);
            r.PopClip();
        }
        // Стрелки «назад/вперёд».
        if (m_hoverArea && m_siblings.size() > 1)
        {
            const Rect lb{area.x + 16.0f, area.CenterY() - 24.0f, 40.0f, 48.0f};
            const Rect rb{area.Right() - 56.0f, area.CenterY() - 24.0f, 40.0f, 48.0f};
            for (const Rect& b : {lb, rb})
            {
                r.FillRoundRect(b, 6.0f, p.dark ? Color(32, 32, 32, 0xD0) : Color(255, 255, 255, 0xD0));
                r.StrokeRoundRect(b, 6.0f, p.controlStroke);
            }
            r.Glyph(Icon::ChevronLeft, {lb.CenterX() - 8.0f, lb.CenterY() - 8.0f, 16.0f, 16.0f}, p.text);
            r.Glyph(Icon::ChevronRight, {rb.CenterX() - 8.0f, rb.CenterY() - 8.0f, 16.0f, 16.0f}, p.text);
        }
        r.Text(std::to_wstring(static_cast<int>(m_zoom * 100.0f + 0.5f)) + L"%",
               {Width() - 12.0f - 40.0f - 44.0f * 4.0f - 52.0f - 6.0f, 36.0f, 52.0f, 36.0f}, p.textSecondary, 12.0f,
               FontWeight::Regular, TextFlags::Middle);
        // Лента миниатюр.
        const float fy = Height() - 76.0f;
        const float tw = 84.0f, th = 56.0f;
        const float total = static_cast<float>(m_siblings.size()) * (tw + 6.0f);
        float fx = std::max(12.0f, (Width() - total) * 0.5f);
        for (FsNode* s : m_siblings)
        {
            std::shared_ptr<Gdiplus::Bitmap> tb = NodeBitmap(s);
            const Rect tr{fx, fy, tw, th};
            if (tb)
                r.ImageRounded(tb.get(), tr, 4.0f);
            if (s == m_current)
                r.StrokeRoundRect(tr.Inflated(2.0f), 6.0f, p.accent, 2.0f);
            fx += tw + 6.0f;
        }
        if (m_info)
        {
            const Rect ip{Width() - 300.0f, 80.0f, 300.0f, Height() - 80.0f - 84.0f};
            r.FillRect(ip, p.layer);
            r.FillRect({ip.x, ip.y, 1.0f, ip.h}, p.divider);
            r.Text(L"Сведения", {ip.x + 20.0f, ip.y + 16.0f, ip.w - 40.0f, 28.0f}, p.text, 20.0f, FontWeight::Semibold);
            VirtualFS& fs = GetShell().FS();
            float y = ip.y + 64.0f;
            auto row = [&](const std::wstring& k, const std::wstring& v) {
                r.Text(k, {ip.x + 20.0f, y, ip.w - 40.0f, 18.0f}, p.textSecondary, 12.0f);
                r.Text(v, {ip.x + 20.0f, y + 18.0f, ip.w - 40.0f, 20.0f}, p.text, 14.0f);
                y += 50.0f;
            };
            row(L"Имя файла", m_current->name);
            if (bmp)
                row(L"Размеры", std::to_wstring(bmp->GetWidth()) + L" x " + std::to_wstring(bmp->GetHeight()));
            row(L"Размер", FormatSize(fs.SizeOf(m_current)));
            row(L"Дата изменения", FormatDateTime(m_current->modified));
            row(L"Расположение", fs.PathOf(m_current->parent));
        }
        m_ui.Draw(r);
    }

    void DrawGallery(Renderer& r, const Rect& client)
    {
        const Palette& p = Theme::P();
        // Левая панель навигации.
        const Rect nav{0.0f, client.y, 220.0f, client.h};
        static const wchar_t* items[] = {L"Все фотографии", L"Избранное", L"Папки"};
        static const Icon icons[] = {Icon::Pictures, Icon::Heart, Icon::Folder};
        for (int i = 0; i < 3; ++i)
        {
            const Rect ir{nav.x + 8.0f, nav.y + 12.0f + static_cast<float>(i) * 40.0f, nav.w - 16.0f, 36.0f};
            if (i == 0)
            {
                r.FillRoundRect(ir, 4.0f, p.subtleHover);
                r.FillRoundRect({ir.x, ir.CenterY() - 8.0f, 3.0f, 16.0f}, 1.5f, p.accent);
            }
            r.Glyph(icons[i], {ir.x + 12.0f, ir.CenterY() - 8.0f, 16.0f, 16.0f}, p.text);
            r.Text(items[i], {ir.x + 40.0f, ir.y, ir.w - 48.0f, ir.h}, p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
        }
        const Rect content{nav.Right(), client.y, Width() - nav.w, client.h};
        r.FillRoundRect(content.Inset(0.0f, 4.0f, 4.0f, 4.0f), 8.0f, 0.0f, 0.0f, 8.0f, p.layer);
        r.Text(L"Все фотографии", {content.x + 28.0f, content.y + 20.0f, 400.0f, 36.0f}, p.text, 28.0f, FontWeight::Semibold);
        const SYSTEMTIME now = LocalNow();
        std::wstring month = MonthName(now.wMonth);
        if (!month.empty())
            month[0] = static_cast<wchar_t>(towupper(month[0]));
        r.Text(month + L" " + std::to_wstring(now.wYear), {content.x + 28.0f, content.y + 70.0f, 300.0f, 20.0f}, p.text, 14.0f,
               FontWeight::Semibold);
        const std::vector<FsNode*> imgs = Gallery();
        const Rect grid{content.x + 24.0f, content.y + 100.0f, content.w - 48.0f, content.h - 104.0f};
        const float tw = 210.0f, th = 140.0f, gap = 6.0f;
        const int cols = std::max(1, static_cast<int>((grid.w + gap) / (tw + gap)));
        const float rowsH = static_cast<float>((static_cast<int>(imgs.size()) + cols - 1) / cols) * (th + gap);
        m_galleryMax = std::max(0.0f, rowsH - grid.h);
        m_gScroll = Clamp(m_gScroll, 0.0f, m_galleryMax);
        r.PushClip(grid);
        m_cells.clear();
        for (size_t i = 0; i < imgs.size(); ++i)
        {
            const int col = static_cast<int>(i) % cols, row = static_cast<int>(i) / cols;
            const Rect cell{grid.x + static_cast<float>(col) * (tw + gap), grid.y + static_cast<float>(row) * (th + gap) - m_gScroll, tw, th};
            m_cells.push_back({cell, imgs[i]});
            if (cell.Bottom() < grid.y || cell.y > grid.Bottom())
                continue;
            std::shared_ptr<Gdiplus::Bitmap> bmp = NodeBitmap(imgs[i]);
            if (bmp)
                r.ImageRounded(bmp.get(), cell, 4.0f);
            if (static_cast<int>(i) == m_hoverCell)
            {
                r.StrokeRoundRect(cell, 4.0f, p.accent, 2.0f);
                r.FillCircle(cell.Right() - 16.0f, cell.y + 16.0f, 9.0f, Color(255, 255, 255, 0xC0));
                r.StrokeCircle(cell.Right() - 16.0f, cell.y + 16.0f, 8.5f, Color(0, 0, 0, 0x60), 1.0f);
            }
        }
        r.PopClip();
        if (imgs.empty())
            r.Text(L"В папке «Изображения» пока нет фотографий", grid, p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::Middle);
    }

    void OnMouseMove(const MouseEvent& e) override
    {
        Window::OnMouseMove(e);
        if (m_viewer)
        {
            m_hoverArea = ImageArea().Contains(e.pos);
            if (m_dragging)
            {
                m_panX = m_panStartX + e.pos.x - m_dragStart.x;
                m_panY = m_panStartY + e.pos.y - m_dragStart.y;
            }
            return;
        }
        m_hoverCell = -1;
        for (size_t i = 0; i < m_cells.size(); ++i)
        {
            if (m_cells[i].first.Contains(e.pos))
                m_hoverCell = static_cast<int>(i);
        }
    }

    void OnMouseDown(const MouseEvent& e) override
    {
        Window::OnMouseDown(e);
        if (m_viewer)
        {
            const Rect area = ImageArea();
            if (area.Contains(e.pos) && e.button == MouseButton::Left)
            {
                if (m_siblings.size() > 1)
                {
                    if (e.pos.x < area.x + 64.0f && std::fabs(e.pos.y - area.CenterY()) < 30.0f)
                    {
                        Step(-1);
                        return;
                    }
                    if (e.pos.x > area.Right() - 64.0f && std::fabs(e.pos.y - area.CenterY()) < 30.0f)
                    {
                        Step(1);
                        return;
                    }
                }
                if (e.clicks >= 2)
                {
                    m_zoom = m_zoom > 1.0f ? 1.0f : 2.0f;
                    m_panX = m_panY = 0.0f;
                    return;
                }
                if (m_zoom > 1.0f)
                {
                    m_dragging = true;
                    m_dragStart = e.pos;
                    m_panStartX = m_panX;
                    m_panStartY = m_panY;
                }
            }
            // Лента миниатюр.
            const float fy = Height() - 76.0f;
            if (e.pos.y >= fy && e.pos.y <= fy + 56.0f)
            {
                const float total = static_cast<float>(m_siblings.size()) * 90.0f;
                const float fx = std::max(12.0f, (Width() - total) * 0.5f);
                const int idx = static_cast<int>((e.pos.x - fx) / 90.0f);
                if (e.pos.x >= fx && idx >= 0 && idx < static_cast<int>(m_siblings.size()))
                    OpenViewer(m_siblings[static_cast<size_t>(idx)]);
            }
            return;
        }
        if (e.clicks >= 2 && m_hoverCell >= 0 && m_hoverCell < static_cast<int>(m_cells.size()))
            OpenViewer(m_cells[static_cast<size_t>(m_hoverCell)].second);
    }

    void OnMouseUp(const MouseEvent& e) override
    {
        Window::OnMouseUp(e);
        m_dragging = false;
    }

    bool OnMouseWheel(const MouseEvent& e) override
    {
        if (m_viewer)
        {
            m_zoom = Clamp(m_zoom * (e.wheel > 0.0f ? 1.15f : 1.0f / 1.15f), 1.0f, 8.0f);
            if (m_zoom <= 1.0f)
                m_panX = m_panY = 0.0f;
            return true;
        }
        m_gScroll = Clamp(m_gScroll - e.wheel * 80.0f, 0.0f, m_galleryMax);
        return true;
    }

    bool OnKeyDown(UINT vk, const KeyMods& mods) override
    {
        if (m_viewer)
        {
            switch (vk)
            {
            case VK_LEFT:
                Step(-1);
                return true;
            case VK_RIGHT:
                Step(1);
                return true;
            case VK_ESCAPE:
                CloseViewer();
                return true;
            case VK_OEM_PLUS:
            case VK_ADD:
                m_zoom = std::min(8.0f, m_zoom * 1.25f);
                return true;
            case VK_OEM_MINUS:
            case VK_SUBTRACT:
                m_zoom = std::max(1.0f, m_zoom / 1.25f);
                return true;
            default:
                break;
            }
        }
        return Window::OnKeyDown(vk, mods);
    }

    CursorType Cursor(Point local) const override
    {
        if (m_viewer && m_zoom > 1.0f && ImageArea().Contains(local))
            return CursorType::SizeAll;
        return Window::Cursor(local);
    }

private:
    bool m_viewer = false;
    FsNode* m_current = nullptr;
    std::vector<FsNode*> m_siblings;
    float m_zoom = 1.0f;
    float m_panX = 0.0f, m_panY = 0.0f;
    float m_panStartX = 0.0f, m_panStartY = 0.0f;
    Point m_dragStart;
    bool m_dragging = false;
    int m_rot = 0;
    bool m_info = false;
    bool m_fav = false;
    bool m_hoverArea = false;
    std::vector<std::pair<Rect, FsNode*>> m_cells;
    int m_hoverCell = -1;
    float m_gScroll = 0.0f;
    float m_galleryMax = 0.0f;
};
} // namespace

std::unique_ptr<Window> CreatePhotos(const std::wstring& arg) { return std::make_unique<PhotosWindow>(arg); }
