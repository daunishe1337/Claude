#pragma once
// Панель задач Windows 11: «Пуск», поиск, представление задач, закреплённые и запущенные
// приложения с индикаторами, миниатюры окон, системный трей, часы.

#include "Shell.h"

void DrawStartLogo(Renderer& r, const Rect& rc);

class Taskbar
{
public:
    static constexpr float kHeight = 48.0f;

    Taskbar();

    void Layout();
    void Draw(Renderer& r);
    void DrawPreview(Renderer& r);

    bool HitTest(Point p) const;
    void MouseMove(const MouseEvent& e);
    void MouseDown(const MouseEvent& e);
    void MouseUp(const MouseEvent& e);
    bool MouseWheel(const MouseEvent& e);
    void MouseLeave();
    CursorType Cursor(Point p) const;

    Point ButtonCenter(const Window& w) const;
    Rect StartButtonRect() const;
    Rect SearchRect() const;
    Rect ClockRect() const;
    Rect QuickRect() const;
    Rect ChevronRect() const;
    Rect WidgetsRect() const;
    float Top() const;

    bool IsPinned(AppId app) const;
    void Pin(AppId app);
    void Unpin(AppId app);
    bool HasUnread() const { return m_unread; }
    void SetUnread(bool u) { m_unread = u; }
    const std::wstring& Language() const { return m_lang; }

private:
    enum class Kind
    {
        Start,
        Search,
        TaskView,
        Widgets,
        App,
        Chevron,
        Lang,
        Quick,
        Clock,
        ShowDesktop,
    };
    struct Item
    {
        Kind kind = Kind::App;
        AppId app = AppId::None;
        Rect rect;
    };
    struct AppState
    {
        Tween press{1.0f};
        Tween indicator{0.0f};
        Tween appear{1.0f};
    };

    int ItemAt(Point p) const;
    void Click(const Item& it);
    void RightClick(const Item& it, Point p);
    std::vector<Window*> AppWindows(AppId app) const;
    AppState& State(AppId app);
    void DrawItem(Renderer& r, const Item& it, int index);
    std::wstring TooltipFor(const Item& it) const;

    // Миниатюры
    Rect PreviewRect() const;
    Rect PreviewCard(int index) const;
    int PreviewCardAt(Point p, bool* onClose) const;

    std::vector<AppId> m_pinned;
    std::vector<Item> m_items;
    std::map<AppId, AppState> m_states;
    int m_hover = -1;
    int m_press = -1;
    bool m_unread = true;
    std::wstring m_lang = L"РУС";

    AppId m_previewApp = AppId::None;
    Rect m_previewAnchor;
    bool m_previewVisible = false;
    double m_hoverStart = 0.0;
    double m_previewLeave = 0.0;
    Tween m_previewAnim;
    int m_previewHover = -1;
    bool m_previewCloseHover = false;
    bool m_pressInPreview = false;
};
