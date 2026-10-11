// «Параметры» Windows 11: навигация с цветными значками, карточка пользователя, поиск параметров,
// страницы из карточек. Параметры связаны с оболочкой: тема, акцентный цвет, прозрачность, обои,
// панель задач, меню «Пуск», яркость, ночной свет, звук, сеть, анимации, имя ПК, обновления.

#include "../Desktop.h"
#include "../Shell.h"
#include "../Taskbar.h"
#include "AppFactories.h"
#include "SysInfo.h"

namespace
{
constexpr float kTitleH = 48.0f;
constexpr float kHeaderH = 64.0f;
constexpr int kSwatchCols = 8;

// ---------------------------------------------------------------------------
// Миниатюры обоев (кэш, рендерятся процедурно в уменьшенном размере)
// ---------------------------------------------------------------------------
Gdiplus::Bitmap* WallpaperBitmap(int index)
{
    static std::map<int, std::unique_ptr<Gdiplus::Bitmap>> cache;
    auto it = cache.find(index);
    if (it != cache.end())
        return it->second.get();
    const int w = 384, h = 216;
    std::vector<uint32_t> px;
    RenderWallpaper(index, w, h, px);
    auto bmp = std::make_unique<Gdiplus::Bitmap>(w, h, PixelFormat32bppRGB);
    Gdiplus::BitmapData bd{};
    Gdiplus::Rect rc(0, 0, w, h);
    if (px.size() >= static_cast<size_t>(w) * static_cast<size_t>(h) &&
        bmp->LockBits(&rc, Gdiplus::ImageLockModeWrite, PixelFormat32bppRGB, &bd) == Gdiplus::Ok)
    {
        for (int y = 0; y < h; ++y)
            std::memcpy(static_cast<uint8_t*>(bd.Scan0) + static_cast<ptrdiff_t>(y) * bd.Stride,
                        &px[static_cast<size_t>(y) * static_cast<size_t>(w)], static_cast<size_t>(w) * 4u);
        bmp->UnlockBits(&bd);
    }
    Gdiplus::Bitmap* raw = bmp.get();
    cache[index] = std::move(bmp);
    return raw;
}

// Аватар по умолчанию: серый круг с силуэтом.
void DrawAvatar(Renderer& r, float cx, float cy, float radius)
{
    const Palette& p = Theme::P();
    r.FillCircle(cx, cy, radius, p.dark ? Color::Hex(0x4A4A4A) : Color::Hex(0xD6D6D6));
    const Color fg = p.dark ? Color::Hex(0xA8A8A8) : Color::Hex(0xF7F7F7);
    r.PushClip({cx - radius, cy - radius, radius * 2.0f, radius * 2.0f});
    r.FillCircle(cx, cy - radius * 0.22f, radius * 0.36f, fg);
    r.FillEllipse({cx - radius * 0.68f, cy + radius * 0.28f, radius * 1.36f, radius * 1.1f}, fg);
    r.PopClip();
}

// Миниатюра темы: обои + маленькое окно в светлом/тёмном стиле.
void DrawThemeTile(Renderer& r, const Rect& t, int wallpaper, bool dark, bool selected, bool hot)
{
    const Palette& p = Theme::P();
    r.ImageRounded(WallpaperBitmap(wallpaper), t, 6.0f);
    const Rect win{t.x + t.w * 0.2f, t.y + t.h * 0.24f, t.w * 0.46f, t.h * 0.52f};
    r.Shadow(win, 3.0f, 6.0f, 2.0f, 0.25f);
    r.FillRoundRect(win, 3.0f, dark ? Color::Hex(0x202020) : Color::Hex(0xF3F3F3));
    r.FillRoundRect({win.x + 6.0f, win.y + 7.0f, win.w * 0.45f, 3.0f}, 1.5f, dark ? Color(255, 255, 255, 0x80) : Color(0, 0, 0, 0x60));
    r.FillRoundRect({win.x + 6.0f, win.Bottom() - 11.0f, win.w * 0.3f, 5.0f}, 2.5f, Theme::A().base);
    if (selected)
        r.StrokeRoundRect(t.Inflated(2.0f), 8.0f, p.accent, 2.0f);
    else if (hot)
        r.StrokeRoundRect(t.Inflated(1.0f), 7.0f, p.controlStrongStroke, 1.0f);
}

// Мини-рабочий стол (предпросмотр персонализации).
void DrawDesktopPreview(Renderer& r, const Rect& rc)
{
    const Palette& p = Theme::P();
    r.ImageRounded(WallpaperBitmap(GetShell().Settings().wallpaper), rc, 8.0f);
    const bool dark = p.dark;
    const Rect win{rc.x + rc.w * 0.16f, rc.y + rc.h * 0.18f, rc.w * 0.52f, rc.h * 0.52f};
    r.Shadow(win, 4.0f, 10.0f, 3.0f, 0.3f);
    r.FillRoundRect(win, 4.0f, dark ? Color::Hex(0x202020) : Color::Hex(0xF3F3F3));
    if (Theme::AccentTitleBars())
        r.FillRoundRect({win.x, win.y, win.w, 12.0f}, 4.0f, 4.0f, 0.0f, 0.0f, Theme::A().dark1);
    r.FillRoundRect({win.x + 10.0f, win.y + 22.0f, win.w * 0.5f, 5.0f}, 2.5f, dark ? Color(255, 255, 255, 0x70) : Color(0, 0, 0, 0x50));
    r.FillRoundRect({win.x + 10.0f, win.y + 34.0f, win.w * 0.7f, 4.0f}, 2.0f, dark ? Color(255, 255, 255, 0x40) : Color(0, 0, 0, 0x30));
    r.FillRoundRect({win.x + 10.0f, win.Bottom() - 18.0f, win.w * 0.28f, 9.0f}, 4.5f, Theme::A().base);
    // Панель задач
    const Rect tb{rc.x, rc.Bottom() - 14.0f, rc.w, 14.0f};
    r.FillRoundRect(tb, 0.0f, 0.0f, 8.0f, 8.0f, dark ? Color(32, 32, 32, 0xE0) : Color(243, 243, 243, 0xE0));
    const bool center = GetShell().Settings().taskbarCenter;
    const float x0 = center ? tb.CenterX() - 30.0f : tb.x + 8.0f;
    for (int i = 0; i < 5; ++i)
        r.FillRoundRect({x0 + static_cast<float>(i) * 12.0f, tb.y + 3.0f, 8.0f, 8.0f}, 2.0f,
                        i == 0 ? Theme::A().base : (dark ? Color(255, 255, 255, 0x60) : Color(0, 0, 0, 0x40)));
}

// Изображение устройства (монитор с текущими обоями).
void DrawDevice(Renderer& r, const Rect& rc)
{
    const Rect screen{rc.x + 6.0f, rc.y + 6.0f, rc.w - 12.0f, rc.h - 28.0f};
    r.FillRoundRect({rc.x, rc.y, rc.w, rc.h - 16.0f}, 6.0f, Color::Hex(0x1F1F1F));
    r.ImageRounded(WallpaperBitmap(GetShell().Settings().wallpaper), screen, 3.0f);
    r.FillRect({rc.CenterX() - 8.0f, rc.Bottom() - 16.0f, 16.0f, 10.0f}, Color::Hex(0x6B6B6B));
    r.FillRoundRect({rc.CenterX() - 30.0f, rc.Bottom() - 7.0f, 60.0f, 6.0f}, 3.0f, Color::Hex(0x8A8A8A));
}

// ---------------------------------------------------------------------------
// Виджеты страниц
// ---------------------------------------------------------------------------
// Произвольная отрисовка с координатами мыши (герой-карточки, сетки цветов и т. п.).
class Canvas : public Widget
{
public:
    std::function<void(Renderer&, const Rect&, Point)> draw;
    std::function<void(Point)> onClick;
    std::function<CursorType(Point)> cursor;
    bool interactive = false;

    void Draw(Renderer& r) override
    {
        if (draw)
            draw(r, m_bounds, m_hovered ? m_mouse : Point{-1e6f, -1e6f});
    }
    bool HitTest(Point pt) const override { return m_visible && interactive && m_bounds.Contains(pt); }
    void OnMouseMove(const MouseEvent& e) override { m_mouse = e.pos; }
    bool OnMouseDown(const MouseEvent& e) override
    {
        if (e.button != MouseButton::Left)
            return false;
        m_pressed = true;
        m_mouse = e.pos;
        return true;
    }
    void OnMouseUp(const MouseEvent& e) override
    {
        const bool click = m_pressed && m_bounds.Contains(e.pos);
        m_pressed = false;
        if (click && onClick)
        {
            auto fn = onClick;
            const Point pt = e.pos;
            PostAction([fn, pt]() { fn(pt); });
        }
    }
    CursorType Cursor(Point pt) const override { return cursor ? cursor(pt) : CursorType::Arrow; }

private:
    Point m_mouse{-1e6f, -1e6f};
};

// Карточка параметра: значок, заголовок, описание, элемент управления справа или переход.
class Card : public Panel
{
public:
    Icon icon = Icon::None;
    AppIcon appIcon = AppIcon::None;
    std::wstring title;
    std::wstring desc;
    std::wstring value;      // текст справа (перед стрелкой)
    bool chevron = false;    // «›» — переход на страницу
    bool external = false;   // ссылка «открыть»
    bool expanded = false;   // «˅» (раскрытая группа)
    std::function<void()> onClick;
    float textRight = 0.0f;  // ширина, занятая элементом управления справа

    void Draw(Renderer& r) override
    {
        const Palette& p = Theme::P();
        const Rect& b = m_bounds;
        const bool hot = onClick && m_hovered && m_hover == nullptr;
        const Color bg = hot ? (m_pressed ? p.controlPressed : p.controlHover) : p.card;
        r.FillRoundRect(b, 4.0f, bg);
        r.StrokeRoundRect(b, 4.0f, p.cardStroke);
        float x = b.x + 20.0f;
        if (appIcon != AppIcon::None)
        {
            r.DrawAppIcon(appIcon, {x, b.CenterY() - 12.0f, 24.0f, 24.0f});
            x += 40.0f;
        }
        else if (icon != Icon::None)
        {
            r.Glyph(icon, {x, b.CenterY() - 10.0f, 20.0f, 20.0f}, m_enabled ? p.text : p.textDisabled);
            x += 38.0f;
        }
        float right = b.Right() - 16.0f - textRight;
        if (chevron || external || expanded)
        {
            const Icon g = external ? Icon::OpenWith : (expanded ? Icon::ChevronDown : Icon::ChevronRight);
            r.Glyph(g, {b.Right() - 30.0f, b.CenterY() - 6.0f, 12.0f, 12.0f}, p.text);
            right = b.Right() - 44.0f - textRight;
        }
        if (!value.empty())
        {
            const float vw = r.TextWidth(value, 14.0f) + 4.0f;
            r.Text(value, {right - vw, b.y, vw, b.h}, p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::VCenter | TextFlags::Right);
            right -= vw + 12.0f;
        }
        const Color tc = m_enabled ? p.text : p.textDisabled;
        if (desc.empty())
            r.Text(title, {x, b.y, right - x, b.h}, tc, 14.0f, FontWeight::Regular, TextFlags::VCenter);
        else
        {
            r.Text(title, {x, b.CenterY() - 19.0f, right - x, 20.0f}, tc, 14.0f);
            r.Text(desc, {x, b.CenterY() + 1.0f, right - x, 18.0f}, m_enabled ? p.textSecondary : p.textDisabled, 12.0f);
        }
        Panel::Draw(r);
    }

    bool OnMouseDown(const MouseEvent& e) override
    {
        if (Panel::OnMouseDown(e))
            return true;
        if (onClick && e.button == MouseButton::Left)
        {
            m_pressed = true;
            return true;
        }
        return false;
    }

