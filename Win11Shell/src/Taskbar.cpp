#include "Taskbar.h"

#include "StartMenu.h"

namespace
{
constexpr float kBtn = 40.0f;    // размер кнопки
constexpr float kStride = 44.0f; // шаг кнопок
constexpr float kPreviewCardW = 212.0f;
constexpr float kPreviewCardH = 160.0f;
} // namespace

void DrawStartLogo(Renderer& r, const Rect& rc)
{
    // Собственный логотип «Пуск»: четыре скруглённые плитки с градиентом.
    const float s = std::min(rc.w, rc.h);
    const float x0 = rc.x + (rc.w - s) * 0.5f, y0 = rc.y + (rc.h - s) * 0.5f;
    const float gap = s * 0.07f;
    const float t = (s - gap) * 0.5f;
    const float rad = t * 0.22f;
    const Color c[4] = {Color::Hex(0x4CC2FF), Color::Hex(0x2AA4F4), Color::Hex(0x1C8FEA), Color::Hex(0x0A6FD4)};
    const Color c2[4] = {Color::Hex(0x34AEF9), Color::Hex(0x1A8AE6), Color::Hex(0x0E78DA), Color::Hex(0x0257BE)};
    for (int i = 0; i < 4; ++i)
    {
        const float x = x0 + static_cast<float>(i % 2) * (t + gap);
        const float y = y0 + static_cast<float>(i / 2) * (t + gap);
        r.FillGradientV({x, y, t, t}, rad, c[i], c2[i]);
    }
}

Taskbar::Taskbar() : m_pinned(TaskbarPinnedApps()) {}

float Taskbar::Top() const { return GetShell().ScreenH() - kHeight; }

bool Taskbar::IsPinned(AppId app) const { return std::find(m_pinned.begin(), m_pinned.end(), app) != m_pinned.end(); }

void Taskbar::Pin(AppId app)
{
    if (!IsPinned(app))
        m_pinned.push_back(app);
}

void Taskbar::Unpin(AppId app) { m_pinned.erase(std::remove(m_pinned.begin(), m_pinned.end(), app), m_pinned.end()); }

std::vector<Window*> Taskbar::AppWindows(AppId app) const
{
    std::vector<Window*> out;
    for (Window* w : GetShell().WM().WindowsOf(app))
        out.push_back(w);
    return out;
}

Taskbar::AppState& Taskbar::State(AppId app) { return m_states[app]; }

