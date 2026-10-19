// ============================================================================
//  Console.cpp
// ============================================================================
#include "Console.h"

Console::~Console() { restore(); }

bool Console::init(std::wstring& error) {
    in_ = GetStdHandle(STD_INPUT_HANDLE);
    out_ = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!GetConsoleMode(out_, &oldOutMode_) || !GetConsoleMode(in_, &oldInMode_)) {
        error = L"HackTerm должен запускаться в консоли (cmd, PowerShell или Windows Terminal), "
                L"а не с перенаправленным вводом/выводом.";
        return false;
    }
    // VT-обработка вывода: ANSI-цвета, позиционирование курсора, альтернативный буфер
    if (!SetConsoleMode(out_, oldOutMode_ | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        error = L"Консоль не поддерживает ENABLE_VIRTUAL_TERMINAL_PROCESSING. Нужна Windows 10 (1607+) или Windows Terminal.";
        return false;
    }
    // Ввод: без ENABLE_PROCESSED_INPUT (Ctrl+C приходит как обычная клавиша),
    // без QuickEdit (иначе не будет событий колёсика мыши), с событиями изменения размера.
    SetConsoleMode(in_, ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS);

    oldOutCp_ = GetConsoleOutputCP();
    oldInCp_ = GetConsoleCP();
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Альтернативный буфер (история консоли не портится), скрытый курсор, без автопереноса
    write(L"\x1b[?1049h\x1b[?25l\x1b[?7l\x1b[0m\x1b[2J");
    FlushConsoleInputBuffer(in_);
    active_ = true;
    return true;
}

void Console::restore() {
    if (!active_) return;
    active_ = false;
    write(L"\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l");
    SetConsoleMode(out_, oldOutMode_);
    SetConsoleMode(in_, oldInMode_);
    if (oldOutCp_) SetConsoleOutputCP(oldOutCp_);
    if (oldInCp_) SetConsoleCP(oldInCp_);
}

void Console::size(int& cols, int& rows) const {
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(out_, &info)) {
        cols = info.srWindow.Right - info.srWindow.Left + 1;
        rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    } else {
        cols = 80;
        rows = 25;
    }
}

void Console::write(const std::wstring& s) const {
    const wchar_t* p = s.data();
    size_t left = s.size();
    while (left > 0) {
        DWORD chunk = static_cast<DWORD>(left > 32768 ? 32768 : left);
        DWORD written = 0;
        if (!WriteConsoleW(out_, p, chunk, &written, nullptr) || written == 0) break;
        p += written;
        left -= written;
    }
}

bool Console::waitInput(DWORD ms) const {
    return WaitForSingleObject(in_, ms) == WAIT_OBJECT_0;
}

InputBatch Console::read() {
    InputBatch batch;
    DWORD count = 0;
    if (!GetNumberOfConsoleInputEvents(in_, &count) || count == 0) return batch;
    std::vector<INPUT_RECORD> recs(count > 1024 ? 1024 : count);
    DWORD got = 0;
    if (!ReadConsoleInputW(in_, recs.data(), static_cast<DWORD>(recs.size()), &got)) return batch;

    CONSOLE_SCREEN_BUFFER_INFO info{};
    GetConsoleScreenBufferInfo(out_, &info);
    int textKeys = 0;

    for (DWORD i = 0; i < got; ++i) {
        const INPUT_RECORD& r = recs[i];
        if (r.EventType == KEY_EVENT) {
            const KEY_EVENT_RECORD& k = r.Event.KeyEvent;
            if (!k.bKeyDown) continue;
            KeyEvent e;
            e.vk = k.wVirtualKeyCode;
            e.ch = k.uChar.UnicodeChar;
            DWORD st = k.dwControlKeyState;
            e.ctrl = (st & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
            e.alt = (st & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
            e.shift = (st & SHIFT_PRESSED) != 0;
            // AltGr (= Ctrl+Alt) на европейских раскладках печатает обычные символы: { } [ ] \ |
            if (e.ctrl && e.alt && e.ch >= 32) e.ctrl = e.alt = false;
            // Одиночные модификаторы не интересны
            if (e.vk == VK_CONTROL || e.vk == VK_SHIFT || e.vk == VK_MENU || e.vk == VK_CAPITAL) continue;
            if (e.ch >= 32 || e.vk == VK_RETURN || e.vk == VK_TAB) ++textKeys;
            WORD rep = k.wRepeatCount ? k.wRepeatCount : 1;
            for (WORD n = 0; n < rep; ++n) batch.keys.push_back(e);
        } else if (r.EventType == MOUSE_EVENT) {
            const MOUSE_EVENT_RECORD& m = r.Event.MouseEvent;
            if (m.dwEventFlags & MOUSE_WHEELED) {
                MouseWheel w;
                w.row = m.dwMousePosition.Y - info.srWindow.Top;
                w.col = m.dwMousePosition.X - info.srWindow.Left;
                w.delta = static_cast<short>(HIWORD(m.dwButtonState)) > 0 ? 1 : -1;
                batch.wheels.push_back(w);
            }
        } else if (r.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            batch.resized = true;
        }
    }
    // Человек не печатает больше 4 символов за ~15 мс — значит, это вставка
    batch.paste = textKeys > 4;
    return batch;
}

void Console::flushInput() { FlushConsoleInputBuffer(in_); }

void Console::setTitle(const std::wstring& title) { SetConsoleTitleW(title.c_str()); }
