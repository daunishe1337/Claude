#pragma once
// Палитры светлой и тёмной темы (токены Fluent / WinUI 3) и акцентный цвет.

#include "Common.h"

struct AccentPalette
{
    Color base;
    Color light1, light2, light3;
    Color dark1, dark2, dark3;
};

struct Palette
{
    bool dark = false;

    // Текст
    Color text, textSecondary, textTertiary, textDisabled;
    Color textOnAccent, textOnAccentSecondary;
    Color accentText, accentTextSecondary;

    // Акцентные заливки (кнопки, переключатели, индикаторы)
    Color accent, accentHover, accentPressed, accentDisabled;

    // Сплошные фоны
    Color solidBase, solidSecondary, solidTertiary, solidQuarternary;
    Color layer, layerAlt;

    // Карточки и элементы управления
    Color card, cardSecondary, cardStroke;
    Color control, controlHover, controlPressed, controlDisabled, controlInputActive;
    Color controlStroke, controlStrokeSecondary, controlStrongStroke, controlStrongFill;
    Color controlAltHover; // заливка «вторичных» кнопок калькулятора и т.п.
    Color controlSolid;    // непрозрачная заливка (бегунок ползунка)
    Color controlAltFill;  // дорожка выключенного переключателя
    Color subtleHover, subtlePressed;
    Color divider, surfaceStroke, flyoutStroke;
    Color smoke;

    // Материалы
    Color micaTint;
    float micaChroma = 0.0f;   // сколько «цвета обоев» пропускает Mica
    Color acrylicTint;         // Пуск и всплывающие панели
    float acrylicAmount = 0.0f;
    Color taskbarTint;
    float taskbarAmount = 0.0f;
    Color menuTint;            // контекстные меню
    float menuAmount = 0.0f;
    Color tooltipBg;

    // Прочее
    Color critical, criticalText, success, caution;
    Color selection, selectionHover, selectionStroke; // выделение в списках
    Color contentBg;          // фон содержимого Проводника
    Color editorBg;           // область текста Блокнота
    Color closeHover, closePressed;
    Color scrollThumb;
    Color shadow;
};