    void OnMouseUp(const MouseEvent& e) override
    {
        if (m_pressed)
        {
            m_pressed = false;
            if (m_bounds.Contains(e.pos) && onClick)
                PostAction(onClick);
            return;
        }
        Panel::OnMouseUp(e);
    }
};

// ---------------------------------------------------------------------------
// Описание страниц
// ---------------------------------------------------------------------------
struct PageInfo
{
    const wchar_t* key;
    const wchar_t* title;
    const wchar_t* parent; // nullptr — раздел верхнего уровня
    Icon icon;
    uint32_t color;
};

const std::vector<PageInfo>& Pages()
{
    static const std::vector<PageInfo> pages = {
        {L"home", L"Главная", nullptr, Icon::Home, 0x3B82D6},
        {L"system", L"Система", nullptr, Icon::Monitor, 0x0A74D6},
        {L"bluetooth", L"Bluetooth и устройства", nullptr, Icon::Bluetooth, 0x1A7FD8},
        {L"network", L"Сеть и Интернет", nullptr, Icon::Wifi, 0x3E69C9},
        {L"personalization", L"Персонализация", nullptr, Icon::Brush, 0xE0742B},
        {L"apps", L"Приложения", nullptr, Icon::Apps, 0x6A5FC9},
        {L"accounts", L"Учётные записи", nullptr, Icon::User, 0x1E9E52},
        {L"time", L"Время и язык", nullptr, Icon::Globe, 0x1688C9},
        {L"gaming", L"Игры", nullptr, Icon::Game, 0x7A7A7A},
        {L"accessibility", L"Специальные возможности", nullptr, Icon::Accessibility, 0x2A63D8},
        {L"privacy", L"Конфиденциальность и защита", nullptr, Icon::Shield, 0x6B7280},
        {L"update", L"Центр обновления Windows", nullptr, Icon::Update, 0x0A74D6},
        // Вложенные страницы
        {L"display", L"Дисплей", L"system", Icon::Display, 0},
        {L"sound", L"Звук", L"system", Icon::Sound, 0},
        {L"notifications", L"Уведомления", L"system", Icon::Bell, 0},
        {L"focus", L"Фокусировка", L"system", Icon::Focus, 0},
        {L"power", L"Питание и батарея", L"system", Icon::Power, 0},
        {L"storage", L"Память", L"system", Icon::Storage, 0},
        {L"multitasking", L"Многозадачность", L"system", Icon::Multitask, 0},
        {L"clipboard", L"Буфер обмена", L"system", Icon::Paste, 0},
        {L"about", L"О системе", L"system", Icon::Info, 0},
        {L"devices", L"Устройства", L"bluetooth", Icon::Keyboard, 0},
        {L"mouse", L"Мышь", L"bluetooth", Icon::Mouse, 0},
        {L"wifi", L"Wi-Fi", L"network", Icon::Wifi, 0},
        {L"background", L"Фон", L"personalization", Icon::Pictures, 0},
        {L"colors", L"Цвета", L"personalization", Icon::Palette, 0},
        {L"themes", L"Темы", L"personalization", Icon::Brush, 0},
        {L"lockscreen", L"Экран блокировки", L"personalization", Icon::Lock, 0},
        {L"start", L"Пуск", L"personalization", Icon::Apps, 0},
        {L"taskbar", L"Панель задач", L"personalization", Icon::TaskView, 0},
        {L"installed", L"Установленные приложения", L"apps", Icon::Apps, 0},
        {L"startup", L"Автозагрузка", L"apps", Icon::Startup, 0},
        {L"yourinfo", L"Ваши данные", L"accounts", Icon::User, 0},
        {L"signin", L"Варианты входа", L"accounts", Icon::Lock, 0},
        {L"datetime", L"Дата и время", L"time", Icon::Clock, 0},
        {L"language", L"Язык и регион", L"time", Icon::Language, 0},
        {L"visualeffects", L"Визуальные эффекты", L"accessibility", Icon::Eye, 0},
        {L"textsize", L"Размер текста", L"accessibility", Icon::TextTool, 0},
        {L"history", L"Журнал обновлений", L"update", Icon::History, 0},
        {L"search", L"Результаты поиска", nullptr, Icon::Search, 0},
    };
    return pages;
}

const PageInfo* FindPage(const std::wstring& key)
{
    for (const PageInfo& p : Pages())
    {
        if (key == p.key)
            return &p;
    }
    return nullptr;
}

std::wstring TopOf(const std::wstring& key)
{
    const PageInfo* p = FindPage(key);
    while (p && p->parent)
        p = FindPage(p->parent);
    return p ? p->key : L"home";
}

// Индекс поиска: отдельные параметры → страница.
struct SearchEntry
{
    const wchar_t* text;
    const wchar_t* page;
};
const std::vector<SearchEntry>& SearchIndex()
{
    static const std::vector<SearchEntry> idx = {
        {L"Яркость", L"display"}, {L"Ночной свет", L"display"}, {L"Разрешение экрана", L"display"},
        {L"Масштаб", L"display"}, {L"Ориентация дисплея", L"display"}, {L"Громкость", L"sound"},
        {L"Устройство вывода звука", L"sound"}, {L"Микрофон", L"sound"}, {L"Уведомления", L"notifications"},
        {L"Не беспокоить", L"notifications"}, {L"Фокусировка", L"focus"}, {L"Экономия заряда", L"power"},
        {L"Режим питания", L"power"}, {L"Батарея", L"power"}, {L"Память", L"storage"},
        {L"Контроль памяти", L"storage"}, {L"Прикрепление окон", L"multitasking"}, {L"Многозадачность", L"multitasking"},
        {L"Буфер обмена", L"clipboard"}, {L"О системе", L"about"}, {L"Имя компьютера", L"about"},
        {L"Переименовать этот компьютер", L"about"}, {L"Процессор", L"about"}, {L"Оперативная память", L"about"},
        {L"Bluetooth", L"bluetooth"}, {L"Мышь", L"mouse"}, {L"Скорость указателя", L"mouse"},
        {L"Устройства", L"devices"}, {L"Wi-Fi", L"wifi"}, {L"Режим «в самолёте»", L"network"},
        {L"Сеть", L"network"}, {L"Прокси-сервер", L"network"}, {L"VPN", L"network"},
        {L"Фон рабочего стола", L"background"}, {L"Обои", L"background"}, {L"Цвета", L"colors"},
        {L"Тёмная тема", L"colors"}, {L"Светлая тема", L"colors"}, {L"Цвет элементов", L"colors"},
        {L"Акцентный цвет", L"colors"}, {L"Эффекты прозрачности", L"colors"}, {L"Темы", L"themes"},
        {L"Экран блокировки", L"lockscreen"}, {L"Пуск", L"start"}, {L"Панель задач", L"taskbar"},
        {L"Выравнивание панели задач", L"taskbar"}, {L"Поиск на панели задач", L"taskbar"}, {L"Мини-приложения", L"taskbar"},
        {L"Представление задач", L"taskbar"}, {L"Установленные приложения", L"installed"}, {L"Удалить приложение", L"installed"},
        {L"Автозагрузка", L"startup"}, {L"Приложения по умолчанию", L"apps"}, {L"Учётные записи", L"accounts"},
        {L"Ваши данные", L"yourinfo"}, {L"Пароль", L"signin"}, {L"ПИН-код", L"signin"}, {L"Дата и время", L"datetime"},
        {L"Часовой пояс", L"datetime"}, {L"Язык", L"language"}, {L"Регион", L"language"}, {L"Игровой режим", L"gaming"},
        {L"Эффекты анимации", L"visualeffects"}, {L"Полосы прокрутки", L"visualeffects"}, {L"Размер текста", L"textsize"},
        {L"Специальные возможности", L"accessibility"}, {L"Экранный диктор", L"accessibility"}, {L"Лупа", L"accessibility"},
        {L"Конфиденциальность", L"privacy"}, {L"Расположение", L"privacy"}, {L"Камера", L"privacy"},
        {L"Центр обновления", L"update"}, {L"Проверить наличие обновлений", L"update"}, {L"Журнал обновлений", L"history"},
    };
    return idx;
}

// ---------------------------------------------------------------------------
// Сведения о системе
// ---------------------------------------------------------------------------
std::wstring TimeZoneText()
{
    TIME_ZONE_INFORMATION tz{};
    GetTimeZoneInformation(&tz);
    const long bias = -tz.Bias;
    const long ah = std::labs(bias) / 60, am = std::labs(bias) % 60;
    std::wstring s = L"(UTC" + std::wstring(bias >= 0 ? L"+" : L"-") + Pad2(static_cast<int>(ah)) + L":" + Pad2(static_cast<int>(am)) + L")";
    if (bias == 180)
        s += L" Москва, Санкт-Петербург";
    else if (bias == 0)
        s += L" Время в формате UTC";
    else if (tz.StandardName[0])
        s += L" " + std::wstring(tz.StandardName);
    return s;
}

// ---------------------------------------------------------------------------
class SettingsWindow : public Window
{
public:
    explicit SettingsWindow(const std::wstring& arg) : Window(AppId::Settings, L"Параметры")
    {
        SetInitialSize(1180.0f, 770.0f);
        m_minSize = {760.0f, 500.0f};
        m_back = m_ui.Add<Button>(L"", Icon::ArrowLeft, ButtonStyle::Subtle);
        m_back->SetTooltip(L"Назад");
        m_back->onClick = [this]() { GoBack(); };
        m_search = m_ui.Add<TextBox>(L"Найти параметр");
        m_search->trailingIcon = Icon::Search;
        m_search->onChange = [this](const std::wstring& q) { OnSearch(q); };
        m_search->onEnter = [this]() {
            const std::vector<SearchEntry> res = SearchResults(Trim(m_search->Text()));
            if (!res.empty())
                OpenSearchResult(res.front().page);
        };
        m_search->onEscape = [this]() {
            m_search->SetText(L"", true);
            m_ui.SetFocus(nullptr);
        };
        m_nav = m_ui.Add<ScrollPanel>();
        for (const PageInfo& pi : Pages())
        {
            if (pi.parent || std::wstring(pi.key) == L"search")
                continue;
            auto* item = m_nav->Add<NavItem>(pi.title, pi.icon);
            item->hasIconColor = true;
            item->iconColor = Color::Hex(pi.color);
            const std::wstring key = pi.key;
            item->onClick = [this, key]() { Navigate(key); };
            m_navItems.emplace_back(item, key);
        }
        m_page = m_ui.Add<ScrollPanel>();
        m_current = FindPage(arg) ? arg : L"home";
        BuildPage();
    }

    void OnLaunchArgs(const std::wstring& arg) override
    {
        if (FindPage(arg))
            Navigate(arg);
    }

    float TitleBarHeight() const override { return kTitleH; }
    bool ShowTitle() const override { return false; }

    void OnActivate(bool active) override
    {
        // Значения могли измениться в других местах (быстрые настройки и т. п.).
        if (active)
            Rebuild();
    }

    void OnTimer1s() override
    {
        if (m_current == L"time" || m_current == L"datetime" || m_current == L"update" || m_current == L"home")
            RequestFrame();
    }

private:
    // ================================================================== навигация
    void Navigate(const std::wstring& key)
    {
        if (key == m_current)
            return;
        if (m_current != L"search")
            m_history.push_back(m_current);
        m_current = key;
        if (!m_search->Text().empty() && key != L"search")
            m_search->SetText(L"");
        BuildPage();
    }

    void GoBack()
    {
        if (m_history.empty())
            return;
        m_current = m_history.back();
        m_history.pop_back();
        m_search->SetText(L"");
        BuildPage();
    }

    void OnSearch(const std::wstring& q)
    {
        const std::wstring t = Trim(q);
        if (t.empty())
        {
            if (m_current == L"search")
            {
                m_current = m_beforeSearch.empty() ? L"home" : m_beforeSearch;
                Rebuild();
            }
            return;
        }
        if (m_current != L"search")
            m_beforeSearch = m_current;
        m_current = L"search";
        m_query = t;
        Rebuild();
    }

    void OpenSearchResult(const std::wstring& page)
    {
        m_history.push_back(m_beforeSearch.empty() ? L"home" : m_beforeSearch);
        m_current = page;
        m_search->SetText(L"");
        m_ui.SetFocus(nullptr);
        BuildPage();
    }

    static std::vector<SearchEntry> SearchResults(const std::wstring& q)
    {
        std::vector<SearchEntry> v;
        if (q.empty())
            return v;
        for (const SearchEntry& e : SearchIndex())
        {
            if (ContainsNoCase(e.text, q))
                v.push_back(e);
        }
        // Названия страниц, которых нет в индексе.
        for (const PageInfo& p : Pages())
        {
            if (std::wstring(p.key) == L"search" || !ContainsNoCase(p.title, q))
                continue;
            bool dup = false;
            for (const SearchEntry& e : v)
                dup = dup || CompareNoCase(e.text, p.title) == 0;
            if (!dup)
                v.push_back({p.title, p.key});
        }
        return v;
    }

    // Перестроить страницу, сохранив прокрутку (безопасно из обработчиков элементов).
    void Rebuild()
    {
        PostAction([this]() {
            const float keep = m_page->ScrollY();
            BuildPage();
            m_page->ScrollTo(keep, false);
        });
    }

    // ================================================================== раскладка
    float NavW() const { return Clamp(Width() * 0.27f, 250.0f, 320.0f); }
    float CX() const { return NavW() + 24.0f; }
    float CW() const { return std::min(1040.0f, Width() - CX() - 32.0f); }
    float PageTop() const { return kTitleH + kHeaderH; }

    void OnResize() override
    {
        m_back->SetBounds({6.0f, 6.0f, 40.0f, 36.0f});
        const float nw = NavW();
        m_search->SetBounds({16.0f, kTitleH + 108.0f, nw - 32.0f, 34.0f});
        const float navTop = kTitleH + 156.0f;
        m_nav->SetBounds({0.0f, navTop, nw, Height() - navTop});
        float y = navTop + 2.0f;
        for (auto& it : m_navItems)
        {
            it.first->SetBounds({8.0f, y, nw - 16.0f, 36.0f});
            y += 40.0f;
        }
        m_nav->contentHeight = y - navTop + 8.0f;
        m_page->SetBounds({nw + 8.0f, PageTop(), Width() - nw - 8.0f, Height() - PageTop()});
        if (std::fabs(m_builtW - Width()) > 0.5f)
        {
            const float keep = m_page->ScrollY();
            BuildPage();
            m_page->ScrollTo(keep, false);
        }
    }

    // ---- строительные блоки страниц --------------------------------------------------
    Card* AddCard(Icon icon, const std::wstring& title, const std::wstring& desc = L"", float h = 0.0f)
    {
        auto* c = m_page->Add<Card>();
        c->icon = icon;
        c->title = title;
        c->desc = desc;
        const float hh = h > 0.0f ? h : (desc.empty() ? 52.0f : 68.0f);
        c->SetBounds({CX(), m_y, CW(), hh});
        m_y += hh + 4.0f;
        return c;
    }

    Rect PageToScreen(const Rect& rc) const { return LocalToScreen(rc.Translated(0.0f, -m_page->ScrollY())); }

    static Rect RightSlot(const Card* c, float w, float h = 32.0f)
    {
        const Rect& b = c->Bounds();
        return {b.Right() - 16.0f - w, b.CenterY() - h * 0.5f, w, h};
    }

    Card* AddLink(Icon icon, const std::wstring& title, const std::wstring& desc, const std::wstring& page)
    {
        Card* c = AddCard(icon, title, desc);
        c->chevron = true;
        c->onClick = [this, page]() { Navigate(page); };
        return c;
    }

    Card* AddInfo(Icon icon, const std::wstring& title, const std::wstring& desc = L"")
    {
        Card* c = AddCard(icon, title, desc);
        c->chevron = true;
        c->onClick = [this, title]() {
            ShowDialog(title, L"Этот раздел параметров показан в демонстрационном режиме.", {{L"ОК", true}}, nullptr);
        };
        return c;
    }

    ToggleSwitch* AddToggle(Icon icon, const std::wstring& title, const std::wstring& desc, bool on,
                            std::function<void(bool)> fn, bool enabled = true)
    {
        Card* c = AddCard(icon, title, desc);
        auto* t = c->Add<ToggleSwitch>(on);
        t->SetBounds(RightSlot(c, 116.0f));
        t->onChange = std::move(fn);
        t->SetEnabled(enabled);
        c->SetEnabled(enabled);
        c->textRight = 116.0f;
        return t;
    }

    // Переключатель, значение которого просто запоминается (параметр без влияния на оболочку).
    ToggleSwitch* AddStateToggle(Icon icon, const std::wstring& title, const std::wstring& desc, const std::wstring& id, bool def,
                                 bool enabled = true)
    {
        auto it = m_state.find(id);
        const bool on = it == m_state.end() ? def : it->second != 0;
        return AddToggle(icon, title, desc, on, [this, id](bool v) { m_state[id] = v ? 1 : 0; }, enabled);
    }

    ComboBox* AddCombo(Icon icon, const std::wstring& title, const std::wstring& desc, std::vector<std::wstring> items, int sel,
                       std::function<void(int)> fn, float w = 200.0f)
    {
        Card* c = AddCard(icon, title, desc);
        auto* cb = c->Add<ComboBox>();
        cb->items = std::move(items);
        cb->selected = sel;
        cb->SetBounds(RightSlot(c, w));
        auto f = std::move(fn);
        cb->onChange = [f](int i) {
            PostAction([f, i]() {
                if (f)
                    f(i);
            });
        };
        c->textRight = w;
        return cb;
    }

    ComboBox* AddStateCombo(Icon icon, const std::wstring& title, const std::wstring& desc, std::vector<std::wstring> items,
                            const std::wstring& id, int def, float w = 200.0f)
    {
        auto it = m_state.find(id);
        const int sel = it == m_state.end() ? def : it->second;
        return AddCombo(icon, title, desc, std::move(items), sel, [this, id](int i) { m_state[id] = i; }, w);
    }

    Button* AddButtonCard(Icon icon, const std::wstring& title, const std::wstring& desc, const std::wstring& text,
                          std::function<void()> fn, bool accent = false)
    {
        Card* c = AddCard(icon, title, desc);
        const float w = Renderer::Get().TextWidth(text, 14.0f) + 34.0f;
        auto* b = c->Add<Button>(text, Icon::None, accent ? ButtonStyle::Accent : ButtonStyle::Standard);
        b->SetBounds(RightSlot(c, w));
        b->onClick = std::move(fn);
        c->textRight = w;
        return b;
    }

    Slider* AddSlider(Icon icon, const std::wstring& title, const std::wstring& desc, float value, std::function<void(float)> fn,
                      Icon leftIcon = Icon::None, Icon rightIcon = Icon::None)
    {
        Card* c = AddCard(icon, title, desc);
        const float w = std::min(300.0f, CW() * 0.38f);
        auto* s = c->Add<Slider>();
        s->value = Saturate(value);
        s->SetBounds(RightSlot(c, w));
        s->onChange = std::move(fn);
        c->textRight = w + 8.0f;
        if (leftIcon != Icon::None || rightIcon != Icon::None)
        {
            const Rect sb = s->Bounds();
            s->SetBounds({sb.x + 26.0f, sb.y, sb.w - 26.0f, sb.h});
            auto* cv = c->Add<Canvas>();
            cv->SetBounds({sb.x, sb.y, 22.0f, sb.h});
            cv->draw = [leftIcon](Renderer& r, const Rect& b, Point) {
                r.Glyph(leftIcon, {b.x + 2.0f, b.CenterY() - 8.0f, 16.0f, 16.0f}, Theme::P().text);
            };
        }
        return s;
    }

    void AddGroup(const std::wstring& text)
    {
        m_y += 18.0f;
        auto* l = m_page->Add<Label>(text, 14.0f, FontWeight::Semibold);
        l->SetBounds({CX() + 2.0f, m_y, CW(), 22.0f});
        m_y += 30.0f;
    }

    Canvas* AddCanvas(float h, std::function<void(Renderer&, const Rect&, Point)> draw, std::function<void(Point)> click = nullptr)
    {
        auto* cv = m_page->Add<Canvas>();
        cv->SetBounds({CX(), m_y, CW(), h});
        cv->draw = std::move(draw);
        if (click)
        {
            cv->interactive = true;
            cv->onClick = std::move(click);
        }
        m_y += h + 4.0f;
        return cv;
    }

