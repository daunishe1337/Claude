#pragma once
// Базовый класс элемента интерфейса (Draw, обработка мыши/клавиатуры, hit-test)
// и контейнер Panel, который маршрутизирует события дочерним элементам.

#include "Renderer.h"

class Widget
{
public:
    Widget() = default;
    virtual ~Widget() = default;
    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;

    // ---- Геометрия и состояние -------------------------------------------
    const Rect& Bounds() const { return m_bounds; }
    void SetBounds(const Rect& r)
    {
        m_bounds = r;
        OnLayout();
    }
    bool Visible() const { return m_visible; }
    void SetVisible(bool v) { m_visible = v; }
    bool Enabled() const { return m_enabled; }
    void SetEnabled(bool e) { m_enabled = e; }
    bool Hovered() const { return m_hovered; }
    bool Pressed() const { return m_pressed; }
    bool Focused() const { return m_focused; }
    const std::wstring& Tooltip() const { return m_tooltip; }
    void SetTooltip(std::wstring t) { m_tooltip = std::move(t); }

    // ---- Отрисовка ---------------------------------------------------------
    virtual void Draw(Renderer& r) = 0;

    // ---- Попадание ----------------------------------------------------------
    virtual bool HitTest(Point p) const { return m_visible && m_bounds.Contains(p); }

    // ---- Мышь (координаты в системе родителя) --------------------------------
    virtual void OnMouseEnter() { m_hovered = true; }
    virtual void OnMouseLeave() { m_hovered = false; }
    virtual void OnMouseMove(const MouseEvent& /*e*/) {}
    virtual bool OnMouseDown(const MouseEvent& /*e*/) { return false; }
    virtual void OnMouseUp(const MouseEvent& /*e*/) {}
    virtual bool OnMouseWheel(const MouseEvent& /*e*/) { return false; }
    virtual CursorType Cursor(Point /*p*/) const { return CursorType::Arrow; }

    // ---- Клавиатура ----------------------------------------------------------
    virtual bool Focusable() const { return false; }
    virtual void OnFocusChanged(bool focused) { m_focused = focused; }
    virtual bool OnKeyDown(UINT /*vk*/, const KeyMods& /*mods*/) { return false; }
    virtual bool OnChar(wchar_t /*ch*/) { return false; }

    // Самый глубокий элемент под курсором (для подсказок).
    virtual Widget* HoverLeaf() { return m_hovered ? this : nullptr; }

    // Смещение системы координат текущего окна относительно экрана (для всплывающих меню).
    static Point& Origin()
    {
        static Point origin;
        return origin;
    }

protected:
    virtual void OnLayout() {}

    Rect m_bounds;
    bool m_visible = true;
    bool m_enabled = true;
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_focused = false;
    std::wstring m_tooltip;
};

// Контейнер: владеет дочерними элементами, отслеживает наведение, захват мыши и фокус.
class Panel : public Widget
{
public:
    template <class T, class... Args>
    T* Add(Args&&... args)
    {
        auto p = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = p.get();
        m_children.push_back(std::move(p));
        return raw;
    }

    void Clear()
    {
        m_hover = nullptr;
        m_capture = nullptr;
        m_focus = nullptr;
        m_children.clear();
    }

    void Remove(Widget* w)
    {
        if (m_hover == w)
            m_hover = nullptr;
        if (m_capture == w)
            m_capture = nullptr;
        if (m_focus == w)
            m_focus = nullptr;
        m_children.erase(std::remove_if(m_children.begin(), m_children.end(),
                                        [w](const std::unique_ptr<Widget>& c) { return c.get() == w; }),
                         m_children.end());
    }

    const std::vector<std::unique_ptr<Widget>>& Children() const { return m_children; }

    void Draw(Renderer& r) override
    {
        for (auto& c : m_children)
        {
            if (c->Visible())
                c->Draw(r);
        }
    }

