// Paint Windows 11: лента инструментов (выделение, обрезка, изменение размера, поворот, карандаш,
// заливка, текст, ластик, пипетка, лупа, кисти, фигуры с контуром и заливкой, толщина, палитра),
// холст с масштабом и маркерами изменения размера, перемещаемое выделение, буфер обмена с
// изображениями (вставка снимков экрана), отмена/повтор, открытие и сохранение в ФС.

#include "../Shell.h"
#include "AppFactories.h"
#include "Photos.h"

namespace
{
constexpr float kTitleH = 44.0f;
constexpr float kRibbonH = 96.0f;
constexpr float kStatusH = 32.0f;
constexpr int kDefaultW = 960; // холст по умолчанию помещается в окно при 100%
constexpr int kDefaultH = 540;

enum class Tool
{
    Select,
    Pencil,
    Fill,
    Text,
    Eraser,
    Picker,
    Zoom,
    Brush,
    Shape,
};

enum class BrushKind
{
    Round,
    Calligraphy,
    Airbrush,
    Marker,
};

enum class ShapeKind
{
    Line,
    Ellipse,
    Rect,
    RoundRect,
    Triangle,
    RightTriangle,
    Diamond,
    Pentagon,
    Hexagon,
    ArrowRight,
    ArrowLeft,
    ArrowUp,
    ArrowDown,
    Star4,
    Star5,
    Star6,
    Heart,
    Lightning,
    Count,
};

const wchar_t* ShapeName(ShapeKind k)
{
    static const wchar_t* names[] = {L"Линия",       L"Овал",           L"Прямоугольник", L"Скруглённый прямоугольник",
                                     L"Треугольник", L"Прямоугольный треугольник", L"Ромб", L"Пятиугольник",
                                     L"Шестиугольник", L"Стрелка вправо", L"Стрелка влево", L"Стрелка вверх",
                                     L"Стрелка вниз", L"Четырёхконечная звезда", L"Пятиконечная звезда", L"Шестиконечная звезда",
                                     L"Сердце", L"Молния"};
    return names[static_cast<int>(k)];
}

// Контур фигуры, вписанной в прямоугольник (используется и для значков, и для рисования).
void ShapePath(Gdiplus::GraphicsPath& path, ShapeKind k, const Gdiplus::RectF& rc)
{
    const float l = rc.X, t = rc.Y, w = rc.Width, h = rc.Height, rgt = l + w, b = t + h, cx = l + w * 0.5f, cy = t + h * 0.5f;
    auto poly = [&](std::initializer_list<Gdiplus::PointF> pts) {
        std::vector<Gdiplus::PointF> v(pts);
        path.AddPolygon(v.data(), static_cast<INT>(v.size()));
    };
    auto star = [&](int n, float inner) {
        std::vector<Gdiplus::PointF> v;
        for (int i = 0; i < n * 2; ++i)
        {
            const float a = -kPi * 0.5f + static_cast<float>(i) * kPi / static_cast<float>(n);
            const float rr = (i % 2 == 0) ? 1.0f : inner;
            v.push_back({cx + std::cos(a) * w * 0.5f * rr, cy + std::sin(a) * h * 0.5f * rr});
        }
        path.AddPolygon(v.data(), static_cast<INT>(v.size()));
    };
    auto regular = [&](int n, float start) {
        std::vector<Gdiplus::PointF> v;
        for (int i = 0; i < n; ++i)
        {
            const float a = start + static_cast<float>(i) * 2.0f * kPi / static_cast<float>(n);
            v.push_back({cx + std::cos(a) * w * 0.5f, cy + std::sin(a) * h * 0.5f});
        }
        path.AddPolygon(v.data(), static_cast<INT>(v.size()));
    };
    switch (k)
    {
    case ShapeKind::Line:
        path.AddLine(l, t, rgt, b);
        break;
    case ShapeKind::Ellipse:
        path.AddEllipse(rc);
        break;
    case ShapeKind::Rect:
        path.AddRectangle(rc);
        break;
    case ShapeKind::RoundRect:
        Renderer::RoundRectPath(path, {l, t, w, h}, std::min(w, h) * 0.18f);
        break;
    case ShapeKind::Triangle:
        poly({{cx, t}, {rgt, b}, {l, b}});
        break;
    case ShapeKind::RightTriangle:
        poly({{l, t}, {rgt, b}, {l, b}});
        break;
    case ShapeKind::Diamond:
        poly({{cx, t}, {rgt, cy}, {cx, b}, {l, cy}});
        break;
    case ShapeKind::Pentagon:
        regular(5, -kPi * 0.5f);
        break;
    case ShapeKind::Hexagon:
        regular(6, 0.0f);
        break;
    case ShapeKind::ArrowRight:
        poly({{l, t + h * 0.3f}, {l + w * 0.55f, t + h * 0.3f}, {l + w * 0.55f, t}, {rgt, cy}, {l + w * 0.55f, b}, {l + w * 0.55f, t + h * 0.7f}, {l, t + h * 0.7f}});
        break;
    case ShapeKind::ArrowLeft:
        poly({{rgt, t + h * 0.3f}, {l + w * 0.45f, t + h * 0.3f}, {l + w * 0.45f, t}, {l, cy}, {l + w * 0.45f, b}, {l + w * 0.45f, t + h * 0.7f}, {rgt, t + h * 0.7f}});
        break;
    case ShapeKind::ArrowUp:
        poly({{l + w * 0.3f, b}, {l + w * 0.3f, t + h * 0.45f}, {l, t + h * 0.45f}, {cx, t}, {rgt, t + h * 0.45f}, {l + w * 0.7f, t + h * 0.45f}, {l + w * 0.7f, b}});
        break;
    case ShapeKind::ArrowDown:
        poly({{l + w * 0.3f, t}, {l + w * 0.3f, t + h * 0.55f}, {l, t + h * 0.55f}, {cx, b}, {rgt, t + h * 0.55f}, {l + w * 0.7f, t + h * 0.55f}, {l + w * 0.7f, t}});
        break;
    case ShapeKind::Star4:
        star(4, 0.38f);
        break;
    case ShapeKind::Star5:
        star(5, 0.4f);
        break;
    case ShapeKind::Star6:
        star(6, 0.55f);
        break;
    case ShapeKind::Heart:
        path.StartFigure();
        path.AddBezier(cx, t + h * 0.28f, cx, t - h * 0.05f, l - w * 0.02f, t, l, t + h * 0.32f);
        path.AddBezier(l, t + h * 0.32f, l, t + h * 0.62f, cx - w * 0.1f, t + h * 0.78f, cx, b);
        path.AddBezier(cx, b, cx + w * 0.1f, t + h * 0.78f, rgt, t + h * 0.62f, rgt, t + h * 0.32f);
        path.AddBezier(rgt, t + h * 0.32f, rgt + w * 0.02f, t, cx, t - h * 0.05f, cx, t + h * 0.28f);
        path.CloseFigure();
        break;
    case ShapeKind::Lightning:
        poly({{l + w * 0.38f, t}, {l + w * 0.62f, t + h * 0.3f}, {l + w * 0.52f, t + h * 0.36f}, {l + w * 0.8f, t + h * 0.62f},
              {l + w * 0.68f, t + h * 0.68f}, {rgt, b}, {l + w * 0.4f, t + h * 0.76f}, {l + w * 0.52f, t + h * 0.7f},
              {l + w * 0.2f, t + h * 0.46f}, {l + w * 0.32f, t + h * 0.4f}, {l, t + h * 0.12f}});
        break;
    case ShapeKind::Count:
        break;
    }
}

// Палитра Paint (2 ряда по 10).
const Color kPalette[20] = {
    Color::Hex(0x000000), Color::Hex(0x7F7F7F), Color::Hex(0x880015), Color::Hex(0xED1C24), Color::Hex(0xFF7F27),
    Color::Hex(0xFFF200), Color::Hex(0x22B14C), Color::Hex(0x00A2E8), Color::Hex(0x3F48CC), Color::Hex(0xA349A4),
    Color::Hex(0xFFFFFF), Color::Hex(0xC3C3C3), Color::Hex(0xB97A57), Color::Hex(0xFFAEC9), Color::Hex(0xFFC90E),
    Color::Hex(0xEFE4B0), Color::Hex(0xB5E61D), Color::Hex(0x99D9EA), Color::Hex(0x7092BE), Color::Hex(0xC8BFE7),
};

Gdiplus::Color GpColor(Color c) { return Gdiplus::Color(c.a, c.r, c.g, c.b); }

uint8_t UnitToByte(float v) { return static_cast<uint8_t>(Clamp(v * 255.0f + 0.5f, 0.0f, 255.0f)); }

// h, s, v в диапазоне 0..1.
Color HsvToColor(float h, float s, float v)
{
    const float hh = (h - std::floor(h)) * 6.0f;
    const int sector = static_cast<int>(hh) % 6;
    const float f = hh - std::floor(hh);
    const float p = v * (1.0f - s), q = v * (1.0f - s * f), t = v * (1.0f - s * (1.0f - f));
    float r = v, g = t, b = p;
    switch (sector)
    {
    case 1:
        r = q;
        g = v;
        b = p;
        break;
    case 2:
        r = p;
        g = v;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = v;
        break;
    case 4:
        r = t;
        g = p;
        b = v;
        break;
    case 5:
        r = v;
        g = p;
        b = q;
        break;
    default:
        break;
    }
    return Color(UnitToByte(r), UnitToByte(g), UnitToByte(b));
}

// Оттенок не меняется для серых цветов (h остаётся прежним).
void ColorToHsv(Color c, float& h, float& s, float& v)
{
    const float r = static_cast<float>(c.r) / 255.0f, g = static_cast<float>(c.g) / 255.0f, b = static_cast<float>(c.b) / 255.0f;
    const float mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn;
    v = mx;
    s = mx > 0.0f ? d / mx : 0.0f;
    if (d <= 0.0f)
        return;
    float hh;
    if (mx == r)
        hh = (g - b) / d + (g < b ? 6.0f : 0.0f);
    else if (mx == g)
        hh = (b - r) / d + 2.0f;
    else
        hh = (r - g) / d + 4.0f;
    h = hh / 6.0f;
}

std::wstring HexText(Color c)
{
    static const wchar_t* digits = L"0123456789ABCDEF";
    std::wstring out = L"#";
    for (uint8_t v : {c.r, c.g, c.b})
    {
        out += digits[v >> 4];
        out += digits[v & 15];
    }
    return out;
}

bool ParseHex(const std::wstring& text, Color& out)
{
    std::wstring hex = Trim(text);
    if (!hex.empty() && hex[0] == L'#')
        hex.erase(0, 1);
    if (hex.size() != 6 || hex.find_first_not_of(L"0123456789abcdefABCDEF") != std::wstring::npos)
        return false;
    out = Color::Hex(static_cast<uint32_t>(std::wcstoul(hex.c_str(), nullptr, 16)));
    return true;
}

void InvertRegion(Gdiplus::Bitmap* bmp, int x, int y, int w, int h)
{
    if (!bmp || w <= 0 || h <= 0)
        return;
    Gdiplus::BitmapData data{};
    Gdiplus::Rect rc(x, y, w, h);
    if (bmp->LockBits(&rc, Gdiplus::ImageLockModeRead | Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &data) != Gdiplus::Ok)
        return;
    for (int yy = 0; yy < h; ++yy)
    {
        auto* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(data.Scan0) + static_cast<ptrdiff_t>(yy) * data.Stride);
        for (int xx = 0; xx < w; ++xx)
            row[xx] ^= 0x00FFFFFFu;
    }
    bmp->UnlockBits(&data);
}

// Спектр «оттенок × насыщенность» при максимальной яркости.
Gdiplus::Bitmap* SpectrumBitmap()
{
    static std::unique_ptr<Gdiplus::Bitmap> bmp;
    if (bmp)
        return bmp.get();
    const int w = 256, h = 160;
    bmp = std::make_unique<Gdiplus::Bitmap>(w, h, PixelFormat32bppARGB);
    Gdiplus::BitmapData data{};
    Gdiplus::Rect rc(0, 0, w, h);
    if (bmp->LockBits(&rc, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &data) == Gdiplus::Ok)
    {
        for (int y = 0; y < h; ++y)
        {
            auto* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(data.Scan0) + static_cast<ptrdiff_t>(y) * data.Stride);
            for (int x = 0; x < w; ++x)
            {
                const Color c = HsvToColor(static_cast<float>(x) / static_cast<float>(w - 1), 1.0f - static_cast<float>(y) / static_cast<float>(h - 1), 1.0f);
                row[x] = 0xFF000000u | (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b;
            }
        }
        bmp->UnlockBits(&data);
    }
    return bmp.get();
}

std::unique_ptr<Gdiplus::Bitmap> NewCanvas(int w, int h, Color fill)
{
    auto bmp = std::make_unique<Gdiplus::Bitmap>(w, h, PixelFormat32bppARGB);
    Gdiplus::Graphics g(bmp.get());
    g.Clear(GpColor(fill));
    return bmp;
}

std::unique_ptr<Gdiplus::Bitmap> CloneBitmap(Gdiplus::Bitmap* src)
{
    if (!src)
        return nullptr;
    const INT w = static_cast<INT>(src->GetWidth()), h = static_cast<INT>(src->GetHeight());
    auto bmp = std::make_unique<Gdiplus::Bitmap>(w, h, PixelFormat32bppARGB);
    Gdiplus::Graphics g(bmp.get());
    g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    g.DrawImage(src, 0, 0, w, h);
    return bmp;
}