    // Подложка «геройской» карточки.
    static void HeroBg(Renderer& r, const Rect& b)
    {
        const Palette& p = Theme::P();
        r.FillRoundRect(b, 8.0f, p.card);
        r.StrokeRoundRect(b, 8.0f, p.cardStroke);
    }

    // ================================================================== построение страниц
    void BuildPage()
    {
        m_builtW = Width();
        m_page->Clear();
        m_y = PageTop() + 2.0f;
        m_hero = nullptr;
        const std::wstring& k = m_current;
        if (k == L"home")
            BuildHome();
        else if (k == L"system")
            BuildSystem();
        else if (k == L"display")
            BuildDisplay();
        else if (k == L"sound")
            BuildSound();
        else if (k == L"notifications")
            BuildNotifications();
        else if (k == L"focus")
            BuildFocus();
        else if (k == L"power")
            BuildPower();
        else if (k == L"storage")
            BuildStorage();
        else if (k == L"multitasking")
            BuildMultitasking();
        else if (k == L"clipboard")
            BuildClipboard();
        else if (k == L"about")
            BuildAbout();
        else if (k == L"bluetooth")
            BuildBluetooth();
        else if (k == L"devices")
            BuildDevices();
        else if (k == L"mouse")
            BuildMouse();
        else if (k == L"network")
            BuildNetwork();
        else if (k == L"wifi")
            BuildWifi();
        else if (k == L"personalization")
            BuildPersonalization();
        else if (k == L"background")
            BuildBackground();
        else if (k == L"colors")
            BuildColors();
        else if (k == L"themes")
            BuildThemes();
        else if (k == L"lockscreen")
            BuildLockScreen();
        else if (k == L"start")
            BuildStart();
        else if (k == L"taskbar")
            BuildTaskbar();
        else if (k == L"apps")
            BuildApps();
        else if (k == L"installed")
            BuildInstalled();
        else if (k == L"startup")
            BuildStartup();
        else if (k == L"accounts")
            BuildAccounts();
        else if (k == L"yourinfo")
            BuildYourInfo();
        else if (k == L"signin")
            BuildSignIn();
        else if (k == L"time")
            BuildTime();
        else if (k == L"datetime")
            BuildDateTime();
        else if (k == L"language")
            BuildLanguage();
        else if (k == L"gaming")
            BuildGaming();
        else if (k == L"accessibility")
            BuildAccessibility();
        else if (k == L"visualeffects")
            BuildVisualEffects();
        else if (k == L"textsize")
            BuildTextSize();
        else if (k == L"privacy")
            BuildPrivacy();
        else if (k == L"update")
            BuildUpdate();
        else if (k == L"history")
            BuildHistory();
        else if (k == L"search")
            BuildSearch();
        m_y += 24.0f;
        m_page->contentHeight = m_y - PageTop();
        m_page->ScrollTo(0.0f, false);
        // Выделение в навигации
        const std::wstring top = TopOf(m_current);
        for (auto& it : m_navItems)
            it.first->selected = m_current != L"search" && it.second == top;
        m_back->SetEnabled(!m_history.empty());
        const PageInfo* pi = FindPage(m_current);
        SetTitle(pi && m_current != L"home" ? std::wstring(L"Параметры — ") + pi->title : std::wstring(L"Параметры"));
    }

    // ---- Главная ------------------------------------------------------------------
    void BuildHome()
    {
        ShellSettings& st = GetShell().Settings();
        AddCanvas(176.0f,
                  [this](Renderer& r, const Rect& b, Point m) {
                      const Palette& p = Theme::P();
                      const ShellSettings& s = GetShell().Settings();
                      HeroBg(r, b);
                      DrawDevice(r, {b.x + 24.0f, b.y + 26.0f, 168.0f, 118.0f});
                      const float tw = std::max(80.0f, HomeTile(b, 0).x - b.x - 224.0f);
                      r.Text(s.computerName, {b.x + 212.0f, b.y + 44.0f, tw, 28.0f}, p.text, 20.0f, FontWeight::Semibold);
                      r.Text(L"Демонстрационный ПК", {b.x + 212.0f, b.y + 74.0f, tw, 20.0f}, p.textSecondary, 14.0f);
                      const Rect ren = HomeRenameRect(b);
                      r.Text(L"Переименовать", ren, ren.Contains(m) ? p.accentTextSecondary : p.accentText, 14.0f);
                      // Плитки состояния справа
                      for (int i = 0; i < 2; ++i)
                      {
                          const Rect t = HomeTile(b, i);
                          if (t.w < 120.0f)
                              continue;
                          const bool hot = t.Contains(m);
                          r.FillRoundRect(t, 6.0f, hot ? p.controlHover : p.layerAlt);
                          r.StrokeRoundRect(t, 6.0f, p.cardStroke);
                          if (i == 0)
                          {
                              r.Glyph(s.wifi && !s.airplane ? Icon::Wifi : Icon::WifiOff, {t.x + 16.0f, t.CenterY() - 12.0f, 24.0f, 24.0f}, p.accentText);
                              r.Text(s.wifi && !s.airplane ? s.network : std::wstring(L"Нет подключения"), {t.x + 52.0f, t.y + 14.0f, t.w - 60.0f, 20.0f}, p.text, 14.0f,
                                     FontWeight::Semibold);
                              r.Text(s.wifi && !s.airplane ? L"Подключено, защищено" : L"Wi-Fi отключён", {t.x + 52.0f, t.y + 36.0f, t.w - 60.0f, 18.0f},
                                     p.textSecondary, 12.0f);
                          }
                          else
                          {
                              r.Glyph(Icon::Update, {t.x + 16.0f, t.CenterY() - 12.0f, 24.0f, 24.0f}, p.accentText);
                              r.Text(L"Центр обновления Windows", {t.x + 52.0f, t.y + 14.0f, t.w - 60.0f, 20.0f}, p.text, 14.0f, FontWeight::Semibold);
                              r.Text(UpdateStatusShort(), {t.x + 52.0f, t.y + 36.0f, t.w - 60.0f, 18.0f}, p.textSecondary, 12.0f);
                          }
                      }
                  },
                  [this](Point pt) {
                      const Rect b = HeroRect();
                      if (HomeRenameRect(b).Contains(pt))
                          RenamePC();
                      else if (HomeTile(b, 0).Contains(pt))
                          Navigate(L"network");
                      else if (HomeTile(b, 1).Contains(pt))
                          Navigate(L"update");
                  });
        m_hero = static_cast<Canvas*>(m_page->Children().back().get());
        m_hero->cursor = [this](Point pt) {
            const Rect b = HeroRect();
            return HomeRenameRect(b).Contains(pt) || HomeTile(b, 0).Contains(pt) || HomeTile(b, 1).Contains(pt) ? CursorType::Hand
                                                                                                                : CursorType::Arrow;
        };
        AddGroup(L"Рекомендуемые параметры");
        AddLink(Icon::Display, L"Дисплей", L"Мониторы, яркость, ночной свет, профиль дисплея", L"display");
        AddLink(Icon::Sound, L"Звук", L"Уровни громкости, вывод, ввод, звуковые устройства", L"sound");
        AddLink(Icon::TaskView, L"Панель задач", L"Поведение панели задач, системные значки", L"taskbar");
        AddGroup(L"Персонализируйте устройство");
        const float th = 92.0f;
        AddCanvas(th + 76.0f,
                  [](Renderer& r, const Rect& b, Point m) {
                      const Palette& p = Theme::P();
                      HeroBg(r, b);
                      r.Text(L"Выберите тему, чтобы изменить вид рабочего стола и цвета", {b.x + 20.0f, b.y + 14.0f, b.w - 40.0f, 20.0f}, p.text,
                             14.0f);
                      for (int i = 0; i < 6; ++i)
                      {
                          const Rect t = ThemeTileRect(b, i, 6, 44.0f);
                          if (t.w <= 0.0f)
                              continue;
                          DrawThemeTile(r, t, i, Wallpapers()[static_cast<size_t>(i)].dark, GetShell().Settings().wallpaper == i, t.Contains(m));
                      }
                      r.Text(L"Обзор других фонов, цветов и тем", {b.x + 20.0f, b.Bottom() - 32.0f, b.w - 40.0f, 20.0f}, p.accentText, 14.0f);
                  },
                  [this](Point pt) {
                      const Rect b = m_lastCanvas;
                      for (int i = 0; i < 6; ++i)
                      {
                          if (ThemeTileRect(b, i, 6, 44.0f).Contains(pt))
                          {
                              ApplyTheme(i);
                              return;
                          }
                      }
                      if (pt.y > b.Bottom() - 40.0f)
                          Navigate(L"personalization");
                  });
        m_lastCanvas = m_page->Children().back()->Bounds();
        AddGroup(L"Bluetooth-устройства");
        AddToggle(Icon::Bluetooth, L"Bluetooth", st.bluetooth ? L"Обнаруживается как «" + st.computerName + L"»" : L"Отключено",
                  st.bluetooth, [this](bool v) {
                      GetShell().Settings().bluetooth = v;
                      Rebuild();
                  });
        AddLink(Icon::Mouse, L"Беспроводная мышь", L"Подключено", L"devices");
        AddLink(Icon::Keyboard, L"Клавиатура", L"Подключено", L"devices");
    }

    Rect HeroRect() const { return m_hero ? m_hero->Bounds() : Rect{}; }
    static Rect HomeRenameRect(const Rect& b) { return {b.x + 212.0f, b.y + 104.0f, 120.0f, 20.0f}; }
    // Плитки состояния: рядом при широком окне, друг под другом — при узком.
    static Rect HomeTile(const Rect& b, int i)
    {
        if (b.w >= 1000.0f)
        {
            const float w = 280.0f;
            return {b.Right() - 20.0f - (w + 10.0f) * static_cast<float>(2 - i) + 10.0f, b.CenterY() - 34.0f, w, 68.0f};
        }
        const float w = std::min(280.0f, b.w - 560.0f);
        return {b.Right() - 20.0f - w, b.y + 20.0f + static_cast<float>(i) * 70.0f, w, 64.0f};
    }
    static Rect ThemeTileRect(const Rect& b, int i, int count, float top)
    {
        const float gap = 12.0f;
        const float w = std::min(150.0f, (b.w - 40.0f - gap * static_cast<float>(count - 1)) / static_cast<float>(count));
        return {b.x + 20.0f + static_cast<float>(i) * (w + gap), b.y + top, w, w * 0.5625f};
    }

    void ApplyTheme(int wallpaper)
    {
        Shell& sh = GetShell();
        const bool dark = Wallpapers()[static_cast<size_t>(wallpaper)].dark;
        if (Theme::Dark() != dark)
            sh.SetDark(dark);
        sh.SetWallpaper(wallpaper);
        Rebuild();
    }

    void RenamePC()
    {
        ShowInputDialog(L"Переименование компьютера", L"Новое имя может содержать буквы, дефисы и цифры.",
                        GetShell().Settings().computerName, L"Далее", [this](const std::wstring& name) {
                            std::wstring n = Trim(name);
                            if (n.empty())
                                return;
                            for (wchar_t& c : n)
                            {
                                if (c == L' ')
                                    c = L'-';
                            }
                            GetShell().Settings().computerName = n.substr(0, 15);
                            Rebuild();
                            ShowDialog(L"Переименование компьютера",
                                       L"Новое имя «" + GetShell().Settings().computerName + L"» будет применено после перезагрузки.",
                                       {{L"Перезагрузить позже", true}}, nullptr);
                        });
    }

    // ---- Система ------------------------------------------------------------------
    void BuildSystem()
    {
        AddCanvas(132.0f,
                  [this](Renderer& r, const Rect& b, Point m) {
                      const Palette& p = Theme::P();
                      HeroBg(r, b);
                      DrawDevice(r, {b.x + 22.0f, b.y + 20.0f, 128.0f, 92.0f});
                      r.Text(GetShell().Settings().computerName, {b.x + 170.0f, b.y + 32.0f, b.w * 0.5f, 26.0f}, p.text, 20.0f, FontWeight::Semibold);
                      r.Text(L"Демонстрационный ПК", {b.x + 170.0f, b.y + 60.0f, b.w * 0.5f, 20.0f}, p.textSecondary, 14.0f);
                      const Rect ren{b.x + 170.0f, b.y + 86.0f, 120.0f, 20.0f};
                      r.Text(L"Переименовать", ren, ren.Contains(m) ? p.accentTextSecondary : p.accentText, 14.0f);
                      const Rect upd{b.Right() - 260.0f, b.CenterY() - 26.0f, 240.0f, 52.0f};
                      if (b.w > 640.0f)
                      {
                          r.Glyph(Icon::Update, {upd.x, upd.CenterY() - 12.0f, 24.0f, 24.0f}, p.accentText);
                          r.Text(L"Центр обновления Windows", {upd.x + 36.0f, upd.y + 4.0f, upd.w - 36.0f, 20.0f}, p.text, 14.0f, FontWeight::Semibold);
                          r.Text(UpdateStatusShort(), {upd.x + 36.0f, upd.y + 26.0f, upd.w - 36.0f, 18.0f}, p.textSecondary, 12.0f);
                      }
                  },
                  [this](Point pt) {
                      const Rect b = HeroRect();
                      if (Rect{b.x + 170.0f, b.y + 86.0f, 120.0f, 20.0f}.Contains(pt))
                          RenamePC();
                      else if (b.w > 640.0f && Rect{b.Right() - 260.0f, b.CenterY() - 26.0f, 240.0f, 52.0f}.Contains(pt))
                          Navigate(L"update");
                  });
        m_hero = static_cast<Canvas*>(m_page->Children().back().get());
        m_y += 8.0f;
        AddLink(Icon::Display, L"Дисплей", L"Мониторы, яркость, ночной свет, профиль дисплея", L"display");
        AddLink(Icon::Sound, L"Звук", L"Уровни громкости, вывод, ввод, звуковые устройства", L"sound");
        AddLink(Icon::Bell, L"Уведомления", L"Оповещения от приложений и системы, режим «Не беспокоить»", L"notifications");
        AddLink(Icon::Focus, L"Фокусировка", L"Сеансы, параметры отвлекающих факторов", L"focus");
        AddLink(Icon::Power, L"Питание и батарея", L"Спящий режим, использование батареи, экономия заряда", L"power");
        AddLink(Icon::Storage, L"Память", L"Место на диске, диски, правила конфигурации", L"storage");
        AddInfo(Icon::Cast, L"Устройства рядом", L"Обнаруживаемость, расположение полученных файлов");
        AddLink(Icon::Multitask, L"Многозадачность", L"Прикрепление окон, рабочие столы, переключение задач", L"multitasking");
        AddInfo(Icon::Check, L"Активация", L"Состояние активации, подписки, ключ продукта");
        AddInfo(Icon::Settings, L"Устранение неполадок", L"Рекомендуемые средства устранения неполадок, параметры, журнал");
        AddInfo(Icon::Restart, L"Восстановление", L"Возврат компьютера в исходное состояние, расширенный запуск, возврат");
        AddInfo(Icon::Monitor, L"Удалённый рабочий стол", L"Пользователи удалённого рабочего стола, разрешения на подключение");
        AddLink(Icon::Paste, L"Буфер обмена", L"Журнал вырезания и копирования, синхронизация, очистка", L"clipboard");
        AddLink(Icon::Info, L"О системе", L"Характеристики устройства, переименование компьютера, характеристики Windows", L"about");
    }

