#pragma once
// Textures.h - процедурные текстуры 64x64 (стены, пол, потолок). Файлов с картинками нет:
// всё генерируется кодом при первом обращении.
#include <cstdint>
#include <vector>

struct Texture {
    static constexpr int kSize = 64;
    std::vector<uint32_t> px;

    // u, v любые целые: координаты "заворачиваются" по размеру текстуры
    uint32_t At(int u, int v) const { return px[(v & (kSize - 1)) * kSize + (u & (kSize - 1))]; }
};

namespace Textures {
    const Texture& Wall(char cell);          // '1'..'7' - тип стены из карты
    const Texture& Floor(int levelId);       // пол для локации
    const Texture& Ceiling(int levelId);     // потолок для локации
}
