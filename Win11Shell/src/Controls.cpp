#include "Controls.h"

#include "ContextMenu.h"

bool IsWordChar(wchar_t c)
{
    if (c == L'_')
        return true;
    WORD type = 0;
    if (!GetStringTypeW(CT_CTYPE1, &c, 1, &type))
        return false;
    return (type & (C1_ALPHA | C1_DIGIT)) != 0;
}

void DrawSpinner(Renderer& r, float cx, float cy, float radius, float thickness, Color c)
{
    const double t = NowSeconds();
    const float phase = static_cast<float>(std::fmod(t * 1.25, 1.0));
    const float rot = static_cast<float>(std::fmod(t * 300.0, 360.0));
    const float k = 0.5f - 0.5f * std::cos(phase * 2.0f * kPi);
    const float sweep = 30.0f + 230.0f * k;
    const float start = rot + phase * 140.0f;
    r.Arc(cx, cy, radius, start, sweep, c, thickness);
    RequestFrame();
}

void DrawProgressBar(Renderer& r, const Rect& rc, float value, Color fill)
{
    const Palette& p = Theme::P();
    r.FillRoundRect({rc.x, rc.CenterY() - 0.5f, rc.w, 1.0f}, 0.5f, p.controlStrongStroke);
    const float w = rc.w * Saturate(value);
    if (w > 0.5f)
        r.FillRoundRect({rc.x, rc.CenterY() - 1.5f, w, 3.0f}, 1.5f, fill);
}

Color ToneColor(Tone t)
{
    const Palette& p = Theme::P();
    switch (t)
    {
    case Tone::Primary:
        return p.text;
    case Tone::Secondary:
        return p.textSecondary;
    case Tone::Tertiary:
        return p.textTertiary;
    case Tone::Accent:
        return p.accentText;
    case Tone::Disabled:
        return p.textDisabled;
    case Tone::OnAccent:
        return p.textOnAccent;
    }
    return p.text;
}

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------
Button::Button(std::wstring caption, Icon glyph, ButtonStyle look) : text(std::move(caption)), icon(glyph), style(look)
{
}

void Button::Draw(Renderer& r)
{
    const Palette& p = Theme::P();
    const Rect& b = m_bounds;
    const bool down = m_pressed && m_hovered;
    Color fg = p.text;

    switch (style)
    {
    case ButtonStyle::Standard:
    {
        const Color fill = !m_enabled ? p.controlDisabled
                                      : (down ? p.controlPressed : (m_hovered || checked ? p.controlHover : p.control));
        r.FillRoundRect(b, radius, fill);
        r.StrokeRoundRect(b, radius, p.controlStroke);
        if (!down && m_enabled)
            r.Line(b.x + radius, b.Bottom() - 0.5f, b.Right() - radius, b.Bottom() - 0.5f, p.controlStrokeSecondary,
                   1.0f, false);
        fg = !m_enabled ? p.textDisabled : (down ? p.textSecondary : p.text);
        break;
    }
    case ButtonStyle::Accent:
    {
        const Color fill = !m_enabled ? p.accentDisabled : (down ? p.accentPressed : (m_hovered ? p.accentHover : p.accent));
        r.FillRoundRect(b, radius, fill);
        if (m_enabled && !down)
            r.Line(b.x + radius, b.Bottom() - 0.5f, b.Right() - radius, b.Bottom() - 0.5f, Color(0, 0, 0, 0x30), 1.0f,
                   false);
        fg = !m_enabled ? p.textDisabled : (down ? p.textOnAccentSecondary : p.textOnAccent);
        break;
    }
    case ButtonStyle::Subtle:
    case ButtonStyle::Hyperlink:
    {
        if (m_enabled && (down || m_hovered || checked))
            r.FillRoundRect(b, radius, down ? p.subtlePressed : p.subtleHover);
        if (style == ButtonStyle::Hyperlink)
            fg = !m_enabled ? p.textDisabled : (down ? p.accentTextSecondary : p.accentText);
        else
            fg = !m_enabled ? p.textDisabled : (down ? p.textSecondary : p.text);
        break;
    }
    }

    // Содержимое: иконка + текст (+ стрелка).
    const bool hasIcon = icon != Icon::None || appIcon != AppIcon::None;
    const float tw = text.empty() ? 0.0f : r.TextWidth(text, fontSize, weight);
    const float gap = (hasIcon && !text.empty()) ? 8.0f : 0.0f;
    const float chevW = chevron ? 20.0f : 0.0f;
    const float total = (hasIcon ? iconSize : 0.0f) + gap + tw + chevW;
    float x = (align & TextFlags::Center) ? b.x + (b.w - total) * 0.5f : b.x + padding;
    const float cy = b.CenterY();
    if (hasIcon)
    {
        const Rect ir{x, cy - iconSize * 0.5f, iconSize, iconSize};
        if (appIcon != AppIcon::None)
        {
            r.PushOpacity(m_enabled ? 1.0f : 0.4f);
            r.DrawAppIcon(appIcon, ir);
            r.PopOpacity();
        }
        else
            r.Glyph(icon, ir, hasIconColor && m_enabled ? iconColor : fg);
        x += iconSize + gap;
    }
    if (!text.empty())
    {
        const float avail = (align & TextFlags::Center) ? tw + 2.0f : std::max(0.0f, b.Right() - padding - chevW - x);
        r.Text(text, {x, b.y, avail, b.h}, fg, fontSize, weight, TextFlags::VCenter);
        x += tw;
    }
    if (chevron)
        r.Glyph(Icon::ChevronDown, {b.Right() - padding - 12.0f, cy - 6.0f, 12.0f, 12.0f}, fg);
}

