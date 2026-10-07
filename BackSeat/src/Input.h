// ============================================================================
//  Input.h — состояние клавиатуры и мыши в терминах игры.
//
//  Platform переводит сообщения WinAPI (WM_KEYDOWN, WM_MOUSEMOVE, ...) в
//  вызовы onKey/onMouse*. Логика игры работает с фиксированным шагом 60 Гц,
//  поэтому «нажатия» (фронты) защёлкиваются до ближайшего тика: beginTick()
//  переносит накопленные нажатия в pressed(), и каждое нажатие видно ровно
//  в одном тике, даже если за кадр не было ни одного обновления.
// ============================================================================
#pragma once

#include <array>

enum class Key : int {
    Left,
    Right,
    Up,
    Down,
    A,
    D,
    W,
    S,
    F,
    E,
    R,
    M,
    Q,
    Tab,
    Space,
    Enter,
    Escape,
    F11,
    Count
};

enum class MouseButton : int { Left = 0, Right = 1, Count = 2 };

class Input {
public:
    // ---- Вызовы со стороны платформы ---------------------------------------
    void onKey(Key k, bool isDown);
    // Координаты уже переведены в пиксели внутреннего буфера (320x180).
    void onMouseMove(int x, int y, bool insideGameArea);
    void onMouseButton(MouseButton b, bool isDown);
    // Потеря фокуса: отпускаем всё, чтобы клавиши не «залипли».
    void releaseAll();

    // ---- Вызов со стороны игрового цикла -----------------------------------
    // В начале каждого фиксированного шага логики.
    void beginTick();

    // ---- Запросы игры --------------------------------------------------------
    bool down(Key k) const { return down_[idx(k)]; }
    bool pressed(Key k) const { return pressed_[idx(k)]; }
    bool mouseDown(MouseButton b) const { return mouseDown_[midx(b)]; }
    bool mousePressed(MouseButton b) const { return mousePressed_[midx(b)]; }
    int mouseX() const { return mouseX_; }
    int mouseY() const { return mouseY_; }
    bool mouseInside() const { return mouseInside_; }
    // Мышь сдвинулась с прошлого тика (чтобы мышь «перехватывала» прицел).
    bool mouseMoved() const { return mouseMovedTick_; }
    // Было ли в этом тике хоть одно нажатие клавиши или кнопки мыши.
    bool anyPressed() const;

    // Направление «влево/вправо» (-1, 0, 1) с учётом A/D и стрелок.
    int horizontal() const;

private:
    static int idx(Key k) { return static_cast<int>(k); }
    static int midx(MouseButton b) { return static_cast<int>(b); }
    static constexpr int kKeys = static_cast<int>(Key::Count);
    static constexpr int kButtons = static_cast<int>(MouseButton::Count);

    std::array<bool, kKeys> down_{};
    std::array<bool, kKeys> pending_{}; // нажато после прошлого тика
    std::array<bool, kKeys> pressed_{};
    std::array<bool, kButtons> mouseDown_{};
    std::array<bool, kButtons> mousePending_{};
    std::array<bool, kButtons> mousePressed_{};
    int mouseX_ = -1;
    int mouseY_ = -1;
    bool mouseInside_ = false;
    bool mouseMovedPending_ = false;
    bool mouseMovedTick_ = false;
};
