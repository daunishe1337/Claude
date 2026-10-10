#include "Desktop.h"

#include "Flyouts.h"

// ===========================================================================
// Процедурные обои
// ===========================================================================
namespace
{
struct RGBf
{
    float r, g, b;
};
inline RGBf Mix(RGBf a, RGBf b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
inline RGBf Add(RGBf a, RGBf b, float k) { return {a.r + b.r * k, a.g + b.g * k, a.b + b.b * k}; }
inline RGBf Mul(RGBf a, float k) { return {a.r * k, a.g * k, a.b * k}; }
inline RGBf H3(uint32_t hex)
{
    return {static_cast<float>((hex >> 16) & 0xFF) / 255.0f, static_cast<float>((hex >> 8) & 0xFF) / 255.0f,
            static_cast<float>(hex & 0xFF) / 255.0f};
}
inline float WrapAngle(float a)
{
    while (a > kPi)
        a -= 2.0f * kPi;
    while (a < -kPi)
        a += 2.0f * kPi;
    return a;
}
inline uint32_t Hash(uint32_t x, uint32_t y)
{
    uint32_t h = x * 374761393u + y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

struct Petal
{
    float theta, length, width, curl;
};

// Один «лепесток» цветка: спиральная полоса с мягкими краями и складкой.
void PetalLayer(RGBf& c, float x, float y, const Petal& pt, float aa, RGBf deep, RGBf bright, RGBf hi)
{
    const float r = std::sqrt(x * x + y * y);
    const float t = r / pt.length;
    if (t >= 1.0f || r < 1e-5f)
        return;
    const float phi = std::atan2(y, x);
    const float d = WrapAngle(phi - (pt.theta + pt.curl * r)) * r;
    const float hw = pt.width * std::pow(std::sin(kPi * t), 0.85f) * (0.55f + 0.45f * t);
    const float edge = hw - std::fabs(d);
    if (edge < -aa)
    {
        // Мягкая тень вокруг лепестка — объём.
        const float sh = (1.0f - SmoothStep(0.0f, 0.035f, -edge)) * 0.16f * std::sin(kPi * t);
        c = Mul(c, 1.0f - sh);
        return;
    }
    const float a = SmoothStep(-aa, aa, edge);
    const float s = Clamp(d / std::max(hw, 1e-4f), -1.0f, 1.0f);
    const float across = Saturate(0.5f + 0.5f * s);
    RGBf pc = Mix(deep, bright, std::pow(across, 1.4f));
    const float fold = std::exp(-((s - 0.62f) / 0.2f) * ((s - 0.62f) / 0.2f));
    pc = Mix(pc, hi, fold * (0.25f + 0.45f * t));
    pc = Mul(pc, 0.72f + 0.38f * t);
    // Тень у основания лепестка
    pc = Mul(pc, 0.75f + 0.25f * SmoothStep(0.0f, 0.35f, t));
    c = Mix(c, pc, a * 0.93f);
}

RGBf SampleBloom(float u, float v, float aspect, bool dark, float aa)
{
    const float x = (u - 0.5f) * aspect, y = v - 0.56f;
    RGBf c = dark ? Mix(H3(0x01040E), H3(0x061433), v) : Mix(H3(0xD2E4F7), H3(0x8DB9EC), v);
    const float rr = std::sqrt(x * x + y * y);
    const RGBf glowC = dark ? H3(0x0A3E9A) : H3(0xE9F4FF);
    c = Mix(c, glowC, std::exp(-rr * rr / (2.0f * 0.2f * 0.2f)) * (dark ? 0.75f : 0.6f));

    const RGBf deep = dark ? H3(0x002A7A) : H3(0x0047C2);
    const RGBf bright = dark ? H3(0x2C8CFF) : H3(0x58AEFF);
    const RGBf hi = dark ? H3(0x8ED0FF) : H3(0xD6EEFF);
    for (int i = 0; i < 6; ++i)
    {
        const Petal p{0.25f + static_cast<float>(i) * (2.0f * kPi / 6.0f), 0.44f, 0.165f, 0.75f};
        PetalLayer(c, x, y, p, aa, deep, bright, hi);
    }
    const RGBf deep2 = dark ? H3(0x003A9E) : H3(0x0A5CD6);
    const RGBf bright2 = dark ? H3(0x45A6FF) : H3(0x7CC4FF);
    for (int i = 0; i < 5; ++i)
    {
        const Petal p{0.85f + static_cast<float>(i) * (2.0f * kPi / 5.0f), 0.28f, 0.115f, 1.2f};
        PetalLayer(c, x, y, p, aa, deep2, bright2, hi);
    }
    const RGBf deep3 = dark ? H3(0x0B4FC0) : H3(0x1E6FE6);
    const RGBf bright3 = dark ? H3(0x6BBDFF) : H3(0x9AD3FF);
    for (int i = 0; i < 4; ++i)
    {
        const Petal p{1.6f + static_cast<float>(i) * (2.0f * kPi / 4.0f), 0.15f, 0.075f, 1.9f};
        PetalLayer(c, x, y, p, aa, deep3, bright3, hi);
    }
    c = Mix(c, dark ? H3(0x9BD7FF) : H3(0xF2FAFF), std::exp(-rr * rr / (2.0f * 0.03f * 0.03f)) * 0.85f);
    if (dark)
    {
        const float vx = (u - 0.5f) * aspect, vy = v - 0.5f;
        c = Mul(c, 1.0f - 0.35f * SmoothStep(0.45f, 1.15f, std::sqrt(vx * vx + vy * vy)));
    }
    return c;
}

RGBf SampleFlow(float u, float v, float aspect)
{
    RGBf c = Mix(H3(0x041A33), H3(0x00566E), Saturate(v * 0.8f + u * 0.25f));
    const float x = u * aspect;
    for (int k = 0; k < 6; ++k)
    {
        const float kf = static_cast<float>(k);
        const float yc = 0.30f + kf * 0.085f + 0.09f * std::sin(x * 2.1f + kf * 1.3f) + 0.03f * std::sin(x * 5.3f + kf);
        const float th = 0.03f + 0.014f * std::sin(x * 2.7f + kf * 0.7f);
        const float d = (v - yc) / th;
        const float a = std::exp(-d * d);
        const RGBf rc = Mul(Mix(H3(0x12E0D8), H3(0x3A62F2), Saturate(u * 0.9f + kf * 0.05f)), 0.55f + 0.08f * kf);
        c = Add(c, rc, a * 0.6f);
        c = Add(c, rc, std::exp(-d * d * 0.08f) * 0.06f);
    }
    return c;
}

RGBf SampleSunset(float u, float v, float aspect)
{
    RGBf c;
    if (v < 0.45f)
        c = Mix(H3(0x1E1145), H3(0xD9606A), SmoothStep(0.0f, 0.45f, v));
    else
        c = Mix(H3(0xD9606A), H3(0xFFC27A), SmoothStep(0.45f, 0.66f, v));
    const float sx = (u - 0.66f) * aspect, sy = v - 0.6f;
    const float sd = std::sqrt(sx * sx + sy * sy);
    c = Mix(c, H3(0xFFF1C9), SmoothStep(0.075f, 0.068f, sd));
    c = Add(c, H3(0xFFB070), std::exp(-sd * sd / (2.0f * 0.12f * 0.12f)) * 0.35f);
    const float x = u * aspect;
    const RGBf layers[3] = {H3(0x7A3E66), H3(0x46254F), H3(0x1D1029)};
    const float base[3] = {0.66f, 0.74f, 0.84f};
    for (int k = 0; k < 3; ++k)
    {
        const float kf = static_cast<float>(k);
        const float h = base[k] - 0.07f * (0.6f * std::sin(x * (1.7f + kf) + kf * 2.0f) + 0.3f * std::sin(x * (4.1f + kf) + 1.0f) +
                                           0.1f * std::sin(x * 11.0f + kf));
        if (v > h)
            c = Mix(c, Mul(layers[k], 1.0f + 0.25f * (v - h)), SmoothStep(h, h + 0.002f, v));
    }
    return c;
}

RGBf SampleAurora(float u, float v, float aspect, uint32_t ix, uint32_t iy)
{
    RGBf c = Mix(H3(0x01030B), H3(0x061526), v);
    if ((Hash(ix, iy) & 0xFFFF) < 18 && v < 0.75f)
        c = Add(c, H3(0xFFFFFF), 0.5f + static_cast<float>(Hash(iy, ix) & 0xFF) / 512.0f);
    const float x = u * aspect;
    for (int k = 0; k < 2; ++k)
    {
        const float kf = static_cast<float>(k);
        const float ya = 0.32f + kf * 0.1f + 0.07f * std::sin(x * 1.6f + kf * 2.0f) + 0.03f * std::sin(x * 4.4f + 1.0f + kf);
        const float rays = 0.65f + 0.35f * std::sin(x * 55.0f + std::sin(x * 6.0f + kf) * 4.0f);
        float inten;
        if (v < ya)
            inten = std::exp(-(ya - v) / 0.16f) * rays;
        else
            inten = std::exp(-((v - ya) / 0.025f) * ((v - ya) / 0.025f));
        const RGBf col = Mix(H3(0x2CF29A), H3(0x8A4BE8), Saturate((ya - v) * 3.0f));
        c = Add(c, col, inten * (0.65f - kf * 0.25f));
    }
    const float g = 0.84f + 0.025f * std::sin(x * 7.0f) + 0.015f * std::sin(x * 23.0f);
    if (v > g)
        c = Mix(c, H3(0x010205), SmoothStep(g, g + 0.003f, v));
    return c;
}

RGBf SampleWallpaper(int variant, float u, float v, float aspect, float aa, uint32_t ix, uint32_t iy)
{
    switch (variant)
    {
    case 0:
        return SampleBloom(u, v, aspect, false, aa);
    case 1:
        return SampleBloom(u, v, aspect, true, aa);
    case 2:
        return SampleFlow(u, v, aspect);
    case 3:
        return SampleSunset(u, v, aspect);
    case 4:
        return SampleAurora(u, v, aspect, ix, iy);
    default:
        return Mix(H3(0x0F3A66), H3(0x0B2C4F), v);
    }
}
} // namespace

const std::vector<WallpaperInfo>& Wallpapers()
{
    static const std::vector<WallpaperInfo> list = {
        {L"Свечение", false}, {L"Свечение (тёмная)", true}, {L"Поток", true},
        {L"Закат", true},     {L"Северное сияние", true},   {L"Сплошной цвет", true},
    };
    return list;
}

void RenderWallpaper(int variant, int width, int height, std::vector<uint32_t>& out)
{
    width = std::max(1, width);
    height = std::max(1, height);
    // Считаем в половинном разрешении и масштабируем с дизерингом (без полос в градиентах).
    const int factor = width > 800 ? 2 : 1;
    const int sw = (width + factor - 1) / factor, sh = (height + factor - 1) / factor;
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float aa = 1.2f / static_cast<float>(sh);
    std::vector<RGBf> buf(static_cast<size_t>(sw) * static_cast<size_t>(sh));
    for (int y = 0; y < sh; ++y)
    {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(sh);
        for (int x = 0; x < sw; ++x)
        {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(sw);
            buf[static_cast<size_t>(y) * static_cast<size_t>(sw) + static_cast<size_t>(x)] =
                SampleWallpaper(variant, u, v, aspect, aa, static_cast<uint32_t>(x), static_cast<uint32_t>(y));
        }
    }
    out.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
    const float inv = 1.0f / static_cast<float>(factor);
    for (int y = 0; y < height; ++y)
    {
        const float fy = (static_cast<float>(y) + 0.5f) * inv - 0.5f;
        const int y0 = Clamp(static_cast<int>(std::floor(fy)), 0, sh - 1), y1 = std::min(y0 + 1, sh - 1);
        const float ty = Saturate(fy - static_cast<float>(y0));
        for (int x = 0; x < width; ++x)
        {
            const float fx = (static_cast<float>(x) + 0.5f) * inv - 0.5f;
            const int x0 = Clamp(static_cast<int>(std::floor(fx)), 0, sw - 1), x1 = std::min(x0 + 1, sw - 1);
            const float tx = Saturate(fx - static_cast<float>(x0));
            const RGBf& a = buf[static_cast<size_t>(y0) * static_cast<size_t>(sw) + static_cast<size_t>(x0)];
            const RGBf& b = buf[static_cast<size_t>(y0) * static_cast<size_t>(sw) + static_cast<size_t>(x1)];
            const RGBf& c = buf[static_cast<size_t>(y1) * static_cast<size_t>(sw) + static_cast<size_t>(x0)];
            const RGBf& d = buf[static_cast<size_t>(y1) * static_cast<size_t>(sw) + static_cast<size_t>(x1)];
            const RGBf top = Mix(a, b, tx), bot = Mix(c, d, tx);
            const RGBf px = Mix(top, bot, ty);
            const float dither = (static_cast<float>(Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y)) & 0xFF) / 255.0f - 0.5f);
            auto q = [dither](float ch) {
                return static_cast<uint32_t>(Clamp(ch * 255.0f + dither, 0.0f, 255.0f) + 0.5f) & 0xFFu;
            };
            out[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] =
                0xFF000000u | (q(px.r) << 16) | (q(px.g) << 8) | q(px.b);
        }
    }
}