bool Button::OnMouseDown(const MouseEvent& e)
{
    if (e.button != MouseButton::Left)
        return false;
    m_pressed = true;
    return true;
}

void Button::OnMouseUp(const MouseEvent& e)
{
    const bool click = m_pressed && m_bounds.Contains(e.pos) && e.button == MouseButton::Left;
    m_pressed = false;
    if (click && onClick)
        PostAction(onClick);
}

// ---------------------------------------------------------------------------
// ToggleSwitch
// ---------------------------------------------------------------------------
ToggleSwitch::ToggleSwitch(bool initial) : m_on(initial), m_knob(initial ? 1.0f : 0.0f) {}

void ToggleSwitch::SetOn(bool on, bool notify)
{
    if (m_on == on)
        return;
    m_on = on;
    m_knob.Set(on ? 1.0f : 0.0f, 0.18f, Ease::OutCubic);
    if (notify && onChange)
    {
        auto cb = onChange;
        PostAction([cb, on]() { cb(on); });
    }
}

void ToggleSwitch::Draw(Renderer& r)
{
    const Palette& p = Theme::P();
    const Rect& b = m_bounds;
    const Rect track{b.Right() - 40.0f, b.CenterY() - 10.0f, 40.0f, 20.0f};
    if (showLabel)
        r.Text(m_on ? onText : offText, {b.x, b.y, std::max(0.0f, track.x - b.x - 12.0f), b.h},
               m_enabled ? p.text : p.textDisabled, 14.0f, FontWeight::Regular, TextFlags::Right | TextFlags::VCenter);

    const float t = m_knob.Value();
    const bool down = m_pressed && m_hovered;
    if (m_on)
    {
        r.FillRoundRect(track, 10.0f, !m_enabled ? p.accentDisabled : (down ? p.accentPressed : (m_hovered ? p.accentHover : p.accent)));
    }
    else
    {
        r.FillRoundRect(track, 10.0f, m_hovered ? p.controlAltHover : p.controlAltFill);
        r.StrokeRoundRect(track, 10.0f, m_enabled ? p.controlStrongStroke : p.textDisabled);
    }
    float kw = m_hovered ? 14.0f : 12.0f;
    const float kh = m_hovered ? 14.0f : 12.0f;
    if (down)
        kw = 17.0f;
    const float minX = track.x + 10.0f, maxX = track.Right() - 10.0f;
    float cx = Lerp(minX, maxX, t);
    if (down)
        cx = Clamp(cx + (m_on ? -1.5f : 1.5f), minX, maxX);
    const Color knob = m_on ? p.textOnAccent : (m_enabled ? p.controlStrongFill : p.textDisabled);
    r.FillRoundRect({cx - kw * 0.5f, track.CenterY() - kh * 0.5f, kw, kh}, kh * 0.5f, knob);
}

bool ToggleSwitch::OnMouseDown(const MouseEvent& e)
{
    if (e.button != MouseButton::Left)
        return false;
    m_pressed = true;
    return true;
}