// Изображение в системный буфер обмена (CF_DIB, 32 бита).
void CopyBitmapToClipboard(HWND hwnd, Gdiplus::Bitmap* bmp)
{
    if (!bmp || !OpenClipboard(hwnd))
        return;
    EmptyClipboard();
    const int w = static_cast<int>(bmp->GetWidth()), h = static_cast<int>(bmp->GetHeight());
    Gdiplus::BitmapData data{};
    Gdiplus::Rect rc(0, 0, w, h);
    if (bmp->LockBits(&rc, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) == Gdiplus::Ok)
    {
        const size_t bytes = sizeof(BITMAPINFOHEADER) + static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (mem)
        {
            if (auto* bi = static_cast<BITMAPINFOHEADER*>(GlobalLock(mem)))
            {
                *bi = BITMAPINFOHEADER{};
                bi->biSize = sizeof(BITMAPINFOHEADER);
                bi->biWidth = w;
                bi->biHeight = h;
                bi->biPlanes = 1;
                bi->biBitCount = 32;
                bi->biCompression = BI_RGB;
                auto* dst = reinterpret_cast<uint8_t*>(bi + 1);
                for (int y = 0; y < h; ++y)
                    std::memcpy(dst + static_cast<size_t>(h - 1 - y) * static_cast<size_t>(w) * 4,
                                static_cast<const uint8_t*>(data.Scan0) + static_cast<ptrdiff_t>(y) * data.Stride, static_cast<size_t>(w) * 4);
                GlobalUnlock(mem);
                if (!SetClipboardData(CF_DIB, mem))
                    GlobalFree(mem);
            }
            else
                GlobalFree(mem);
        }
        bmp->UnlockBits(&data);
    }
    CloseClipboard();
}

// Изображение из буфера обмена (CF_DIB 24/32 бита).
std::unique_ptr<Gdiplus::Bitmap> BitmapFromClipboard(HWND hwnd)
{
    if (!IsClipboardFormatAvailable(CF_DIB) || !OpenClipboard(hwnd))
        return nullptr;
    std::unique_ptr<Gdiplus::Bitmap> out;
    if (HANDLE hdata = GetClipboardData(CF_DIB))
    {
        if (const auto* bi = static_cast<const BITMAPINFOHEADER*>(GlobalLock(hdata)))
        {
            const int w = bi->biWidth;
            const int hh = bi->biHeight < 0 ? -bi->biHeight : bi->biHeight;
            const bool bottomUp = bi->biHeight > 0;
            const int bpp = bi->biBitCount;
            if (w > 0 && hh > 0 && w <= 8192 && hh <= 8192 && (bpp == 24 || bpp == 32) && bi->biCompression == BI_RGB)
            {
                const uint8_t* bits = reinterpret_cast<const uint8_t*>(bi) + bi->biSize + bi->biClrUsed * 4;
                const size_t stride = ((static_cast<size_t>(w) * static_cast<size_t>(bpp) + 31) / 32) * 4;
                out = std::make_unique<Gdiplus::Bitmap>(w, hh, PixelFormat32bppARGB);
                Gdiplus::BitmapData data{};
                Gdiplus::Rect rc(0, 0, w, hh);
                if (out->LockBits(&rc, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &data) == Gdiplus::Ok)
                {
                    for (int y = 0; y < hh; ++y)
                    {
                        const uint8_t* src = bits + stride * static_cast<size_t>(bottomUp ? hh - 1 - y : y);
                        auto* dst = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(data.Scan0) + static_cast<ptrdiff_t>(y) * data.Stride);
                        for (int x = 0; x < w; ++x)
                        {
                            const uint8_t* px = src + static_cast<size_t>(x) * static_cast<size_t>(bpp / 8);
                            dst[x] = 0xFF000000u | (static_cast<uint32_t>(px[2]) << 16) | (static_cast<uint32_t>(px[1]) << 8) | px[0];
                        }
                    }
                    out->UnlockBits(&data);
                }
            }
            GlobalUnlock(hdata);
        }
    }
    CloseClipboard();
    return out;
}

class PaintWindow : public Window
{
public:
    explicit PaintWindow(const std::wstring& arg) : Window(AppId::Paint, L"Paint")
    {
        SetInitialSize(1180.0f, 760.0f);
        m_minSize = {720.0f, 480.0f};
        m_canvas = NewCanvas(kDefaultW, kDefaultH, Color(255, 255, 255));
        if (FsNode* n = GetShell().FS().FromArg(arg))
        {
            if (n->kind == FileKind::Image)
                LoadNode(n);
        }
        auto menu = [&](const wchar_t* text, int which) {
            auto* b = m_ui.Add<Button>(text, Icon::None, ButtonStyle::Subtle);
            b->fontSize = 14.0f;
            b->onClick = [this, b, which]() { ShowMainMenu(which, LocalToScreen(b->Bounds())); };
            m_menus.push_back(b);
        };
        menu(L"Файл", 0);
        menu(L"Правка", 1);
        menu(L"Вид", 2);
        m_undoBtn = m_ui.Add<Button>(L"", Icon::Undo, ButtonStyle::Subtle);
        m_undoBtn->SetTooltip(L"Отменить (Ctrl+Z)");
        m_undoBtn->onClick = [this]() { Undo(); };
        m_redoBtn = m_ui.Add<Button>(L"", Icon::Redo, ButtonStyle::Subtle);
        m_redoBtn->SetTooltip(L"Вернуть (Ctrl+Y)");
        m_redoBtn->onClick = [this]() { Redo(); };
        m_text = m_ui.Add<TextBox>();
        m_text->borderless = true;
        m_text->SetVisible(false);
        m_text->onEnter = [this]() { CommitText(); };
        m_text->onEscape = [this]() { CancelText(); };
        m_hexBox = m_ui.Add<TextBox>();
        m_hexBox->SetVisible(false);
        m_hexBox->onChange = [this](const std::wstring& t) {
            Color c;
            if (ParseHex(t, c))
                ColorToHsv(c, m_pickH, m_pickS, m_pickV);
        };
        m_hexBox->onEnter = [this]() { ClosePicker(true); };
        m_hexBox->onEscape = [this]() { ClosePicker(false); };
        m_pickOk = m_ui.Add<Button>(L"ОК", Icon::None, ButtonStyle::Accent);
        m_pickOk->SetVisible(false);
        m_pickOk->onClick = [this]() { ClosePicker(true); };
        m_pickCancel = m_ui.Add<Button>(L"Отмена", Icon::None, ButtonStyle::Standard);
        m_pickCancel->SetVisible(false);
        m_pickCancel->onClick = [this]() { ClosePicker(false); };
        UpdateTitle();
    }

    float TitleBarHeight() const override { return kTitleH; }
    bool ShowTitle() const override { return false; }

    bool OnClose() override
    {
        if (!m_dirty)
            return true;
        ShowDialog(L"Paint", L"Вы хотите сохранить изменения в «" + DocName() + L"»?", {{L"Сохранить", true}, {L"Не сохранять", false}, {L"Отмена", false}},
                   [this](int r) {
                       if (r == 1)
                       {
                           m_dirty = false;
                           PostAction([this]() { Close(); });
                       }
                       else if (r == 0)
                           Save([this]() { Close(); });
                   });
        return false;
    }

private:
    // ================================================================== документ
    std::wstring DocName() const { return m_node ? VirtualFS::DisplayName(m_node, true) : L"Без имени"; }
    void UpdateTitle() { SetTitle(DocName() + L" — Paint"); }

    void LoadNode(FsNode* n)
    {
        std::shared_ptr<Gdiplus::Bitmap> src = NodeBitmap(n);
        if (!src)
            return;
        m_canvas = CloneBitmap(src.get());
        m_node = n;
        m_undo.clear();
        m_redo.clear();
        m_dirty = false;
        m_zoom = 0.0f; // «по размеру окна» при первой отрисовке
        UpdateTitle();
    }

    void NewDocument()
    {
        auto doNew = [this]() {
            m_canvas = NewCanvas(kDefaultW, kDefaultH, Color(255, 255, 255));
            m_node = nullptr;
            m_undo.clear();
            m_redo.clear();
            m_dirty = false;
            m_zoom = 1.0f;
            m_scroll = {};
            ClearSelection();
            UpdateTitle();
        };
        if (!m_dirty)
        {
            doNew();
            return;
        }
        ShowDialog(L"Paint", L"Вы хотите сохранить изменения в «" + DocName() + L"»?", {{L"Сохранить", true}, {L"Не сохранять", false}, {L"Отмена", false}},
                   [this, doNew](int r) {
                       if (r == 1)
                           doNew();
                       else if (r == 0)
                           Save(doNew);
                   });
    }

    void Save(std::function<void()> then)
    {
        CommitFloat();
        CommitText();
        if (m_node && GetShell().FS().IsAlive(m_node))
        {
            WriteNode(m_node);
            if (then)
                PostAction(then);
            return;
        }
        SaveAs(std::move(then));
    }

    void SaveAs(std::function<void()> then)
    {
        CommitFloat();
        ShowInputDialog(L"Сохранить как", L"Изображение будет сохранено в папке «Изображения».", m_node ? m_node->name : L"Без имени.png",
                        L"Сохранить", [this, then](const std::wstring& raw) {
                            VirtualFS& fs = GetShell().FS();
                            std::wstring name = Trim(raw);
                            if (name.empty() || !VirtualFS::ValidName(name))
                                return;
                            std::wstring ext;
                            VirtualFS::SplitExtension(name, ext);
                            if (VirtualFS::KindFor(name) != FileKind::Image)
                                name += L".png";
                            FsNode* n = fs.Find(fs.Pictures(), name);
                            if (n && n->IsFolder())
                                return;
                            if (!n)
                                n = fs.NewFile(fs.Pictures(), name, FileKind::Image);
                            WriteNode(n);
                            if (then)
                                PostAction(then);
                        });
    }

    void WriteNode(FsNode* n)
    {
        n->bitmap = std::shared_ptr<Gdiplus::Bitmap>(CloneBitmap(m_canvas.get()).release());
        n->size = static_cast<unsigned long long>(m_canvas->GetWidth()) * m_canvas->GetHeight() / 3ULL + 2048ULL;
        n->modified = LocalNow();
        n->kind = FileKind::Image;
        GetShell().FS().Touch();
        m_node = n;
        m_dirty = false;
        UpdateTitle();
    }

    // ================================================================== отмена
    void PushUndo()
    {
        m_undo.push_back(CloneBitmap(m_canvas.get()));
        if (m_undo.size() > 30)
            m_undo.erase(m_undo.begin());
        m_redo.clear();
        m_dirty = true;
    }

    void Undo()
    {
        if (m_float)
        {
            CancelFloat();
            return;
        }
        if (m_undo.empty())
            return;
        m_redo.push_back(std::move(m_canvas));
        m_canvas = std::move(m_undo.back());
        m_undo.pop_back();
        m_dirty = true;
        ClearSelection();
    }

    void Redo()
    {
        if (m_redo.empty())
            return;
        m_undo.push_back(std::move(m_canvas));
        m_canvas = std::move(m_redo.back());
        m_redo.pop_back();
        m_dirty = true;
        ClearSelection();
    }

    // ================================================================== геометрия
    int CW() const { return static_cast<int>(m_canvas->GetWidth()); }
    int CH() const { return static_cast<int>(m_canvas->GetHeight()); }
    Rect ViewRect() const
    {
        const float sb = m_statusBar ? kStatusH : 0.0f;
        return {0.0f, kTitleH + kRibbonH, Width(), Height() - kTitleH - kRibbonH - sb};
    }

    void FitZoom()
    {
        const Rect v = ViewRect();
        const float z = std::min((v.w - 48.0f) / static_cast<float>(CW()), (v.h - 48.0f) / static_cast<float>(CH()));
        m_zoom = Clamp(z, 0.125f, 1.0f);
        m_scroll = {};
    }

    Rect CanvasRect() const
    {
        const Rect v = ViewRect();
        const float w = static_cast<float>(CW()) * m_zoom, h = static_cast<float>(CH()) * m_zoom;
        const float x = w + 48.0f < v.w ? std::floor(v.x + (v.w - w) * 0.5f) : v.x + 24.0f - m_scroll.x;
        const float y = h + 48.0f < v.h ? std::floor(v.y + (v.h - h) * 0.5f) : v.y + 24.0f - m_scroll.y;
        return {x, y, w, h};
    }

    Point ToCanvas(Point local) const
    {
        const Rect c = CanvasRect();
        return {(local.x - c.x) / m_zoom, (local.y - c.y) / m_zoom};
    }

    void ClampScroll()
    {
        const Rect v = ViewRect();
        const float w = static_cast<float>(CW()) * m_zoom + 48.0f, h = static_cast<float>(CH()) * m_zoom + 48.0f;
        m_scroll.x = Clamp(m_scroll.x, 0.0f, std::max(0.0f, w - v.w));
        m_scroll.y = Clamp(m_scroll.y, 0.0f, std::max(0.0f, h - v.h));
    }

    void SetZoom(float z, Point anchorLocal)
    {
        const Point before = ToCanvas(anchorLocal);
        m_zoom = Clamp(z, 0.125f, 8.0f);
        const Rect c = CanvasRect();
        // Сохранить точку под курсором на месте.
        m_scroll.x += c.x + before.x * m_zoom - anchorLocal.x;
        m_scroll.y += c.y + before.y * m_zoom - anchorLocal.y;
        ClampScroll();
    }

