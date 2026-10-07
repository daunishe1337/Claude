// ============================================================================
//  Font.cpp — данные шрифта 5x7 и вывод текста.
//  Каждый глиф — 7 строк по 5 символов: '#' — пиксель, '.' — пусто.
//  Кириллица, совпадающая по начертанию с латиницей (А, В, Е, К, М, Н, О,
//  Р, С, Т, Х), использует латинские глифы.
// ============================================================================
#include "Font.h"

#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace font {
namespace {

struct GlyphDef {
    uint32_t cp;
    const char* rows[kGlyphH];
};

// clang-format off
const GlyphDef kGlyphs[] = {
    {' ', {".....", ".....", ".....", ".....", ".....", ".....", "....."}},
    // ---- Цифры ----------------------------------------------------------
    {'0', {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."}},
    {'1', {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."}},
    {'2', {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"}},
    {'3', {"#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."}},
    {'4', {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."}},
    {'5', {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."}},
    {'6', {"..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."}},
    {'7', {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."}},
    {'8', {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."}},
    {'9', {".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."}},
    // ---- Латиница ---------------------------------------------------------
    {'A', {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
    {'B', {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."}},
    {'C', {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."}},
    {'D', {"###..", "#..#.", "#...#", "#...#", "#...#", "#..#.", "###.."}},
    {'E', {"#####", "#....", "#....", "####.", "#....", "#....", "#####"}},
    {'F', {"#####", "#....", "#....", "####.", "#....", "#....", "#...."}},
    {'G', {".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####"}},
    {'H', {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
    {'I', {".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."}},
    {'J', {"..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."}},
    {'K', {"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"}},
    {'L', {"#....", "#....", "#....", "#....", "#....", "#....", "#####"}},
    {'M', {"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"}},
    {'N', {"#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#"}},
    {'O', {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
    {'P', {"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."}},
    {'Q', {".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"}},
    {'R', {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"}},
    {'S', {".####", "#....", "#....", ".###.", "....#", "....#", "####."}},
    {'T', {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}},
    {'U', {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
    {'V', {"#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."}},
    {'W', {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#."}},
    {'X', {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"}},
    {'Y', {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."}},
    {'Z', {"#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"}},
    // ---- Знаки --------------------------------------------------------------
    {'.', {".....", ".....", ".....", ".....", ".....", ".....", "..#.."}},
    {',', {".....", ".....", ".....", ".....", ".....", "..#..", ".#..."}},
    {'!', {"..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."}},
    {'?', {".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."}},
    {':', {".....", "..#..", ".....", ".....", ".....", "..#..", "....."}},
    {';', {".....", "..#..", ".....", ".....", ".....", "..#..", ".#..."}},
    {'-', {".....", ".....", ".....", ".###.", ".....", ".....", "....."}},
    {'+', {".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."}},
    {'=', {".....", ".....", "#####", ".....", "#####", ".....", "....."}},
    {'/', {"....#", "....#", "...#.", "..#..", ".#...", "#....", "#...."}},
    {'(', {"...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#."}},
    {')', {".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..."}},
    {'[', {".###.", ".#...", ".#...", ".#...", ".#...", ".#...", ".###."}},
    {']', {".###.", "...#.", "...#.", "...#.", "...#.", "...#.", ".###."}},
    {'<', {"...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#."}},
    {'>', {".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#..."}},
    {'^', {"..#..", ".###.", "#.#.#", "..#..", "..#..", "..#..", "..#.."}},
    {'\'', {"..#..", "..#..", ".....", ".....", ".....", ".....", "....."}},
    {'"', {".#.#.", ".#.#.", ".....", ".....", ".....", ".....", "....."}},
    {'%', {"##..#", "##..#", "...#.", "..#..", ".#...", "#..##", "#..##"}},
    {'_', {".....", ".....", ".....", ".....", ".....", ".....", "#####"}},
    {'*', {".....", "#.#.#", ".###.", "#####", ".###.", "#.#.#", "....."}},
    {'#', {".#.#.", ".#.#.", "#####", ".#.#.", "#####", ".#.#.", ".#.#."}},
    // ---- Кириллица (только уникальные начертания) ---------------------------
    {0x0411, {"#####", "#....", "#....", "####.", "#...#", "#...#", "####."}}, // Б
    {0x0413, {"#####", "#....", "#....", "#....", "#....", "#....", "#...."}}, // Г
    {0x0414, {"..##.", ".#.#.", ".#.#.", ".#.#.", ".#.#.", "#####", "#...#"}}, // Д
    {0x0401, {".#.#.", ".....", "#####", "#....", "####.", "#....", "#####"}}, // Ё
    {0x0416, {"#.#.#", "#.#.#", ".###.", "..#..", ".###.", "#.#.#", "#.#.#"}}, // Ж
    {0x0417, {".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###."}}, // З
    {0x0418, {"#...#", "#...#", "#..##", "#.#.#", "##..#", "#...#", "#...#"}}, // И
    {0x0419, {".#.#.", "..#..", "#...#", "#..##", "#.#.#", "##..#", "#...#"}}, // Й
    {0x041B, {"..###", ".#..#", ".#..#", ".#..#", ".#..#", ".#..#", "#...#"}}, // Л
    {0x041F, {"#####", "#...#", "#...#", "#...#", "#...#", "#...#", "#...#"}}, // П
    {0x0423, {"#...#", "#...#", "#...#", ".####", "....#", "#...#", ".###."}}, // У
    {0x0424, {"..#..", ".###.", "#.#.#", "#.#.#", "#.#.#", ".###.", "..#.."}}, // Ф
    {0x0426, {"#..#.", "#..#.", "#..#.", "#..#.", "#..#.", "#####", "....#"}}, // Ц
    {0x0427, {"#...#", "#...#", "#...#", ".####", "....#", "....#", "....#"}}, // Ч
    {0x0428, {"#.#.#", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#####"}}, // Ш
    {0x0429, {"#.#.#", "#.#.#", "#.#.#", "#.#.#", "#.#.#", "#####", "....#"}}, // Щ
    {0x042A, {"##...", ".#...", ".#...", ".###.", ".#..#", ".#..#", ".###."}}, // Ъ
    {0x042B, {"#...#", "#...#", "#...#", "##..#", "#.#.#", "#.#.#", "##..#"}}, // Ы
    {0x042C, {"#....", "#....", "#....", "####.", "#...#", "#...#", "####."}}, // Ь
    {0x042D, {".###.", "#...#", "....#", "..###", "....#", "#...#", ".###."}}, // Э
    {0x042E, {"#..#.", "#.#.#", "#.#.#", "###.#", "#.#.#", "#.#.#", "#..#."}}, // Ю
    {0x042F, {".####", "#...#", "#...#", ".####", "..#.#", ".#..#", "#...#"}}, // Я
};
// clang-format on

// Глиф в упакованном виде: 7 строк по 5 бит (старший бит — левый пиксель).
using Packed = std::array<uint8_t, kGlyphH>;

// Таблица быстрого поиска по кодовой точке.
class GlyphTable {
public:
    GlyphTable() {
        for (const GlyphDef& g : kGlyphs) {
            Packed p{};
            for (int r = 0; r < kGlyphH; ++r) {
                uint8_t bits = 0;
                for (int c = 0; c < kGlyphW; ++c)
                    if (g.rows[r][c] == '#') bits = static_cast<uint8_t>(bits | (0x10 >> c));
                p[r] = bits;
            }
            store(g.cp, p);
        }
        // Кириллица, совпадающая с латиницей.
        const struct { uint32_t cyr; char lat; } aliases[] = {
            {0x0410, 'A'}, {0x0412, 'B'}, {0x0415, 'E'}, {0x041A, 'K'}, {0x041C, 'M'},
            {0x041D, 'H'}, {0x041E, 'O'}, {0x0420, 'P'}, {0x0421, 'C'}, {0x0422, 'T'},
            {0x0425, 'X'},
        };
        for (const auto& a : aliases) store(a.cyr, *find(static_cast<uint32_t>(a.lat)));
    }

    const Packed* find(uint32_t cp) const {
        if (cp < 128) return ascii_[cp].second ? &ascii_[cp].first : nullptr;
        if (cp >= 0x0400 && cp < 0x0460) {
            const auto& e = cyr_[cp - 0x0400];
            return e.second ? &e.first : nullptr;
        }
        return nullptr;
    }

private:
    void store(uint32_t cp, const Packed& p) {
        if (cp < 128) ascii_[cp] = {p, true};
        else if (cp >= 0x0400 && cp < 0x0460) cyr_[cp - 0x0400] = {p, true};
    }
    std::array<std::pair<Packed, bool>, 128> ascii_{};
    std::array<std::pair<Packed, bool>, 0x60> cyr_{};
};

const GlyphTable& table() {
    static const GlyphTable t; // потокобезопасная инициализация (C++11)
    return t;
}

// Приведение к заглавной и замена «экзотики» на близкие символы.
uint32_t normalize(uint32_t cp) {
    if (cp >= 'a' && cp <= 'z') return cp - 32;
    if (cp >= 0x0430 && cp <= 0x044F) return cp - 0x20; // а..я -> А..Я
    if (cp == 0x0451) return 0x0401;                    // ё -> Ё
    switch (cp) {
    case 0x2014: case 0x2013: case 0x2212: return '-';   // тире, минус
    case 0x00AB: case 0x00BB: case 0x201C: case 0x201D: case 0x201E: return '"';
    case 0x2019: case 0x2018: return '\'';
    case 0x00A0: return ' ';
    default: return cp;
    }
}

const Packed* glyphFor(uint32_t cp) {
    const Packed* g = table().find(normalize(cp));
    if (!g) g = table().find('?');
    return g;
}

void drawGlyph(Canvas& c, int x, int y, const Packed& g, uint32_t color, int scale) {
    for (int r = 0; r < kGlyphH; ++r) {
        const uint8_t bits = g[r];
        if (!bits) continue;
        for (int col = 0; col < kGlyphW; ++col) {
            if (!(bits & (0x10 >> col))) continue;
            if (scale == 1) c.plot(x + col, y + r, color);
            else c.fillRect(x + col * scale, y + r * scale, scale, scale, color);
        }
    }
}

// Ширина одной строки (до '\n' или конца) в символах.
int lineChars(const char* p) {
    int n = 0;
    while (*p && *p != '\n') {
        decodeUtf8(p);
        ++n;
    }
    return n;
}

} // namespace

uint32_t decodeUtf8(const char*& p) {
    const unsigned char c0 = static_cast<unsigned char>(*p);
    if (c0 == 0) return 0;
    ++p;
    if (c0 < 0x80) return c0;
    int extra = 0;
    uint32_t cp = 0;
    if ((c0 & 0xE0) == 0xC0) { extra = 1; cp = c0 & 0x1Fu; }
    else if ((c0 & 0xF0) == 0xE0) { extra = 2; cp = c0 & 0x0Fu; }
    else if ((c0 & 0xF8) == 0xF0) { extra = 3; cp = c0 & 0x07u; }
    else return '?';
    for (int i = 0; i < extra; ++i) {
        const unsigned char cn = static_cast<unsigned char>(*p);
        if ((cn & 0xC0) != 0x80) return '?'; // обрыв последовательности: не съедаем байт
        cp = (cp << 6) | (cn & 0x3Fu);
        ++p;
    }
    return cp;
}

int countChars(const char* utf8) {
    if (!utf8) return 0;
    int n = 0;
    while (*utf8) {
        decodeUtf8(utf8);
        ++n;
    }
    return n;
}

int textWidth(const char* utf8, int scale) {
    if (!utf8) return 0;
    int best = 0;
    const char* p = utf8;
    while (true) {
        const int n = lineChars(p);
        best = std::max(best, n);
        while (*p && *p != '\n') decodeUtf8(p);
        if (*p != '\n') break;
        ++p;
    }
    return best > 0 ? (best * kAdvance - 1) * scale : 0;
}

int lineCount(const char* utf8) {
    if (!utf8 || !*utf8) return 0;
    int n = 1;
    for (const char* p = utf8; *p; ++p)
        if (*p == '\n') ++n;
    return n;
}

void drawText(Canvas& c, int x, int y, const char* utf8, uint32_t color, int scale, int maxChars) {
    if (!utf8) return;
    if (scale < 1) scale = 1;
    const char* p = utf8;
    int cx = x, cy = y, shown = 0;
    while (*p) {
        if (maxChars >= 0 && shown >= maxChars) break;
        const uint32_t cp = decodeUtf8(p);
        ++shown;
        if (cp == '\n') {
            cx = x;
            cy += kLineHeight * scale;
            continue;
        }
        if (cp != ' ') drawGlyph(c, cx, cy, *glyphFor(cp), color, scale);
        cx += kAdvance * scale;
    }
}

void drawTextShadow(Canvas& c, int x, int y, const char* utf8, uint32_t color, uint32_t shadow,
                    int scale, int maxChars) {
    drawText(c, x + scale, y + scale, utf8, shadow, scale, maxChars);
    drawText(c, x, y, utf8, color, scale, maxChars);
}

void drawTextCentered(Canvas& c, int cx, int y, const char* utf8, uint32_t color, int scale,
                      int maxChars) {
    if (!utf8) return;
    if (scale < 1) scale = 1;
    // Рисуем построчно, каждая строка центрируется отдельно.
    const char* p = utf8;
    int shownBefore = 0;
    int ly = y;
    while (true) {
        const char* lineStart = p;
        const int n = lineChars(p);
        const int w = n > 0 ? (n * kAdvance - 1) * scale : 0;
        std::string line;
        const char* q = lineStart;
        while (*q && *q != '\n') {
            const char* before = q;
            decodeUtf8(q);
            line.append(before, q);
        }
        int budget = -1;
        if (maxChars >= 0) {
            budget = maxChars - shownBefore;
            if (budget <= 0) break;
        }
        drawText(c, cx - w / 2, ly, line.c_str(), color, scale, budget);
        shownBefore += n + 1; // +1 за перенос строки
        p = q;
        if (*p != '\n') break;
        ++p;
        ly += kLineHeight * scale;
    }
}

void drawTextCenteredShadow(Canvas& c, int cx, int y, const char* utf8, uint32_t color,
                            uint32_t shadow, int scale, int maxChars) {
    drawTextCentered(c, cx + scale, y + scale, utf8, shadow, scale, maxChars);
    drawTextCentered(c, cx, y, utf8, color, scale, maxChars);
}

int drawTextWrapped(Canvas& c, int x, int y, int maxWidth, const char* utf8, uint32_t color,
                    uint32_t shadow, bool centered, int scale, int maxChars) {
    if (!utf8) return 0;
    if (scale < 1) scale = 1;
    const int maxCols = std::max(1, (maxWidth + scale) / (kAdvance * scale));

    // Разбиваем на слова с учётом явных переносов строк, собираем строки.
    std::vector<std::string> lines;
    std::string current;
    int currentCols = 0;
    const char* p = utf8;
    while (*p) {
        // Слово — до пробела или '\n'.
        std::string word;
        int wordCols = 0;
        while (*p && *p != ' ' && *p != '\n') {
            const char* before = p;
            decodeUtf8(p);
            word.append(before, p);
            ++wordCols;
        }
        if (wordCols > 0) {
            const int need = currentCols == 0 ? wordCols : currentCols + 1 + wordCols;
            if (need > maxCols && currentCols > 0) {
                lines.push_back(current);
                current = word;
                currentCols = wordCols;
            } else {
                if (currentCols > 0) current += ' ';
                current += word;
                currentCols = need;
            }
        }
        if (*p == '\n') {
            lines.push_back(current);
            current.clear();
            currentCols = 0;
            ++p;
        } else if (*p == ' ') {
            ++p;
        }
    }
    if (currentCols > 0 || lines.empty()) lines.push_back(current);

    // Вывод с учётом лимита символов (пробел/перенос между строками = 1 символ).
    int remaining = maxChars;
    int ly = y;
    for (const std::string& line : lines) {
        const int n = countChars(line.c_str());
        int budget = -1;
        if (maxChars >= 0) {
            if (remaining <= 0) break;
            budget = remaining;
            remaining -= n + 1;
        }
        if (centered) drawTextCenteredShadow(c, x, ly, line.c_str(), color, shadow, scale, budget);
        else drawTextShadow(c, x, ly, line.c_str(), color, shadow, scale, budget);
        ly += kLineHeight * scale;
    }
    return static_cast<int>(lines.size()) * kLineHeight * scale;
}

} // namespace font
