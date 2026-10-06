#pragma once
// Player.h - игрок: позиция, угол обзора, движение с коллизиями.
#include <cmath>
#include "Map.h"

// Состояние ввода за один шаг симуляции
struct InputState {
    bool forward = false, back = false;
    bool strafeLeft = false, strafeRight = false;
    bool turnLeft = false, turnRight = false;
    double mouseDX = 0.0; // смещение мыши в пикселях
};

class Player {
public:
    double x = 2.0, y = 2.0;  // позиция в клетках карты
    double angle = 0.0;       // угол обзора, радианы (0 = на восток, ось Y направлена вниз)

    void Spawn(const Map& map);
    void Update(const InputState& in, const Map& map, double dt);

    double DirX() const { return std::cos(angle); }
    double DirY() const { return std::sin(angle); }

private:
    static bool Collides(const Map& map, double px, double py);
};