void Taskbar::Layout()
{
    Shell& sh = GetShell();
    const ShellSettings& st = sh.Settings();
    Renderer& r = sh.R();
    const float W = sh.ScreenW();
    const float y0 = Top();
    const float by = y0 + (kHeight - kBtn) * 0.5f;
    m_items.clear();

    // Порядок приложений: закреплённые, затем запущенные незакреплённые (в порядке запуска).
    std::vector<AppId> apps = m_pinned;
    for (auto& w : sh.WM().Windows())
    {
        if (w->IsClosing() || !w->ShowInTaskbar())
            continue;
        if (std::find(apps.begin(), apps.end(), w->App()) == apps.end())
            apps.push_back(w->App());
    }

    // Центральная группа.
    std::vector<Item> group;
    auto add = [&](Kind k, AppId app, float w) {
        Item it;
        it.kind = k;
        it.app = app;
        it.rect = {0.0f, by, w, kBtn};
        group.push_back(it);
    };
    add(Kind::Start, AppId::None, kBtn);
    if (st.searchMode == 2)
        add(Kind::Search, AppId::None, 196.0f);
    else if (st.searchMode == 1)
        add(Kind::Search, AppId::None, kBtn);
    if (st.showTaskView)
        add(Kind::TaskView, AppId::None, kBtn);
    if (st.showWidgets && !st.taskbarCenter)
        add(Kind::Widgets, AppId::None, kBtn);
    for (AppId a : apps)
        add(Kind::App, a, kBtn);

    float groupW = 0.0f;
    for (size_t i = 0; i < group.size(); ++i)
        groupW += group[i].rect.w + (i + 1 < group.size() ? kStride - kBtn : 0.0f);
    float x = st.taskbarCenter ? std::floor((W - groupW) * 0.5f) : 12.0f;
    for (Item& it : group)
    {
        it.rect.x = x;
        x += it.rect.w + (kStride - kBtn);
        if (it.kind == Kind::Search && st.searchMode == 2)
            it.rect = {it.rect.x, y0 + 7.0f, it.rect.w, kHeight - 14.0f};
        m_items.push_back(it);
    }

    // Мини-приложения слева (при выравнивании по центру).
    if (st.showWidgets && st.taskbarCenter)
    {
        Item it;
        it.kind = Kind::Widgets;
        it.rect = {4.0f, by, 132.0f, kBtn};
        m_items.push_back(it);
    }

    // Трей справа налево.
    float rx = W - 8.0f;
    {
        Item it;
        it.kind = Kind::ShowDesktop;
        it.rect = {W - 8.0f, y0, 8.0f, kHeight};
        m_items.push_back(it);
    }
    const SYSTEMTIME now = LocalNow();
    const float clockW = std::max(r.TextWidth(FormatTime(now), 12.0f), r.TextWidth(FormatDate(now), 12.0f)) + 16.0f + 26.0f;
    {
        Item it;
        it.kind = Kind::Clock;
        it.rect = {rx - clockW - 4.0f, by, clockW, kBtn};
        rx = it.rect.x;
        m_items.push_back(it);
    }
    {
        Item it;
        it.kind = Kind::Quick;
        it.rect = {rx - 82.0f - 2.0f, by, 82.0f, kBtn};
        rx = it.rect.x;
        m_items.push_back(it);
    }
    {
        Item it;
        it.kind = Kind::Lang;
        it.rect = {rx - 42.0f - 2.0f, by, 42.0f, kBtn};
        rx = it.rect.x;
        m_items.push_back(it);
    }
    {
        Item it;
        it.kind = Kind::Chevron;
        it.rect = {rx - 28.0f - 2.0f, by, 28.0f, kBtn};
        m_items.push_back(it);
    }
}

Rect Taskbar::StartButtonRect() const
{
    for (const Item& it : m_items)
    {
        if (it.kind == Kind::Start)
            return it.rect;
    }
    return {};
}

Rect Taskbar::SearchRect() const
{
    for (const Item& it : m_items)
    {
        if (it.kind == Kind::Search)
            return it.rect;
    }
    return StartButtonRect();
}

Rect Taskbar::ClockRect() const
{
    for (const Item& it : m_items)
    {
        if (it.kind == Kind::Clock)
            return it.rect;
    }
    return {};
}

Rect Taskbar::QuickRect() const
{
    for (const Item& it : m_items)
    {
        if (it.kind == Kind::Quick)
            return it.rect;
    }
    return {};
}

Rect Taskbar::ChevronRect() const
{
    for (const Item& it : m_items)
    {
        if (it.kind == Kind::Chevron)
            return it.rect;
    }
    return {};
}

Rect Taskbar::WidgetsRect() const
{
    for (const Item& it : m_items)
    {
        if (it.kind == Kind::Widgets)
            return it.rect;
    }
    return {};
}

Point Taskbar::ButtonCenter(const Window& w) const
{
    for (const Item& it : m_items)
    {
        if (it.kind == Kind::App && it.app == w.App())
            return it.rect.Center();
    }
    return {GetShell().ScreenW() * 0.5f, Top() + kHeight * 0.5f};
}

int Taskbar::ItemAt(Point p) const
{
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
    {
        const Rect& rc = m_items[static_cast<size_t>(i)].rect;
        Rect hit = rc;
        if (m_items[static_cast<size_t>(i)].kind != Kind::ShowDesktop)
            hit = {rc.x, Top(), rc.w, kHeight};
        if (hit.Contains(p))
            return i;
    }
    return -1;
}

