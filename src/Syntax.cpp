// ============================================================================
//  Syntax.cpp
// ============================================================================
#include "Syntax.h"

#include <cwctype>
#include <unordered_set>

namespace {

const std::unordered_set<std::wstring>& keywords() {
    static const std::unordered_set<std::wstring> k = {
        L"alignas", L"alignof", L"and", L"asm", L"break", L"case", L"catch", L"class", L"co_await",
        L"co_return", L"co_yield", L"concept", L"const", L"consteval", L"constexpr", L"constinit",
        L"const_cast", L"continue", L"decltype", L"default", L"delete", L"do", L"dynamic_cast", L"else",
        L"enum", L"explicit", L"export", L"extern", L"false", L"final", L"for", L"friend", L"goto", L"if",
        L"inline", L"mutable", L"namespace", L"new", L"noexcept", L"not", L"nullptr", L"operator", L"or",
        L"override", L"private", L"protected", L"public", L"register", L"reinterpret_cast", L"requires",
        L"return", L"sizeof", L"static", L"static_assert", L"static_cast", L"struct", L"switch",
        L"template", L"this", L"thread_local", L"throw", L"true", L"try", L"typedef", L"typeid",
        L"typename", L"union", L"using", L"virtual", L"volatile", L"while", L"xor"};
    return k;
}

const std::unordered_set<std::wstring>& types() {
    static const std::unordered_set<std::wstring> t = {
        L"auto", L"bool", L"char", L"char8_t", L"char16_t", L"char32_t", L"double", L"float", L"int",
        L"long", L"short", L"signed", L"unsigned", L"void", L"wchar_t", L"size_t", L"ptrdiff_t",
        L"int8_t", L"int16_t", L"int32_t", L"int64_t", L"uint8_t", L"uint16_t", L"uint32_t", L"uint64_t",
        L"std", L"string", L"wstring", L"string_view", L"vector", L"map", L"unordered_map", L"set",
        L"unordered_set", L"list", L"deque", L"array", L"pair", L"tuple", L"optional", L"variant",
        L"unique_ptr", L"shared_ptr", L"weak_ptr", L"function", L"thread", L"mutex", L"queue", L"stack",
        L"priority_queue", L"cout", L"cin", L"cerr", L"endl", L"wcout", L"wcin", L"printf", L"scanf",
        L"main", L"chrono", L"ostream", L"istream", L"stringstream", L"fstream", L"ifstream", L"ofstream"};
    return t;
}

bool isIdentStart(wchar_t c) { return std::iswalpha(c) || c == L'_'; }
bool isIdentChar(wchar_t c) { return std::iswalnum(c) || c == L'_'; }

void fill(std::vector<Tok>& out, size_t from, size_t to, Tok t) {
    for (size_t i = from; i < to && i < out.size(); ++i) out[i] = t;
}

} // namespace

void Highlighter::line(const std::wstring& s, bool& inBlock, std::vector<Tok>& out) {
    const size_t n = s.size();
    out.assign(n, Tok::Text);
    size_t i = 0;

    // Директивы препроцессора: #include, #define ...
    bool pp = false;
    size_t first = s.find_first_not_of(L" \t");
    if (!inBlock && first != std::wstring::npos && s[first] == L'#') {
        pp = true;
        size_t j = first + 1;
        while (j < n && s[j] == L' ') ++j;
        size_t wordStart = j;
        while (j < n && std::iswalpha(s[j])) ++j;
        fill(out, first, j, Tok::Preproc);
        if (s.compare(wordStart, j - wordStart, L"include") == 0) {
            size_t lt = s.find(L'<', j);
            if (lt != std::wstring::npos) {
                size_t gt = s.find(L'>', lt);
                fill(out, lt, gt == std::wstring::npos ? n : gt + 1, Tok::String);
                i = gt == std::wstring::npos ? n : gt + 1;
            }
        }
        if (i < j) i = j;
    }

    while (i < n) {
        if (inBlock) {
            size_t e = s.find(L"*/", i);
            size_t end = e == std::wstring::npos ? n : e + 2;
            fill(out, i, end, Tok::Comment);
            if (e != std::wstring::npos) inBlock = false;
            i = end;
            continue;
        }
        wchar_t c = s[i];
        if (c == L'/' && i + 1 < n && s[i + 1] == L'/') {
            fill(out, i, n, Tok::Comment);
            break;
        }
        if (c == L'/' && i + 1 < n && s[i + 1] == L'*') {
            inBlock = true;
            fill(out, i, i + 2, Tok::Comment);
            i += 2;
            continue;
        }
        if (c == L'"' || c == L'\'') {
            size_t j = i + 1;
            while (j < n) {
                if (s[j] == L'\\') { j += 2; continue; }
                if (s[j] == c) { ++j; break; }
                ++j;
            }
            if (j > n) j = n;
            fill(out, i, j, Tok::String);
            i = j;
            continue;
        }
        if (std::iswdigit(c) || (c == L'.' && i + 1 < n && std::iswdigit(s[i + 1]))) {
            bool hex = (c == L'0' && i + 1 < n && (s[i + 1] == L'x' || s[i + 1] == L'X'));
            size_t j = i;
            while (j < n) {
                wchar_t d = s[j];
                if (std::iswalnum(d) || d == L'.' || d == L'\'') { ++j; continue; }
                if (!hex && (d == L'+' || d == L'-') && j > i && (s[j - 1] == L'e' || s[j - 1] == L'E')) { ++j; continue; }
                break;
            }
            fill(out, i, j, Tok::Number);
            i = j;
            continue;
        }
        if (isIdentStart(c)) {
            size_t j = i;
            while (j < n && isIdentChar(s[j])) ++j;
            std::wstring word = s.substr(i, j - i);
            Tok t = Tok::Text;
            if (keywords().count(word)) t = Tok::Keyword;
            else if (types().count(word)) t = Tok::Type;
            else if (pp) t = Tok::Preproc;
            fill(out, i, j, t);
            i = j;
            continue;
        }
        static const std::wstring punct = L"{}()[]<>;,.:+-*/%=&|!^~?";
        if (punct.find(c) != std::wstring::npos) out[i] = Tok::Punct;
        ++i;
    }
}
