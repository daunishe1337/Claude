// Input.h - состояние клавиатуры. Заполняется из оконной процедуры.
#pragma once

namespace lrh {

class Input {
public:
    void onKey(int vk, bool down) {
        if (vk < 0 || vk > 255) return;
        if (down && !d_[vk]) p_[vk] = true;  // фронт нажатия (без автоповтора)
        d_[vk] = down;
    }
    bool down(int vk) const { return vk >= 0 && vk < 256 && d_[vk]; }
    bool pressed(int vk) const { return vk >= 0 && vk < 256 && p_[vk]; }
    // Вызывается после каждого шага логики.
    void endFrame() { for (bool& b : p_) b = false; }
    void releaseAll() { for (int i = 0; i < 256; ++i) d_[i] = p_[i] = false; }

private:
    bool d_[256] = {};
    bool p_[256] = {};
};

// Коды клавиш (совпадают с VK_*), чтобы не тянуть windows.h в логику.
enum Key {
    K_TAB = 0x09, K_ENTER = 0x0D, K_ESC = 0x1B, K_SPACE = 0x20,
    K_LEFT = 0x25, K_RIGHT = 0x27, K_A = 'A', K_D = 'D', K_F = 'F', K_R = 'R', K_M = 'M'
};

}  // namespace lrh
