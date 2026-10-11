#pragma once
// Контекстное меню Windows 11: акриловый фон, скругление, иконки, подменю,
// флажки/переключатели, строка быстрых действий (вырезать, копировать...).

#include "Renderer.h"

struct MenuItem
{
    std::wstring text;
    Icon icon = Icon::None;
    std::wstring shortcut;
    std::function<void()> action;
    std::vector<MenuItem> submenu;
    bool separator = false;
    bool enabled = true;
    bool checked = false;
    bool radio = false;
    bool pill = false; // выбранный элемент выпадающего списка (акцентная полоска)

    MenuItem() = default;
    MenuItem(std::wstring caption, Icon glyph, std::function<void()> fn, std::wstring keys = L"")
        : text(std::move(caption)), icon(glyph), shortcut(std::move(keys)), action(std::move(fn))
    {
    }

    static MenuItem Sep()
    {
        MenuItem m;
        m.separator = true;
        return m;
    }
    static MenuItem Sub(std::wstring caption, Icon glyph, std::vector<MenuItem> items)
    {
        MenuItem m(std::move(caption), glyph, nullptr);
        m.submenu = std::move(items);
        return m;
    }
    static MenuItem Check(std::wstring caption, bool on, std::function<void()> fn, bool isRadio = false,
                          std::wstring keys = L"")
    {
        MenuItem m(std::move(caption), Icon::None, std::move(fn), std::move(keys));
        m.checked = on;
        m.radio = isRadio;
        return m;
    }
    MenuItem& Disabled(bool d = true)
    {
        enabled = !d;
        return *this;
    }
};

struct MenuAction
{
    Icon icon = Icon::None;
    std::wstring tooltip;
    std::function<void()> action;
};

class ContextMenu
{
public:
    // Открыть в точке экрана (меню разворачивается вправо-вниз с учётом краёв экрана).
    void Open(Point screenPos, std::vector<MenuItem> items, std::vector<MenuAction> top = {}, bool classic = false,
              float minWidth = 0.0f);
    // Открыть под прямоугольником-якорем (выпадающие меню «Файл», «Создать» и т.п.).
    void OpenBelow(const Rect& anchor, std::vector<MenuItem> items, float minWidth = 0.0f, bool above = false);
    void Close();
    bool IsOpen() const { return !m_levels.empty(); }
    void SetScreen(float w, float h)
    {
        m_screenW = w;
        m_screenH = h;
    }

    void Draw(Renderer& r);
    bool HitTest(Point p) const;
    void OnMouseMove(Point p);
    void OnMouseDown(Point p);
    void OnMouseUp(Point p);
    bool OnKeyDown(UINT vk);
    const MenuAction* HoveredAction(Rect& anchor) const;

private:
    struct Level
    {
        std::vector<MenuItem> items;
        std::vector<MenuAction> top;
        Rect rect;
        int hover = -1;
        int topHover = -1;
        int parentItem = -1;
        Tween anim;
        bool upward = false;
    };

    float ItemHeight(const MenuItem& m) const;
    float TopRowHeight(const Level& l) const;
    Rect ItemRect(const Level& l, int index) const;
    Rect TopRect(const Level& l, int index) const;
    void Measure(Level& l, float minWidth);
    void Place(Level& l, float x, float y, bool allowUp);
    void OpenSub(int levelIndex, int item);
    int LevelAt(Point p) const;
    int ItemAt(const Level& l, Point p) const;
    int TopAt(const Level& l, Point p) const;
    void Activate(int levelIndex, int item);
    void MoveHover(int dir);

    std::vector<Level> m_levels;
    float m_screenW = 1920.0f;
    float m_screenH = 1080.0f;
    bool m_classic = false;
    int m_pendingLevel = -1;
    int m_pendingItem = -1;
    double m_pendingTime = 0.0;
};

// Выпадающий список ComboBox (реализация в Shell.cpp — использует общее меню оболочки).
void ShowDropdown(const Rect& screenAnchor, const std::vector<std::wstring>& items, int selected,
                  std::function<void(int)> onSelect);