    void ZoomStep(int dir)
    {
        static const float steps[] = {0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f};
        int idx = 0;
        for (int i = 0; i < 11; ++i)
        {
            if (steps[i] <= m_zoom + 0.001f)
                idx = i;
        }
        if (dir < 0 && steps[idx] < m_zoom - 0.001f)
            idx = idx + 1;
        idx = Clamp(idx + dir, 0, 10);
        const Rect v = ViewRect();
        SetZoom(steps[idx], v.Center());
    }

    // ================================================================== лента
    struct Tb
    {
        Rect r;
        Icon icon = Icon::None;
        std::wstring tip;
        std::function<void()> action;
        std::function<bool()> isActive;
        bool dropdown = false;
        int shape = -1;
        bool big = false;
        int custom = 0; // 1 — значок «Изменить размер»
        std::wstring label;
    };

    void LayoutRibbon()
    {
        m_tbs.clear();
        m_tbs.reserve(48);
        m_groups.clear();
        const float y0 = kTitleH + 6.0f;
        float x = 12.0f;
        auto group = [&](const wchar_t* name, float w) {
            m_groups.push_back({{x, y0, w, kRibbonH - 12.0f}, name});
            x += w + 9.0f;
        };
        // Указатель на добавленную кнопку действителен до следующего вызова tb().
        auto tb = [&](float bx, float by, float w, float h, Icon icon, const wchar_t* tip, std::function<void()> fn,
                      std::function<bool()> active = nullptr) {
            Tb t;
            t.r = {bx, by, w, h};
            t.icon = icon;
            t.tip = tip;
            t.action = std::move(fn);
            t.isActive = std::move(active);
            m_tbs.push_back(std::move(t));
            return &m_tbs.back();
        };
        // Выделение
        {
            Tb* sel = tb(x + 8.0f, y0 + 4.0f, 48.0f, 50.0f, Icon::Select, L"Выделение (выделить прямоугольную область)",
                         [this]() { SetTool(Tool::Select); }, [this]() { return m_tool == Tool::Select; });
            sel->big = true;
            sel->dropdown = true;
            group(L"Выделение", 64.0f);
        }
        // Изображение
        {
            tb(x + 2.0f, y0 + 2.0f, 34.0f, 28.0f, Icon::Crop, L"Обрезать (Ctrl+Shift+X)", [this]() { Crop(); });
            Tb* rs = tb(x + 38.0f, y0 + 2.0f, 34.0f, 28.0f, Icon::None, L"Изменить размер (Ctrl+W)", [this]() { ResizeDialog(); });
            rs->custom = 1;
            Tb* rot = tb(x + 2.0f, y0 + 32.0f, 70.0f, 28.0f, Icon::RotateRight, L"Повернуть", [this]() { ShowRotateMenu(); });
            rot->dropdown = true;
            group(L"Изображение", 74.0f);
        }
        // Инструменты
        {
            const struct
            {
                Tool tool;
                Icon icon;
                const wchar_t* tip;
            } tools[] = {{Tool::Pencil, Icon::Pencil, L"Карандаш"}, {Tool::Fill, Icon::Fill, L"Заливка"},
                         {Tool::Text, Icon::TextTool, L"Текст"},     {Tool::Eraser, Icon::Eraser, L"Ластик"},
                         {Tool::Picker, Icon::Picker, L"Палитра (выбор цвета)"}, {Tool::Zoom, Icon::ZoomIn, L"Лупа"}};
            for (int i = 0; i < 6; ++i)
            {
                const Tool t = tools[i].tool;
                tb(x + 2.0f + static_cast<float>(i % 3) * 34.0f, y0 + 2.0f + static_cast<float>(i / 3) * 30.0f, 32.0f, 28.0f, tools[i].icon, tools[i].tip,
                   [this, t]() { SetTool(t); }, [this, t]() { return m_tool == t; });
            }
            group(L"Инструменты", 104.0f);
        }
        // Кисти
        {
            Tb* br = tb(x + 4.0f, y0 + 4.0f, 48.0f, 50.0f, Icon::Brush, L"Кисти", [this]() {
                SetTool(Tool::Brush);
                ShowBrushMenu();
            }, [this]() { return m_tool == Tool::Brush; });
            br->big = true;
            br->dropdown = true;
            group(L"Кисти", 56.0f);
        }
        // Фигуры
        {
            const int cols = 6;
            for (int i = 0; i < static_cast<int>(ShapeKind::Count); ++i)
            {
                const ShapeKind k = static_cast<ShapeKind>(i);
                Tb* s = tb(x + 2.0f + static_cast<float>(i % cols) * 25.0f, y0 + 2.0f + static_cast<float>(i / cols) * 20.0f, 24.0f, 19.0f, Icon::None,
                           ShapeName(k), [this, k]() {
                               m_shape = k;
                               SetTool(Tool::Shape);
                           },
                           [this, k]() { return m_tool == Tool::Shape && m_shape == k; });
                s->shape = i;
            }
            const float sx = x + 2.0f + static_cast<float>(cols) * 25.0f + 6.0f;
            Tb* outline = tb(sx, y0 + 2.0f, 92.0f, 28.0f, Icon::Line, L"Контур фигуры", [this]() { ShowOutlineMenu(); });
            outline->dropdown = true;
            outline->label = L"Контур";
            Tb* fill = tb(sx, y0 + 32.0f, 92.0f, 28.0f, Icon::Fill, L"Заливка фигуры", [this]() { ShowFillMenu(); });
            fill->dropdown = true;
            fill->label = L"Заливка";
            group(L"Фигуры", static_cast<float>(cols) * 25.0f + 104.0f);
        }
        // Размер
        {
            Tb* sz = tb(x + 4.0f, y0 + 4.0f, 48.0f, 50.0f, Icon::Hamburger, L"Размер (толщина линии)", [this]() { ShowSizeMenu(); });
            sz->big = true;
            sz->dropdown = true;
            group(L"Размер", 56.0f);
        }
        // Цвета
        {
            m_colorsX = x;
            group(L"Цвета", 64.0f + 10.0f * 22.0f + 46.0f);
        }
    }

    Rect PrimaryRect() const { return {m_colorsX + 6.0f, kTitleH + 12.0f, 30.0f, 30.0f}; }
    Rect SecondaryRect() const { return {m_colorsX + 28.0f, kTitleH + 34.0f, 24.0f, 24.0f}; }
    Rect SwatchRect(int i) const
    {
        return {m_colorsX + 66.0f + static_cast<float>(i % 10) * 22.0f, kTitleH + 8.0f + static_cast<float>(i / 10) * 20.0f, 20.0f, 20.0f};
    }
    Rect CustomSwatchRect(int i) const { return {m_colorsX + 66.0f + static_cast<float>(i) * 22.0f, kTitleH + 48.0f, 20.0f, 20.0f}; }
    Rect EditColorsRect() const { return {m_colorsX + 66.0f + 10.0f * 22.0f + 6.0f, kTitleH + 8.0f, 40.0f, 60.0f}; }

    void SetTool(Tool t)
    {
        if (t == Tool::Picker && m_tool != Tool::Picker)
            m_prevTool = m_tool;
        CommitText();
        if (t != Tool::Select)
        {
            CommitFloat();
            ClearSelection();
        }
        m_tool = t;
    }

    void ShowBrushMenu()
    {
        auto pick = [this](BrushKind b) {
            return [this, b]() {
                m_brush = b;
                SetTool(Tool::Brush);
            };
        };
        std::vector<MenuItem> items = {
            MenuItem::Check(L"Кисть", m_brush == BrushKind::Round, pick(BrushKind::Round), true),
            MenuItem::Check(L"Каллиграфическая кисть", m_brush == BrushKind::Calligraphy, pick(BrushKind::Calligraphy), true),
            MenuItem::Check(L"Аэрограф", m_brush == BrushKind::Airbrush, pick(BrushKind::Airbrush), true),
            MenuItem::Check(L"Маркер", m_brush == BrushKind::Marker, pick(BrushKind::Marker), true),
        };
        GetShell().ShowMenuBelow(LocalToScreen(m_menuAnchor), std::move(items), 220.0f);
    }

    void ShowSizeMenu()
    {
        std::vector<MenuItem> items;
        for (int s : {1, 3, 5, 8, 12, 20, 32})
            items.push_back(MenuItem::Check(std::to_wstring(s) + L" пкс", m_size == s, [this, s]() { m_size = s; }, true));
        GetShell().ShowMenuBelow(LocalToScreen(m_menuAnchor), std::move(items), 160.0f);
    }

    void ShowOutlineMenu()
    {
        std::vector<MenuItem> items = {
            MenuItem::Check(L"Без контура", !m_outline, [this]() { m_outline = false; }, true),
            MenuItem::Check(L"Сплошной цвет", m_outline, [this]() { m_outline = true; }, true),
        };
        GetShell().ShowMenuBelow(LocalToScreen(m_menuAnchor), std::move(items), 180.0f);
    }

    void ShowFillMenu()
    {
        std::vector<MenuItem> items = {
            MenuItem::Check(L"Без заливки", !m_fillShape, [this]() { m_fillShape = false; }, true),
            MenuItem::Check(L"Сплошной цвет (дополнительный)", m_fillShape, [this]() { m_fillShape = true; }, true),
        };
        GetShell().ShowMenuBelow(LocalToScreen(m_menuAnchor), std::move(items), 240.0f);
    }

    void ShowRotateMenu()
    {
        auto rot = [this](Gdiplus::RotateFlipType t) {
            return [this, t]() {
                CommitFloat();
                PushUndo();
                m_canvas->RotateFlip(t);
                ClearSelection();
            };
        };
        std::vector<MenuItem> items = {
            MenuItem(L"Повернуть вправо на 90°", Icon::RotateRight, rot(Gdiplus::Rotate90FlipNone)),
            MenuItem(L"Повернуть влево на 90°", Icon::RotateLeft, rot(Gdiplus::Rotate270FlipNone)),
            MenuItem(L"Повернуть на 180°", Icon::Refresh, rot(Gdiplus::Rotate180FlipNone)),
            MenuItem::Sep(),
            MenuItem(L"Отразить по вертикали", Icon::ArrowDown, rot(Gdiplus::RotateNoneFlipY)),
            MenuItem(L"Отразить по горизонтали", Icon::ArrowRight, rot(Gdiplus::RotateNoneFlipX)),
        };
        GetShell().ShowMenuBelow(LocalToScreen(m_menuAnchor), std::move(items), 240.0f);
    }

    void ShowMainMenu(int which, const Rect& anchor)
    {
        std::vector<MenuItem> items;
        if (which == 0)
        {
            items = {
                MenuItem(L"Создать", Icon::Add, [this]() { NewDocument(); }, L"Ctrl+N"),
                MenuItem(L"Открыть", Icon::Open, [this]() { ShowOpen(); }, L"Ctrl+O"),
                MenuItem(L"Сохранить", Icon::Save, [this]() { Save(nullptr); }, L"Ctrl+S"),
                MenuItem(L"Сохранить как", Icon::Save, [this]() { SaveAs(nullptr); }, L"Ctrl+Shift+S"),
                MenuItem::Sep(),
                MenuItem(L"Печать", Icon::Print, nullptr, L"Ctrl+P").Disabled(),
                MenuItem(L"Свойства изображения", Icon::Properties, [this]() {
                    ShowDialog(L"Свойства изображения",
                               L"Ширина: " + std::to_wstring(CW()) + L" пкс\nВысота: " + std::to_wstring(CH()) +
                                   L" пкс\nРазрешение: 96 точек на дюйм\nЦвета: 32 бита", {{L"ОК", true}}, nullptr);
                }, L"Ctrl+E"),
                MenuItem::Sep(),
                MenuItem(L"Выход", Icon::Close, [this]() { Close(); }),
            };
        }
        else if (which == 1)
        {
            const bool sel = m_hasSel || m_float;
            items = {
                MenuItem(L"Отменить", Icon::Undo, [this]() { Undo(); }, L"Ctrl+Z").Disabled(m_undo.empty() && !m_float),
                MenuItem(L"Вернуть", Icon::Redo, [this]() { Redo(); }, L"Ctrl+Y").Disabled(m_redo.empty()),
                MenuItem::Sep(),
                MenuItem(L"Вырезать", Icon::Cut, [this]() { CutSel(); }, L"Ctrl+X").Disabled(!sel),
                MenuItem(L"Копировать", Icon::Copy, [this]() { CopySel(); }, L"Ctrl+C").Disabled(!sel),
                MenuItem(L"Вставить", Icon::Paste, [this]() { PasteImage(); }, L"Ctrl+V"),
                MenuItem::Sep(),
                MenuItem(L"Выделить все", Icon::Select, [this]() { SelectAll(); }, L"Ctrl+A"),
                MenuItem(L"Удалить выделенный фрагмент", Icon::Delete, [this]() { DeleteSel(); }, L"Delete").Disabled(!sel),
            };
        }
        else
        {
            std::vector<MenuItem> zoom = {
                MenuItem(L"Увеличить", Icon::ZoomIn, [this]() { ZoomStep(1); }, L"Ctrl+Плюс"),
                MenuItem(L"Уменьшить", Icon::ZoomOut, [this]() { ZoomStep(-1); }, L"Ctrl+Минус"),
                MenuItem(L"Фактический размер (100%)", Icon::None, [this]() { SetZoom(1.0f, ViewRect().Center()); }, L"Ctrl+1"),
                MenuItem(L"По размеру окна", Icon::None, [this]() { FitZoom(); }, L"Ctrl+0"),
            };
            items = {
                MenuItem::Sub(L"Масштаб", Icon::ZoomIn, zoom),
                MenuItem::Check(L"Линии сетки", m_grid, [this]() { m_grid = !m_grid; }, false, L"Ctrl+G"),
                MenuItem::Check(L"Строка состояния", m_statusBar, [this]() { m_statusBar = !m_statusBar; }),
            };
        }
        GetShell().ShowMenuBelow(anchor, std::move(items), 260.0f);
    }

