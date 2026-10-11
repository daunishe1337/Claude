#include "Overlays.h"

#include "Taskbar.h"

// ===========================================================================
// Представление задач
// ===========================================================================
void TaskView::Open()
{
    if (m_open)
        return;
    m_open = true;
    m_pending = nullptr;
    m_hover = -1;
    m_sel = -1;
    Layout();
    m_anim.Start(m_anim.Value(), 1.0f, 0.28f, Ease::OutCubic);
}

void TaskView::Close(Window* activate)
{
    if (!m_open)
        return;
    m_open = false;
    m_pending = activate;
    Shell& sh = GetShell();
    // Обратная анимация к реальным позициям окон.
    for (Thumb& t : m_thumbs)
    {
        if (!sh.WM().Contains(t.win))
            continue;
        if (t.win == activate && t.win->IsMinimized())
            t.from = t.win->Frame();
    }
    if (activate)
        sh.WM().Activate(activate);
    m_anim.Set(0.0f, 0.22f, Ease::OutCubic);
}

void TaskView::Layout()
{
    Shell& sh = GetShell();
    m_thumbs.clear();
    std::vector<Window*> wins;
    const auto& all = sh.WM().Windows();
    for (auto it = all.rbegin(); it != all.rend(); ++it)
    {
        if (!(*it)->IsClosing() && (*it)->ShowInTaskbar())
            wins.push_back(it->get());
    }
    const Rect wa = sh.WorkArea();
    const Rect area{wa.x + 48.0f, wa.y + 40.0f, wa.w - 96.0f, wa.h - 40.0f - 200.0f};
    const int n = static_cast<int>(wins.size());
    if (n == 0)
        return;
    int bestRows = 1;
    float bestScale = 0.0f;
    for (int rows = 1; rows <= n; ++rows)
    {
        const int cols = (n + rows - 1) / rows;
        const float cellW = (area.w - static_cast<float>(cols - 1) * 28.0f) / static_cast<float>(cols);
        const float cellH = (area.h - static_cast<float>(rows - 1) * 28.0f) / static_cast<float>(rows) - 36.0f;
        float s = 0.5f;
        for (Window* w : wins)
            s = std::min(s, std::min(cellW / std::max(1.0f, w->Frame().w), cellH / std::max(1.0f, w->Frame().h)));
        if (s > bestScale)
        {
            bestScale = s;
            bestRows = rows;
        }
    }
    const int rows = bestRows, cols = (n + rows - 1) / rows;
    float maxH = 0.0f;
    for (Window* w : wins)
        maxH = std::max(maxH, w->Frame().h * bestScale);
    const float rowH = maxH + 36.0f;
    const float totalH = static_cast<float>(rows) * rowH + static_cast<float>(rows - 1) * 28.0f;
    float y = area.y + std::max(0.0f, (area.h - totalH) * 0.5f);
    int idx = 0;
    for (int rr = 0; rr < rows && idx < n; ++rr)
    {
        const int count = std::min(cols, n - idx);
        float rowW = 0.0f;
        for (int k = 0; k < count; ++k)
            rowW += wins[static_cast<size_t>(idx + k)]->Frame().w * bestScale + (k > 0 ? 28.0f : 0.0f);
        float x = area.x + (area.w - rowW) * 0.5f;
        for (int k = 0; k < count; ++k, ++idx)
        {
            Window* w = wins[static_cast<size_t>(idx)];
            Thumb t;
            t.win = w;
            const float tw = w->Frame().w * bestScale, th = w->Frame().h * bestScale;
            t.target = {std::floor(x), std::floor(y + 36.0f + (maxH - th) * 0.5f), std::floor(tw), std::floor(th)};
            if (w->IsMinimized())
            {
                const Point c = sh.TB().ButtonCenter(*w);
                t.from = {c.x - tw * 0.1f, c.y - th * 0.1f, tw * 0.2f, th * 0.2f};
            }
            else
                t.from = w->VisualFrame();
            m_thumbs.push_back(t);
            x += tw + 28.0f;
        }
        y += rowH + 28.0f;
    }
}

