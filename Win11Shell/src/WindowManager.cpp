#include "WindowManager.h"

#include "Shell.h"
#include "Taskbar.h"

namespace
{
unsigned g_nextWindowId = 1;
constexpr float kCaptionW = 46.0f;
} // namespace

// ===========================================================================
// Window
// ===========================================================================
Window::Window(AppId app, std::wstring title) : m_app(app), m_title(std::move(title)), m_id(g_nextWindowId++)
{
    m_frame = {0.0f, 0.0f, 900.0f, 600.0f};
    m_restoreFrame = m_frame;
}

Window::~Window() = default;

AppIcon Window::WindowIcon() const { return GetAppInfo(m_app).icon; }

Rect Window::VisualFrame() const
{
    if (m_morph.Animating())
        return LerpRect(m_morphFrom, m_frame, m_morph.Value());
    return m_frame;
}

void Window::SetInitialSize(float w, float h)
{
    m_frame.w = w;
    m_frame.h = h;
    m_restoreFrame = m_frame;
}

void Window::UpdateLayoutSize(float w, float h)
{
    w = std::floor(w + 0.5f);
    h = std::floor(h + 0.5f);
    if (w == m_layoutW && h == m_layoutH)
        return;
    m_layoutW = w;
    m_layoutH = h;
    m_ui.SetBounds({0.0f, 0.0f, w, h});
    OnResize();
    if (m_dialogActive)
        LayoutDialog();
}

bool Window::IsDragArea(Point local) const
{
    if (local.y >= TitleBarHeight() || local.x > m_layoutW - kCaptionW * 3.0f)
        return false;
    for (auto& c : m_ui.Children())
    {
        if (c->Visible() && c->HitTest(local))
            return false;
    }
    return true;
}

void Window::OnMouseDown(const MouseEvent& e)
{
    if (m_dialogActive)
    {
        m_dialogUi.OnMouseDown(e);
        return;
    }
    m_ui.OnMouseDown(e);
}

void Window::OnMouseMove(const MouseEvent& e)
{
    if (m_dialogActive)
    {
        m_dialogUi.OnMouseMove(e);
        return;
    }
    m_ui.OnMouseMove(e);
    if (!m_ui.HasCapture())
    {
        Widget* leaf = m_ui.HoverLeaf();
        if (leaf && !leaf->Tooltip().empty())
            GetShell().Tooltip(leaf, leaf->Tooltip(), LocalToScreen(leaf->Bounds()), false);
    }
}

void Window::OnMouseUp(const MouseEvent& e)
{
    if (m_dialogActive)
    {
        m_dialogUi.OnMouseUp(e);
        return;
    }
    m_ui.OnMouseUp(e);
}

bool Window::OnMouseWheel(const MouseEvent& e)
{
    if (m_dialogActive)
        return true;
    return m_ui.OnMouseWheel(e);
}

void Window::OnMouseLeave()
{
    m_ui.OnMouseLeave();
    m_dialogUi.OnMouseLeave();
}

bool Window::OnKeyDown(UINT vk, const KeyMods& mods) { return m_ui.OnKeyDown(vk, mods); }

bool Window::OnChar(wchar_t ch) { return m_ui.OnChar(ch); }

CursorType Window::Cursor(Point local) const
{
    if (m_dialogActive)
        return CursorType::Arrow;
    return m_ui.Cursor(local);
}

void Window::Close()
{
    if (m_closing)
        return;
    if (m_dialogActive)
    {
        Activate();
        return;
    }
    if (OnClose())
        ForceClose();
}

void Window::ForceClose() { GetShell().WM().ForceClose(this); }

void Window::Activate() { GetShell().WM().Activate(this); }

void Window::ShowDialog(std::wstring title, std::wstring message, std::vector<DialogButton> buttons,
                        std::function<void(int)> onResult)
{
    m_dialogActive = true;
    m_dialogTitle = std::move(title);
    m_dialogText = std::move(message);
    m_dialogResult = std::move(onResult);
    m_dialogUi.Clear();
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        auto* b = m_dialogUi.Add<Button>(buttons[i].text, Icon::None,
                                         buttons[i].accent ? ButtonStyle::Accent : ButtonStyle::Standard);
        const int index = static_cast<int>(i);
        b->onClick = [this, index]() {
            m_dialogActive = false;
            m_dialogUi.Clear();
            auto cb = m_dialogResult;
            m_dialogResult = nullptr;
            if (cb)
                cb(index);
        };
    }
    m_dialogAnim.Start(0.0f, 1.0f, 0.2f, Ease::OutCubic);
    LayoutDialog();
}

void Window::LayoutDialog()
{
    Renderer& r = Renderer::Get();
    const Rect client = ClientRect();
    const float w = std::min(460.0f, client.w - 40.0f);
    const float textH = r.TextHeight(m_dialogText, 14.0f, FontWeight::Regular, w - 48.0f);
    const float h = 24.0f + 28.0f + 12.0f + textH + 24.0f + 80.0f;
    m_dialogRect = {std::floor(client.x + (client.w - w) * 0.5f), std::floor(client.y + (client.h - h) * 0.5f), w, h};
    const auto& kids = m_dialogUi.Children();
    const float n = static_cast<float>(kids.size());
    if (n <= 0.0f)
        return;
    const float bw = (w - 48.0f - 8.0f * (n - 1.0f)) / n;
    float x = m_dialogRect.x + 24.0f;
    for (auto& k : kids)
    {
        k->SetBounds({x, m_dialogRect.Bottom() - 56.0f, bw, 32.0f});
        x += bw + 8.0f;
    }
}