// ===========================================================================
// Desktop
// ===========================================================================
Desktop::Desktop()
{
    Icon2 pc;
    pc.type = Icon2::Type::ThisPC;
    pc.col = 0;
    pc.row = 0;
    m_icons.push_back(pc);
    Icon2 bin;
    bin.type = Icon2::Type::RecycleBin;
    bin.col = 0;
    bin.row = 1;
    m_icons.push_back(bin);
}

float Desktop::IconSize() const
{
    const int s = GetShell().Settings().desktopIconSize;
    return s == 0 ? 32.0f : (s == 2 ? 96.0f : 48.0f);
}
float Desktop::CellW() const
{
    const int s = GetShell().Settings().desktopIconSize;
    return s == 0 ? 76.0f : (s == 2 ? 118.0f : 82.0f);
}
float Desktop::CellH() const
{
    const int s = GetShell().Settings().desktopIconSize;
    return s == 0 ? 70.0f : (s == 2 ? 140.0f : 92.0f);
}
int Desktop::Rows() const { return std::max(1, static_cast<int>((GetShell().WorkArea().h - 8.0f) / CellH())); }
int Desktop::Cols() const { return std::max(1, static_cast<int>((GetShell().ScreenW() - 8.0f) / CellW())); }
Rect Desktop::CellRect(int col, int row) const
{
    return {4.0f + static_cast<float>(col) * CellW(), 4.0f + static_cast<float>(row) * CellH(), CellW(), CellH()};
}
Rect Desktop::IconRect(const Icon2& ic) const { return CellRect(ic.col, ic.row).Inflated(-2.0f); }

