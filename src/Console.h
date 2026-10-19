#pragma once
// ============================================================================
//  Console.h — низкоуровневая работа с консолью Windows (часть слоя UI)
//  Включает VT-режим, альтернативный экранный буфер, читает сырые события
//  клавиатуры/мыши через ReadConsoleInputW.
// ============================================================================
#include "Common.h"

struct KeyEvent {
    WORD vk = 0;          // виртуальный код клавиши
    wchar_t ch = 0;       // символ (если есть)
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
};

struct MouseWheel {
    int row = 0, col = 0;
    int delta = 0;        // > 0 — прокрутка вверх
};

struct InputBatch {
    std::vector<KeyEvent> keys;
    std::vector<MouseWheel> wheels;
    bool resized = false;
    bool paste = false;   // много символов за один раз — скорее всего вставка из буфера
};

class Console {
public:
    Console() = default;
    ~Console();
    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;

    bool init(std::wstring& error);
    void restore();
    void size(int& cols, int& rows) const;
    void write(const std::wstring& s) const;
    bool waitInput(DWORD ms) const;
    InputBatch read();
    void flushInput();
    void setTitle(const std::wstring& title);

private:
    HANDLE in_ = nullptr;
    HANDLE out_ = nullptr;
    DWORD oldInMode_ = 0;
    DWORD oldOutMode_ = 0;
    UINT oldOutCp_ = 0;
    UINT oldInCp_ = 0;
    bool active_ = false;
};
