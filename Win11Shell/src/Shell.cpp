#include "Shell.h"

#include "Desktop.h"
#include "Flyouts.h"
#include "Overlays.h"
#include "StartMenu.h"
#include "Taskbar.h"
#include "apps/AppFactories.h"
#include "apps/Photos.h"

namespace
{
Shell* g_shell = nullptr;
constexpr UINT_PTR kTimerTip = 4;
} // namespace

Shell& GetShell() { return *g_shell; }

Shell::Shell(HWND hwnd, Renderer& r) : m_hwnd(hwnd), m_r(r)
{
    g_shell = this;
    m_fs = std::make_unique<VirtualFS>();
    m_wm = std::make_unique<WindowManager>();
    m_desktop = std::make_unique<Desktop>();
    m_taskbar = std::make_unique<Taskbar>();
    m_start = std::make_unique<StartMenu>();
    m_qs = std::make_unique<QuickSettings>();
    m_nc = std::make_unique<NotificationCenter>();
    m_widgets = std::make_unique<WidgetsBoard>();
    m_tray = std::make_unique<TrayOverflow>();
    m_toasts = std::make_unique<Toasts>();
    m_taskView = std::make_unique<TaskView>();
    m_screens = std::make_unique<SystemScreens>();
}

Shell::~Shell()
{
    m_menu.Close();
    m_wm.reset();
    m_fs.reset();
    ClearPhotoCache();
    g_shell = nullptr;
}

Rect Shell::WorkArea() const { return {0.0f, 0.0f, ScreenW(), ScreenH() - Taskbar::kHeight}; }

std::vector<Flyout*> Shell::Flyouts() const
{
    return {m_tray.get(), m_nc.get(), m_qs.get(), m_start.get(), m_widgets.get()};
}

// ---------------------------------------------------------------------------
// Кадр
// ---------------------------------------------------------------------------
void Shell::Resize(int pxWidth, int pxHeight, float scale)
{
    const bool sizeChanged = pxWidth != m_r.PixelWidth() || pxHeight != m_r.PixelHeight() || scale != m_r.Scale();
    m_r.Resize(pxWidth, pxHeight, scale);
    m_menu.SetScreen(ScreenW(), ScreenH());
    if (sizeChanged || !m_r.HasWallpaper())
    {
        std::vector<uint32_t> px;
        RenderWallpaper(m_settings.wallpaper, pxWidth, pxHeight, px);
        m_r.SetWallpaper(std::move(px), pxWidth, pxHeight);
    }
    // Развёрнутые окна — по новой рабочей области.
    for (auto& w : m_wm->Windows())
    {
        if (w->IsMaximized())
        {
            m_wm->Restore(w.get());
            m_wm->Maximize(w.get());
        }
    }
    if (!m_initialized)
    {
        m_initialized = true;
        Notify(AppIcon::Tips, L"Советы", L"Добро пожаловать в Windows 11",
               L"Нажмите Win, чтобы открыть «Пуск». Перетащите окно к краю экрана, чтобы прикрепить его.");
    }
    Invalidate();
}

void Shell::Invalidate() { InvalidateRect(m_hwnd, nullptr, FALSE); }

void Shell::Paint(HDC hdc)
{
    if (!m_r.Ready())
        return;
    RunPostedActions();
    m_wm->Update();
    TakeFrameRequest();
    m_r.BeginFrame();

    m_desktop->Draw(m_r);
    if (!m_taskView->IsVisible())
        m_wm->Draw(m_r);
    m_wm->DrawOverlays(m_r);
    m_taskView->Draw(m_r);
    m_taskbar->Draw(m_r);
    m_taskbar->DrawPreview(m_r);
    const std::vector<Flyout*> fl = Flyouts();
    for (auto it = fl.rbegin(); it != fl.rend(); ++it)
    {
        if ((*it)->IsVisible())
            (*it)->Draw(m_r);
    }
    m_toasts->Draw(m_r);
    m_menu.Draw(m_r);
    DrawTooltip(m_r);
    m_screens->Draw(m_r);

    // Ночной свет и яркость действуют на весь экран.
    if (m_settings.nightLight)
        m_r.Dim(Color(255, 150, 40, 0x2C));
    if (m_settings.brightness < 0.999f)
        m_r.Dim(Color(0, 0, 0, static_cast<uint8_t>(Saturate(1.0f - m_settings.brightness) * 200.0f)));

    m_r.EndFrame(hdc);
    m_wantFrame = TakeFrameRequest();
}

