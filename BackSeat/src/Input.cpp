// ============================================================================
//  Input.cpp — защёлкивание нажатий для фиксированного шага логики.
// ============================================================================
#include "Input.h"

void Input::onKey(Key k, bool isDown) {
    const int i = idx(k);
    if (i < 0 || i >= kKeys) return;
    if (isDown && !down_[i]) pending_[i] = true; // автоповтор не считается нажатием
    down_[i] = isDown;
}

void Input::onMouseMove(int x, int y, bool insideGameArea) {
    if (x != mouseX_ || y != mouseY_) mouseMovedPending_ = true;
    mouseX_ = x;
    mouseY_ = y;
    mouseInside_ = insideGameArea;
}

void Input::onMouseButton(MouseButton b, bool isDown) {
    const int i = midx(b);
    if (i < 0 || i >= kButtons) return;
    if (isDown && !mouseDown_[i]) mousePending_[i] = true;
    mouseDown_[i] = isDown;
}

void Input::releaseAll() {
    down_.fill(false);
    mouseDown_.fill(false);
}

void Input::beginTick() {
    pressed_ = pending_;
    pending_.fill(false);
    mousePressed_ = mousePending_;
    mousePending_.fill(false);
    mouseMovedTick_ = mouseMovedPending_;
    mouseMovedPending_ = false;
}

bool Input::anyPressed() const {
    for (bool p : pressed_)
        if (p) return true;
    for (bool p : mousePressed_)
        if (p) return true;
    return false;
}

int Input::horizontal() const {
    int h = 0;
    if (down(Key::A) || down(Key::Left)) h -= 1;
    if (down(Key::D) || down(Key::Right)) h += 1;
    return h;
}
