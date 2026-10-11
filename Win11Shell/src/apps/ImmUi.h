#pragma once
// Немедленный режим интерфейса для приложений, рисующих страницы целиком (Погода, Браузер,
// Медиаплеер, Календарь): во время отрисовки регистрируются «горячие» прямоугольники, а ввод
// сопоставляется с ними (наведение, нажатие, подсказки). Плюс общие элементы в стиле WinUI.

#include "../Shell.h"

struct ImmHot
{
    Rect r;
    Rect clip;
    int kind = 0;
    int index = 0;
    std::wstring tip;
};

class ImmUi
{
public:
    void Begin(const Rect& full)
    {
        m_hots.clear();
        m_clip = full;
        m_full = full;
    }
    void SetClip(const Rect& c) { m_clip = c; }
    void ResetClip() { m_clip = m_full; }
    const Rect& Clip() const { return m_clip; }
    void Clear() { m_hots.clear(); }

    void Add(const Rect& rc, int kind, int index = 0, std::wstring tip = {})
    {
        m_hots.push_back({rc, m_clip, kind, index, std::move(tip)});
    }

    const ImmHot* Hit(Point p) const
    {
        for (auto it = m_hots.rbegin(); it != m_hots.rend(); ++it)
        {
            if (it->r.Contains(p) && it->clip.Contains(p))
                return &*it;
        }
        return nullptr;
    }

    bool Hot(int kind, int index = 0) const { return m_hoverKind == kind && m_hoverIndex == index; }
    bool HotKind(int kind) const { return m_hoverKind == kind; }
    int HoverIndex() const { return m_hoverIndex; }
    bool Pressed(int kind, int index = 0) const { return Hot(kind, index) && m_pressKind == kind && m_pressIndex == index; }

    // Обработка ввода (координаты окна). owner — окно, для подсказок в экранных координатах.
    void Move(Point p, Point screenOffset)
    {
        const ImmHot* h = Hit(p);
        m_hoverKind = h ? h->kind : 0;
        m_hoverIndex = h ? h->index : 0;
        if (h && !h->tip.empty())
            GetShell().Tooltip(TipId(h->kind, h->index), h->tip, h->r.Translated(screenOffset.x, screenOffset.y), false);
    }
    void Leave() { m_hoverKind = 0; }
    void Down(Point p, int clicks)
    {
        const ImmHot* h = Hit(p);
        m_pressKind = h ? h->kind : 0;
        m_pressIndex = h ? h->index : 0;
        m_clicks = clicks;
    }
    // true — щелчок завершён над тем же элементом, что и нажатие.
    bool Up(Point p, int& kind, int& index)
    {
        const ImmHot* h = Hit(p);
        kind = m_pressKind;
        index = m_pressIndex;
        m_pressKind = 0;
        return h && kind != 0 && h->kind == kind && h->index == index;
    }
    int Clicks() const { return m_clicks; }
    int PressKind() const { return m_pressKind; }

private:
    static const void* TipId(int kind, int index)
    {
        static char ids[4096];
        return &ids[static_cast<size_t>(kind * 64 + index) & 4095u];
    }

    std::vector<ImmHot> m_hots;
    Rect m_clip;
    Rect m_full;
    int m_hoverKind = 0;
    int m_hoverIndex = 0;
    int m_pressKind = 0;
    int m_pressIndex = 0;
    int m_clicks = 1;
};