// ---------------------------------------------------------------------------
// Маршрутизация ввода
// ---------------------------------------------------------------------------
Shell::Layer Shell::LayerAt(Point p, Flyout** flyout) const
{
    *flyout = nullptr;
    if (m_screens->BlocksInput())
        return Layer::Screens;
    if (m_menu.IsOpen() && m_menu.HitTest(p))
        return Layer::Menu;
    for (Flyout* f : Flyouts())
    {
        if (f->IsOpen() && f->HitTest(p))
        {
            *flyout = f;
            return Layer::Flyout;
        }
    }
    if (m_toasts->HitTest(p))
        return Layer::Toast;
    if (m_taskbar->HitTest(p))
        return Layer::Taskbar;
    if (m_taskView->IsOpen())
        return Layer::TaskView;
    if (m_wm->HitTest(p))
        return Layer::Windows;
    return Layer::Desktop;
}

void Shell::LeaveLayer(Layer l, Flyout* f)
{
    switch (l)
    {
    case Layer::Taskbar:
        m_taskbar->MouseLeave();
        break;
    case Layer::Windows:
        m_wm->MouseLeave();
        break;
    case Layer::Desktop:
        m_desktop->MouseLeave();
        break;
    case Layer::Flyout:
        if (f)
            f->MouseLeave();
        break;
    case Layer::Toast:
        m_toasts->MouseLeave();
        break;
    case Layer::TaskView:
        m_taskView->MouseLeave();
        break;
    case Layer::Menu:
    case Layer::Screens:
    case Layer::None:
        break;
    }
}

void Shell::RouteMove(const MouseEvent& e)
{
    if (m_menu.IsOpen())
    {
        m_menu.OnMouseMove(e.pos);
        Rect anchor;
        if (const MenuAction* a = m_menu.HoveredAction(anchor))
            Tooltip(a, a->tooltip, anchor, false);
    }
    Layer l = m_capture;
    Flyout* f = m_captureFlyout;
    if (l == Layer::None)
    {
        l = LayerAt(e.pos, &f);
        if (l != m_hoverLayer || f != m_hoverFlyout)
        {
            LeaveLayer(m_hoverLayer, m_hoverFlyout);
            m_hoverLayer = l;
            m_hoverFlyout = f;
        }
    }
    switch (l)
    {
    case Layer::Screens:
        m_screens->MouseMove(e);
        break;
    case Layer::Menu:
        break;
    case Layer::Flyout:
        if (f)
            f->MouseMove(e);
        break;
    case Layer::Toast:
        m_toasts->MouseMove(e);
        break;
    case Layer::Taskbar:
        m_taskbar->MouseMove(e);
        break;
    case Layer::TaskView:
        m_taskView->MouseMove(e);
        break;
    case Layer::Windows:
        m_wm->MouseMove(e);
        break;
    case Layer::Desktop:
        m_desktop->MouseMove(e);
        break;
    case Layer::None:
        break;
    }
}

void Shell::MouseMove(Point p)
{
    m_mouse = p;
    m_tipTouched = false;
    MouseEvent e;
    e.pos = p;
    e.mods = CurrentMods();
    RouteMove(e);
    if (!m_tipTouched)
        m_tipOwner = nullptr;
    AfterInput();
}

void Shell::MouseDown(Point p, MouseButton b, int clicks)
{
    ++m_clickSerial;
    m_buttonsDown = true;
    m_mouse = p;
    m_tipBlockOwner = m_tipOwner;
    m_tipBlockText = m_tipText;
    m_tipOwner = nullptr;
    MouseEvent e;
    e.pos = p;
    e.button = b;
    e.clicks = clicks;
    e.mods = CurrentMods();

    if (m_screens->BlocksInput())
    {
        if (m_menu.IsOpen() && m_menu.HitTest(p))
        {
            m_menu.OnMouseDown(p);
            m_capture = Layer::Menu;
        }
        else
        {
            m_menu.Close();
            m_screens->MouseDown(e);
            m_capture = Layer::Screens;
        }
        AfterInput();
        return;
    }
    if (m_menu.IsOpen())
    {
        if (m_menu.HitTest(p))
        {
            m_menu.OnMouseDown(p);
            m_capture = Layer::Menu;
        }
        else
        {
            m_menu.Close();
            m_capture = Layer::None;
            m_suppressUp = true;
        }
        AfterInput();
        return;
    }
    for (Flyout* f : Flyouts())
    {
        if (f->IsOpen() && f->HitTest(p))
        {
            f->MouseDown(e);
            m_capture = Layer::Flyout;
            m_captureFlyout = f;
            AfterInput();
            return;
        }
    }
    for (Flyout* f : Flyouts())
    {
        if (f->IsOpen())
        {
            m_closedByClick.emplace_back(f, m_clickSerial);
            f->Close();
        }
    }
    if (m_toasts->HitTest(p))
    {
        m_toasts->MouseDown(e);
        m_capture = Layer::Toast;
    }
    else if (m_taskbar->HitTest(p))
    {
        m_taskbar->MouseDown(e);
        m_capture = Layer::Taskbar;
    }
    else if (m_taskView->IsOpen())
    {
        m_taskView->MouseDown(e);
        m_capture = Layer::TaskView;
    }
    else if (m_wm->HitTest(p))
    {
        if (m_desktop->Renaming())
            m_desktop->FinishRename();
        m_wm->MouseDown(e);
        m_capture = Layer::Windows;
    }
    else
    {
        m_wm->Deactivate();
        m_desktop->MouseDown(e);
        m_capture = Layer::Desktop;
    }
    AfterInput();
}