void ToggleSwitch::OnMouseUp(const MouseEvent& e)
{
    const bool click = m_pressed && m_bounds.Contains(e.pos);
    m_pressed = false;
    if (click)
        SetOn(!m_on, true);
}

// ---------------------------------------------------------------------------
// Slider
// ---------------------------------------------------------------------------
void Slider::SetValue(float v, bool notify)
{
    v = Saturate(v);
    if (v == value)
        return;
    value = v;
    if (notify && onChange)
    {
        auto cb = onChange;
        cb(v);
    }
}

void Slider::Draw(Renderer& r)
{
    const Palette& p = Theme::P();
    const Rect& b = m_bounds;
    const float x0 = b.x + 10.0f, x1 = b.Right() - 10.0f;
    const float cy = b.CenterY();
    const float tx = Lerp(x0, x1, value);
    r.FillRoundRect({x0, cy - 2.0f, x1 - x0, 4.0f}, 2.0f, p.controlStrongFill);
    r.FillRoundRect({x0, cy - 2.0f, tx - x0, 4.0f}, 2.0f, m_enabled ? p.accent : p.accentDisabled);
    r.FillCircle(tx, cy, 10.0f, p.controlSolid);
    r.StrokeCircle(tx, cy, 9.5f, p.controlStroke, 1.0f);
    const bool down = m_pressed;
    const float inner = down ? 5.0f : (m_hovered ? 7.0f : 6.0f);
    r.FillCircle(tx, cy, inner, m_enabled ? (down ? p.accentPressed : p.accent) : p.accentDisabled);
}

void Slider::SetFromX(float x)
{
    const float x0 = m_bounds.x + 10.0f, x1 = m_bounds.Right() - 10.0f;
    SetValue((x - x0) / std::max(1.0f, x1 - x0), true);
}

bool Slider::OnMouseDown(const MouseEvent& e)
{
    if (e.button != MouseButton::Left)
        return false;
    m_pressed = true;
    SetFromX(e.pos.x);
    return true;
}

void Slider::OnMouseMove(const MouseEvent& e)
{
    if (m_pressed)
        SetFromX(e.pos.x);
}

void Slider::OnMouseUp(const MouseEvent& /*e*/) { m_pressed = false; }

bool Slider::OnMouseWheel(const MouseEvent& e)
{
    SetValue(value + e.wheel * 0.02f, true);
    return true;
}

// ---------------------------------------------------------------------------
// TextBox
// ---------------------------------------------------------------------------
TextBox::TextBox(std::wstring placeholderText) : placeholder(std::move(placeholderText)) {}

void TextBox::SetText(const std::wstring& s, bool notify)
{
    m_text = s;
    m_caret = m_anchor = static_cast<int>(m_text.size());
    m_scrollX = 0.0f;
    if (notify)
        Changed();
}

void TextBox::SelectAll()
{
    m_anchor = 0;
    m_caret = static_cast<int>(m_text.size());
}

void TextBox::OnFocusChanged(bool focused)
{
    Widget::OnFocusChanged(focused);
    m_lastInput = NowSeconds();
    if (!focused)
        m_anchor = m_caret;
}

Rect TextBox::TextArea() const
{
    const float l = leadingIcon != Icon::None ? 38.0f : (pill ? 16.0f : 11.0f);
    const float rr = trailingIcon != Icon::None ? 36.0f : 11.0f;
    return m_bounds.Inset(l, 0.0f, rr, 0.0f);
}

std::wstring TextBox::Display() const
{
    if (!password)
        return m_text;
    return std::wstring(m_text.size(), L'●');
}

int TextBox::HitIndex(float x) const
{
    Renderer& r = Renderer::Get();
    const Rect ta = TextArea();
    return r.HitTestText(Display(), x - ta.x + m_scrollX, fontSize);
}

