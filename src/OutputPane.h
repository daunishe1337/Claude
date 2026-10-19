#pragma once
// ============================================================================
//  OutputPane.h — буфер панели вывода с эффектом «печатной машинки»
// ============================================================================
#include <algorithm>
#include <deque>
#include <string>
#include <vector>

enum class OutKind { Normal, System, Success, Error, Warning, Note, Context, ProgOut, ProgErr, Input, Accent, Dim };

struct OutLine {
    std::wstring text;
    OutKind kind = OutKind::Normal;
    size_t shown = 0;        // сколько символов уже «напечатано»
};

struct VisualRow {
    std::wstring text;
    OutKind kind = OutKind::Normal;
    bool typingCursor = false;  // в конце строки мигает курсор печати
};

class OutputPane {
public:
    // Системное сообщение (может содержать \n). typed = печатать посимвольно
    void print(const std::wstring& text, OutKind kind, bool typed = false);
    // Поток вывода программы: обрабатывает \r, \n, \t и вырезает ANSI-последовательности
    void appendStream(const std::wstring& chunk, OutKind kind);
    void closeOpenLine() { open_ = false; }
    void clear();

    bool tick(size_t chars);     // продвигает анимацию печати; true — что-то изменилось
    bool typing() const { return pendingTyped_ > 0; }

    void scrollBy(int rows) { scroll_ = std::max(0, scroll_ + rows); }
    void scrollToBottom() { scroll_ = 0; }
    int scroll() const { return scroll_; }

    // Строки экрана с учётом переноса, сверху вниз, не более height штук
    std::vector<VisualRow> visualRows(int width, int height) const;

private:
    void pushLine(OutLine l);
    void ensureOpen(OutKind kind);

    std::deque<OutLine> lines_;
    bool open_ = false;          // последняя строка потока ещё не завершена \n
    bool pendingCR_ = false;     // был \r, ждём \n
    int escState_ = 0;           // состояние вырезания ESC-последовательностей
    size_t pendingTyped_ = 0;    // сколько строк ещё печатается
    mutable int scroll_ = 0;     // прокрутка в экранных строках от низа
};
