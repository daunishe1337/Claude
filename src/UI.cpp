// ============================================================================
//  UI.cpp
// ============================================================================
#include "UI.h"

#include <algorithm>
#include <random>

#include "Syntax.h"

namespace {

// Построитель одной строки экрана: следит за шириной и выдаёт SGR только при смене стиля
class RowBuilder {
public:
    RowBuilder(std::wstring& out, int width) : out_(out), width_(width) {}
    void put(wchar_t ch, const Style& s) {
        if (col_ >= width_) return;
        if (!has_ || s != cur_) {
            out_ += ansi::sgr(s);
            cur_ = s;
            has_ = true;
        }
        out_ += (ch < 32) ? L' ' : ch;
        ++col_;
    }
    void text(const std::wstring& s, const Style& st) {
        for (wchar_t c : s) put(c, st);
    }
    void fillTo(int col, wchar_t ch, const Style& s) {
        while (col_ < col && col_ < width_) put(ch, s);
    }
    void fill(const Style& s) { fillTo(width_, L' ', s); }
    int col() const { return col_; }
    int left() const { return width_ - col_; }

private:
    std::wstring& out_;
    int width_;
    int col_ = 0;
    Style cur_;
    bool has_ = false;
};

int digits(int n) {
    int d = 1;
    while (n >= 10) { n /= 10; ++d; }
    return d;
}

// Текстовое поле с курсором и горизонтальной прокруткой (командная строка, stdin)
void fieldWithCursor(RowBuilder& rb, const std::wstring& text, size_t cursor, bool showCursor,
                     const Style& normal, const Style& cursorStyle) {
    int avail = std::max(1, rb.left() - 1);
    size_t start = cursor >= static_cast<size_t>(avail) ? cursor - static_cast<size_t>(avail) + 1 : 0;
    for (size_t i = start; i <= text.size() && rb.left() > 0; ++i) {
        wchar_t ch = i < text.size() ? text[i] : L' ';
        if (i == cursor && showCursor) rb.put(ch, cursorStyle);
        else if (i < text.size()) rb.put(ch, normal);
    }
}

const wchar_t* kLogo[] = {
    LR"( _   _    _    ____ _  _______ _____ ____  __  __ )",
    LR"(| | | |  / \  / ___| |/ /_   _| ____|  _ \|  \/  |)",
    LR"(| |_| | / _ \| |   | ' /  | | |  _| | |_) | |\/| |)",
    LR"(|  _  |/ ___ \ |___| . \  | | | |___|  _ <| |  | |)",
    LR"(|_| |_/_/   \_\____|_|\_\ |_| |_____|_| \_\_|  |_|)",
};

} // namespace

// ---------------------------------------------------------------------------
Style UI::st(const Rgb& fg, bool bold) const {
    Style s;
    s.fg = fg;
    s.bg = theme_.bg;
    s.bold = bold;
    return s;
}

Style UI::tokStyle(int tok) const {
    switch (static_cast<Tok>(tok)) {
        case Tok::Keyword: return st(theme_.keyword, true);
        case Tok::Type:    return st(theme_.type);
        case Tok::String:  return st(theme_.string);
        case Tok::Number:  return st(theme_.number);
        case Tok::Comment: return st(theme_.comment);
        case Tok::Preproc: return st(theme_.preproc);
        case Tok::Punct:   return st(theme_.punct);
        case Tok::Text:    break;
    }
    return st(theme_.text);
}

Style UI::kindStyle(OutKind k) const {
    switch (k) {
        case OutKind::System:  return st(theme_.text);
        case OutKind::Success: return st(theme_.success, true);
        case OutKind::Error:   return st(theme_.error, true);
        case OutKind::Warning: return st(theme_.warning);
        case OutKind::Note:    return st(theme_.note);
        case OutKind::Context: return st(theme_.dim);
        case OutKind::ProgOut: return st(theme_.bright);
        case OutKind::ProgErr: return st(theme_.progErr);
        case OutKind::Input:   return st(theme_.input, true);
        case OutKind::Accent:  return st(theme_.accent, true);
        case OutKind::Dim:     return st(theme_.dim);
        case OutKind::Normal:  break;
    }
    return st(theme_.text);
}