void Window::DrawDialog(Renderer& r)
{
    if (!m_dialogActive)
        return;
    const Palette& p = Theme::P();
    const float t = m_dialogAnim.Value();
    const Rect client = ClientRect();
    r.FillRect(client, p.smoke.MulAlpha(t));
    r.PushOpacity(t);
    const float s = 1.05f - 0.05f * t;
    const Point c = m_dialogRect.Center();
    r.PushTransform(c.x - c.x * s, c.y - c.y * s, s);
    const Rect& d = m_dialogRect;
    r.Shadow(d, 8.0f, 28.0f, 10.0f, p.dark ? 0.5f : 0.3f);
    r.FillRoundRect(d, 8.0f, p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xFFFFFF));
    r.FillRoundRect({d.x, d.Bottom() - 80.0f, d.w, 80.0f}, 0.0f, 0.0f, 8.0f, 8.0f,
                    p.dark ? Color::Hex(0x202020) : Color::Hex(0xF3F3F3));
    r.FillRect({d.x, d.Bottom() - 80.0f, d.w, 1.0f}, p.divider);
    r.StrokeRoundRect(d, 8.0f, p.surfaceStroke);
    r.Text(m_dialogTitle, {d.x + 24.0f, d.y + 22.0f, d.w - 48.0f, 30.0f}, p.text, 20.0f, FontWeight::Semibold);
    r.Text(m_dialogText, {d.x + 24.0f, d.y + 64.0f, d.w - 48.0f, d.h - 64.0f - 96.0f}, p.text, 14.0f,
           FontWeight::Regular, TextFlags::Wrap | TextFlags::NoEllipsis);
    m_dialogUi.Draw(r);
    r.PopTransform();
    r.PopOpacity();
}

// ===========================================================================
// WindowManager
// ===========================================================================
Window* WindowManager::Add(std::unique_ptr<Window> w)
{
    Window* raw = w.get();
    const Rect wa = GetShell().WorkArea();
    Rect f = raw->m_frame;
    f.w = std::min(f.w, wa.w - 24.0f);
    f.h = std::min(f.h, wa.h - 24.0f);
    const float off = static_cast<float>(m_cascade % 7) * 26.0f;
    ++m_cascade;
    f.x = std::floor(wa.x + (wa.w - f.w) * 0.5f - 78.0f + off);
    f.y = std::floor(wa.y + (wa.h - f.h) * 0.4f - 52.0f + off);
    f.x = Clamp(f.x, wa.x, std::max(wa.x, wa.Right() - f.w));
    f.y = Clamp(f.y, wa.y, std::max(wa.y, wa.Bottom() - f.h));
    raw->m_frame = f;
    raw->m_restoreFrame = f;
    raw->UpdateLayoutSize(f.w, f.h);
    raw->m_open.Start(0.0f, 1.0f, 0.24f, Ease::OutCubic);
    m_windows.push_back(std::move(w));
    Activate(raw);
    return raw;
}

bool WindowManager::Contains(const Window* w) const
{
    for (auto& p : m_windows)
    {
        if (p.get() == w)
            return true;
    }
    return false;
}

void WindowManager::Activate(Window* w)
{
    if (!w || !Contains(w) || w->m_closing)
        return;
    if (w->IsMinimized())
    {
        w->m_state = w->m_stateBeforeMin;
        w->m_min.Set(0.0f, 0.25f, Ease::OutCubic);
    }
    // Поднять наверх.
    auto it = std::find_if(m_windows.begin(), m_windows.end(), [w](const std::unique_ptr<Window>& p) { return p.get() == w; });
    if (it != m_windows.end() && it + 1 != m_windows.end())
    {
        std::unique_ptr<Window> tmp = std::move(*it);
        m_windows.erase(it);
        m_windows.push_back(std::move(tmp));
    }
    if (m_active == w)
        return;
    if (m_active)
    {
        m_active->m_active = false;
        m_active->m_ui.SetFocus(nullptr);
        m_active->OnActivate(false);
    }
    m_active = w;
    w->m_active = true;
    w->OnActivate(true);
}

void WindowManager::Deactivate()
{
    if (m_active)
    {
        m_active->m_active = false;
        m_active->OnActivate(false);
        m_active = nullptr;
    }
}

void WindowManager::ActivateTopmost()
{
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it)
    {
        Window* w = it->get();
        if (!w->IsMinimized() && !w->m_closing)
        {
            Activate(w);
            return;
        }
    }
    m_active = nullptr;
}

void WindowManager::Minimize(Window* w)
{
    if (!w || w->IsMinimized() || w->m_closing)
        return;
    w->m_stateBeforeMin = w->m_state;
    w->m_state = WindowState::Minimized;
    w->m_min.Set(1.0f, 0.26f, Ease::OutCubic);
    if (w == m_active)
    {
        w->m_active = false;
        w->OnActivate(false);
        m_active = nullptr;
        ActivateTopmost();
    }
}

void WindowManager::Restore(Window* w)
{
    if (!w)
        return;
    if (w->IsMinimized())
    {
        Activate(w);
        return;
    }
    if (w->IsMaximized())
    {
        w->m_state = WindowState::Normal;
        BeginMorph(w, w->m_restoreFrame);
    }
    else if (w->m_snapped)
    {
        w->m_snapped = false;
        BeginMorph(w, w->m_restoreFrame);
    }
    Activate(w);
}

void WindowManager::Maximize(Window* w)
{
    if (!w || !w->m_resizable || w->IsMaximized())
        return;
    if (!w->m_snapped && w->m_state == WindowState::Normal)
        w->m_restoreFrame = w->m_frame;
    w->m_snapped = false;
    w->m_state = WindowState::Maximized;
    BeginMorph(w, GetShell().WorkArea());
    Activate(w);
}

void WindowManager::ToggleMaximize(Window* w)
{
    if (!w || !w->m_resizable)
        return;
    if (w->IsMaximized())
        Restore(w);
    else
        Maximize(w);
}

void WindowManager::SnapTo(Window* w, SnapZone z)
{
    if (!w || z == SnapZone::None)
        return;
    if (z == SnapZone::Top)
    {
        Maximize(w);
        return;
    }
    SnapToRect(w, ZoneRect(z));
}

