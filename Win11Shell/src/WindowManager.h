#pragma once
// Оконный менеджер: «окна» — это объекты, рисуемые на общем холсте.
// Перетаскивание, изменение размера, свернуть/развернуть/закрыть, привязка к краям (snap),
// макеты прикрепления, порядок по Z, фокус и анимации открытия/сворачивания.

#include "Apps.h"
#include "Controls.h"

enum class WindowState
{
    Normal,
    Maximized,
    Minimized,
};

enum class SnapZone
{
    None,
    Left,
    Right,
    Top, // развернуть
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

struct DialogButton
{
    std::wstring text;
    bool accent = false;
};

class Window
{
public:
    Window(AppId app, std::wstring title);
    virtual ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // ---- Описание ----------------------------------------------------------
    AppId App() const { return m_app; }
    const std::wstring& Title() const { return m_title; }
    void SetTitle(std::wstring t) { m_title = std::move(t); }
    virtual AppIcon WindowIcon() const;
    bool ShowInTaskbar() const { return m_showInTaskbar; }
    unsigned Id() const { return m_id; }

    // ---- Геометрия (экранные DIP) -------------------------------------------
    const Rect& Frame() const { return m_frame; }
    Rect VisualFrame() const;
    float Width() const { return m_layoutW; }
    float Height() const { return m_layoutH; }
    bool IsMaximized() const { return m_state == WindowState::Maximized; }
    bool IsMinimized() const { return m_state == WindowState::Minimized; }
    bool IsSnapped() const { return m_snapped; }
    bool IsActive() const { return m_active; }
    bool IsClosing() const { return m_closing; }
    bool Resizable() const { return m_resizable; }
    Point MinSize() const { return m_minSize; }
    Rect ClientRect() const { return {0.0f, TitleBarHeight(), m_layoutW, m_layoutH - TitleBarHeight()}; }
    Point LocalToScreen(Point p) const { return {p.x + m_frame.x, p.y + m_frame.y}; }
    Rect LocalToScreen(const Rect& r) const { return r.Translated(m_frame.x, m_frame.y); }

    // ---- Переопределяемое приложением ----------------------------------------
    virtual float TitleBarHeight() const { return 32.0f; }
    virtual bool ShowTitle() const { return true; }       // иконка и заголовок в строке заголовка
    virtual bool DarkContent() const { return false; }    // тёмное содержимое (терминал)
    virtual void DrawContent(Renderer& r, const Rect& client) = 0;
    virtual bool IsDragArea(Point local) const;           // можно ли тащить окно за эту точку
    virtual void OnResize() {}
    virtual void OnActivate(bool /*active*/) {}
    virtual bool OnClose() { return true; }                // false — отменить закрытие
    virtual void OnTimer1s() {}
    virtual void OnLaunchArgs(const std::wstring& /*arg*/) {} // повторный запуск однократного приложения
    virtual void OnMouseDown(const MouseEvent& e);
    virtual void OnMouseMove(const MouseEvent& e);
    virtual void OnMouseUp(const MouseEvent& e);
    virtual bool OnMouseWheel(const MouseEvent& e);
    virtual void OnMouseLeave();
    virtual bool OnKeyDown(UINT vk, const KeyMods& mods);
    virtual bool OnChar(wchar_t ch);
    virtual CursorType Cursor(Point local) const;

    // ---- Сервисы ------------------------------------------------------------
    void Close();      // с вызовом OnClose()
    void ForceClose(); // без вопросов
    void Activate();
    void ShowDialog(std::wstring title, std::wstring message, std::vector<DialogButton> buttons,
                    std::function<void(int)> onResult);
    bool HasDialog() const { return m_dialogActive; }
    Panel& UI() { return m_ui; }

protected:
    void SetInitialSize(float w, float h);
    void UpdateLayoutSize(float w, float h);
    void DrawDialog(Renderer& r);

    Panel m_ui;
    bool m_resizable = true;
    bool m_canMinimize = true;
    bool m_canMaximize = true;
    Point m_minSize{360.0f, 240.0f};
    bool m_showInTaskbar = true;

private:
    friend class WindowManager;

    AppId m_app;
    std::wstring m_title;
    unsigned m_id;
    Rect m_frame;
    Rect m_restoreFrame;
    WindowState m_state = WindowState::Normal;
    WindowState m_stateBeforeMin = WindowState::Normal;
    bool m_snapped = false;
    bool m_active = false;
    bool m_closing = false;
    float m_layoutW = 0.0f;
    float m_layoutH = 0.0f;

    // Анимации
    Tween m_open;       // 0 → 1 появление, 1 → 0 закрытие
    Tween m_min;        // 0 — обычное, 1 — свёрнуто
    Tween m_morph;      // 0 → 1 переход геометрии
    Rect m_morphFrom;
    int m_captionHover = -1; // 0 свернуть, 1 развернуть, 2 закрыть
    int m_captionPress = -1;
    bool m_clientHover = false;

