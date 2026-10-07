// ============================================================================
//  Hud.h — общие элементы интерфейса (шкалы, значки, подсказки, субтитры).
//  Используются и в реальном мире (RealWorldScene), и в режиме консоли и на
//  экранах меню (Game), чтобы всё выглядело единообразно.
// ============================================================================
#pragma once

#include "Common.h"
#include "Renderer.h"

namespace hud {

// Палитра интерфейса: приглушённые «ночные» цвета.
constexpr uint32_t kText = rgb(222, 214, 196);     // основной текст (тёплый белый)
constexpr uint32_t kTextDim = rgb(130, 126, 120);  // второстепенный текст
constexpr uint32_t kShadow = rgb(8, 6, 12);        // тень текста
constexpr uint32_t kPanel = rgb(14, 12, 20);       // подложка
constexpr uint32_t kFuel = rgb(230, 170, 60);      // бензин (янтарный)
constexpr uint32_t kFuelLow = rgb(220, 60, 40);    // бензин на исходе
constexpr uint32_t kThreat = rgb(190, 30, 40);     // угроза (тёмно-красный)
constexpr uint32_t kTrip = rgb(120, 170, 220);     // путь до дома (холодный синий)
constexpr uint32_t kCamera = rgb(240, 240, 255);   // камера/вспышка
constexpr uint32_t kLock = rgb(140, 210, 150);     // замок

// Шкала бензина с подписью и значком канистры. blink — мигание при нехватке.
// Занимает примерно 70x14 пикселей начиная с (x, y).
void drawFuelGauge(Canvas& c, int x, int y, float fuel01, bool blink, float time);

// Шкала пути до дома с подписью «ДОМ x.x КМ». Ширина w, высота ~14.
void drawTripBar(Canvas& c, int x, int y, int w, float progress01, float kmLeft);

// Шкала угрозы (вертикальная или горизонтальная по ситуации).
// Пульсирует при высоких значениях. ~70x14.
void drawThreatBar(Canvas& c, int x, int y, float threat01, float time);

// Значок камеры с кольцом зарядки (charge01), пометкой усиленной вспышки.
// Если камеры нет — серый силуэт и подпись «НЕТ КАМЕРЫ».
void drawCameraStatus(Canvas& c, int x, int y, bool hasCamera, float charge01, bool superFlash,
                      float time);

// Значок замка с таймером (если активен).
void drawLockStatus(Canvas& c, int x, int y, float secondsLeft);

// Подсказка клавиши: рамка с текстом клавиши и подпись справа, напр. [TAB] КОНСОЛЬ.
// Возвращает ширину в пикселях.
int drawKeyHint(Canvas& c, int x, int y, const char* key, const char* label, float alpha = 1.0f);

// Субтитр реплики внизу/вверху экрана: «ИМЯ: текст» c печатной машинкой.
// y — верх блока, текст переносится по ширине maxWidth и центрируется по cx.
void drawSubtitle(Canvas& c, int cx, int y, int maxWidth, const char* speaker, const char* text,
                  int visibleChars, float alpha, uint32_t speakerColor);

// Затемнённая плашка с рамкой (для меню/паузы/экранов итога).
void drawPanel(Canvas& c, int x, int y, int w, int h, float alpha = 0.85f);

} // namespace hud