void WindowManager::SnapToRect(Window* w, const Rect& r)
{
    if (!w || !w->m_resizable)
        return;
    if (!w->m_snapped && w->m_state == WindowState::Normal)
        w->m_restoreFrame = w->m_frame;
    w->m_state = WindowState::Normal;
    w->m_snapped = true;
    BeginMorph(w, r);
    Activate(w);
}

void WindowManager::BeginMorph(Window* w, const Rect& target)
{
    w->m_morphFrom = w->VisualFrame();
    w->m_frame = target;
    w->m_morph.Start(0.0f, 1.0f, 0.24f, Ease::OutCubic);
}

void WindowManager::Close(Window* w)
{
    if (w)
        w->Close();
}

void WindowManager::ForceClose(Window* w)
{
    if (!w || w->m_closing || !Contains(w))
        return;
    w->m_closing = true;
    w->m_open.Set(0.0f, 0.16f, Ease::OutCubic);
    if (m_drag.win == w)
        m_drag = Drag{};
    if (m_snapWin == w)
    {
        m_snapWin = nullptr;
        m_snapVisible = false;
        m_maxHovering = false;
    }
    if (w == m_active)
    {
        w->m_active = false;
        m_active = nullptr;
        ActivateTopmost();
    }
}

void WindowManager::CloseAll()
{
    for (auto& w : m_windows)
        ForceClose(w.get());
}

void WindowManager::MinimizeAll()
{
    for (auto& w : m_windows)
    {
        if (!w->m_closing)
            Minimize(w.get());
    }
}

void WindowManager::ShowDesktop()
{
    std::vector<Window*> visible;
    for (auto& w : m_windows)
    {
        if (!w->IsMinimized() && !w->m_closing)
            visible.push_back(w.get());
    }
    if (!visible.empty())
    {
        m_shownBeforeDesktop = visible;
        for (Window* w : visible)
            Minimize(w);
    }
    else
    {
        for (Window* w : m_shownBeforeDesktop)
        {
            if (Contains(w) && w->IsMinimized())
                Activate(w);
        }
        m_shownBeforeDesktop.clear();
    }
}

void WindowManager::SnapActive(UINT vk)
{
    Window* w = m_active;
    if (!w)
        return;
    switch (vk)
    {
    case VK_UP:
        Maximize(w);
        break;
    case VK_DOWN:
        if (w->IsMaximized() || w->m_snapped)
            Restore(w);
        else
            Minimize(w);
        break;
    case VK_LEFT:
        SnapTo(w, SnapZone::Left);
        break;
    case VK_RIGHT:
        SnapTo(w, SnapZone::Right);
        break;
    default:
        break;
    }
}

std::vector<Window*> WindowManager::WindowsOf(AppId app) const
{
    std::vector<Window*> out;
    for (auto& w : m_windows)
    {
        if (w->App() == app && !w->m_closing && w->ShowInTaskbar())
            out.push_back(w.get());
    }
    return out;
}

Window* WindowManager::FindApp(AppId app) const
{
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it)
    {
        if ((*it)->App() == app && !(*it)->m_closing)
            return it->get();
    }
    return nullptr;
}

bool WindowManager::AnyVisible() const
{
    for (auto& w : m_windows)
    {
        if (!w->IsMinimized() && !w->m_closing)
            return true;
    }
    return false;
}

Rect WindowManager::ZoneRect(SnapZone z) const
{
    const Rect wa = GetShell().WorkArea();
    const float hw = std::floor(wa.w * 0.5f), hh = std::floor(wa.h * 0.5f);
    switch (z)
    {
    case SnapZone::Left:
        return {wa.x, wa.y, hw, wa.h};
    case SnapZone::Right:
        return {wa.x + hw, wa.y, wa.w - hw, wa.h};
    case SnapZone::Top:
        return wa;
    case SnapZone::TopLeft:
        return {wa.x, wa.y, hw, hh};
    case SnapZone::TopRight:
        return {wa.x + hw, wa.y, wa.w - hw, hh};
    case SnapZone::BottomLeft:
        return {wa.x, wa.y + hh, hw, wa.h - hh};
    case SnapZone::BottomRight:
        return {wa.x + hw, wa.y + hh, wa.w - hw, wa.h - hh};
    case SnapZone::None:
        break;
    }
    return wa;
}

void WindowManager::Update()
{
    for (auto it = m_windows.begin(); it != m_windows.end();)
    {
        Window* w = it->get();
        if (w->m_closing && !w->m_open.Animating())
        {
            if (m_hoverWin == w)
                m_hoverWin = nullptr;
            if (m_drag.win == w)
                m_drag = Drag{};
            if (m_snapWin == w)
                m_snapWin = nullptr;
            m_shownBeforeDesktop.erase(std::remove(m_shownBeforeDesktop.begin(), m_shownBeforeDesktop.end(), w),
                                       m_shownBeforeDesktop.end());
            it = m_windows.erase(it);
        }
        else
            ++it;
    }
}

// ---------------------------------------------------------------------------
// Отрисовка
// ---------------------------------------------------------------------------
Rect WindowManager::CaptionButtonRect(const Window& w, int index) const
{
    const float th = std::min(w.TitleBarHeight(), 48.0f);
    return {w.m_layoutW - kCaptionW * static_cast<float>(3 - index), 0.0f, kCaptionW, th};
}

Point WindowManager::MinimizeTarget(const Window& w) const { return GetShell().TB().ButtonCenter(w); }

void WindowManager::Draw(Renderer& r)
{
    for (auto& w : m_windows)
    {
        if (w->m_closing && !w->m_open.Animating())
            continue;
        if (w->IsMinimized() && !w->m_min.Animating())
            continue;
        DrawWindow(r, *w);
    }
}