    void OnResize() override
    {
        float x = 44.0f;
        for (Button* b : m_menus)
        {
            const float w = Renderer::Get().TextWidth(b->text, 14.0f) + 24.0f;
            b->SetBounds({x, 6.0f, w, 32.0f});
            x += w + 2.0f;
        }
        m_undoBtn->SetBounds({x + 8.0f, 6.0f, 36.0f, 32.0f});
        m_redoBtn->SetBounds({x + 46.0f, 6.0f, 36.0f, 32.0f});
        LayoutRibbon();
        ClampScroll();
        if (m_pickOpen)
            LayoutPicker();
    }

    bool IsDragArea(Point local) const override
    {
        if (local.y >= kTitleH || local.x > Width() - 46.0f * 3.0f)
            return false;
        for (auto& c : m_ui.Children())
        {
            if (c->Visible() && c->HitTest(local))
                return false;
        }
        return true;
    }

    // ================================================================== рисование на холсте
    Color StrokeColor() const { return m_rightButton ? m_secondary : m_primary; }

    void Segment(Point a, Point b)
    {
        Gdiplus::Graphics g(m_canvas.get());
        const Color c = StrokeColor();
        switch (m_tool)
        {
        case Tool::Pencil:
        {
            g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
            g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            const float w = static_cast<float>(std::max(1, m_size / 3));
            if (std::fabs(a.x - b.x) < 0.01f && std::fabs(a.y - b.y) < 0.01f)
            {
                Gdiplus::SolidBrush br(GpColor(c));
                g.FillRectangle(&br, std::floor(a.x - w * 0.5f + 0.5f), std::floor(a.y - w * 0.5f + 0.5f), w, w);
                break;
            }
            Gdiplus::Pen pen(GpColor(c), w);
            pen.SetStartCap(Gdiplus::LineCapSquare);
            pen.SetEndCap(Gdiplus::LineCapSquare);
            g.DrawLine(&pen, std::floor(a.x) + 0.5f, std::floor(a.y) + 0.5f, std::floor(b.x) + 0.5f, std::floor(b.y) + 0.5f);
            break;
        }
        case Tool::Eraser:
        {
            const float s = static_cast<float>(m_size) * 2.0f + 4.0f;
            Gdiplus::SolidBrush br(GpColor(m_secondary));
            const float dist = Distance(a, b);
            const int steps = std::max(1, static_cast<int>(dist / (s * 0.25f)));
            for (int i = 0; i <= steps; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                const float x = Lerp(a.x, b.x, t), y = Lerp(a.y, b.y, t);
                g.FillRectangle(&br, std::floor(x - s * 0.5f), std::floor(y - s * 0.5f), s, s);
            }
            break;
        }
        case Tool::Brush:
        {
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            const float w = static_cast<float>(m_size);
            if (m_brush == BrushKind::Round)
            {
                if (Distance(a, b) < 0.01f)
                {
                    Gdiplus::SolidBrush br(GpColor(c));
                    g.FillEllipse(&br, a.x - w * 0.5f, a.y - w * 0.5f, w, w);
                }
                else
                {
                    Gdiplus::Pen pen(GpColor(c), w);
                    pen.SetStartCap(Gdiplus::LineCapRound);
                    pen.SetEndCap(Gdiplus::LineCapRound);
                    g.DrawLine(&pen, a.x, a.y, b.x, b.y);
                }
            }
            else if (m_brush == BrushKind::Calligraphy)
            {
                // Наклонное перо под 45°.
                const float d = std::max(2.0f, w * 0.8f);
                const Gdiplus::PointF quad[4] = {{a.x - d, a.y + d}, {a.x + d, a.y - d}, {b.x + d, b.y - d}, {b.x - d, b.y + d}};
                Gdiplus::SolidBrush br(GpColor(c));
                g.FillPolygon(&br, quad, 4);
            }
            else if (m_brush == BrushKind::Airbrush)
            {
                Gdiplus::SolidBrush br(GpColor(c));
                const float rad = w * 1.5f + 4.0f;
                for (int i = 0; i < 18 + m_size * 2; ++i)
                {
                    const float ang = RandomFloat(0.0f, 2.0f * kPi), rr = rad * std::sqrt(RandomFloat(0.0f, 1.0f));
                    g.FillRectangle(&br, b.x + std::cos(ang) * rr, b.y + std::sin(ang) * rr, 1.0f, 1.0f);
                }
            }
            break;
        }
        default:
            break;
        }
    }

    // Маркер: весь штрих перерисовывается поверх снимка (без накопления прозрачности).
    void RedrawMarker()
    {
        if (m_undo.empty() || m_stroke.size() < 1)
            return;
        m_canvas = CloneBitmap(m_undo.back().get());
        Gdiplus::Graphics g(m_canvas.get());
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        const Color c = StrokeColor();
        Gdiplus::Pen pen(Gdiplus::Color(110, c.r, c.g, c.b), static_cast<float>(m_size) * 2.0f + 4.0f);
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        pen.SetStartCap(Gdiplus::LineCapSquare);
        pen.SetEndCap(Gdiplus::LineCapSquare);
        std::vector<Gdiplus::PointF> pts;
        for (const Point& p : m_stroke)
            pts.push_back({p.x, p.y});
        if (pts.size() == 1)
            pts.push_back({pts[0].X + 0.5f, pts[0].Y});
        g.DrawLines(&pen, pts.data(), static_cast<INT>(pts.size()));
    }

    void DrawShapeTo(Gdiplus::Graphics& g, Point a, Point b, bool shift)
    {
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        float x0 = std::min(a.x, b.x), y0 = std::min(a.y, b.y), x1 = std::max(a.x, b.x), y1 = std::max(a.y, b.y);
        if (shift && m_shape != ShapeKind::Line)
        {
            const float s = std::max(x1 - x0, y1 - y0);
            x1 = a.x <= b.x ? x0 + s : x1;
            x0 = a.x <= b.x ? x0 : x1 - s;
            y1 = a.y <= b.y ? y0 + s : y1;
            y0 = a.y <= b.y ? y0 : y1 - s;
        }
        const float w = static_cast<float>(m_size);
        const Color oc = m_rightButton ? m_secondary : m_primary;
        const Color fc = m_rightButton ? m_primary : m_secondary;
        if (m_shape == ShapeKind::Line)
        {
            Gdiplus::Pen pen(GpColor(oc), w);
            pen.SetStartCap(Gdiplus::LineCapRound);
            pen.SetEndCap(Gdiplus::LineCapRound);
            g.DrawLine(&pen, a.x, a.y, b.x, b.y);
            return;
        }
        Gdiplus::GraphicsPath path;
        ShapePath(path, m_shape, Gdiplus::RectF(x0, y0, std::max(1.0f, x1 - x0), std::max(1.0f, y1 - y0)));
        if (m_fillShape)
        {
            Gdiplus::SolidBrush br(GpColor(fc));
            g.FillPath(&br, &path);
        }
        if (m_outline || !m_fillShape)
        {
            Gdiplus::Pen pen(GpColor(oc), w);
            pen.SetLineJoin(Gdiplus::LineJoinRound);
            g.DrawPath(&pen, &path);
        }
    }

    void FloodFill(int x, int y, Color c)
    {
        const int w = CW(), h = CH();
        if (x < 0 || y < 0 || x >= w || y >= h)
            return;
        Gdiplus::BitmapData data{};
        Gdiplus::Rect rc(0, 0, w, h);
        if (m_canvas->LockBits(&rc, Gdiplus::ImageLockModeRead | Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &data) != Gdiplus::Ok)
            return;
        auto px = [&](int xx, int yy) -> uint32_t& {
            return reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(data.Scan0) + static_cast<ptrdiff_t>(yy) * data.Stride)[xx];
        };
        const uint32_t target = px(x, y);
        const uint32_t repl = (static_cast<uint32_t>(c.a) << 24) | (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b;
        if (target != repl)
        {
            std::vector<std::pair<int, int>> stack;
            stack.emplace_back(x, y);
            while (!stack.empty())
            {
                auto [sx, sy] = stack.back();
                stack.pop_back();
                int lx = sx;
                while (lx > 0 && px(lx - 1, sy) == target)
                    --lx;
                int rx = sx;
                while (rx < w - 1 && px(rx + 1, sy) == target)
                    ++rx;
                if (px(sx, sy) != target)
                    continue;
                for (int i = lx; i <= rx; ++i)
                    px(i, sy) = repl;
                for (int ny : {sy - 1, sy + 1})
                {
                    if (ny < 0 || ny >= h)
                        continue;
                    bool run = false;
                    for (int i = lx; i <= rx; ++i)
                    {
                        const bool match = px(i, ny) == target;
                        if (match && !run)
                            stack.emplace_back(i, ny);
                        run = match;
                    }
                }
            }
        }
        m_canvas->UnlockBits(&data);
    }

    Color PixelAt(int x, int y) const
    {
        Gdiplus::Color c;
        if (x < 0 || y < 0 || x >= CW() || y >= CH() || m_canvas->GetPixel(x, y, &c) != Gdiplus::Ok)
            return Color(255, 255, 255);
        return Color(c.GetR(), c.GetG(), c.GetB(), 255);
    }

    // ---- выделение -------------------------------------------------------------------
    void ClearSelection()
    {
        m_hasSel = false;
        m_float.reset();
    }

    Rect SelRect() const
    {
        if (m_float)
            return {m_floatPos.x, m_floatPos.y, static_cast<float>(m_float->GetWidth()), static_cast<float>(m_float->GetHeight())};
        return m_sel;
    }

    // Превратить выделение в «плавающий» фрагмент (для перемещения).
    void LiftSelection()
    {
        if (m_float || !m_hasSel)
            return;
        const int x = RoundToInt(m_sel.x), y = RoundToInt(m_sel.y), w = RoundToInt(m_sel.w), h = RoundToInt(m_sel.h);
        if (w <= 0 || h <= 0)
            return;
        PushUndo();
        m_float = std::make_unique<Gdiplus::Bitmap>(w, h, PixelFormat32bppARGB);
        {
            Gdiplus::Graphics g(m_float.get());
            g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
            g.DrawImage(m_canvas.get(), Gdiplus::Rect(0, 0, w, h), x, y, w, h, Gdiplus::UnitPixel);
        }
        Gdiplus::Graphics g(m_canvas.get());
        Gdiplus::SolidBrush br(GpColor(m_secondary));
        g.FillRectangle(&br, x, y, w, h);
        m_floatPos = {static_cast<float>(x), static_cast<float>(y)};
    }

    void CommitFloat()
    {
        if (!m_float)
            return;
        Gdiplus::Graphics g(m_canvas.get());
        g.DrawImage(m_float.get(), RoundToInt(m_floatPos.x), RoundToInt(m_floatPos.y), static_cast<INT>(m_float->GetWidth()), static_cast<INT>(m_float->GetHeight()));
        m_sel = SelRect();
        m_float.reset();
        m_hasSel = false;
        m_dirty = true;
    }

    void CancelFloat()
    {
        if (!m_float)
            return;
        m_float.reset();
        m_hasSel = false;
        if (!m_undo.empty())
        {
            m_canvas = std::move(m_undo.back());
            m_undo.pop_back();
        }
    }

    std::unique_ptr<Gdiplus::Bitmap> SelectionBitmap() const
    {
        if (m_float)
            return CloneBitmap(m_float.get());
        if (!m_hasSel)
            return CloneBitmap(m_canvas.get());
        const int x = RoundToInt(m_sel.x), y = RoundToInt(m_sel.y), w = RoundToInt(m_sel.w), h = RoundToInt(m_sel.h);
        auto bmp = std::make_unique<Gdiplus::Bitmap>(std::max(1, w), std::max(1, h), PixelFormat32bppARGB);
        Gdiplus::Graphics g(bmp.get());
        g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        g.DrawImage(m_canvas.get(), Gdiplus::Rect(0, 0, w, h), x, y, w, h, Gdiplus::UnitPixel);
        return bmp;
    }

    void CopySel()
    {
        if (!m_hasSel && !m_float)
            return;
        auto bmp = SelectionBitmap();
        CopyBitmapToClipboard(GetShell().Hwnd(), bmp.get());
    }

    void CutSel()
    {
        if (!m_hasSel && !m_float)
            return;
        CopySel();
        DeleteSel();
    }

    void DeleteSel()
    {
        if (m_float)
        {
            m_float.reset();
            m_hasSel = false;
            m_dirty = true;
            return;
        }
        if (!m_hasSel)
            return;
        PushUndo();
        Gdiplus::Graphics g(m_canvas.get());
        Gdiplus::SolidBrush br(GpColor(m_secondary));
        g.FillRectangle(&br, RoundToInt(m_sel.x), RoundToInt(m_sel.y), RoundToInt(m_sel.w), RoundToInt(m_sel.h));
        m_hasSel = false;
    }