    void BuildDisplay()
    {
        ShellSettings& st = GetShell().Settings();
        AddCanvas(150.0f, [](Renderer& r, const Rect& b, Point) {
            const Palette& p = Theme::P();
            HeroBg(r, b);
            const Rect mon{b.CenterX() - 64.0f, b.y + 24.0f, 128.0f, 80.0f};
            r.FillRoundRect(mon, 6.0f, p.accent);
            r.Text(L"1", mon, p.textOnAccent, 32.0f, FontWeight::Semibold, TextFlags::Middle);
            r.Text(L"Выберите дисплей, чтобы изменить его параметры", {b.x, b.Bottom() - 36.0f, b.w, 20.0f}, p.textSecondary, 12.0f,
                   FontWeight::Regular, TextFlags::Center);
        });
        AddGroup(L"Яркость и цвет");
        AddSlider(Icon::Brightness, L"Яркость", L"Настройка яркости встроенного дисплея", (st.brightness - 0.3f) / 0.7f,
                  [](float v) { GetShell().Settings().brightness = 0.3f + 0.7f * v; }, Icon::Brightness);
        AddToggle(Icon::NightLight, L"Ночной свет", L"Используйте более тёплые цвета, чтобы уменьшить синий свет", st.nightLight, [](bool v) {
            GetShell().Settings().nightLight = v;
            GetShell().ApplyNightLight();
        });
        AddStateToggle(Icon::Display, L"HDR", L"Более яркие и насыщенные видео, игры и приложения", L"hdr", false, false);
        AddGroup(L"Масштаб и макет");
        const int scalePct = RoundToInt(Renderer::Get().Scale() * 100.0f);
        AddCombo(Icon::ZoomIn, L"Масштаб", L"Изменение размера текста, приложений и других элементов",
                 {std::to_wstring(scalePct) + L"% (рекомендуется)"}, 0, nullptr, 220.0f);
        const int pw = GetSystemMetrics(SM_CXSCREEN), ph = GetSystemMetrics(SM_CYSCREEN);
        AddCombo(Icon::Display, L"Разрешение экрана", L"Настройка разрешения для подключённого дисплея",
                 {std::to_wstring(pw) + L" × " + std::to_wstring(ph) + L" (рекомендуется)"}, 0, nullptr, 240.0f);
        AddStateCombo(Icon::RotateRight, L"Ориентация дисплея", L"", {L"Альбомная", L"Книжная", L"Альбомная (перевёрнутая)", L"Книжная (перевёрнутая)"},
                      L"orientation", 0, 220.0f);
        AddGroup(L"Сопутствующие параметры");
        AddInfo(Icon::Monitor, L"Расширенные параметры дисплея", L"Сведения о дисплее, частота обновления");
        AddInfo(Icon::Gpu, L"Графика", L"");
    }

    void BuildSound()
    {
        ShellSettings& st = GetShell().Settings();
        AddGroup(L"Вывод");
        AddStateCombo(Icon::Sound, L"Выберите, где воспроизводить звук",
                      L"Для подключённых устройств могут быть доступны собственные параметры", {L"Динамики (High Definition Audio)", L"Наушники"},
                      L"output", 0, 280.0f);
        AddSlider(st.muted ? Icon::VolumeMute : Icon::Volume3, L"Громкость", L"", st.volume, [](float v) {
            GetShell().Settings().volume = v;
            if (v > 0.0f)
                GetShell().Settings().muted = false;
        }, Icon::Volume2);
        AddToggle(Icon::VolumeMute, L"Отключить звук", L"", st.muted, [this](bool v) {
            GetShell().Settings().muted = v;
            Rebuild();
        });
        AddStateToggle(Icon::Sound, L"Монофонический звук", L"Объединение левого и правого аудиоканалов в один", L"mono", false);
        AddGroup(L"Ввод");
        AddStateCombo(Icon::Sound, L"Выберите устройство для речи или записи", L"", {L"Микрофон (High Definition Audio)"}, L"input", 0, 280.0f);
        AddSlider(Icon::Sound, L"Громкость", L"", 0.7f, nullptr, Icon::Sound);
        AddGroup(L"Дополнительно");
        AddInfo(Icon::Settings, L"Устранение распространённых неполадок со звуком", L"Устройства вывода, устройства ввода");
        AddInfo(Icon::Apps, L"Микшер громкости", L"Микшер громкости приложений, устройства ввода и вывода приложений");
    }

    void BuildNotifications()
    {
        ShellSettings& st = GetShell().Settings();
        AddStateToggle(Icon::Bell, L"Уведомления", L"Получайте уведомления от приложений и других отправителей", L"notify", true);
        AddToggle(Icon::Focus, L"Не беспокоить", L"Уведомления будут отправляться непосредственно в центр уведомлений", st.focus,
                  [](bool v) { GetShell().Settings().focus = v; });
        AddStateCombo(Icon::Clock, L"Включать режим «Не беспокоить» автоматически", L"", {L"Никогда", L"Ежедневно с 22:00 до 07:00"},
                      L"dndauto", 0, 260.0f);
        AddGroup(L"Уведомления от приложений и других отправителей");
        const AppId apps[] = {AppId::Calendar, AppId::Mail, AppId::Settings, AppId::Tips, AppId::Weather, AppId::Store, AppId::Explorer};
        for (AppId a : apps)
        {
            const AppInfo& ai = GetAppInfo(a);
            Card* c = AddCard(Icon::None, ai.name, L"Баннеры, звуки");
            c->appIcon = ai.icon;
            auto* t = c->Add<ToggleSwitch>(true);
            t->SetBounds(RightSlot(c, 116.0f));
            c->textRight = 116.0f;
        }
    }

    void BuildFocus()
    {
        AddCanvas(190.0f, [](Renderer& r, const Rect& b, Point) {
            const Palette& p = Theme::P();
            HeroBg(r, b);
            r.Text(L"30", {b.CenterX() - 60.0f, b.y + 28.0f, 120.0f, 56.0f}, p.text, 48.0f, FontWeight::Semibold, TextFlags::Center);
            r.Text(L"мин", {b.CenterX() - 60.0f, b.y + 86.0f, 120.0f, 20.0f}, p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::Center);
            r.Text(L"Сеансы фокусировки помогают сосредоточиться: на время сеанса включается «Не беспокоить».", {b.x + 24.0f, b.y + 126.0f, b.w - 48.0f, 40.0f},
                   p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::Center | TextFlags::Wrap);
        });
        AddButtonCard(Icon::Focus, L"Начать сеанс фокусировки", L"Длительность: 30 минут", L"Начать сеанс фокусировки", [this]() {
            GetShell().Settings().focus = true;
            GetShell().Notify(AppIcon::Clock, L"Часы", L"Сеанс фокусировки начат", L"Режим «Не беспокоить» включён на 30 минут.");
            Rebuild();
        }, true);
        AddStateToggle(Icon::Clock, L"Показывать таймер в приложении «Часы»", L"", L"focustimer", true);
        AddStateToggle(Icon::Bell, L"Показывать индикаторы событий в приложениях на панели задач", L"", L"focusbadges", false);
        AddStateToggle(Icon::Flag, L"Включить режим «Не беспокоить»", L"", L"focusdnd", true);
    }

    void BuildPower()
    {
        ShellSettings& st = GetShell().Settings();
        AddCanvas(120.0f, [](Renderer& r, const Rect& b, Point) {
            const Palette& p = Theme::P();
            const ShellSettings& s = GetShell().Settings();
            HeroBg(r, b);
            r.BatteryIcon({b.x + 28.0f, b.CenterY() - 18.0f, 52.0f, 36.0f}, p.text, s.battery, s.charging);
            r.Text(std::to_wstring(RoundToInt(s.battery * 100.0f)) + L"%", {b.x + 100.0f, b.y + 26.0f, 200.0f, 40.0f}, p.text, 32.0f,
                   FontWeight::Semibold);
            r.Text(s.charging ? L"Идёт зарядка" : L"Осталось примерно 5 ч 20 мин", {b.x + 100.0f, b.y + 70.0f, 300.0f, 20.0f}, p.textSecondary, 14.0f);
            if (b.w > 600.0f)
            {
                r.Text(L"Уровень заряда за последние 24 часа", {b.Right() - 300.0f, b.y + 22.0f, 280.0f, 18.0f}, p.textSecondary, 12.0f);
                const Rect chart{b.Right() - 300.0f, b.y + 44.0f, 280.0f, 52.0f};
                for (int i = 0; i < 24; ++i)
                {
                    const float v = 0.45f + 0.4f * std::sin(static_cast<float>(i) * 0.45f) * 0.5f + static_cast<float>(i % 5) * 0.06f;
                    const float h = chart.h * Saturate(v);
                    r.FillRoundRect({chart.x + static_cast<float>(i) * 11.6f, chart.Bottom() - h, 8.0f, h}, 2.0f, p.accent.MulAlpha(0.85f));
                }
            }
        });
        AddGroup(L"Питание");
        AddInfo(Icon::Sleep, L"Экран и спящий режим", L"");
        AddStateCombo(Icon::Performance, L"Режим питания", L"Оптимизируйте устройство в зависимости от потребления энергии и производительности",
                      {L"Оптимальная энергоэффективность", L"Сбалансированный", L"Максимальная производительность"}, L"powermode", 1, 280.0f);
        AddGroup(L"Батарея");
        AddToggle(Icon::BatterySaver, L"Экономия заряда", L"Продлите время работы от батареи, ограничив некоторые уведомления и фоновую активность",
                  st.batterySaver, [](bool v) { GetShell().Settings().batterySaver = v; });
        AddInfo(Icon::Chart, L"Использование батареи", L"");
    }

    void BuildStorage()
    {
        VirtualFS& fs = GetShell().FS();
        FsNode* c = fs.DriveC();
        const unsigned long long cap = c->capacity, freeB = c->freeSpace, used = cap - freeB;
        AddCanvas(220.0f, [cap, used](Renderer& r, const Rect& b, Point) {
            const Palette& p = Theme::P();
            HeroBg(r, b);
            r.Text(L"Локальный диск (C:) — " + FormatSize(cap), {b.x + 24.0f, b.y + 18.0f, b.w - 48.0f, 22.0f}, p.text, 14.0f, FontWeight::Semibold);
            r.Text(L"Использовано: " + FormatSize(used), {b.x + 24.0f, b.y + 42.0f, 300.0f, 18.0f}, p.textSecondary, 12.0f);
            r.Text(L"Свободно: " + FormatSize(cap - used), {b.Right() - 324.0f, b.y + 42.0f, 300.0f, 18.0f}, p.textSecondary, 12.0f,
                   FontWeight::Regular, TextFlags::Right);
            const Rect bar{b.x + 24.0f, b.y + 68.0f, b.w - 48.0f, 8.0f};
            r.FillRoundRect(bar, 4.0f, p.dark ? Color(255, 255, 255, 0x20) : Color(0, 0, 0, 0x12));
            r.FillRoundRect({bar.x, bar.y, bar.w * static_cast<float>(static_cast<double>(used) / static_cast<double>(cap)), bar.h}, 4.0f, p.accent);
            // Категории
            const struct
            {
                const wchar_t* name;
                double share;
                Icon icon;
            } cats[] = {
                {L"Системные и зарезервированные", 0.29, Icon::Settings}, {L"Приложения и компоненты", 0.27, Icon::Apps},
                {L"Временные файлы", 0.07, Icon::Delete},                 {L"Документы", 0.05, Icon::Documents},
                {L"Изображения", 0.08, Icon::Pictures},                   {L"Другое", 0.24, Icon::Folder},
            };
            const float colW = (b.w - 48.0f) / 3.0f;
            for (int i = 0; i < 6; ++i)
            {
                const float x = b.x + 24.0f + static_cast<float>(i % 3) * colW;
                const float y = b.y + 96.0f + static_cast<float>(i / 3) * 56.0f;
                r.Glyph(cats[i].icon, {x, y + 4.0f, 16.0f, 16.0f}, p.text);
                r.Text(cats[i].name, {x + 26.0f, y, colW - 34.0f, 20.0f}, p.text, 12.0f);
                const unsigned long long sz = static_cast<unsigned long long>(static_cast<double>(used) * cats[i].share);
                r.Text(FormatSize(sz), {x + 26.0f, y + 20.0f, colW - 34.0f, 18.0f}, p.textSecondary, 12.0f);
                const Rect mini{x + 26.0f, y + 40.0f, colW - 50.0f, 3.0f};
                r.FillRoundRect(mini, 1.5f, p.dark ? Color(255, 255, 255, 0x20) : Color(0, 0, 0, 0x12));
                r.FillRoundRect({mini.x, mini.y, mini.w * static_cast<float>(cats[i].share / 0.3), mini.h}, 1.5f, p.accent);
            }
        });
        AddGroup(L"Управление хранилищем");
        AddStateToggle(Icon::Storage, L"Контроль памяти", L"Автоматическое освобождение места, удаление временных файлов и управление локальным содержимым",
                       L"storagesense", false);
        AddButtonCard(Icon::Lightbulb, L"Рекомендации по очистке", L"Временные файлы: " + FormatSize(used / 14), L"Очистить", [this]() {
            ShowDialog(L"Рекомендации по очистке", L"Временные файлы удалены. Освобождено " + FormatSize(GetShell().FS().DriveC()->capacity / 160) + L".",
                       {{L"ОК", true}}, nullptr);
        });
        AddInfo(Icon::Disk, L"Дополнительные параметры хранилища", L"Параметры резервного копирования, дисковые пространства, другие диски и тома");
    }

    void BuildMultitasking()
    {
        AddStateToggle(Icon::Multitask, L"Прикрепление окон", L"Автоматическое изменение размера и упорядочивание окон", L"snap", true);
        AddStateToggle(Icon::Maximize, L"Показывать макеты прикрепления при наведении на кнопку «Развернуть»", L"", L"snaplayouts", true);
        AddStateToggle(Icon::Minimize, L"Встряхивание заголовка окна", L"Если встряхнуть заголовок окна, все остальные окна свернутся", L"shake", false);
        AddStateCombo(Icon::TaskView, L"Рабочие столы", L"Показывать на панели задач окна, открытые на", {L"Только на этом рабочем столе", L"На всех рабочих столах"},
                      L"desktops", 0, 280.0f);
        AddStateCombo(Icon::Apps, L"Alt+Tab", L"При нажатии Alt+Tab показывать вкладки из приложений", {L"Последние 3 вкладки", L"Последние 5 вкладок", L"Не показывать вкладки"},
                      L"alttab", 0, 220.0f);
    }

    void BuildClipboard()
    {
        AddStateToggle(Icon::History, L"Журнал буфера обмена", L"Сохраняйте несколько элементов в буфере обмена для дальнейшего использования", L"cliphistory", false);
        AddStateToggle(Icon::Paste, L"Предлагаемые действия", L"Получайте предложения при копировании даты, номеров телефонов и т. д.", L"clipsuggest", true);
        AddButtonCard(Icon::Delete, L"Очистить данные буфера обмена", L"", L"Очистить", []() {
            if (OpenClipboard(GetShell().Hwnd()))
            {
                EmptyClipboard();
                CloseClipboard();
            }
        });
    }

    void BuildAbout()
    {
        ShellSettings& st = GetShell().Settings();
        AddCanvas(108.0f,
                  [](Renderer& r, const Rect& b, Point) {
                      const Palette& p = Theme::P();
                      HeroBg(r, b);
                      DrawDevice(r, {b.x + 20.0f, b.y + 14.0f, 104.0f, 80.0f});
                      r.Text(GetShell().Settings().computerName, {b.x + 144.0f, b.y + 30.0f, b.w * 0.5f, 24.0f}, p.text, 20.0f, FontWeight::Semibold);
                      r.Text(L"Демонстрационный ПК", {b.x + 144.0f, b.y + 58.0f, b.w * 0.5f, 20.0f}, p.textSecondary, 14.0f);
                  });
        {
            const Rect hb = m_page->Children().back()->Bounds();
            auto* rn = m_page->Add<Button>(L"Переименовать этот ПК");
            const float w = Renderer::Get().TextWidth(rn->text, 14.0f) + 34.0f;
            rn->SetBounds({hb.Right() - 20.0f - w, hb.CenterY() - 16.0f, w, 32.0f});
            rn->onClick = [this]() { RenamePC(); };
        }
        const std::vector<std::pair<std::wstring, std::wstring>> device = {
            {L"Имя устройства", st.computerName},
            {L"Процессор", SysInfo::ProcessorName()},
            {L"Оперативная память", SysInfo::InstalledMemoryText()},
            {L"Код устройства", L"8F3C2A1B-4D5E-4F60-9A7B-1C2D3E4F5A6B"},
            {L"Код продукта", L"00330-80000-00000-AA431"},
            {L"Тип системы", L"64-разрядная операционная система, процессор x64"},
            {L"Перо и сенсорный ввод", L"Для этого монитора недоступен ввод с помощью пера и сенсорный ввод"},
        };
        AddSpecs(Icon::Monitor, L"Характеристики устройства", device);
        AddInfo(Icon::Info, L"Связанные ссылки", L"Домен или рабочая группа, защита системы, дополнительные параметры системы");
        const std::vector<std::pair<std::wstring, std::wstring>> win = {
            {L"Выпуск", L"Windows 11 Pro"},
            {L"Версия", L"23H2"},
            {L"Дата установки", L"15.03.2026"},
            {L"Сборка ОС", L"22631.4317"},
            {L"Взаимодействие", L"Пакет взаимодействия с компонентами 1000.22700.1041.0"},
        };
        AddSpecs(Icon::Settings, L"Характеристики Windows", win);
    }