void WindowManager::DrawWindow(Renderer& r, Window& w, bool thumbnail)
{
    const Palette& p = Theme::P();
    const Rect f = thumbnail ? w.m_frame : w.VisualFrame();
    w.UpdateLayoutSize(f.w, f.h);

    float scale = 1.0f, dx = 0.0f, dy = 0.0f, alpha = 1.0f;
    if (!thumbnail)
    {
        const float open = w.m_open.Value();
        const float minT = w.m_min.Value();
        const float sOpen = 0.93f + 0.07f * open;
        const Point c = f.Center();
        const Point target = MinimizeTarget(w);
        const float sMin = Lerp(1.0f, 0.1f, minT);
        const Point cc{Lerp(c.x, target.x, minT), Lerp(c.y, target.y, minT)};
        scale = sOpen * sMin;
        alpha = Saturate(open * 1.15f) * (1.0f - SmoothStep(0.35f, 1.0f, minT));
        dx = cc.x - c.x * scale;
        dy = cc.y - c.y * scale;
        if (alpha <= 0.004f)
            return;
    }
    const bool transformed = std::fabs(scale - 1.0f) > 0.0005f || std::fabs(dx) > 0.01f || std::fabs(dy) > 0.01f;
    if (transformed)
        r.PushTransform(dx, dy, scale);
    r.PushOpacity(alpha);

    const bool maxed = w.IsMaximized() && !thumbnail;
    const float rad = maxed ? 0.0f : 8.0f;
    const bool active = thumbnail || w.m_active;
    if (!maxed)
        r.Shadow(f, rad, active ? 26.0f : 14.0f, active ? 10.0f : 4.0f, (p.dark ? 0.6f : 0.32f) * (active ? 1.0f : 0.55f));
    r.BeginRounded(f, rad);
    if (active)
        r.Mica(f);
    else
        r.FillRect(f, p.solidBase);
    r.PushClip(f);
    r.PushTransform(f.x, f.y);

    const float th = w.TitleBarHeight();
    const bool accentBar = Theme::AccentTitleBars() && active && w.ShowTitle();
    if (accentBar)
        r.FillRect({0.0f, 0.0f, f.w, th}, Theme::A().dark1);
    w.DrawContent(r, {0.0f, th, f.w, f.h - th});

    const Color titleColor = accentBar ? Color(255, 255, 255) : (active ? p.text : p.textTertiary);
    if (w.ShowTitle())
    {
        r.DrawAppIcon(w.WindowIcon(), {12.0f, std::floor(th * 0.5f) - 8.0f, 16.0f, 16.0f});
        r.Text(w.Title(), {40.0f, 0.0f, std::max(0.0f, f.w - 40.0f - kCaptionW * 3.0f - 8.0f), th}, titleColor, 12.0f,
               FontWeight::Regular, TextFlags::VCenter);
    }

    // Кнопки заголовка.
    const bool closeOnly = !w.m_canMinimize && !w.m_canMaximize;
    for (int i = closeOnly ? 2 : 0; i < 3; ++i)
    {
        const Rect br = CaptionButtonRect(w, i);
        const bool hover = !thumbnail && w.m_captionHover == i;
        const bool press = hover && w.m_captionPress == i;
        const bool enabled = i == 2 || (i == 0 && w.m_canMinimize) || (i == 1 && w.m_canMaximize);
        Color fg = titleColor;
        if (!enabled)
            fg = accentBar ? Color(255, 255, 255, 100) : p.textDisabled;
        if (hover && enabled)
        {
            if (i == 2)
            {
                r.FillRect(br, press ? p.closePressed : p.closeHover);
                fg = Color(255, 255, 255);
            }
            else
                r.FillRect(br, press ? p.subtlePressed : (accentBar ? Color(255, 255, 255, 0x20) : p.subtleHover));
        }
        const Icon glyph = i == 0 ? Icon::Minimize : (i == 1 ? (w.IsMaximized() ? Icon::Restore : Icon::Maximize) : Icon::Close);
        const float gs = 17.0f;
        r.Glyph(glyph, {br.CenterX() - gs * 0.5f, std::min(br.CenterY(), 16.0f) - gs * 0.5f, gs, gs}, fg, 0.95f);
    }

    w.DrawDialog(r);
    r.PopTransform();
    r.PopClip();
    r.EndRounded();
    if (!maxed)
        r.StrokeRoundRect(f, rad, active ? p.surfaceStroke : p.surfaceStroke.MulAlpha(0.7f));

    r.PopOpacity();
    if (transformed)
        r.PopTransform();
}

void WindowManager::DrawThumbnail(Renderer& r, Window& w, const Rect& target)
{
    const Rect f = w.m_frame;
    if (f.w <= 0.0f || f.h <= 0.0f)
        return;
    const float s = std::min(target.w / f.w, target.h / f.h);
    const float tw = f.w * s, tx = target.x + (target.w - tw) * 0.5f;
    const float th = f.h * s, ty = target.y + (target.h - th) * 0.5f;
    r.PushTransform(tx - f.x * s, ty - f.y * s, s);
    DrawWindow(r, w, true);
    r.PopTransform();
}

// ---------------------------------------------------------------------------
// Макеты прикрепления
// ---------------------------------------------------------------------------
const std::vector<WindowManager::Layout>& WindowManager::Layouts() const
{
    static const std::vector<Layout> layouts = {
        {{{0.0f, 0.0f, 0.5f, 1.0f}, {0.5f, 0.0f, 0.5f, 1.0f}}},
        {{{0.0f, 0.0f, 0.66f, 1.0f}, {0.66f, 0.0f, 0.34f, 1.0f}}},
        {{{0.0f, 0.0f, 0.333f, 1.0f}, {0.333f, 0.0f, 0.334f, 1.0f}, {0.667f, 0.0f, 0.333f, 1.0f}}},
        {{{0.0f, 0.0f, 0.5f, 1.0f}, {0.5f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f}}},
        {{{0.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 0.5f, 0.5f}, {0.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f}}},
        {{{0.0f, 0.0f, 0.25f, 1.0f}, {0.25f, 0.0f, 0.5f, 1.0f}, {0.75f, 0.0f, 0.25f, 1.0f}}},
    };
    return layouts;
}