void TextBox::Draw(Renderer& r)
{
    const Palette& p = Theme::P();
    const Rect& b = m_bounds;
    const float rad = pill ? b.h * 0.5f : 4.0f;
    if (!borderless)
    {
        const Color fill = m_focused ? p.controlInputActive : (m_hovered ? p.controlHover : p.control);
        r.FillRoundRect(b, rad, fill);
        r.StrokeRoundRect(b, rad, p.controlStroke);
        if (!pill)
        {
            r.PushClip({b.x, b.Bottom() - 2.0f, b.w, 2.0f});
            if (m_focused)
                r.FillRoundRect(b, rad, p.accent);
            else
                r.StrokeRoundRect(b, rad, p.controlStrongStroke);
            r.PopClip();
        }
        else if (m_focused)
        {
            r.StrokeRoundRect(b, rad, p.accent, 1.0f);
        }
    }
    if (leadingIcon != Icon::None)
        r.Glyph(leadingIcon, {b.x + (pill ? 16.0f : 12.0f), b.CenterY() - 8.0f, 16.0f, 16.0f}, p.textSecondary);
    if (trailingIcon != Icon::None)
        r.Glyph(trailingIcon, {b.Right() - 28.0f, b.CenterY() - 7.0f, 14.0f, 14.0f}, p.textSecondary);

    const Rect ta = TextArea();
    const std::wstring disp = Display();
    const float lh = r.LineHeight(fontSize);
    const float ty = b.CenterY() - lh * 0.5f;

    // Прокрутка, чтобы каретка была видна.
    const float caretX = r.TextWidth(disp.c_str(), m_caret, fontSize, FontWeight::Regular, FontFace::UI);
    if (caretX - m_scrollX > ta.w - 2.0f)
        m_scrollX = caretX - ta.w + 2.0f;
    if (caretX - m_scrollX < 0.0f)
        m_scrollX = caretX;
    if (m_scrollX < 0.0f)
        m_scrollX = 0.0f;

    r.PushClip(ta);
    if (m_text.empty())
    {
        if (!placeholder.empty())
            r.Text(placeholder, {ta.x, b.y, ta.w, b.h}, p.textSecondary, fontSize, FontWeight::Regular, TextFlags::VCenter);
    }
    else
    {
        const Rect tr{ta.x - m_scrollX, b.y, ta.w + m_scrollX + 1000.0f, b.h};
        r.Text(disp, tr, p.text, fontSize, FontWeight::Regular, TextFlags::VCenter | TextFlags::NoEllipsis);
        if (HasSelection() && m_focused)
        {
            const int s0 = std::min(m_caret, m_anchor), s1 = std::max(m_caret, m_anchor);
            const float x0 = r.TextWidth(disp.c_str(), s0, fontSize, FontWeight::Regular, FontFace::UI);
            const float x1 = r.TextWidth(disp.c_str(), s1, fontSize, FontWeight::Regular, FontFace::UI);
            const Rect sel{ta.x - m_scrollX + x0, ty, x1 - x0, lh};
            r.FillRect(sel, p.accent);
            r.PushClip(sel);
            r.Text(disp, tr, p.textOnAccent, fontSize, FontWeight::Regular, TextFlags::VCenter | TextFlags::NoEllipsis);
            r.PopClip();
        }
    }
    if (m_focused)
    {
        r.NoteCaret();
        const double since = NowSeconds() - m_lastInput;
        if (std::fmod(since, 1.06) < 0.53)
        {
            const float cx = std::floor(ta.x - m_scrollX + caretX) + 0.5f;
            r.Line(cx, ty + 1.0f, cx, ty + lh - 1.0f, p.text, 1.0f, false);
        }
    }
    r.PopClip();
}

bool TextBox::OnMouseDown(const MouseEvent& e)
{
    if (e.button != MouseButton::Left)
        return true;
    const int idx = HitIndex(e.pos.x);
    if (e.clicks >= 2)
    {
        m_anchor = WordLeft(std::min(idx + 1, static_cast<int>(m_text.size())));
        m_caret = WordRight(idx);
        if (m_anchor > idx)
            m_anchor = idx;
    }
    else
    {
        MoveCaret(idx, e.mods.shift);
        m_dragging = true;
    }
    m_lastInput = NowSeconds();
    return true;
}

void TextBox::OnMouseMove(const MouseEvent& e)
{
    if (m_dragging)
    {
        m_caret = HitIndex(e.pos.x);
        m_lastInput = NowSeconds();
    }
}

void TextBox::OnMouseUp(const MouseEvent& /*e*/) { m_dragging = false; }

CursorType TextBox::Cursor(Point p) const
{
    if (trailingIcon != Icon::None && p.x > m_bounds.Right() - 36.0f)
        return CursorType::Arrow;
    return CursorType::IBeam;
}