    // Раскрытая карточка «ключ: значение» с кнопкой «Копировать».
    void AddSpecs(Icon icon, const std::wstring& title, const std::vector<std::pair<std::wstring, std::wstring>>& rows)
    {
        Card* head = AddCard(icon, title, L"", 56.0f);
        head->expanded = true;
        const float w = Renderer::Get().TextWidth(L"Копировать", 14.0f) + 34.0f;
        auto* copy = head->Add<Button>(L"Копировать");
        copy->SetBounds({head->Bounds().Right() - 50.0f - w, head->Bounds().CenterY() - 16.0f, w, 32.0f});
        std::wstring text = title + L"\r\n\r\n";
        for (auto& rw : rows)
            text += rw.first + L"\t" + rw.second + L"\r\n";
        copy->onClick = [text]() { SetClipboardText(GetShell().Hwnd(), text); };
        m_y -= 4.0f;
        const float h = 20.0f + static_cast<float>(rows.size()) * 30.0f;
        AddCanvas(h, [rows](Renderer& r, const Rect& b, Point) {
            const Palette& p = Theme::P();
            r.FillRoundRect(b, 0.0f, 0.0f, 4.0f, 4.0f, p.cardSecondary);
            r.StrokeRoundRect(b, 4.0f, p.cardStroke);
            float y = b.y + 12.0f;
            const float kw = std::min(220.0f, b.w * 0.3f);
            for (auto& rw : rows)
            {
                r.Text(rw.first, {b.x + 58.0f, y, kw, 22.0f}, p.text, 14.0f);
                r.Text(rw.second, {b.x + 58.0f + kw, y, b.w - 78.0f - kw, 22.0f}, p.textSecondary, 14.0f);
                y += 30.0f;
            }
        });
    }

    // ---- Bluetooth и устройства ------------------------------------------------------
    void BuildBluetooth()
    {
        ShellSettings& st = GetShell().Settings();
        AddCanvas(120.0f, [](Renderer& r, const Rect& b, Point m) {
            const Palette& p = Theme::P();
            const struct
            {
                Icon icon;
                const wchar_t* name;
                const wchar_t* state;
            } devs[] = {
                {Icon::Mouse, L"Беспроводная мышь", L"Подключено"},
                {Icon::Keyboard, L"Клавиатура", L"Подключено"},
                {Icon::Volume3, L"Наушники", L"Сопряжено"},
                {Icon::Add, L"Добавить устройство", L""},
            };
            const float gap = 8.0f;
            const float w = (b.w - gap * 3.0f) / 4.0f;
            for (int i = 0; i < 4; ++i)
            {
                const Rect t{b.x + static_cast<float>(i) * (w + gap), b.y, w, b.h};
                r.FillRoundRect(t, 6.0f, t.Contains(m) ? p.controlHover : p.card);
                r.StrokeRoundRect(t, 6.0f, p.cardStroke);
                r.Glyph(devs[i].icon, {t.x + 18.0f, t.y + 20.0f, 28.0f, 28.0f}, i == 3 ? p.accentText : p.text);
                r.Text(devs[i].name, {t.x + 18.0f, t.y + 62.0f, t.w - 30.0f, 20.0f}, p.text, 14.0f);
                r.Text(devs[i].state, {t.x + 18.0f, t.y + 84.0f, t.w - 30.0f, 18.0f}, p.textSecondary, 12.0f);
            }
        }, [this](Point pt) {
            const Rect b = m_devGrid;
            const float w = (b.w - 24.0f) / 4.0f;
            if (pt.x > b.x + (w + 8.0f) * 3.0f)
                AddDevice();
            else
                Navigate(L"devices");
        });
        m_devGrid = m_page->Children().back()->Bounds();
        static_cast<Canvas*>(m_page->Children().back().get())->cursor = [](Point) { return CursorType::Hand; };
        m_y += 8.0f;
        AddToggle(Icon::Bluetooth, L"Bluetooth", st.bluetooth ? L"Обнаруживается как «" + st.computerName + L"»" : L"Отключено", st.bluetooth,
                  [this](bool v) {
                      GetShell().Settings().bluetooth = v;
                      if (v)
                          GetShell().Settings().airplane = false;
                      Rebuild();
                  });
        AddLink(Icon::Keyboard, L"Устройства", L"Мышь, клавиатура, перо, аудио, дисплеи и док-станции, другие устройства", L"devices");
        AddInfo(Icon::Printer, L"Принтеры и сканеры", L"Параметры, устранение неполадок");
        AddInfo(Icon::Phone, L"Связь с телефоном", L"Мгновенный доступ к фотографиям, текстам и уведомлениям с мобильного устройства");
        AddInfo(Icon::Camera, L"Камеры", L"Подключённые камеры, параметры изображения по умолчанию");
        AddLink(Icon::Mouse, L"Мышь", L"Кнопки, скорость указателя, прокрутка", L"mouse");
        AddInfo(Icon::Pencil, L"Перо и Windows Ink", L"Правая или левая рука, ярлыки для кнопки пера, рукописный ввод");
        AddInfo(Icon::Play, L"Автозапуск", L"Значения по умолчанию для съёмных дисков и карт памяти");
    }

    void AddDevice()
    {
        ShowDialog(L"Добавить устройство", L"Убедитесь, что устройство включено и доступно для обнаружения.\nПоиск устройств…",
                   {{L"Отмена", false}}, nullptr);
    }

    void BuildDevices()
    {
        AddButtonCard(Icon::Bluetooth, L"Добавить устройство", L"", L"Добавить устройство", [this]() { AddDevice(); });
        AddGroup(L"Мышь, клавиатура и перо");
        AddInfo(Icon::Mouse, L"Беспроводная мышь", L"Подключено");
        AddInfo(Icon::Keyboard, L"Клавиатура", L"Подключено");
        AddGroup(L"Звук");
        AddInfo(Icon::Volume3, L"Наушники", L"Сопряжено");
        AddGroup(L"Другие устройства");
        AddInfo(Icon::Monitor, L"Универсальный монитор PnP", L"");
        AddStateToggle(Icon::Downloads, L"Скачивать через лимитные подключения", L"Драйверы, сведения и приложения для новых устройств могут не скачиваться",
                       L"metered", false);
    }

    void BuildMouse()
    {
        AddStateCombo(Icon::Mouse, L"Основная кнопка мыши", L"", {L"Левая", L"Правая"}, L"primarybtn", 0, 160.0f);
        AddSlider(Icon::Mouse, L"Скорость указателя мыши", L"", 0.5f, nullptr);
        AddGroup(L"Прокрутка");
        AddStateCombo(Icon::ArrowDown, L"Прокрутка колёсиком мыши", L"", {L"Сразу на несколько строк", L"По одному экрану"}, L"wheelmode", 0, 240.0f);
        AddSlider(Icon::List, L"Число строк для прокрутки за раз", L"", 0.3f, nullptr);
        AddStateToggle(Icon::Eye, L"Прокручивать неактивные окна при наведении на них указателя", L"", L"inactivescroll", true);
    }

    // ---- Сеть ---------------------------------------------------------------------
    void BuildNetwork()
    {
        ShellSettings& st = GetShell().Settings();
        AddCanvas(96.0f, [](Renderer& r, const Rect& b, Point) {
            const Palette& p = Theme::P();
            const ShellSettings& s = GetShell().Settings();
            HeroBg(r, b);
            const bool on = s.wifi && !s.airplane;
            r.WifiIcon({b.x + 26.0f, b.CenterY() - 18.0f, 36.0f, 36.0f}, on ? p.accentText : p.textSecondary, on ? 4 : 0, !on);
            r.Text(on ? s.network : std::wstring(L"Нет подключения"), {b.x + 84.0f, b.y + 24.0f, b.w * 0.5f, 24.0f}, p.text, 20.0f,
                   FontWeight::Semibold);
            r.Text(on ? L"Подключено, защищено" : (s.airplane ? L"Режим «в самолёте» включён" : L"Wi-Fi отключён"),
                   {b.x + 84.0f, b.y + 52.0f, b.w * 0.5f, 20.0f}, p.textSecondary, 14.0f);
            if (on && b.w > 600.0f)
            {
                r.Text(L"Использование данных", {b.Right() - 240.0f, b.y + 26.0f, 220.0f, 20.0f}, p.text, 14.0f);
                r.Text(L"2,3 ГБ за последние 30 дней", {b.Right() - 240.0f, b.y + 50.0f, 220.0f, 18.0f}, p.textSecondary, 12.0f);
            }
        });
        m_y += 8.0f;
        Card* wifi = AddCard(Icon::Wifi, L"Wi-Fi", L"Подключение, управление известными сетями, лимитная сеть");
        wifi->chevron = true;
        wifi->onClick = [this]() { Navigate(L"wifi"); };
        auto* t = wifi->Add<ToggleSwitch>(st.wifi);
        t->SetBounds({wifi->Bounds().Right() - 44.0f - 116.0f, wifi->Bounds().CenterY() - 16.0f, 116.0f, 32.0f});
        t->onChange = [this](bool v) {
            GetShell().Settings().wifi = v;
            Rebuild();
        };
        wifi->textRight = 116.0f;
        AddInfo(Icon::Ethernet, L"Ethernet", L"Не подключено");
        AddInfo(Icon::Shield, L"VPN", L"Добавление, подключение, управление");
        AddStateToggle(Icon::Cast, L"Мобильный хот-спот", L"Предоставляйте доступ к подключению к Интернету", L"hotspot", false);
        AddToggle(Icon::Airplane, L"Режим «в самолёте»", L"Остановка всей беспроводной связи", st.airplane, [this](bool v) {
            ShellSettings& s = GetShell().Settings();
            s.airplane = v;
            if (v)
            {
                s.wifi = false;
                s.bluetooth = false;
            }
            Rebuild();
        });
        AddInfo(Icon::Globe, L"Прокси-сервер", L"Прокси-сервер для подключений Wi-Fi и Ethernet");
        AddInfo(Icon::Phone, L"Набор номера", L"Настройка коммутируемого подключения к Интернету");
        AddInfo(Icon::Network, L"Дополнительные сетевые параметры", L"Просмотр всех сетевых адаптеров, сброс сети");
    }

    void BuildWifi()
    {
        ShellSettings& st = GetShell().Settings();
        AddToggle(Icon::Wifi, L"Wi-Fi", L"", st.wifi, [this](bool v) {
            GetShell().Settings().wifi = v;
            Rebuild();
        });
        if (st.wifi && !st.airplane)
        {
            Card* c = AddCard(Icon::Wifi, st.network + L" — свойства", L"Подключено, защищено");
            c->chevron = true;
            c->onClick = [this]() {
                ShowDialog(GetShell().Settings().network,
                           L"Протокол: Wi-Fi 6 (802.11ax)\nТип безопасности: WPA3-Personal\nДиапазон: 5 ГГц\nIPv4-адрес: 192.168.1.42", {{L"ОК", true}},
                           nullptr);
            };
        }
        AddInfo(Icon::Settings, L"Управление известными сетями", L"Добавление, удаление и изменение сетей");
        AddStateToggle(Icon::Shield, L"Случайные аппаратные адреса", L"Затрудняет отслеживание вашего расположения", L"randommac", false);
        AddInfo(Icon::Info, L"Свойства оборудования", L"Просмотр и управление свойствами адаптера Wi-Fi");
    }

    // ---- Персонализация -------------------------------------------------------------
    void BuildPersonalization()
    {
        AddCanvas(214.0f,
                  [](Renderer& r, const Rect& b, Point m) {
                      const Palette& p = Theme::P();
                      HeroBg(r, b);
                      DrawDesktopPreview(r, {b.x + 20.0f, b.y + 20.0f, 310.0f, 174.0f});
                      r.Text(L"Выберите тему для применения", {b.x + 356.0f, b.y + 18.0f, b.w - 376.0f, 20.0f}, p.text, 14.0f);
                      for (int i = 0; i < 6; ++i)
                      {
                          const Rect t = PersTile(b, i);
                          if (t.w <= 10.0f)
                              continue;
                          DrawThemeTile(r, t, i, Wallpapers()[static_cast<size_t>(i)].dark, GetShell().Settings().wallpaper == i, t.Contains(m));
                      }
                  },
                  [this](Point pt) {
                      const Rect b = HeroRect();
                      for (int i = 0; i < 6; ++i)
                      {
                          if (PersTile(b, i).Contains(pt))
                          {
                              ApplyTheme(i);
                              return;
                          }
                      }
                  });
        m_hero = static_cast<Canvas*>(m_page->Children().back().get());
        m_y += 8.0f;
        AddLink(Icon::Pictures, L"Фон", L"Фоновое изображение, цвет, слайд-шоу", L"background");
        AddLink(Icon::Palette, L"Цвета", L"Цвет элементов, эффекты прозрачности, цветовой режим", L"colors");
        AddLink(Icon::Brush, L"Темы", L"Установка, создание, управление", L"themes");
        AddLink(Icon::Lock, L"Экран блокировки", L"Изображения, приложения и анимация на экране блокировки", L"lockscreen");
        AddInfo(Icon::Keyboard, L"Сенсорная клавиатура", L"Темы, размер");
        AddLink(Icon::Apps, L"Пуск", L"Последние приложения и элементы, папки", L"start");
        AddLink(Icon::TaskView, L"Панель задач", L"Поведение панели задач, системные закрепления", L"taskbar");
        AddInfo(Icon::TextTool, L"Шрифты", L"Установка, управление");
        AddInfo(Icon::Lightbulb, L"Использование устройства", L"Выберите все способы, которыми вы планируете использовать устройство");
    }

    static Rect PersTile(const Rect& b, int i)
    {
        const float left = b.x + 356.0f;
        const float avail = b.Right() - 20.0f - left;
        const float gap = 10.0f;
        const float w = std::min(132.0f, (avail - gap * 2.0f) / 3.0f);
        return {left + static_cast<float>(i % 3) * (w + gap), b.y + 50.0f + static_cast<float>(i / 3) * (w * 0.5625f + gap), w, w * 0.5625f};
    }