Rect WindowManager::SnapFlyoutRect() const
{
    if (!m_snapWin)
        return {};
    const Rect btn = m_snapWin->LocalToScreen(CaptionButtonRect(*m_snapWin, 1));
    const float w = 16.0f * 2.0f + 92.0f * 3.0f + 12.0f * 2.0f;
    const float h = 16.0f * 2.0f + 58.0f * 2.0f + 12.0f;
    const Shell& sh = GetShell();
    float x = btn.CenterX() - w * 0.5f;
    x = Clamp(x, 8.0f, sh.ScreenW() - w - 8.0f);
    float y = btn.Bottom() + 2.0f;
    if (y + h > sh.ScreenH() - 56.0f)
        y = btn.y - h - 2.0f;
    return {std::floor(x), std::floor(y), w, h};
}

Rect WindowManager::LayoutPreviewRect(int index) const
{
    const Rect fr = SnapFlyoutRect();
    const int col = index % 3, row = index / 3;
    return {fr.x + 16.0f + static_cast<float>(col) * (92.0f + 12.0f), fr.y + 16.0f + static_cast<float>(row) * (58.0f + 12.0f),
            92.0f, 58.0f};
}

void WindowManager::SnapFlyoutHit(Point p, int& layout, int& zone) const
{
    layout = -1;
    zone = -1;
    const auto& ls = Layouts();
    for (int i = 0; i < static_cast<int>(ls.size()); ++i)
    {
        const Rect pr = LayoutPreviewRect(i);
        if (!pr.Contains(p))
            continue;
        for (int z = 0; z < static_cast<int>(ls[static_cast<size_t>(i)].zones.size()); ++z)
        {
            const Rect& f = ls[static_cast<size_t>(i)].zones[static_cast<size_t>(z)];
            const Rect zr{pr.x + f.x * pr.w, pr.y + f.y * pr.h, f.w * pr.w, f.h * pr.h};
            if (zr.Contains(p))
            {
                layout = i;
                zone = z;
                return;
            }
        }
    }
}

void WindowManager::DrawOverlays(Renderer& r)
{
    const Palette& p = Theme::P();
    // Предпросмотр привязки.
    const float pa = m_previewAlpha.Value();
    if (pa > 0.003f)
    {
        const Rect pr = LerpRect(m_previewFrom, m_previewTo, m_previewAnim.Value()).Inflated(-6.0f);
        r.PushOpacity(pa);
        r.Shadow(pr, 8.0f, 20.0f, 6.0f, 0.25f);
        r.Acrylic(pr, 8.0f, p.dark ? Color::Hex(0x3A3A3A) : Color::Hex(0xFFFFFF), 0.45f);
        r.StrokeRoundRect(pr, 8.0f, p.dark ? Color(255, 255, 255, 0x30) : Color(0, 0, 0, 0x20));
        r.PopOpacity();
    }

    // Всплывающая панель макетов.
    const double now = NowSeconds();
    if (m_maxHovering && !m_snapVisible && m_snapWin)
    {
        if (now - m_maxHoverStart > 0.45)
        {
            m_snapVisible = true;
            m_snapLeaveTime = 0.0;
            m_snapAnim.Start(0.0f, 1.0f, 0.18f, Ease::OutCubic);
        }
        else
            RequestFrame();
    }
    if (m_snapVisible && m_snapLeaveTime > 0.0)
    {
        if (now - m_snapLeaveTime > 0.35)
        {
            m_snapVisible = false;
            m_maxHovering = false;
        }
        else
            RequestFrame();
    }
    if (!m_snapVisible || !m_snapWin)
        return;
    const float t = m_snapAnim.Value();
    const Rect fr = SnapFlyoutRect();
    r.PushOpacity(t);
    r.PushTransform(0.0f, (1.0f - t) * -6.0f);
    r.Shadow(fr, 8.0f, 18.0f, 6.0f, p.dark ? 0.5f : 0.25f);
    r.Acrylic(fr, 8.0f, p.menuTint, p.menuAmount);
    r.StrokeRoundRect(fr, 8.0f, p.flyoutStroke);
    const auto& ls = Layouts();
    for (int i = 0; i < static_cast<int>(ls.size()); ++i)
    {
        const Rect pr = LayoutPreviewRect(i);
        for (int z = 0; z < static_cast<int>(ls[static_cast<size_t>(i)].zones.size()); ++z)
        {
            const Rect& f = ls[static_cast<size_t>(i)].zones[static_cast<size_t>(z)];
            const Rect zr = Rect{pr.x + f.x * pr.w, pr.y + f.y * pr.h, f.w * pr.w, f.h * pr.h}.Inflated(-1.5f);
            const bool hot = i == m_snapHoverLayout && z == m_snapHoverZone;
            r.FillRoundRect(zr, 3.0f, hot ? p.accent : (p.dark ? Color(255, 255, 255, 0x18) : Color(0, 0, 0, 0x10)));
            r.StrokeRoundRect(zr, 3.0f, hot ? p.accent : (p.dark ? Color(255, 255, 255, 0x30) : Color(0, 0, 0, 0x30)));
        }
    }
    r.PopTransform();
    r.PopOpacity();
}