    bool HitTest(Point p) const override
    {
        if (!m_visible)
            return false;
        if (!m_bounds.Empty())
            return m_bounds.Contains(p);
        return ChildAt(p) != nullptr;
    }

    void OnMouseLeave() override
    {
        Widget::OnMouseLeave();
        if (m_hover && !m_capture)
        {
            m_hover->OnMouseLeave();
            m_hover = nullptr;
        }
    }

    void OnMouseMove(const MouseEvent& e) override
    {
        if (m_capture)
        {
            const bool inside = m_capture->HitTest(e.pos);
            if (inside != m_capture->Hovered())
            {
                if (inside)
                    m_capture->OnMouseEnter();
                else
                    m_capture->OnMouseLeave();
            }
            m_capture->OnMouseMove(e);
            return;
        }
        Widget* hit = ChildAt(e.pos);
        if (hit != m_hover)
        {
            if (m_hover)
                m_hover->OnMouseLeave();
            m_hover = hit;
            if (m_hover)
                m_hover->OnMouseEnter();
        }
        if (m_hover)
            m_hover->OnMouseMove(e);
    }

    bool OnMouseDown(const MouseEvent& e) override
    {
        Widget* hit = ChildAt(e.pos);
        if (!hit)
        {
            SetFocus(nullptr);
            return false;
        }
        if (hit != m_hover)
        {
            if (m_hover)
                m_hover->OnMouseLeave();
            m_hover = hit;
            hit->OnMouseEnter();
        }
        if (hit->Focusable() || dynamic_cast<Panel*>(hit) != nullptr)
            SetFocus(hit);
        else
            SetFocus(nullptr);
        m_capture = hit;
        hit->OnMouseDown(e);
        return true;
    }

    void OnMouseUp(const MouseEvent& e) override
    {
        Widget* cap = m_capture;
        m_capture = nullptr;
        if (cap)
            cap->OnMouseUp(e);
        // Обновить наведение после отпускания.
        Widget* hit = ChildAt(e.pos);
        if (hit != m_hover)
        {
            if (m_hover)
                m_hover->OnMouseLeave();
            m_hover = hit;
            if (m_hover)
                m_hover->OnMouseEnter();
        }
    }

    bool OnMouseWheel(const MouseEvent& e) override
    {
        Widget* hit = ChildAt(e.pos);
        return hit ? hit->OnMouseWheel(e) : false;
    }

    CursorType Cursor(Point p) const override
    {
        if (m_capture)
            return m_capture->Cursor(p);
        Widget* hit = ChildAt(p);
        return hit ? hit->Cursor(p) : CursorType::Arrow;
    }

    bool Focusable() const override { return false; }

    void OnFocusChanged(bool focused) override
    {
        Widget::OnFocusChanged(focused);
        if (!focused)
            SetFocus(nullptr);
    }

    bool OnKeyDown(UINT vk, const KeyMods& mods) override { return m_focus ? m_focus->OnKeyDown(vk, mods) : false; }
    bool OnChar(wchar_t ch) override { return m_focus ? m_focus->OnChar(ch) : false; }

    Widget* HoverLeaf() override
    {
        if (m_hover)
            return m_hover->HoverLeaf();
        return nullptr;
    }

    void SetFocus(Widget* w)
    {
        if (m_focus == w)
            return;
        if (m_focus)
            m_focus->OnFocusChanged(false);
        m_focus = w;
        if (m_focus)
            m_focus->OnFocusChanged(true);
    }
    Widget* FocusedChild() const { return m_focus; }
    bool HasCapture() const { return m_capture != nullptr; }

protected:
    Widget* ChildAt(Point p) const
    {
        for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
        {
            Widget* c = it->get();
            if (c->Visible() && c->Enabled() && c->HitTest(p))
                return c;
        }
        return nullptr;
    }

    std::vector<std::unique_ptr<Widget>> m_children;
    Widget* m_hover = nullptr;
    Widget* m_capture = nullptr;
    Widget* m_focus = nullptr;
};