// ---------------------------------------------------------------------------
Layout UI::layout() const {
    Layout L;
    console_.size(L.cols, L.rows);
    L.tooSmall = L.cols < 40 || L.rows < 12;
    if (L.tooSmall) return L;
    int avail = L.rows - 3;                       // заголовок, разделитель, командная строка
    L.editorTop = 1;
    L.editorH = std::max(3, avail * 3 / 5);       // 60% — редактор, 40% — вывод
    L.sepRow = L.editorTop + L.editorH;
    L.outTop = L.sepRow + 1;
    L.promptRow = L.rows - 1;
    L.outH = std::max(1, L.promptRow - L.outTop);
    return L;
}

int UI::gutterWidth(const Editor& ed) const {
    // [маркер][номер][пробел][│][пробел]
    return std::max(3, digits(static_cast<int>(ed.lines().size()))) + 4;
}

void UI::render(const Layout& L, const Editor& ed, const OutputPane& out, const StatusInfo& si) {
    std::wstring f;
    f.reserve(static_cast<size_t>(L.cols) * static_cast<size_t>(L.rows) * 6);
    f += L"\x1b[?25l";
    if (clearNext_) {
        f += L"\x1b[0m\x1b[2J";
        clearNext_ = false;
    }
    if (L.tooSmall) {
        f += L"\x1b[0m\x1b[2J\x1b[1;1H";
        f += ansi::sgr(st(theme_.error, true));
        f += L"Окно слишком маленькое (нужно минимум 40x12)";
        f += ansi::kReset;
        console_.write(f);
        clearNext_ = true;
        return;
    }
    renderHeader(f, L, ed, si);
    renderEditor(f, L, ed, si);
    renderSeparator(f, L, out, si);
    renderOutput(f, L, out, si);
    renderPrompt(f, L, si);
    f += ansi::kReset;
    console_.write(f);
}

void UI::renderHeader(std::wstring& f, const Layout& L, const Editor& ed, const StatusInfo& si) const {
    f += ansi::moveTo(0, 0);
    RowBuilder rb(f, L.cols);
    Style hs;
    hs.fg = theme_.headerFg;
    hs.bg = theme_.headerBg;
    hs.bold = true;

    std::wstring file = ed.fileName().empty() ? L"untitled.cpp" : util::baseName(ed.fileName());
    std::wstring left = L" ▓▒░ HACKTERM ░▒▓  " + file + (ed.modified() ? L" [+]" : L"");
    std::wstring right = si.compiler + L" │ C++" + std::to_wstring(si.standard) + L" │ Ln " +
                         std::to_wstring(ed.row() + 1) + L", Col " + std::to_wstring(ed.col() + 1) + L" ";
    if (static_cast<int>(left.size() + right.size()) > L.cols) right.clear();
    rb.text(left, hs);
    rb.fillTo(L.cols - static_cast<int>(right.size()), L' ', hs);
    rb.text(right, hs);
    rb.fill(hs);
}