bool Taskbar::HitTest(Point p) const
{
    if (p.y >= Top())
        return true;
    return m_previewVisible && PreviewRect().Contains(p);
}

// ---------------------------------------------------------------------------
// Отрисовка
// ---------------------------------------------------------------------------
void Taskbar::Draw(Renderer& r)
{
    Layout();
    Shell& sh = GetShell();
    const Palette& p = Theme::P();
    const float W = sh.ScreenW();
    const float y0 = Top();
    r.Acrylic({0.0f, y0, W, kHeight}, 0.0f, p.taskbarTint, p.taskbarAmount, 40.0f);
    r.FillRect({0.0f, y0, W, 1.0f}, p.dark ? Color(255, 255, 255, 0x12) : Color(0, 0, 0, 0x10));
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
        DrawItem(r, m_items[static_cast<size_t>(i)], i);
}

void Taskbar::DrawItem(Renderer& r, const Item& it, int index)
{
    Shell& sh = GetShell();
    const Palette& p = Theme::P();
    const ShellSettings& st = sh.Settings();
    const bool hover = index == m_hover;
    const bool press = index == m_press && hover;
    const Color hoverFill = p.dark ? Color(255, 255, 255, 0x10) : Color(255, 255, 255, 0x9A);
    const Color pressFill = p.dark ? Color(255, 255, 255, 0x0A) : Color(255, 255, 255, 0x60);
    const Color activeFill = p.dark ? Color(255, 255, 255, 0x0E) : Color(255, 255, 255, 0x88);
    auto bg = [&](bool on) {
        if (press)
            r.FillRoundRect(it.rect, 4.0f, pressFill);
        else if (hover || on)
        {
            r.FillRoundRect(it.rect, 4.0f, on && !hover ? activeFill : hoverFill);
            r.StrokeRoundRect(it.rect, 4.0f, p.dark ? Color(255, 255, 255, 0x0C) : Color(0, 0, 0, 0x08));
        }
    };
    const Rect& rc = it.rect;
    const float cx = rc.CenterX(), cy = rc.CenterY();
    switch (it.kind)
    {
    case Kind::Start:
    {
        bg(sh.Start().IsOpen());
        const float s = press ? 20.0f : 24.0f;
        DrawStartLogo(r, {cx - s * 0.5f, cy - s * 0.5f, s, s});
        break;
    }
    case Kind::Search:
        if (st.searchMode == 2)
        {
            const Rect box = rc;
            const Color fill = p.dark ? (hover ? Color(255, 255, 255, 0x18) : Color(255, 255, 255, 0x0F))
                                      : (hover ? Color(255, 255, 255, 0xE0) : Color(255, 255, 255, 0xB8));
            r.FillRoundRect(box, box.h * 0.5f, fill);
            r.StrokeRoundRect(box, box.h * 0.5f, p.dark ? Color(255, 255, 255, 0x14) : Color(0, 0, 0, 0x14));
            r.Glyph(Icon::Search, {box.x + 14.0f, cy - 8.0f, 16.0f, 16.0f}, p.text);
            r.Text(L"Поиск", {box.x + 40.0f, box.y, box.w - 48.0f, box.h}, p.textSecondary, 14.0f, FontWeight::Regular,
                   TextFlags::VCenter);
        }
        else
        {
            bg(false);
            r.Glyph(Icon::Search, {cx - 10.0f, cy - 10.0f, 20.0f, 20.0f}, p.text, 1.1f);
        }
        break;
    case Kind::TaskView:
    {
        bg(sh.TaskViewOpen());
        // Значок «Представление задач»: два прямоугольника.
        const float s = press ? 18.0f : 22.0f;
        const Rect a{cx - s * 0.5f + s * 0.28f, cy - s * 0.5f, s * 0.72f, s * 0.62f};
        const Rect b{cx - s * 0.5f, cy - s * 0.5f + s * 0.38f, s * 0.72f, s * 0.62f};
        r.FillRoundRect(a, 2.5f, p.dark ? Color::Hex(0x9A9A9A) : Color::Hex(0x5F5F5F));
        r.FillRoundRect(b, 2.5f, p.dark ? Color::Hex(0xF2F2F2) : Color::Hex(0x1F1F1F));
        r.StrokeRoundRect(b, 2.5f, p.dark ? Color::Hex(0x1C1C1C) : Color::Hex(0xEDEDED), 1.0f);
        break;
    }
    case Kind::Widgets:
        bg(false);
        if (st.taskbarCenter)
        {
            r.DrawAppIcon(AppIcon::Weather, {rc.x + 8.0f, cy - 12.0f, 24.0f, 24.0f});
            r.Text(L"17°C", {rc.x + 40.0f, rc.y + 4.0f, rc.w - 44.0f, 16.0f}, p.text, 12.0f);
            r.Text(L"Облачно", {rc.x + 40.0f, rc.y + 20.0f, rc.w - 44.0f, 16.0f}, p.textSecondary, 12.0f);
        }
        else
            r.DrawAppIcon(AppIcon::Weather, {cx - 12.0f, cy - 12.0f, 24.0f, 24.0f});
        break;
    case Kind::App:
    {
        const std::vector<Window*> wins = AppWindows(it.app);
        bool active = false;
        for (Window* w : wins)
            active = active || w->IsActive();
        AppState& s = State(it.app);
        bg(active);
        const float k = s.press.Value();
        const float sz = 24.0f * k;
        r.DrawAppIcon(GetAppInfo(it.app).icon, {cx - sz * 0.5f, cy - sz * 0.5f - 1.0f, sz, sz});
        // Индикатор запущенного приложения.
        const float target = wins.empty() ? 0.0f : (active ? 16.0f : 6.0f);
        s.indicator.Set(target, 0.2f, Ease::OutCubic);
        const float iw = s.indicator.Value();
        if (iw > 0.5f)
        {
            const Color ic = active ? p.accent : (p.dark ? Color::Hex(0x9E9E9E) : Color::Hex(0x8A8A8A));
            r.FillRoundRect({cx - iw * 0.5f, rc.Bottom() - 3.0f, iw, 3.0f}, 1.5f, ic);
        }
        if (wins.size() > 1)
        {
            // Несколько окон: второй «слой» за кнопкой.
            r.FillRoundRect({rc.Right() - 3.0f, rc.y + 8.0f, 2.0f, rc.h - 16.0f}, 1.0f,
                            p.dark ? Color(255, 255, 255, 0x30) : Color(0, 0, 0, 0x22));
        }
        break;
    }
    case Kind::Chevron:
        bg(false);
        r.Glyph(Icon::ChevronUp, {cx - 7.0f, cy - 7.0f, 14.0f, 14.0f}, p.text);
        break;
    case Kind::Lang:
        bg(false);
        r.Text(m_lang, rc, p.text, 12.0f, FontWeight::Regular, TextFlags::Middle);
        break;
    case Kind::Quick:
    {
        bg(sh.QS().IsOpen());
        const float x0 = rc.x + 10.0f;
        if (st.airplane)
            r.Glyph(Icon::Airplane, {x0, cy - 8.0f, 16.0f, 16.0f}, p.text);
        else
            r.WifiIcon({x0, cy - 8.0f, 16.0f, 16.0f}, p.text, 3, !st.wifi);
        Icon vol = Icon::Volume3;
        if (st.muted || st.volume <= 0.001f)
            vol = Icon::VolumeMute;
        else if (st.volume < 0.34f)
            vol = Icon::Volume1;
        else if (st.volume < 0.67f)
            vol = Icon::Volume2;
        r.Glyph(vol, {x0 + 24.0f, cy - 8.0f, 16.0f, 16.0f}, p.text);
        r.BatteryIcon({x0 + 48.0f, cy - 8.0f, 16.0f, 16.0f}, p.text, st.battery, st.charging);
        break;
    }
    case Kind::Clock:
    {
        bg(sh.NC().IsOpen());
        const SYSTEMTIME now = LocalNow();
        const Rect tr{rc.x + 8.0f, rc.y + 3.0f, rc.w - 36.0f, 17.0f};
        r.Text(FormatTime(now), tr, p.text, 12.0f, FontWeight::Regular, TextFlags::Right | TextFlags::VCenter);
        r.Text(FormatDate(now), tr.Translated(0.0f, 17.0f), p.text, 12.0f, FontWeight::Regular,
               TextFlags::Right | TextFlags::VCenter);
        r.Glyph(Icon::Bell, {rc.Right() - 24.0f, cy - 8.0f, 16.0f, 16.0f}, p.text);
        if (m_unread)
            r.FillCircle(rc.Right() - 10.0f, cy - 6.0f, 3.5f, p.accent);
        break;
    }
    case Kind::ShowDesktop:
        if (hover)
            r.FillRect({rc.x + 3.0f, rc.y + 10.0f, 1.0f, rc.h - 20.0f}, p.textTertiary);
        break;
    }
}