void TextBox::MoveCaret(int pos, bool extend)
{
    m_caret = Clamp(pos, 0, static_cast<int>(m_text.size()));
    if (!extend)
        m_anchor = m_caret;
    m_lastInput = NowSeconds();
}

int TextBox::WordLeft(int pos) const
{
    pos = Clamp(pos, 0, static_cast<int>(m_text.size()));
    while (pos > 0 && !IsWordChar(m_text[static_cast<size_t>(pos - 1)]))
        --pos;
    while (pos > 0 && IsWordChar(m_text[static_cast<size_t>(pos - 1)]))
        --pos;
    return pos;
}

int TextBox::WordRight(int pos) const
{
    const int n = static_cast<int>(m_text.size());
    pos = Clamp(pos, 0, n);
    while (pos < n && IsWordChar(m_text[static_cast<size_t>(pos)]))
        ++pos;
    while (pos < n && !IsWordChar(m_text[static_cast<size_t>(pos)]))
        ++pos;
    return pos;
}

void TextBox::DeleteSelection()
{
    if (!HasSelection())
        return;
    const int s0 = std::min(m_caret, m_anchor), s1 = std::max(m_caret, m_anchor);
    m_text.erase(static_cast<size_t>(s0), static_cast<size_t>(s1 - s0));
    m_caret = m_anchor = s0;
}

void TextBox::Insert(const std::wstring& s)
{
    DeleteSelection();
    std::wstring clean;
    for (wchar_t c : s)
    {
        if (c != L'\n' && c != L'\r' && c != L'\t')
            clean += c;
    }
    m_text.insert(static_cast<size_t>(m_caret), clean);
    m_caret += static_cast<int>(clean.size());
    m_anchor = m_caret;
    Changed();
}

void TextBox::Changed()
{
    m_lastInput = NowSeconds();
    if (onChange)
    {
        auto cb = onChange;
        const std::wstring t = m_text;
        PostAction([cb, t]() { cb(t); });
    }
}

bool TextBox::OnKeyDown(UINT vk, const KeyMods& mods)
{
    if (onKey && onKey(vk, mods))
        return true;
    const int n = static_cast<int>(m_text.size());
    switch (vk)
    {
    case VK_LEFT:
        if (HasSelection() && !mods.shift)
            MoveCaret(std::min(m_caret, m_anchor), false);
        else
            MoveCaret(mods.ctrl ? WordLeft(m_caret) : m_caret - 1, mods.shift);
        return true;
    case VK_RIGHT:
        if (HasSelection() && !mods.shift)
            MoveCaret(std::max(m_caret, m_anchor), false);
        else
            MoveCaret(mods.ctrl ? WordRight(m_caret) : m_caret + 1, mods.shift);
        return true;
    case VK_HOME:
        MoveCaret(0, mods.shift);
        return true;
    case VK_END:
        MoveCaret(n, mods.shift);
        return true;
    case VK_BACK:
        if (HasSelection())
            DeleteSelection();
        else if (m_caret > 0)
        {
            const int from = mods.ctrl ? WordLeft(m_caret) : m_caret - 1;
            m_text.erase(static_cast<size_t>(from), static_cast<size_t>(m_caret - from));
            m_caret = m_anchor = from;
        }
        else
            return true;
        Changed();
        return true;
    case VK_DELETE:
        if (HasSelection())
            DeleteSelection();
        else if (m_caret < n)
        {
            const int to = mods.ctrl ? WordRight(m_caret) : m_caret + 1;
            m_text.erase(static_cast<size_t>(m_caret), static_cast<size_t>(to - m_caret));
        }
        else
            return true;
        Changed();
        return true;
    case VK_RETURN:
        if (onEnter)
            PostAction(onEnter);
        return true;
    case VK_ESCAPE:
        if (onEscape)
        {
            PostAction(onEscape);
            return true;
        }
        return false;
    default:
        break;
    }
    if (mods.ctrl)
    {
        HWND hwnd = GetActiveWindow();
        if (vk == 'A')
        {
            SelectAll();
            return true;
        }
        if ((vk == 'C' || vk == 'X') && HasSelection() && !password)
        {
            const int s0 = std::min(m_caret, m_anchor), s1 = std::max(m_caret, m_anchor);
            SetClipboardText(hwnd, m_text.substr(static_cast<size_t>(s0), static_cast<size_t>(s1 - s0)));
            if (vk == 'X')
            {
                DeleteSelection();
                Changed();
            }
            return true;
        }
        if (vk == 'V')
        {
            Insert(GetClipboardText(hwnd));
            return true;
        }
    }
    return false;
}

