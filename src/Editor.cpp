// ============================================================================
//  Editor.cpp
// ============================================================================
#include "Editor.h"

#include <algorithm>
#include <cwctype>

namespace {
constexpr int kTab = 4;
constexpr size_t kUndoLimit = 300;

int leadingSpaces(const std::wstring& s) {
    int n = 0;
    while (n < static_cast<int>(s.size()) && s[static_cast<size_t>(n)] == L' ') ++n;
    return n;
}

bool isWordChar(wchar_t c) { return std::iswalnum(c) || c == L'_'; }

std::wstring ltrim(const std::wstring& s) {
    size_t p = s.find_first_not_of(L' ');
    return p == std::wstring::npos ? std::wstring() : s.substr(p);
}
} // namespace

Editor::Editor() { lines_.push_back(L""); }

void Editor::setText(const std::wstring& text) {
    lines_ = util::splitLines(text);
    for (auto& l : lines_) l = util::expandTabs(l, kTab);
    // Убираем одну завершающую пустую строку, появившуюся из-за финального \n
    if (lines_.size() > 1 && lines_.back().empty()) lines_.pop_back();
    row_ = col_ = wantCol_ = 0;
    scrollRow_ = scrollCol_ = 0;
    undo_.clear();
    redo_.clear();
    lastKind_ = EditKind::None;
    modified_ = false;
    markers_.clear();
    follow_ = true;
}

std::wstring Editor::text() const {
    std::wstring r;
    for (size_t i = 0; i < lines_.size(); ++i) {
        r += lines_[i];
        r += L'\n';
    }
    return r;
}

void Editor::clear() {
    pushUndo(EditKind::Other);
    lines_.assign(1, L"");
    row_ = col_ = wantCol_ = 0;
    markers_.clear();
    follow_ = true;
}

void Editor::insertText(const std::wstring& text) {
    if (text.empty()) return;
    pushUndo(EditKind::Other);
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t c = text[i];
        if (c == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') ++i;
            rawNewline(false);
        } else if (c == L'\n') {
            rawNewline(false);
        } else if (c == L'\t') {
            int spaces = kTab - (col_ % kTab);
            for (int s = 0; s < spaces; ++s) rawInsert(L' ');
        } else if (c >= 32) {
            rawInsert(c);
        }
    }
    breakGroup();
    follow_ = true;
}