void UI::renderEditor(std::wstring& f, const Layout& L, const Editor& ed, const StatusInfo& si) const {
    const auto& lines = ed.lines();
    const int n = static_cast<int>(lines.size());
    const int gw = gutterWidth(ed);
    const int numW = gw - 4;
    const int textW = std::max(1, L.cols - gw);

    // Состояние многострочного комментария до первой видимой строки
    bool inBlock = false;
    std::vector<Tok> toks;
    for (int i = 0; i < ed.scrollRow() && i < n; ++i) Highlighter::line(lines[static_cast<size_t>(i)], inBlock, toks);

    const bool focused = si.focus == Focus::Editor;
    for (int y = 0; y < L.editorH; ++y) {
        int li = ed.scrollRow() + y;
        f += ansi::moveTo(L.editorTop + y, 0);
        RowBuilder rb(f, L.cols);
        bool curLine = (li == ed.row());
        Rgb lineBg = curLine ? theme_.lineHighlight : theme_.bg;

        if (li >= n) {
            Style tilde = st(theme_.gutter);
            rb.text(std::wstring(static_cast<size_t>(numW), L' ') + L" ~", tilde);
            rb.fill(st(theme_.text));
            continue;
        }
        // Гуттер: маркер ошибки/предупреждения и номер строки
        int mark = ed.markerAt(li);
        if (mark == Editor::MarkError) rb.put(L'►', st(theme_.error, true));
        else if (mark == Editor::MarkWarning) rb.put(L'•', st(theme_.warning, true));
        else rb.put(L' ', st(theme_.gutter));
        std::wstring num = std::to_wstring(li + 1);
        num = std::wstring(static_cast<size_t>(std::max(0, numW - static_cast<int>(num.size()))), L' ') + num;
        Style ns = curLine ? st(theme_.gutterActive, true) : st(theme_.gutter);
        if (mark == Editor::MarkError) ns = st(theme_.error, true);
        rb.text(num, ns);
        rb.text(L" │ ", st(theme_.gutter));

        // Текст с подсветкой
        const std::wstring& line = lines[static_cast<size_t>(li)];
        Highlighter::line(line, inBlock, toks);
        for (int x = 0; x < textW; ++x) {
            int ci = ed.scrollCol() + x;
            bool inText = ci < static_cast<int>(line.size());
            wchar_t ch = inText ? line[static_cast<size_t>(ci)] : L' ';
            Style s = inText ? tokStyle(static_cast<int>(toks[static_cast<size_t>(ci)])) : st(theme_.text);
            s.bg = lineBg;
            if (curLine && ci == ed.col()) {
                if (focused && si.blinkOn) {          // мигающий блочный курсор
                    s.fg = theme_.bg;
                    s.bg = theme_.cursor;
                } else if (!focused) {                // редактор без фокуса — тусклый курсор
                    s.fg = theme_.bg;
                    s.bg = theme_.dim;
                }
            }
            rb.put(ch, s);
        }
    }
}

void UI::renderSeparator(std::wstring& f, const Layout& L, const OutputPane& out, const StatusInfo& si) const {
    f += ansi::moveTo(L.sepRow, 0);
    RowBuilder rb(f, L.cols);
    Style line = st(theme_.dim);
    Style label = st(theme_.bright, true);
    Style stateSt = st(theme_.dim, true);
    if (si.state == RunState::Compiling) stateSt = st(theme_.warning, true);
    if (si.state == RunState::Running) stateSt = si.blinkOn ? st(theme_.accent, true) : st(theme_.note);

    rb.text(L"──[ ", line);
    rb.text(L"OUTPUT", label);
    rb.text(L" ]──[ ", line);
    rb.text(si.stateText, stateSt);
    rb.text(L" ]", line);
    if (out.scroll() > 0) {
        rb.text(L"──[ ", line);
        rb.text(L"↑ " + std::to_wstring(out.scroll()), st(theme_.warning));
        rb.text(L" ]", line);
    }
    std::wstring right = L"[ F5 run · Esc cmd · F1 help ]──";
    if (rb.left() > static_cast<int>(right.size()) + 2) {
        rb.fillTo(L.cols - static_cast<int>(right.size()), L'─', line);
        rb.text(right, line);
    }
    rb.fillTo(L.cols, L'─', line);
}

void UI::renderOutput(std::wstring& f, const Layout& L, const OutputPane& out, const StatusInfo& si) const {
    auto rows = out.visualRows(L.cols - 2, L.outH);
    for (int y = 0; y < L.outH; ++y) {
        f += ansi::moveTo(L.outTop + y, 0);
        RowBuilder rb(f, L.cols);
        rb.put(L' ', st(theme_.text));
        if (y < static_cast<int>(rows.size())) {
            const VisualRow& r = rows[static_cast<size_t>(y)];
            rb.text(r.text, kindStyle(r.kind));
            if (r.typingCursor && si.blinkOn) rb.put(L'█', st(theme_.text));
        }
        rb.fill(st(theme_.text));
    }
}

