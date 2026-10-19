#pragma once
// ============================================================================
//  Editor.h — многострочный текстовый редактор кода
//  Хранит текст как вектор строк UTF-16, курсор, прокрутку, историю undo/redo
//  и маркеры строк с ошибками компиляции.
// ============================================================================
#include "Common.h"
#include "Console.h"

#include <map>

class Editor {
public:
    enum Marker { MarkWarning = 1, MarkError = 2 };

    Editor();

    void setText(const std::wstring& text);          // загрузка (сбрасывает undo)
    std::wstring text() const;                       // строки через \n
    void clear();                                    // :new (можно отменить Ctrl+Z)
    void insertText(const std::wstring& text);       // вставка из буфера обмена

    // Обрабатывает клавишу; возвращает true, если клавиша обработана
    bool handleKey(const KeyEvent& k, int pageRows, bool paste);

    bool undo();
    bool redo();
    void gotoLine(int line1);                        // номер строки с 1
    void scrollBy(int delta);                        // колёсико мыши
    void layout(int height, int width);              // пересчёт прокрутки перед отрисовкой

    const std::vector<std::wstring>& lines() const { return lines_; }
    int row() const { return row_; }
    int col() const { return col_; }
    int scrollRow() const { return scrollRow_; }
    int scrollCol() const { return scrollCol_; }

    bool modified() const { return modified_; }
    void setModified(bool m) { modified_ = m; }
    const std::wstring& fileName() const { return fileName_; }
    void setFileName(const std::wstring& f) { fileName_ = f; }

    void setMarkers(const std::map<int, int>& m) { markers_ = m; }
    void clearMarkers() { markers_.clear(); }
    int markerAt(int row) const;
    int nextErrorRow(int fromRow) const;             // -1, если ошибок нет

private:
    enum class EditKind { None, Insert, Delete, Other };
    struct Snapshot {
        std::vector<std::wstring> lines;
        int row, col;
    };

    void pushUndo(EditKind kind);
    void breakGroup() { lastKind_ = EditKind::None; }
    void clampCursor();
    std::wstring& cur() { return lines_[static_cast<size_t>(row_)]; }
    int lineLen(int r) const { return static_cast<int>(lines_[static_cast<size_t>(r)].size()); }

    // элементарные операции (без записи в undo)
    void rawInsert(wchar_t c);
    void rawNewline(bool autoIndent);

    void insertChar(wchar_t c, bool paste);
    void newline(bool autoIndent);
    void backspace();
    void del();
    void tab();
    void dedent();
    void duplicateLine();
    void deleteLine();
    void moveLeft();
    void moveRight();
    void moveVert(int delta);
    void wordLeft();
    void wordRight();
    void smartHome();

    std::vector<std::wstring> lines_;
    int row_ = 0, col_ = 0, wantCol_ = 0;
    int scrollRow_ = 0, scrollCol_ = 0;
    bool follow_ = true;      // при следующей отрисовке прокрутить к курсору
    bool center_ = false;     // ... и поставить курсор в центр (переход к ошибке)
    bool modified_ = false;
    std::wstring fileName_;
    std::vector<Snapshot> undo_, redo_;
    EditKind lastKind_ = EditKind::None;
    std::map<int, int> markers_;  // строка (с 0) -> Marker
};
