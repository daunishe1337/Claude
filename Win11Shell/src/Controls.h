#pragma once
// Элементы управления в стиле Fluent: кнопки, переключатели, ползунки, поля ввода,
// выпадающие списки, флажки, прокручиваемые панели и пункты навигации.

#include "Widget.h"

bool IsWordChar(wchar_t c);
void DrawSpinner(Renderer& r, float cx, float cy, float radius, float thickness, Color c);
void DrawProgressBar(Renderer& r, const Rect& rc, float value, Color fill);

// ---------------------------------------------------------------------------
enum class ButtonStyle
{
    Standard,
    Accent,
    Subtle,
    Hyperlink,
};

class Button : public Widget
{
public:
    explicit Button(std::wstring caption = L"", Icon glyph = Icon::None, ButtonStyle look = ButtonStyle::Standard);

    std::function<void()> onClick;
    std::wstring text;
    Icon icon = Icon::None;
    AppIcon appIcon = AppIcon::None;
    ButtonStyle style = ButtonStyle::Standard;
    float fontSize = 14.0f;
    FontWeight weight = FontWeight::Regular;
    float iconSize = 16.0f;
    float radius = 4.0f;
    unsigned align = TextFlags::Center;
    float padding = 12.0f;
    bool chevron = false;  // стрелка раскрытия справа
    bool checked = false;  // «включённое» состояние (подсветка как при наведении)
    bool hasIconColor = false;
    Color iconColor;

    void Draw(Renderer& r) override;
    bool OnMouseDown(const MouseEvent& e) override;
    void OnMouseUp(const MouseEvent& e) override;
};

// ---------------------------------------------------------------------------
class ToggleSwitch : public Widget
{
public:
    explicit ToggleSwitch(bool initial = false);

    bool IsOn() const { return m_on; }
    void SetOn(bool on, bool notify = false);

    std::function<void(bool)> onChange;
    bool showLabel = true;
    std::wstring onText = L"Вкл.";
    std::wstring offText = L"Откл.";

    void Draw(Renderer& r) override;
    bool OnMouseDown(const MouseEvent& e) override;
    void OnMouseUp(const MouseEvent& e) override;

private:
    bool m_on = false;
    Tween m_knob;
};

// ---------------------------------------------------------------------------
class Slider : public Widget
{
public:
    Slider() = default;

    float value = 0.5f; // 0..1
    std::function<void(float)> onChange;

    void SetValue(float v, bool notify = false);
    void Draw(Renderer& r) override;
    bool OnMouseDown(const MouseEvent& e) override;
    void OnMouseMove(const MouseEvent& e) override;
    void OnMouseUp(const MouseEvent& e) override;
    bool OnMouseWheel(const MouseEvent& e) override;

private:
    void SetFromX(float x);
};

// ---------------------------------------------------------------------------
class TextBox : public Widget
{
public:
    explicit TextBox(std::wstring placeholderText = L"");

    std::wstring placeholder;
    float fontSize = 14.0f;
    Icon leadingIcon = Icon::None;
    Icon trailingIcon = Icon::None;
    bool pill = false;
    bool password = false;
    bool borderless = false;
    std::function<void(const std::wstring&)> onChange;
    std::function<void()> onEnter;
    std::function<void()> onEscape;
    std::function<bool(UINT, const KeyMods&)> onKey; // предварительная обработка клавиш

    const std::wstring& Text() const { return m_text; }
    void SetText(const std::wstring& s, bool notify = false);
    void SelectAll();
    bool HasSelection() const { return m_caret != m_anchor; }

    bool Focusable() const override { return true; }
    void OnFocusChanged(bool focused) override;
    void Draw(Renderer& r) override;
    bool OnMouseDown(const MouseEvent& e) override;
    void OnMouseMove(const MouseEvent& e) override;
    void OnMouseUp(const MouseEvent& e) override;
    bool OnKeyDown(UINT vk, const KeyMods& mods) override;
    bool OnChar(wchar_t ch) override;
    CursorType Cursor(Point p) const override;

private:
    Rect TextArea() const;
    std::wstring Display() const;
    int HitIndex(float x) const;
    void Insert(const std::wstring& s);
    void DeleteSelection();
    void MoveCaret(int pos, bool extend);
    int WordLeft(int pos) const;
    int WordRight(int pos) const;
    void Changed();