std::wstring Desktop::NameOf(const Icon2& ic) const
{
    switch (ic.type)
    {
    case Icon2::Type::ThisPC:
        return L"Этот компьютер";
    case Icon2::Type::RecycleBin:
        return L"Корзина";
    case Icon2::Type::Node:
        return VirtualFS::DisplayName(ic.node, GetShell().Settings().showExtensions);
    }
    return L"";
}

AppIcon Desktop::IconOf(const Icon2& ic) const
{
    VirtualFS& fs = GetShell().FS();
    switch (ic.type)
    {
    case Icon2::Type::ThisPC:
        return AppIcon::ThisPC;
    case Icon2::Type::RecycleBin:
        return fs.Bin()->children.empty() ? AppIcon::RecycleBinEmpty : AppIcon::RecycleBinFull;
    case Icon2::Type::Node:
        return fs.IconFor(ic.node);
    }
    return AppIcon::FileGeneric;
}

bool Desktop::CellFree(int col, int row, int ignore) const
{
    for (int i = 0; i < static_cast<int>(m_icons.size()); ++i)
    {
        if (i != ignore && m_icons[static_cast<size_t>(i)].col == col && m_icons[static_cast<size_t>(i)].row == row)
            return false;
    }
    return true;
}

void Desktop::FreeCell(int& col, int& row) const
{
    const int rows = Rows(), cols = Cols();
    for (int c = 0; c < cols; ++c)
    {
        for (int r = 0; r < rows; ++r)
        {
            if (CellFree(c, r))
            {
                col = c;
                row = r;
                return;
            }
        }
    }
    col = 0;
    row = 0;
}