namespace Theme
{
struct State
{
    bool dark = false;
    bool transparency = true;
    bool accentTitleBars = false;
    Color accentBase = Color::Hex(0x0078D4);
    AccentPalette accent;
    Palette palette;
    unsigned version = 0; // меняется при каждом изменении темы
};

inline State& S()
{
    static State s;
    return s;
}

// ---- HSL ------------------------------------------------------------------
inline void RgbToHsl(Color c, float& h, float& s, float& l)
{
    const float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f;
    const float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
    l = (mx + mn) * 0.5f;
    if (mx - mn < 1e-5f)
    {
        h = 0.0f;
        s = 0.0f;
        return;
    }
    const float d = mx - mn;
    s = l > 0.5f ? d / (2.0f - mx - mn) : d / (mx + mn);
    if (mx == r)
        h = (g - b) / d + (g < b ? 6.0f : 0.0f);
    else if (mx == g)
        h = (b - r) / d + 2.0f;
    else
        h = (r - g) / d + 4.0f;
    h /= 6.0f;
}

inline float HueToRgb(float p, float q, float t)
{
    if (t < 0.0f)
        t += 1.0f;
    if (t > 1.0f)
        t -= 1.0f;
    if (t < 1.0f / 6.0f)
        return p + (q - p) * 6.0f * t;
    if (t < 0.5f)
        return q;
    if (t < 2.0f / 3.0f)
        return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

inline Color HslToRgb(float h, float s, float l, uint8_t a = 255)
{
    float r = l, g = l, b = l;
    if (s > 1e-5f)
    {
        const float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        const float p = 2.0f * l - q;
        r = HueToRgb(p, q, h + 1.0f / 3.0f);
        g = HueToRgb(p, q, h);
        b = HueToRgb(p, q, h - 1.0f / 3.0f);
    }
    return Color(static_cast<uint8_t>(Saturate(r) * 255.0f + 0.5f), static_cast<uint8_t>(Saturate(g) * 255.0f + 0.5f),
                 static_cast<uint8_t>(Saturate(b) * 255.0f + 0.5f), a);
}

// Оттенки акцента. Для стандартного синего — точные значения палитры Windows 11.
inline AccentPalette MakeAccentPalette(Color base)
{
    AccentPalette p;
    p.base = base;
    if (base == Color::Hex(0x0078D4))
    {
        p.light3 = Color::Hex(0x99EBFF);
        p.light2 = Color::Hex(0x4CC2FF);
        p.light1 = Color::Hex(0x0091F8);
        p.dark1 = Color::Hex(0x0067C0);
        p.dark2 = Color::Hex(0x003E92);
        p.dark3 = Color::Hex(0x001A68);
        return p;
    }
    float h = 0.0f, s = 0.0f, l = 0.0f;
    RgbToHsl(base, h, s, l);
    p.light1 = HslToRgb(h, s, l + (1.0f - l) * 0.12f);
    p.light2 = HslToRgb(h, s, l + (1.0f - l) * 0.40f);
    p.light3 = HslToRgb(h, s, l + (1.0f - l) * 0.66f);
    p.dark1 = HslToRgb(h, s, l * 0.90f);
    p.dark2 = HslToRgb(h, s, l * 0.69f);
    p.dark3 = HslToRgb(h, s, l * 0.49f);
    return p;
}

inline void Rebuild()
{
    State& st = S();
    st.accent = MakeAccentPalette(st.accentBase);
    const AccentPalette& a = st.accent;
    Palette& p = st.palette;
    p = Palette{};
    p.dark = st.dark;
    p.critical = Color::Hex(0xC42B1C);
    p.closeHover = Color::Hex(0xC42B1C);
    p.closePressed = Color::Hex(0xC42B1C, 0xE6);
    p.shadow = Color(0, 0, 0, 255);

    if (!st.dark)
    {
        p.text = Color(0, 0, 0, 0xE4);
        p.textSecondary = Color(0, 0, 0, 0x9E);
        p.textTertiary = Color(0, 0, 0, 0x72);
        p.textDisabled = Color(0, 0, 0, 0x5C);
        p.textOnAccent = Color(255, 255, 255);
        p.textOnAccentSecondary = Color(255, 255, 255, 0xB3);
        p.accentText = a.dark2;
        p.accentTextSecondary = a.dark3;

        p.accent = a.dark1;
        p.accentHover = a.dark1.WithAlpha(0xE6);
        p.accentPressed = a.dark1.WithAlpha(0xCC);
        p.accentDisabled = Color(0, 0, 0, 0x37);

        p.solidBase = Color::Hex(0xF3F3F3);
        p.solidSecondary = Color::Hex(0xEEEEEE);
        p.solidTertiary = Color::Hex(0xF9F9F9);
        p.solidQuarternary = Color::Hex(0xFFFFFF);
        p.layer = Color(255, 255, 255, 0x80);
        p.layerAlt = Color(255, 255, 255, 0xFF);

        p.card = Color(255, 255, 255, 0xB3);
        p.cardSecondary = Color(246, 246, 246, 0x80);
        p.cardStroke = Color(0, 0, 0, 0x0F);
        p.control = Color(255, 255, 255, 0xB3);
        p.controlHover = Color(249, 249, 249, 0x80);
        p.controlPressed = Color(249, 249, 249, 0x4D);
        p.controlDisabled = Color(249, 249, 249, 0x4D);
        p.controlInputActive = Color(255, 255, 255, 255);
        p.controlStroke = Color(0, 0, 0, 0x0F);
        p.controlStrokeSecondary = Color(0, 0, 0, 0x29);
        p.controlStrongStroke = Color(0, 0, 0, 0x72);
        p.controlStrongFill = Color(0, 0, 0, 0x72);
        p.controlAltHover = Color(0, 0, 0, 0x06);
        p.controlSolid = Color::Hex(0xFFFFFF);
        p.controlAltFill = Color(0, 0, 0, 0x06);
        p.subtleHover = Color(0, 0, 0, 0x09);
        p.subtlePressed = Color(0, 0, 0, 0x06);
        p.divider = Color(0, 0, 0, 0x14);
        p.surfaceStroke = Color(117, 117, 117, 0x66);
        p.flyoutStroke = Color(0, 0, 0, 0x17);
        p.smoke = Color(0, 0, 0, 0x4D);

        p.micaTint = Color::Hex(0xF3F3F3);
        p.micaChroma = 0.16f;
        p.acrylicTint = Color::Hex(0xF0F0F0);
        p.acrylicAmount = 0.84f;
        p.taskbarTint = Color::Hex(0xF0F0F0);
        p.taskbarAmount = 0.80f;
        p.menuTint = Color::Hex(0xF9F9F9);
        p.menuAmount = 0.86f;
        p.tooltipBg = Color::Hex(0xF9F9F9);

        p.criticalText = Color::Hex(0xC42B1C);
        p.success = Color::Hex(0x0F7B0F);
        p.caution = Color::Hex(0x9D5D00);
        p.selection = Color::Hex(0xCCE8FF);
        p.selectionHover = Color::Hex(0xE5F3FF);
        p.selectionStroke = Color::Hex(0x99D1FF);
        p.contentBg = Color::Hex(0xFFFFFF);
        p.editorBg = Color::Hex(0xFBFBFB);
        p.scrollThumb = Color(0, 0, 0, 0x72);
    }
    else
    {
        p.text = Color(255, 255, 255);
        p.textSecondary = Color(255, 255, 255, 0xC5);
        p.textTertiary = Color(255, 255, 255, 0x87);
        p.textDisabled = Color(255, 255, 255, 0x5D);
        p.textOnAccent = Color(0, 0, 0);
        p.textOnAccentSecondary = Color(0, 0, 0, 0x80);
        p.accentText = a.light3;
        p.accentTextSecondary = a.light3.WithAlpha(0xCC);

        p.accent = a.light2;
        p.accentHover = a.light2.WithAlpha(0xE6);
        p.accentPressed = a.light2.WithAlpha(0xCC);
        p.accentDisabled = Color(255, 255, 255, 0x28);

        p.solidBase = Color::Hex(0x202020);
        p.solidSecondary = Color::Hex(0x1C1C1C);
        p.solidTertiary = Color::Hex(0x282828);
        p.solidQuarternary = Color::Hex(0x2C2C2C);
        p.layer = Color(58, 58, 58, 0x4C);
        p.layerAlt = Color(255, 255, 255, 0x0D);

        p.card = Color(255, 255, 255, 0x0D);
        p.cardSecondary = Color(255, 255, 255, 0x08);
        p.cardStroke = Color(0, 0, 0, 0x19);
        p.control = Color(255, 255, 255, 0x0F);
        p.controlHover = Color(255, 255, 255, 0x15);
        p.controlPressed = Color(255, 255, 255, 0x08);
        p.controlDisabled = Color(255, 255, 255, 0x0B);
        p.controlInputActive = Color(30, 30, 30, 0xB3);
        p.controlStroke = Color(255, 255, 255, 0x12);
        p.controlStrokeSecondary = Color(255, 255, 255, 0x18);
        p.controlStrongStroke = Color(255, 255, 255, 0x8B);
        p.controlStrongFill = Color(255, 255, 255, 0x8B);
        p.controlAltHover = Color(255, 255, 255, 0x06);
        p.controlSolid = Color::Hex(0x454545);
        p.controlAltFill = Color(0, 0, 0, 0x19);
        p.subtleHover = Color(255, 255, 255, 0x0F);
        p.subtlePressed = Color(255, 255, 255, 0x0A);
        p.divider = Color(255, 255, 255, 0x15);
        p.surfaceStroke = Color(117, 117, 117, 0x66);
        p.flyoutStroke = Color(0, 0, 0, 0x33);
        p.smoke = Color(0, 0, 0, 0x4D);

        p.micaTint = Color::Hex(0x202020);
        p.micaChroma = 0.10f;
        p.acrylicTint = Color::Hex(0x222222);
        p.acrylicAmount = 0.84f;
        p.taskbarTint = Color::Hex(0x1C1C1C);
        p.taskbarAmount = 0.84f;
        p.menuTint = Color::Hex(0x2C2C2C);
        p.menuAmount = 0.90f;
        p.tooltipBg = Color::Hex(0x2C2C2C);

        p.criticalText = Color::Hex(0xFF99A4);
        p.success = Color::Hex(0x6CCB5F);
        p.caution = Color::Hex(0xFCE100);
        p.selection = Color(255, 255, 255, 0x2E);
        p.selectionHover = Color(255, 255, 255, 0x14);
        p.selectionStroke = Color(255, 255, 255, 0x40);
        p.contentBg = Color::Hex(0x191919);
        p.editorBg = Color::Hex(0x272727);
        p.scrollThumb = Color(255, 255, 255, 0x8B);
    }
    ++st.version;
}

inline const Palette& P()
{
    State& st = S();
    if (st.version == 0)
        Rebuild();
    return st.palette;
}
inline const AccentPalette& A()
{
    P();
    return S().accent;
}
inline bool Dark() { return S().dark; }
inline bool Transparency() { return S().transparency; }
inline bool AccentTitleBars() { return S().accentTitleBars; }
inline unsigned Version()
{
    P();
    return S().version;
}
inline void SetDark(bool d)
{
    S().dark = d;
    Rebuild();
}
inline void SetAccent(Color c)
{
    S().accentBase = c;
    Rebuild();
}
inline void SetTransparency(bool t)
{
    S().transparency = t;
    Rebuild();
}
inline void SetAccentTitleBars(bool t)
{
    S().accentTitleBars = t;
    Rebuild();
}

// Набор акцентных цветов из раздела «Персонализация > Цвета» (8 x 6).
inline const std::vector<Color>& AccentSwatches()
{
    static const std::vector<Color> sw = {
        Color::Hex(0xFFB900), Color::Hex(0xFF8C00), Color::Hex(0xF7630C), Color::Hex(0xCA5010), Color::Hex(0xDA3B01),
        Color::Hex(0xEF6950), Color::Hex(0xD13438), Color::Hex(0xFF4343), Color::Hex(0xE74856), Color::Hex(0xE81123),
        Color::Hex(0xEA005E), Color::Hex(0xC30052), Color::Hex(0xE3008C), Color::Hex(0xBF0077), Color::Hex(0xC239B3),
        Color::Hex(0x9A0089), Color::Hex(0x0078D4), Color::Hex(0x0063B1), Color::Hex(0x8E8CD8), Color::Hex(0x6B69D6),
        Color::Hex(0x8764B8), Color::Hex(0x744DA9), Color::Hex(0xB146C2), Color::Hex(0x881798), Color::Hex(0x0099BC),
        Color::Hex(0x2D7D9A), Color::Hex(0x00B7C3), Color::Hex(0x038387), Color::Hex(0x00B294), Color::Hex(0x018574),
        Color::Hex(0x00CC6A), Color::Hex(0x10893E), Color::Hex(0x7A7574), Color::Hex(0x5D5A58), Color::Hex(0x68768A),
        Color::Hex(0x515C6B), Color::Hex(0x567C73), Color::Hex(0x486860), Color::Hex(0x498205), Color::Hex(0x107C10),
        Color::Hex(0x767676), Color::Hex(0x4C4A48), Color::Hex(0x69797E), Color::Hex(0x4A5459), Color::Hex(0x647C64),
        Color::Hex(0x525E54), Color::Hex(0x847545), Color::Hex(0x7E735F),
    };
    return sw;
}
} // namespace Theme