    std::wstring m_text;
    int m_caret = 0;
    int m_anchor = 0;
    float m_scrollX = 0.0f;
    bool m_dragging = false;
    double m_lastInput = 0.0;
};

// ---------------------------------------------------------------------------
class ComboBox : public Widget
{
public:
    ComboBox() = default;

    std::vector<std::wstring> items;
    int selected = 0;
    std::function<void(int)> onChange;

    void Draw(Renderer& r) override;
    bool OnMouseDown(const MouseEvent& e) override;
    void OnMouseUp(const MouseEvent& e) override;
};

// ---------------------------------------------------------------------------
class CheckBox : public Widget
{
public:
    explicit CheckBox(std::wstring caption = L"", bool initial = false);

    std::wstring text;
    bool checked = false;
    bool radio = false;
    std::function<void(bool)> onChange;

    void Draw(Renderer& r) override;
    bool OnMouseDown(const MouseEvent& e) override;
    void OnMouseUp(const MouseEvent& e) override;
};

// ---------------------------------------------------------------------------
enum class Tone
{
    Primary,
    Secondary,
    Tertiary,
    Accent,
    Disabled,
    OnAccent,
};
Color ToneColor(Tone t);

class Label : public Widget
{
public:
    explicit Label(std::wstring caption = L"", float size = 14.0f, FontWeight w = FontWeight::Regular,
                   Tone t = Tone::Primary);

    std::wstring text;
    float fontSize = 14.0f;
    FontWeight weight = FontWeight::Regular;
    Tone tone = Tone::Primary;
    unsigned flags = TextFlags::VCenter;

    bool HitTest(Point /*p*/) const override { return false; }
    void Draw(Renderer& r) override;
};

// ---------------------------------------------------------------------------
class NavItem : public Widget
{
public:
    NavItem(std::wstring caption, Icon glyph, AppIcon app = AppIcon::None);

    std::wstring text;
    Icon icon = Icon::None;
    AppIcon appIcon = AppIcon::None;
    bool selected = false;
    float fontSize = 14.0f;
    bool hasIconColor = false;
    Color iconColor;
    bool compact = false; // только иконка
    std::function<void()> onClick;

    void Draw(Renderer& r) override;
    bool OnMouseDown(const MouseEvent& e) override;
    void OnMouseUp(const MouseEvent& e) override;
};

// ---------------------------------------------------------------------------
// Панель с вертикальной прокруткой. Дочерние элементы располагаются в координатах
// содержимого (как будто прокрутка равна нулю).
class ScrollPanel : public Panel
{
public:
    float contentHeight = 0.0f;

    float ScrollY() const { return m_scroll.Value(); }
    float MaxScroll() const { return std::max(0.0f, contentHeight - m_bounds.h); }
    void ScrollTo(float y, bool animate = true);
    void ScrollBy(float dy) { ScrollTo(m_scroll.Target() + dy); }

    void Draw(Renderer& r) override;
    bool HitTest(Point p) const override { return m_visible && m_bounds.Contains(p); }
    void OnMouseMove(const MouseEvent& e) override;
    bool OnMouseDown(const MouseEvent& e) override;
    void OnMouseUp(const MouseEvent& e) override;
    bool OnMouseWheel(const MouseEvent& e) override;
    CursorType Cursor(Point p) const override;
    void OnMouseLeave() override;

private:
    MouseEvent ToContent(const MouseEvent& e) const;
    Rect ThumbRect() const;
    bool OnScrollbar(Point p) const;

    Tween m_scroll;
    bool m_dragThumb = false;
    float m_dragStartY = 0.0f;
    float m_dragStartScroll = 0.0f;
    bool m_barHover = false;
};