void Shell::MouseUp(Point p, MouseButton b)
{
    m_buttonsDown = false;
    MouseEvent e;
    e.pos = p;
    e.button = b;
    e.mods = CurrentMods();
    const Layer cap = m_capture;
    Flyout* f = m_captureFlyout;
    m_capture = Layer::None;
    m_captureFlyout = nullptr;
    if (m_suppressUp)
    {
        m_suppressUp = false;
        AfterInput();
        return;
    }
    switch (cap)
    {
    case Layer::Screens:
        m_screens->MouseUp(e);
        break;
    case Layer::Menu:
        m_menu.OnMouseUp(p);
        break;
    case Layer::Flyout:
        if (f)
            f->MouseUp(e);
        break;
    case Layer::Toast:
        m_toasts->MouseUp(e);
        break;
    case Layer::Taskbar:
        m_taskbar->MouseUp(e);
        break;
    case Layer::TaskView:
        m_taskView->MouseUp(e);
        break;
    case Layer::Windows:
        m_wm->MouseUp(e);
        break;
    case Layer::Desktop:
        m_desktop->MouseUp(e);
        break;
    case Layer::None:
        break;
    }
    RunPostedActions();
    m_closedByClick.clear();
    MouseEvent mv;
    mv.pos = p;
    mv.mods = e.mods;
    RouteMove(mv);
    AfterInput();
}

void Shell::MouseWheel(Point p, float notches)
{
    MouseEvent e;
    e.pos = p;
    e.wheel = notches;
    e.mods = CurrentMods();
    m_tipOwner = nullptr;
    Flyout* f = nullptr;
    const Layer l = LayerAt(p, &f);
    switch (l)
    {
    case Layer::Flyout:
        if (f)
            f->MouseWheel(e);
        break;
    case Layer::Taskbar:
        m_taskbar->MouseWheel(e);
        break;
    case Layer::Windows:
        m_wm->MouseWheel(e);
        break;
    default:
        break;
    }
    AfterInput();
}

void Shell::MouseLeftWindow()
{
    LeaveLayer(m_hoverLayer, m_hoverFlyout);
    m_hoverLayer = Layer::None;
    m_hoverFlyout = nullptr;
    m_tipOwner = nullptr;
    AfterInput();
}

void Shell::CaptureLost()
{
    m_wm->CancelDrag();
    m_capture = Layer::None;
    m_captureFlyout = nullptr;
    m_buttonsDown = false;
}

void Shell::AfterInput()
{
    RunPostedActions();
    Invalidate();
}

// ---------------------------------------------------------------------------
// Клавиатура
// ---------------------------------------------------------------------------
bool Shell::GlobalKey(UINT vk, const KeyMods& mods)
{
    if (vk == VK_F4 && mods.alt)
    {
        if (Window* w = m_wm->Active())
            w->Close();
        else
            ShowShutdownDialog();
        return true;
    }
    if (vk == VK_SNAPSHOT)
    {
        TakeScreenshot();
        return true;
    }
    if (vk == VK_ESCAPE && mods.ctrl && mods.shift)
    {
        Launch(AppId::TaskManager);
        return true;
    }
    if (vk == VK_SPACE && mods.alt)
    {
        if (Window* w = m_wm->Active())
        {
            std::vector<MenuItem> items;
            items.push_back(MenuItem(L"Восстановить", Icon::Restore, [w]() { GetShell().WM().Restore(w); }));
            items.push_back(MenuItem(L"Свернуть", Icon::Minimize, [w]() { GetShell().WM().Minimize(w); }));
            items.push_back(MenuItem(L"Развернуть", Icon::Maximize, [w]() { GetShell().WM().Maximize(w); }));
            items.push_back(MenuItem::Sep());
            items.push_back(MenuItem(L"Закрыть", Icon::Close, [w]() { w->Close(); }, L"Alt+F4"));
            ShowMenu({w->Frame().x + 8.0f, w->Frame().y + 32.0f}, std::move(items));
        }
        return true;
    }
    return false;
}