// ---------------------------------------------------------------------------
// Миниатюры окон
// ---------------------------------------------------------------------------
Rect Taskbar::PreviewRect() const
{
    const std::vector<Window*> wins = AppWindows(m_previewApp);
    const float n = static_cast<float>(std::max<size_t>(1, wins.size()));
    const float w = n * kPreviewCardW + (n - 1.0f) * 8.0f + 16.0f;
    const float h = kPreviewCardH + 16.0f;
    float x = m_previewAnchor.CenterX() - w * 0.5f;
    x = Clamp(x, 8.0f, GetShell().ScreenW() - w - 8.0f);
    return {std::floor(x), Top() - h - 8.0f, w, h};
}

Rect Taskbar::PreviewCard(int index) const
{
    const Rect pr = PreviewRect();
    return {pr.x + 8.0f + static_cast<float>(index) * (kPreviewCardW + 8.0f), pr.y + 8.0f, kPreviewCardW, kPreviewCardH};
}

int Taskbar::PreviewCardAt(Point p, bool* onClose) const
{
    const std::vector<Window*> wins = AppWindows(m_previewApp);
    for (int i = 0; i < static_cast<int>(wins.size()); ++i)
    {
        const Rect c = PreviewCard(i);
        if (c.Contains(p))
        {
            if (onClose)
                *onClose = Rect{c.Right() - 34.0f, c.y + 2.0f, 32.0f, 30.0f}.Contains(p);
            return i;
        }
    }
    return -1;
}