bool TextBox::OnChar(wchar_t ch)
{
    if (ch < 32 || ch == 127)
        return ch == L'\r' || ch == 8 || ch == 27 || ch == 1 || ch == 3 || ch == 22 || ch == 24;
    Insert(std::wstring(1, ch));
    return true;
}

// ---------------------------------------------------------------------------
// ComboBox
// ---------------------------------------------------------------------------
void ComboBox::Draw(Renderer& r)
{
    const Palette& p = Theme::P();
    const Rect& b = m_bounds;
    const bool down = m_pressed && m_hovered;
    r.FillRoundRect(b, 4.0f, down ? p.controlPressed : (m_hovered ? p.controlHover : p.control));
    r.StrokeRoundRect(b, 4.0f, p.controlStroke);
    r.Line(b.x + 4.0f, b.Bottom() - 0.5f, b.Right() - 4.0f, b.Bottom() - 0.5f, p.controlStrokeSecondary, 1.0f, false);
    if (selected >= 0 && selected < static_cast<int>(items.size()))
        r.Text(items[static_cast<size_t>(selected)], {b.x + 12.0f, b.y, b.w - 44.0f, b.h}, m_enabled ? p.text : p.textDisabled,
               14.0f, FontWeight::Regular, TextFlags::VCenter);
    r.Glyph(Icon::ChevronDown, {b.Right() - 26.0f, b.CenterY() - 6.0f + (down ? 1.0f : 0.0f), 12.0f, 12.0f},
            p.textSecondary);
}

bool ComboBox::OnMouseDown(const MouseEvent& e)
{
    if (e.button != MouseButton::Left)
        return false;
    m_pressed = true;
    return true;
}

void ComboBox::OnMouseUp(const MouseEvent& e)
{
    const bool click = m_pressed && m_bounds.Contains(e.pos);
    m_pressed = false;
    if (!click || items.empty())
        return;
    const Point o = Widget::Origin();
    const Rect anchor = m_bounds.Translated(o.x, o.y);
    ComboBox* self = this;
    const std::vector<std::wstring> list = items;
    const int sel = selected;
    PostAction([anchor, list, sel, self]() {
        ShowDropdown(anchor, list, sel, [self](int i) {
            self->selected = i;
            if (self->onChange)
                self->onChange(i);
        });
    });
}

// ---------------------------------------------------------------------------
// CheckBox / RadioButton
// ---------------------------------------------------------------------------
CheckBox::CheckBox(std::wstring caption, bool initial) : text(std::move(caption)), checked(initial) {}

void CheckBox::Draw(Renderer& r)
{
    const Palette& p = Theme::P();
    const Rect& b = m_bounds;
    const Rect box{b.x, b.CenterY() - 10.0f, 20.0f, 20.0f};
    const bool down = m_pressed && m_hovered;
    if (radio)
    {
        if (checked)
        {
            r.FillCircle(box.CenterX(), box.CenterY(), 10.0f, down ? p.accentPressed : (m_hovered ? p.accentHover : p.accent));
            r.FillCircle(box.CenterX(), box.CenterY(), down ? 3.5f : (m_hovered ? 5.0f : 4.0f), p.textOnAccent);
        }
        else
        {
            r.FillCircle(box.CenterX(), box.CenterY(), 10.0f, m_hovered ? p.controlAltHover : p.controlAltFill);
            r.StrokeCircle(box.CenterX(), box.CenterY(), 9.5f, p.controlStrongStroke, 1.0f);
            if (down)
                r.FillCircle(box.CenterX(), box.CenterY(), 5.0f, p.controlStrongFill);
        }
    }
    else
    {
        if (checked)
        {
            r.FillRoundRect(box, 4.0f, down ? p.accentPressed : (m_hovered ? p.accentHover : p.accent));
            r.Glyph(Icon::Check, box.Inflated(-3.0f), p.textOnAccent, 1.4f);
        }
        else
        {
            r.FillRoundRect(box, 4.0f, m_hovered ? p.controlAltHover : p.controlAltFill);
            r.StrokeRoundRect(box, 4.0f, p.controlStrongStroke);
        }
    }
    if (!text.empty())
        r.Text(text, {b.x + 28.0f, b.y, b.w - 28.0f, b.h}, m_enabled ? p.text : p.textDisabled, 14.0f,
               FontWeight::Regular, TextFlags::VCenter);
}

