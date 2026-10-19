// ============================================================================
//  OutputPane.cpp
// ============================================================================
#include "OutputPane.h"

#include <algorithm>

#include "Common.h"

namespace {
constexpr size_t kMaxLines = 5000;      // ограничение истории вывода
constexpr size_t kMaxLineLen = 4000;    // очень длинные строки принудительно переносим
}

void OutputPane::pushLine(OutLine l) {
    if (l.shown < l.text.size()) ++pendingTyped_;
    lines_.push_back(std::move(l));
    while (lines_.size() > kMaxLines) {
        if (lines_.front().shown < lines_.front().text.size() && pendingTyped_ > 0) --pendingTyped_;
        lines_.pop_front();
    }
}

void OutputPane::print(const std::wstring& text, OutKind kind, bool typed) {
    open_ = false;
    pendingCR_ = false;
    for (auto& s : util::splitLines(text)) {
        OutLine l;
        l.text = util::expandTabs(s);
        l.kind = kind;
        l.shown = typed ? 0 : l.text.size();
        pushLine(std::move(l));
    }
}

void OutputPane::ensureOpen(OutKind kind) {
    if (open_ && lines_.back().text.size() < kMaxLineLen) return;
    OutLine l;
    l.kind = kind;
    pushLine(std::move(l));
    open_ = true;
}

void OutputPane::appendStream(const std::wstring& chunk, OutKind kind) {
    for (wchar_t c : chunk) {
        // Вырезаем ANSI ESC-последовательности (цвета, курсор) из вывода программы
        if (escState_ == 1) { escState_ = (c == L'[') ? 2 : 0; continue; }
        if (escState_ == 2) { if (c >= 0x40 && c <= 0x7E) escState_ = 0; continue; }
        if (c == 0x1B) { escState_ = 1; continue; }

        if (c == L'\r') { pendingCR_ = true; continue; }
        if (pendingCR_) {
            pendingCR_ = false;
            // Одиночный \r — возврат каретки: строка перерисовывается (прогресс-бары)
            if (c != L'\n' && open_) {
                lines_.back().text.clear();
                lines_.back().shown = 0;
            }
        }
        if (c == L'\n') {
            ensureOpen(kind);
            open_ = false;
            continue;
        }
        if (c == L'\t') {
            ensureOpen(kind);
            auto& t = lines_.back().text;
            t.append(4 - t.size() % 4, L' ');
            lines_.back().shown = t.size();
            continue;
        }
        if (c < 32 || c == 127) continue;
        ensureOpen(kind);
        lines_.back().text += c;
        lines_.back().shown = lines_.back().text.size();
    }
}

void OutputPane::clear() {
    lines_.clear();
    open_ = false;
    pendingCR_ = false;
    pendingTyped_ = 0;
    scroll_ = 0;
}

bool OutputPane::tick(size_t chars) {
    if (pendingTyped_ == 0) return false;
    for (auto& l : lines_) {
        if (l.shown >= l.text.size()) continue;
        size_t add = std::min(chars, l.text.size() - l.shown);
        l.shown += add;
        chars -= add;
        if (l.shown >= l.text.size()) --pendingTyped_;
        if (chars == 0 || pendingTyped_ == 0) break;
    }
    return true;
}

std::vector<VisualRow> OutputPane::visualRows(int width, int height) const {
    std::vector<VisualRow> result;
    if (width < 1 || height < 1) return result;

    // Показываем строки только до первой «недопечатанной» — сохраняем порядок вывода
    size_t limit = lines_.size();
    if (pendingTyped_ > 0) {
        for (size_t i = 0; i < lines_.size(); ++i) {
            if (lines_[i].shown < lines_[i].text.size()) { limit = i + 1; break; }
        }
    }

    // Собираем экранные строки снизу вверх, пока не наберём height + scroll
    std::vector<VisualRow> rev;
    const size_t need = static_cast<size_t>(height + scroll_);
    const size_t w = static_cast<size_t>(width);
    for (size_t i = limit; i-- > 0 && rev.size() < need;) {
        const OutLine& l = lines_[i];
        std::wstring t = l.text.substr(0, l.shown);
        bool typingNow = l.shown < l.text.size();
        std::vector<std::wstring> segs;
        if (t.empty()) segs.push_back(L"");
        for (size_t p = 0; p < t.size(); p += w) segs.push_back(t.substr(p, w));
        for (size_t s = segs.size(); s-- > 0;) {
            VisualRow vr;
            vr.text = segs[s];
            vr.kind = l.kind;
            vr.typingCursor = typingNow && s + 1 == segs.size();
            rev.push_back(std::move(vr));
        }
    }
    // Не даём прокрутить дальше начала истории
    int maxScroll = std::max(0, static_cast<int>(rev.size()) - height);
    if (scroll_ > maxScroll) scroll_ = maxScroll;

    int top = std::min(static_cast<int>(rev.size()), scroll_ + height) - 1;
    for (int k = top; k >= scroll_; --k) result.push_back(rev[static_cast<size_t>(k)]);
    return result;
}
