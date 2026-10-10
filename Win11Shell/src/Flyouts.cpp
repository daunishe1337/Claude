#include "Flyouts.h"

#include "Taskbar.h"
#include "apps/Photos.h"

// ===========================================================================
// Flyout
// ===========================================================================
void Flyout::Open()
{
    if (m_open)
        return;
    m_open = true;
    OnOpen();
    m_anim.Set(1.0f, 0.26f, Ease::OutCubic);
}

void Flyout::Close()
{
    if (!m_open)
        return;
    m_open = false;
    m_ui.SetFocus(nullptr);
    m_ui.OnMouseLeave();
    OnClose();
    m_anim.Set(0.0f, 0.16f, Ease::OutCubic);
}

bool Flyout::BeginAnim(Renderer& r, float dx, float dy)
{
    const float t = m_anim.Value();
    if (t <= 0.002f)
        return false;
    r.PushOpacity(Saturate(t * 1.4f));
    r.PushTransform(dx * (1.0f - t), dy * (1.0f - t));
    return true;
}

void Flyout::EndAnim(Renderer& r)
{
    r.PopTransform();
    r.PopOpacity();
}

void Flyout::DrawPanel(Renderer& r, const Rect& rc, float radius)
{
    const Palette& p = Theme::P();
    r.Shadow(rc, radius, 30.0f, 10.0f, p.dark ? 0.55f : 0.26f);
    r.Acrylic(rc, radius, p.acrylicTint, p.acrylicAmount);
    r.StrokeRoundRect(rc, radius, p.flyoutStroke);
}

void Flyout::MouseMove(const MouseEvent& e)
{
    Widget::Origin() = {0.0f, 0.0f};
    m_ui.OnMouseMove(e);
    Widget* leaf = m_ui.HoverLeaf();
    if (leaf && !leaf->Tooltip().empty() && !m_ui.HasCapture())
        GetShell().Tooltip(leaf, leaf->Tooltip(), leaf->Bounds(), true);
}

void Flyout::MouseDown(const MouseEvent& e)
{
    Widget::Origin() = {0.0f, 0.0f};
    m_ui.OnMouseDown(e);
}

void Flyout::MouseUp(const MouseEvent& e)
{
    Widget::Origin() = {0.0f, 0.0f};
    m_ui.OnMouseUp(e);
}

bool Flyout::MouseWheel(const MouseEvent& e) { return m_ui.OnMouseWheel(e); }
void Flyout::MouseLeave() { m_ui.OnMouseLeave(); }

bool Flyout::KeyDown(UINT vk, const KeyMods& mods)
{
    if (m_ui.OnKeyDown(vk, mods))
        return true;
    if (vk == VK_ESCAPE)
    {
        Close();
        return true;
    }
    return false;
}

bool Flyout::Char(wchar_t ch) { return m_ui.OnChar(ch); }
CursorType Flyout::Cursor(Point p) const { return m_ui.Cursor(p); }

// ===========================================================================
// Быстрые настройки
// ===========================================================================
namespace
{
class QsTile : public Widget
{
public:
    QsTile(Icon glyph, std::wstring caption, bool on, bool split)
        : icon(glyph), label(std::move(caption)), state(on), hasMore(split)
    {
    }
    Icon icon;
    std::wstring label;
    bool state;
    bool hasMore;
    bool toggle = true;
    std::function<void(bool)> onToggle;
    std::function<void()> onMore;

    Rect Button() const { return {m_bounds.x, m_bounds.y, m_bounds.w, 48.0f}; }
    bool HitTest(Point p) const override { return m_visible && Button().Contains(p); }
    void OnMouseMove(const MouseEvent& e) override { m_overMore = hasMore && e.pos.x > m_bounds.Right() - 34.0f; }
    void OnMouseLeave() override
    {
        Widget::OnMouseLeave();
        m_overMore = false;
    }
    bool OnMouseDown(const MouseEvent& e) override
    {
        m_pressed = e.button == MouseButton::Left;
        m_pressMore = m_overMore;
        return true;
    }
    void OnMouseUp(const MouseEvent& e) override
    {
        const bool click = m_pressed && Button().Contains(e.pos);
        m_pressed = false;
        if (!click)
            return;
        if (m_pressMore && onMore)
            PostAction(onMore);
        else
        {
            if (toggle)
                state = !state;
            if (onToggle)
            {
                auto cb = onToggle;
                const bool v = state;
                PostAction([cb, v]() { cb(v); });
            }
        }
    }
    void Draw(Renderer& r) override
    {
        const Palette& p = Theme::P();
        const Rect b = Button();
        const bool down = m_pressed && m_hovered;
        if (state)
            r.FillRoundRect(b, 4.0f, down ? p.accentPressed : (m_hovered && !m_overMore ? p.accentHover : p.accent));
        else
        {
            r.FillRoundRect(b, 4.0f, down ? p.controlPressed : (m_hovered && !m_overMore ? p.controlHover : p.control));
            r.StrokeRoundRect(b, 4.0f, p.controlStroke);
        }
        const Color fg = state ? p.textOnAccent : p.text;
        float iconCx = b.CenterX();
        if (hasMore)
        {
            const Rect more{b.Right() - 34.0f, b.y, 34.0f, b.h};
            if (m_hovered && m_overMore)
                r.FillRoundRect(more, 0.0f, 4.0f, 4.0f, 0.0f, state ? Color(255, 255, 255, 0x26) : p.subtleHover);
            r.FillRect({more.x, b.y + 12.0f, 1.0f, b.h - 24.0f}, state ? Color(0, 0, 0, 0x26) : p.divider);
            r.Glyph(Icon::ChevronRight, {more.CenterX() - 6.0f, b.CenterY() - 6.0f, 12.0f, 12.0f}, fg);
            iconCx = b.x + (b.w - 34.0f) * 0.5f;
        }
        if (icon == Icon::Wifi)
            r.WifiIcon({iconCx - 8.0f, b.CenterY() - 8.0f, 16.0f, 16.0f}, fg, 3, false);
        else
            r.Glyph(icon, {iconCx - 8.0f, b.CenterY() - 8.0f, 16.0f, 16.0f}, fg);
        r.Text(label, {m_bounds.x - 4.0f, b.Bottom() + 6.0f, m_bounds.w + 8.0f, 18.0f}, p.text, 12.0f,
               FontWeight::Regular, TextFlags::Center);
    }

private:
    bool m_overMore = false;
    bool m_pressMore = false;
};

class IconButton : public Button
{
public:
    explicit IconButton(Icon glyph) : Button(L"", glyph, ButtonStyle::Subtle) {}
};

class NetRow : public Widget
{
public:
    std::wstring name;
    bool secured = true;
    bool connected = false;
    bool expanded = false;
    int bars = 3;
    std::function<void()> onClick;
    void Draw(Renderer& r) override
    {
        const Palette& p = Theme::P();
        const Rect& b = m_bounds;
        if (m_hovered || expanded)
            r.FillRoundRect(b, 4.0f, m_pressed ? p.subtlePressed : p.subtleHover);
        r.WifiIcon({b.x + 14.0f, b.y + 14.0f, 18.0f, 18.0f}, p.text, bars, false);
        if (secured)
            r.Glyph(Icon::Lock, {b.x + 25.0f, b.y + 27.0f, 9.0f, 9.0f}, p.text, 1.4f);
        r.Text(name, {b.x + 46.0f, b.y + 8.0f, b.w - 56.0f, 20.0f}, p.text, 14.0f);
        const std::wstring status = connected ? (secured ? L"Подключено, защищено" : L"Подключено")
                                              : (secured ? L"Защищено" : L"Открытая");
        r.Text(status, {b.x + 46.0f, b.y + 28.0f, b.w - 56.0f, 18.0f}, p.textSecondary, 12.0f);
    }
    bool OnMouseDown(const MouseEvent& e) override
    {
        m_pressed = e.button == MouseButton::Left;
        return true;
    }
    void OnMouseUp(const MouseEvent& e) override
    {
        const bool click = m_pressed && m_bounds.Contains(e.pos);
        m_pressed = false;
        if (click && onClick)
            PostAction(onClick);
    }
};
} // namespace

