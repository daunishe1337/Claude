#pragma once
// ============================================================================
//  Theme.h — цвета интерфейса и генерация ANSI/VT-последовательностей
// ============================================================================
#include <cstdint>
#include <string>
#include <vector>

struct Rgb {
    uint8_t r = 0, g = 0, b = 0;
    bool operator==(const Rgb& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const Rgb& o) const { return !(*this == o); }
};

// Стиль одной ячейки экрана
struct Style {
    Rgb fg;
    Rgb bg;
    bool bold = false;
    bool operator==(const Style& o) const { return fg == o.fg && bg == o.bg && bold == o.bold; }
    bool operator!=(const Style& o) const { return !(*this == o); }
};

// Набор цветов. Тему можно сменить на лету командой :theme
struct Theme {
    std::wstring name;
    Rgb bg, text, dim, bright;                                   // базовые
    Rgb keyword, type, string, number, comment, preproc, punct;  // подсветка синтаксиса
    Rgb gutter, gutterActive, lineHighlight, cursor;             // редактор
    Rgb error, warning, note, success, accent, progErr, input;   // сообщения
    Rgb headerBg, headerFg;                                      // заголовок окна

    static Theme matrix();   // классический зелёный «Матрица»
    static Theme amber();    // янтарный монитор 80-х
    static Theme ice();      // холодный голубой киберпанк
    static std::vector<std::wstring> names();
    static bool byName(const std::wstring& name, Theme& out);
};

namespace ansi {
constexpr const wchar_t* kReset = L"\x1b[0m";
std::wstring sgr(const Style& s);         // SGR с truecolor (38;2 / 48;2)
std::wstring moveTo(int row, int col);    // координаты с нуля
Rgb scale(const Rgb& c, float k);         // затемнение цвета (k = 0..1)
}