void Shell::KeyDown(UINT vk, bool /*repeat*/)
{
    const KeyMods mods = CurrentMods();
    m_tipOwner = nullptr;
    if (m_screens->BlocksInput())
    {
        if (m_menu.IsOpen())
            m_menu.OnKeyDown(vk);
        else
            m_screens->KeyDown(vk, mods);
        AfterInput();
        return;
    }
    if (m_menu.IsOpen())
    {
        m_menu.OnKeyDown(vk);
        AfterInput();
        return;
    }
    if (GlobalKey(vk, mods))
    {
        AfterInput();
        return;
    }
    if (m_taskView->IsOpen())
    {
        m_taskView->KeyDown(vk, mods);
        AfterInput();
        return;
    }
    for (Flyout* f : Flyouts())
    {
        if (f->IsOpen())
        {
            f->KeyDown(vk, mods);
            AfterInput();
            return;
        }
    }
    if (m_desktop->Renaming())
    {
        m_desktop->KeyDown(vk, mods);
        AfterInput();
        return;
    }
    if (m_wm->Active())
        m_wm->KeyDown(vk, mods);
    else
        m_desktop->KeyDown(vk, mods);
    AfterInput();
}

void Shell::KeyUp(UINT /*vk*/) {}

void Shell::Char(wchar_t ch)
{
    if (m_screens->BlocksInput())
    {
        m_screens->Char(ch);
        AfterInput();
        return;
    }
    if (m_menu.IsOpen() || m_taskView->IsOpen())
        return;
    for (Flyout* f : Flyouts())
    {
        if (f->IsOpen())
        {
            f->Char(ch);
            AfterInput();
            return;
        }
    }
    if (m_desktop->Renaming())
        m_desktop->Char(ch);
    else if (m_wm->Active())
        m_wm->Char(ch);
    AfterInput();
}

void Shell::WinKey()
{
    if (m_screens->BlocksInput())
        return;
    m_menu.Close();
    ToggleStart();
    AfterInput();
}

void Shell::WinCombo(UINT vk)
{
    if (m_screens->BlocksInput())
        return;
    m_menu.Close();
    switch (vk)
    {
    case 'D':
        CloseFlyouts();
        m_wm->ShowDesktop();
        break;
    case 'M':
        CloseFlyouts();
        m_wm->MinimizeAll();
        break;
    case 'E':
        Launch(AppId::Explorer);
        break;
    case 'I':
        Launch(AppId::Settings);
        break;
    case 'A':
        ToggleQuickSettings();
        break;
    case 'N':
        ToggleNotifications();
        break;
    case 'W':
        ToggleWidgets();
        break;
    case 'S':
    case 'Q':
        ToggleStart(true);
        break;
    case 'R':
        Launch(AppId::Terminal);
        break;
    case 'X':
        ShowWinXMenu({m_taskbar->StartButtonRect().x, m_taskbar->Top() - 4.0f});
        break;
    case 'L':
        Lock();
        break;
    case VK_TAB:
        ToggleTaskView();
        break;
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
        m_wm->SnapActive(vk);
        break;
    case VK_SNAPSHOT:
        TakeScreenshot();
        break;
    default:
        break;
    }
    AfterInput();
}

// ---------------------------------------------------------------------------
// Таймеры и курсор
// ---------------------------------------------------------------------------
void Shell::Timer(UINT_PTR id)
{
    switch (id)
    {
    case 1: // кадр анимации
        if (RunPostedActions() || m_wantFrame)
            Invalidate();
        break;
    case 2: // раз в секунду
    {
        for (auto& w : m_wm->Windows())
            w->OnTimer1s();
        const SYSTEMTIME now = LocalNow();
        if (now.wMinute != m_lastMinute)
            m_lastMinute = now.wMinute;
        Invalidate();
        break;
    }
    case 3: // мигание каретки
        if (m_r.TakeCaretNote())
            Invalidate();
        break;
    case kTimerTip:
        KillTimer(m_hwnd, kTimerTip);
        Invalidate();
        break;
    default:
        break;
    }
}