void Desktop::Sync()
{
    VirtualFS& fs = GetShell().FS();
    if (m_fsVersion == fs.Version())
        return;
    m_fsVersion = fs.Version();
    FsNode* desk = fs.Desktop();
    // Удалить исчезнувшие.
    for (int i = static_cast<int>(m_icons.size()) - 1; i >= 0; --i)
    {
        const Icon2& ic = m_icons[static_cast<size_t>(i)];
        if (ic.type != Icon2::Type::Node)
            continue;
        if (ic.node->parent != desk)
        {
            if (m_renameIndex == i)
                CancelRename();
            m_icons.erase(m_icons.begin() + i);
            if (m_focus >= static_cast<int>(m_icons.size()))
                m_focus = -1;
            m_hover = -1;
        }
    }
    // Добавить новые.
    for (auto& child : desk->children)
    {
        bool found = false;
        for (const Icon2& ic : m_icons)
            found = found || ic.node == child.get();
        if (found)
            continue;
        Icon2 ic;
        ic.node = child.get();
        FreeCell(ic.col, ic.row);
        m_icons.push_back(ic);
    }
    if (GetShell().Settings().autoArrange)
        Arrange(0);
}

void Desktop::Arrange(int sortMode)
{
    VirtualFS& fs = GetShell().FS();
    std::vector<Icon2> special, nodes;
    for (const Icon2& ic : m_icons)
        (ic.type == Icon2::Type::Node ? nodes : special).push_back(ic);
    std::stable_sort(nodes.begin(), nodes.end(), [&fs, sortMode](const Icon2& a, const Icon2& b) {
        if (a.node->IsFolder() != b.node->IsFolder())
            return a.node->IsFolder();
        switch (sortMode)
        {
        case 1:
            return fs.SizeOf(a.node) > fs.SizeOf(b.node);
        case 2:
            return fs.TypeName(a.node) < fs.TypeName(b.node);
        case 3:
        {
            FILETIME fa{}, fb{};
            SystemTimeToFileTime(&a.node->modified, &fa);
            SystemTimeToFileTime(&b.node->modified, &fb);
            return CompareFileTime(&fa, &fb) > 0;
        }
        default:
            return CompareNoCase(a.node->name, b.node->name) < 0;
        }
    });
    m_icons = special;
    m_icons.insert(m_icons.end(), nodes.begin(), nodes.end());
    const int rows = Rows();
    for (size_t i = 0; i < m_icons.size(); ++i)
    {
        m_icons[i].col = static_cast<int>(i) / rows;
        m_icons[i].row = static_cast<int>(i) % rows;
    }
    m_renameIndex = -1;
    m_renameBox = nullptr;
    m_ui.Clear();
}

int Desktop::IconAt(Point p) const
{
    if (!GetShell().Settings().showDesktopIcons)
        return -1;
    for (int i = static_cast<int>(m_icons.size()) - 1; i >= 0; --i)
    {
        if (IconRect(m_icons[static_cast<size_t>(i)]).Contains(p))
            return i;
    }
    return -1;
}

void Desktop::Draw(Renderer& r)
{
    Sync();
    r.DrawWallpaper();
    const ShellSettings& st = GetShell().Settings();
    if (!st.showDesktopIcons)
        return;
    const float isz = IconSize();
    const float fs = st.desktopIconSize == 2 ? 13.0f : 12.0f;
    const float refresh = m_refresh.Value();
    r.PushOpacity(refresh);
    const float dragDx = m_dragging ? m_dragPos.x - m_pressPos.x : 0.0f;
    const float dragDy = m_dragging ? m_dragPos.y - m_pressPos.y : 0.0f;
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int i = 0; i < static_cast<int>(m_icons.size()); ++i)
        {
            const Icon2& ic = m_icons[static_cast<size_t>(i)];
            const bool dragged = m_dragging && ic.selected;
            if ((pass == 1) != dragged)
                continue;
            Rect cell = IconRect(ic);
            if (dragged)
                cell = cell.Translated(dragDx, dragDy);
            const bool hover = i == m_hover && !m_dragging;
            if (dragged)
                r.PushOpacity(0.6f);
            if (ic.selected || hover)
            {
                const Color fill = ic.selected ? Color(204, 232, 255, i == m_focus ? 0x55 : 0x40) : Color(255, 255, 255, 0x1E);
                const Color stroke = ic.selected ? Color(204, 232, 255, 0x90) : Color(255, 255, 255, 0x40);
                r.FillRoundRect(cell, 3.0f, fill);
                r.StrokeRoundRect(cell, 3.0f, stroke);
            }
            r.DrawAppIcon(IconOf(ic), {cell.CenterX() - isz * 0.5f, cell.y + 4.0f, isz, isz});
            if (i != m_renameIndex)
            {
                const std::wstring name = NameOf(ic);
                const Rect tr{cell.x + 3.0f, cell.y + isz + 7.0f, cell.w - 6.0f, ic.selected ? 60.0f : fs * 2.75f};
                const unsigned flags = TextFlags::Center | TextFlags::Wrap;
                r.Text(name, tr.Translated(1.0f, 1.0f), Color(0, 0, 0, 0xB0), fs, FontWeight::Regular, flags);
                r.Text(name, tr.Translated(0.0f, 1.0f), Color(0, 0, 0, 0x50), fs, FontWeight::Regular, flags);
                r.Text(name, tr, Color(255, 255, 255), fs, FontWeight::Regular, flags);
            }
            if (dragged)
                r.PopOpacity();
        }
    }
    r.PopOpacity();
    if (m_banding)
    {
        const Rect band = Rect::LTRB(std::min(m_pressPos.x, m_dragPos.x), std::min(m_pressPos.y, m_dragPos.y),
                                     std::max(m_pressPos.x, m_dragPos.x), std::max(m_pressPos.y, m_dragPos.y));
        const Color acc = Theme::A().base;
        r.FillRect(band, acc.WithAlpha(0x48));
        r.StrokeRect(band, acc.WithAlpha(0xD0));
    }
    m_ui.Draw(r);
}

