// ============================================================================
//  Theme.cpp — предустановленные темы и VT-последовательности
// ============================================================================
#include "Theme.h"

Theme Theme::matrix() {
    Theme t;
    t.name = L"matrix";
    t.bg = {0, 0, 0};          t.text = {0, 255, 65};     t.dim = {0, 110, 30};     t.bright = {190, 255, 190};
    t.keyword = {130, 255, 130}; t.type = {0, 215, 255};  t.string = {255, 225, 80}; t.number = {255, 175, 60};
    t.comment = {0, 125, 45};  t.preproc = {0, 255, 200};  t.punct = {0, 190, 60};
    t.gutter = {0, 95, 30};    t.gutterActive = {0, 255, 65}; t.lineHighlight = {0, 28, 10}; t.cursor = {0, 255, 65};
    t.error = {255, 70, 70};   t.warning = {255, 215, 0}; t.note = {0, 200, 255};   t.success = {90, 255, 90};
    t.accent = {0, 255, 255};  t.progErr = {255, 140, 80}; t.input = {200, 255, 255};
    t.headerBg = {0, 200, 50}; t.headerFg = {0, 0, 0};
    return t;
}

Theme Theme::amber() {
    Theme t = matrix();
    t.name = L"amber";
    t.text = {255, 176, 0};    t.dim = {125, 85, 0};      t.bright = {255, 225, 150};
    t.keyword = {255, 215, 100}; t.type = {255, 130, 40}; t.string = {255, 245, 190}; t.number = {255, 150, 90};
    t.comment = {140, 95, 10}; t.preproc = {255, 200, 60}; t.punct = {210, 145, 0};
    t.gutter = {110, 75, 0};   t.gutterActive = {255, 176, 0}; t.lineHighlight = {35, 22, 0}; t.cursor = {255, 176, 0};
    t.headerBg = {230, 150, 0};
    return t;
}

Theme Theme::ice() {
    Theme t = matrix();
    t.name = L"ice";
    t.text = {0, 220, 255};    t.dim = {0, 90, 120};      t.bright = {200, 245, 255};
    t.keyword = {120, 170, 255}; t.type = {190, 130, 255}; t.string = {140, 255, 170}; t.number = {255, 200, 90};
    t.comment = {50, 110, 140}; t.preproc = {255, 120, 220}; t.punct = {0, 170, 210};
    t.gutter = {0, 80, 110};   t.gutterActive = {0, 220, 255}; t.lineHighlight = {0, 20, 32}; t.cursor = {0, 220, 255};
    t.headerBg = {0, 170, 210};
    return t;
}

std::vector<std::wstring> Theme::names() { return {L"matrix", L"amber", L"ice"}; }

bool Theme::byName(const std::wstring& name, Theme& out) {
    if (name == L"matrix") { out = matrix(); return true; }
    if (name == L"amber")  { out = amber();  return true; }
    if (name == L"ice")    { out = ice();    return true; }
    return false;
}

namespace ansi {

static void appendRgb(std::wstring& s, const Rgb& c) {
    s += std::to_wstring(c.r); s += L';';
    s += std::to_wstring(c.g); s += L';';
    s += std::to_wstring(c.b);
}

std::wstring sgr(const Style& st) {
    std::wstring s = L"\x1b[0;";
    if (st.bold) s += L"1;";
    s += L"38;2;"; appendRgb(s, st.fg);
    s += L";48;2;"; appendRgb(s, st.bg);
    s += L'm';
    return s;
}

std::wstring moveTo(int row, int col) {
    return L"\x1b[" + std::to_wstring(row + 1) + L";" + std::to_wstring(col + 1) + L"H";
}

Rgb scale(const Rgb& c, float k) {
    if (k < 0) k = 0;
    if (k > 1) k = 1;
    return {static_cast<uint8_t>(c.r * k), static_cast<uint8_t>(c.g * k), static_cast<uint8_t>(c.b * k)};
}

} // namespace ansi