QuickSettings::QuickSettings() = default;

Rect QuickSettings::Bounds() const
{
    Shell& sh = GetShell();
    const float h = m_page == 0 ? 376.0f : 466.0f;
    const float top = sh.TB().Top();
    return {sh.ScreenW() - 12.0f - 360.0f, top - 12.0f - h, 360.0f, h};
}

void QuickSettings::OnOpen()
{
    m_page = 0;
    m_expandedNet = -1;
    Build();
}

bool QuickSettings::KeyDown(UINT vk, const KeyMods& mods)
{
    if (vk == VK_ESCAPE && m_page != 0)
    {
        m_page = 0;
        Build();
        return true;
    }
    return Flyout::KeyDown(vk, mods);
}

void QuickSettings::Build()
{
    m_ui.Clear();
    if (m_page == 1)
    {
        BuildWifiPage();
        return;
    }
    Shell& sh = GetShell();
    ShellSettings& st = sh.Settings();
    const Rect b = Bounds();
    m_ui.SetBounds(b);
    const float x0 = b.x + 24.0f, y0 = b.y + 24.0f;
    auto tile = [&](int idx, Icon icon, const std::wstring& label, bool on, bool split) {
        const float x = x0 + static_cast<float>(idx % 3) * (96.0f + 12.0f);
        const float y = y0 + static_cast<float>(idx / 3) * 84.0f;
        auto* t = m_ui.Add<QsTile>(icon, label, on, split);
        t->SetBounds({x, y, 96.0f, 76.0f});
        return t;
    };
    auto* wifi = tile(0, Icon::Wifi, st.wifi && !st.airplane ? st.network : L"Wi-Fi", st.wifi && !st.airplane, true);
    wifi->onToggle = [this](bool v) {
        ShellSettings& s = GetShell().Settings();
        s.wifi = v;
        if (v)
            s.airplane = false;
        Build();
    };
    wifi->onMore = [this]() {
        m_page = 1;
        Build();
    };
    auto* bt = tile(1, Icon::Bluetooth, L"Bluetooth", st.bluetooth && !st.airplane, true);
    bt->onToggle = [this](bool v) {
        GetShell().Settings().bluetooth = v;
        if (v)
            GetShell().Settings().airplane = false;
        Build();
    };
    bt->onMore = []() { GetShell().Launch(AppId::Settings, L"bluetooth"); };
    auto* air = tile(2, Icon::Airplane, L"Режим «в самолёте»", st.airplane, false);
    air->onToggle = [this](bool v) {
        ShellSettings& s = GetShell().Settings();
        s.airplane = v;
        if (v)
        {
            s.wifi = false;
            s.bluetooth = false;
        }
        else
            s.wifi = true;
        Build();
    };
    auto* saver = tile(3, Icon::BatterySaver, L"Экономия заряда", st.batterySaver, false);
    saver->onToggle = [](bool v) { GetShell().Settings().batterySaver = v; };
    auto* night = tile(4, Icon::NightLight, L"Ночной свет", st.nightLight, false);
    night->onToggle = [](bool v) {
        GetShell().Settings().nightLight = v;
        GetShell().ApplyNightLight();
    };
    auto* acc = tile(5, Icon::Accessibility, L"Спец. возможности", false, true);
    acc->toggle = false;
    acc->onToggle = [](bool) { GetShell().Launch(AppId::Settings, L"accessibility"); };
    acc->onMore = []() { GetShell().Launch(AppId::Settings, L"accessibility"); };

    // Ползунки яркости и громкости.
    const float sy = y0 + 84.0f * 2.0f + 6.0f;
    auto* sunBtn = m_ui.Add<IconButton>(Icon::Brightness);
    sunBtn->SetBounds({x0 - 8.0f, sy, 36.0f, 36.0f});
    sunBtn->SetTooltip(L"Яркость");
    auto* bright = m_ui.Add<Slider>();
    bright->SetBounds({x0 + 32.0f, sy, b.w - 48.0f - 32.0f - 8.0f, 36.0f});
    bright->value = (st.brightness - 0.3f) / 0.7f;
    bright->onChange = [](float v) { GetShell().Settings().brightness = 0.3f + 0.7f * v; };

    auto* volBtn = m_ui.Add<IconButton>(st.muted || st.volume <= 0.001f ? Icon::VolumeMute : Icon::Volume3);
    volBtn->SetBounds({x0 - 8.0f, sy + 48.0f, 36.0f, 36.0f});
    volBtn->SetTooltip(L"Без звука");
    auto* vol = m_ui.Add<Slider>();
    vol->SetBounds({x0 + 32.0f, sy + 48.0f, b.w - 48.0f - 32.0f - 8.0f - 36.0f, 36.0f});
    vol->value = st.muted ? 0.0f : st.volume;
    vol->onChange = [volBtn](float v) {
        ShellSettings& s = GetShell().Settings();
        s.volume = v;
        s.muted = false;
        volBtn->icon = v <= 0.001f ? Icon::VolumeMute : (v < 0.34f ? Icon::Volume1 : (v < 0.67f ? Icon::Volume2 : Icon::Volume3));
    };
    volBtn->onClick = [this]() {
        ShellSettings& s = GetShell().Settings();
        s.muted = !s.muted;
        Build();
    };
    auto* outBtn = m_ui.Add<IconButton>(Icon::ChevronRight);
    outBtn->iconSize = 12.0f;
    outBtn->SetBounds({b.Right() - 24.0f - 32.0f, sy + 50.0f, 32.0f, 32.0f});
    outBtn->SetTooltip(L"Выбрать устройство вывода звука");
    outBtn->onClick = []() { GetShell().Launch(AppId::Settings, L"sound"); };

    // Нижняя панель.
    auto* gear = m_ui.Add<IconButton>(Icon::Settings);
    gear->SetBounds({b.Right() - 16.0f - 36.0f, b.Bottom() - 42.0f, 36.0f, 36.0f});
    gear->SetTooltip(L"Все параметры");
    gear->onClick = []() { GetShell().Launch(AppId::Settings); };
    auto* edit = m_ui.Add<IconButton>(Icon::Edit);
    edit->SetBounds({b.Right() - 16.0f - 36.0f - 40.0f, b.Bottom() - 42.0f, 36.0f, 36.0f});
    edit->SetTooltip(L"Изменить быстрые настройки");
    edit->SetEnabled(false);
}