HCURSOR Shell::CursorAt(Point p)
{
    CursorType c = CursorType::Arrow;
    Flyout* f = nullptr;
    Layer l = m_capture != Layer::None ? m_capture : LayerAt(p, &f);
    if (m_capture == Layer::Flyout)
        f = m_captureFlyout;
    switch (l)
    {
    case Layer::Flyout:
        if (f)
            c = f->Cursor(p);
        break;
    case Layer::Windows:
        c = m_wm->Cursor(p);
        break;
    case Layer::Desktop:
        c = m_desktop->Cursor(p);
        break;
    case Layer::Screens:
        c = m_screens->Cursor(p);
        break;
    default:
        break;
    }
    LPCWSTR id = IDC_ARROW;
    switch (c)
    {
    case CursorType::Arrow:
        id = IDC_ARROW;
        break;
    case CursorType::Hand:
        id = IDC_HAND;
        break;
    case CursorType::IBeam:
        id = IDC_IBEAM;
        break;
    case CursorType::SizeWE:
        id = IDC_SIZEWE;
        break;
    case CursorType::SizeNS:
        id = IDC_SIZENS;
        break;
    case CursorType::SizeNWSE:
        id = IDC_SIZENWSE;
        break;
    case CursorType::SizeNESW:
        id = IDC_SIZENESW;
        break;
    case CursorType::SizeAll:
        id = IDC_SIZEALL;
        break;
    case CursorType::Wait:
        id = IDC_WAIT;
        break;
    case CursorType::Cross:
        id = IDC_CROSS;
        break;
    }
    return LoadCursorW(nullptr, id);
}

// ---------------------------------------------------------------------------
// Подсказки
// ---------------------------------------------------------------------------
void Shell::Tooltip(const void* owner, const std::wstring& text, const Rect& anchor, bool above)
{
    m_tipTouched = true;
    if (m_buttonsDown)
        return;
    if (owner == m_tipBlockOwner && text == m_tipBlockText)
        return;
    m_tipBlockOwner = nullptr;
    if (owner != m_tipOwner || text != m_tipText)
    {
        m_tipOwner = owner;
        m_tipText = text;
        m_tipAnchor = anchor;
        m_tipAbove = above;
        m_tipStart = NowSeconds();
        SetTimer(m_hwnd, kTimerTip, 650, nullptr);
    }
}

void Shell::DrawTooltip(Renderer& r)
{
    if (!m_tipOwner || m_tipText.empty() || m_buttonsDown)
        return;
    const double since = NowSeconds() - m_tipStart;
    if (since < 0.6)
        return;
    const float a = Saturate(static_cast<float>(since - 0.6) / 0.12f);
    if (a < 1.0f)
        RequestFrame();
    const Palette& p = Theme::P();
    // Разбить на строки.
    std::vector<std::wstring> lines;
    size_t start = 0;
    while (start <= m_tipText.size())
    {
        const size_t nl = m_tipText.find(L'\n', start);
        lines.push_back(m_tipText.substr(start, nl == std::wstring::npos ? std::wstring::npos : nl - start));
        if (nl == std::wstring::npos)
            break;
        start = nl + 1;
    }
    float w = 0.0f;
    for (const auto& l : lines)
        w = std::max(w, r.TextWidth(l, 12.0f));
    w = std::min(w + 18.0f, 420.0f);
    const float lh = 16.0f;
    const float h = static_cast<float>(lines.size()) * lh + 12.0f;
    float x = m_tipAnchor.CenterX() - w * 0.5f;
    float y = m_tipAbove ? m_tipAnchor.y - h - 8.0f : m_tipAnchor.Bottom() + 8.0f;
    if (y < 4.0f)
        y = m_tipAnchor.Bottom() + 8.0f;
    if (y + h > ScreenH() - 4.0f)
        y = m_tipAnchor.y - h - 8.0f;
    x = Clamp(x, 4.0f, ScreenW() - w - 4.0f);
    const Rect tr{std::floor(x), std::floor(y), std::ceil(w), h};
    r.PushOpacity(a);
    r.Shadow(tr, 4.0f, 8.0f, 2.0f, p.dark ? 0.4f : 0.16f);
    r.FillRoundRect(tr, 4.0f, p.tooltipBg);
    r.StrokeRoundRect(tr, 4.0f, p.flyoutStroke);
    for (size_t i = 0; i < lines.size(); ++i)
        r.Text(lines[i], {tr.x + 9.0f, tr.y + 6.0f + static_cast<float>(i) * lh, tr.w - 18.0f, lh}, p.text, 12.0f,
               FontWeight::Regular, TextFlags::VCenter);
    r.PopOpacity();
}

