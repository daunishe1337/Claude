#pragma once
// UI.h - текст интерфейса через GDI (шрифт, вывод строк с тенью).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

class UI {
public:
    UI() = default;
    ~UI();
    UI(const UI&) = delete;
    UI& operator=(const UI&) = delete;

    bool Init();

    // Рисует текст в прямоугольнике rc с тенью. format - флаги DT_* (например DT_WORDBREAK).
    void DrawString(HDC dc, const std::wstring& text, RECT rc, COLORREF color, UINT format = DT_LEFT | DT_TOP);

private:
    HFONT font_ = nullptr;
};