std::vector<FsNode*> Desktop::SelectedNodes() const
{
    std::vector<FsNode*> out;
    for (const Icon2& ic : m_icons)
    {
        if (ic.selected && ic.type == Icon2::Type::Node)
            out.push_back(ic.node);
    }
    return out;
}

void Desktop::ClearSelection()
{
    for (Icon2& ic : m_icons)
        ic.selected = false;
}

void Desktop::Open(int index)
{
    if (index < 0 || index >= static_cast<int>(m_icons.size()))
        return;
    Shell& sh = GetShell();
    const Icon2& ic = m_icons[static_cast<size_t>(index)];
    switch (ic.type)
    {
    case Icon2::Type::ThisPC:
        sh.Launch(AppId::Explorer, L"::pc");
        break;
    case Icon2::Type::RecycleBin:
        sh.Launch(AppId::Explorer, L"::bin");
        break;
    case Icon2::Type::Node:
        sh.Open(ic.node);
        break;
    }
}

void Desktop::Refresh()
{
    m_refresh.Start(0.0f, 1.0f, 0.35f, Ease::OutCubic);
    m_fsVersion = 0;
    Sync();
}

void Desktop::DeleteSelected()
{
    VirtualFS& fs = GetShell().FS();
    for (FsNode* n : SelectedNodes())
        fs.Recycle(n);
}

void Desktop::StartRename(FsNode* node)
{
    for (int i = 0; i < static_cast<int>(m_icons.size()); ++i)
    {
        Icon2& ic = m_icons[static_cast<size_t>(i)];
        if (ic.type == Icon2::Type::Node && ic.node == node)
        {
            ClearSelection();
            ic.selected = true;
            m_focus = i;
            m_renameIndex = i;
            m_ui.Clear();
            const Rect cell = IconRect(ic);
            m_renameBox = m_ui.Add<TextBox>();
            m_renameBox->fontSize = 12.0f;
            m_renameBox->SetBounds({cell.x - 4.0f, cell.y + IconSize() + 6.0f, cell.w + 8.0f, 26.0f});
            m_renameBox->SetText(NameOf(ic));
            m_renameBox->SelectAll();
            m_renameBox->onEnter = [this]() { CommitRename(); };
            m_renameBox->onEscape = [this]() { CancelRename(); };
            m_ui.SetFocus(m_renameBox);
            return;
        }
    }
}

void Desktop::CommitRename()
{
    if (m_renameIndex < 0 || !m_renameBox)
        return;
    const Icon2 ic = m_icons[static_cast<size_t>(m_renameIndex)];
    std::wstring name = Trim(m_renameBox->Text());
    m_renameIndex = -1;
    m_renameBox = nullptr;
    PostAction([this]() { m_ui.Clear(); });
    if (ic.type != Icon2::Type::Node || name.empty())
        return;
    if (!GetShell().Settings().showExtensions && !ic.node->IsFolder())
    {
        std::wstring ext;
        VirtualFS::SplitExtension(ic.node->name, ext);
        if (VirtualFS::KindFor(ic.node->name) != FileKind::Other)
            name += ext;
    }
    if (!GetShell().FS().Rename(ic.node, name))
        GetShell().ShowMessage(L"Переименование", L"Имя файла не может содержать следующие знаки: \\ / : * ? \" < > |\nили такое имя уже существует.");
}

void Desktop::CancelRename()
{
    m_renameIndex = -1;
    m_renameBox = nullptr;
    PostAction([this]() { m_ui.Clear(); });
}

void Desktop::Drop(Point p)
{
    VirtualFS& fs = GetShell().FS();
    const int target = IconAt(p);
    // Бросили на корзину или папку?
    if (target >= 0 && !m_icons[static_cast<size_t>(target)].selected)
    {
        const Icon2& t = m_icons[static_cast<size_t>(target)];
        if (t.type == Icon2::Type::RecycleBin)
        {
            DeleteSelected();
            return;
        }
        if (t.type == Icon2::Type::Node && t.node->IsFolder())
        {
            for (FsNode* n : SelectedNodes())
                fs.Move(n, t.node);
            return;
        }
    }
    // Перенос по сетке.
    const int dc = RoundToInt((m_dragPos.x - m_pressPos.x) / CellW());
    const int dr = RoundToInt((m_dragPos.y - m_pressPos.y) / CellH());
    if (dc == 0 && dr == 0)
        return;
    const int rows = Rows(), cols = Cols();
    std::vector<int> moved;
    for (int i = 0; i < static_cast<int>(m_icons.size()); ++i)
    {
        if (m_icons[static_cast<size_t>(i)].selected)
            moved.push_back(i);
    }
    // Временно убираем перемещаемые значки с сетки.
    std::vector<std::pair<int, int>> old;
    for (int i : moved)
    {
        old.emplace_back(m_icons[static_cast<size_t>(i)].col, m_icons[static_cast<size_t>(i)].row);
        m_icons[static_cast<size_t>(i)].col = -100;
    }
    for (size_t k = 0; k < moved.size(); ++k)
    {
        Icon2& ic = m_icons[static_cast<size_t>(moved[k])];
        int c = Clamp(old[k].first + dc, 0, cols - 1);
        int r = Clamp(old[k].second + dr, 0, rows - 1);
        if (!CellFree(c, r, moved[k]))
        {
            // Ближайшая свободная ячейка.
            int best = 1 << 30;
            for (int cc = 0; cc < cols; ++cc)
            {
                for (int rr = 0; rr < rows; ++rr)
                {
                    if (!CellFree(cc, rr, moved[k]))
                        continue;
                    const int d = (cc - c) * (cc - c) + (rr - r) * (rr - r);
                    if (d < best)
                    {
                        best = d;
                        ic.col = cc;
                        ic.row = rr;
                    }
                }
            }
            c = ic.col;
            r = ic.row;
        }
        ic.col = c;
        ic.row = r;
    }
}