    void SelectAll()
    {
        SetTool(Tool::Select);
        CommitFloat();
        m_sel = {0.0f, 0.0f, static_cast<float>(CW()), static_cast<float>(CH())};
        m_hasSel = true;
    }

    void PasteImage()
    {
        std::unique_ptr<Gdiplus::Bitmap> bmp = BitmapFromClipboard(GetShell().Hwnd());
        if (!bmp)
            return;
        CommitFloat();
        SetTool(Tool::Select);
        // Холст расширяется, если вставляемое изображение больше.
        const int bw = static_cast<int>(bmp->GetWidth()), bh = static_cast<int>(bmp->GetHeight());
        PushUndo();
        if (bw > CW() || bh > CH())
            ResizeCanvas(std::max(bw, CW()), std::max(bh, CH()));
        const Point origin = ToCanvas({ViewRect().x + 24.0f, ViewRect().y + 24.0f});
        m_float = std::move(bmp);
        m_floatPos = {std::max(0.0f, std::floor(origin.x)), std::max(0.0f, std::floor(origin.y))};
        if (m_floatPos.x + static_cast<float>(bw) > static_cast<float>(CW()))
            m_floatPos.x = 0.0f;
        if (m_floatPos.y + static_cast<float>(bh) > static_cast<float>(CH()))
            m_floatPos.y = 0.0f;
        m_hasSel = true;
    }

    void Crop()
    {
        CommitFloat();
        if (!m_hasSel)
            return;
        auto part = SelectionBitmap();
        PushUndo();
        m_canvas = std::move(part);
        ClearSelection();
        m_scroll = {};
    }

    void ResizeCanvas(int w, int h)
    {
        auto bmp = NewCanvas(std::max(1, w), std::max(1, h), m_secondary);
        Gdiplus::Graphics g(bmp.get());
        g.DrawImage(m_canvas.get(), 0, 0, CW(), CH());
        m_canvas = std::move(bmp);
    }

    void ResizeDialog()
    {
        CommitFloat();
        ShowInputDialog(L"Изменить размер", L"Новый размер в процентах от текущего (" + std::to_wstring(CW()) + L" × " + std::to_wstring(CH()) + L" пкс):",
                        L"100", L"ОК", [this](const std::wstring& t) {
                            const int pct = static_cast<int>(Clamp(std::wcstol(t.c_str(), nullptr, 10), 1L, 500L));
                            if (pct == 100)
                                return;
                            const int w = std::max(1, CW() * pct / 100), h = std::max(1, CH() * pct / 100);
                            if (w > 6000 || h > 6000)
                                return;
                            PushUndo();
                            auto bmp = std::make_unique<Gdiplus::Bitmap>(w, h, PixelFormat32bppARGB);
                            Gdiplus::Graphics g(bmp.get());
                            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                            g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
                            g.DrawImage(m_canvas.get(), 0, 0, w, h);
                            m_canvas = std::move(bmp);
                            ClearSelection();
                        });
    }

    // ---- текст ---------------------------------------------------------------------
    float TextPx() const { return 10.0f + static_cast<float>(m_size) * 2.0f; }

    void StartText(Point canvasPt)
    {
        CommitText();
        m_textPos = canvasPt;
        const Rect c = CanvasRect();
        m_text->fontSize = TextPx() * m_zoom;
        m_text->textColor = m_primary;
        m_text->SetText(L"");
        m_text->SetBounds({c.x + canvasPt.x * m_zoom - 11.0f, c.y + canvasPt.y * m_zoom - 4.0f, std::max(160.0f, 300.0f * m_zoom), TextPx() * m_zoom * 1.6f + 8.0f});
        m_text->SetVisible(true);
        m_ui.SetFocus(m_text);
    }

    void CommitText()
    {
        if (!m_text || !m_text->Visible())
            return;
        const std::wstring s = m_text->Text();
        m_text->SetVisible(false);
        m_ui.SetFocus(nullptr);
        if (s.empty())
            return;
        PushUndo();
        Gdiplus::Graphics g(m_canvas.get());
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
        Gdiplus::SolidBrush br(GpColor(m_primary));
        Gdiplus::Font* f = Renderer::Get().Font(TextPx(), FontWeight::Regular);
        const float lh = Renderer::Get().LineHeight(TextPx());
        g.DrawString(s.c_str(), static_cast<INT>(s.size()), f, Gdiplus::PointF(m_textPos.x, m_textPos.y + (TextPx() * 1.6f - lh) * 0.5f),
                     Gdiplus::StringFormat::GenericTypographic(), &br);
    }

    void CancelText()
    {
        m_text->SetVisible(false);
        m_ui.SetFocus(nullptr);
    }

    // ---- открытие --------------------------------------------------------------------
    void ShowOpen()
    {
        m_openList.clear();
        std::function<void(FsNode*)> collect = [&](FsNode* d) {
            for (auto& c : d->children)
            {
                if (c->IsFolder())
                    collect(c.get());
                else if (c->kind == FileKind::Image)
                    m_openList.push_back(c.get());
            }
        };
        collect(GetShell().FS().Home());
        m_openOpen = true;
        m_openHover = -1;
        m_openScroll = 0.0f;
        m_openAnim.Start(0.0f, 1.0f, 0.18f, Ease::OutCubic);
    }

    Rect OpenRect() const
    {
        const float w = std::min(720.0f, Width() - 60.0f), h = std::min(520.0f, Height() - kTitleH - 40.0f);
        return {std::floor((Width() - w) * 0.5f), std::floor(kTitleH + (Height() - kTitleH - h) * 0.5f), w, h};
    }
    Rect OpenCell(int i) const
    {
        const Rect d = OpenRect();
        const int cols = std::max(1, static_cast<int>((d.w - 40.0f) / 164.0f));
        return {d.x + 24.0f + static_cast<float>(i % cols) * 164.0f, d.y + 64.0f + static_cast<float>(i / cols) * 132.0f - m_openScroll, 156.0f, 124.0f};
    }

    float OpenMaxScroll() const
    {
        if (m_openList.empty())
            return 0.0f;
        const Rect d = OpenRect();
        const Rect last = OpenCell(static_cast<int>(m_openList.size()) - 1);
        return std::max(0.0f, last.Bottom() + m_openScroll + 16.0f - d.Bottom());
    }