// ---------------------------------------------------------------------------
// Действия
// ---------------------------------------------------------------------------
Window* Shell::Launch(AppId id, const std::wstring& arg)
{
    if (id == AppId::None)
        return nullptr;
    CloseFlyouts();
    m_menu.Close();
    if (m_taskView->IsOpen())
        m_taskView->Close(nullptr);
    const AppInfo& info = GetAppInfo(id);
    if (info.singleInstance)
    {
        if (Window* w = m_wm->FindApp(id))
        {
            m_wm->Activate(w);
            if (!arg.empty())
                w->OnLaunchArgs(arg);
            return w;
        }
    }
    std::unique_ptr<Window> w = CreateAppWindow(id, arg);
    if (!w)
        return nullptr;
    return m_wm->Add(std::move(w));
}

void Shell::Open(FsNode* node)
{
    if (!node)
        return;
    const std::wstring arg = VirtualFS::NodeArg(node);
    switch (node->kind)
    {
    case FileKind::Folder:
    case FileKind::Drive:
        Launch(AppId::Explorer, arg);
        break;
    case FileKind::Text:
    case FileKind::Other:
        Launch(AppId::Notepad, arg);
        break;
    case FileKind::Image:
        Launch(AppId::Photos, arg);
        break;
    case FileKind::Audio:
    case FileKind::Video:
        Launch(AppId::MediaPlayer, arg);
        break;
    case FileKind::Shortcut:
        Launch(node->app);
        break;
    case FileKind::App:
        ShowMessage(node->name, L"Это приложение не может быть запущено в демонстрационной версии Windows.\n"
                                L"Файл существует только в виртуальной файловой системе.",
                    AppIcon::FileApp);
        break;
    case FileKind::Document:
    case FileKind::Archive:
        ShowMessage(node->name, L"Для этого файла не найдено подходящее приложение.\nОткройте текстовые файлы, "
                                L"изображения, музыку или видео — для них есть встроенные приложения.",
                    m_fs->IconFor(node));
        break;
    }
}

void Shell::ShowMenu(Point screen, std::vector<MenuItem> items, std::vector<MenuAction> top, bool classic)
{
    m_tipOwner = nullptr;
    m_menu.Open(screen, std::move(items), std::move(top), classic);
}

void Shell::ShowMenuBelow(const Rect& anchor, std::vector<MenuItem> items, float minWidth, bool above)
{
    m_tipOwner = nullptr;
    m_menu.OpenBelow(anchor, std::move(items), minWidth, above);
}

void ShowDropdown(const Rect& screenAnchor, const std::vector<std::wstring>& items, int selected,
                  std::function<void(int)> onSelect)
{
    std::vector<MenuItem> list;
    for (size_t i = 0; i < items.size(); ++i)
    {
        const int idx = static_cast<int>(i);
        MenuItem m(items[i], Icon::None, [onSelect, idx]() {
            if (onSelect)
                onSelect(idx);
        });
        m.pill = idx == selected;
        list.push_back(m);
    }
    const float y = screenAnchor.y - 4.0f - static_cast<float>(std::max(0, selected)) * 32.0f + (screenAnchor.h - 32.0f) * 0.5f;
    GetShell().Menu().Open({screenAnchor.x - 4.0f, y}, std::move(list), {}, false, screenAnchor.w + 8.0f);
}

void Shell::CloseFlyouts(const Flyout* except)
{
    for (Flyout* f : Flyouts())
    {
        if (f != except && f->IsOpen())
            f->Close();
    }
}

bool Shell::FlyoutJustClosed(const Flyout* f) const
{
    for (const auto& pr : m_closedByClick)
    {
        if (pr.first == f && pr.second == m_clickSerial)
            return true;
    }
    return false;
}

void Shell::ToggleStart(bool search)
{
    if (m_start->IsOpen())
    {
        if (search)
            m_start->OpenSearch();
        else
            m_start->Close();
        return;
    }
    if (!search && FlyoutJustClosed(m_start.get()))
        return;
    CloseFlyouts(m_start.get());
    m_menu.Close();
    if (m_taskView->IsOpen())
        m_taskView->Close(nullptr);
    m_start->Open();
    if (search)
        m_start->OpenSearch();
}

void Shell::ToggleQuickSettings()
{
    if (m_qs->IsOpen())
    {
        m_qs->Close();
        return;
    }
    if (FlyoutJustClosed(m_qs.get()))
        return;
    CloseFlyouts(m_qs.get());
    m_qs->Open();
}