void Taskbar::DrawPreview(Renderer& r)
{
    const double now = NowSeconds();
    // Показ после задержки наведения.
    if (!m_previewVisible && m_previewApp != AppId::None && m_hover >= 0)
    {
        if (now - m_hoverStart > 0.45)
        {
            m_previewVisible = true;
            m_previewAnim.Start(0.0f, 1.0f, 0.16f, Ease::OutCubic);
        }
        else
            RequestFrame();
    }
    if (m_previewVisible && m_previewLeave > 0.0)
    {
        if (now - m_previewLeave > 0.3)
        {
            m_previewVisible = false;
            m_previewApp = AppId::None;
            m_previewLeave = 0.0;
        }
        else
            RequestFrame();
    }
    if (!m_previewVisible)
        return;
    const std::vector<Window*> wins = AppWindows(m_previewApp);
    if (wins.empty())
    {
        m_previewVisible = false;
        return;
    }
    const Palette& p = Theme::P();
    const float t = m_previewAnim.Value();
    const Rect pr = PreviewRect();
    r.PushOpacity(t);
    r.PushTransform(0.0f, (1.0f - t) * 8.0f);
    r.Shadow(pr, 8.0f, 20.0f, 6.0f, p.dark ? 0.5f : 0.25f);
    r.Acrylic(pr, 8.0f, p.acrylicTint, p.acrylicAmount);
    r.StrokeRoundRect(pr, 8.0f, p.flyoutStroke);
    for (int i = 0; i < static_cast<int>(wins.size()); ++i)
    {
        Window* w = wins[static_cast<size_t>(i)];
        const Rect c = PreviewCard(i);
        const bool hot = i == m_previewHover;
        if (hot)
            r.FillRoundRect(c, 6.0f, p.subtleHover);
        r.DrawAppIcon(w->WindowIcon(), {c.x + 10.0f, c.y + 9.0f, 16.0f, 16.0f});
        r.Text(w->Title(), {c.x + 34.0f, c.y + 2.0f, c.w - 34.0f - 36.0f, 30.0f}, p.text, 12.0f, FontWeight::Regular,
               TextFlags::VCenter);
        if (hot)
        {
            const Rect cb{c.Right() - 34.0f, c.y + 4.0f, 28.0f, 26.0f};
            if (m_previewCloseHover)
                r.FillRoundRect(cb, 4.0f, p.closeHover);
            r.Glyph(Icon::Close, {cb.CenterX() - 6.0f, cb.CenterY() - 6.0f, 12.0f, 12.0f},
                    m_previewCloseHover ? Color(255, 255, 255) : p.text);
        }
        GetShell().WM().DrawThumbnail(r, *w, {c.x + 8.0f, c.y + 34.0f, c.w - 16.0f, c.h - 42.0f});
    }
    r.PopTransform();
    r.PopOpacity();
}

