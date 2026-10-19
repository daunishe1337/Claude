#pragma once
// ============================================================================
//  UI.h — отрисовка интерфейса: заголовок, редактор с номерами строк и
//  подсветкой, панель вывода, командная строка, заставка и Matrix-дождь.
//  Кадр собирается в одну строку с VT-последовательностями и выводится одним
//  вызовом WriteConsoleW — так нет мерцания.
// ============================================================================
#include <functional>

#include "Console.h"
#include "Editor.h"
#include "OutputPane.h"
#include "Theme.h"

enum class Focus { Editor, Command, Stdin };
enum class RunState { Idle, Compiling, Running };

struct Layout {
    int cols = 80, rows = 25;
    int editorTop = 1, editorH = 10;
    int sepRow = 11;
    int outTop = 12, outH = 10;
    int promptRow = 24;
    bool tooSmall = false;
};

struct StatusInfo {
    std::wstring compiler;     // "g++ 13.2.0" или "NO COMPILER"
    int standard = 17;
    RunState state = RunState::Idle;
    std::wstring stateText;    // "RUNNING 1.25s / 10s"
    Focus focus = Focus::Editor;
    std::wstring cmd;          // командная строка
    size_t cmdCursor = 0;
    std::wstring stdinLine;    // вводимая строка для программы
    bool blinkOn = true;
};

class UI {
public:
    UI(Console& console, const Theme& theme) : console_(console), theme_(theme) {}

    Layout layout() const;
    int gutterWidth(const Editor& ed) const;
    void render(const Layout& L, const Editor& ed, const OutputPane& out, const StatusInfo& si);
    void invalidate() { clearNext_ = true; }   // полная очистка экрана перед следующим кадром

    // Заставка. detect вызывается во время анимации и возвращает имя компилятора ("" — не найден)
    void splash(const std::function<std::wstring()>& detect);
    void matrixRain(DWORD ms);
    void resetSkip() { skip_ = false; }

private:
    Style st(const Rgb& fg, bool bold = false) const;
    Style tokStyle(int tok) const;
    Style kindStyle(OutKind k) const;

    void renderHeader(std::wstring& f, const Layout& L, const Editor& ed, const StatusInfo& si) const;
    void renderEditor(std::wstring& f, const Layout& L, const Editor& ed, const StatusInfo& si) const;
    void renderSeparator(std::wstring& f, const Layout& L, const OutputPane& out, const StatusInfo& si) const;
    void renderOutput(std::wstring& f, const Layout& L, const OutputPane& out, const StatusInfo& si) const;
    void renderPrompt(std::wstring& f, const Layout& L, const StatusInfo& si) const;

    // Помощники заставки
    bool pause(DWORD ms);  // false — пользователь нажал клавишу (пропуск анимации)
    void writeAt(int row, int col, const std::wstring& text, const Style& s);
    void typeAt(int row, int col, const std::wstring& text, const Style& s, DWORD perCharMs);

    Console& console_;
    const Theme& theme_;
    bool skip_ = false;
    mutable bool clearNext_ = true;
};