void Desktop::MouseDown(const MouseEvent& e)
{
    Shell& sh = GetShell();
    if (m_renameIndex >= 0)
    {
        if (m_renameBox && m_renameBox->Bounds().Contains(e.pos))
        {
            Widget::Origin() = {0.0f, 0.0f};
            m_ui.OnMouseDown(e);
            return;
        }
        CommitRename();
    }
    const int idx = IconAt(e.pos);
    m_pressPos = e.pos;
    m_dragPos = e.pos;
    m_pressIndex = idx;
    m_pressed = true;
    if (e.button == MouseButton::Left)
    {
        if (idx >= 0)
        {
            Icon2& ic = m_icons[static_cast<size_t>(idx)];
            if (e.clicks >= 2)
            {
                m_pressed = false;
                Open(idx);
                return;
            }
            if (e.mods.ctrl)
                ic.selected = !ic.selected;
            else if (!ic.selected)
            {
                ClearSelection();
                ic.selected = true;
            }
            m_focus = idx;
        }
        else
        {
            if (!e.mods.ctrl)
                ClearSelection();
            m_bandBase.clear();
            for (const Icon2& ic : m_icons)
                m_bandBase.push_back(ic.selected);
            m_banding = true;
            m_focus = -1;
        }
    }
    else if (e.button == MouseButton::Right)
    {
        if (idx >= 0)
        {
            if (!m_icons[static_cast<size_t>(idx)].selected)
            {
                ClearSelection();
                m_icons[static_cast<size_t>(idx)].selected = true;
            }
            m_focus = idx;
        }
        else
            ClearSelection();
    }
    (void)sh;
}

void Desktop::MouseMove(const MouseEvent& e)
{
    if (m_renameIndex >= 0 && m_ui.HasCapture())
    {
        m_ui.OnMouseMove(e);
        return;
    }
    m_dragPos = e.pos;
    if (m_banding)
    {
        const Rect band = Rect::LTRB(std::min(m_pressPos.x, e.pos.x), std::min(m_pressPos.y, e.pos.y),
                                     std::max(m_pressPos.x, e.pos.x), std::max(m_pressPos.y, e.pos.y));
        for (size_t i = 0; i < m_icons.size(); ++i)
        {
            const bool in = band.Intersects(IconRect(m_icons[i]).Inflated(-8.0f));
            const bool base = i < m_bandBase.size() ? m_bandBase[i] : false;
            m_icons[i].selected = e.mods.ctrl ? (base != in) : in;
        }
        return;
    }
    if (m_pressed && m_pressIndex >= 0 && !m_dragging && (e.pos.x - m_pressPos.x) * (e.pos.x - m_pressPos.x) +
                                                                 (e.pos.y - m_pressPos.y) * (e.pos.y - m_pressPos.y) > 25.0f)
        m_dragging = true;
    if (!m_pressed)
    {
        m_hover = IconAt(e.pos);
        if (m_hover >= 0)
        {
            const Icon2& ic = m_icons[static_cast<size_t>(m_hover)];
            std::wstring tip;
            if (ic.type == Icon2::Type::Node && !ic.node->IsFolder())
                tip = L"Тип: " + GetShell().FS().TypeName(ic.node) + L"\nРазмер: " + FormatSize(GetShell().FS().SizeOf(ic.node)) +
                      L"\nДата изменения: " + FormatDateTime(ic.node->modified);
            else if (ic.type == Icon2::Type::ThisPC)
                tip = L"Просмотр дисков и устройств, подключённых к этому компьютеру";
            else if (ic.type == Icon2::Type::RecycleBin)
                tip = L"Содержит удалённые файлы и папки";
            if (!tip.empty())
                GetShell().Tooltip(ic.node ? static_cast<const void*>(ic.node) : static_cast<const void*>(&m_icons), tip,
                                   {e.pos.x, e.pos.y + 18.0f, 1.0f, 1.0f}, false);
        }
    }
}

void Desktop::MouseUp(const MouseEvent& e)
{
    if (m_renameIndex >= 0 && m_ui.HasCapture())
    {
        m_ui.OnMouseUp(e);
        return;
    }
    const bool wasDragging = m_dragging;
    const bool wasBanding = m_banding;
    m_pressed = false;
    m_dragging = false;
    m_banding = false;
    if (wasDragging)
    {
        Drop(e.pos);
        return;
    }
    if (e.button == MouseButton::Right && !wasBanding)
    {
        const int idx = IconAt(e.pos);
        if (idx >= 0)
            ShowIconMenu(idx, e.pos);
        else
            ShowBackgroundMenu(e.pos, false);
        return;
    }
    if (e.button == MouseButton::Left && !wasBanding && m_pressIndex >= 0 && !e.mods.ctrl)
    {
        // Щелчок по одному из выделенных: оставить выделенным только его.
        for (int i = 0; i < static_cast<int>(m_icons.size()); ++i)
            m_icons[static_cast<size_t>(i)].selected = i == m_pressIndex;
    }
}