// ---------------------------------------------------------------------------
// Ввод
// ---------------------------------------------------------------------------
WindowManager::Zone WindowManager::HitZone(const Window& w, Point p) const
{
    const Rect f = w.m_frame;
    const bool canResize = w.m_resizable && !w.IsMaximized() && !w.m_dialogActive;
    const float outer = canResize ? 6.0f : 0.0f;
    if (!f.Inflated(outer).Contains(p))
        return Zone::None;
    const float lx = p.x - f.x, ly = p.y - f.y;
    if (canResize)
    {
        const bool l = lx < 4.0f, rr = lx > f.w - 4.0f, t = ly < 3.0f, b = ly > f.h - 4.0f;
        const bool lc = lx < 16.0f, rc = lx > f.w - 16.0f, tc = ly < 16.0f, bc = ly > f.h - 16.0f;
        if ((t && lc) || (l && tc))
            return Zone::TopLeft;
        if ((t && rc) || (rr && tc && (lx > f.w || ly < 0.0f)))
            return Zone::TopRight;
        if ((b && lc) || (l && bc))
            return Zone::BottomLeft;
        if ((b && rc) || (rr && bc))
            return Zone::BottomRight;
        if (l)
            return Zone::Left;
        if (rr && ly >= w.TitleBarHeight())
            return Zone::Right;
        if (rr && lx > f.w)
            return Zone::Right;
        if (t && lx < f.w - kCaptionW * 3.0f)
            return Zone::Top;
        if (b)
            return Zone::Bottom;
    }
    const float th = w.TitleBarHeight();
    if (ly < th && !w.m_dialogActive)
    {
        const bool closeOnly = !w.m_canMinimize && !w.m_canMaximize;
        for (int i = closeOnly ? 2 : 0; i < 3; ++i)
        {
            if (CaptionButtonRect(w, i).Contains({lx, ly}))
                return i == 0 ? Zone::MinBtn : (i == 1 ? Zone::MaxBtn : Zone::CloseBtn);
        }
        if (w.IsDragArea({lx, ly}))
            return Zone::Caption;
    }
    if (ly < th && w.m_dialogActive && lx < f.w - kCaptionW * 3.0f)
        return Zone::Caption;
    return Zone::Client;
}

Window* WindowManager::WindowAt(Point p, Zone* zone) const
{
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it)
    {
        const Window* w = it->get();
        if (w->IsMinimized() || w->m_closing)
            continue;
        const Zone z = HitZone(*w, p);
        if (z != Zone::None)
        {
            if (zone)
                *zone = z;
            return it->get();
        }
    }
    if (zone)
        *zone = Zone::None;
    return nullptr;
}

bool WindowManager::HitTest(Point p) const
{
    if (m_snapVisible && SnapFlyoutRect().Contains(p))
        return true;
    return WindowAt(p) != nullptr;
}

MouseEvent WindowManager::ToLocal(const Window& w, const MouseEvent& e) const
{
    MouseEvent l = e;
    l.pos = {e.pos.x - w.m_frame.x, e.pos.y - w.m_frame.y};
    Widget::Origin() = {w.m_frame.x, w.m_frame.y};
    return l;
}

void WindowManager::MouseDown(const MouseEvent& e)
{
    if (m_snapVisible && SnapFlyoutRect().Contains(e.pos))
    {
        int layout = -1, zone = -1;
        SnapFlyoutHit(e.pos, layout, zone);
        if (layout >= 0 && m_snapWin)
        {
            const Rect& f = Layouts()[static_cast<size_t>(layout)].zones[static_cast<size_t>(zone)];
            const Rect wa = GetShell().WorkArea();
            SnapToRect(m_snapWin, {std::floor(wa.x + f.x * wa.w), std::floor(wa.y + f.y * wa.h), std::floor(f.w * wa.w),
                                   std::floor(f.h * wa.h)});
            m_snapVisible = false;
            m_maxHovering = false;
        }
        return;
    }
    Zone z = Zone::None;
    Window* w = WindowAt(e.pos, &z);
    if (!w)
        return;
    Activate(w);
    m_snapVisible = false;
    m_maxHovering = false;

    switch (z)
    {
    case Zone::Caption:
        if (e.button == MouseButton::Left)
        {
            if (e.clicks >= 2 && w->m_canMaximize)
            {
                ToggleMaximize(w);
                return;
            }
            m_drag = Drag{};
            m_drag.mode = DragMode::Move;
            m_drag.win = w;
            m_drag.zone = z;
            m_drag.start = e.pos;
            m_drag.startFrame = w->m_frame;
        }
        break;
    case Zone::MinBtn:
    case Zone::MaxBtn:
    case Zone::CloseBtn:
        if (e.button == MouseButton::Left)
        {
            m_drag = Drag{};
            m_drag.mode = DragMode::CaptionButton;
            m_drag.win = w;
            m_drag.button = z == Zone::MinBtn ? 0 : (z == Zone::MaxBtn ? 1 : 2);
            w->m_captionPress = m_drag.button;
            w->m_captionHover = m_drag.button;
        }
        break;
    case Zone::Client:
        m_drag = Drag{};
        m_drag.mode = DragMode::Client;
        m_drag.win = w;
        w->OnMouseDown(ToLocal(*w, e));
        break;
    case Zone::None:
        break;
    default:
        if (e.button == MouseButton::Left)
        {
            m_drag = Drag{};
            m_drag.mode = DragMode::Resize;
            m_drag.win = w;
            m_drag.zone = z;
            m_drag.start = e.pos;
            m_drag.startFrame = w->m_frame;
            w->m_snapped = false;
        }
        break;
    }
}

SnapZone WindowManager::DetectSnap(Point p) const
{
    const Shell& sh = GetShell();
    const float W = sh.ScreenW();
    const float H = sh.WorkArea().Bottom();
    const float corner = 72.0f;
    if (p.y <= 1.5f)
    {
        if (p.x < corner * 0.5f)
            return SnapZone::TopLeft;
        if (p.x > W - corner * 0.5f)
            return SnapZone::TopRight;
        return SnapZone::Top;
    }
    if (p.x <= 1.5f)
    {
        if (p.y < corner)
            return SnapZone::TopLeft;
        if (p.y > H - corner)
            return SnapZone::BottomLeft;
        return SnapZone::Left;
    }
    if (p.x >= W - 2.5f)
    {
        if (p.y < corner)
            return SnapZone::TopRight;
        if (p.y > H - corner)
            return SnapZone::BottomRight;
        return SnapZone::Right;
    }
    return SnapZone::None;
}

void WindowManager::UpdatePreview(SnapZone z, const Rect& from)
{
    if (z == m_preview)
        return;
    if (z != SnapZone::None)
    {
        m_previewFrom = (m_preview == SnapZone::None || m_previewAlpha.Value() < 0.05f)
                            ? from
                            : LerpRect(m_previewFrom, m_previewTo, m_previewAnim.Value());
        m_previewTo = ZoneRect(z);
        m_previewAnim.Start(0.0f, 1.0f, 0.22f, Ease::OutCubic);
        m_previewAlpha.Set(1.0f, 0.12f);
    }
    else
        m_previewAlpha.Set(0.0f, 0.15f);
    m_preview = z;
}