void QuickSettings::BuildWifiPage()
{
    Shell& sh = GetShell();
    ShellSettings& st = sh.Settings();
    const Rect b = Bounds();
    m_ui.SetBounds(b);
    auto* back = m_ui.Add<IconButton>(Icon::ArrowLeft);
    back->SetBounds({b.x + 12.0f, b.y + 14.0f, 36.0f, 32.0f});
    back->onClick = [this]() {
        m_page = 0;
        Build();
    };
    auto* title = m_ui.Add<Label>(L"Wi-Fi", 14.0f, FontWeight::Semibold);
    title->SetBounds({b.x + 56.0f, b.y + 14.0f, 120.0f, 32.0f});
    auto* tg = m_ui.Add<ToggleSwitch>(st.wifi && !st.airplane);
    tg->showLabel = false;
    tg->SetBounds({b.Right() - 24.0f - 44.0f, b.y + 14.0f, 44.0f, 32.0f});
    tg->onChange = [this](bool v) {
        ShellSettings& s = GetShell().Settings();
        s.wifi = v;
        if (v)
            s.airplane = false;
        Build();
    };
    if (!st.wifi || st.airplane)
    {
        auto* off = m_ui.Add<Label>(L"Wi-Fi выключен", 14.0f, FontWeight::Regular, Tone::Secondary);
        off->flags = TextFlags::Middle;
        off->SetBounds({b.x, b.y + 160.0f, b.w, 40.0f});
        return;
    }
    struct Net
    {
        const wchar_t* name;
        bool secured;
        int bars;
    };
    static const Net nets[] = {{L"HomeNet-5G", true, 3}, {L"HomeNet", true, 3}, {L"Office_Guest", false, 2},
                               {L"TP-Link_A3F2", true, 2}, {L"Keenetic-1234", true, 1}};
    float y = b.y + 60.0f;
    for (int i = 0; i < 5; ++i)
    {
        auto* row = m_ui.Add<NetRow>();
        row->name = nets[i].name;
        row->secured = nets[i].secured;
        row->bars = nets[i].bars;
        row->connected = st.network == nets[i].name;
        row->expanded = m_expandedNet == i;
        const float h = row->expanded ? 96.0f : 56.0f;
        row->SetBounds({b.x + 12.0f, y, b.w - 24.0f, h});
        row->onClick = [this, i]() {
            m_expandedNet = m_expandedNet == i ? -1 : i;
            Build();
        };
        if (row->expanded)
        {
            const bool conn = row->connected;
            auto* btn = m_ui.Add<Button>(conn ? L"Отключиться" : L"Подключиться", Icon::None,
                                         conn ? ButtonStyle::Standard : ButtonStyle::Accent);
            btn->SetBounds({b.Right() - 12.0f - 12.0f - 130.0f, y + 54.0f, 130.0f, 32.0f});
            const std::wstring name = nets[i].name;
            btn->onClick = [this, conn, name]() {
                ShellSettings& s = GetShell().Settings();
                s.network = conn ? L"" : name;
                s.wifi = true;
                m_expandedNet = -1;
                Build();
            };
        }
        y += h + 4.0f;
    }
    auto* more = m_ui.Add<Button>(L"Другие параметры Wi-Fi", Icon::None, ButtonStyle::Hyperlink);
    more->fontSize = 14.0f;
    more->SetBounds({b.x + 12.0f, b.Bottom() - 44.0f, 200.0f, 32.0f});
    more->onClick = []() { GetShell().Launch(AppId::Settings, L"network"); };
}