bool CheckBox::OnMouseDown(const MouseEvent& e)
{
    if (e.button != MouseButton::Left)
        return false;
    m_pressed = true;
    return true;
}

void CheckBox::OnMouseUp(const MouseEvent& e)
{
    const bool click = m_pressed && m_bounds.Contains(e.pos);
    m_pressed = false;
    if (!click)
        return;
    if (radio)
        checked = true;
    else
        checked = !checked;
    if (onChange)
    {
        auto cb = onChange;
        const bool v = checked;
        PostAction([cb, v]() { cb(v); });
    }
}

// ---------------------------------------------------------------------------
// Label
// ---------------------------------------------------------------------------
Label::Label(std::wstring caption, float size, FontWeight w, Tone t)
    : text(std::move(caption)), fontSize(size), weight(w), tone(t)
{
}

void Label::Draw(Renderer& r) { r.Text(text, m_bounds, ToneColor(tone), fontSize, weight, flags); }

// ---------------------------------------------------------------------------
// NavItem
// ---------------------------------------------------------------------------
NavItem::NavItem(std::wstring caption, Icon glyph, AppIcon app) : text(std::move(caption)), icon(glyph), appIcon(app) {}

void NavItem::Draw(Renderer& r)
{
    const Palette& p = Theme::P();
    const Rect& b = m_bounds;
    const bool down = m_pressed && m_hovered;
    if (selected || m_hovered)
        r.FillRoundRect(b, 4.0f, down ? p.subtlePressed : p.subtleHover);
    if (selected)
        r.FillRoundRect({b.x, b.CenterY() - 8.0f, 3.0f, 16.0f}, 1.5f, p.accent);
    float x = b.x + 12.0f;
    if (appIcon != AppIcon::None)
    {
        r.DrawAppIcon(appIcon, {x, b.CenterY() - 10.0f, 20.0f, 20.0f});
        x += 32.0f;
    }
    else if (icon != Icon::None)
    {
        r.Glyph(icon, {x, b.CenterY() - 8.0f, 16.0f, 16.0f}, hasIconColor ? iconColor : p.text);
        x += 32.0f;
    }
    if (!compact)
        r.Text(text, {x, b.y, b.Right() - x - 8.0f, b.h}, p.text, fontSize, FontWeight::Regular, TextFlags::VCenter);
}

bool NavItem::OnMouseDown(const MouseEvent& e)
{
    if (e.button != MouseButton::Left)
        return false;
    m_pressed = true;
    return true;
}

void NavItem::OnMouseUp(const MouseEvent& e)
{
    const bool click = m_pressed && m_bounds.Contains(e.pos);
    m_pressed = false;
    if (click && onClick)
        PostAction(onClick);
}

// ---------------------------------------------------------------------------
// ScrollPanel
// ---------------------------------------------------------------------------
void ScrollPanel::ScrollTo(float y, bool animate)
{
    y = Clamp(y, 0.0f, MaxScroll());
    if (animate)
        m_scroll.Set(y, 0.25f, Ease::OutCubic);
    else
        m_scroll.Snap(y);
}

MouseEvent ScrollPanel::ToContent(const MouseEvent& e) const
{
    MouseEvent c = e;
    c.pos.y += m_scroll.Value();
    return c;
}

Rect ScrollPanel::ThumbRect() const
{
    const float viewH = m_bounds.h;
    if (contentHeight <= viewH + 0.5f)
        return {};
    const float trackY = m_bounds.y + 4.0f, trackH = viewH - 8.0f;
    const float thumbH = std::max(24.0f, trackH * viewH / contentHeight);
    const float t = MaxScroll() > 0.0f ? m_scroll.Value() / MaxScroll() : 0.0f;
    const float w = (m_barHover || m_dragThumb) ? 6.0f : 2.0f;
    return {m_bounds.Right() - 4.0f - w * 0.5f - 1.0f, trackY + (trackH - thumbH) * t, w, thumbH};
}