void WindowManager::MouseMove(const MouseEvent& e)
{
    const Shell& sh = GetShell();
    switch (m_drag.mode)
    {
    case DragMode::Move:
    {
        Window* w = m_drag.win;
        const Point d = e.pos - m_drag.start;
        if (!m_drag.moved && std::fabs(d.x) + std::fabs(d.y) < 4.0f)
            return;
        if (!m_drag.moved)
        {
            m_drag.moved = true;
            if (w->IsMaximized() || w->m_snapped)
            {
                Rect rf = w->m_restoreFrame;
                const float fx = (m_drag.start.x - w->m_frame.x) / std::max(1.0f, w->m_frame.w);
                rf.x = std::floor(e.pos.x - fx * rf.w);
                rf.y = std::floor(e.pos.y - std::min(m_drag.start.y - w->m_frame.y, 20.0f));
                w->m_state = WindowState::Normal;
                w->m_snapped = false;
                BeginMorph(w, rf);
                m_drag.startFrame = rf;
                m_drag.start = e.pos;
            }
        }
        const Rect wa = sh.WorkArea();
        Rect f = m_drag.startFrame;
        f.x = std::floor(f.x + e.pos.x - m_drag.start.x);
        f.y = std::floor(Clamp(f.y + e.pos.y - m_drag.start.y, wa.y, wa.Bottom() - 40.0f));
        w->m_frame = f;
        UpdatePreview(w->m_resizable ? DetectSnap(e.pos) : SnapZone::None, f);
        return;
    }
    case DragMode::Resize:
    {
        Window* w = m_drag.win;
        Rect f = m_drag.startFrame;
        const float dx = e.pos.x - m_drag.start.x, dy = e.pos.y - m_drag.start.y;
        const Point mn = w->m_minSize;
        const Zone z = m_drag.zone;
        const bool left = z == Zone::Left || z == Zone::TopLeft || z == Zone::BottomLeft;
        const bool right = z == Zone::Right || z == Zone::TopRight || z == Zone::BottomRight;
        const bool top = z == Zone::Top || z == Zone::TopLeft || z == Zone::TopRight;
        const bool bottom = z == Zone::Bottom || z == Zone::BottomLeft || z == Zone::BottomRight;
        if (left)
        {
            const float nx = std::min(f.x + dx, f.Right() - mn.x);
            f.w = f.Right() - nx;
            f.x = nx;
        }
        if (right)
            f.w = std::max(mn.x, f.w + dx);
        if (top)
        {
            const float ny = Clamp(f.y + dy, 0.0f, f.Bottom() - mn.y);
            f.h = f.Bottom() - ny;
            f.y = ny;
        }
        if (bottom)
            f.h = std::max(mn.y, f.h + dy);
        f = {std::floor(f.x), std::floor(f.y), std::floor(f.w), std::floor(f.h)};
        w->m_frame = f;
        return;
    }
    case DragMode::Client:
        if (m_drag.win)
            m_drag.win->OnMouseMove(ToLocal(*m_drag.win, e));
        return;
    case DragMode::CaptionButton:
        if (m_drag.win)
            m_drag.win->m_captionHover =
                CaptionButtonRect(*m_drag.win, m_drag.button).Contains({e.pos.x - m_drag.win->m_frame.x, e.pos.y - m_drag.win->m_frame.y})
                    ? m_drag.button
                    : -1;
        return;
    case DragMode::Dialog:
    case DragMode::None:
        break;
    }

    // Наведение.
    Zone z = Zone::None;
    Window* w = WindowAt(e.pos, &z);
    const bool overFlyout = m_snapVisible && SnapFlyoutRect().Contains(e.pos);
    if (overFlyout)
    {
        w = nullptr;
        z = Zone::None;
    }
    if (w != m_hoverWin)
    {
        if (m_hoverWin && Contains(m_hoverWin))
        {
            m_hoverWin->m_captionHover = -1;
            if (m_hoverWin->m_clientHover)
            {
                m_hoverWin->OnMouseLeave();
                m_hoverWin->m_clientHover = false;
            }
        }
        m_hoverWin = w;
    }
    if (w)
    {
        w->m_captionHover = z == Zone::MinBtn ? 0 : (z == Zone::MaxBtn ? 1 : (z == Zone::CloseBtn ? 2 : -1));
        if (z == Zone::Client)
        {
            w->m_clientHover = true;
            w->OnMouseMove(ToLocal(*w, e));
        }
        else if (w->m_clientHover)
        {
            w->OnMouseLeave();
            w->m_clientHover = false;
        }
        if (w->m_captionHover >= 0)
        {
            static const wchar_t* tips[] = {L"Свернуть", L"Развернуть", L"Закрыть"};
            const wchar_t* tip = w->m_captionHover == 1 && w->IsMaximized() ? L"Свернуть в окно" : tips[w->m_captionHover];
            GetShell().Tooltip(&w->m_captionHover, tip, w->LocalToScreen(CaptionButtonRect(*w, w->m_captionHover)), false);
        }
    }

    // Макеты прикрепления при наведении на «Развернуть».
    const bool overMax = w && z == Zone::MaxBtn && w->m_canMaximize;
    if (overMax)
    {
        if (!m_maxHovering || m_snapWin != w)
        {
            m_maxHovering = true;
            m_maxHoverStart = NowSeconds();
            if (m_snapWin != w)
                m_snapVisible = false;
            m_snapWin = w;
            RequestFrame();
        }
    }
    else if (!overFlyout)
        m_maxHovering = m_snapVisible && m_maxHovering;
    if (m_snapVisible)
    {
        if (overMax || overFlyout)
            m_snapLeaveTime = 0.0;
        else if (m_snapLeaveTime == 0.0)
            m_snapLeaveTime = NowSeconds();
        SnapFlyoutHit(e.pos, m_snapHoverLayout, m_snapHoverZone);
    }
}

