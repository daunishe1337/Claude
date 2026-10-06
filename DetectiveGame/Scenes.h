#pragma once
// Scenes.h - процедурно нарисованные картинки для катсцены и машины (всё в пиксельный буфер).
#include "Renderer.h"

namespace Scenes {
    void DrawDayStreet(Renderer& r, double t);    // день, герой идёт по улице
    void DrawDusk(Renderer& r, double t);         // вечер, герой идёт домой
    void DrawBurningHouse(Renderer& r, double t); // ночь, дом горит
    // Вид из машины: лобовое стекло и приборная панель.
    // glow - зарево пожара в зеркале (0..1), speed - скорость езды (0 = стоим).
    void DrawCarView(Renderer& r, double t, double glow, double speed);
}