    void BuildBackground()
    {
        AddCanvas(214.0f, [](Renderer& r, const Rect& b, Point) {
            HeroBg(r, b);
            DrawDesktopPreview(r, {b.x + 20.0f, b.y + 20.0f, 310.0f, 174.0f});
        });
        m_y += 8.0f;
        ShellSettings& st = GetShell().Settings();
        const bool solid = st.wallpaper == 5;
        AddCombo(Icon::Pictures, L"Персонализация фона", L"", {L"Изображение", L"Сплошной цвет", L"Слайд-шоу"}, solid ? 1 : 0, [this](int i) {
            if (i == 1)
                GetShell().SetWallpaper(5);
            else if (GetShell().Settings().wallpaper == 5)
                GetShell().SetWallpaper(Theme::Dark() ? 1 : 0);
            Rebuild();
        }, 200.0f);
        m_y -= 4.0f;
        AddCanvas(132.0f,
                  [](Renderer& r, const Rect& b, Point m) {
                      const Palette& p = Theme::P();
                      r.FillRoundRect(b, 0.0f, 0.0f, 4.0f, 4.0f, p.cardSecondary);
                      r.StrokeRoundRect(b, 4.0f, p.cardStroke);
                      r.Text(L"Последние изображения", {b.x + 58.0f, b.y + 12.0f, b.w - 80.0f, 20.0f}, p.text, 14.0f);
                      for (int i = 0; i < 6; ++i)
                      {
                          const Rect t = BgTile(b, i);
                          r.ImageRounded(WallpaperBitmap(i), t, 4.0f);
                          if (GetShell().Settings().wallpaper == i)
                              r.StrokeRoundRect(t.Inflated(2.0f), 6.0f, p.accent, 2.0f);
                          else if (t.Contains(m))
                              r.StrokeRoundRect(t.Inflated(1.0f), 5.0f, p.controlStrongStroke);
                      }
                  },
                  [this](Point pt) {
                      const Rect b = m_bgGrid;
                      for (int i = 0; i < 6; ++i)
                      {
                          if (BgTile(b, i).Contains(pt))
                          {
                              GetShell().SetWallpaper(i);
                              Rebuild();
                              return;
                          }
                      }
                  });
        m_bgGrid = m_page->Children().back()->Bounds();
        AddStateCombo(Icon::Crop, L"Выберите положение для изображения на рабочем столе", L"",
                      {L"Заполнение", L"По размеру", L"Растянуть", L"Замостить", L"По центру", L"Расширение"}, L"fit", 0, 180.0f);
        AddGroup(L"Сопутствующие параметры");
        AddLink(Icon::Palette, L"Контрастные темы", L"Цветовые темы для слабовидящих людей, светочувствительности", L"colors");
    }

    static Rect BgTile(const Rect& b, int i)
    {
        const float gap = 10.0f;
        const float w = std::min(132.0f, (b.w - 58.0f - 20.0f - gap * 5.0f) / 6.0f);
        return {b.x + 58.0f + static_cast<float>(i) * (w + gap), b.y + 44.0f, w, w * 0.5625f};
    }