void WindowManager::MouseUp(const MouseEvent& e)
{
    const Drag d = m_drag;
    m_drag = Drag{};
    switch (d.mode)
    {
    case DragMode::Move:
        if (m_preview != SnapZone::None && d.win)
            SnapTo(d.win, m_preview);
        UpdatePreview(SnapZone::None, {});
        break;
    case DragMode::Client:
        if (d.win && Contains(d.win))
            d.win->OnMouseUp(ToLocal(*d.win, e));
        break;
    case DragMode::CaptionButton:
        if (d.win && Contains(d.win))
        {
            d.win->m_captionPress = -1;
            const bool inside = CaptionButtonRect(*d.win, d.button)
                                    .Contains({e.pos.x - d.win->m_frame.x, e.pos.y - d.win->m_frame.y});
            if (inside)
            {
                if (d.button == 0)
                    Minimize(d.win);
                else if (d.button == 1)
                    ToggleMaximize(d.win);
                else
                    d.win->Close();
            }
        }
        break;
    case DragMode::Resize:
    case DragMode::Dialog:
        break;
    case DragMode::None:
        if (e.button == MouseButton::Right)
        {
            Zone z = Zone::None;
            Window* w = WindowAt(e.pos, &z);
            if (w && z == Zone::Caption)
                ShowSystemMenu(w, e.pos);
        }
        break;
    }
}

bool WindowManager::MouseWheel(const MouseEvent& e)
{
    Window* w = WindowAt(e.pos);
    if (!w)
        return false;
    w->OnMouseWheel(ToLocal(*w, e));
    return true;
}

void WindowManager::MouseLeave()
{
    if (m_hoverWin && Contains(m_hoverWin))
    {
        m_hoverWin->m_captionHover = -1;
        if (m_hoverWin->m_clientHover)
        {
            m_hoverWin->OnMouseLeave();
            m_hoverWin->m_clientHover = false;
        }
    }
    m_hoverWin = nullptr;
    if (m_snapVisible && m_snapLeaveTime == 0.0)
        m_snapLeaveTime = NowSeconds();
    m_maxHovering = m_snapVisible && m_maxHovering;
}

void WindowManager::CancelDrag()
{
    if (m_drag.mode == DragMode::Move)
        UpdatePreview(SnapZone::None, {});
    if (m_drag.win && Contains(m_drag.win))
        m_drag.win->m_captionPress = -1;
    m_drag = Drag{};
}

bool WindowManager::KeyDown(UINT vk, const KeyMods& mods)
{
    Window* w = m_active;
    if (!w)
        return false;
    Widget::Origin() = {w->m_frame.x, w->m_frame.y};
    if (w->m_dialogActive)
    {
        const auto& kids = w->m_dialogUi.Children();
        if (kids.empty())
            return true;
        if (vk == VK_RETURN)
        {
            auto* b = dynamic_cast<Button*>(kids.front().get());
            if (b && b->onClick)
                PostAction(b->onClick);
        }
        else if (vk == VK_ESCAPE)
        {
            auto* b = dynamic_cast<Button*>(kids.back().get());
            if (b && b->onClick)
                PostAction(b->onClick);
        }
        return true;
    }
    return w->OnKeyDown(vk, mods);
}

bool WindowManager::Char(wchar_t ch)
{
    Window* w = m_active;
    if (!w || w->m_dialogActive)
        return w != nullptr;
    return w->OnChar(ch);
}

CursorType WindowManager::Cursor(Point p) const
{
    Zone z = Zone::None;
    const Window* w = nullptr;
    if (m_drag.mode == DragMode::Resize)
    {
        z = m_drag.zone;
        w = m_drag.win;
    }
    else if (m_drag.mode == DragMode::Client && m_drag.win)
        return m_drag.win->Cursor({p.x - m_drag.win->m_frame.x, p.y - m_drag.win->m_frame.y});
    else if (m_drag.mode == DragMode::Move)
        return CursorType::Arrow;
    else
        w = WindowAt(p, &z);
    switch (z)
    {
    case Zone::Left:
    case Zone::Right:
        return CursorType::SizeWE;
    case Zone::Top:
    case Zone::Bottom:
        return CursorType::SizeNS;
    case Zone::TopLeft:
    case Zone::BottomRight:
        return CursorType::SizeNWSE;
    case Zone::TopRight:
    case Zone::BottomLeft:
        return CursorType::SizeNESW;
    case Zone::Client:
        return w ? w->Cursor({p.x - w->m_frame.x, p.y - w->m_frame.y}) : CursorType::Arrow;
    default:
        return CursorType::Arrow;
    }
}

void WindowManager::ShowSystemMenu(Window* w, Point at)
{
    const bool maxed = w->IsMaximized() || w->m_snapped;
    std::vector<MenuItem> items;
    items.push_back(MenuItem(L"Восстановить", Icon::Restore, [this, w]() {
                        if (Contains(w))
                            Restore(w);
                    }).Disabled(!maxed));
    items.push_back(MenuItem(L"Переместить", Icon::None, nullptr).Disabled());
    items.push_back(MenuItem(L"Размер", Icon::None, nullptr).Disabled());
    items.push_back(MenuItem(L"Свернуть", Icon::Minimize, [this, w]() {
                        if (Contains(w))
                            Minimize(w);
                    }).Disabled(!w->m_canMinimize));
    items.push_back(MenuItem(L"Развернуть", Icon::Maximize, [this, w]() {
                        if (Contains(w))
                            Maximize(w);
                    }).Disabled(w->IsMaximized() || !w->m_canMaximize));
    items.push_back(MenuItem::Sep());
    items.push_back(MenuItem(L"Закрыть", Icon::Close, [this, w]() {
                        if (Contains(w))
                            w->Close();
                    }, L"Alt+F4"));
    GetShell().ShowMenu(at, std::move(items));
}