void UI::renderPrompt(std::wstring& f, const Layout& L, const StatusInfo& si) const {
    f += ansi::moveTo(L.promptRow, 0);
    // Последнюю ячейку экрана не трогаем: на старых conhost запись в неё прокручивает экран
    const int W = L.cols - 1;
    RowBuilder rb(f, W);
    Style cursor;
    cursor.fg = theme_.bg;
    cursor.bg = theme_.cursor;

    if (si.focus == Focus::Stdin) {
        rb.text(L"stdin> ", st(theme_.accent, true));
        std::wstring hint = L"  Ctrl+C kill · Ctrl+Z EOF · Esc editor ";
        int fieldEnd = W - static_cast<int>(hint.size());
        RowBuilder field(f, std::max(rb.col() + 2, fieldEnd) - rb.col());
        fieldWithCursor(field, si.stdinLine, si.stdinLine.size(), si.blinkOn, st(theme_.input), cursor);
        field.fill(st(theme_.text));
        RowBuilder tail(f, W - std::max(rb.col() + 2, fieldEnd));
        tail.text(hint, st(theme_.dim));
        tail.fill(st(theme_.text));
        return;
    }
    bool active = si.focus == Focus::Command;
    rb.text(L"root", active ? st(theme_.error, true) : st(theme_.dim));
    rb.text(L"@hack", active ? st(theme_.text, true) : st(theme_.dim));
    rb.text(L":~# ", active ? st(theme_.bright) : st(theme_.dim));
    if (active) {
        fieldWithCursor(rb, si.cmd, si.cmdCursor, si.blinkOn, st(theme_.bright), cursor);
    } else {
        rb.text(L"[Esc] командная строка · [F5] запуск · [F1] помощь", st(theme_.dim));
    }
    rb.fill(st(theme_.text));
}

// ===========================================================================
//  Заставка
// ===========================================================================
bool UI::pause(DWORD ms) {
    if (skip_) return false;
    ULONGLONG end = GetTickCount64() + ms;
    for (;;) {
        ULONGLONG now = GetTickCount64();
        if (now >= end) return true;
        if (console_.waitInput(static_cast<DWORD>(end - now))) {
            InputBatch b = console_.read();
            if (!b.keys.empty()) {
                skip_ = true;
                return false;
            }
        }
    }
}

void UI::writeAt(int row, int col, const std::wstring& text, const Style& s) {
    console_.write(ansi::moveTo(row, col) + ansi::sgr(s) + text + ansi::kReset);
}

void UI::typeAt(int row, int col, const std::wstring& text, const Style& s, DWORD perCharMs) {
    if (skip_) {
        writeAt(row, col, text, s);
        return;
    }
    for (size_t i = 0; i < text.size(); ++i) {
        writeAt(row, col + static_cast<int>(i), std::wstring(1, text[i]), s);
        if (!pause(perCharMs)) {  // клавиша — допечатываем остаток сразу
            writeAt(row, col + static_cast<int>(i) + 1, text.substr(i + 1), s);
            return;
        }
    }
}

void UI::matrixRain(DWORD ms) {
    if (skip_) return;
    int cols = 80, rows = 25;
    console_.size(cols, rows);
    if (cols < 2 || rows < 2) return;

    std::mt19937 rng(static_cast<unsigned>(GetTickCount64()));
    const std::wstring glyphs = L"0123456789ABCDEFXZ@#$%&*+=<>?/\\|{}[]";
    auto glyph = [&]() { return glyphs[rng() % glyphs.size()]; };

    struct Drop {
        float head, speed;
        int len;
    };
    auto respawn = [&](Drop& d, bool initial) {
        d.head = -static_cast<float>(rng() % static_cast<unsigned>(initial ? rows * 2 : rows));
        d.speed = 0.35f + static_cast<float>(rng() % 100) / 100.0f * 0.9f;
        d.len = 6 + static_cast<int>(rng() % static_cast<unsigned>(std::max(2, rows * 2 / 3)));
    };
    std::vector<Drop> drops(static_cast<size_t>(cols));
    for (auto& d : drops) respawn(d, true);
    std::vector<wchar_t> grid(static_cast<size_t>(cols * rows));
    for (auto& c : grid) c = glyph();

    Style blank = st(theme_.text);
    ULONGLONG end = GetTickCount64() + ms;
    std::wstring f;
    while (GetTickCount64() < end && !skip_) {
        for (int i = 0; i < cols * rows / 25; ++i) grid[rng() % grid.size()] = glyph();  // мерцание символов
        f.clear();
        f += L"\x1b[?25l";
        for (int y = 0; y < rows; ++y) {
            f += ansi::moveTo(y, 0);
            RowBuilder rb(f, cols);
            for (int x = 0; x < cols; ++x) {
                const Drop& d = drops[static_cast<size_t>(x)];
                float dist = d.head - static_cast<float>(y);
                if (dist < 0 || dist >= static_cast<float>(d.len)) {
                    rb.put(L' ', blank);
                    continue;
                }
                Style s = blank;
                if (dist < 1) {
                    s.fg = theme_.bright;  // яркая «голова» капли
                    s.bold = true;
                } else {
                    float k = 1.0f - dist / static_cast<float>(d.len);
                    k = static_cast<float>(static_cast<int>(k * 8)) / 8.0f;  // квантуем — меньше escape-кодов
                    s.fg = ansi::scale(theme_.text, 0.2f + 0.8f * k);
                }
                rb.put(grid[static_cast<size_t>(y * cols + x)], s);
            }
        }
        console_.write(f);
        for (auto& d : drops) {
            d.head += d.speed;
            if (d.head - static_cast<float>(d.len) > static_cast<float>(rows)) respawn(d, false);
        }
        pause(33);
    }
    console_.write(L"\x1b[0m\x1b[2J");
    clearNext_ = true;
}

