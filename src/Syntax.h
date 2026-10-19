#pragma once
// ============================================================================
//  Syntax.h — простая подсветка синтаксиса C++ (построчный лексер)
// ============================================================================
#include <cstdint>
#include <string>
#include <vector>

enum class Tok : uint8_t { Text, Keyword, Type, String, Number, Comment, Preproc, Punct };

class Highlighter {
public:
    // Размечает строку s. inBlock — «находимся внутри /* ... */» на входе,
    // на выходе — состояние для следующей строки.
    static void line(const std::wstring& s, bool& inBlock, std::vector<Tok>& out);
};
