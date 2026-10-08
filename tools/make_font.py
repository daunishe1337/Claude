#!/usr/bin/env python3
"""Генерирует src/font_data.hpp — растровый атлас шрифта DejaVu Sans (обычный и жирный)
с латиницей и кириллицей. Игре не нужны библиотеки для шрифтов: атлас встраивается в exe.

Запуск: python3 tools/make_font.py   (нужен Pillow)
"""
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = os.path.join(ROOT, "assets", "fonts")
OUT = os.path.join(ROOT, "src", "font_data.hpp")
BAKE = 32
ATLAS_W = 1024
PAD = 2
GAP = 3
WHITE = 32  # белый блок для сплошных фигур (переживает уменьшение мип-уровнями)

codepoints = list(range(32, 127)) + list(range(0x400, 0x460))
codepoints += [0xB0, 0xAB, 0xBB, 0xD7, 0x2014, 0x2013, 0x2026, 0x2022, 0x2116, 0x2190, 0x2191, 0x2192,
               0x2193, 0x25B6, 0x25A0, 0x25CF, 0x2713, 0x2717, 0x2605, 0x26A1, 0x2122, 0xA9, 0x2665,
               0x25B2, 0x25BC, 0x25C0, 0x2606, 0xB7]


def bake(font_file):
    font = ImageFont.truetype(os.path.join(FONTS, font_file), BAKE)
    ascent, descent = font.getmetrics()
    glyphs = []
    for cp in codepoints:
        ch = chr(cp)
        l, t, r, b = font.getbbox(ch, anchor="la")
        w, h = max(0, r - l), max(0, b - t)
        img = Image.new("L", (w + 2 * PAD, h + 2 * PAD), 0)
        if w and h:
            ImageDraw.Draw(img).text((PAD - l, PAD - t), ch, font=font, fill=255, anchor="la")
        glyphs.append({"cp": cp, "img": img, "xo": l - PAD, "yo": t - PAD, "adv": font.getlength(ch)})
    return glyphs, ascent, descent


def main():
    reg, asc, desc = bake("DejaVuSans.ttf")
    bold, _, _ = bake("DejaVuSans-Bold.ttf")

    # Упаковка по строкам; в углу (0,0) — белый блок WHITE x WHITE для сплошных фигур
    x, y, row_h = WHITE + GAP, 0, WHITE
    placed = []
    for g in reg + bold:
        w, h = g["img"].size
        if x + w > ATLAS_W:
            x, y, row_h = 0, y + row_h + GAP, 0
        g["x"], g["y"] = x, y
        x += w + GAP
        row_h = max(row_h, h)
        placed.append(g)
    atlas_h = 1
    while atlas_h < y + row_h + GAP:
        atlas_h *= 2
    atlas = Image.new("L", (ATLAS_W, atlas_h), 0)
    atlas.paste(255, (0, 0, WHITE, WHITE))
    for g in placed:
        atlas.paste(g["img"], (g["x"], g["y"]))

    px = atlas.tobytes()
    packed = bytearray()
    for i in range(0, len(px), 2):
        packed.append((px[i] >> 4) | ((px[i + 1] >> 4) << 4))

    def glyph_rows(gs):
        rows = []
        for g in gs:
            w, h = g["img"].size
            rows.append("{%d,%d,%d,%d,%d,%d,%d,%.2ff}" % (g["cp"], g["x"], g["y"], w, h, g["xo"], g["yo"], g["adv"]))
        return ",\n".join(rows)

    with open(OUT, "w", encoding="utf-8") as f:
        f.write("// Сгенерировано tools/make_font.py из DejaVu Sans (лицензия: assets/fonts/LICENSE-DejaVu.txt).\n")
        f.write("// Не редактировать вручную.\n#pragma once\n\nnamespace fontdata {\n")
        f.write("constexpr int kBake = %d;\nconstexpr int kAscent = %d;\nconstexpr int kDescent = %d;\n" % (BAKE, asc, desc))
        f.write("constexpr int kWhite = %d;\n" % WHITE)
        f.write("constexpr int kAtlasW = %d;\nconstexpr int kAtlasH = %d;\n" % (ATLAS_W, atlas_h))
        f.write("struct Glyph { int cp; short x, y, w, h, xo, yo; float adv; };\n")
        f.write("constexpr int kGlyphCount = %d;\n" % len(codepoints))
        f.write("static const Glyph kRegular[] = {\n%s};\n" % glyph_rows(reg))
        f.write("static const Glyph kBold[] = {\n%s};\n" % glyph_rows(bold))
        f.write("// 4 бита на пиксель, два пикселя в байте (младшие биты — левый)\n")
        f.write("static const unsigned char kAtlas4[] = {\n")
        for i in range(0, len(packed), 40):
            f.write(",".join(str(v) for v in packed[i:i + 40]) + ",\n")
        f.write("};\n}  // namespace fontdata\n")
    print("atlas %dx%d, %d bytes packed -> %s" % (ATLAS_W, atlas_h, len(packed), OUT))


if __name__ == "__main__":
    main()