void UI::splash(const std::function<std::wstring()>& detect) {
    skip_ = false;
    console_.write(L"\x1b[0m\x1b[2J\x1b[?25l");
    matrixRain(1600);

    int cols = 80, rows = 25;
    console_.size(cols, rows);
    console_.write(L"\x1b[0m\x1b[2J");

    // ASCII-логотип с вертикальным градиентом
    const int logoN = static_cast<int>(sizeof(kLogo) / sizeof(kLogo[0]));
    const int logoW = static_cast<int>(std::wstring(kLogo[0]).size());
    int top = std::max(0, rows / 2 - 9);
    int left = std::max(0, (cols - logoW) / 2);
    for (int i = 0; i < logoN; ++i) {
        float k = 1.0f - static_cast<float>(i) * 0.12f;
        writeAt(top + i, left, kLogo[i], st(ansi::scale(theme_.text, k), true));
        pause(70);
    }
    std::wstring sub = L"[ live C++ compile terminal :: v1.0 ]";
    typeAt(top + logoN + 1, std::max(0, (cols - static_cast<int>(sub.size())) / 2), sub, st(theme_.dim), 10);

    // Строки «взлома»
    int y = top + logoN + 3;
    int x = std::max(1, (cols - 60) / 2);
    const int dotsTo = x + 44;
    auto step = [&](const std::wstring& label, const std::function<std::pair<std::wstring, bool>()>& action) {
        std::wstring text = L"[*] " + label;
        typeAt(y, x, text, st(theme_.text), 12);
        auto result = action();
        for (int c = x + static_cast<int>(text.size()); c < dotsTo; ++c) {
            writeAt(y, c, L".", st(theme_.dim));
            pause(12);
        }
        typeAt(y, dotsTo + 1, result.first, result.second ? st(theme_.success, true) : st(theme_.error, true), 15);
        ++y;
    };
    auto ok = []() { return std::make_pair(std::wstring(L"OK"), true); };
    step(L"Initializing kernel modules", ok);
    step(L"Bypassing firewall", ok);
    step(L"Scanning for compilers", [&]() {
        std::wstring name = detect();
        return name.empty() ? std::make_pair(std::wstring(L"NOT FOUND"), false) : std::make_pair(name, true);
    });
    step(L"Decrypting session", ok);

    // Мигающая плашка ACCESS GRANTED
    const std::wstring b1 = L"╔══════════════════════╗";
    const std::wstring b2 = L"║    ACCESS  GRANTED    ║";
    const std::wstring b3 = L"╚══════════════════════╝";
    int by = y + 1;
    int bx = std::max(0, (cols - static_cast<int>(b2.size())) / 2);
    Style inv;
    inv.fg = theme_.bg;
    inv.bg = theme_.text;
    inv.bold = true;
    for (int i = 0; i < 3; ++i) {
        writeAt(by, bx, b1, inv);
        writeAt(by + 1, bx, b2, inv);
        writeAt(by + 2, bx, b3, inv);
        pause(160);
        writeAt(by, bx, b1, st(theme_.text, true));
        writeAt(by + 1, bx, b2, st(theme_.text, true));
        writeAt(by + 2, bx, b3, st(theme_.text, true));
        pause(110);
    }
    pause(600);
    clearNext_ = true;
}
