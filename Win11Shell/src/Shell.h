#pragma once
// Корневой объект оболочки: владеет рабочим столом, панелью задач, меню «Пуск»,
// всплывающими панелями и оконным менеджером; маршрутизирует ввод по слоям.

#include "ContextMenu.h"
#include "VirtualFS.h"
#include "WindowManager.h"

class Desktop;
class Taskbar;
class StartMenu;
class QuickSettings;
class NotificationCenter;
class WidgetsBoard;
class TrayOverflow;
class Toasts;
class TaskView;
class SystemScreens;
class Flyout;

struct ShellSettings
{
    bool taskbarCenter = true;
    int searchMode = 2;          // 0 — скрыть, 1 — значок, 2 — поле поиска
    bool showTaskView = true;
    bool showWidgets = true;
    float brightness = 1.0f;
    float volume = 0.62f;
    bool muted = false;
    bool wifi = true;
    bool bluetooth = true;
    bool airplane = false;
    bool batterySaver = false;
    bool nightLight = false;
    bool focus = false;
    float battery = 0.87f;
    bool charging = true;
    int wallpaper = 0;
    bool startRecentApps = true;
    bool startRecommended = true;
    int desktopIconSize = 1;     // 0 — мелкие, 1 — обычные, 2 — крупные
    bool showDesktopIcons = true;
    bool autoArrange = false;
    bool alignToGrid = true;
    bool showExtensions = false; // расширения имён файлов (по умолчанию скрыты, как в Windows)
    std::wstring network = L"HomeNet-5G";
    std::wstring computerName = L"DESKTOP-W11DEMO";
};

class Shell
{
public:
    Shell(HWND hwnd, Renderer& r);
    ~Shell();
    Shell(const Shell&) = delete;
    Shell& operator=(const Shell&) = delete;

    // ---- Кадр ----------------------------------------------------------------
    void Resize(int pxWidth, int pxHeight, float scale);
    void Paint(HDC hdc);
    void Invalidate();
    bool WantsFrame() const { return m_wantFrame; }

    // ---- Ввод (DIP) ------------------------------------------------------------
    void MouseMove(Point p);
    void MouseDown(Point p, MouseButton b, int clicks);
    void MouseUp(Point p, MouseButton b);
    void MouseWheel(Point p, float notches);
    void MouseLeftWindow();
    void KeyDown(UINT vk, bool repeat);
    void KeyUp(UINT vk);
    void Char(wchar_t ch);
    void WinKey();
    void WinCombo(UINT vk);
    void Timer(UINT_PTR id);
    HCURSOR CursorAt(Point p);
    void CaptureLost();

    // ---- Компоненты --------------------------------------------------------------
    Renderer& R() { return m_r; }
    WindowManager& WM() { return *m_wm; }
    Desktop& Desk() { return *m_desktop; }
    Taskbar& TB() { return *m_taskbar; }
    StartMenu& Start() { return *m_start; }
    QuickSettings& QS() { return *m_qs; }
    NotificationCenter& NC() { return *m_nc; }
    ContextMenu& Menu() { return m_menu; }
    VirtualFS& FS() { return *m_fs; }
    ShellSettings& Settings() { return m_settings; }
    HWND Hwnd() const { return m_hwnd; }
    float ScreenW() const { return m_r.Width(); }
    float ScreenH() const { return m_r.Height(); }
    Rect WorkArea() const;
    Point MousePos() const { return m_mouse; }

    // ---- Действия ----------------------------------------------------------------
    Window* Launch(AppId id, const std::wstring& arg = L"");
    void Open(FsNode* node);
    void ShowMenu(Point screen, std::vector<MenuItem> items, std::vector<MenuAction> top = {}, bool classic = false);
    void ShowMenuBelow(const Rect& anchor, std::vector<MenuItem> items, float minWidth = 0.0f, bool above = false);
    void CloseFlyouts(const Flyout* except = nullptr);
    void ToggleStart(bool search = false);
    void ToggleQuickSettings();
    void ToggleNotifications();
    void ToggleWidgets();
    void ToggleTray();
    void ToggleTaskView();
    bool TaskViewOpen() const;
    bool FlyoutJustClosed(const Flyout* f) const;
    void ShowWinXMenu(Point at);
    void ShowShutdownDialog();
    void ShowMessage(const std::wstring& title, const std::wstring& text, AppIcon icon = AppIcon::None);
    void ShowProperties(FsNode* node);

    void SetDark(bool dark);
    void SetAccent(Color c);
    void SetTransparency(bool on);
    void SetAccentTitleBars(bool on);
    void SetWallpaper(int index);
    void SetAnimations(bool on);
    void ApplyNightLight();

    void Notify(AppIcon icon, const std::wstring& app, const std::wstring& title, const std::wstring& body);
    void Tooltip(const void* owner, const std::wstring& text, const Rect& anchor, bool above = true);
    void Lock();
    void Sleep();
    void Shutdown(bool restart);
    void TakeScreenshot();

private:
    enum class Layer
    {
        None,
        Screens,
        Menu,
        Flyout,
        Toast,
        TaskView,
        Taskbar,
        Windows,
        Desktop,
    };
    Layer LayerAt(Point p, Flyout** flyout) const;
    void RouteMove(const MouseEvent& e);
    void LeaveLayer(Layer l, Flyout* f);
    void AfterInput();
    void DrawTooltip(Renderer& r);
    bool GlobalKey(UINT vk, const KeyMods& mods);
    std::vector<Flyout*> Flyouts() const;

    HWND m_hwnd;
    Renderer& m_r;
    std::unique_ptr<VirtualFS> m_fs;
    std::unique_ptr<WindowManager> m_wm;
    std::unique_ptr<Desktop> m_desktop;
    std::unique_ptr<Taskbar> m_taskbar;
    std::unique_ptr<StartMenu> m_start;
    std::unique_ptr<QuickSettings> m_qs;
    std::unique_ptr<NotificationCenter> m_nc;
    std::unique_ptr<WidgetsBoard> m_widgets;
    std::unique_ptr<TrayOverflow> m_tray;
    std::unique_ptr<Toasts> m_toasts;
    std::unique_ptr<TaskView> m_taskView;
    std::unique_ptr<SystemScreens> m_screens;
    ContextMenu m_menu;
    ShellSettings m_settings;

    bool m_initialized = false;
    bool m_wantFrame = false;
    Point m_mouse;
    Layer m_capture = Layer::None;
    Flyout* m_captureFlyout = nullptr;
    Layer m_hoverLayer = Layer::None;
    Flyout* m_hoverFlyout = nullptr;
    unsigned m_clickSerial = 0;
    std::vector<std::pair<const Flyout*, unsigned>> m_closedByClick;
    bool m_buttonsDown = false;
    bool m_suppressUp = false;

    // Подсказка
    const void* m_tipOwner = nullptr;
    std::wstring m_tipText;
    Rect m_tipAnchor;
    bool m_tipAbove = true;
    double m_tipStart = 0.0;
    bool m_tipTouched = false;
    const void* m_tipBlockOwner = nullptr; // подсказка, скрытая щелчком (не показывать снова)
    std::wstring m_tipBlockText;
    int m_lastMinute = -1;
};

Shell& GetShell();