void Desktop::MouseLeave() { m_hover = -1; }

CursorType Desktop::Cursor(Point p) const
{
    if (m_renameBox && m_renameBox->Bounds().Contains(p))
        return CursorType::IBeam;
    return CursorType::Arrow;
}

bool Desktop::KeyDown(UINT vk, const KeyMods& mods)
{
    if (m_renameIndex >= 0)
    {
        if (m_ui.OnKeyDown(vk, mods))
            return true;
        return true;
    }
    Shell& sh = GetShell();
    VirtualFS& fs = sh.FS();
    switch (vk)
    {
    case VK_DELETE:
        DeleteSelected();
        return true;
    case VK_F2:
        if (m_focus >= 0 && m_icons[static_cast<size_t>(m_focus)].type == Icon2::Type::Node)
            StartRename(m_icons[static_cast<size_t>(m_focus)].node);
        return true;
    case VK_F5:
        Refresh();
        return true;
    case VK_RETURN:
        if (m_focus >= 0)
            Open(m_focus);
        return true;
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    {
        if (m_icons.empty())
            return true;
        if (m_focus < 0)
            m_focus = 0;
        else
        {
            const Icon2& cur = m_icons[static_cast<size_t>(m_focus)];
            const int dc = vk == VK_LEFT ? -1 : (vk == VK_RIGHT ? 1 : 0);
            const int dr = vk == VK_UP ? -1 : (vk == VK_DOWN ? 1 : 0);
            int best = -1;
            float bestD = 1e9f;
            for (int i = 0; i < static_cast<int>(m_icons.size()); ++i)
            {
                const Icon2& o = m_icons[static_cast<size_t>(i)];
                const int ddc = o.col - cur.col, ddr = o.row - cur.row;
                if ((dc != 0 && ddc * dc <= 0) || (dr != 0 && ddr * dr <= 0))
                    continue;
                const float d = static_cast<float>(dc != 0 ? std::abs(ddc) * 10 + std::abs(ddr) : std::abs(ddr) * 10 + std::abs(ddc));
                if (d < bestD)
                {
                    bestD = d;
                    best = i;
                }
            }
            if (best >= 0)
                m_focus = best;
        }
        if (!mods.ctrl)
            ClearSelection();
        m_icons[static_cast<size_t>(m_focus)].selected = true;
        return true;
    }
    default:
        break;
    }
    if (mods.ctrl)
    {
        if (vk == 'A')
        {
            for (Icon2& ic : m_icons)
                ic.selected = true;
            return true;
        }
        if (vk == 'C' || vk == 'X')
        {
            fs.clip = SelectedNodes();
            fs.clipCut = vk == 'X';
            return true;
        }
        if (vk == 'V')
        {
            fs.Paste(fs.Desktop());
            return true;
        }
    }
    return false;
}

bool Desktop::Char(wchar_t ch)
{
    if (m_renameIndex >= 0)
        return m_ui.OnChar(ch);
    return false;
}

void Desktop::ShowBackgroundMenu(Point p, bool classic)
{
    Shell& sh = GetShell();
    ShellSettings& st = sh.Settings();
    VirtualFS& fs = sh.FS();
    std::vector<MenuItem> view = {
        MenuItem::Check(L"Крупные значки", st.desktopIconSize == 2, [this]() { GetShell().Settings().desktopIconSize = 2; Arrange(0); }, true, L"Ctrl+Shift+2"),
        MenuItem::Check(L"Обычные значки", st.desktopIconSize == 1, [this]() { GetShell().Settings().desktopIconSize = 1; Arrange(0); }, true, L"Ctrl+Shift+3"),
        MenuItem::Check(L"Мелкие значки", st.desktopIconSize == 0, [this]() { GetShell().Settings().desktopIconSize = 0; Arrange(0); }, true, L"Ctrl+Shift+4"),
        MenuItem::Sep(),
        MenuItem::Check(L"Упорядочить значки автоматически", st.autoArrange, [this]() {
            ShellSettings& s = GetShell().Settings();
            s.autoArrange = !s.autoArrange;
            if (s.autoArrange)
                Arrange(0);
        }),
        MenuItem::Check(L"Выровнять значки по сетке", st.alignToGrid, []() {
            ShellSettings& s = GetShell().Settings();
            s.alignToGrid = !s.alignToGrid;
        }),
        MenuItem::Sep(),
        MenuItem::Check(L"Отображать значки рабочего стола", st.showDesktopIcons, []() {
            ShellSettings& s = GetShell().Settings();
            s.showDesktopIcons = !s.showDesktopIcons;
        }),
    };
    std::vector<MenuItem> sort = {
        MenuItem(L"Имя", Icon::None, [this]() { Arrange(0); }),
        MenuItem(L"Размер", Icon::None, [this]() { Arrange(1); }),
        MenuItem(L"Тип элемента", Icon::None, [this]() { Arrange(2); }),
        MenuItem(L"Дата изменения", Icon::None, [this]() { Arrange(3); }),
    };
    std::vector<MenuItem> create = {
        MenuItem(L"Папку", Icon::Folder, [this]() {
            FsNode* n = GetShell().FS().NewFolder(GetShell().FS().Desktop());
            Sync();
            StartRename(n);
        }),
        MenuItem(L"Ярлык", Icon::Share, nullptr).Disabled(),
        MenuItem::Sep(),
        MenuItem(L"Текстовый документ", Icon::Documents, [this]() {
            FsNode* n = GetShell().FS().NewFile(GetShell().FS().Desktop(), L"Новый текстовый документ.txt", FileKind::Text);
            Sync();
            StartRename(n);
        }),
        MenuItem(L"Точечный рисунок", Icon::Pictures, [this]() {
            FsNode* n = GetShell().FS().NewFile(GetShell().FS().Desktop(), L"Новый точечный рисунок.bmp", FileKind::Image);
            Sync();
            StartRename(n);
        }),
    };
    std::vector<MenuItem> items;
    items.push_back(MenuItem::Sub(L"Вид", Icon::Grid, view));
    items.push_back(MenuItem::Sub(L"Сортировка", Icon::Sort, sort));
    items.push_back(MenuItem(L"Обновить", Icon::Refresh, [this]() { Refresh(); }));
    items.push_back(MenuItem::Sep());
    if (!fs.clip.empty())
        items.push_back(MenuItem(L"Вставить", Icon::Paste, []() { GetShell().FS().Paste(GetShell().FS().Desktop()); }, L"Ctrl+V"));
    items.push_back(MenuItem::Sub(L"Создать", Icon::Add, create));
    items.push_back(MenuItem::Sep());
    items.push_back(MenuItem(L"Параметры экрана", Icon::Display, []() { GetShell().Launch(AppId::Settings, L"display"); }));
    items.push_back(MenuItem(L"Персонализация", Icon::Personalize, []() { GetShell().Launch(AppId::Settings, L"personalization"); }));
    items.push_back(MenuItem::Sep());
    items.push_back(MenuItem(L"Открыть в Терминале", Icon::Terminal, []() {
        GetShell().Launch(AppId::Terminal, GetShell().FS().PathOf(GetShell().FS().Desktop()));
    }));
    if (!classic)
    {
        items.push_back(MenuItem::Sep());
        items.push_back(MenuItem(L"Показать дополнительные параметры", Icon::OpenWith, [this, p]() { ShowBackgroundMenu(p, true); },
                                 L"Shift+F10"));
    }
    else
    {
        for (MenuItem& m : items)
            m.icon = Icon::None;
    }
    sh.ShowMenu(p, std::move(items), {}, classic);
}

