#pragma once
// Всплывающие панели: базовый класс Flyout, быстрые настройки, центр уведомлений с календарём,
// скрытые значки трея, мини-приложения и всплывающие уведомления (toast).

#include "Controls.h"
#include "Shell.h"

class Flyout
{
public:
    virtual ~Flyout() = default;

    bool IsOpen() const { return m_open; }
    bool IsVisible() const { return m_open || m_anim.Value() > 0.002f; }
    virtual void Open();
    virtual void Close();
    void Toggle()
    {
        if (m_open)
            Close();
        else
            Open();
    }

    virtual Rect Bounds() const = 0;
    virtual void Draw(Renderer& r) = 0;
    virtual bool HitTest(Point p) const { return m_open && Bounds().Contains(p); }
    virtual void MouseMove(const MouseEvent& e);
    virtual void MouseDown(const MouseEvent& e);
    virtual void MouseUp(const MouseEvent& e);
    virtual bool MouseWheel(const MouseEvent& e);
    virtual void MouseLeave();
    virtual bool KeyDown(UINT vk, const KeyMods& mods);
    virtual bool Char(wchar_t ch);
    virtual CursorType Cursor(Point p) const;
    Widget* HoverLeaf() { return m_ui.HoverLeaf(); }

protected:
    virtual void OnOpen() {}
    virtual void OnClose() {}
    // Фон панели: тень, акрил, обводка (с учётом анимации).
    void DrawPanel(Renderer& r, const Rect& rc, float radius = 8.0f);
    // Применить анимацию появления (сдвиг + прозрачность). Возвращает false, если панель невидима.
    bool BeginAnim(Renderer& r, float dx, float dy);
    void EndAnim(Renderer& r);

    bool m_open = false;
    Tween m_anim;
    Panel m_ui;
};

// ---------------------------------------------------------------------------
class QuickSettings : public Flyout
{
public:
    QuickSettings();
    Rect Bounds() const override;
    void Draw(Renderer& r) override;
    bool KeyDown(UINT vk, const KeyMods& mods) override;

protected:
    void OnOpen() override;

private:
    void Build();
    void BuildWifiPage();
    int m_page = 0; // 0 — главная, 1 — сети Wi-Fi
    int m_expandedNet = -1;
};

// ---------------------------------------------------------------------------
struct Notification
{
    AppIcon icon = AppIcon::None;
    std::wstring app;
    std::wstring title;
    std::wstring body;
    SYSTEMTIME time{};
    unsigned id = 0;
};

class NotificationCenter : public Flyout
{
public:
    NotificationCenter();
    Rect Bounds() const override;
    Rect NotifRect() const;
    Rect CalendarRect() const;
    bool HitTest(Point p) const override;
    void Draw(Renderer& r) override;
    void MouseMove(const MouseEvent& e) override;
    void MouseDown(const MouseEvent& e) override;
    void MouseUp(const MouseEvent& e) override;
    bool MouseWheel(const MouseEvent& e) override;
    void MouseLeave() override;

    void Add(const Notification& n);
    const std::vector<Notification>& Items() const { return m_items; }
    void ClearAll();

protected:
    void OnOpen() override;

private:
    float NotifHeight() const;
    Rect CardRect(int index) const;
    void Build();
    void ShiftMonth(int delta);

    std::vector<Notification> m_items;
    unsigned m_nextId = 1;
    int m_viewYear = 2026;
    int m_viewMonth = 1;
    int m_hoverDay = -1;
    int m_selDay = -1;
    int m_selMonth = 0;
    int m_selYear = 0;
    int m_hoverCard = -1;
    bool m_hoverCardClose = false;
    bool m_collapsed = false;
    int m_focusMinutes = 30;
    Tween m_monthAnim{1.0f};
    int m_monthDir = 0;
};

// ---------------------------------------------------------------------------
class TrayOverflow : public Flyout
{
public:
    TrayOverflow();
    Rect Bounds() const override;
    void Draw(Renderer& r) override;

protected:
    void OnOpen() override;
};

// ---------------------------------------------------------------------------
class WidgetsBoard : public Flyout
{
public:
    WidgetsBoard();
    Rect Bounds() const override;
    void Draw(Renderer& r) override;
    bool MouseWheel(const MouseEvent& e) override;

protected:
    void OnOpen() override;

private:
    Tween m_scroll;
};

// ---------------------------------------------------------------------------
class Toasts
{
public:
    void Show(const Notification& n);
    void Draw(Renderer& r);
    bool HitTest(Point p) const;
    void MouseMove(const MouseEvent& e);
    void MouseDown(const MouseEvent& e);
    void MouseUp(const MouseEvent& e);
    void MouseLeave();
    bool Any() const { return !m_toasts.empty(); }

private:
    struct Toast
    {
        Notification n;
        double shown = 0.0;
        Tween anim;
        bool leaving = false;
    };
    Rect ToastRect(size_t index) const;
    std::vector<Toast> m_toasts;
    int m_hover = -1;
    bool m_hoverClose = false;
};