// ---------------------------------------------------------------------------
// Ввод
// ---------------------------------------------------------------------------
std::wstring Taskbar::TooltipFor(const Item& it) const
{
    const ShellSettings& st = GetShell().Settings();
    switch (it.kind)
    {
    case Kind::Start:
        return L"Пуск";
    case Kind::Search:
        return L"Поиск";
    case Kind::TaskView:
        return L"Представление задач";
    case Kind::Widgets:
        return L"Мини-приложения";
    case Kind::App:
        return GetAppInfo(it.app).name;
    case Kind::Chevron:
        return L"Отображать скрытые значки";
    case Kind::Lang:
        return m_lang == L"РУС" ? L"Русский (Россия)\nРусская клавиатура" : L"Английский (США)\nКлавиатура США";
    case Kind::Quick:
    {
        std::wstring s = st.airplane ? L"Режим «в самолёте»" : (st.wifi ? st.network + L"\nДоступ к Интернету" : L"Нет подключения");
        s += L"\nДинамики: " + std::to_wstring(static_cast<int>(st.volume * 100.0f + 0.5f)) + L"%";
        s += L"\nЗаряд батареи: " + std::to_wstring(static_cast<int>(st.battery * 100.0f + 0.5f)) + L"%";
        return s;
    }
    case Kind::Clock:
    {
        const SYSTEMTIME now = LocalNow();
        return FormatLongDate(now) + L" " + std::to_wstring(now.wYear) + L" г.";
    }
    case Kind::ShowDesktop:
        return L"Свернуть все окна";
    }
    return L"";
}

