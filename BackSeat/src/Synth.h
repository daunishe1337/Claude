// ============================================================================
//  Synth.h — переносимый процедурный синтезатор и микшер.
//
//  Не потокобезопасен: им владеет звуковой поток (см. Audio.cpp). Все
//  звуки — формулы: осцилляторы (синус, пила, квадрат, треугольник), белый
//  и «коричневый» шум, простые фильтры (однополюсный НЧ/ВЧ, полосовой
//  SVF), огибающие. Внешних файлов нет.
//
//  render() пишет interleaved stereo float в диапазоне [-1, 1]
//  (с мягким ограничением), количество кадров — любое.
// ============================================================================
#pragma once

#include "Audio.h"
#include "Common.h"

#include <memory>

class Synth {
public:
    explicit Synth(int sampleRate = cfg::kSampleRate);
    ~Synth();
    Synth(const Synth&) = delete;
    Synth& operator=(const Synth&) = delete;

    // Запустить эффект. Если все голоса заняты — вытесняется самый тихий/старый.
    void trigger(Sfx s, float volume, float pan, float pitch);
    // Целевые значения непрерывных слоёв (внутри плавно сглаживаются).
    void setAmbient(const AmbientParams& p);
    // Оборвать все короткие эффекты (непрерывные слои продолжают звучать).
    void stopAllSfx();

    // Сгенерировать frames кадров стерео (out[2*i] — левый, out[2*i+1] — правый).
    void render(float* out, int frames);

    int sampleRate() const;

private:
    struct State; // всё состояние синтезатора — в Synth.cpp
    std::unique_ptr<State> st_;
};