void Shell::ToggleNotifications()
{
    if (m_nc->IsOpen())
    {
        m_nc->Close();
        return;
    }
    if (FlyoutJustClosed(m_nc.get()))
        return;
    CloseFlyouts(m_nc.get());
    m_nc->Open();
}

void Shell::ToggleWidgets()
{
    if (m_widgets->IsOpen())
    {
        m_widgets->Close();
        return;
    }
    if (FlyoutJustClosed(m_widgets.get()))
        return;
    CloseFlyouts(m_widgets.get());
    m_widgets->Open();
}

void Shell::ToggleTray()
{
    if (m_tray->IsOpen())
    {
        m_tray->Close();
        return;
    }
    if (FlyoutJustClosed(m_tray.get()))
        return;
    CloseFlyouts(m_tray.get());
    m_tray->Open();
}

void Shell::ToggleTaskView()
{
    if (m_taskView->IsOpen())
    {
        m_taskView->Close(nullptr);
        return;
    }
    CloseFlyouts();
    m_menu.Close();
    m_taskView->Open();
}

bool Shell::TaskViewOpen() const { return m_taskView->IsOpen(); }

void Shell::ShowWinXMenu(Point at)
{
    std::vector<MenuItem> power = {
        MenuItem(L"Выход", Icon::SignOut, []() { GetShell().Lock(); }),
        MenuItem(L"Спящий режим", Icon::Sleep, []() { GetShell().Sleep(); }),
        MenuItem(L"Завершение работы", Icon::Power, []() { GetShell().Shutdown(false); }),
        MenuItem(L"Перезагрузка", Icon::Restart, []() { GetShell().Shutdown(true); }),
    };
    std::vector<MenuItem> items = {
        MenuItem(L"Установленные приложения", Icon::Apps, []() { GetShell().Launch(AppId::Settings, L"apps"); }),
        MenuItem(L"Управление электропитанием", Icon::Battery2, []() { GetShell().Launch(AppId::Settings, L"power"); }),
        MenuItem(L"Просмотр событий", Icon::None, nullptr).Disabled(),
        MenuItem(L"Система", Icon::Info, []() { GetShell().Launch(AppId::Settings, L"about"); }),
        MenuItem(L"Диспетчер устройств", Icon::None, nullptr).Disabled(),
        MenuItem(L"Сетевые подключения", Icon::Globe, []() { GetShell().Launch(AppId::Settings, L"network"); }),
        MenuItem(L"Управление дисками", Icon::Storage, []() { GetShell().Launch(AppId::Settings, L"storage"); }),
        MenuItem(L"Управление компьютером", Icon::None, nullptr).Disabled(),
        MenuItem(L"Терминал", Icon::Terminal, []() { GetShell().Launch(AppId::Terminal); }),
        MenuItem(L"Терминал (администратор)", Icon::Shield, []() { GetShell().Launch(AppId::Terminal, L"::admin"); }),
        MenuItem::Sep(),
        MenuItem(L"Диспетчер задач", Icon::Performance, []() { GetShell().Launch(AppId::TaskManager); }),
        MenuItem(L"Параметры", Icon::Settings, []() { GetShell().Launch(AppId::Settings); }),
        MenuItem(L"Проводник", Icon::Folder, []() { GetShell().Launch(AppId::Explorer); }),
        MenuItem(L"Найти", Icon::Search, []() { GetShell().ToggleStart(true); }),
        MenuItem(L"Выполнить", Icon::Open, []() { GetShell().Launch(AppId::Terminal); }),
        MenuItem::Sep(),
        MenuItem::Sub(L"Завершение работы или выход из системы", Icon::Power, power),
        MenuItem(L"Рабочий стол", Icon::Desktop, []() { GetShell().WM().ShowDesktop(); }),
    };
    CloseFlyouts();
    ShowMenu(at, std::move(items));
}

void Shell::ShowShutdownDialog()
{
    if (Window* w = m_wm->FindApp(AppId::System))
    {
        if (w->Title() == L"Завершение работы Windows")
        {
            m_wm->Activate(w);
            return;
        }
    }
    m_wm->Add(CreateShutdownDialog());
}

void Shell::ShowMessage(const std::wstring& title, const std::wstring& text, AppIcon icon)
{
    m_wm->Add(CreateMessageBox(title, text, icon));
}

void Shell::ShowProperties(FsNode* node)
{
    if (node)
        m_wm->Add(CreateProperties(node));
}

void Shell::SetDark(bool dark)
{
    Theme::SetDark(dark);
    if (dark && m_settings.wallpaper == 0)
        SetWallpaper(1);
    else if (!dark && m_settings.wallpaper == 1)
        SetWallpaper(0);
    Invalidate();
}