void Taskbar::MouseMove(const MouseEvent& e)
{
    Shell& sh = GetShell();
    // Над миниатюрами.
    if (m_previewVisible && PreviewRect().Contains(e.pos))
    {
        m_previewLeave = 0.0;
        bool onClose = false;
        m_previewHover = PreviewCardAt(e.pos, &onClose);
        m_previewCloseHover = onClose;
        if (m_previewHover >= 0)
        {
            const std::vector<Window*> wins = AppWindows(m_previewApp);
            if (m_previewHover < static_cast<int>(wins.size()))
                sh.Tooltip(wins[static_cast<size_t>(m_previewHover)], wins[static_cast<size_t>(m_previewHover)]->Title(),
                           PreviewCard(m_previewHover), true);
        }
        return;
    }
    m_previewHover = -1;
    const int idx = ItemAt(e.pos);
    if (idx != m_hover)
    {
        m_hover = idx;
        m_hoverStart = NowSeconds();
    }
    if (idx < 0)
    {
        if (m_previewVisible && m_previewLeave == 0.0)
            m_previewLeave = NowSeconds();
        return;
    }
    const Item& it = m_items[static_cast<size_t>(idx)];
    if (it.kind == Kind::App && !AppWindows(it.app).empty())
    {
        if (m_previewApp != it.app)
        {
            const bool wasVisible = m_previewVisible;
            m_previewApp = it.app;
            m_previewAnchor = it.rect;
            if (wasVisible)
                m_previewAnim.Snap(1.0f);
            else
                m_hoverStart = NowSeconds();
        }
        m_previewLeave = 0.0;
        return;
    }
    if (m_previewVisible && m_previewLeave == 0.0)
        m_previewLeave = NowSeconds();
    if (!m_previewVisible)
        m_previewApp = AppId::None;
    const std::wstring tip = TooltipFor(it);
    if (!tip.empty() && it.kind != Kind::Search)
        sh.Tooltip(&m_items, tip + std::wstring(static_cast<size_t>(idx) % 2, L'​'), it.rect, true);
}

void Taskbar::MouseDown(const MouseEvent& e)
{
    m_pressInPreview = m_previewVisible && PreviewRect().Contains(e.pos);
    if (m_pressInPreview)
        return;
    m_press = ItemAt(e.pos);
    if (m_press >= 0 && e.button == MouseButton::Left)
    {
        const Item& it = m_items[static_cast<size_t>(m_press)];
        if (it.kind == Kind::App)
            State(it.app).press.Set(0.82f, 0.1f, Ease::OutCubic);
    }
}

void Taskbar::MouseUp(const MouseEvent& e)
{
    Shell& sh = GetShell();
    if (m_pressInPreview)
    {
        m_pressInPreview = false;
        bool onClose = false;
        const int card = PreviewCardAt(e.pos, &onClose);
        const std::vector<Window*> wins = AppWindows(m_previewApp);
        if (card >= 0 && card < static_cast<int>(wins.size()))
        {
            Window* w = wins[static_cast<size_t>(card)];
            if (onClose)
                w->Close();
            else
            {
                sh.WM().Activate(w);
                m_previewVisible = false;
                m_previewApp = AppId::None;
            }
        }
        return;
    }
    const int pressed = m_press;
    m_press = -1;
    for (auto& kv : m_states)
    {
        if (kv.second.press.Target() < 1.0f)
            kv.second.press.Start(kv.second.press.Value(), 1.0f, 0.35f, Ease::OutBack);
    }
    const int idx = ItemAt(e.pos);
    if (idx < 0 || idx != pressed)
        return;
    const Item it = m_items[static_cast<size_t>(idx)];
    if (e.button == MouseButton::Left)
        Click(it);
    else if (e.button == MouseButton::Right)
        RightClick(it, e.pos);
}

