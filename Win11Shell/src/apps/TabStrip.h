#pragma once
// Полоса вкладок в строке заголовка (Блокнот, Терминал): отрисовка и hit-test.

#include "../Renderer.h"

struct TabInfo
{
    std::wstring title;
    AppIcon appIcon = AppIcon::None;
    Icon glyph = Icon::None;
    bool modified = false; // точка вместо крестика (несохранённые изменения)
};

class TabStrip
{
public:
    float left = 8.0f;
    float top = 8.0f;
    float height = 32.0f;
    float maxWidth = 240.0f;
    float minWidth = 72.0f;
    float reserveRight = 46.0f * 3.0f + 56.0f; // кнопки заголовка и «+»
    float extraButtons = 0.0f;                 // дополнительные кнопки после «+» (раскрывающийся список)

    int hover = -1;
    bool hoverClose = false;
    bool hoverNew = false;
    bool hoverExtra = false;

    float TabWidth(float windowW, int count) const
    {
        const float avail = windowW - left - reserveRight - extraButtons;
        return Clamp(avail / static_cast<float>(std::max(1, count)), minWidth, maxWidth);
    }
    Rect TabRect(int i, float windowW, int count) const
    {
        const float w = TabWidth(windowW, count);
        return {left + static_cast<float>(i) * w, top, w, height};
    }
    Rect CloseRect(const Rect& tab) const { return {tab.Right() - 30.0f, tab.CenterY() - 11.0f, 22.0f, 22.0f}; }
    Rect NewRect(float windowW, int count) const
    {
        return {left + static_cast<float>(count) * TabWidth(windowW, count) + 6.0f, top + 2.0f, 34.0f, height - 4.0f};
    }
    Rect ExtraRect(float windowW, int count) const
    {
        const Rect n = NewRect(windowW, count);
        return {n.Right(), n.y, extraButtons, n.h};
    }

    // Возвращает индекс вкладки или -1; onClose — попадание в крестик.
    int HitTest(Point p, float windowW, int count, bool* onClose) const
    {
        for (int i = 0; i < count; ++i)
        {
            const Rect tr = TabRect(i, windowW, count);
            if (tr.Contains(p))
            {
                if (onClose)
                    *onClose = CloseRect(tr).Contains(p);
                return i;
            }
        }
        if (onClose)
            *onClose = false;
        return -1;
    }

    void UpdateHover(Point p, float windowW, int count)
    {
        hover = HitTest(p, windowW, count, &hoverClose);
        hoverNew = NewRect(windowW, count).Contains(p);
        hoverExtra = extraButtons > 0.0f && ExtraRect(windowW, count).Contains(p);
    }
    void ClearHover()
    {
        hover = -1;
        hoverClose = hoverNew = hoverExtra = false;
    }

    // selectedFill — цвет выбранной вкладки (совпадает с фоном области под вкладками).
    void Draw(Renderer& r, float windowW, const std::vector<TabInfo>& tabs, int current, Color selectedFill, Color text,
              Color hoverFill, Color divider) const
    {
        Draw(r, windowW, tabs, current, selectedFill, text, hoverFill, divider, text);
    }

    // selectedText — цвет подписи выбранной вкладки (у Терминала она тёмная, как фон консоли).
    void Draw(Renderer& r, float windowW, const std::vector<TabInfo>& tabs, int current, Color selectedFill, Color text,
              Color hoverFill, Color divider, Color selectedText) const
    {
        const int count = static_cast<int>(tabs.size());
        for (int i = 0; i < count; ++i)
        {
            const Rect tr = TabRect(i, windowW, count);
            const bool sel = i == current;
            const bool hot = i == hover;
            if (sel)
            {
                r.FillRoundRect(tr, 8.0f, 8.0f, 0.0f, 0.0f, selectedFill);
                // «Ушки» у основания выбранной вкладки, как в Windows 11.
                DrawFoot(r, tr.x - 8.0f, tr.Bottom() - 8.0f, false, selectedFill);
                DrawFoot(r, tr.Right(), tr.Bottom() - 8.0f, true, selectedFill);
            }
            else if (hot)
                r.FillRoundRect(tr.Inset(2.0f, 2.0f, 2.0f, 4.0f), 5.0f, hoverFill);
            else if (i + 1 != current && i + 1 < count && i + 1 != hover)
                r.FillRect({tr.Right() - 0.5f, tr.y + 8.0f, 1.0f, 16.0f}, divider);
            const TabInfo& t = tabs[static_cast<size_t>(i)];
            const Color fg = sel ? selectedText : text;
            float tx = tr.x + 12.0f;
            if (t.appIcon != AppIcon::None)
            {
                r.DrawAppIcon(t.appIcon, {tx, tr.CenterY() - 8.0f, 16.0f, 16.0f});
                tx += 24.0f;
            }
            else if (t.glyph != Icon::None)
            {
                r.Glyph(t.glyph, {tx, tr.CenterY() - 8.0f, 16.0f, 16.0f}, fg);
                tx += 24.0f;
            }
            r.Text(t.title, {tx, tr.y, tr.Right() - tx - 34.0f, tr.h}, fg, 12.0f, FontWeight::Regular, TextFlags::VCenter);
            const Rect xr = CloseRect(tr);
            if (t.modified && !(hot && hoverClose))
                r.FillCircle(xr.CenterX(), xr.CenterY(), 3.5f, fg);
            else if (sel || hot)
            {
                if (hot && hoverClose)
                    r.FillRoundRect(xr, 4.0f, sel ? Color(255, 255, 255, 0x18) : hoverFill);
                r.Glyph(Icon::Close, {xr.CenterX() - 5.0f, xr.CenterY() - 5.0f, 10.0f, 10.0f}, fg);
            }
        }
        const Rect nr = NewRect(windowW, count);
        if (hoverNew)
            r.FillRoundRect(nr, 4.0f, hoverFill);
        r.Glyph(Icon::Add, {nr.CenterX() - 6.0f, nr.CenterY() - 6.0f, 12.0f, 12.0f}, text);
        if (extraButtons > 0.0f)
        {
            const Rect er = ExtraRect(windowW, count);
            if (hoverExtra)
                r.FillRoundRect(er, 4.0f, hoverFill);
            r.Glyph(Icon::ChevronDown, {er.CenterX() - 5.0f, er.CenterY() - 5.0f, 10.0f, 10.0f}, text);
        }
    }

private:
    // Вогнутый уголок 8x8 (заливка вне четверти окружности).
    static void DrawFoot(Renderer& r, float x, float y, bool right, Color c)
    {
        Gdiplus::GraphicsPath path;
        if (!right)
        {
            path.AddLine(x, y + 8.0f, x + 8.0f, y + 8.0f);
            path.AddLine(x + 8.0f, y + 8.0f, x + 8.0f, y);
            path.AddArc(x - 8.0f, y - 8.0f, 16.0f, 16.0f, 0.0f, 90.0f);
        }
        else
        {
            path.AddLine(x, y, x, y + 8.0f);
            path.AddLine(x, y + 8.0f, x + 8.0f, y + 8.0f);
            path.AddArc(x, y - 8.0f, 16.0f, 16.0f, 90.0f, 90.0f);
        }
        path.CloseFigure();
        r.FillPath(path, c);
    }
};
