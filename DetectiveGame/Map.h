#pragma once
// Map.h - карта особняка: массив строк, 1 символ = 1 клетка.
#include <string>
#include <vector>

class Map {
public:
    Map();

    int Width() const { return width_; }
    int Height() const { return height_; }

    // Символ клетки. За пределами карты возвращает '1' (стена).
    char Cell(int x, int y) const;

    // Клетка непроходима: любой символ кроме '.'
    bool IsWall(int x, int y) const { return Cell(x, y) != '.'; }

    // Стартовая позиция и направление игрока
    double StartX() const { return 12.5; }
    double StartY() const { return 16.5; }
    double StartAngle() const { return -1.57079632679; } // смотрим "вверх" (на север)

private:
    std::vector<std::string> rows_;
    int width_ = 0;
    int height_ = 0;
};