    void DrawOpen(Renderer& r)
    {
        if (!m_openOpen)
            return;
        const Palette& p = Theme::P();
        const float t = m_openAnim.Value();
        r.FillRect({0.0f, kTitleH, Width(), Height() - kTitleH}, p.smoke.MulAlpha(t));
        r.PushOpacity(t);
        const Rect d = OpenRect();
        r.Shadow(d, 8.0f, 28.0f, 10.0f, p.dark ? 0.5f : 0.3f);
        r.FillRoundRect(d, 8.0f, p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xFFFFFF));
        r.StrokeRoundRect(d, 8.0f, p.surfaceStroke);
        r.Text(L"Открыть изображение", {d.x + 24.0f, d.y + 18.0f, d.w - 100.0f, 30.0f}, p.text, 20.0f, FontWeight::Semibold);
        const Rect closeR{d.Right() - 44.0f, d.y + 14.0f, 32.0f, 32.0f};
        if (m_openHover == -2)
            r.FillRoundRect(closeR, 4.0f, p.subtleHover);
        r.Glyph(Icon::Close, {closeR.CenterX() - 6.0f, closeR.CenterY() - 6.0f, 12.0f, 12.0f}, p.text);
        r.PushClip({d.x, d.y + 56.0f, d.w, d.h - 64.0f});
        if (m_openList.empty())
            r.Text(L"Изображения не найдены.", {d.x, d.y + 120.0f, d.w, 24.0f}, p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::Center);
        for (int i = 0; i < static_cast<int>(m_openList.size()); ++i)
        {
            const Rect cell = OpenCell(i);
            if (cell.y > d.Bottom())
                break;
            if (cell.Bottom() < d.y + 56.0f)
                continue;
            if (m_openHover == i)
                r.FillRoundRect(cell, 6.0f, p.subtleHover);
            if (std::shared_ptr<Gdiplus::Bitmap> bmp = NodeBitmap(m_openList[static_cast<size_t>(i)]))
            {
                const float iw = static_cast<float>(bmp->GetWidth()), ih = static_cast<float>(bmp->GetHeight());
                const float s = std::min((cell.w - 16.0f) / iw, 80.0f / ih);
                const Rect thumb{cell.CenterX() - iw * s * 0.5f, cell.y + 8.0f + (80.0f - ih * s) * 0.5f, iw * s, ih * s};
                r.ImageRounded(bmp.get(), thumb, 4.0f);
                r.StrokeRoundRect(thumb, 4.0f, p.controlStroke);
            }
            r.Text(m_openList[static_cast<size_t>(i)]->name, {cell.x + 6.0f, cell.y + 94.0f, cell.w - 12.0f, 22.0f}, p.text, 12.0f, FontWeight::Regular,
                   TextFlags::Center | TextFlags::VCenter);
        }
        r.PopClip();
        r.PopOpacity();
    }

    // ================================================================== отрисовка
    void DrawContent(Renderer& r, const Rect& /*client*/) override
    {
        if (m_zoom <= 0.0f)
            FitZoom();
        const Palette& p = Theme::P();
        r.DrawAppIcon(AppIcon::Paint, {14.0f, kTitleH * 0.5f - 8.0f, 16.0f, 16.0f});
        // Имя документа между кнопками отмены и кнопками заголовка.
        const float tx = m_redoBtn->Bounds().Right() + 16.0f;
        const float tw = Width() - 46.0f * 3.0f - 16.0f - tx;
        if (tw > 60.0f)
            r.Text(Title(), {tx, 0.0f, tw, kTitleH}, IsActive() ? p.text : p.textSecondary, 12.0f, FontWeight::Regular,
                   TextFlags::VCenter | TextFlags::Center);
        const bool modal = m_pickOpen || m_openOpen;
        for (Button* b : m_menus)
            b->SetEnabled(!modal);
        m_undoBtn->SetEnabled(!modal && (!m_undo.empty() || m_float != nullptr));
        m_redoBtn->SetEnabled(!modal && !m_redo.empty());
        DrawRibbon(r);
        DrawCanvasArea(r);
        if (m_statusBar)
            DrawStatus(r);
        DrawPicker(r);
        if (m_text->Visible())
        {
            // Пунктирная рамка текстового блока.
            const Rect b = m_text->Bounds();
            Gdiplus::Pen dash(GpColor(Theme::A().base), 1.0f);
            dash.SetDashStyle(Gdiplus::DashStyleDash);
            r.G().DrawRectangle(&dash, std::floor(b.x) + 0.5f, std::floor(b.y) + 0.5f, std::floor(b.w), std::floor(b.h));
        }
        m_ui.Draw(r);
        DrawOpen(r);
    }

    void DrawRibbon(Renderer& r)
    {
        const Palette& p = Theme::P();
        const Rect rb{0.0f, kTitleH, Width(), kRibbonH};
        r.FillRect(rb, p.dark ? Color(255, 255, 255, 0x08) : Color(255, 255, 255, 0x90));
        r.FillRect({0.0f, rb.Bottom() - 1.0f, Width(), 1.0f}, p.divider);
        for (size_t i = 0; i < m_groups.size(); ++i)
        {
            const Rect& g = m_groups[i].first;
            r.Text(m_groups[i].second, {g.x, g.Bottom() - 18.0f, g.w, 16.0f}, p.textSecondary, 11.0f, FontWeight::Regular, TextFlags::Center);
            if (i + 1 < m_groups.size())
                r.FillRect({g.Right() + 4.0f, g.y + 6.0f, 1.0f, g.h - 12.0f}, p.divider);
        }
        for (size_t i = 0; i < m_tbs.size(); ++i)
        {
            const Tb& t = m_tbs[i];
            const bool hot = static_cast<int>(i) == m_hoverTb;
            const bool act = t.isActive && t.isActive();
            if (act || hot)
                r.FillRoundRect(t.r, 4.0f, act ? (p.dark ? Color(255, 255, 255, 0x18) : Color(0, 0, 0, 0x0E)) : p.subtleHover);
            if (act)
                r.StrokeRoundRect(t.r, 4.0f, p.dark ? Color(255, 255, 255, 0x30) : Color(0, 0, 0, 0x20));
            if (t.custom == 1)
            {
                // Значок изменения размера: угол большой рамки, малый квадрат и диагональная стрелка.
                const float gx = std::floor(t.r.CenterX() - 8.0f) + 0.5f, gy = std::floor(t.r.CenterY() - 8.0f) + 0.5f;
                r.Polyline({{gx + 1.0f, gy + 5.0f}, {gx + 1.0f, gy + 1.0f}, {gx + 15.0f, gy + 1.0f}, {gx + 15.0f, gy + 15.0f}, {gx + 11.0f, gy + 15.0f}}, p.text, 1.0f);
                r.StrokeRect({gx + 1.0f, gy + 8.0f, 7.0f, 7.0f}, p.text);
                r.Line(gx + 8.0f, gy + 8.0f, gx + 12.0f, gy + 4.0f, p.text, 1.0f);
                r.Polyline({{gx + 9.0f, gy + 4.0f}, {gx + 12.0f, gy + 4.0f}, {gx + 12.0f, gy + 7.0f}}, p.text, 1.0f);
                continue;
            }
            if (t.shape >= 0)
            {
                Gdiplus::GraphicsPath path;
                const Rect ir = t.r.Inset(5.0f, 3.0f, 5.0f, 3.0f);
                ShapePath(path, static_cast<ShapeKind>(t.shape), Gdiplus::RectF(ir.x, ir.y, ir.w, ir.h));
                r.StrokePath(path, p.text, 1.0f);
                continue;
            }
            if (t.big)
            {
                if (t.icon == Icon::Hamburger)
                {
                    // Значок толщины: три линии разной толщины.
                    for (int k = 0; k < 3; ++k)
                        r.FillRect({t.r.CenterX() - 11.0f, t.r.y + 12.0f + static_cast<float>(k) * 8.0f, 22.0f, static_cast<float>(k + 1)}, p.text);
                }
                else
                    r.Glyph(t.icon, {t.r.CenterX() - 12.0f, t.r.y + 8.0f, 24.0f, 24.0f}, p.text);
                if (t.dropdown)
                    r.Glyph(Icon::ChevronDown, {t.r.CenterX() - 4.0f, t.r.Bottom() - 13.0f, 8.0f, 8.0f}, p.textSecondary);
                continue;
            }
            float ix = t.r.x + (t.label.empty() && !t.dropdown ? t.r.w * 0.5f - 8.0f : 8.0f);
            r.Glyph(t.icon, {ix, t.r.CenterY() - 8.0f, 16.0f, 16.0f}, p.text);
            if (!t.label.empty())
                r.Text(t.label, {ix + 22.0f, t.r.y, t.r.w - 44.0f, t.r.h}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            if (t.dropdown)
                r.Glyph(Icon::ChevronDown, {t.r.Right() - 14.0f, t.r.CenterY() - 4.0f, 8.0f, 8.0f}, p.textSecondary);
        }
        // Цвета
        const Rect pr = PrimaryRect(), sr = SecondaryRect();
        r.FillCircle(sr.CenterX(), sr.CenterY(), sr.w * 0.5f, m_secondary);
        r.StrokeCircle(sr.CenterX(), sr.CenterY(), sr.w * 0.5f, p.controlStrongStroke);
        r.FillCircle(pr.CenterX(), pr.CenterY(), pr.w * 0.5f + 2.0f, p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xFFFFFF));
        r.FillCircle(pr.CenterX(), pr.CenterY(), pr.w * 0.5f, m_primary);
        r.StrokeCircle(pr.CenterX(), pr.CenterY(), pr.w * 0.5f, p.controlStrongStroke);
        for (int i = 0; i < 20; ++i)
        {
            const Rect s = SwatchRect(i);
            r.FillCircle(s.CenterX(), s.CenterY(), 8.0f, kPalette[i]);
            r.StrokeCircle(s.CenterX(), s.CenterY(), 8.0f, m_hoverSwatch == i ? p.text : p.controlStroke);
        }
        for (int i = 0; i < 10; ++i)
        {
            const Rect s = CustomSwatchRect(i);
            if (i < static_cast<int>(m_custom.size()))
            {
                r.FillCircle(s.CenterX(), s.CenterY(), 8.0f, m_custom[static_cast<size_t>(i)]);
                r.StrokeCircle(s.CenterX(), s.CenterY(), 8.0f, m_hoverSwatch == 100 + i ? p.text : p.controlStroke);
            }
            else
                r.StrokeCircle(s.CenterX(), s.CenterY(), 7.5f, p.controlStroke);
        }
        const Rect ec = EditColorsRect();
        if (m_hoverEdit)
            r.FillRoundRect(ec, 4.0f, p.subtleHover);
        // Радужный круг «Изменить цвета».
        for (int k = 0; k < 12; ++k)
        {
            const float a0 = static_cast<float>(k) * 30.0f;
            r.Arc(ec.CenterX(), ec.y + 22.0f, 9.0f, a0, 31.0f, Theme::HslToRgb(static_cast<float>(k) / 12.0f, 0.85f, 0.55f), 4.0f);
        }
        r.Glyph(Icon::Add, {ec.CenterX() - 5.0f, ec.y + 40.0f, 10.0f, 10.0f}, p.text);
    }

    void DrawCanvasArea(Renderer& r)
    {
        const Palette& p = Theme::P();
        const Rect v = ViewRect();
        r.FillRect(v, p.dark ? Color::Hex(0x202020) : Color::Hex(0xE9EDF2));
        r.PushClip(v);
        const Rect c = CanvasRect();
        r.Shadow(c, 0.0f, 10.0f, 2.0f, p.dark ? 0.5f : 0.18f);
        // Холст: при увеличении — без сглаживания пикселей.
        Gdiplus::Graphics& g = r.G();
        const Gdiplus::InterpolationMode im = g.GetInterpolationMode();
        const Gdiplus::PixelOffsetMode pom = g.GetPixelOffsetMode();
        if (m_zoom >= 2.0f)
        {
            g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
            g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        }
        r.Image(m_canvas.get(), c);
        if (m_float)
            r.Image(m_float.get(), {c.x + m_floatPos.x * m_zoom, c.y + m_floatPos.y * m_zoom, static_cast<float>(m_float->GetWidth()) * m_zoom,
                                    static_cast<float>(m_float->GetHeight()) * m_zoom});
        g.SetInterpolationMode(im);
        g.SetPixelOffsetMode(pom);
        // Сетка
        if (m_grid && m_zoom >= 4.0f)
        {
            const Color gc(128, 128, 128, 0x50);
            for (int x = 0; x <= CW(); ++x)
            {
                const float px = c.x + static_cast<float>(x) * m_zoom;
                if (px >= v.x && px <= v.Right())
                    r.FillRect({std::floor(px), std::max(c.y, v.y), 1.0f, std::min(c.h, v.h)}, gc);
            }
            for (int y = 0; y <= CH(); ++y)
            {
                const float py = c.y + static_cast<float>(y) * m_zoom;
                if (py >= v.y && py <= v.Bottom())
                    r.FillRect({std::max(c.x, v.x), std::floor(py), std::min(c.w, v.w), 1.0f}, gc);
            }
        }
        // Предпросмотр фигуры поверх холста (в координатах холста).
        if (m_drawing && m_tool == Tool::Shape)
        {
            r.PushClip(c);
            r.PushTransform(c.x, c.y, m_zoom);
            DrawShapeTo(r.G(), m_start, m_last, CurrentMods().shift);
            r.PopTransform();
            r.PopClip();
        }
        // Рамка выделения
        if (m_hasSel || m_float || (m_drawing && m_tool == Tool::Select && !m_moving))
        {
            Rect s = SelRect();
            if (m_drawing && m_tool == Tool::Select && !m_moving)
                s = Rect::LTRB(std::min(m_start.x, m_last.x), std::min(m_start.y, m_last.y), std::max(m_start.x, m_last.x), std::max(m_start.y, m_last.y));
            const Rect sr{std::floor(c.x + s.x * m_zoom) + 0.5f, std::floor(c.y + s.y * m_zoom) + 0.5f, std::floor(s.w * m_zoom), std::floor(s.h * m_zoom)};
            Gdiplus::Pen white(Gdiplus::Color(255, 255, 255, 255), 1.0f);
            Gdiplus::Pen dash(GpColor(Theme::A().base), 1.0f);
            dash.SetDashStyle(Gdiplus::DashStyleDash);
            g.DrawRectangle(&white, sr.x, sr.y, sr.w, sr.h);
            g.DrawRectangle(&dash, sr.x, sr.y, sr.w, sr.h);
        }
        // Новый размер холста при перетаскивании маркера.
        if (m_resizing >= 0)
        {
            Gdiplus::Pen dash(GpColor(p.text), 1.0f);
            dash.SetDashStyle(Gdiplus::DashStyleDash);
            g.DrawRectangle(&dash, std::floor(c.x) + 0.5f, std::floor(c.y) + 0.5f, std::floor(m_resizePreview.x * m_zoom),
                            std::floor(m_resizePreview.y * m_zoom));
        }
        // Маркеры изменения размера холста
        const Rect hr = HandleRect(0), hb = HandleRect(1), hc = HandleRect(2);
        for (const Rect& h : {hr, hb, hc})
        {
            r.FillRect(h, p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xFFFFFF));
            r.StrokeRect(h, p.controlStrongStroke);
        }
        r.PopClip();
        // Полосы прокрутки
        const float cwz = static_cast<float>(CW()) * m_zoom + 48.0f, chz = static_cast<float>(CH()) * m_zoom + 48.0f;
        if (cwz > v.w)
        {
            const float tw = std::max(30.0f, v.w * v.w / cwz);
            const float tx = v.x + (v.w - tw) * (m_scroll.x / (cwz - v.w));
            r.FillRoundRect({tx, v.Bottom() - 5.0f, tw, 3.0f}, 1.5f, p.scrollThumb);
        }
        if (chz > v.h)
        {
            const float thh = std::max(30.0f, v.h * v.h / chz);
            const float ty = v.y + (v.h - thh) * (m_scroll.y / (chz - v.h));
            r.FillRoundRect({v.Right() - 5.0f, ty, 3.0f, thh}, 1.5f, p.scrollThumb);
        }
    }

    // 0 — правый край, 1 — нижний, 2 — угол.
    Rect HandleRect(int i) const
    {
        Rect c = CanvasRect();
        if (m_resizing >= 0)
        {
            c.w = m_resizePreview.x * m_zoom;
            c.h = m_resizePreview.y * m_zoom;
        }
        switch (i)
        {
        case 0:
            return {c.Right() + 2.0f, c.CenterY() - 4.0f, 8.0f, 8.0f};
        case 1:
            return {c.CenterX() - 4.0f, c.Bottom() + 2.0f, 8.0f, 8.0f};
        default:
            return {c.Right() + 2.0f, c.Bottom() + 2.0f, 8.0f, 8.0f};
        }
    }

    void DrawStatus(Renderer& r)
    {
        const Palette& p = Theme::P();
        const Rect sb{0.0f, Height() - kStatusH, Width(), kStatusH};
        r.FillRect({0.0f, sb.y, Width(), 1.0f}, p.divider);
        float x = 16.0f;
        auto item = [&](Icon icon, const std::wstring& text, float w) {
            r.Glyph(icon, {x, sb.CenterY() - 7.0f, 14.0f, 14.0f}, p.textSecondary);
            r.Text(text, {x + 22.0f, sb.y, w, sb.h}, p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            x += w + 30.0f;
        };
        item(Icon::Select, m_cursorIn ? std::to_wstring(static_cast<int>(m_cursor.x)) + L", " + std::to_wstring(static_cast<int>(m_cursor.y)) + L" пкс" : L"",
             110.0f);
        const Rect s = SelRect();
        item(Icon::Crop, (m_hasSel || m_float) ? std::to_wstring(RoundToInt(s.w)) + L" × " + std::to_wstring(RoundToInt(s.h)) + L" пкс" : L"", 110.0f);
        const int sw = m_resizing >= 0 ? static_cast<int>(m_resizePreview.x) : CW();
        const int sh = m_resizing >= 0 ? static_cast<int>(m_resizePreview.y) : CH();
        item(Icon::RectShape, std::to_wstring(sw) + L" × " + std::to_wstring(sh) + L" пкс", 120.0f);
        // Масштаб
        const Rect zr = ZoomSliderRect();
        r.Text(std::to_wstring(RoundToInt(m_zoom * 100.0f)) + L"%", {zr.x - 80.0f, sb.y, 52.0f, sb.h}, p.text, 12.0f, FontWeight::Regular,
               TextFlags::VCenter | TextFlags::Right);
        r.Glyph(Icon::ZoomOut, {zr.x - 6.0f - 12.0f, sb.CenterY() - 6.0f, 12.0f, 12.0f}, p.textSecondary);
        r.Glyph(Icon::ZoomIn, {zr.Right() + 6.0f, sb.CenterY() - 6.0f, 12.0f, 12.0f}, p.textSecondary);
        r.FillRoundRect({zr.x, zr.CenterY() - 2.0f, zr.w, 4.0f}, 2.0f, p.controlStrongStroke.MulAlpha(0.5f));
        const float t = (std::log2(m_zoom) + 3.0f) / 6.0f; // 12,5%..800%
        r.FillRoundRect({zr.x, zr.CenterY() - 2.0f, zr.w * t, 4.0f}, 2.0f, p.accent);
        r.FillCircle(zr.x + zr.w * t, zr.CenterY(), 8.0f, p.controlSolid);
        r.StrokeCircle(zr.x + zr.w * t, zr.CenterY(), 8.0f, p.controlStroke);
        r.FillCircle(zr.x + zr.w * t, zr.CenterY(), 5.0f, p.accent);
    }

    Rect ZoomSliderRect() const { return {Width() - 40.0f - 160.0f, Height() - kStatusH + 6.0f, 160.0f, 20.0f}; }

    // ================================================================== ввод
    int TbAt(Point pt) const
    {
        for (size_t i = 0; i < m_tbs.size(); ++i)
        {
            if (m_tbs[i].r.Contains(pt))
                return static_cast<int>(i);
        }
        return -1;
    }

    int SwatchAt(Point pt) const
    {
        for (int i = 0; i < 20; ++i)
        {
            if (Distance(pt, SwatchRect(i).Center()) <= 9.0f)
                return i;
        }
        for (int i = 0; i < static_cast<int>(m_custom.size()); ++i)
        {
            if (Distance(pt, CustomSwatchRect(i).Center()) <= 9.0f)
                return 100 + i;
        }
        return -1;
    }

    Color SwatchColor(int i) const { return i >= 100 ? m_custom[static_cast<size_t>(i - 100)] : kPalette[i]; }

    void OnMouseMove(const MouseEvent& e) override
    {
        Window::OnMouseMove(e);
        if (HasDialog())
            return;
        const Point pt = e.pos;
        if (m_pickOpen)
        {
            if (m_pickDrag)
                PickFromPoint(pt);
            return;
        }
        if (m_openOpen)
        {
            m_openHover = -1;
            const Rect d = OpenRect();
            if (Rect{d.Right() - 44.0f, d.y + 14.0f, 32.0f, 32.0f}.Contains(pt))
                m_openHover = -2;
            const Rect area{d.x, d.y + 56.0f, d.w, d.h - 64.0f};
            for (int i = 0; i < static_cast<int>(m_openList.size()); ++i)
            {
                if (area.Contains(pt) && OpenCell(i).Contains(pt))
                    m_openHover = i;
            }
            return;
        }
        m_hoverTb = TbAt(pt);
        if (m_hoverTb >= 0 && !m_drawing)
            GetShell().Tooltip(&m_tbs, m_tbs[static_cast<size_t>(m_hoverTb)].tip, LocalToScreen(m_tbs[static_cast<size_t>(m_hoverTb)].r), false);
        m_hoverSwatch = SwatchAt(pt);
        m_hoverEdit = EditColorsRect().Contains(pt);
        const Point cp = ToCanvas(pt);
        m_cursorIn = ViewRect().Contains(pt) && cp.x >= 0.0f && cp.y >= 0.0f && cp.x < static_cast<float>(CW()) && cp.y < static_cast<float>(CH());
        m_cursor = cp;
        if (m_zoomDrag)
        {
            const Rect zr = ZoomSliderRect();
            const float t = Saturate((pt.x - zr.x) / zr.w);
            SetZoom(std::pow(2.0f, t * 6.0f - 3.0f), ViewRect().Center());
            return;
        }
        if (m_resizing >= 0)
        {
            const int w = m_resizing == 1 ? CW() : std::max(1, RoundToInt(cp.x));
            const int h = m_resizing == 0 ? CH() : std::max(1, RoundToInt(cp.y));
            m_resizePreview = {static_cast<float>(w), static_cast<float>(h)};
            return;
        }
        if (!m_drawing)
            return;
        if (m_tool == Tool::Select)
        {
            if (m_moving && m_float)
            {
                m_floatPos = {std::floor(m_moveOrigin.x + cp.x - m_start.x), std::floor(m_moveOrigin.y + cp.y - m_start.y)};
                return;
            }
            m_last = {Clamp(cp.x, 0.0f, static_cast<float>(CW())), Clamp(cp.y, 0.0f, static_cast<float>(CH()))};
            return;
        }
        if (m_tool == Tool::Shape)
        {
            m_last = cp;
            return;
        }
        if (m_tool == Tool::Brush && m_brush == BrushKind::Marker)
        {
            m_stroke.push_back(cp);
            RedrawMarker();
            m_last = cp;
            return;
        }
        if (m_tool == Tool::Pencil || m_tool == Tool::Brush || m_tool == Tool::Eraser)
        {
            Segment(m_last, cp);
            m_last = cp;
        }
    }

    void OnMouseLeave() override
    {
        Window::OnMouseLeave();
        m_hoverTb = -1;
        m_hoverSwatch = -1;
        m_hoverEdit = false;
        m_cursorIn = false;
    }

    void OnMouseDown(const MouseEvent& e) override
    {
        if (HasDialog())
        {
            Window::OnMouseDown(e);
            return;
        }
        const Point pt = e.pos;
        if (m_pickOpen)
        {
            Window::OnMouseDown(e);
            if (m_ui.HasCapture() || e.button != MouseButton::Left)
                return;
            if (!PickRect().Contains(pt))
                return;
            if (SpectrumRect().Inflated(4.0f).Contains(pt))
                m_pickDrag = 1;
            else if (ValueRect().Inflated(4.0f, 10.0f).Contains(pt))
                m_pickDrag = 2;
            if (m_pickDrag)
            {
                m_ui.SetFocus(nullptr);
                PickFromPoint(pt);
            }
            return;
        }
        if (m_openOpen)
        {
            const Rect d = OpenRect();
            if (!d.Contains(pt) || Rect{d.Right() - 44.0f, d.y + 14.0f, 32.0f, 32.0f}.Contains(pt))
            {
                m_openOpen = false;
                return;
            }
            const Rect area{d.x, d.y + 56.0f, d.w, d.h - 64.0f};
            for (int i = 0; i < static_cast<int>(m_openList.size()); ++i)
            {
                if (area.Contains(pt) && OpenCell(i).Contains(pt))
                {
                    FsNode* n = m_openList[static_cast<size_t>(i)];
                    m_openOpen = false;
                    auto load = [this, n]() {
                        if (GetShell().FS().IsAlive(n))
                            LoadNode(n);
                    };
                    if (m_dirty)
                        ShowDialog(L"Paint", L"Вы хотите сохранить изменения в «" + DocName() + L"»?",
                                   {{L"Сохранить", true}, {L"Не сохранять", false}, {L"Отмена", false}}, [this, load](int r) {
                                       if (r == 1)
                                           load();
                                       else if (r == 0)
                                           Save(load);
                                   });
                    else
                        load();
                    return;
                }
            }
            return;
        }
        // Текстовое поле: щелчок вне — фиксация текста.
        if (m_text->Visible() && !m_text->Bounds().Contains(pt))
            CommitText();
        Window::OnMouseDown(e);
        if (m_ui.HasCapture())
            return;
        const bool right = e.button == MouseButton::Right;
        if (pt.y < kTitleH + kRibbonH)
        {
            const int tbi = TbAt(pt);
            if (tbi >= 0 && !right)
            {
                m_menuAnchor = m_tbs[static_cast<size_t>(tbi)].r;
                if (auto fn = m_tbs[static_cast<size_t>(tbi)].action)
                    PostAction(std::move(fn));
                return;
            }
            const int sw = SwatchAt(pt);
            if (sw >= 0)
            {
                (right ? m_secondary : m_primary) = SwatchColor(sw);
                return;
            }
            if (PrimaryRect().Contains(pt) || SecondaryRect().Contains(pt))
            {
                std::swap(m_primary, m_secondary);
                return;
            }
            if (EditColorsRect().Contains(pt))
            {
                OpenColorPicker();
                return;
            }
            return;
        }
        if (m_statusBar && pt.y >= Height() - kStatusH)
        {
            const Rect zr = ZoomSliderRect();
            if (zr.Inflated(10.0f, 4.0f).Contains(pt))
            {
                m_zoomDrag = true;
                OnMouseMove(e);
            }
            else if (Rect{zr.x - 22.0f, zr.y, 18.0f, zr.h}.Contains(pt))
                ZoomStep(-1);
            else if (Rect{zr.Right() + 2.0f, zr.y, 18.0f, zr.h}.Contains(pt))
                ZoomStep(1);
            return;
        }
        if (!ViewRect().Contains(pt))
            return;
        // Маркеры размера холста
        for (int i = 0; i < 3; ++i)
        {
            if (HandleRect(i).Inflated(4.0f).Contains(pt) && !right)
            {
                CommitFloat();
                m_resizing = i;
                m_resizePreview = {static_cast<float>(CW()), static_cast<float>(CH())};
                return;
            }
        }
        const Point cp = ToCanvas(pt);
        m_rightButton = right;
        switch (m_tool)
        {
        case Tool::Picker:
            // Как в Paint: после выбора цвета возвращается предыдущий инструмент.
            (right ? m_secondary : m_primary) = PixelAt(static_cast<int>(cp.x), static_cast<int>(cp.y));
            SetTool(m_prevTool);
            return;
        case Tool::Zoom:
            SetZoom(m_zoom * (right ? 0.5f : 2.0f), pt);
            return;
        case Tool::Fill:
            if (cp.x >= 0.0f && cp.y >= 0.0f && cp.x < static_cast<float>(CW()) && cp.y < static_cast<float>(CH()))
            {
                PushUndo();
                FloodFill(static_cast<int>(cp.x), static_cast<int>(cp.y), right ? m_secondary : m_primary);
            }
            return;
        case Tool::Text:
            if (!right)
                StartText(cp);
            return;
        case Tool::Select:
        {
            const bool inside = (m_hasSel || m_float) && SelRect().Contains(cp);
            if (right)
            {
                // Меню открывается при отпускании кнопки; щелчок вне выделения снимает его.
                if (!inside)
                {
                    CommitFloat();
                    m_hasSel = false;
                }
                m_menuPending = true;
                return;
            }
            if (inside)
            {
                LiftSelection();
                m_moving = true;
                m_moveOrigin = m_floatPos;
                m_start = cp;
                m_drawing = true;
                return;
            }
            CommitFloat();
            m_hasSel = false;
            m_moving = false;
            m_start = m_last = {Clamp(cp.x, 0.0f, static_cast<float>(CW())), Clamp(cp.y, 0.0f, static_cast<float>(CH()))};
            m_drawing = true;
            return;
        }
        case Tool::Shape:
            m_start = m_last = cp;
            m_drawing = true;
            return;
        case Tool::Pencil:
        case Tool::Brush:
        case Tool::Eraser:
            PushUndo();
            m_drawing = true;
            m_start = m_last = cp;
            m_stroke = {cp};
            if (m_tool == Tool::Brush && m_brush == BrushKind::Marker)
                RedrawMarker();
            else
                Segment(cp, cp);
            return;
        }
    }

    void OnMouseUp(const MouseEvent& e) override
    {
        Window::OnMouseUp(e);
        if (HasDialog())
            return;
        m_pickDrag = 0;
        if (m_menuPending && e.button == MouseButton::Right)
        {
            m_menuPending = false;
            ShowCanvasMenu(LocalToScreen(e.pos));
            return;
        }
        m_zoomDrag = false;
        if (m_resizing >= 0)
        {
            PushUndo();
            ResizeCanvas(static_cast<int>(m_resizePreview.x), static_cast<int>(m_resizePreview.y));
            m_resizing = -1;
            ClampScroll();
            return;
        }
        if (!m_drawing)
            return;
        m_drawing = false;
        if (m_tool == Tool::Shape)
        {
            if (Distance(m_start, m_last) > 1.0f)
            {
                PushUndo();
                Gdiplus::Graphics g(m_canvas.get());
                DrawShapeTo(g, m_start, m_last, e.mods.shift);
            }
        }
        else if (m_tool == Tool::Select)
        {
            if (m_moving)
                m_moving = false;
            else
            {
                const Rect s = Rect::LTRB(std::floor(std::min(m_start.x, m_last.x)), std::floor(std::min(m_start.y, m_last.y)),
                                          std::ceil(std::max(m_start.x, m_last.x)), std::ceil(std::max(m_start.y, m_last.y)));
                m_hasSel = s.w >= 2.0f && s.h >= 2.0f;
                m_sel = s;
            }
        }
        m_stroke.clear();
    }

    void ShowCanvasMenu(Point screen)
    {
        std::vector<MenuItem> items;
        if (m_hasSel || m_float)
        {
            items = {
                MenuItem(L"Вырезать", Icon::Cut, [this]() { CutSel(); }, L"Ctrl+X"),
                MenuItem(L"Копировать", Icon::Copy, [this]() { CopySel(); }, L"Ctrl+C"),
                MenuItem(L"Вставить", Icon::Paste, [this]() { PasteImage(); }, L"Ctrl+V"),
                MenuItem::Sep(),
                MenuItem(L"Обрезать", Icon::Crop, [this]() { Crop(); }, L"Ctrl+Shift+X"),
                MenuItem(L"Выделить все", Icon::Select, [this]() { SelectAll(); }, L"Ctrl+A"),
                MenuItem(L"Удалить", Icon::Delete, [this]() { DeleteSel(); }, L"Delete"),
                MenuItem::Sep(),
                MenuItem(L"Обратить цвета", Icon::Palette, [this]() { InvertColors(); }, L"Ctrl+Shift+I"),
            };
        }
        else
        {
            items = {
                MenuItem(L"Вставить", Icon::Paste, [this]() { PasteImage(); }, L"Ctrl+V"),
                MenuItem(L"Выделить все", Icon::Select, [this]() { SelectAll(); }, L"Ctrl+A"),
                MenuItem::Sep(),
                MenuItem(L"Обратить цвета", Icon::Palette, [this]() { InvertColors(); }, L"Ctrl+Shift+I"),
            };
        }
        GetShell().ShowMenu(screen, std::move(items));
    }

    bool OnMouseWheel(const MouseEvent& e) override
    {
        if (m_openOpen && !HasDialog())
        {
            m_openScroll = Clamp(m_openScroll - e.wheel * 80.0f, 0.0f, OpenMaxScroll());
            return true;
        }
        if (HasDialog() || m_openOpen || m_pickOpen)
            return true;
        if (e.mods.ctrl)
        {
            SetZoom(m_zoom * (e.wheel > 0.0f ? 1.25f : 0.8f), e.pos);
            return true;
        }
        if (e.mods.shift)
            m_scroll.x -= e.wheel * 80.0f;
        else
            m_scroll.y -= e.wheel * 80.0f;
        ClampScroll();
        return true;
    }

    CursorType Cursor(Point local) const override
    {
        if (HasDialog() || m_openOpen || m_pickOpen)
            return Window::Cursor(local);
        if (m_text->Visible() && m_text->Bounds().Contains(local))
            return CursorType::IBeam;
        for (int i = 0; i < 3; ++i)
        {
            if (HandleRect(i).Inflated(4.0f).Contains(local) || m_resizing == i)
                return i == 0 ? CursorType::SizeWE : (i == 1 ? CursorType::SizeNS : CursorType::SizeNWSE);
        }
        if (ViewRect().Contains(local))
        {
            if (m_tool == Tool::Select)
            {
                const Rect s = SelRect();
                return (m_hasSel || m_float) && s.Contains(ToCanvas(local)) ? CursorType::SizeAll : CursorType::Cross;
            }
            if (m_tool == Tool::Text)
                return CursorType::IBeam;
            return CursorType::Cross;
        }
        return Window::Cursor(local);
    }

    bool OnKeyDown(UINT vk, const KeyMods& mods) override
    {
        if (HasDialog())
            return true;
        if (m_pickOpen)
        {
            if (m_ui.FocusedChild() == m_hexBox)
                return Window::OnKeyDown(vk, mods);
            if (vk == VK_ESCAPE || vk == VK_RETURN)
                ClosePicker(vk == VK_RETURN);
            return true;
        }
        if (m_ui.FocusedChild() == m_text)
            return Window::OnKeyDown(vk, mods);
        if (m_openOpen)
        {
            if (vk == VK_ESCAPE)
                m_openOpen = false;
            return true;
        }
        if (mods.ctrl)
        {
            switch (vk)
            {
            case 'Z':
                Undo();
                return true;
            case 'Y':
                Redo();
                return true;
            case 'C':
                CopySel();
                return true;
            case 'X':
                if (mods.shift)
                    Crop();
                else
                    CutSel();
                return true;
            case 'V':
                PasteImage();
                return true;
            case 'A':
                SelectAll();
                return true;
            case 'N':
                NewDocument();
                return true;
            case 'O':
                ShowOpen();
                return true;
            case 'S':
                if (mods.shift)
                    SaveAs(nullptr);
                else
                    Save(nullptr);
                return true;
            case 'W':
                ResizeDialog();
                return true;
            case 'G':
                m_grid = !m_grid;
                return true;
            case 'I':
                if (mods.shift)
                    InvertColors();
                return true;
            case VK_OEM_PLUS:
            case VK_ADD:
                ZoomStep(1);
                return true;
            case VK_OEM_MINUS:
            case VK_SUBTRACT:
                ZoomStep(-1);
                return true;
            case '1':
                SetZoom(1.0f, ViewRect().Center());
                return true;
            case '0':
                FitZoom();
                return true;
            default:
                break;
            }
        }
        switch (vk)
        {
        case VK_DELETE:
            DeleteSel();
            return true;
        case VK_ESCAPE:
            if (m_float)
                CommitFloat();
            m_hasSel = false;
            m_drawing = false;
            return true;
        case VK_RETURN:
            CommitFloat();
            return true;
        default:
            break;
        }
        return Window::OnKeyDown(vk, mods);
    }

    // ---- выбор цвета (HSV) ------------------------------------------------------------
    void OpenColorPicker()
    {
        CommitText();
        m_pickOld = m_primary;
        ColorToHsv(m_primary, m_pickH, m_pickS, m_pickV);
        m_pickOpen = true;
        m_pickDrag = 0;
        m_pickAnim.Start(0.0f, 1.0f, 0.18f, Ease::OutCubic);
        m_hexBox->SetText(HexText(m_primary));
        m_hexBox->SetVisible(true);
        m_pickOk->SetVisible(true);
        m_pickCancel->SetVisible(true);
        LayoutPicker();
    }

    void ClosePicker(bool apply)
    {
        if (!m_pickOpen)
            return;
        m_pickOpen = false;
        m_hexBox->SetVisible(false);
        m_pickOk->SetVisible(false);
        m_pickCancel->SetVisible(false);
        m_ui.SetFocus(nullptr);
        if (!apply)
            return;
        const Color c = PickColor();
        m_primary = c;
        m_custom.erase(std::remove(m_custom.begin(), m_custom.end(), c), m_custom.end());
        m_custom.insert(m_custom.begin(), c);
        if (m_custom.size() > 10)
            m_custom.pop_back();
    }

    Color PickColor() const { return HsvToColor(m_pickH, m_pickS, m_pickV); }

    Rect PickRect() const
    {
        const float w = 520.0f, h = 420.0f;
        return {std::floor((Width() - w) * 0.5f), std::floor(kTitleH + std::max(0.0f, (Height() - kTitleH - h) * 0.5f)), w, h};
    }
    Rect SpectrumRect() const
    {
        const Rect d = PickRect();
        return {d.x + 24.0f, d.y + 64.0f, 296.0f, 200.0f};
    }
    Rect ValueRect() const
    {
        const Rect sp = SpectrumRect();
        return {sp.x, sp.Bottom() + 22.0f, sp.w, 12.0f};
    }

    void LayoutPicker()
    {
        const Rect d = PickRect();
        const Rect sp = SpectrumRect();
        const float rx = sp.Right() + 24.0f, rw = d.Right() - 24.0f - rx;
        m_hexBox->SetBounds({rx, d.y + 196.0f, rw, 32.0f});
        const float bw = (d.w - 48.0f - 8.0f) * 0.5f;
        m_pickOk->SetBounds({d.x + 24.0f, d.Bottom() - 56.0f, bw, 32.0f});
        m_pickCancel->SetBounds({d.x + 32.0f + bw, d.Bottom() - 56.0f, bw, 32.0f});
    }

    void PickFromPoint(Point pt)
    {
        if (m_pickDrag == 1)
        {
            const Rect sp = SpectrumRect();
            m_pickH = Saturate((pt.x - sp.x) / sp.w) * 0.9999f;
            m_pickS = 1.0f - Saturate((pt.y - sp.y) / sp.h);
            if (m_pickV < 0.05f)
                m_pickV = 1.0f; // на чёрном выбор оттенка иначе был бы незаметен
        }
        else if (m_pickDrag == 2)
        {
            const Rect vr = ValueRect();
            m_pickV = Saturate((pt.x - vr.x) / vr.w);
        }
        m_hexBox->SetText(HexText(PickColor()));
    }

    void DrawPicker(Renderer& r)
    {
        if (!m_pickOpen)
            return;
        const Palette& p = Theme::P();
        const float t = m_pickAnim.Value();
        r.FillRect({0.0f, kTitleH, Width(), Height() - kTitleH}, p.smoke.MulAlpha(t));
        const Rect d = PickRect();
        r.Shadow(d, 8.0f, 28.0f, 10.0f, p.dark ? 0.5f : 0.3f);
        r.FillRoundRect(d, 8.0f, p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xFFFFFF));
        // Нижняя панель с кнопками, как у диалогов WinUI.
        r.FillRoundRect({d.x, d.Bottom() - 80.0f, d.w, 80.0f}, 0.0f, 0.0f, 8.0f, 8.0f, p.dark ? Color::Hex(0x202020) : Color::Hex(0xF3F3F3));
        r.FillRect({d.x, d.Bottom() - 80.0f, d.w, 1.0f}, p.divider);
        r.StrokeRoundRect(d, 8.0f, p.surfaceStroke);
        r.Text(L"Изменить цвета", {d.x + 24.0f, d.y + 18.0f, d.w - 100.0f, 30.0f}, p.text, 20.0f, FontWeight::Semibold);
        // Спектр
        const Rect sp = SpectrumRect();
        r.ImageRounded(SpectrumBitmap(), sp, 4.0f);
        // Затемнение спектра по текущей яркости.
        r.FillRoundRect(sp, 4.0f, Color(0, 0, 0, UnitToByte(1.0f - m_pickV)));
        r.StrokeRoundRect(sp, 4.0f, p.controlStroke);
        const Point th{sp.x + m_pickH / 0.9999f * sp.w, sp.y + (1.0f - m_pickS) * sp.h};
        r.StrokeCircle(th.x, th.y, 7.0f, Color(0, 0, 0, 0x90), 3.0f);
        r.StrokeCircle(th.x, th.y, 7.0f, Color(255, 255, 255), 1.5f);
        // Яркость
        const Rect vr = ValueRect();
        r.FillGradientH(vr, vr.h * 0.5f, Color(0, 0, 0), HsvToColor(m_pickH, m_pickS, 1.0f));
        r.StrokeRoundRect(vr, vr.h * 0.5f, p.controlStroke);
        const float vx = vr.x + m_pickV * vr.w;
        r.FillCircle(vx, vr.CenterY(), 10.0f, p.controlSolid);
        r.StrokeCircle(vx, vr.CenterY(), 10.0f, p.controlStroke);
        r.FillCircle(vx, vr.CenterY(), 6.0f, PickColor());
        // Предпросмотр: новый цвет сверху, прежний снизу.
        const float rx = sp.Right() + 24.0f, rw = d.Right() - 24.0f - rx;
        const Rect pv{rx, sp.y, rw, 96.0f};
        r.FillRoundRect({pv.x, pv.y, pv.w, pv.h * 0.5f}, 6.0f, 6.0f, 0.0f, 0.0f, PickColor());
        r.FillRoundRect({pv.x, pv.CenterY(), pv.w, pv.h * 0.5f}, 0.0f, 0.0f, 6.0f, 6.0f, m_pickOld);
        r.StrokeRoundRect(pv, 6.0f, p.controlStroke);
        r.Text(L"Шестнадцатеричный", {rx, sp.y + 108.0f, rw, 20.0f}, p.textSecondary, 12.0f);
        const Color c = PickColor();
        const struct
        {
            const wchar_t* name;
            int value;
        } rows[] = {{L"Красный", c.r}, {L"Зелёный", c.g}, {L"Синий", c.b}};
        for (int i = 0; i < 3; ++i)
        {
            const float y = d.y + 240.0f + static_cast<float>(i) * 24.0f;
            r.Text(rows[i].name, {rx, y, rw, 20.0f}, p.textSecondary, 13.0f);
            r.Text(std::to_wstring(rows[i].value), {rx, y, rw, 20.0f}, p.text, 13.0f, FontWeight::Regular, TextFlags::Right);
        }
    }

    // ---- обращение цветов --------------------------------------------------------------
    void InvertColors()
    {
        if (m_float)
        {
            InvertRegion(m_float.get(), 0, 0, static_cast<int>(m_float->GetWidth()), static_cast<int>(m_float->GetHeight()));
            m_dirty = true;
            return;
        }
        PushUndo();
        if (m_hasSel)
            InvertRegion(m_canvas.get(), RoundToInt(m_sel.x), RoundToInt(m_sel.y), RoundToInt(m_sel.w), RoundToInt(m_sel.h));
        else
            InvertRegion(m_canvas.get(), 0, 0, CW(), CH());
    }

    // ================================================================== поля
    std::unique_ptr<Gdiplus::Bitmap> m_canvas;
    std::unique_ptr<Gdiplus::Bitmap> m_float;
    Point m_floatPos;
    Point m_moveOrigin;
    std::vector<std::unique_ptr<Gdiplus::Bitmap>> m_undo;
    std::vector<std::unique_ptr<Gdiplus::Bitmap>> m_redo;
    FsNode* m_node = nullptr;
    bool m_dirty = false;
    float m_zoom = 1.0f;
    Point m_scroll;
    Tool m_tool = Tool::Brush;
    BrushKind m_brush = BrushKind::Round;
    ShapeKind m_shape = ShapeKind::Rect;
    int m_size = 3;
    bool m_outline = true;
    bool m_fillShape = false;
    Color m_primary = Color(0, 0, 0);
    Color m_secondary = Color(255, 255, 255);
    std::vector<Color> m_custom;
    bool m_grid = false;
    bool m_statusBar = true;
    // Ввод
    bool m_drawing = false;
    bool m_rightButton = false;
    bool m_moving = false;
    Point m_start;
    Point m_last;
    std::vector<Point> m_stroke;
    bool m_hasSel = false;
    Rect m_sel;
    int m_resizing = -1;
    Point m_resizePreview;
    bool m_zoomDrag = false;
    Point m_cursor;
    bool m_cursorIn = false;
    Point m_textPos;
    // Лента
    std::vector<Tb> m_tbs;
    std::vector<std::pair<Rect, std::wstring>> m_groups;
    float m_colorsX = 0.0f;
    Rect m_menuAnchor;
    int m_hoverTb = -1;
    int m_hoverSwatch = -1;
    bool m_hoverEdit = false;
    std::vector<Button*> m_menus;
    Button* m_undoBtn = nullptr;
    Button* m_redoBtn = nullptr;
    TextBox* m_text = nullptr;
    // Выбор цвета
    bool m_pickOpen = false;
    float m_pickH = 0.0f;
    float m_pickS = 0.0f;
    float m_pickV = 0.0f;
    Color m_pickOld;
    int m_pickDrag = 0; // 1 — спектр, 2 — яркость
    Tween m_pickAnim;
    TextBox* m_hexBox = nullptr;
    Button* m_pickOk = nullptr;
    Button* m_pickCancel = nullptr;
    bool m_menuPending = false;
    Tool m_prevTool = Tool::Brush;
    // Открытие
    bool m_openOpen = false;
    std::vector<FsNode*> m_openList;
    int m_openHover = -1;
    float m_openScroll = 0.0f;
    Tween m_openAnim;
};
} // namespace

std::unique_ptr<Window> CreatePaint(const std::wstring& arg) { return std::make_unique<PaintWindow>(arg); }
