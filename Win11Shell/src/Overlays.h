#pragma once
// Полноэкранные слои: представление задач, экран блокировки/входа, завершение работы, сон.

#include "Controls.h"
#include "Shell.h"

class TaskView
{
public:
    bool IsOpen() const { return m_open; }
    bool IsVisible() const { return m_open || m_anim.Value() > 0.002f; }
    void Open();
    void Close(Window* activate = nullptr);
    void Draw(Renderer& r);
    bool HitTest(Point p) const;
    void MouseMove(const MouseEvent& e);
    void MouseDown(const MouseEvent& e);
    void MouseUp(const MouseEvent& e);
    bool KeyDown(UINT vk, const KeyMods& mods);
    void MouseLeave();

private:
    struct Thumb
    {
        Window* win = nullptr;
        Rect target; // прямоугольник миниатюры (без строки заголовка)
        Rect from;   // исходное положение окна
    };
    void Layout();
    int ThumbAt(Point p, bool* onClose) const;
    Rect DesktopCard(int index) const;
    Rect CurrentRect(const Thumb& t) const;

    bool m_open = false;
    Tween m_anim;
    std::vector<Thumb> m_thumbs;
    int m_hover = -1;
    bool m_hoverClose = false;
    int m_sel = -1;
    int m_desktops = 1;
    int m_hoverDesktop = -1;
    Window* m_pending = nullptr;
};

class SystemScreens
{
public:
    enum class Mode
    {
        None,
        Lock,
        SignIn,
        Welcome,
        ShuttingDown,
        Restarting,
        Booting,
        Sleep,
    };

    bool Active() const { return m_mode != Mode::None || m_fade.Value() > 0.002f; }
    bool BlocksInput() const { return m_mode != Mode::None; }
    Mode CurrentMode() const { return m_mode; }
    void Lock();
    void Sleep();
    void Shutdown(bool restart);
    void Draw(Renderer& r);
    void MouseMove(const MouseEvent& e);
    void MouseDown(const MouseEvent& e);
    void MouseUp(const MouseEvent& e);
    bool KeyDown(UINT vk, const KeyMods& mods);
    bool Char(wchar_t ch);
    CursorType Cursor(Point p) const;

private:
    void SetMode(Mode m);
    void BuildSignIn();
    void SignIn();
    void DrawLock(Renderer& r, float offsetY);
    void DrawSignIn(Renderer& r, float alpha);

    Mode m_mode = Mode::None;
    double m_modeStart = 0.0;
    Tween m_fade;    // общая непрозрачность слоя
    Tween m_slide;   // подъём экрана блокировки
    Panel m_ui;
};