void Shell::SetAccent(Color c)
{
    Theme::SetAccent(c);
    Invalidate();
}

void Shell::SetTransparency(bool on)
{
    Theme::SetTransparency(on);
    Invalidate();
}

void Shell::SetAccentTitleBars(bool on)
{
    Theme::SetAccentTitleBars(on);
    Invalidate();
}

void Shell::SetWallpaper(int index)
{
    m_settings.wallpaper = index;
    std::vector<uint32_t> px;
    RenderWallpaper(index, m_r.PixelWidth(), m_r.PixelHeight(), px);
    m_r.SetWallpaper(std::move(px), m_r.PixelWidth(), m_r.PixelHeight());
    Invalidate();
}

void Shell::SetAnimations(bool on)
{
    SetAnimationsEnabled(on);
    Invalidate();
}

void Shell::ApplyNightLight() { Invalidate(); }

void Shell::Notify(AppIcon icon, const std::wstring& app, const std::wstring& title, const std::wstring& body)
{
    Notification n;
    n.icon = icon;
    n.app = app;
    n.title = title;
    n.body = body;
    n.time = LocalNow();
    m_nc->Add(n);
    if (!m_settings.focus || title.find(L"фокусировки") != std::wstring::npos)
        m_toasts->Show(n);
    m_taskbar->SetUnread(true);
    Invalidate();
}

void Shell::Lock() { m_screens->Lock(); }
void Shell::Sleep() { m_screens->Sleep(); }
void Shell::Shutdown(bool restart) { m_screens->Shutdown(restart); }

void Shell::TakeScreenshot()
{
    const int w = m_r.PixelWidth(), h = m_r.PixelHeight();
    const uint32_t* src = m_r.Bits();
    if (!src || w <= 0 || h <= 0)
        return;
    auto bmp = std::make_shared<Gdiplus::Bitmap>(w, h, PixelFormat32bppRGB);
    Gdiplus::BitmapData data{};
    Gdiplus::Rect rc(0, 0, w, h);
    if (bmp->LockBits(&rc, Gdiplus::ImageLockModeWrite, PixelFormat32bppRGB, &data) == Gdiplus::Ok)
    {
        for (int y = 0; y < h; ++y)
            std::memcpy(static_cast<BYTE*>(data.Scan0) + static_cast<ptrdiff_t>(y) * data.Stride,
                        src + static_cast<size_t>(y) * static_cast<size_t>(w), static_cast<size_t>(w) * 4);
        bmp->UnlockBits(&data);
    }
    // В системный буфер обмена (CF_DIB, строки снизу вверх).
    if (OpenClipboard(m_hwnd))
    {
        EmptyClipboard();
        const size_t bytes = sizeof(BITMAPINFOHEADER) + static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (mem)
        {
            auto* bi = static_cast<BITMAPINFOHEADER*>(GlobalLock(mem));
            if (bi)
            {
                *bi = BITMAPINFOHEADER{};
                bi->biSize = sizeof(BITMAPINFOHEADER);
                bi->biWidth = w;
                bi->biHeight = h;
                bi->biPlanes = 1;
                bi->biBitCount = 32;
                bi->biCompression = BI_RGB;
                auto* dst = reinterpret_cast<uint32_t*>(bi + 1);
                for (int y = 0; y < h; ++y)
                    std::memcpy(dst + static_cast<size_t>(h - 1 - y) * static_cast<size_t>(w),
                                src + static_cast<size_t>(y) * static_cast<size_t>(w), static_cast<size_t>(w) * 4);
                GlobalUnlock(mem);
                if (!SetClipboardData(CF_DIB, mem))
                    GlobalFree(mem);
            }
            else
                GlobalFree(mem);
        }
        CloseClipboard();
    }
    const SYSTEMTIME st = LocalNow();
    const std::wstring name = L"Снимок экрана " + std::to_wstring(st.wYear) + L"-" + Pad2(st.wMonth) + L"-" + Pad2(st.wDay) +
                              L" " + Pad2(st.wHour) + Pad2(st.wMinute) + Pad2(st.wSecond) + L".png";
    FsNode* n = m_fs->NewFile(m_fs->Screenshots(), name, FileKind::Image);
    n->bitmap = bmp;
    n->size = static_cast<unsigned long long>(w) * static_cast<unsigned long long>(h) * 3ULL / 4ULL;
    Notify(AppIcon::Photos, L"Ножницы", L"Снимок экрана скопирован в буфер обмена",
           L"Он также сохранён в папке «Изображения\\Снимки экрана».");
}