// ---------------------------------------------------------------------------
bool Editor::handleKey(const KeyEvent& k, int pageRows, bool paste) {
    follow_ = true;
    if (k.ctrl && !k.alt) {
        switch (k.vk) {
            case VK_LEFT:  wordLeft(); return true;
            case VK_RIGHT: wordRight(); return true;
            case VK_HOME:  breakGroup(); row_ = col_ = wantCol_ = 0; return true;
            case VK_END:
                breakGroup();
                row_ = static_cast<int>(lines_.size()) - 1;
                col_ = wantCol_ = lineLen(row_);
                return true;
            case 'Z': undo(); return true;
            case 'Y': redo(); return true;
            case 'D': duplicateLine(); return true;
            case 'K': deleteLine(); return true;
            default: return false;
        }
    }
    if (k.alt) return false;

    switch (k.vk) {
        case VK_LEFT:   moveLeft(); return true;
        case VK_RIGHT:  moveRight(); return true;
        case VK_UP:     moveVert(-1); return true;
        case VK_DOWN:   moveVert(1); return true;
        case VK_PRIOR:  moveVert(-std::max(1, pageRows - 1)); return true;
        case VK_NEXT:   moveVert(std::max(1, pageRows - 1)); return true;
        case VK_HOME:   smartHome(); return true;
        case VK_END:    breakGroup(); col_ = wantCol_ = lineLen(row_); return true;
        case VK_BACK:   backspace(); return true;
        case VK_DELETE: del(); return true;
        case VK_TAB:    if (k.shift) dedent(); else tab(); return true;
        case VK_RETURN: newline(!paste); return true;
        default: break;
    }
    if (k.ch == L'\r' || k.ch == L'\n') { newline(!paste); return true; }
    if (k.ch == L'\t') { tab(); return true; }
    if (k.ch >= 32 && k.ch != 127) {
        insertChar(k.ch, paste);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
void Editor::pushUndo(EditKind kind) {
    modified_ = true;
    // Подряд идущие однотипные правки (набор слова, серия Backspace) — один шаг отмены
    if (kind != EditKind::Other && kind == lastKind_) return;
    undo_.push_back({lines_, row_, col_});
    if (undo_.size() > kUndoLimit) undo_.erase(undo_.begin());
    redo_.clear();
    lastKind_ = kind;
}

bool Editor::undo() {
    if (undo_.empty()) return false;
    redo_.push_back({lines_, row_, col_});
    Snapshot s = std::move(undo_.back());
    undo_.pop_back();
    lines_ = std::move(s.lines);
    row_ = s.row;
    col_ = s.col;
    clampCursor();
    wantCol_ = col_;
    lastKind_ = EditKind::None;
    modified_ = true;
    follow_ = true;
    return true;
}

bool Editor::redo() {
    if (redo_.empty()) return false;
    undo_.push_back({lines_, row_, col_});
    Snapshot s = std::move(redo_.back());
    redo_.pop_back();
    lines_ = std::move(s.lines);
    row_ = s.row;
    col_ = s.col;
    clampCursor();
    wantCol_ = col_;
    lastKind_ = EditKind::None;
    modified_ = true;
    follow_ = true;
    return true;
}

void Editor::clampCursor() {
    if (lines_.empty()) lines_.push_back(L"");
    row_ = std::clamp(row_, 0, static_cast<int>(lines_.size()) - 1);
    col_ = std::clamp(col_, 0, lineLen(row_));
}

// ---------------------------------------------------------------------------
void Editor::rawInsert(wchar_t c) {
    cur().insert(static_cast<size_t>(col_), 1, c);
    ++col_;
    wantCol_ = col_;
}

void Editor::rawNewline(bool autoIndent) {
    std::wstring& line = cur();
    std::wstring rest = line.substr(static_cast<size_t>(col_));
    line.erase(static_cast<size_t>(col_));

    if (!autoIndent) {
        lines_.insert(lines_.begin() + row_ + 1, rest);
        ++row_;
        col_ = wantCol_ = 0;
        return;
    }
    // Автоотступ: повторяем отступ текущей строки, после '{' — на уровень глубже
    int indent = leadingSpaces(line);
    std::wstring trimmed = util::trim(line);
    bool opens = !trimmed.empty() && trimmed.back() == L'{';
    std::wstring restTrim = ltrim(rest);

    if (opens && !restTrim.empty() && restTrim[0] == L'}') {
        // Случай "{|}" — раскрываем блок на три строки
        lines_.insert(lines_.begin() + row_ + 1, std::wstring(static_cast<size_t>(indent + kTab), L' '));
        lines_.insert(lines_.begin() + row_ + 2, std::wstring(static_cast<size_t>(indent), L' ') + restTrim);
        ++row_;
        col_ = wantCol_ = indent + kTab;
        return;
    }
    int newIndent = indent + (opens ? kTab : 0);
    lines_.insert(lines_.begin() + row_ + 1, std::wstring(static_cast<size_t>(newIndent), L' ') + restTrim);
    ++row_;
    col_ = wantCol_ = newIndent;
}

void Editor::insertChar(wchar_t c, bool paste) {
    pushUndo(EditKind::Insert);
    // Авто-уменьшение отступа при вводе '}' в пустой строке
    if (c == L'}' && !paste) {
        std::wstring& line = cur();
        if (col_ >= kTab && leadingSpaces(line) >= col_) {
            int remove = std::min(kTab, col_);
            line.erase(static_cast<size_t>(col_ - remove), static_cast<size_t>(remove));
            col_ -= remove;
        }
    }
    rawInsert(c);
}

void Editor::newline(bool autoIndent) {
    pushUndo(EditKind::Other);
    rawNewline(autoIndent);
    breakGroup();
}

void Editor::backspace() {
    if (col_ > 0) {
        pushUndo(EditKind::Delete);
        std::wstring& line = cur();
        int remove = 1;
        // В зоне отступа удаляем пробелы до предыдущей позиции табуляции
        if (leadingSpaces(line) >= col_) remove = ((col_ - 1) % kTab) + 1;
        line.erase(static_cast<size_t>(col_ - remove), static_cast<size_t>(remove));
        col_ -= remove;
    } else if (row_ > 0) {
        pushUndo(EditKind::Other);
        int prevLen = lineLen(row_ - 1);
        lines_[static_cast<size_t>(row_ - 1)] += cur();
        lines_.erase(lines_.begin() + row_);
        --row_;
        col_ = prevLen;
        breakGroup();
    }
    wantCol_ = col_;
}

void Editor::del() {
    if (col_ < lineLen(row_)) {
        pushUndo(EditKind::Delete);
        cur().erase(static_cast<size_t>(col_), 1);
    } else if (row_ + 1 < static_cast<int>(lines_.size())) {
        pushUndo(EditKind::Other);
        cur() += lines_[static_cast<size_t>(row_ + 1)];
        lines_.erase(lines_.begin() + row_ + 1);
        breakGroup();
    }
}

void Editor::tab() {
    pushUndo(EditKind::Insert);
    int spaces = kTab - (col_ % kTab);
    for (int i = 0; i < spaces; ++i) rawInsert(L' ');
}

void Editor::dedent() {
    int n = std::min(kTab, leadingSpaces(cur()));
    if (n == 0) return;
    pushUndo(EditKind::Other);
    cur().erase(0, static_cast<size_t>(n));
    col_ = std::max(0, col_ - n);
    wantCol_ = col_;
    breakGroup();
}

void Editor::duplicateLine() {
    pushUndo(EditKind::Other);
    lines_.insert(lines_.begin() + row_ + 1, cur());
    ++row_;
    breakGroup();
}

void Editor::deleteLine() {
    pushUndo(EditKind::Other);
    if (lines_.size() == 1) {
        lines_[0].clear();
    } else {
        lines_.erase(lines_.begin() + row_);
    }
    clampCursor();
    col_ = std::min(col_, lineLen(row_));
    wantCol_ = col_;
    breakGroup();
}

// ---------------------------------------------------------------------------
void Editor::moveLeft() {
    breakGroup();
    if (col_ > 0) --col_;
    else if (row_ > 0) { --row_; col_ = lineLen(row_); }
    wantCol_ = col_;
}

void Editor::moveRight() {
    breakGroup();
    if (col_ < lineLen(row_)) ++col_;
    else if (row_ + 1 < static_cast<int>(lines_.size())) { ++row_; col_ = 0; }
    wantCol_ = col_;
}

void Editor::moveVert(int delta) {
    breakGroup();
    row_ = std::clamp(row_ + delta, 0, static_cast<int>(lines_.size()) - 1);
    col_ = std::min(wantCol_, lineLen(row_));
}

void Editor::wordLeft() {
    breakGroup();
    if (col_ == 0) { moveLeft(); return; }
    const std::wstring& l = cur();
    while (col_ > 0 && !isWordChar(l[static_cast<size_t>(col_ - 1)])) --col_;
    while (col_ > 0 && isWordChar(l[static_cast<size_t>(col_ - 1)])) --col_;
    wantCol_ = col_;
}

void Editor::wordRight() {
    breakGroup();
    if (col_ >= lineLen(row_)) { moveRight(); return; }
    const std::wstring& l = cur();
    int n = lineLen(row_);
    while (col_ < n && isWordChar(l[static_cast<size_t>(col_)])) ++col_;
    while (col_ < n && !isWordChar(l[static_cast<size_t>(col_)])) ++col_;
    wantCol_ = col_;
}

void Editor::smartHome() {
    breakGroup();
    int first = leadingSpaces(cur());
    col_ = (col_ == first) ? 0 : first;
    wantCol_ = col_;
}

// ---------------------------------------------------------------------------
void Editor::gotoLine(int line1) {
    breakGroup();
    row_ = std::clamp(line1 - 1, 0, static_cast<int>(lines_.size()) - 1);
    col_ = wantCol_ = leadingSpaces(cur());
    follow_ = true;
    center_ = true;
}

void Editor::scrollBy(int delta) {
    scrollRow_ += delta;
    follow_ = false;
}

void Editor::layout(int height, int width) {
    if (height < 1) height = 1;
    if (width < 1) width = 1;
    if (follow_) {
        if (center_) scrollRow_ = std::max(0, row_ - height / 2);
        if (row_ < scrollRow_) scrollRow_ = row_;
        if (row_ >= scrollRow_ + height) scrollRow_ = row_ - height + 1;
        if (col_ < scrollCol_) scrollCol_ = std::max(0, col_ - width / 4);
        if (col_ >= scrollCol_ + width) scrollCol_ = col_ - width + width / 4 + 1;
        follow_ = false;
        center_ = false;
    }
    scrollRow_ = std::clamp(scrollRow_, 0, std::max(0, static_cast<int>(lines_.size()) - 1));
    if (scrollCol_ < 0) scrollCol_ = 0;
}

int Editor::markerAt(int row) const {
    auto it = markers_.find(row);
    return it == markers_.end() ? 0 : it->second;
}

int Editor::nextErrorRow(int fromRow) const {
    int first = -1;
    for (const auto& [r, sev] : markers_) {
        if (sev != MarkError) continue;
        if (first < 0) first = r;
        if (r > fromRow) return r;
    }
    return first;  // по кругу — к первой ошибке
}