void Desktop::ShowIconMenu(int index, Point p)
{
    Shell& sh = GetShell();
    const Icon2 ic = m_icons[static_cast<size_t>(index)];
    std::vector<MenuItem> items;
    std::vector<MenuAction> top;
    if (ic.type == Icon2::Type::ThisPC)
    {
        items.push_back(MenuItem(L"Открыть", Icon::Open, [this, index]() { Open(index); }, L"Enter"));
        items.push_back(MenuItem(L"Закрепить на начальном экране", Icon::Pin, nullptr).Disabled());
        items.push_back(MenuItem::Sep());
        items.push_back(MenuItem(L"Свойства", Icon::Properties, []() { GetShell().Launch(AppId::Settings, L"about"); }, L"Alt+Enter"));
    }
    else if (ic.type == Icon2::Type::RecycleBin)
    {
        const bool empty = sh.FS().Bin()->children.empty();
        items.push_back(MenuItem(L"Открыть", Icon::Open, [this, index]() { Open(index); }, L"Enter"));
        items.push_back(MenuItem(L"Очистить корзину", Icon::Delete, []() { GetShell().FS().EmptyBin(); }).Disabled(empty));
        items.push_back(MenuItem::Sep());
        items.push_back(MenuItem(L"Свойства", Icon::Properties, []() { GetShell().ShowProperties(GetShell().FS().Bin()); }, L"Alt+Enter"));
    }
    else
    {
        FsNode* node = ic.node;
        top.push_back({Icon::Cut, L"Вырезать", [this]() {
                           GetShell().FS().clip = SelectedNodes();
                           GetShell().FS().clipCut = true;
                       }});
        top.push_back({Icon::Copy, L"Копировать", [this]() {
                           GetShell().FS().clip = SelectedNodes();
                           GetShell().FS().clipCut = false;
                       }});
        top.push_back({Icon::Rename, L"Переименовать", [this, node]() { StartRename(node); }});
        top.push_back({Icon::Share, L"Поделиться", []() {
                           GetShell().ShowMessage(L"Поделиться", L"Функция «Поделиться» недоступна в этой демонстрации.");
                       }});
        top.push_back({Icon::Delete, L"Удалить", [this]() { DeleteSelected(); }});
        items.push_back(MenuItem(L"Открыть", Icon::Open, [this, index]() { Open(index); }, L"Enter"));
        if (node->kind == FileKind::Text)
            items.push_back(MenuItem(L"Изменить в Блокноте", Icon::Edit, [node]() { GetShell().Launch(AppId::Notepad, GetShell().FS().PathOf(node)); }));
        if (node->IsFolder())
            items.push_back(MenuItem(L"Открыть в Терминале", Icon::Terminal, [node]() { GetShell().Launch(AppId::Terminal, GetShell().FS().PathOf(node)); }));
        items.push_back(MenuItem::Sep());
        items.push_back(MenuItem(L"Копировать как путь", Icon::CopyPath, [node]() {
            SetClipboardText(GetShell().Hwnd(), L"\"" + GetShell().FS().PathOf(node) + L"\"");
        }, L"Ctrl+Shift+C"));
        items.push_back(MenuItem(L"Свойства", Icon::Properties, [node]() { GetShell().ShowProperties(node); }, L"Alt+Enter"));
    }
    sh.ShowMenu(p, std::move(items), std::move(top));
}