    void BuildColors()
    {
        AddCombo(Icon::Palette, L"Выберите режим", L"Изменение цветов, отображаемых в Windows и приложениях", {L"Светлый", L"Тёмный"},
                 Theme::Dark() ? 1 : 0, [this](int i) {
                     GetShell().SetDark(i == 1);
                     Rebuild();
                 }, 160.0f);
        AddToggle(Icon::Eye, L"Эффекты прозрачности", L"Окна и поверхности отображаются полупрозрачными", Theme::Transparency(),
                  [](bool v) { GetShell().SetTransparency(v); });
        // Цвет элементов: заголовок + сетка образцов.
        const auto& sw = Theme::AccentSwatches();
        AddStateCombo(Icon::Brush, L"Цвет элементов", L"", {L"Вручную", L"Автоматически"}, L"accentmode", 0, 160.0f);
        m_y -= 4.0f;
        const int rows = static_cast<int>((sw.size() + kSwatchCols - 1) / kSwatchCols);
        const float cell = 44.0f;
        AddCanvas(54.0f + static_cast<float>(rows) * (cell + 6.0f) + 64.0f,
                  [](Renderer& r, const Rect& b, Point m) {
                      const Palette& p = Theme::P();
                      r.FillRoundRect(b, 0.0f, 0.0f, 4.0f, 4.0f, p.cardSecondary);
                      r.StrokeRoundRect(b, 4.0f, p.cardStroke);
                      r.Text(L"Цвета Windows", {b.x + 58.0f, b.y + 14.0f, b.w - 80.0f, 20.0f}, p.text, 14.0f);
                      const auto& list = Theme::AccentSwatches();
                      for (size_t i = 0; i < list.size(); ++i)
                      {
                          const Rect s = SwatchRect(b, static_cast<int>(i), kSwatchCols);
                          const bool sel = Theme::S().accentBase == list[i];
                          r.FillRoundRect(s, 4.0f, list[i]);
                          if (sel)
                          {
                              r.StrokeRoundRect(s.Inflated(3.0f), 6.0f, p.text, 2.0f);
                              r.FillCircle(s.Right() - 9.0f, s.y + 9.0f, 7.0f, Color(255, 255, 255));
                              r.Glyph(Icon::Check, {s.Right() - 14.0f, s.y + 4.0f, 10.0f, 10.0f}, Color(0, 0, 0));
                          }
                          else if (s.Contains(m))
                              r.StrokeRoundRect(s.Inflated(2.0f), 5.0f, p.controlStrongStroke, 1.5f);
                      }
                      const float by = b.Bottom() - 52.0f;
                      r.FillRect({b.x + 1.0f, by - 10.0f, b.w - 2.0f, 1.0f}, p.divider);
                      r.Text(L"Пользовательские цвета", {b.x + 58.0f, by, 300.0f, 32.0f}, p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
                  },
                  [this](Point pt) {
                      const Rect b = m_swatchGrid;
                      const auto& list = Theme::AccentSwatches();
                      for (size_t i = 0; i < list.size(); ++i)
                      {
                          if (SwatchRect(b, static_cast<int>(i), kSwatchCols).Contains(pt))
                          {
                              GetShell().SetAccent(list[i]);
                              m_state[L"accentmode"] = 0;
                              Rebuild();
                              return;
                          }
                      }
                  });
        m_swatchGrid = m_page->Children().back()->Bounds();
        {
            const Rect g = m_swatchGrid;
            auto* view = m_page->Add<Button>(L"Просмотреть цвета");
            const float w = Renderer::Get().TextWidth(view->text, 14.0f) + 34.0f;
            view->SetBounds({g.Right() - 16.0f - w, g.Bottom() - 52.0f, w, 32.0f});
            view->onClick = [this]() {
                // Случайный пользовательский цвет из палитры HSL.
                const float h = RandomFloat(0.0f, 1.0f);
                GetShell().SetAccent(Theme::HslToRgb(h, 0.75f, 0.45f));
                Rebuild();
            };
        }
        AddStateToggle(Icon::Apps, L"Показать цвет элементов в меню «Пуск» и на панели задач", L"", L"accentstart", false, Theme::Dark());
        AddToggle(Icon::Monitor, L"Показать цвет элементов в заголовках окон и рамках окон", L"", Theme::AccentTitleBars(),
                  [](bool v) { GetShell().SetAccentTitleBars(v); });
        AddGroup(L"Сопутствующие параметры");
        AddInfo(Icon::Palette, L"Контрастные темы", L"Цветовые темы для слабовидящих людей, светочувствительности");
    }

    static Rect SwatchRect(const Rect& b, int i, int cols)
    {
        const float cell = 44.0f, gap = 6.0f;
        return {b.x + 58.0f + static_cast<float>(i % cols) * (cell + gap), b.y + 46.0f + static_cast<float>(i / cols) * (cell + gap), cell, cell};
    }

    void BuildThemes()
    {
        AddCanvas(214.0f,
                  [](Renderer& r, const Rect& b, Point) {
                      const Palette& p = Theme::P();
                      HeroBg(r, b);
                      DrawDesktopPreview(r, {b.x + 20.0f, b.y + 20.0f, 310.0f, 174.0f});
                      const ShellSettings& s = GetShell().Settings();
                      r.Text(L"Текущая тема: " + std::wstring(Wallpapers()[static_cast<size_t>(s.wallpaper)].name), {b.x + 356.0f, b.y + 30.0f, b.w - 376.0f, 24.0f}, p.text,
                             20.0f, FontWeight::Semibold);
                      const std::pair<const wchar_t*, std::wstring> rows[] = {
                          {L"Фон", Wallpapers()[static_cast<size_t>(s.wallpaper)].name},
                          {L"Цвет", Theme::Dark() ? L"Тёмный" : L"Светлый"},
                          {L"Звуки", L"По умолчанию (Windows)"},
                          {L"Указатель мыши", L"Windows (по умолчанию)"},
                      };
                      float y = b.y + 70.0f;
                      for (auto& rw : rows)
                      {
                          r.Text(rw.first, {b.x + 356.0f, y, 140.0f, 20.0f}, p.textSecondary, 14.0f);
                          r.Text(rw.second, {b.x + 500.0f, y, b.w - 520.0f, 20.0f}, p.text, 14.0f);
                          y += 28.0f;
                      }
                  });
        AddGroup(L"Текущая тема");
        const float th = 110.0f;
        AddCanvas(th + 44.0f,
                  [](Renderer& r, const Rect& b, Point m) {
                      const Palette& p = Theme::P();
                      HeroBg(r, b);
                      for (int i = 0; i < 6; ++i)
                      {
                          const Rect t = ThemeTileRect(b, i, 6, 20.0f);
                          DrawThemeTile(r, t, i, Wallpapers()[static_cast<size_t>(i)].dark, GetShell().Settings().wallpaper == i, t.Contains(m));
                          r.Text(Wallpapers()[static_cast<size_t>(i)].name, {t.x, t.Bottom() + 6.0f, t.w, 18.0f}, p.text, 12.0f);
                      }
                  },
                  [this](Point pt) {
                      for (int i = 0; i < 6; ++i)
                      {
                          if (ThemeTileRect(m_themeGrid, i, 6, 20.0f).Contains(pt))
                          {
                              ApplyTheme(i);
                              return;
                          }
                      }
                  });
        m_themeGrid = m_page->Children().back()->Bounds();
        AddInfo(Icon::Globe, L"Получить больше тем в Магазине", L"Бесплатные темы с фоновыми изображениями, звуками и цветами");
    }

    void BuildLockScreen()
    {
        AddCanvas(214.0f, [](Renderer& r, const Rect& b, Point) {
            const Palette& p = Theme::P();
            HeroBg(r, b);
            const Rect pv{b.x + 20.0f, b.y + 20.0f, 310.0f, 174.0f};
            r.ImageRounded(WallpaperBitmap(GetShell().Settings().wallpaper), pv, 8.0f);
            const SYSTEMTIME now = LocalNow();
            r.Text(FormatTime(now), {pv.x, pv.y + 36.0f, pv.w, 50.0f}, Color(255, 255, 255), 40.0f, FontWeight::Semibold, TextFlags::Center);
            r.Text(FormatLongDate(now), {pv.x, pv.y + 90.0f, pv.w, 20.0f}, Color(255, 255, 255), 12.0f, FontWeight::Regular, TextFlags::Center);
            r.Text(L"Экран блокировки отображается при блокировке компьютера (Win+L).", {b.x + 356.0f, b.y + 30.0f, b.w - 376.0f, 60.0f}, p.textSecondary,
                   14.0f, FontWeight::Regular, TextFlags::Wrap);
        });
        m_y += 8.0f;
        AddStateCombo(Icon::Pictures, L"Персонализируйте экран блокировки", L"", {L"Изображение", L"Слайд-шоу", L"Windows: интересное"}, L"lockbg", 0, 220.0f);
        AddStateCombo(Icon::Apps, L"Состояние экрана блокировки", L"Выберите приложение для отображения подробного состояния",
                      {L"Нет", L"Погода и другое", L"Календарь", L"Почта"}, L"lockstatus", 1, 200.0f);
        AddStateToggle(Icon::Pictures, L"Показывать фоновый рисунок экрана блокировки на экране входа", L"", L"lockonsignin", true);
        AddButtonCard(Icon::Lock, L"Заблокировать сейчас", L"Win + L", L"Заблокировать", []() { GetShell().Lock(); });
    }

    void BuildStart()
    {
        ShellSettings& st = GetShell().Settings();
        AddStateCombo(Icon::Apps, L"Макет", L"", {L"Больше закреплённых", L"По умолчанию", L"Больше рекомендаций"}, L"startlayout", 1, 220.0f);
        AddToggle(Icon::Clock, L"Показывать недавно добавленные приложения", L"", st.startRecentApps,
                  [](bool v) { GetShell().Settings().startRecentApps = v; });
        AddStateToggle(Icon::Star, L"Показывать наиболее часто используемые приложения", L"", L"startfrequent", true);
        AddToggle(Icon::Documents, L"Показывать рекомендуемые файлы в меню «Пуск», последние файлы в проводнике и элементы в списках переходов", L"",
                  st.startRecommended, [](bool v) { GetShell().Settings().startRecommended = v; });
        AddStateToggle(Icon::Lightbulb, L"Показывать рекомендации с советами, ярлыками, новыми приложениями и т. д.", L"", L"starttips", true);
        AddStateToggle(Icon::User, L"Показывать уведомления, связанные с учётной записью", L"", L"startaccount", true);
        AddInfo(Icon::Folder, L"Папки", L"Выберите папки, которые будут отображаться в меню «Пуск» рядом с кнопкой питания");
    }

    void BuildTaskbar()
    {
        ShellSettings& st = GetShell().Settings();
        AddGroup(L"Элементы панели задач");
        AddCombo(Icon::Search, L"Поиск", L"", {L"Скрыть", L"Только значок поиска", L"Поле поиска"}, Clamp(st.searchMode, 0, 2), [](int i) {
            GetShell().Settings().searchMode = i;
            GetShell().TB().Layout();
        }, 220.0f);
        AddToggle(Icon::TaskView, L"Представление задач", L"", st.showTaskView, [](bool v) {
            GetShell().Settings().showTaskView = v;
            GetShell().TB().Layout();
        });
        AddToggle(Icon::Widgets, L"Мини-приложения", L"", st.showWidgets, [](bool v) {
            GetShell().Settings().showWidgets = v;
            GetShell().TB().Layout();
        });
        AddGroup(L"Значки в углу панели задач");
        AddStateToggle(Icon::Pencil, L"Меню пера", L"Показывать значок меню пера, когда используется перо", L"penmenu", false);
        AddStateToggle(Icon::Keyboard, L"Сенсорная клавиатура", L"Показывать значок сенсорной клавиатуры", L"touchkbd", false);
        AddStateToggle(Icon::Mouse, L"Виртуальная сенсорная панель", L"Всегда показывать значок виртуальной сенсорной панели", L"vtouchpad", false);
        AddInfo(Icon::More, L"Другие значки панели задач", L"Отображение или скрытие дополнительных значков");
        AddGroup(L"Поведение панели задач");
        AddCombo(Icon::TaskView, L"Выравнивание панели задач", L"", {L"По центру", L"Слева"}, st.taskbarCenter ? 0 : 1, [](int i) {
            GetShell().Settings().taskbarCenter = i == 0;
            GetShell().TB().Layout();
        }, 160.0f);
        AddStateToggle(Icon::Minimize, L"Автоматически скрывать панель задач", L"", L"autohide", false);
        AddStateToggle(Icon::Bell, L"Показывать индикаторы событий в приложениях панели задач", L"", L"badges", true);
        AddStateToggle(Icon::Desktop, L"Чтобы показать рабочий стол, выберите дальний угол панели задач", L"", L"peek", true);
    }

    // ---- Приложения ------------------------------------------------------------------
    void BuildApps()
    {
        AddLink(Icon::Apps, L"Установленные приложения", L"Удаление, свойства", L"installed");
        AddInfo(Icon::Settings, L"Дополнительные параметры приложений", L"Выбор места получения приложений, общий доступ между устройствами");
        AddInfo(Icon::Check, L"Приложения по умолчанию", L"Значения по умолчанию для файлов и типов ссылок, другие значения по умолчанию");
        AddInfo(Icon::Location, L"Автономные карты", L"Загрузки, место хранения, обновления карт");
        AddInfo(Icon::Add, L"Дополнительные компоненты", L"Дополнительные функции для устройства");
        AddInfo(Icon::Globe, L"Приложения для веб-сайтов", L"Веб-сайты, которые можно открывать в приложениях");
        AddInfo(Icon::Play, L"Воспроизведение видео", L"Настройка видео, потоковая передача HDR, параметры батареи");
        AddLink(Icon::Startup, L"Автозагрузка", L"Приложения, запускаемые автоматически при входе в систему", L"startup");
    }

    void BuildInstalled()
    {
        std::vector<AppId> apps = AllApps();
        std::sort(apps.begin(), apps.end(), [](AppId a, AppId b) { return CompareNoCase(GetAppInfo(a).name, GetAppInfo(b).name) < 0; });
        AddCanvas(26.0f, [n = apps.size()](Renderer& r, const Rect& b, Point) {
            r.Text(L"Найдено приложений: " + std::to_wstring(n), b, Theme::P().textSecondary, 14.0f, FontWeight::Regular, TextFlags::VCenter);
        });
        int idx = 0;
        for (AppId a : apps)
        {
            const AppInfo& ai = GetAppInfo(a);
            const int sizeKb = 2400 + (static_cast<int>(a) * 7919) % 90000;
            Card* c = AddCard(Icon::None, ai.name, std::wstring(L"Демонстрация  |  ") + FormatDate(AddDays(LocalNow(), -(idx * 9 + 3))));
            c->appIcon = ai.icon;
            c->value = FormatSize(static_cast<unsigned long long>(sizeKb) * 1024ULL);
            auto* more = c->Add<Button>(L"", Icon::More, ButtonStyle::Subtle);
            more->SetBounds(RightSlot(c, 36.0f));
            more->SetTooltip(L"Дополнительные параметры");
            const bool system = std::wstring(ai.kind).find(L"Системное") != std::wstring::npos;
            more->onClick = [this, more, a, system]() {
                std::vector<MenuItem> items = {
                    MenuItem(L"Дополнительные параметры", Icon::Settings, [this, a]() {
                        const AppInfo& i2 = GetAppInfo(a);
                        ShowDialog(i2.name, std::wstring(i2.kind) + L"\nИздатель: Демонстрация\nВерсия: 11.2409.24.0", {{L"ОК", true}}, nullptr);
                    }),
                    MenuItem(L"Изменить", Icon::Edit, nullptr).Disabled(),
                    MenuItem(L"Удалить", Icon::Delete, [this, a]() {
                        const AppInfo& i2 = GetAppInfo(a);
                        ShowDialog(L"Удаление", L"Приложение «" + std::wstring(i2.name) + L"» является частью демонстрации и не может быть удалено.",
                                   {{L"ОК", true}}, nullptr);
                    }).Disabled(system),
                };
                GetShell().ShowMenuBelow(PageToScreen(more->Bounds()), std::move(items), 220.0f);
            };
            c->textRight = 44.0f;
            ++idx;
        }
    }

    void BuildStartup()
    {
        AddCanvas(40.0f, [](Renderer& r, const Rect& b, Point) {
            r.Text(L"Приложения можно настроить на автоматический запуск при входе в систему.", b, Theme::P().textSecondary, 14.0f,
                   FontWeight::Regular, TextFlags::VCenter);
        });
        const AppId apps[] = {AppId::Terminal, AppId::Weather, AppId::Mail, AppId::Clock};
        int i = 0;
        for (AppId a : apps)
        {
            const AppInfo& ai = GetAppInfo(a);
            const std::wstring id = std::wstring(L"startup_") + ai.name;
            AddStateToggle(Icon::None, ai.name, i % 2 ? L"Влияние на запуск: низкое" : L"Влияние на запуск: не измерено", id, i == 1);
            static_cast<Card*>(m_page->Children().back().get())->appIcon = ai.icon;
            ++i;
        }
    }

    // ---- Учётные записи ---------------------------------------------------------------
    void BuildAccounts()
    {
        AddCanvas(132.0f, [](Renderer& r, const Rect& b, Point) {
            const Palette& p = Theme::P();
            HeroBg(r, b);
            DrawAvatar(r, b.x + 70.0f, b.CenterY(), 46.0f);
            r.Text(GetShell().FS().UserName(), {b.x + 136.0f, b.y + 30.0f, b.w * 0.6f, 28.0f}, p.text, 20.0f, FontWeight::Semibold);
            r.Text(L"Локальная учётная запись", {b.x + 136.0f, b.y + 60.0f, b.w * 0.6f, 20.0f}, p.textSecondary, 14.0f);
            r.Text(L"Администратор", {b.x + 136.0f, b.y + 82.0f, b.w * 0.6f, 20.0f}, p.textSecondary, 14.0f);
        });
        AddGroup(L"Параметры учётной записи");
        AddLink(Icon::User, L"Ваши данные", L"Изображение профиля", L"yourinfo");
        AddInfo(Icon::Mail, L"Учётные записи электронной почты и приложений", L"Учётные записи, используемые электронной почтой, календарём и контактами");
        AddLink(Icon::Lock, L"Варианты входа", L"Распознавание лиц, отпечатков пальцев, ПИН-код, ключ безопасности", L"signin");
        AddInfo(Icon::Shield, L"Доступ к учётной записи места работы или учебного заведения", L"Ресурсы организации, такие как электронная почта, приложения и сеть");
        AddInfo(Icon::Apps, L"Другие пользователи", L"Доступ к устройству, рабочие или учебные пользователи, назначенный доступ в режиме киоска");
        AddInfo(Icon::Update, L"Архивация данных Windows", L"Резервное копирование файлов, приложений, настроек для восстановления на разных устройствах");
    }

    void BuildYourInfo()
    {
        AddCanvas(220.0f, [](Renderer& r, const Rect& b, Point) {
            const Palette& p = Theme::P();
            DrawAvatar(r, b.CenterX(), b.y + 84.0f, 72.0f);
            r.Text(GetShell().FS().UserName(), {b.x, b.y + 164.0f, b.w, 28.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::Center);
            r.Text(L"Локальная учётная запись · Администратор", {b.x, b.y + 194.0f, b.w, 20.0f}, p.textSecondary, 14.0f, FontWeight::Regular,
                   TextFlags::Center);
        });
        AddGroup(L"Настройка фотографии");
        AddButtonCard(Icon::Camera, L"Сделать фото", L"", L"Открыть камеру", []() { GetShell().Launch(AppId::Camera); });
        AddButtonCard(Icon::Pictures, L"Выбрать файл", L"", L"Обзор файлов", []() { GetShell().Launch(AppId::Explorer, VirtualFS::NodeArg(GetShell().FS().Pictures())); });
    }

    void BuildSignIn()
    {
        AddGroup(L"Способы входа");
        AddInfo(Icon::Eye, L"Распознавание лиц (Windows Hello)", L"Этот вариант сейчас недоступен");
        AddInfo(Icon::User, L"Распознавание отпечатков пальцев (Windows Hello)", L"Этот вариант сейчас недоступен");
        AddInfo(Icon::Keyboard, L"ПИН-код (Windows Hello)", L"Вход с помощью ПИН-кода (рекомендуется)");
        AddInfo(Icon::Shield, L"Ключ безопасности", L"Вход с помощью физического ключа безопасности");
        AddInfo(Icon::Lock, L"Пароль", L"Вход с помощью пароля учётной записи");
        AddGroup(L"Дополнительные параметры");
        AddStateCombo(Icon::Clock, L"Если вы отсутствовали, когда Windows должна потребовать выполнить повторный вход?", L"",
                      {L"Никогда", L"При выходе компьютера из спящего режима"}, L"relogin", 1, 290.0f);
        AddStateToggle(Icon::Restart, L"Автоматически сохранять перезапускаемые приложения и перезапускать их при повторном входе", L"", L"restartapps", false);
    }

    // ---- Время и язык --------------------------------------------------------------------
    void BuildTime()
    {
        AddCanvas(124.0f, [](Renderer& r, const Rect& b, Point) {
            const Palette& p = Theme::P();
            HeroBg(r, b);
            const SYSTEMTIME now = LocalNow();
            r.Text(FormatTime(now), {b.x + 28.0f, b.y + 20.0f, 300.0f, 52.0f}, p.text, 40.0f, FontWeight::Semibold);
            std::wstring date = FormatLongDate(now) + L" " + std::to_wstring(now.wYear) + L" г.";
            r.Text(date, {b.x + 28.0f, b.y + 76.0f, b.w * 0.5f, 20.0f}, p.textSecondary, 14.0f);
            if (b.w > 600.0f)
            {
                r.Text(L"Язык и регион", {b.Right() - 300.0f, b.y + 34.0f, 280.0f, 20.0f}, p.text, 14.0f, FontWeight::Semibold);
                r.Text(L"Русский (Россия)", {b.Right() - 300.0f, b.y + 58.0f, 280.0f, 18.0f}, p.textSecondary, 12.0f);
            }
        });
        m_y += 8.0f;
        AddLink(Icon::Clock, L"Дата и время", L"Часовые пояса, автоматическая настройка часов, отображение календаря", L"datetime");
        AddLink(Icon::Language, L"Язык и регион", L"Язык интерфейса Windows и некоторых приложений, региональный формат", L"language");
        AddInfo(Icon::Keyboard, L"Ввод", L"Сенсорная клавиатура, предложения текста, параметры");
        AddInfo(Icon::Sound, L"Речь", L"Язык речи, голосовой доступ, голоса для преобразования текста в речь");
    }

    void BuildDateTime()
    {
        AddCanvas(96.0f, [](Renderer& r, const Rect& b, Point) {
            const Palette& p = Theme::P();
            HeroBg(r, b);
            const SYSTEMTIME now = LocalNow();
            r.Text(FormatTime(now) + L":" + Pad2(now.wSecond), {b.x + 28.0f, b.y + 18.0f, 300.0f, 40.0f}, p.text, 32.0f, FontWeight::Semibold);
            r.Text(FormatLongDate(now) + L" " + std::to_wstring(now.wYear) + L" г.", {b.x + 28.0f, b.y + 60.0f, b.w * 0.6f, 20.0f}, p.textSecondary,
                   14.0f);
        });
        AddStateToggle(Icon::Clock, L"Установить время автоматически", L"", L"autotime", true);
        AddStateToggle(Icon::Globe, L"Автоматически устанавливать часовой пояс", L"", L"autotz", false);
        AddCombo(Icon::Globe, L"Часовой пояс", L"", {TimeZoneText()}, 0, nullptr, 320.0f);
        AddStateToggle(Icon::Sun, L"Автоматически переходить на летнее время и обратно", L"", L"dst", true, false);
        AddGroup(L"Дополнительные параметры");
        AddButtonCard(Icon::Refresh, L"Синхронизировать сейчас",
                      m_syncTime.empty() ? L"Последняя успешная синхронизация времени: сегодня" : L"Последняя успешная синхронизация времени: " + m_syncTime,
                      L"Синхронизировать", [this]() {
                          m_syncTime = L"сегодня, " + FormatTime(LocalNow());
                          Rebuild();
                      });
        AddStateCombo(Icon::Calendar, L"Показывать дополнительные календари на панели задач", L"", {L"Не показывать дополнительные календари", L"Упрощённый китайский (лунный)"},
                      L"altcal", 0, 300.0f);
    }

    void BuildLanguage()
    {
        AddGroup(L"Язык");
        AddCombo(Icon::Language, L"Язык интерфейса Windows", L"Компоненты Windows, такие как «Параметры» и проводник, будут отображаться на этом языке",
                 {L"Русский"}, 0, nullptr, 200.0f);
        AddInfo(Icon::Keyboard, L"Предпочитаемые языки", L"Русский (Россия), английский (США)");
        AddGroup(L"Регион");
        AddCombo(Icon::Globe, L"Страна или регион", L"Windows и приложения могут использовать вашу страну или регион", {L"Россия"}, 0, nullptr, 200.0f);
        AddCombo(Icon::Calendar, L"Региональный формат", L"Windows и некоторые приложения форматируют даты и время на основе регионального формата",
                 {L"Русский (Россия)"}, 0, nullptr, 200.0f);
    }

    // ---- Игры --------------------------------------------------------------------------
    void BuildGaming()
    {
        AddStateToggle(Icon::Game, L"Игровая панель", L"Открывайте игровую панель с помощью этой кнопки на геймпаде", L"gamebar", true);
        AddInfo(Icon::Camera, L"Записи", L"Место сохранения, качество записи");
        AddStateToggle(Icon::Performance, L"Игровой режим", L"Оптимизируйте компьютер для игр, отключая фоновые процессы", L"gamemode", true);
    }

    // ---- Специальные возможности ----------------------------------------------------------
    void BuildAccessibility()
    {
        AddGroup(L"Зрение");
        AddLink(Icon::TextTool, L"Размер текста", L"Размер текста во всей Windows и приложениях", L"textsize");
        AddLink(Icon::Eye, L"Визуальные эффекты", L"Полосы прокрутки, прозрачность, анимация, время ожидания уведомлений", L"visualeffects");
        AddInfo(Icon::Mouse, L"Указатель мыши и сенсорный ввод", L"Цвет и размер указателя мыши");
        AddInfo(Icon::TextTool, L"Курсор ввода текста", L"Внешний вид и толщина курсора ввода текста");
        AddInfo(Icon::ZoomIn, L"Экранная лупа", L"Увеличение, увеличение при чтении");
        AddInfo(Icon::Palette, L"Цветовые фильтры", L"Фильтры дальтонизма, оттенки серого, инверсия");
        AddInfo(Icon::Brightness, L"Контрастные темы", L"Цветовые темы для слабовидящих людей");
        AddInfo(Icon::Sound, L"Экранный диктор", L"Голос, детализация, клавиатура, шрифт Брайля");
        AddGroup(L"Слух");
        AddInfo(Icon::Volume3, L"Звук", L"Монофонический звук, уведомления о звуках");
        AddInfo(Icon::TextTool, L"Субтитры", L"Стили");
        AddGroup(L"Взаимодействие");
        AddInfo(Icon::Sound, L"Речь", L"Голосовой доступ, голосовой ввод");
        AddInfo(Icon::Keyboard, L"Клавиатура", L"Залипание, фильтрация и переключение клавиш, экранная клавиатура");
        AddInfo(Icon::Mouse, L"Мышь", L"Клавиши мыши, сенсорный ввод");
    }

    void BuildVisualEffects()
    {
        AddStateToggle(Icon::List, L"Всегда показывать полосы прокрутки", L"", L"scrollbars", false);
        AddToggle(Icon::Eye, L"Эффекты прозрачности", L"", Theme::Transparency(), [](bool v) { GetShell().SetTransparency(v); });
        AddToggle(Icon::Play, L"Эффекты анимации", L"", AnimationsEnabled(), [](bool v) { GetShell().SetAnimations(v); });
        AddStateCombo(Icon::Bell, L"Закрывать уведомления через это время", L"", {L"5 секунд", L"7 секунд", L"15 секунд", L"30 секунд", L"1 минута", L"5 минут"},
                      L"notiftimeout", 0, 160.0f);
    }

    void BuildTextSize()
    {
        AddCanvas(120.0f, [](Renderer& r, const Rect& b, Point) {
            const Palette& p = Theme::P();
            HeroBg(r, b);
            r.Text(L"Предварительный просмотр размера текста", {b.x + 24.0f, b.y + 20.0f, b.w - 48.0f, 20.0f}, p.textSecondary, 12.0f);
            r.Text(L"Перетащите ползунок, пока этот текст не станет удобным для чтения", {b.x + 24.0f, b.y + 48.0f, b.w - 48.0f, 50.0f}, p.text, 18.0f,
                   FontWeight::Regular, TextFlags::Wrap);
        });
        AddSlider(Icon::TextTool, L"Размер текста", L"100%", 0.0f, nullptr);
    }

    // ---- Конфиденциальность ------------------------------------------------------------------
    void BuildPrivacy()
    {
        AddGroup(L"Безопасность");
        AddInfo(Icon::Shield, L"Безопасность Windows", L"Антивирусная программа, брандмауэр и защита сети для вашего устройства");
        AddInfo(Icon::Location, L"Поиск устройства", L"Отслеживание устройства, если вы его потеряли");
        AddInfo(Icon::Code, L"Для разработчиков", L"Эти параметры предназначены только для использования в целях разработки");
        AddGroup(L"Разрешения Windows");
        AddInfo(Icon::Lock, L"Общие", L"Реклама, локальное содержимое, запуск приложений, предложения");
        AddInfo(Icon::Sound, L"Речь", L"Распознавание речи в Интернете для диктовки и других голосовых взаимодействий");
        AddInfo(Icon::Keyboard, L"Персонализация рукописного ввода и ввода с клавиатуры", L"Персональный словарь, слова из словаря");
        AddInfo(Icon::Chart, L"Диагностика и отзывы", L"Диагностические данные, рукописные данные и ввод с клавиатуры, рекомендации");
        AddInfo(Icon::History, L"Журнал действий", L"Параметры журнала действий для этого устройства");
        AddInfo(Icon::Search, L"Разрешения для поиска", L"Безопасный поиск, облачный поиск, журнал поиска");
        AddGroup(L"Разрешения приложений");
        AddStateToggle(Icon::Location, L"Расположение", L"Службы определения местоположения", L"location", true);
        AddStateToggle(Icon::Camera, L"Камера", L"Доступ к камере", L"camera", true);
        AddStateToggle(Icon::Sound, L"Микрофон", L"Доступ к микрофону", L"mic", true);
        AddStateToggle(Icon::Bell, L"Уведомления", L"Доступ к уведомлениям", L"notifaccess", true);
    }

    // ---- Центр обновления Windows ----------------------------------------------------------------
    struct UpdateItem
    {
        const wchar_t* name;
        float weight;
    };
    static const std::vector<UpdateItem>& Updates()
    {
        static const std::vector<UpdateItem> v = {
            {L"Накопительное обновление 2026-10 для Windows 11 Version 23H2 для систем на базе x64 (KB5099412)", 0.6f},
            {L"Обновление аналитики безопасности антивирусной программы — KB2267602 (версия 1.419.88.0)", 0.15f},
            {L"Средство удаления вредоносных программ x64 — v5.130 (KB890830)", 0.25f},
        };
        return v;
    }

    std::wstring UpdateStatusShort() const
    {
        switch (m_updState)
        {
        case 1:
            return L"Проверка наличия обновлений…";
        case 2:
            return L"Доступны обновления";
        case 3:
        case 4:
            return L"Устанавливаются обновления";
        case 5:
            return L"Требуется перезагрузка";
        default:
            return L"Последняя проверка: " + m_lastCheck;
        }
    }

    // Состояние: 0 — нет обновлений, 1 — проверка, 2 — доступны, 3 — загрузка, 4 — установка, 5 — нужна перезагрузка.
    void TickUpdate()
    {
        const double t = NowSeconds() - m_updStart;
        int next = m_updState;
        if (m_updState == 1 && t > 3.0)
            next = 2;
        else if (m_updState == 3 && t > 7.0)
            next = 4;
        else if (m_updState == 4 && t > 12.0)
            next = 5;
        if (next != m_updState)
        {
            m_updState = next;
            if (next == 2)
                m_lastCheck = L"сегодня, " + FormatTime(LocalNow());
            if (next == 5)
                PostAction([]() {
                    GetShell().Notify(AppIcon::Settings, L"Центр обновления Windows", L"Требуется перезагрузка",
                                      L"Перезагрузите устройство, чтобы завершить установку обновлений.");
                });
            if (m_current == L"update")
                Rebuild();
        }
        if (m_updState == 1 || m_updState == 3 || m_updState == 4)
            RequestFrame();
    }

    void BuildUpdate()
    {
        AddCanvas(124.0f, [this](Renderer& r, const Rect& b, Point) {
            const Palette& p = Theme::P();
            HeroBg(r, b);
            const float cx = b.x + 64.0f, cy = b.CenterY();
            if (m_updState == 1)
                DrawSpinner(r, cx, cy, 26.0f, 3.0f, p.accent);
            else
            {
                r.FillCircle(cx, cy, 30.0f, m_updState == 5 ? Color::Hex(0xE6A23C) : p.accent);
                r.Glyph(m_updState == 5 ? Icon::Restart : (m_updState == 0 ? Icon::Check : Icon::Update), {cx - 14.0f, cy - 14.0f, 28.0f, 28.0f},
                        p.textOnAccent, 1.4f);
            }
            std::wstring title, sub;
            switch (m_updState)
            {
            case 1:
                title = L"Проверка наличия обновлений…";
                sub = L"Это может занять несколько минут";
                break;
            case 2:
                title = L"Доступны обновления";
                sub = L"Последняя проверка: " + m_lastCheck;
                break;
            case 3:
            case 4:
                title = L"Устанавливаются обновления";
                sub = L"Вы можете продолжать работу во время установки";
                break;
            case 5:
                title = L"Требуется перезагрузка";
                sub = L"Перезагрузите устройство, чтобы установить обновления";
                break;
            default:
                title = L"У вас установлены все последние обновления";
                sub = L"Последняя проверка: " + m_lastCheck;
                break;
            }
            r.Text(title, {b.x + 118.0f, b.y + 38.0f, b.w - 380.0f, 26.0f}, p.text, 20.0f, FontWeight::Semibold);
            r.Text(sub, {b.x + 118.0f, b.y + 68.0f, b.w - 380.0f, 20.0f}, p.textSecondary, 14.0f);
        });
        {
            const Rect hb = m_page->Children().back()->Bounds();
            std::wstring text = L"Проверить наличие обновлений";
            std::function<void()> fn = [this]() {
                m_updState = 1;
                m_updStart = NowSeconds();
                Rebuild();
            };
            if (m_updState == 2)
            {
                text = L"Скачать и установить все";
                fn = [this]() {
                    m_updState = 3;
                    m_updStart = NowSeconds();
                    Rebuild();
                };
            }
            else if (m_updState == 5)
            {
                text = L"Перезагрузить сейчас";
                fn = [this]() {
                    m_updState = 0;
                    GetShell().Shutdown(true);
                };
            }
            if (m_updState != 1 && m_updState != 3 && m_updState != 4)
            {
                auto* b = m_page->Add<Button>(text, Icon::None, ButtonStyle::Accent);
                const float w = Renderer::Get().TextWidth(text, 14.0f) + 36.0f;
                b->SetBounds({hb.Right() - 24.0f - w, hb.CenterY() - 16.0f, w, 32.0f});
                b->onClick = fn;
            }
        }
        if (m_updState >= 2 && m_updState <= 5)
        {
            m_y += 4.0f;
            for (size_t i = 0; i < Updates().size(); ++i)
            {
                AddCanvas(64.0f, [this, i](Renderer& r, const Rect& b, Point) {
                    const Palette& p = Theme::P();
                    r.FillRoundRect(b, 4.0f, p.card);
                    r.StrokeRoundRect(b, 4.0f, p.cardStroke);
                    r.Glyph(Icon::Update, {b.x + 20.0f, b.CenterY() - 10.0f, 20.0f, 20.0f}, p.text);
                    r.Text(Updates()[i].name, {b.x + 58.0f, b.y + 12.0f, b.w - 300.0f, 20.0f}, p.text, 14.0f);
                    const double t = NowSeconds() - m_updStart;
                    std::wstring status = L"Ожидание скачивания";
                    float progress = -1.0f;
                    if (m_updState == 3)
                    {
                        const float pr = Saturate(static_cast<float>(t / 7.0) * (1.4f - 0.2f * static_cast<float>(i)));
                        progress = pr;
                        status = pr >= 1.0f ? L"Ожидание установки" : L"Скачивание — " + std::to_wstring(RoundToInt(pr * 100.0f)) + L"%";
                    }
                    else if (m_updState == 4)
                    {
                        const float pr = Saturate(static_cast<float>((t - 7.0) / 5.0) * (1.3f - 0.15f * static_cast<float>(i)));
                        progress = pr;
                        status = pr >= 1.0f ? L"Ожидание перезагрузки" : L"Установка — " + std::to_wstring(RoundToInt(pr * 100.0f)) + L"%";
                    }
                    else if (m_updState == 5)
                        status = L"Ожидание перезагрузки";
                    r.Text(status, {b.x + 58.0f, b.y + 34.0f, 300.0f, 18.0f}, p.textSecondary, 12.0f);
                    if (progress >= 0.0f && progress < 1.0f)
                        DrawProgressBar(r, {b.Right() - 236.0f, b.CenterY() - 4.0f, 200.0f, 8.0f}, progress, p.accent);
                });
            }
        }
        AddGroup(L"Дополнительные параметры");
        AddStateToggle(Icon::Update, L"Получайте последние обновления, как только они станут доступны",
                       L"Будьте одним из первых, кто получит последние обновления, исправления и улучшения", L"earlyupd", false);
        AddStateCombo(Icon::Pause, L"Приостановка обновлений", L"", {L"Приостановить на 1 неделю", L"Приостановить на 2 недели", L"Приостановить на 3 недели",
                                                                    L"Приостановить на 4 недели", L"Приостановить на 5 недель"},
                      L"pause", 0, 260.0f);
        AddLink(Icon::History, L"Журнал обновлений", L"", L"history");
        AddInfo(Icon::Settings, L"Дополнительные параметры", L"Оптимизация доставки, необязательные обновления, часы активности, другие параметры");
        AddInfo(Icon::Heart, L"Программа предварительной оценки Windows", L"Получайте предварительные сборки Windows, чтобы делиться отзывами");
    }

    void BuildHistory()
    {
        AddGroup(L"Обновления качества");
        const SYSTEMTIME now = LocalNow();
        const wchar_t* names[] = {L"Накопительное обновление 2026-09 для Windows 11 Version 23H2 (KB5098211)",
                                  L"Накопительное обновление 2026-08 для Windows 11 Version 23H2 (KB5096023)",
                                  L"Накопительное обновление .NET Framework 3.5 и 4.8.1 (KB5095577)"};
        for (int i = 0; i < 3; ++i)
            AddCard(Icon::Update, names[i], L"Успешно установлено " + FormatDate(AddDays(now, -(i + 1) * 30)));
        AddGroup(L"Обновления определений");
        AddCard(Icon::Shield, L"Обновление аналитики безопасности (версия 1.419.71.0)", L"Успешно установлено " + FormatDate(AddDays(now, -1)));
        AddGroup(L"Сопутствующие параметры");
        AddInfo(Icon::Delete, L"Удалить обновления", L"");
        AddInfo(Icon::Restart, L"Параметры восстановления", L"");
    }

    // ---- Поиск ------------------------------------------------------------------------------------
    void BuildSearch()
    {
        const std::vector<SearchEntry> res = SearchResults(m_query);
        if (res.empty())
        {
            AddCanvas(80.0f, [q = m_query](Renderer& r, const Rect& b, Point) {
                r.Text(L"Нет результатов для «" + q + L"»", {b.x, b.y + 10.0f, b.w, 24.0f}, Theme::P().text, 14.0f);
                r.Text(L"Попробуйте другие ключевые слова.", {b.x, b.y + 36.0f, b.w, 20.0f}, Theme::P().textSecondary, 12.0f);
            });
            return;
        }
        for (const SearchEntry& e : res)
        {
            const PageInfo* pi = FindPage(e.page);
            std::wstring path;
            if (pi)
            {
                path = pi->title;
                if (pi->parent)
                {
                    if (const PageInfo* par = FindPage(pi->parent))
                        path = std::wstring(par->title) + L" › " + path;
                }
            }
            Card* c = AddCard(pi ? pi->icon : Icon::Settings, e.text, path);
            c->chevron = true;
            const std::wstring page = e.page;
            c->onClick = [this, page]() { OpenSearchResult(page); };
        }
    }

    // ================================================================== отрисовка
    void DrawContent(Renderer& r, const Rect& /*client*/) override
    {
        const Palette& p = Theme::P();
        TickUpdate();
        // Заголовок окна
        r.Text(L"Параметры", {54.0f, 0.0f, 300.0f, kTitleH}, IsActive() ? p.text : p.textTertiary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        // Карточка пользователя
        const Rect uc = UserCardRect();
        if (m_userHover)
            r.FillRoundRect(uc, 6.0f, p.subtleHover);
        DrawAvatar(r, uc.x + 40.0f, uc.CenterY(), 32.0f);
        r.Text(GetShell().FS().UserName(), {uc.x + 84.0f, uc.CenterY() - 20.0f, uc.w - 90.0f, 22.0f}, p.text, 14.0f, FontWeight::Semibold);
        r.Text(L"Локальная учётная запись", {uc.x + 84.0f, uc.CenterY() + 2.0f, uc.w - 90.0f, 18.0f}, p.textSecondary, 12.0f);
        // Заголовок страницы с «хлебными крошками»
        DrawHeader(r);
        m_ui.Draw(r);
    }

    Rect UserCardRect() const { return {8.0f, kTitleH + 6.0f, NavW() - 16.0f, 90.0f}; }

    void DrawHeader(Renderer& r)
    {
        const Palette& p = Theme::P();
        const PageInfo* pi = FindPage(m_current);
        const float y = kTitleH + 6.0f, h = 52.0f;
        m_crumbRect = {};
        std::wstring title = pi ? pi->title : L"";
        if (m_current == L"search")
            title = L"Результаты поиска «" + m_query + L"»";
        float x = CX();
        if (pi && pi->parent)
        {
            const PageInfo* par = FindPage(pi->parent);
            const std::wstring pt = par ? par->title : L"";
            const float pw = r.TextWidth(pt, 28.0f, FontWeight::Semibold);
            m_crumbRect = {x, y, pw, h};
            r.Text(pt, m_crumbRect, m_crumbHover ? p.text : p.textSecondary, 28.0f, FontWeight::Semibold, TextFlags::VCenter);
            x += pw + 12.0f;
            r.Glyph(Icon::ChevronRight, {x, y + h * 0.5f - 7.0f, 14.0f, 14.0f}, p.textSecondary, 1.2f);
            x += 26.0f;
        }
        r.Text(title, {x, y, Width() - x - 24.0f, h}, p.text, 28.0f, FontWeight::Semibold, TextFlags::VCenter);
    }

    // ================================================================== ввод
    void OnMouseDown(const MouseEvent& e) override
    {
        if (!HasDialog() && e.button == MouseButton::Left)
        {
            if (UserCardRect().Contains(e.pos))
            {
                Navigate(L"yourinfo");
                return;
            }
            if (!m_crumbRect.Empty() && m_crumbRect.Contains(e.pos))
            {
                const PageInfo* pi = FindPage(m_current);
                if (pi && pi->parent)
                    Navigate(pi->parent);
                return;
            }
        }
        Window::OnMouseDown(e);
    }

    void OnMouseMove(const MouseEvent& e) override
    {
        Window::OnMouseMove(e);
        m_userHover = !HasDialog() && UserCardRect().Contains(e.pos);
        m_crumbHover = !HasDialog() && !m_crumbRect.Empty() && m_crumbRect.Contains(e.pos);
    }

    void OnMouseLeave() override
    {
        Window::OnMouseLeave();
        m_userHover = m_crumbHover = false;
    }

    bool OnKeyDown(UINT vk, const KeyMods& mods) override
    {
        if (mods.alt && vk == VK_LEFT)
        {
            GoBack();
            return true;
        }
        if (vk == VK_BACK && m_ui.FocusedChild() != m_search)
        {
            GoBack();
            return true;
        }
        if (mods.ctrl && (vk == 'F' || vk == 'E'))
        {
            m_ui.SetFocus(m_search);
            m_search->SelectAll();
            return true;
        }
        return Window::OnKeyDown(vk, mods);
    }

    CursorType Cursor(Point local) const override
    {
        if (!HasDialog() && (UserCardRect().Contains(local) || (!m_crumbRect.Empty() && m_crumbRect.Contains(local))))
            return CursorType::Arrow;
        return Window::Cursor(local);
    }

    // ================================================================== поля
    Button* m_back = nullptr;
    TextBox* m_search = nullptr;
    ScrollPanel* m_nav = nullptr;
    ScrollPanel* m_page = nullptr;
    std::vector<std::pair<NavItem*, std::wstring>> m_navItems;
    std::wstring m_current;
    std::vector<std::wstring> m_history;
    std::wstring m_beforeSearch;
    std::wstring m_query;
    std::map<std::wstring, int> m_state;
    float m_y = 0.0f;
    float m_builtW = 0.0f;
    Canvas* m_hero = nullptr;
    Rect m_lastCanvas;
    Rect m_bgGrid;
    Rect m_swatchGrid;
    Rect m_themeGrid;
    Rect m_devGrid;
    Rect m_crumbRect;
    bool m_crumbHover = false;
    bool m_userHover = false;
    std::wstring m_syncTime;
    int m_updState = 0;
    double m_updStart = 0.0;
    std::wstring m_lastCheck = L"сегодня, " + FormatTime(AddMinutes(LocalNow(), -42));
};
} // namespace

std::unique_ptr<Window> CreateSettings(const std::wstring& arg) { return std::make_unique<SettingsWindow>(arg); }