void Taskbar::Click(const Item& it)
{
    Shell& sh = GetShell();
    switch (it.kind)
    {
    case Kind::Start:
        sh.ToggleStart();
        break;
    case Kind::Search:
        sh.ToggleStart(true);
        break;
    case Kind::TaskView:
        sh.ToggleTaskView();
        break;
    case Kind::Widgets:
        sh.ToggleWidgets();
        break;
    case Kind::App:
    {
        m_previewVisible = false;
        m_previewApp = AppId::None;
        const std::vector<Window*> wins = AppWindows(it.app);
        if (wins.empty())
        {
            sh.Launch(it.app);
            break;
        }
        if (wins.size() == 1)
        {
            Window* w = wins.front();
            if (w->IsActive() && !w->IsMinimized())
                sh.WM().Minimize(w);
            else
                sh.WM().Activate(w);
            break;
        }
        // Несколько окон: переключение по кругу, начиная с самого верхнего неактивного.
        Window* active = sh.WM().Active();
        Window* next = nullptr;
        const auto& all = sh.WM().Windows();
        for (auto rit = all.rbegin(); rit != all.rend(); ++rit)
        {
            Window* w = rit->get();
            if (w->App() == it.app && w != active && !w->IsClosing())
            {
                next = w;
                break;
            }
        }
        if (next)
            sh.WM().Activate(next);
        break;
    }
    case Kind::Chevron:
        sh.ToggleTray();
        break;
    case Kind::Lang:
    {
        std::vector<MenuItem> items;
        items.push_back(MenuItem::Check(L"РУС   Русский (Россия)", m_lang == L"РУС", [this]() { m_lang = L"РУС"; }, true));
        items.push_back(MenuItem::Check(L"ENG   Английский (США)", m_lang == L"ENG", [this]() { m_lang = L"ENG"; }, true));
        items.push_back(MenuItem::Sep());
        items.push_back(MenuItem(L"Параметры языка", Icon::Settings, []() { GetShell().Launch(AppId::Settings, L"time"); }));
        sh.ShowMenuBelow(it.rect, std::move(items), 0.0f, true);
        break;
    }
    case Kind::Quick:
        sh.ToggleQuickSettings();
        break;
    case Kind::Clock:
        sh.ToggleNotifications();
        break;
    case Kind::ShowDesktop:
        sh.CloseFlyouts();
        sh.WM().ShowDesktop();
        break;
    }
}

void Taskbar::RightClick(const Item& it, Point p)
{
    Shell& sh = GetShell();
    if (it.kind == Kind::Start)
    {
        sh.ShowWinXMenu({it.rect.x, it.rect.y - 4.0f});
        return;
    }
    if (it.kind == Kind::App)
    {
        const AppId app = it.app;
        const std::vector<Window*> wins = AppWindows(app);
        std::vector<MenuItem> items;
        MenuItem open(GetAppInfo(app).name, Icon::None, [app]() { GetShell().Launch(app); });
        items.push_back(open);
        items.push_back(MenuItem::Sep());
        if (IsPinned(app))
            items.push_back(MenuItem(L"Открепить от панели задач", Icon::Unpin, [this, app]() { Unpin(app); }));
        else
            items.push_back(MenuItem(L"Закрепить на панели задач", Icon::Pin, [this, app]() { Pin(app); }));
        if (!wins.empty())
        {
            items.push_back(MenuItem(wins.size() > 1 ? L"Закрыть все окна" : L"Закрыть окно", Icon::Close, [app]() {
                for (Window* w : GetShell().WM().WindowsOf(app))
                    w->Close();
            }));
        }
        sh.ShowMenuBelow(it.rect, std::move(items), 220.0f, true);
        return;
    }
    std::vector<MenuItem> items;
    items.push_back(MenuItem(L"Диспетчер задач", Icon::Performance, []() { GetShell().Launch(AppId::TaskManager); }));
    items.push_back(MenuItem::Sep());
    items.push_back(MenuItem(L"Параметры панели задач", Icon::Settings, []() { GetShell().Launch(AppId::Settings, L"taskbar"); }));
    sh.ShowMenu({p.x, Top() - 4.0f}, std::move(items));
}

bool Taskbar::MouseWheel(const MouseEvent& e)
{
    // Колесо над значком громкости меняет громкость (как в Windows 11).
    if (QuickRect().Contains(e.pos))
    {
        ShellSettings& st = GetShell().Settings();
        st.volume = Saturate(st.volume + e.wheel * 0.02f);
        st.muted = false;
        return true;
    }
    return true;
}

void Taskbar::MouseLeave()
{
    m_hover = -1;
    m_previewHover = -1;
    if (m_previewVisible && m_previewLeave == 0.0)
        m_previewLeave = NowSeconds();
    if (!m_previewVisible)
        m_previewApp = AppId::None;
}

CursorType Taskbar::Cursor(Point /*p*/) const { return CursorType::Arrow; }