bool ScrollPanel::OnScrollbar(Point p) const
{
    return contentHeight > m_bounds.h + 0.5f && p.x >= m_bounds.Right() - 12.0f && m_bounds.Contains(p);
}

void ScrollPanel::Draw(Renderer& r)
{
    // Ограничение прокрутки при изменении размеров содержимого.
    if (m_scroll.Target() > MaxScroll())
        m_scroll.Snap(MaxScroll());
    const float sy = m_scroll.Value();
    r.PushClip(m_bounds);
    r.PushTransform(0.0f, -sy);
    for (auto& c : m_children)
    {
        if (!c->Visible())
            continue;
        const Rect& cb = c->Bounds();
        if (cb.Bottom() < m_bounds.y + sy - 50.0f || cb.y > m_bounds.Bottom() + sy + 50.0f)
            continue;
        c->Draw(r);
    }
    r.PopTransform();
    const Rect th = ThumbRect();
    if (!th.Empty())
    {
        const Palette& p = Theme::P();
        if (m_barHover || m_dragThumb)
            r.FillRoundRect({m_bounds.Right() - 12.0f, m_bounds.y + 2.0f, 10.0f, m_bounds.h - 4.0f}, 5.0f,
                            p.dark ? Color(0, 0, 0, 0x30) : Color(255, 255, 255, 0x90));
        r.FillRoundRect(th, th.w * 0.5f, p.scrollThumb);
    }
    r.PopClip();
}

void ScrollPanel::OnMouseMove(const MouseEvent& e)
{
    m_barHover = OnScrollbar(e.pos) || m_dragThumb;
    if (m_dragThumb)
    {
        const float trackH = m_bounds.h - 8.0f;
        const float thumbH = std::max(24.0f, trackH * m_bounds.h / std::max(1.0f, contentHeight));
        const float k = MaxScroll() / std::max(1.0f, trackH - thumbH);
        ScrollTo(m_dragStartScroll + (e.pos.y - m_dragStartY) * k, false);
        return;
    }
    Point& o = Widget::Origin();
    const float sy = m_scroll.Value();
    o.y -= sy;
    Panel::OnMouseMove(ToContent(e));
    o.y += sy;
}

bool ScrollPanel::OnMouseDown(const MouseEvent& e)
{
    if (OnScrollbar(e.pos) && e.button == MouseButton::Left)
    {
        const Rect th = ThumbRect();
        if (e.pos.y < th.y || e.pos.y > th.Bottom())
            ScrollBy(e.pos.y < th.y ? -m_bounds.h * 0.9f : m_bounds.h * 0.9f);
        m_dragThumb = true;
        m_dragStartY = e.pos.y;
        m_dragStartScroll = m_scroll.Target();
        return true;
    }
    Point& o = Widget::Origin();
    const float sy = m_scroll.Value();
    o.y -= sy;
    Panel::OnMouseDown(ToContent(e));
    o.y += sy;
    return true;
}

void ScrollPanel::OnMouseUp(const MouseEvent& e)
{
    if (m_dragThumb)
    {
        m_dragThumb = false;
        return;
    }
    Point& o = Widget::Origin();
    const float sy = m_scroll.Value();
    o.y -= sy;
    Panel::OnMouseUp(ToContent(e));
    o.y += sy;
}

bool ScrollPanel::OnMouseWheel(const MouseEvent& e)
{
    // Сначала даём шанс дочернему элементу (например, ползунку).
    Widget* hit = ChildAt(ToContent(e).pos);
    if (hit && dynamic_cast<Slider*>(hit) != nullptr && hit->OnMouseWheel(ToContent(e)))
        return true;
    if (MaxScroll() <= 0.0f)
        return hit ? hit->OnMouseWheel(ToContent(e)) : false;
    ScrollBy(-e.wheel * 90.0f);
    return true;
}

CursorType ScrollPanel::Cursor(Point p) const
{
    if (OnScrollbar(p))
        return CursorType::Arrow;
    Point c = p;
    c.y += m_scroll.Value();
    return Panel::Cursor(c);
}

void ScrollPanel::OnMouseLeave()
{
    m_barHover = false;
    Panel::OnMouseLeave();
}
