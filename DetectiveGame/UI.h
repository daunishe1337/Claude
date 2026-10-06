#pragma once
// UI.h - текст интерфейса через GDI (шрифты, вывод строк с тенью, панели).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

enum class FontSize { Small, Normal, Large };

class UI {
public:
    UI() = default;
    ~UI();
    UI(const UI&) = delete;
    UI& operator=(const UI&) = delete;

    bool Init();

    // Рисует текст в прямоугольнике rc с тенью. format - флаги DT_* (например DT_WORDBREAK).
    void DrawString(HDC dc, const std::wstring& text, RECT rc, COLORREF color,
                    UINT format = DT_LEFT | DT_TOP, FontSize size = FontSize::Normal);

    // Высота текста при переносе по ширине width (для вёрстки блоков)
    int MeasureHeight(HDC dc, const std::wstring& text, int width, FontSize size = FontSize::Normal);

    // Закруглённая панель с рамкой
    void FillBox(HDC dc, RECT rc, COLORREF fill, COLORREF border, int radius = 10, int borderWidth = 1);

private:
    HFONT FontFor(FontSize size) const;
    HFONT fontSmall_ = nullptr, fontNormal_ = nullptr, fontLarge_ = nullptr;
};
