#pragma once
// Map.h - карты локаций: массив строк, 1 символ = 1 клетка.
// Локации: 0 - Особняк, 1 - Офис, 2 - Дом, 3 - Квартира.
#include <string>
#include <vector>

class Map {
public:
    Map();

    static int LevelCount();
    void Load(int levelId);

    int Width() const { return width_; }
    int Height() const { return height_; }
    const std::wstring& Name() const { return name_; }

    // Символ клетки. За пределами карты возвращает '1' (стена).
    char Cell(int x, int y) const;

    // Клетка непроходима: любой символ кроме '.'
    bool IsWall(int x, int y) const { return Cell(x, y) != '.'; }

    // Точка входа (там же "дверь" к машине) и направление взгляда
    double StartX() const { return startX_; }
    double StartY() const { return startY_; }
    double StartAngle() const { return startAngle_; }

private:
    std::vector<std::string> rows_;
    std::wstring name_;
    int width_ = 0, height_ = 0;
    double startX_ = 2.5, startY_ = 2.5, startAngle_ = 0.0;
};
