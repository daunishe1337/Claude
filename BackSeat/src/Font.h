// ============================================================================
//  Font.h — встроенный растровый шрифт 5x7 (латиница, кириллица, цифры,
//  знаки препинания). Текст передаётся в UTF-8; строчные буквы рисуются
//  заглавными (стиль старых консолей).
// ============================================================================
#pragma once

#include "Renderer.h"

namespace font {

constexpr int kGlyphW = 5;     // ширина глифа в пикселях
constexpr int kGlyphH = 7;     // высота глифа
constexpr int kAdvance = 6;    // шаг между символами
constexpr int kLineHeight = 9; // шаг между строками

// Декодирует один символ UTF-8 и сдвигает указатель (ошибки -> '?').
uint32_t decodeUtf8(const char*& p);
// Количество символов (кодовых точек) в строке.
int countChars(const char* utf8);
// Ширина самой длинной строки текста в пикселях.
int textWidth(const char* utf8, int scale = 1);
// Количество строк ('\n' — перенос).
int lineCount(const char* utf8);

// Вывод текста. '\n' переносит строку. maxChars >= 0 — показать только
// первые maxChars символов (эффект печатной машинки).
void drawText(Canvas& c, int x, int y, const char* utf8, uint32_t color, int scale = 1,
              int maxChars = -1);
// Текст с тенью (смещение на 1 пиксель * scale вправо-вниз).
void drawTextShadow(Canvas& c, int x, int y, const char* utf8, uint32_t color, uint32_t shadow,
                    int scale = 1, int maxChars = -1);
// Каждая строка центрируется относительно cx.
void drawTextCentered(Canvas& c, int cx, int y, const char* utf8, uint32_t color, int scale = 1,
                      int maxChars = -1);
void drawTextCenteredShadow(Canvas& c, int cx, int y, const char* utf8, uint32_t color,
                            uint32_t shadow, int scale = 1, int maxChars = -1);
// Перенос по словам в пределах maxWidth. centered: x — центр, иначе левый край.
// Возвращает высоту выведенного блока в пикселях.
int drawTextWrapped(Canvas& c, int x, int y, int maxWidth, const char* utf8, uint32_t color,
                    uint32_t shadow, bool centered, int scale = 1, int maxChars = -1);

} // namespace font