// ---- элементы ------------------------------------------------------------------------
namespace Imm
{
inline void RoundButton(Renderer& r, ImmUi& ui, const Rect& rc, Icon icon, bool accent, bool enabled, int kind, int index,
                        const wchar_t* tip)
{
    const Palette& p = Theme::P();
    const bool hot = enabled && ui.Hot(kind, index), pressed = enabled && ui.Pressed(kind, index);
    const float cx = rc.CenterX(), cy = rc.CenterY(), rad = rc.w * 0.5f;
    const float gs = rc.w >= 48.0f ? 18.0f : 16.0f;
    const Rect gr{cx - gs * 0.5f, cy - gs * 0.5f, gs, gs};
    if (accent)
    {
        r.FillCircle(cx, cy, rad, !enabled ? p.accentDisabled : (pressed ? p.accentPressed : (hot ? p.accentHover : p.accent)));
        r.Glyph(icon, gr, p.textOnAccent);
    }
    else
    {
        r.FillCircle(cx, cy, rad, !enabled ? p.controlDisabled : (pressed ? p.controlPressed : (hot ? p.controlHover : p.control)));
        r.StrokeCircle(cx, cy, rad - 0.5f, p.controlStroke);
        r.Glyph(icon, gr, enabled ? (pressed ? p.textSecondary : p.text) : p.textDisabled);
    }
    if (enabled)
        ui.Add(rc, kind, index, tip ? tip : L"");
}

inline void SubtleButton(Renderer& r, ImmUi& ui, const Rect& rc, Icon icon, int kind, int index, const wchar_t* tip, bool active = false,
                         Color fg = Color(0, 0, 0, 0))
{
    const Palette& p = Theme::P();
    if (ui.Pressed(kind, index))
        r.FillRoundRect(rc, 4.0f, p.subtlePressed);
    else if (ui.Hot(kind, index) || active)
        r.FillRoundRect(rc, 4.0f, p.subtleHover);
    const Color c = fg.a ? fg : (ui.Pressed(kind, index) ? p.textSecondary : p.text);
    r.Glyph(icon, {rc.CenterX() - 8.0f, rc.CenterY() - 8.0f, 16.0f, 16.0f}, c);
    ui.Add(rc, kind, index, tip ? tip : L"");
}

inline void TextButton(Renderer& r, ImmUi& ui, const Rect& rc, const std::wstring& text, Icon icon, bool accent, int kind, int index = 0)
{
    const Palette& p = Theme::P();
    const bool hot = ui.Hot(kind, index), pressed = ui.Pressed(kind, index);
    if (accent)
        r.FillRoundRect(rc, 4.0f, pressed ? p.accentPressed : (hot ? p.accentHover : p.accent));
    else
    {
        r.FillRoundRect(rc, 4.0f, pressed ? p.controlPressed : (hot ? p.controlHover : p.control));
        r.StrokeRoundRect(rc, 4.0f, p.controlStroke);
    }
    const Color fg = accent ? p.textOnAccent : (pressed ? p.textSecondary : p.text);
    const float tw = r.TextWidth(text, 14.0f);
    float x = rc.CenterX() - tw * 0.5f;
    if (icon != Icon::None)
    {
        x += 12.0f;
        r.Glyph(icon, {x - 26.0f, rc.CenterY() - 7.0f, 14.0f, 14.0f}, fg);
    }
    r.Text(text, {x, rc.y, tw + 4.0f, rc.h}, fg, 14.0f, FontWeight::Regular, TextFlags::VCenter);
    ui.Add(rc, kind, index);
}

inline void Toggle(Renderer& r, const Rect& rc, bool on, bool hot)
{
    const Palette& p = Theme::P();
    const float rad = rc.h * 0.5f, knob = hot ? 7.0f : 6.0f;
    if (on)
    {
        r.FillRoundRect(rc, rad, hot ? p.accentHover : p.accent);
        r.FillCircle(rc.Right() - rad, rc.CenterY(), knob, p.textOnAccent);
    }
    else
    {
        r.FillRoundRect(rc, rad, hot ? p.controlHover : p.controlAltFill);
        r.StrokeRoundRect(rc, rad, p.controlStrongStroke);
        r.FillCircle(rc.x + rad, rc.CenterY(), knob, p.textSecondary);
    }
}

// Флажок или переключатель (radio) с подписью.
inline void Check(Renderer& r, ImmUi& ui, float x, float y, bool checked, const std::wstring& text, int kind, int index = 0,
                  bool enabled = true, bool radio = false)
{
    const Palette& p = Theme::P();
    const Rect box{x, y, 20.0f, 20.0f};
    const bool hot = enabled && ui.Hot(kind, index);
    if (radio)
    {
        if (checked)
        {
            r.FillCircle(box.CenterX(), box.CenterY(), 10.0f, !enabled ? p.accentDisabled : (hot ? p.accentHover : p.accent));
            r.FillCircle(box.CenterX(), box.CenterY(), hot ? 5.0f : 4.0f, p.textOnAccent);
        }
        else
        {
            r.FillCircle(box.CenterX(), box.CenterY(), 10.0f, hot ? p.controlHover : p.controlAltFill);
            r.StrokeCircle(box.CenterX(), box.CenterY(), 9.5f, enabled ? p.controlStrongStroke : p.controlStroke);
        }
    }
    else if (checked)
    {
        r.FillRoundRect(box, 4.0f, !enabled ? p.accentDisabled : (hot ? p.accentHover : p.accent));
        r.Glyph(Icon::Check, {x + 4.0f, y + 4.0f, 12.0f, 12.0f}, p.textOnAccent);
    }
    else
    {
        r.FillRoundRect(box, 4.0f, hot ? p.controlHover : p.controlAltFill);
        r.StrokeRoundRect(box, 4.0f, enabled ? p.controlStrongStroke : p.controlStroke);
    }
    const float tw = r.TextWidth(text, 14.0f);
    r.Text(text, {x + 30.0f, y - 2.0f, tw + 4.0f, 24.0f}, enabled ? p.text : p.textDisabled, 14.0f, FontWeight::Regular, TextFlags::VCenter);
    if (enabled)
        ui.Add({x - 2.0f, y - 4.0f, tw + 36.0f, 28.0f}, kind, index);
}

inline void Card(Renderer& r, const Rect& rc, bool hot = false)
{
    const Palette& p = Theme::P();
    r.FillRoundRect(rc, 8.0f, hot ? (p.dark ? Color(255, 255, 255, 0x12) : Color(0xF9, 0xF9, 0xF9)) : p.card);
    r.StrokeRoundRect(rc, 8.0f, p.cardStroke);
}

inline void ProgressRing(Renderer& r, float cx, float cy, float rad, float frac, float width)
{
    const Palette& p = Theme::P();
    r.Arc(cx, cy, rad, 0.0f, 360.0f, p.dark ? Color(255, 255, 255, 0x26) : Color(0, 0, 0, 0x16), width);
    if (frac > 0.0005f)
        r.Arc(cx, cy, rad, -90.0f, 360.0f * Saturate(frac), p.accent, width);
}

// Слой содержимого со скруглённым левым верхним углом (как у NavigationView).
inline void ContentLayer(Renderer& r, const Rect& c)
{
    const Palette& p = Theme::P();
    Gdiplus::GraphicsPath path;
    Renderer::RoundRectPath(path, {c.x, c.y, c.w + 12.0f, c.h + 12.0f}, 8.0f, 0.0f, 0.0f, 0.0f);
    r.FillPath(path, p.layer);
    r.StrokePath(path, p.cardStroke);
}

inline void Scrollbar(Renderer& r, const Rect& view, float scroll, float contentH)
{
    const float maxS = std::max(0.0f, contentH - view.h);
    if (maxS <= 0.0f)
        return;
    const float th = std::max(40.0f, view.h * view.h / contentH);
    const float ty = view.y + 4.0f + (view.h - 8.0f - th) * (scroll / maxS);
    r.FillRoundRect({view.Right() - 6.0f, ty, 3.0f, th}, 1.5f, Theme::P().scrollThumb);
}

// Пункт боковой навигации: выделение с акцентной полоской слева.
inline void NavRow(Renderer& r, ImmUi& ui, const Rect& rr, Icon icon, const wchar_t* text, bool selected, bool showText, int kind, int index)
{
    const Palette& p = Theme::P();
    if (ui.Pressed(kind, index))
        r.FillRoundRect(rr, 4.0f, p.subtlePressed);
    else if (selected || ui.Hot(kind, index))
        r.FillRoundRect(rr, 4.0f, selected && ui.Hot(kind, index) ? p.subtlePressed : p.subtleHover);
    if (selected)
        r.FillRoundRect({rr.x, rr.CenterY() - 8.0f, 3.0f, 16.0f}, 1.5f, p.accent);
    r.Glyph(icon, {rr.x + 12.0f, rr.CenterY() - 8.0f, 16.0f, 16.0f}, p.text);
    if (showText)
        r.Text(text, {rr.x + 44.0f, rr.y, rr.w - 50.0f, rr.h}, p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
    ui.Add(rr, kind, index, showText ? L"" : text);
}

// Модальная подложка и карточка диалога с нижней панелью кнопок.
inline void DialogFrame(Renderer& r, const Rect& full, const Rect& d, float t, const wchar_t* title)
{
    const Palette& p = Theme::P();
    r.FillRect(full, p.smoke.MulAlpha(t));
    r.Shadow(d, 8.0f, 32.0f, 12.0f, p.dark ? 0.55f : 0.3f);
    r.FillRoundRect(d, 8.0f, p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xFFFFFF));
    r.FillRoundRect({d.x, d.Bottom() - 80.0f, d.w, 80.0f}, 0.0f, 0.0f, 8.0f, 8.0f, p.dark ? Color::Hex(0x202020) : Color::Hex(0xF3F3F3));
    r.FillRect({d.x, d.Bottom() - 80.0f, d.w, 1.0f}, p.divider);
    r.StrokeRoundRect(d, 8.0f, p.surfaceStroke);
    r.Text(title, {d.x + 24.0f, d.y + 18.0f, d.w - 90.0f, 30.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::VCenter);
}
} // namespace Imm
