// Player.cpp
#include "Player.h"

namespace {
const double kPi = 3.14159265358979323846;
const double kMoveSpeed = 3.0;    // клеток в секунду
const double kTurnSpeed = 2.2;    // радиан в секунду (стрелки)
const double kMouseSens = 0.0025; // радиан на пиксель мыши
const double kRadius = 0.25;      // радиус игрока для коллизий
}

void Player::Spawn(const Map& map) {
    x = map.StartX();
    y = map.StartY();
    angle = map.StartAngle();
}

// Проверка: попадает ли "квадрат" игрока в стену
bool Player::Collides(const Map& map, double px, double py) {
    const double xs[2] = { px - kRadius, px + kRadius };
    const double ys[2] = { py - kRadius, py + kRadius };
    for (double cx : xs)
        for (double cy : ys)
            if (map.IsWall(static_cast<int>(std::floor(cx)), static_cast<int>(std::floor(cy))))
                return true;
    return false;
}

void Player::Update(const InputState& in, const Map& map, double dt) {
    // --- Поворот ---
    double turn = (in.turnRight ? 1.0 : 0.0) - (in.turnLeft ? 1.0 : 0.0);
    angle += turn * kTurnSpeed * dt;
    angle += in.mouseDX * kMouseSens;
    if (angle > kPi) angle -= 2.0 * kPi;
    if (angle < -kPi) angle += 2.0 * kPi;

    // --- Движение ---
    double fwd = (in.forward ? 1.0 : 0.0) - (in.back ? 1.0 : 0.0);
    double str = (in.strafeRight ? 1.0 : 0.0) - (in.strafeLeft ? 1.0 : 0.0);
    if (fwd == 0.0 && str == 0.0) return;

    double fx = std::cos(angle), fy = std::sin(angle); // вперёд
    double rx = -fy, ry = fx;                           // вправо
    double mx = fx * fwd + rx * str;
    double my = fy * fwd + ry * str;
    double len = std::sqrt(mx * mx + my * my);
    mx = mx / len * kMoveSpeed * dt;
    my = my / len * kMoveSpeed * dt;

    // Двигаем по осям раздельно - так игрок скользит вдоль стен
    if (!Collides(map, x + mx, y)) x += mx;
    if (!Collides(map, x, y + my)) y += my;
}