Rect TaskView::CurrentRect(const Thumb& t) const { return LerpRect(t.from, t.target, m_anim.Value()); }

Rect TaskView::DesktopCard(int index) const
{
    Shell& sh = GetShell();
    const Rect wa = sh.WorkArea();
    const float cw = 196.0f, ch = 150.0f;
    const int count = m_desktops + 1;
    const float total = static_cast<float>(count) * cw + static_cast<float>(count - 1) * 16.0f;
    const float x0 = wa.x + (wa.w - total) * 0.5f;
    return {x0 + static_cast<float>(index) * (cw + 16.0f), wa.Bottom() - ch - 24.0f, cw, ch};
}

int TaskView::ThumbAt(Point p, bool* onClose) const
{
    for (int i = 0; i < static_cast<int>(m_thumbs.size()); ++i)
    {
        const Rect tr = m_thumbs[static_cast<size_t>(i)].target;
        const Rect item{tr.x - 6.0f, tr.y - 36.0f, tr.w + 12.0f, tr.h + 42.0f};
        if (item.Contains(p))
        {
            if (onClose)
                *onClose = Rect{tr.Right() - 30.0f, tr.y - 33.0f, 30.0f, 30.0f}.Contains(p);
            return i;
        }
    }
    return -1;
}

bool TaskView::HitTest(Point p) const { return m_open && p.y < GetShell().TB().Top(); }