void QuickSettings::Draw(Renderer& r)
{
    if (!BeginAnim(r, 0.0f, 40.0f))
        return;
    const Palette& p = Theme::P();
    const ShellSettings& st = GetShell().Settings();
    const Rect b = Bounds();
    DrawPanel(r, b);
    // Нижняя полоса.
    const Rect bar{b.x, b.Bottom() - 48.0f, b.w, 48.0f};
    r.FillRoundRect(bar, 0.0f, 0.0f, 8.0f, 8.0f, p.dark ? Color(0, 0, 0, 0x2A) : Color(0, 0, 0, 0x08));
    r.FillRect({bar.x, bar.y, bar.w, 1.0f}, p.divider);
    if (m_page == 0)
    {
        r.BatteryIcon({bar.x + 22.0f, bar.CenterY() - 8.0f, 16.0f, 16.0f}, p.text, st.battery, st.charging);
        r.Text(std::to_wstring(static_cast<int>(st.battery * 100.0f + 0.5f)) + L" %", {bar.x + 46.0f, bar.y, 80.0f, bar.h},
               p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
    }
    m_ui.Draw(r);
    EndAnim(r);
}

// ===========================================================================
// Центр уведомлений и календарь
// ===========================================================================
NotificationCenter::NotificationCenter()
{
    const SYSTEMTIME now = LocalNow();
    m_viewYear = now.wYear;
    m_viewMonth = now.wMonth;
}

float NotificationCenter::NotifHeight() const
{
    if (m_items.empty())
        return 52.0f + 64.0f;
    const size_t n = std::min<size_t>(m_items.size(), 4);
    return 52.0f + static_cast<float>(n) * 104.0f + 4.0f;
}

Rect NotificationCenter::CalendarRect() const
{
    Shell& sh = GetShell();
    const float h = m_collapsed ? 48.0f : 48.0f + 1.0f + 48.0f + 32.0f + 240.0f + 10.0f + 57.0f;
    return {sh.ScreenW() - 12.0f - 360.0f, sh.TB().Top() - 12.0f - h, 360.0f, h};
}

Rect NotificationCenter::NotifRect() const
{
    const Rect cal = CalendarRect();
    const float h = NotifHeight();
    return {cal.x, cal.y - 12.0f - h, 360.0f, h};
}

Rect NotificationCenter::Bounds() const
{
    const Rect n = NotifRect(), c = CalendarRect();
    return Rect::LTRB(n.x, n.y, c.Right(), c.Bottom());
}

bool NotificationCenter::HitTest(Point p) const
{
    return m_open && (NotifRect().Contains(p) || CalendarRect().Contains(p));
}

Rect NotificationCenter::CardRect(int index) const
{
    const Rect n = NotifRect();
    return {n.x + 12.0f, n.y + 52.0f + static_cast<float>(index) * 104.0f, n.w - 24.0f, 96.0f};
}

void NotificationCenter::Add(const Notification& n)
{
    Notification copy = n;
    copy.id = m_nextId++;
    copy.time = LocalNow();
    m_items.insert(m_items.begin(), copy);
    if (m_items.size() > 20)
        m_items.resize(20);
    if (m_open)
        Build();
}

void NotificationCenter::ClearAll()
{
    m_items.clear();
    GetShell().TB().SetUnread(false);
    Build();
}

void NotificationCenter::OnOpen()
{
    const SYSTEMTIME now = LocalNow();
    m_viewYear = now.wYear;
    m_viewMonth = now.wMonth;
    m_selDay = -1;
    GetShell().TB().SetUnread(false);
    Build();
}

void NotificationCenter::ShiftMonth(int delta)
{
    m_viewMonth += delta;
    while (m_viewMonth < 1)
    {
        m_viewMonth += 12;
        --m_viewYear;
    }
    while (m_viewMonth > 12)
    {
        m_viewMonth -= 12;
        ++m_viewYear;
    }
    m_monthDir = delta > 0 ? 1 : -1;
    m_monthAnim.Start(0.0f, 1.0f, 0.25f, Ease::OutCubic);
}

void NotificationCenter::Build()
{
    m_ui.Clear();
    const Rect n = NotifRect();
    const Rect c = CalendarRect();
    if (!m_items.empty())
    {
        auto* clear = m_ui.Add<Button>(L"Очистить все", Icon::None, ButtonStyle::Standard);
        clear->fontSize = 12.0f;
        clear->SetBounds({n.Right() - 16.0f - 100.0f, n.y + 12.0f, 100.0f, 28.0f});
        clear->onClick = [this]() { ClearAll(); };
    }
    auto* collapse = m_ui.Add<IconButton>(m_collapsed ? Icon::ChevronUp : Icon::ChevronDown);
    collapse->iconSize = 12.0f;
    collapse->SetBounds({c.Right() - 12.0f - 32.0f, c.y + 8.0f, 32.0f, 32.0f});
    collapse->SetTooltip(m_collapsed ? L"Развернуть календарь" : L"Свернуть календарь");
    collapse->onClick = [this]() {
        m_collapsed = !m_collapsed;
        Build();
    };
    if (m_collapsed)
        return;
    auto* up = m_ui.Add<IconButton>(Icon::ChevronUp);
    up->iconSize = 12.0f;
    up->SetBounds({c.Right() - 12.0f - 32.0f - 36.0f, c.y + 57.0f, 32.0f, 32.0f});
    up->SetTooltip(L"Предыдущий месяц");
    up->onClick = [this]() { ShiftMonth(-1); };
    auto* down = m_ui.Add<IconButton>(Icon::ChevronDown);
    down->iconSize = 12.0f;
    down->SetBounds({c.Right() - 12.0f - 32.0f, c.y + 57.0f, 32.0f, 32.0f});
    down->SetTooltip(L"Следующий месяц");
    down->onClick = [this]() { ShiftMonth(1); };

    const float fy = c.Bottom() - 48.0f;
    auto* minus = m_ui.Add<IconButton>(Icon::Minimize);
    minus->iconSize = 12.0f;
    minus->SetBounds({c.x + 12.0f, fy + 6.0f, 32.0f, 32.0f});
    minus->SetEnabled(!GetShell().Settings().focus && m_focusMinutes > 5);
    minus->onClick = [this]() {
        m_focusMinutes = std::max(5, m_focusMinutes - 5);
        Build();
    };
    auto* plus = m_ui.Add<IconButton>(Icon::Add);
    plus->iconSize = 12.0f;
    plus->SetBounds({c.x + 12.0f + 32.0f + 64.0f, fy + 6.0f, 32.0f, 32.0f});
    plus->SetEnabled(!GetShell().Settings().focus && m_focusMinutes < 240);
    plus->onClick = [this]() {
        m_focusMinutes = std::min(240, m_focusMinutes + 5);
        Build();
    };
    const bool focus = GetShell().Settings().focus;
    auto* start = m_ui.Add<Button>(focus ? L"Остановить" : L"Фокусировка", focus ? Icon::Stop : Icon::Play, ButtonStyle::Standard);
    start->iconSize = 12.0f;
    start->SetBounds({c.Right() - 12.0f - 140.0f, fy + 6.0f, 140.0f, 32.0f});
    start->onClick = [this]() {
        ShellSettings& s = GetShell().Settings();
        s.focus = !s.focus;
        if (s.focus)
            GetShell().Notify(AppIcon::Clock, L"Часы", L"Сеанс фокусировки начат",
                              L"Продолжительность: " + std::to_wstring(m_focusMinutes) + L" мин. Уведомления будут скрыты.");
        Build();
    };
}

void NotificationCenter::Draw(Renderer& r)
{
    if (!BeginAnim(r, 0.0f, 40.0f))
        return;
    const Palette& p = Theme::P();
    // ---- Уведомления
    const Rect n = NotifRect();
    DrawPanel(r, n);
    r.Text(L"Уведомления", {n.x + 16.0f, n.y + 12.0f, 200.0f, 28.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
    if (m_items.empty())
        r.Text(L"Нет новых уведомлений", {n.x, n.y + 44.0f, n.w, n.h - 52.0f}, p.textSecondary, 14.0f, FontWeight::Regular,
               TextFlags::Middle);
    for (int i = 0; i < static_cast<int>(std::min<size_t>(m_items.size(), 4)); ++i)
    {
        const Notification& it = m_items[static_cast<size_t>(i)];
        const Rect cr = CardRect(i);
        r.FillRoundRect(cr, 6.0f, i == m_hoverCard ? (p.dark ? Color(255, 255, 255, 0x14) : Color(255, 255, 255, 0xE0)) : p.card);
        r.StrokeRoundRect(cr, 6.0f, p.cardStroke);
        r.DrawAppIcon(it.icon, {cr.x + 12.0f, cr.y + 12.0f, 16.0f, 16.0f});
        r.Text(it.app, {cr.x + 36.0f, cr.y + 10.0f, cr.w - 110.0f, 20.0f}, p.textSecondary, 12.0f, FontWeight::Regular,
               TextFlags::VCenter);
        if (i == m_hoverCard)
        {
            const Rect xr{cr.Right() - 34.0f, cr.y + 6.0f, 28.0f, 28.0f};
            if (m_hoverCardClose)
                r.FillRoundRect(xr, 4.0f, p.subtleHover);
            r.Glyph(Icon::Close, {xr.CenterX() - 6.0f, xr.CenterY() - 6.0f, 12.0f, 12.0f}, p.text);
        }
        else
            r.Text(FormatTime(it.time), {cr.x, cr.y + 10.0f, cr.w - 12.0f, 20.0f}, p.textSecondary, 12.0f,
                   FontWeight::Regular, TextFlags::VCenter | TextFlags::Right);
        r.Text(it.title, {cr.x + 12.0f, cr.y + 34.0f, cr.w - 24.0f, 20.0f}, p.text, 14.0f, FontWeight::Semibold);
        r.Text(it.body, {cr.x + 12.0f, cr.y + 55.0f, cr.w - 24.0f, 38.0f}, p.textSecondary, 13.0f, FontWeight::Regular,
               TextFlags::Wrap);
    }

    // ---- Календарь
    const Rect c = CalendarRect();
    DrawPanel(r, c);
    const SYSTEMTIME now = LocalNow();
    r.Text(FormatLongDate(now), {c.x + 16.0f, c.y, c.w - 70.0f, 48.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
    if (!m_collapsed)
    {
        r.FillRect({c.x, c.y + 48.0f, c.w, 1.0f}, p.divider);
        const float mt = m_monthAnim.Value();
        const float slide = (1.0f - mt) * 24.0f * static_cast<float>(m_monthDir);
        r.PushClip({c.x, c.y + 49.0f, c.w, 48.0f + 32.0f + 242.0f});
        r.PushOpacity(0.3f + 0.7f * mt);
        r.Text(std::wstring(MonthName(m_viewMonth)) + L" " + std::to_wstring(m_viewYear) + L" г.",
               {c.x + 16.0f, c.y + 49.0f + slide, 220.0f, 48.0f}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
        const float gx = c.x + 12.0f, gy = c.y + 49.0f + 48.0f;
        for (int d = 0; d < 7; ++d)
            r.Text(WeekdayShort(d), {gx + static_cast<float>(d) * 48.0f, gy, 48.0f, 32.0f}, p.text, 12.0f,
                   FontWeight::Regular, TextFlags::Middle);
        const int first = (DayOfWeek(m_viewYear, m_viewMonth, 1) + 6) % 7;
        const int dim = DaysInMonth(m_viewYear, m_viewMonth);
        int py = m_viewYear, pm = m_viewMonth - 1;
        if (pm < 1)
        {
            pm = 12;
            --py;
        }
        const int prevDim = DaysInMonth(py, pm);
        for (int cell = 0; cell < 42; ++cell)
        {
            const int dayNum = cell - first + 1;
            int shown = dayNum;
            bool other = false;
            if (dayNum < 1)
            {
                shown = prevDim + dayNum;
                other = true;
            }
            else if (dayNum > dim)
            {
                shown = dayNum - dim;
                other = true;
            }
            const float cx = gx + static_cast<float>(cell % 7) * 48.0f + 24.0f;
            const float cy = gy + 32.0f + static_cast<float>(cell / 7) * 40.0f + 20.0f + slide;
            const bool today = !other && shown == now.wDay && m_viewMonth == now.wMonth && m_viewYear == now.wYear;
            const bool sel = !other && shown == m_selDay && m_viewMonth == m_selMonth && m_viewYear == m_selYear;
            Color fg = other ? p.textTertiary : p.text;
            if (today)
            {
                r.FillCircle(cx, cy, 17.0f, p.accent);
                if (sel)
                    r.StrokeCircle(cx, cy, 15.0f, p.textOnAccent, 1.0f);
                fg = p.textOnAccent;
            }
            else if (cell == m_hoverDay)
                r.FillCircle(cx, cy, 17.0f, p.subtleHover);
            if (sel && !today)
                r.StrokeCircle(cx, cy, 16.5f, p.accent, 1.5f);
            r.Text(std::to_wstring(shown), {cx - 24.0f, cy - 20.0f, 48.0f, 40.0f}, fg, 14.0f, FontWeight::Regular,
                   TextFlags::Middle);
        }
        r.PopOpacity();
        r.PopClip();
        // Фокусировка.
        const float fy = c.Bottom() - 48.0f;
        r.FillRect({c.x, fy - 9.0f, c.w, 1.0f}, p.divider);
        r.FillRoundRect({c.x, fy - 8.0f, c.w, 56.0f}, 0.0f, 0.0f, 8.0f, 8.0f, p.dark ? Color(0, 0, 0, 0x2A) : Color(0, 0, 0, 0x08));
        r.Text(std::to_wstring(m_focusMinutes) + L" мин", {c.x + 44.0f, fy + 6.0f, 64.0f, 32.0f}, p.text, 14.0f,
               FontWeight::Regular, TextFlags::Middle);
    }
    m_ui.Draw(r);
    EndAnim(r);
}

void NotificationCenter::MouseMove(const MouseEvent& e)
{
    Flyout::MouseMove(e);
    m_hoverCard = -1;
    m_hoverCardClose = false;
    for (int i = 0; i < static_cast<int>(std::min<size_t>(m_items.size(), 4)); ++i)
    {
        const Rect cr = CardRect(i);
        if (cr.Contains(e.pos))
        {
            m_hoverCard = i;
            m_hoverCardClose = Rect{cr.Right() - 34.0f, cr.y + 6.0f, 28.0f, 28.0f}.Contains(e.pos);
        }
    }
    m_hoverDay = -1;
    if (!m_collapsed)
    {
        const Rect c = CalendarRect();
        const float gx = c.x + 12.0f, gy = c.y + 49.0f + 48.0f + 32.0f;
        if (e.pos.x >= gx && e.pos.x < gx + 336.0f && e.pos.y >= gy && e.pos.y < gy + 240.0f)
            m_hoverDay = static_cast<int>((e.pos.y - gy) / 40.0f) * 7 + static_cast<int>((e.pos.x - gx) / 48.0f);
    }
}

void NotificationCenter::MouseDown(const MouseEvent& e) { Flyout::MouseDown(e); }

void NotificationCenter::MouseUp(const MouseEvent& e)
{
    Flyout::MouseUp(e);
    if (e.button != MouseButton::Left)
        return;
    for (int i = 0; i < static_cast<int>(std::min<size_t>(m_items.size(), 4)); ++i)
    {
        const Rect cr = CardRect(i);
        if (!cr.Contains(e.pos))
            continue;
        const bool close = Rect{cr.Right() - 34.0f, cr.y + 6.0f, 28.0f, 28.0f}.Contains(e.pos);
        m_items.erase(m_items.begin() + i);
        m_hoverCard = -1;
        if (!close)
            Close();
        else
            Build();
        return;
    }
    if (m_hoverDay >= 0)
    {
        const int first = (DayOfWeek(m_viewYear, m_viewMonth, 1) + 6) % 7;
        const int dayNum = m_hoverDay - first + 1;
        const int dim = DaysInMonth(m_viewYear, m_viewMonth);
        if (dayNum < 1)
            ShiftMonth(-1);
        else if (dayNum > dim)
            ShiftMonth(1);
        else
        {
            m_selDay = dayNum;
            m_selMonth = m_viewMonth;
            m_selYear = m_viewYear;
        }
    }
}

bool NotificationCenter::MouseWheel(const MouseEvent& e)
{
    if (CalendarRect().Contains(e.pos) && !m_collapsed)
        ShiftMonth(e.wheel > 0.0f ? -1 : 1);
    return true;
}

void NotificationCenter::MouseLeave()
{
    Flyout::MouseLeave();
    m_hoverCard = -1;
    m_hoverDay = -1;
}

// ===========================================================================
// Скрытые значки
// ===========================================================================
TrayOverflow::TrayOverflow() = default;

Rect TrayOverflow::Bounds() const
{
    Shell& sh = GetShell();
    const Rect ch = sh.TB().ChevronRect();
    const float w = 4.0f * 40.0f + 16.0f, h = 56.0f;
    float x = ch.CenterX() - w * 0.5f;
    x = std::min(x, sh.ScreenW() - w - 8.0f);
    return {std::floor(x), sh.TB().Top() - 12.0f - h, w, h};
}

void TrayOverflow::OnOpen()
{
    m_ui.Clear();
    const Rect b = Bounds();
    struct Def
    {
        Icon icon;
        const wchar_t* tip;
        const wchar_t* page;
    };
    static const Def defs[] = {{Icon::Shield, L"Безопасность: защита включена", L"privacy"},
                               {Icon::Bluetooth, L"Устройства Bluetooth", L"bluetooth"},
                               {Icon::Cloud, L"Облачное хранилище: синхронизировано", L"accounts"},
                               {Icon::Update, L"Центр обновления: обновления не требуются", L"update"}};
    for (int i = 0; i < 4; ++i)
    {
        auto* b2 = m_ui.Add<IconButton>(defs[i].icon);
        b2->SetBounds({b.x + 8.0f + static_cast<float>(i) * 40.0f, b.y + 8.0f, 40.0f, 40.0f});
        b2->SetTooltip(defs[i].tip);
        const std::wstring page = defs[i].page;
        b2->onClick = [page]() { GetShell().Launch(AppId::Settings, page); };
    }
}

void TrayOverflow::Draw(Renderer& r)
{
    if (!BeginAnim(r, 0.0f, 20.0f))
        return;
    DrawPanel(r, Bounds());
    m_ui.Draw(r);
    EndAnim(r);
}

// ===========================================================================
// Мини-приложения
// ===========================================================================
WidgetsBoard::WidgetsBoard() = default;

Rect WidgetsBoard::Bounds() const
{
    Shell& sh = GetShell();
    const float w = std::min(820.0f, std::max(560.0f, sh.ScreenW() * 0.5f));
    return {12.0f, 12.0f, w, sh.TB().Top() - 24.0f};
}

void WidgetsBoard::OnOpen()
{
    m_scroll.Snap(0.0f);
    m_ui.Clear();
    const Rect b = Bounds();
    auto* search = m_ui.Add<TextBox>(L"Поиск в Интернете");
    search->pill = true;
    search->leadingIcon = Icon::Search;
    search->SetBounds({b.x + (b.w - 360.0f) * 0.5f, b.y + 20.0f, 360.0f, 36.0f});
    search->onEnter = [search]() {
        const std::wstring q = search->Text();
        GetShell().CloseFlyouts();
        GetShell().Launch(AppId::Browser, q);
    };
}

bool WidgetsBoard::MouseWheel(const MouseEvent& e)
{
    m_scroll.Set(Clamp(m_scroll.Target() - e.wheel * 90.0f, 0.0f, 420.0f), 0.25f);
    return true;
}

void WidgetsBoard::Draw(Renderer& r)
{
    if (!BeginAnim(r, -60.0f, 0.0f))
        return;
    const Palette& p = Theme::P();
    const Rect b = Bounds();
    DrawPanel(r, b);
    const SYSTEMTIME now = LocalNow();
    r.Text(FormatTime(now), {b.x + 24.0f, b.y + 20.0f, 120.0f, 36.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::VCenter);
    r.DrawAppIcon(AppIcon::User, {b.Right() - 24.0f - 32.0f, b.y + 22.0f, 32.0f, 32.0f});

    const float sy = m_scroll.Value();
    const Rect area{b.x + 1.0f, b.y + 72.0f, b.w - 2.0f, b.h - 73.0f};
    r.PushClip(area);
    r.PushTransform(0.0f, -sy);
    const float colW = (b.w - 24.0f * 2.0f - 16.0f) * 0.5f;
    const float x0 = b.x + 24.0f, x1 = x0 + colW + 16.0f;
    float y = area.y + 8.0f;

    // Погода
    const Rect wc{x0, y, colW, 300.0f};
    r.FillGradientV(wc, 8.0f, Color::Hex(0x2C78DC), Color::Hex(0x5FB0FA));
    r.Text(L"Погода", {wc.x + 16.0f, wc.y + 12.0f, wc.w - 32.0f, 20.0f}, Color(255, 255, 255, 0xD0), 12.0f);
    r.Text(L"Москва", {wc.x + 16.0f, wc.y + 32.0f, wc.w - 32.0f, 22.0f}, Color(255, 255, 255), 14.0f, FontWeight::Semibold);
    r.DrawAppIcon(AppIcon::Weather, {wc.x + 16.0f, wc.y + 68.0f, 64.0f, 64.0f});
    r.Text(L"17°", {wc.x + 92.0f, wc.y + 64.0f, 120.0f, 64.0f}, Color(255, 255, 255), 48.0f, FontWeight::Light);
    r.Text(L"Облачно", {wc.x + 16.0f, wc.y + 142.0f, wc.w - 32.0f, 20.0f}, Color(255, 255, 255), 14.0f);
    r.Text(L"Ощущается как 15°  •  Ветер 4 м/с  •  Влажность 72%", {wc.x + 16.0f, wc.y + 164.0f, wc.w - 32.0f, 20.0f},
           Color(255, 255, 255, 0xD0), 12.0f);
    static const wchar_t* hours[] = {L"15:00", L"16:00", L"17:00", L"18:00", L"19:00"};
    static const Icon hourIcons[] = {Icon::PartlyCloudy, Icon::Cloud, Icon::Rain, Icon::Cloud, Icon::Moon};
    static const wchar_t* temps[] = {L"17°", L"16°", L"14°", L"13°", L"12°"};
    const float hw = (wc.w - 32.0f) / 5.0f;
    for (int i = 0; i < 5; ++i)
    {
        const float hx = wc.x + 16.0f + static_cast<float>(i) * hw;
        r.Text(hours[i], {hx, wc.y + 200.0f, hw, 18.0f}, Color(255, 255, 255, 0xD0), 12.0f, FontWeight::Regular, TextFlags::Center);
        r.Glyph(hourIcons[i], {hx + hw * 0.5f - 12.0f, wc.y + 222.0f, 24.0f, 24.0f}, Color(255, 255, 255), 1.2f);
        r.Text(temps[i], {hx, wc.y + 252.0f, hw, 20.0f}, Color(255, 255, 255), 14.0f, FontWeight::Semibold, TextFlags::Center);
    }

    // Календарь
    const Rect cc{x1, y, colW, 140.0f};
    r.FillRoundRect(cc, 8.0f, p.card);
    r.StrokeRoundRect(cc, 8.0f, p.cardStroke);
    r.DrawAppIcon(AppIcon::Calendar, {cc.x + 16.0f, cc.y + 14.0f, 16.0f, 16.0f});
    r.Text(L"Календарь", {cc.x + 40.0f, cc.y + 12.0f, cc.w - 56.0f, 20.0f}, p.textSecondary, 12.0f);
    r.Text(FormatLongDate(now), {cc.x + 16.0f, cc.y + 44.0f, cc.w - 32.0f, 24.0f}, p.text, 16.0f, FontWeight::Semibold);
    r.Text(L"На сегодня событий нет. Хорошего дня!", {cc.x + 16.0f, cc.y + 76.0f, cc.w - 32.0f, 40.0f}, p.textSecondary, 12.0f,
           FontWeight::Regular, TextFlags::Wrap);

    // Фото
    const Rect pc{x1, y + 156.0f, colW, 144.0f};
    r.FillRoundRect(pc, 8.0f, p.card);
    r.StrokeRoundRect(pc, 8.0f, p.cardStroke);
    std::shared_ptr<Gdiplus::Bitmap> photo = PhotoBitmap(3);
    if (photo)
        r.ImageRounded(photo.get(), pc.Inset(8.0f, 32.0f, 8.0f, 8.0f), 6.0f);
    r.DrawAppIcon(AppIcon::Photos, {pc.x + 16.0f, pc.y + 8.0f, 16.0f, 16.0f});
    r.Text(L"Фотографии • Воспоминания", {pc.x + 40.0f, pc.y + 6.0f, pc.w - 56.0f, 20.0f}, p.textSecondary, 12.0f);

    // Лента
    y += 316.0f;
    struct News
    {
        const wchar_t* title;
        const wchar_t* source;
        uint32_t c1, c2;
        Icon icon;
    };
    static const News news[] = {
        {L"Как продлить жизнь комнатным растениям осенью: 7 простых советов", L"Дом и сад • 2 ч", 0x3BAA5C, 0x1E6B3A, Icon::Sun},
        {L"Синоптики рассказали, какой будет погода в выходные", L"Погода • 3 ч", 0x4F8DF5, 0x2B4FB8, Icon::Rain},
        {L"Пять привычек, которые помогают лучше высыпаться", L"Здоровье • 5 ч", 0x8E63E6, 0x5B37B0, Icon::Moon},
        {L"Как устроены современные интерфейсы: тени, размытие и анимации", L"Технологии • 6 ч", 0xF08A3C, 0xC2521C, Icon::Code},
        {L"Маршрут выходного дня: парки и набережные города", L"Путешествия • 8 ч", 0x18A8B8, 0x0C6E7A, Icon::Location},
        {L"Рецепт тыквенного супа за 30 минут", L"Еда • 9 ч", 0xE8B23A, 0xB07A12, Icon::Heart},
    };
    for (int i = 0; i < 6; ++i)
    {
        const float nx = (i % 2 == 0) ? x0 : x1;
        const float ny = y + static_cast<float>(i / 2) * 236.0f;
        const Rect nc{nx, ny, colW, 220.0f};
        r.FillRoundRect(nc, 8.0f, p.card);
        r.StrokeRoundRect(nc, 8.0f, p.cardStroke);
        const Rect img{nc.x, nc.y, nc.w, 120.0f};
        r.FillGradientV(img, 0.0f, Color::Hex(news[i].c1), Color::Hex(news[i].c2));
        r.FillRoundRect(img, 8.0f, 8.0f, 0.0f, 0.0f, Color(0, 0, 0, 0));
        r.Glyph(news[i].icon, {img.CenterX() - 24.0f, img.CenterY() - 24.0f, 48.0f, 48.0f}, Color(255, 255, 255, 0xE0), 1.3f);
        r.Text(news[i].source, {nc.x + 14.0f, nc.y + 128.0f, nc.w - 28.0f, 18.0f}, p.textSecondary, 12.0f);
        r.Text(news[i].title, {nc.x + 14.0f, nc.y + 148.0f, nc.w - 28.0f, 64.0f}, p.text, 14.0f, FontWeight::Semibold,
               TextFlags::Wrap);
    }
    r.PopTransform();
    r.PopClip();
    m_ui.Draw(r);
    EndAnim(r);
}

// ===========================================================================
// Всплывающие уведомления
// ===========================================================================
Rect Toasts::ToastRect(size_t index) const
{
    Shell& sh = GetShell();
    const float w = 364.0f, h = 102.0f;
    return {sh.ScreenW() - 12.0f - w, sh.TB().Top() - 12.0f - h - static_cast<float>(index) * (h + 8.0f), w, h};
}

void Toasts::Show(const Notification& n)
{
    Toast t;
    t.n = n;
    t.shown = NowSeconds();
    t.anim.Start(0.0f, 1.0f, 0.35f, Ease::OutCubic);
    m_toasts.insert(m_toasts.begin(), std::move(t));
    if (m_toasts.size() > 3)
        m_toasts.resize(3);
}

void Toasts::Draw(Renderer& r)
{
    const Palette& p = Theme::P();
    const double now = NowSeconds();
    for (size_t i = 0; i < m_toasts.size();)
    {
        Toast& t = m_toasts[i];
        if (!t.leaving && now - t.shown > 6.0 && static_cast<int>(i) != m_hover)
        {
            t.leaving = true;
            t.anim.Set(0.0f, 0.25f, Ease::OutCubic);
        }
        if (t.leaving && !t.anim.Animating())
        {
            m_toasts.erase(m_toasts.begin() + static_cast<long>(i));
            m_hover = -1;
            continue;
        }
        ++i;
    }
    if (!m_toasts.empty())
        RequestFrame(); // проверка таймера скрытия
    for (size_t i = 0; i < m_toasts.size(); ++i)
    {
        const Toast& t = m_toasts[i];
        const float a = t.anim.Value();
        const Rect rc = ToastRect(i);
        r.PushOpacity(a);
        r.PushTransform((1.0f - a) * 80.0f, 0.0f);
        r.Shadow(rc, 8.0f, 24.0f, 8.0f, p.dark ? 0.55f : 0.28f);
        r.Acrylic(rc, 8.0f, p.acrylicTint, p.acrylicAmount);
        r.StrokeRoundRect(rc, 8.0f, p.flyoutStroke);
        r.DrawAppIcon(t.n.icon, {rc.x + 14.0f, rc.y + 12.0f, 16.0f, 16.0f});
        r.Text(t.n.app, {rc.x + 38.0f, rc.y + 10.0f, rc.w - 90.0f, 20.0f}, p.textSecondary, 12.0f, FontWeight::Regular,
               TextFlags::VCenter);
        if (static_cast<int>(i) == m_hover)
        {
            const Rect xr{rc.Right() - 36.0f, rc.y + 6.0f, 28.0f, 28.0f};
            if (m_hoverClose)
                r.FillRoundRect(xr, 4.0f, p.subtleHover);
            r.Glyph(Icon::Close, {xr.CenterX() - 6.0f, xr.CenterY() - 6.0f, 12.0f, 12.0f}, p.text);
        }
        r.Text(t.n.title, {rc.x + 14.0f, rc.y + 36.0f, rc.w - 28.0f, 20.0f}, p.text, 14.0f, FontWeight::Semibold);
        r.Text(t.n.body, {rc.x + 14.0f, rc.y + 58.0f, rc.w - 28.0f, 38.0f}, p.textSecondary, 13.0f, FontWeight::Regular,
               TextFlags::Wrap);
        r.PopTransform();
        r.PopOpacity();
    }
}

bool Toasts::HitTest(Point p) const
{
    for (size_t i = 0; i < m_toasts.size(); ++i)
    {
        if (!m_toasts[i].leaving && ToastRect(i).Contains(p))
            return true;
    }
    return false;
}

void Toasts::MouseMove(const MouseEvent& e)
{
    m_hover = -1;
    m_hoverClose = false;
    for (size_t i = 0; i < m_toasts.size(); ++i)
    {
        const Rect rc = ToastRect(i);
        if (rc.Contains(e.pos))
        {
            m_hover = static_cast<int>(i);
            m_hoverClose = Rect{rc.Right() - 36.0f, rc.y + 6.0f, 28.0f, 28.0f}.Contains(e.pos);
            m_toasts[i].shown = NowSeconds();
        }
    }
}

void Toasts::MouseDown(const MouseEvent& /*e*/) {}

void Toasts::MouseUp(const MouseEvent& e)
{
    for (size_t i = 0; i < m_toasts.size(); ++i)
    {
        const Rect rc = ToastRect(i);
        if (!rc.Contains(e.pos))
            continue;
        const bool close = Rect{rc.Right() - 36.0f, rc.y + 6.0f, 28.0f, 28.0f}.Contains(e.pos);
        m_toasts[i].leaving = true;
        m_toasts[i].anim.Set(0.0f, 0.2f, Ease::OutCubic);
        if (!close)
            PostAction([]() { GetShell().ToggleNotifications(); });
        return;
    }
}

void Toasts::MouseLeave()
{
    m_hover = -1;
    m_hoverClose = false;
}