    // Диалог внутри окна
    bool m_dialogActive = false;
    std::wstring m_dialogTitle;
    std::wstring m_dialogText;
    std::function<void(int)> m_dialogResult;
    Panel m_dialogUi;
    Rect m_dialogRect;
    Tween m_dialogAnim;
    void LayoutDialog();
};

class WindowManager
{
public:
    WindowManager() = default;

    Window* Add(std::unique_ptr<Window> w);
    void Activate(Window* w);
    void Deactivate(); // снять фокус со всех окон (щелчок по рабочему столу)
    void Minimize(Window* w);
    void Restore(Window* w);
    void ToggleMaximize(Window* w);
    void Maximize(Window* w);
    void SnapTo(Window* w, SnapZone z);
    void SnapToRect(Window* w, const Rect& r);
    void Close(Window* w);
    void ForceClose(Window* w);
    void ShowDesktop();
    void MinimizeAll();
    void CloseAll();
    void SnapActive(UINT vk); // Win + стрелки

    Window* Active() const { return m_active; }
    const std::vector<std::unique_ptr<Window>>& Windows() const { return m_windows; }
    std::vector<Window*> WindowsOf(AppId app) const;
    Window* FindApp(AppId app) const;
    bool Contains(const Window* w) const;
    bool AnyVisible() const;
    Rect ZoneRect(SnapZone z) const;

    // ---- Отрисовка ----------------------------------------------------------
    void Update(); // удалить закрытые окна
    void Draw(Renderer& r);
    void DrawOverlays(Renderer& r); // предпросмотр привязки, макеты прикрепления
    void DrawWindow(Renderer& r, Window& w, bool thumbnail = false);
    void DrawThumbnail(Renderer& r, Window& w, const Rect& target);

    // ---- Ввод (экранные координаты) -----------------------------------------
    bool HitTest(Point p) const;
    void MouseDown(const MouseEvent& e);
    void MouseMove(const MouseEvent& e);
    void MouseUp(const MouseEvent& e);
    bool MouseWheel(const MouseEvent& e);
    void MouseLeave();
    bool KeyDown(UINT vk, const KeyMods& mods);
    bool Char(wchar_t ch);
    CursorType Cursor(Point p) const;
    bool Dragging() const { return m_drag.mode != DragMode::None; }
    void CancelDrag();

private:
    enum class Zone
    {
        None,
        Client,
        Caption,
        MinBtn,
        MaxBtn,
        CloseBtn,
        Left,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
    };
    enum class DragMode
    {
        None,
        Move,
        Resize,
        Client,
        CaptionButton,
        Dialog,
    };
    struct Drag
    {
        DragMode mode = DragMode::None;
        Window* win = nullptr;
        Zone zone = Zone::None;
        Point start;
        Rect startFrame;
        bool moved = false;
        int button = -1;
    };
    struct Layout
    {
        std::vector<Rect> zones; // доли рабочей области
    };

    Zone HitZone(const Window& w, Point p) const;
    Window* WindowAt(Point p, Zone* zone = nullptr) const;
    Rect CaptionButtonRect(const Window& w, int index) const;
    void BeginMorph(Window* w, const Rect& target);
    void ActivateTopmost();
    SnapZone DetectSnap(Point p) const;
    void UpdatePreview(SnapZone z, const Rect& from);
    Point MinimizeTarget(const Window& w) const;
    MouseEvent ToLocal(const Window& w, const MouseEvent& e) const;
    void ShowSystemMenu(Window* w, Point at);

    // Макеты прикрепления
    const std::vector<Layout>& Layouts() const;
    Rect SnapFlyoutRect() const;
    void SnapFlyoutHit(Point p, int& layout, int& zone) const;
    Rect LayoutPreviewRect(int index) const;

    std::vector<std::unique_ptr<Window>> m_windows;
    Window* m_active = nullptr;
    Window* m_hoverWin = nullptr;
    Drag m_drag;
    unsigned m_cascade = 0;
    std::vector<Window*> m_shownBeforeDesktop;

    // Предпросмотр привязки
    SnapZone m_preview = SnapZone::None;
    Rect m_previewFrom;
    Rect m_previewTo;
    Tween m_previewAnim;
    Tween m_previewAlpha;

    // Всплывающее окно макетов
    Window* m_snapWin = nullptr;
    double m_maxHoverStart = 0.0;
    bool m_maxHovering = false;
    Tween m_snapAnim;
    bool m_snapVisible = false;
    double m_snapLeaveTime = 0.0;
    int m_snapHoverLayout = -1;
    int m_snapHoverZone = -1;
};