void TaskView::Draw(Renderer& r)
{
    const float t = m_anim.Value();
    if (t <= 0.002f)
    {
        m_thumbs.clear();
        return;
    }
    Shell& sh = GetShell();
    const Palette& p = Theme::P();
    const Rect screen{0.0f, 0.0f, sh.ScreenW(), sh.TB().Top()};
    r.PushOpacity(t);
    r.Acrylic(screen, 0.0f, p.dark ? Color::Hex(0x1F1F1F) : Color::Hex(0xE8E8E8), 0.55f, 60.0f);
    r.PopOpacity();

    // Миниатюры окон.
    for (int i = 0; i < static_cast<int>(m_thumbs.size()); ++i)
    {
        const Thumb& th = m_thumbs[static_cast<size_t>(i)];
        if (!sh.WM().Contains(th.win) || th.win->IsClosing())
            continue;
        const Rect rc = CurrentRect(th);
        const bool hot = i == m_hover || i == m_sel;
        if (hot && m_open)
        {
            const Rect item{rc.x - 6.0f, rc.y - 36.0f, rc.w + 12.0f, rc.h + 42.0f};
            r.FillRoundRect(item, 8.0f, p.dark ? Color(255, 255, 255, 0x14) : Color(255, 255, 255, 0x80));
            r.StrokeRoundRect(item, 8.0f, p.dark ? Color(255, 255, 255, 0x30) : Color(0, 0, 0, 0x22));
        }
        r.PushOpacity(t);
        r.DrawAppIcon(th.win->WindowIcon(), {rc.x + 2.0f, rc.y - 26.0f, 16.0f, 16.0f});
        r.Text(th.win->Title(), {rc.x + 26.0f, rc.y - 34.0f, rc.w - 60.0f, 32.0f}, p.text, 12.0f, FontWeight::Regular,
               TextFlags::VCenter);
        if (hot && m_open)
        {
            const Rect cb{rc.Right() - 30.0f, rc.y - 33.0f, 30.0f, 30.0f};
            if (m_hoverClose && i == m_hover)
                r.FillRoundRect(cb, 4.0f, p.closeHover);
            r.Glyph(Icon::Close, {cb.CenterX() - 6.0f, cb.CenterY() - 6.0f, 12.0f, 12.0f},
                    m_hoverClose && i == m_hover ? Color(255, 255, 255) : p.text);
        }
        r.PopOpacity();
        sh.WM().DrawThumbnail(r, *th.win, rc);
    }
    if (m_thumbs.empty() && m_open)
        r.Text(L"Нет открытых окон", {0.0f, screen.h * 0.35f, screen.w, 40.0f}, p.textSecondary, 20.0f, FontWeight::Regular,
               TextFlags::Center);

    // Рабочие столы.
    r.PushOpacity(t);
    r.PushTransform(0.0f, (1.0f - t) * 40.0f);
    for (int i = 0; i <= m_desktops; ++i)
    {
        const Rect card = DesktopCard(i);
        const bool hot = i == m_hoverDesktop;
        r.FillRoundRect(card, 8.0f, hot ? (p.dark ? Color(255, 255, 255, 0x18) : Color(255, 255, 255, 0xB0))
                                        : (p.dark ? Color(255, 255, 255, 0x0C) : Color(255, 255, 255, 0x70)));
        r.StrokeRoundRect(card, 8.0f, i == 0 ? p.accent : p.cardStroke, i == 0 ? 2.0f : 1.0f);
        const Rect thumb{card.x + 12.0f, card.y + 12.0f, card.w - 24.0f, (card.w - 24.0f) * 9.0f / 16.0f};
        if (i < m_desktops)
        {
            r.DrawWallpaperThumb(thumb, 4.0f);
            r.Text(L"Рабочий стол " + std::to_wstring(i + 1), {card.x + 12.0f, thumb.Bottom() + 8.0f, card.w - 24.0f, 22.0f},
                   p.text, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        }
        else
        {
            r.FillRoundRect(thumb, 4.0f, p.dark ? Color(255, 255, 255, 0x0A) : Color(0, 0, 0, 0x06));
            r.Glyph(Icon::Add, {thumb.CenterX() - 12.0f, thumb.CenterY() - 12.0f, 24.0f, 24.0f}, p.text);
            r.Text(L"Создать рабочий стол", {card.x + 12.0f, thumb.Bottom() + 8.0f, card.w - 24.0f, 22.0f}, p.text, 12.0f,
                   FontWeight::Regular, TextFlags::VCenter);
        }
    }
    r.PopTransform();
    r.PopOpacity();
}

void TaskView::MouseMove(const MouseEvent& e)
{
    bool onClose = false;
    m_hover = ThumbAt(e.pos, &onClose);
    m_hoverClose = onClose;
    m_hoverDesktop = -1;
    for (int i = 0; i <= m_desktops; ++i)
    {
        if (DesktopCard(i).Contains(e.pos))
            m_hoverDesktop = i;
    }
}

void TaskView::MouseDown(const MouseEvent& /*e*/) {}

void TaskView::MouseUp(const MouseEvent& e)
{
    if (e.button != MouseButton::Left)
        return;
    bool onClose = false;
    const int idx = ThumbAt(e.pos, &onClose);
    if (idx >= 0)
    {
        Window* w = m_thumbs[static_cast<size_t>(idx)].win;
        if (onClose)
        {
            w->Close();
            PostAction([this]() {
                if (m_open)
                    Layout();
            });
            return;
        }
        Close(w);
        return;
    }
    for (int i = 0; i <= m_desktops; ++i)
    {
        if (!DesktopCard(i).Contains(e.pos))
            continue;
        if (i == m_desktops)
        {
            if (m_desktops < 4)
                ++m_desktops;
            GetShell().Notify(AppIcon::Settings, L"Рабочие столы", L"Новый рабочий стол создан",
                              L"Виртуальные рабочие столы в этой демонстрации отображаются только в представлении задач.");
            return;
        }
        Close(nullptr);
        return;
    }
    Close(nullptr);
}

bool TaskView::KeyDown(UINT vk, const KeyMods& /*mods*/)
{
    const int n = static_cast<int>(m_thumbs.size());
    switch (vk)
    {
    case VK_ESCAPE:
        Close(nullptr);
        return true;
    case VK_RIGHT:
    case VK_TAB:
        if (n > 0)
            m_sel = (m_sel + 1) % n;
        return true;
    case VK_LEFT:
        if (n > 0)
            m_sel = (m_sel - 1 + n) % n;
        return true;
    case VK_RETURN:
        if (m_sel >= 0 && m_sel < n)
            Close(m_thumbs[static_cast<size_t>(m_sel)].win);
        else
            Close(nullptr);
        return true;
    case VK_DELETE:
        if (m_sel >= 0 && m_sel < n)
        {
            m_thumbs[static_cast<size_t>(m_sel)].win->Close();
            PostAction([this]() {
                if (m_open)
                    Layout();
            });
        }
        return true;
    default:
        return true;
    }
}

void TaskView::MouseLeave()
{
    m_hover = -1;
    m_hoverDesktop = -1;
}

// ===========================================================================
// Экраны системы
// ===========================================================================
void SystemScreens::SetMode(Mode m)
{
    m_mode = m;
    m_modeStart = NowSeconds();
    m_ui.Clear();
    if (m == Mode::SignIn)
        BuildSignIn();
}

void SystemScreens::Lock()
{
    Shell& sh = GetShell();
    sh.CloseFlyouts();
    SetMode(Mode::Lock);
    m_slide.Snap(0.0f);
    m_fade.Start(0.0f, 1.0f, 0.3f, Ease::OutCubic);
}

void SystemScreens::Sleep()
{
    GetShell().CloseFlyouts();
    SetMode(Mode::Sleep);
    m_fade.Start(0.0f, 1.0f, 0.6f, Ease::InOutCubic);
}

void SystemScreens::Shutdown(bool restart)
{
    Shell& sh = GetShell();
    sh.CloseFlyouts();
    sh.WM().CloseAll();
    SetMode(restart ? Mode::Restarting : Mode::ShuttingDown);
    m_fade.Start(0.0f, 1.0f, 0.4f, Ease::OutCubic);
}

void SystemScreens::BuildSignIn()
{
    Shell& sh = GetShell();
    const float W = sh.ScreenW(), H = sh.ScreenH();
    class SignInButton : public Button
    {
    public:
        SignInButton() : Button(L"Войти") {}
        void Draw(Renderer& r) override
        {
            const Rect& b = m_bounds;
            const bool down = m_pressed && m_hovered;
            r.FillRoundRect(b, 4.0f, down ? Color(255, 255, 255, 0x22) : (m_hovered ? Color(255, 255, 255, 0x3C) : Color(255, 255, 255, 0x2C)));
            r.StrokeRoundRect(b, 4.0f, Color(255, 255, 255, 0x40));
            r.Text(text, b, Color(255, 255, 255), 14.0f, FontWeight::Regular, TextFlags::Middle);
        }
    };
    auto* btn = m_ui.Add<SignInButton>();
    btn->SetBounds({W * 0.5f - 60.0f, H * 0.3f + 192.0f + 90.0f, 120.0f, 34.0f});
    btn->onClick = [this]() { SignIn(); };
}

void SystemScreens::SignIn()
{
    SetMode(Mode::Welcome);
}

void SystemScreens::DrawLock(Renderer& r, float offsetY)
{
    Shell& sh = GetShell();
    const float W = sh.ScreenW(), H = sh.ScreenH();
    r.PushTransform(0.0f, offsetY);
    if (r.Opacity() >= 0.999f && offsetY == 0.0f)
        r.DrawWallpaper();
    else if (r.WallpaperThumb())
        r.Image(r.WallpaperThumb(), {0.0f, 0.0f, W, H});
    r.FillGradientV({0.0f, 0.0f, W, H * 0.5f}, 0.0f, Color(0, 0, 0, 0x50), Color(0, 0, 0, 0));
    const SYSTEMTIME now = LocalNow();
    const Rect tr{0.0f, H * 0.12f, W, 150.0f};
    r.Text(FormatTime(now), tr.Translated(0.0f, 2.0f), Color(0, 0, 0, 0x40), 120.0f, FontWeight::Semibold, TextFlags::Center);
    r.Text(FormatTime(now), tr, Color(255, 255, 255), 120.0f, FontWeight::Semibold, TextFlags::Center);
    const Rect dr{0.0f, H * 0.12f + 150.0f, W, 36.0f};
    std::wstring date = FormatLongDate(now);
    if (!date.empty())
        date[0] = static_cast<wchar_t>(towupper(date[0]));
    r.Text(date, dr.Translated(0.0f, 1.0f), Color(0, 0, 0, 0x40), 22.0f, FontWeight::Regular, TextFlags::Center);
    r.Text(date, dr, Color(255, 255, 255), 22.0f, FontWeight::Regular, TextFlags::Center);
    const ShellSettings& st = sh.Settings();
    r.WifiIcon({W - 92.0f, H - 52.0f, 20.0f, 20.0f}, Color(255, 255, 255), 3, !st.wifi);
    r.BatteryIcon({W - 56.0f, H - 52.0f, 20.0f, 20.0f}, Color(255, 255, 255), st.battery, st.charging);
    r.PopTransform();
}

void SystemScreens::DrawSignIn(Renderer& r, float alpha)
{
    Shell& sh = GetShell();
    const float W = sh.ScreenW(), H = sh.ScreenH();
    if (alpha >= 0.999f)
        r.DrawWallpaper();
    r.PushOpacity(alpha);
    r.Acrylic({0.0f, 0.0f, W, H}, 0.0f, Color::Hex(0x1A1A1A), 0.35f, 90.0f);
    r.PopOpacity();
}

void SystemScreens::Draw(Renderer& r)
{
    if (!Active())
        return;
    Shell& sh = GetShell();
    const float W = sh.ScreenW(), H = sh.ScreenH();
    const double elapsed = NowSeconds() - m_modeStart;
    const float a = m_fade.Value();
    const Color white(255, 255, 255);
    switch (m_mode)
    {
    case Mode::None:
        // Плавный выход после приветствия: размытие исчезает.
        r.PushOpacity(a);
        r.Acrylic({0.0f, 0.0f, W, H}, 0.0f, Color::Hex(0x1A1A1A), 0.35f, 90.0f);
        r.PopOpacity();
        break;
    case Mode::Lock:
    {
        const float s = m_slide.Value();
        r.PushOpacity(a);
        if (s > 0.0f)
            DrawSignIn(r, 1.0f);
        DrawLock(r, -H * s);
        r.PopOpacity();
        if (s >= 0.999f && !m_slide.Animating())
            SetMode(Mode::SignIn);
        break;
    }
    case Mode::SignIn:
    {
        DrawSignIn(r, 1.0f);
        const float t = Saturate(static_cast<float>(elapsed) / 0.3f);
        r.PushOpacity(t);
        const float cx = W * 0.5f, y = H * 0.3f;
        r.DrawAppIcon(AppIcon::User, {cx - 96.0f, y, 192.0f, 192.0f});
        r.Text(sh.FS().UserName(), {0.0f, y + 204.0f, W, 40.0f}, white, 28.0f, FontWeight::Semibold, TextFlags::Center);
        m_ui.Draw(r);
        r.DrawAppIcon(AppIcon::User, {24.0f, H - 72.0f, 48.0f, 48.0f});
        r.Text(sh.FS().UserName(), {82.0f, H - 72.0f, 300.0f, 48.0f}, white, 14.0f, FontWeight::Regular, TextFlags::VCenter);
        r.WifiIcon({W - 132.0f, H - 52.0f, 20.0f, 20.0f}, white, 3, false);
        r.Glyph(Icon::Accessibility, {W - 92.0f, H - 52.0f, 20.0f, 20.0f}, white);
        r.Glyph(Icon::Power, {W - 52.0f, H - 52.0f, 20.0f, 20.0f}, white);
        r.PopOpacity();
        break;
    }
    case Mode::Welcome:
    {
        DrawSignIn(r, 1.0f);
        const float cx = W * 0.5f, y = H * 0.3f;
        r.DrawAppIcon(AppIcon::User, {cx - 96.0f, y, 192.0f, 192.0f});
        DrawSpinner(r, cx - 100.0f, y + 228.0f, 11.0f, 2.5f, white);
        r.Text(L"Добро пожаловать", {cx - 80.0f, y + 210.0f, 300.0f, 36.0f}, white, 24.0f, FontWeight::Regular, TextFlags::VCenter);
        if (elapsed > 1.6)
        {
            m_mode = Mode::None;
            m_fade.Start(1.0f, 0.0f, 0.45f, Ease::OutCubic);
        }
        RequestFrame();
        break;
    }
    case Mode::ShuttingDown:
    case Mode::Restarting:
    {
        r.PushOpacity(a);
        r.FillRect({0.0f, 0.0f, W, H}, Color::Hex(0x0A1A33));
        r.FillGradientV({0.0f, 0.0f, W, H}, 0.0f, Color::Hex(0x0B2A55), Color::Hex(0x061226));
        const float cy = H * 0.5f;
        DrawSpinner(r, W * 0.5f - 110.0f, cy, 12.0f, 2.5f, white);
        r.Text(m_mode == Mode::Restarting ? L"Перезагрузка" : L"Завершение работы", {W * 0.5f - 88.0f, cy - 20.0f, 320.0f, 40.0f}, white,
               24.0f, FontWeight::Regular, TextFlags::VCenter);
        r.PopOpacity();
        if (elapsed > 2.6)
        {
            if (m_mode == Mode::ShuttingDown)
            {
                PostMessageW(sh.Hwnd(), WM_CLOSE, 0, 0);
                m_modeStart = NowSeconds() + 100.0;
            }
            else
            {
                SetMode(Mode::Booting);
            }
        }
        RequestFrame();
        break;
    }
    case Mode::Booting:
        r.FillRect({0.0f, 0.0f, W, H}, Color(0, 0, 0));
        if (elapsed > 0.6)
            DrawSpinner(r, W * 0.5f, H * 0.72f, 14.0f, 2.5f, white);
        if (elapsed > 2.4)
            Lock();
        RequestFrame();
        break;
    case Mode::Sleep:
        r.FillRect({0.0f, 0.0f, W, H}, Color(0, 0, 0, static_cast<uint8_t>(255.0f * a)));
        break;
    }
}

void SystemScreens::MouseMove(const MouseEvent& e)
{
    if (m_mode == Mode::SignIn)
        m_ui.OnMouseMove(e);
}

void SystemScreens::MouseDown(const MouseEvent& e)
{
    switch (m_mode)
    {
    case Mode::Lock:
        m_slide.Set(1.0f, 0.4f, Ease::OutCubic);
        break;
    case Mode::SignIn:
    {
        Widget::Origin() = {0.0f, 0.0f};
        m_ui.OnMouseDown(e);
        const Shell& sh = GetShell();
        const Rect power{sh.ScreenW() - 62.0f, sh.ScreenH() - 62.0f, 40.0f, 40.0f};
        if (power.Contains(e.pos))
        {
            std::vector<MenuItem> items;
            items.push_back(MenuItem(L"Спящий режим", Icon::Sleep, []() { GetShell().Sleep(); }));
            items.push_back(MenuItem(L"Завершение работы", Icon::Power, []() { GetShell().Shutdown(false); }));
            items.push_back(MenuItem(L"Перезагрузка", Icon::Restart, []() { GetShell().Shutdown(true); }));
            GetShell().ShowMenuBelow(power, std::move(items), 200.0f, true);
        }
        break;
    }
    case Mode::Sleep:
        Lock();
        break;
    default:
        break;
    }
}

void SystemScreens::MouseUp(const MouseEvent& e)
{
    if (m_mode == Mode::SignIn)
        m_ui.OnMouseUp(e);
}

bool SystemScreens::KeyDown(UINT vk, const KeyMods& /*mods*/)
{
    switch (m_mode)
    {
    case Mode::Lock:
        m_slide.Set(1.0f, 0.4f, Ease::OutCubic);
        return true;
    case Mode::SignIn:
        if (vk == VK_RETURN || vk == VK_SPACE)
            SignIn();
        else if (vk == VK_ESCAPE)
        {
            SetMode(Mode::Lock);
            m_slide.Start(1.0f, 0.0f, 0.35f, Ease::OutCubic);
        }
        return true;
    case Mode::Sleep:
        Lock();
        return true;
    default:
        return true;
    }
}

bool SystemScreens::Char(wchar_t /*ch*/) { return true; }

CursorType SystemScreens::Cursor(Point /*p*/) const { return CursorType::Arrow; }
