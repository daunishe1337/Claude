// ============================================================================
//  Synth.cpp — процедурный синтезатор и микшер (см. Synth.h).
//
//  Устройство:
//    * пул голосов для коротких эффектов. У голоса есть локальное время
//      (pitch ускоряет и время, и все частоты — как скорость плёнки), набор
//      осцилляторов/фильтров и параметры, случайно выбранные при запуске:
//      два шага по крыше никогда не звучат одинаково;
//    * «шина консоли»: эффекты Console* и мелодия консоли проходят через
//      общий фильтр приглушения (консоль на коленях — глуше и тише);
//    * непрерывные фоновые слои (двигатель, дорога, ветер, дождь, сердце,
//      тревожный дрон). Их параметры плавно сглаживаются — щелчков нет;
//    * мастер: сумма -> подавление постоянной составляющей -> мягкий
//      ограничитель -> общая громкость.
//  Обработка идёт блоками по kBlock кадров; render() не выделяет память.
// ============================================================================
#include "Synth.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace {

// ---------------------------------------------------------------------------
//  Константы микшера
// ---------------------------------------------------------------------------
constexpr int kMaxVoices = 32;             // одновременно звучащих эффектов
constexpr int kPoolSize = kMaxVoices + 4;  // + слоты под затухающие вытесненные голоса
constexpr int kBlock = 64;                 // внутренний блок обработки, кадров
constexpr float kStealFade = 0.006f;       // затухание вытесненного/остановленного голоса, с
constexpr float kEndFade = 0.02f;          // страховочное затухание в самом конце голоса, с
constexpr float kParamTau = 0.06f;         // сглаживание фоновых параметров, с
constexpr float kPanWidth = 0.8f;          // даже крайняя панорама слышна и вторым ухом
constexpr float kSqrt2 = 1.41421356f;

// ---------------------------------------------------------------------------
//  Мелкие помощники
// ---------------------------------------------------------------------------

// NaN/бесконечность -> def, иначе ограничение диапазоном.
inline float sanitize(float v, float lo, float hi, float def) {
    if (!std::isfinite(v)) return def;
    return clampf(v, lo, hi);
}

inline float fi(int i) { return static_cast<float>(i); }

// Быстрый синус по фазе (период 1): свёртка к четверти периода и ряд
// Тейлора до x^9 (ошибка < 4e-6 — на слух неотличимо от std::sin).
inline float sinPh(float ph) {
    if (ph < 0.0f || ph >= 1.0f) ph -= std::floor(ph);
    float p = ph - 0.5f; // sin(2pi*ph) = -sin(2pi*p), p в [-0.5, 0.5)
    if (p > 0.25f) p = 0.5f - p;
    else if (p < -0.25f) p = -0.5f - p;
    const float x = kTau * p;
    const float x2 = x * x;
    const float sn = x * (1.0f + x2 * (-1.0f / 6.0f + x2 * (1.0f / 120.0f + x2 * (-1.0f / 5040.0f +
                                                                              x2 * (1.0f / 362880.0f)))));
    return -sn;
}

// Приращение фазы за кадр для частоты hz (строго ниже частоты Найквиста).
inline float phaseInc(float hz, float dtl) { return clampf(hz * dtl, 0.0f, 0.49f); }

// Сдвиг фазы с заворачиванием в [0, 1) (inc < 1 гарантирует phaseInc).
inline void advance(float& ph, float inc) {
    ph += inc;
    if (ph >= 1.0f) ph -= 1.0f;
}

// Частота MIDI-ноты (69 = Ля первой октавы, 440 Гц).
inline float midiHz(int note) { return 440.0f * std::pow(2.0f, fi(note - 69) / 12.0f); }

// PolyBLEP-поправка: сглаживает разрыв пилы/меандра и убирает «грязь» алиасинга.
inline float polyBlep(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// Пила -1..1.
inline float sawWave(float ph, float inc) { return 2.0f * ph - 1.0f - polyBlep(ph, inc); }

// Прямоугольник со скважностью duty, без постоянной составляющей.
inline float pulseWave(float ph, float inc, float duty) {
    float v = ph < duty ? 1.0f : -1.0f;
    v += polyBlep(ph, inc);
    float p2 = ph - duty;
    if (p2 < 0.0f) p2 += 1.0f;
    v -= polyBlep(p2, inc);
    return v - (2.0f * duty - 1.0f);
}

// Треугольник -1..1.
inline float triWave(float ph) { return 4.0f * std::fabs(ph - 0.5f) - 1.0f; }

// Рациональное приближение tanh (точно равно ±1 при |x| >= 3).
inline float softSat(float x) {
    if (x >= 3.0f) return 1.0f;
    if (x <= -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Мягкий ограничитель мастера: линеен до 0.6, выше плавно стремится к 1.
inline float softClip(float x) {
    constexpr float kKnee = 0.6f;
    const float a = std::fabs(x);
    if (a <= kKnee) return x;
    const float y = kKnee + (1.0f - kKnee) * softSat((a - kKnee) / (1.0f - kKnee));
    return x < 0.0f ? -y : y;
}

// Быстрый tan (Паде [3/2]) для коэффициентов фильтра, x в [0, 1.45].
inline float fastTan(float x) {
    const float x2 = x * x;
    return x * (15.0f - x2) / (15.0f - 6.0f * x2);
}

// Атака (линейная, att с) + экспоненциальный спад с постоянной tau.
inline float envAD(float t, float att, float tau) {
    if (t <= 0.0f) return 0.0f;
    if (t < att) return t / att;
    return std::exp((att - t) / tau);
}

// Плавный подъём за att и плавный спад за rel к концу отрезка длиной len.
inline float envFade(float t, float att, float rel, float len) {
    return smoothstep(0.0f, att, t) * (1.0f - smoothstep(len - rel, len, t));
}

// Обнуление «денормализованных» хвостов фильтров (они очень медленные на x86).
inline void flushTiny(float& v) {
    if (std::fabs(v) < 1e-15f) v = 0.0f;
}

// ---------------------------------------------------------------------------
//  Фильтры и генераторы
// ---------------------------------------------------------------------------

// Однополюсный фильтр: НЧ (lp) или ВЧ (hp). fn — нормированная частота fc/fs.
struct OnePole {
    float z = 0.0f;
    float a = 1.0f;
    void tune(float fn) { a = 1.0f - std::exp(-kTau * clampf(fn, 1e-6f, 0.45f)); }
    float lp(float x) {
        z += a * (x - z);
        return z;
    }
    float hp(float x) {
        z += a * (x - z);
        return x - z;
    }
    void flush() { flushTiny(z); }
};

// Фильтр переменных состояний (топология TPT, Zavalishin/Simper): устойчив
// при быстрой перестройке частоты прямо во время звучания.
struct Svf {
    float ic1 = 0.0f, ic2 = 0.0f;  // состояния интеграторов
    float k = 1.0f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;
    float low = 0.0f, band = 0.0f; // выходы последнего шага

    // fn — нормированная частота (fc / fs), q — добротность.
    void tune(float fn, float q) {
        const float g = fastTan(kPi * clampf(fn, 1e-5f, 0.46f));
        k = 1.0f / std::max(q, 0.3f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void run(float x) {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        low = v2;
        band = v1;
    }
    float lp(float x) {
        run(x);
        return low;
    }
    // Полоса с единичным усилением на центральной частоте.
    float bp(float x) {
        run(x);
        return band * k;
    }
    void flush() {
        flushTiny(ic1);
        flushTiny(ic2);
    }
    void clear() { ic1 = ic2 = low = band = 0.0f; }
};

// Затухающий синус — «модальный резонатор» (двухполюсная рекурсия, без
// вычисления синуса на каждом кадре): звон стекла, корпус машины, капли.
struct Ping {
    float y1 = 0.0f, y2 = 0.0f; // два предыдущих отсчёта
    float c = 0.0f, r2 = 0.0f;  // 2*r*cos(w) и r^2
    float r = 0.0f;             // затухание за кадр
    float amp = 0.0f;           // текущая амплитуда (для отсечки и выбора тихого)
    void start(float hz, float a, float tau, float dtl) {
        const float w = kTau * phaseInc(hz, dtl);
        r = std::exp(-dtl / std::max(tau, 1e-4f));
        c = 2.0f * r * std::cos(w);
        r2 = r * r;
        // Начальные условия для y[n] = a * r^n * sin(w*n): первый отсчёт — ноль.
        y1 = -a * std::sin(w) / r;
        y2 = -a * std::sin(2.0f * w) / r2;
        amp = a;
    }
    float tick() {
        if (amp < 1e-5f) {
            amp = 0.0f;
            return 0.0f;
        }
        const float y = c * y1 - r2 * y2;
        y2 = y1;
        y1 = y;
        amp *= r;
        return y;
    }
};

// Самый тихий резонатор пула — его не жалко перезапустить.
template <size_t N>
Ping& quietestPing(Ping (&pool)[N]) {
    size_t best = 0;
    for (size_t i = 1; i < N; ++i)
        if (pool[i].amp < pool[best].amp) best = i;
    return pool[best];
}

// Плавное случайное блуждание в [-1, 1]: новая цель через 1/rate секунд,
// между целями — интерполяция smoothstep. Дрожание частот, порывы ветра.
struct Wander {
    float from = 0.0f, to = 0.0f, x = 1.0f;
    // step = rate * dt (доля пути до следующей цели за вызов).
    float next(Rng& rng, float step) {
        x += step;
        if (x >= 1.0f) {
            x -= std::floor(x);
            from = to;
            to = rng.signedUnit();
        }
        const float u = x * x * (3.0f - 2.0f * x);
        return from + (to - from) * u;
    }
};

// Шум старых карманных консолей: 15-битный регистр сдвига с обратной связью.
// В «коротком» режиме (период 127) шум звенит металлически.
struct Lfsr {
    uint32_t reg = 0x7FFFu;
    float ph = 0.0f;
    float out = 1.0f;
    // inc — тактов регистра за кадр (не больше 1).
    float tick(float inc, bool shortMode) {
        ph += clampf(inc, 0.0f, 1.0f);
        if (ph >= 1.0f) {
            ph -= 1.0f;
            const uint32_t bit = (reg ^ (reg >> 1)) & 1u;
            reg = (reg >> 1) | (bit << 14);
            if (shortMode) reg = (reg & ~(1u << 6)) | (bit << 6);
            out = (reg & 1u) ? -1.0f : 1.0f;
        }
        return out;
    }
};

// ---------------------------------------------------------------------------
//  Голос короткого эффекта.
//  Состояния DSP — обычные массивы фиксированного размера: к ним обращаются
//  на каждом кадре, а в отладочной сборке MSVC каждый operator[] у
//  std::array — это вызов функции с проверкой границ.
// ---------------------------------------------------------------------------
struct Voice {
    bool active = false;
    bool console = false;    // эффект консоли: идёт через фильтр приглушения
    bool fading = false;     // вытеснен или остановлен — быстро затухает
    Sfx type = Sfx::MenuMove;
    uint32_t serial = 0;     // порядковый номер запуска
    float t = 0.0f;          // локальное время, с
    float dur = 0.0f;        // длительность в локальном времени, с
    float dtl = 0.0f;        // шаг локального времени за кадр (pitch / fs)
    float endFade = 0.0f;    // длина финального затухания в локальном времени
    float vol = 0.0f, gainL = 0.0f, gainR = 0.0f;
    float fade = 1.0f, fadeStep = 0.0f;
    float level = 0.0f;      // оценка текущей громкости (для выбора вытесняемого)
    float evNext = 0.0f;     // время следующего события (слог речи и т.п.)
    Rng rng{1u};
    float ph[16] = {};  // фазы осцилляторов
    float p[24] = {};   // параметры, выбранные при запуске, и состояния
    Svf svf[4];
    OnePole op[4];
    Ping pings[16];
    Wander wd[4];
    Lfsr lfsr;

    float white() { return rng.signedUnit(); }
    float at(int i) const { return t + dtl * fi(i); }
    // Обнулить затихшие до денормалов состояния фильтров (раз в блок).
    void flushFilters() {
        for (Svf& f : svf) f.flush();
        for (OnePole& f : op) f.flush();
    }
};

// ===========================================================================
//  Эффекты консоли: квадратные волны и регистровый шум. Все — через шину
//  консоли (приглушаются, когда консоль опущена на колени).
// ===========================================================================

// Подбор канистры: быстрое арпеджио вверх (До-Ми-Соль-До).
constexpr float kFuelNotes[4] = {1046.5f, 1318.5f, 1568.0f, 2093.0f};

void initPickupFuel(Voice& v) { v.dur = 0.42f; }

void renderPickupFuel(Voice& v, float* out, int n) {
    constexpr float kStep = 0.048f;
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        const int k = std::min(3, static_cast<int>(t / kStep));
        const float tn = t - kStep * fi(k);
        const float inc = phaseInc(kFuelNotes[k], v.dtl);
        const float env = k < 3 ? 0.8f + 0.2f * std::exp(-tn / 0.015f) : std::exp(-tn / 0.07f);
        out[i] = pulseWave(v.ph[0], inc, 0.25f) * env * 0.16f;
        advance(v.ph[0], inc);
    }
}

// Подбор предмета: джингл из пяти нот, последняя тянется с вибрато и терцией.
constexpr float kItemNotes[5] = {783.99f, 1046.5f, 1318.5f, 1568.0f, 2093.0f};

void initPickupItem(Voice& v) { v.dur = 0.86f; }

void renderPickupItem(Voice& v, float* out, int n) {
    constexpr float kStep = 0.062f;
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        const int k = std::min(4, static_cast<int>(t / kStep));
        const float tn = t - kStep * fi(k);
        float s = 0.0f;
        if (k < 4) {
            const float inc = phaseInc(kItemNotes[k], v.dtl);
            s = pulseWave(v.ph[0], inc, 0.5f) * (0.75f + 0.25f * std::exp(-tn / 0.02f)) * 0.11f;
            advance(v.ph[0], inc);
        } else {
            // Финальная нота: отложенное вибрато, тихая терция снизу.
            const float vib = 1.0f + 0.012f * sinPh(v.ph[2]) * smoothstep(0.05f, 0.15f, tn);
            advance(v.ph[2], phaseInc(6.5f, v.dtl));
            const float env = std::exp(-tn / 0.16f);
            const float inc = phaseInc(kItemNotes[4] * vib, v.dtl);
            const float inc2 = phaseInc(1661.2f * vib, v.dtl);
            s = (pulseWave(v.ph[0], inc, 0.25f) + 0.45f * pulseWave(v.ph[1], inc2, 0.125f)) * env * 0.1f;
            advance(v.ph[0], inc);
            advance(v.ph[1], inc2);
        }
        out[i] = s;
    }
}

// Авария: шумовой «взрыв» и нисходящий писк ступеньками (частота меняется
// 128 раз в секунду, как у аппаратного свипа старых консолей).
void initCrash(Voice& v) { v.dur = 0.62f; }

void renderCrash(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        const float tq = std::floor(t * 128.0f) / 128.0f;
        const float inc = phaseInc(55.0f + 800.0f * std::exp(-tq / 0.12f), v.dtl);
        const float sqEnv = smoothstep(0.0f, 0.004f, t) * (1.0f - smoothstep(0.3f, 0.6f, t));
        float s = pulseWave(v.ph[0], inc, 0.5f) * sqEnv * 0.12f;
        advance(v.ph[0], inc);
        const float clock = 1500.0f + 18000.0f * std::exp(-t / 0.09f);
        s += v.lfsr.tick(clock * v.dtl, false) * envAD(t, 0.001f, 0.11f) * 0.16f;
        out[i] = s;
    }
}

// Занос на масляном пятне: квадрат с быстрым «плаванием» тона + звенящий шум.
void initSkid(Voice& v) { v.dur = 0.36f; }

void renderSkid(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        const float f = 560.0f * (1.0f + 0.16f * sinPh(v.ph[1])) * (1.0f - 0.8f * t);
        advance(v.ph[1], phaseInc(27.0f, v.dtl));
        const float inc = phaseInc(f, v.dtl);
        const float env = envFade(t, 0.008f, 0.12f, v.dur);
        float s = pulseWave(v.ph[0], inc, 0.25f) * 0.1f + v.lfsr.tick(9000.0f * v.dtl, true) * 0.045f;
        advance(v.ph[0], inc);
        out[i] = s * env;
    }
}

// Включение консоли: короткий «пинг» и звонкий второй тон октавой выше.
void initBoot(Voice& v) { v.dur = 1.05f; }

void renderBoot(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        const bool first = t < 0.075f;
        const float inc = phaseInc(first ? 987.77f : 1975.53f, v.dtl);
        const float env = first ? 0.9f + 0.1f * std::exp(-t / 0.02f) : std::exp(-(t - 0.075f) / 0.28f);
        out[i] = pulseWave(v.ph[0], inc, 0.5f) * env * 0.14f;
        advance(v.ph[0], inc);
    }
}

// Короткий тик (отсчёт, меню консоли).
void initBlip(Voice& v) { v.dur = 0.06f; }

void renderBlip(Voice& v, float* out, int n) {
    const float inc = phaseInc(1760.0f, v.dtl);
    for (int i = 0; i < n; ++i) {
        out[i] = pulseWave(v.ph[0], inc, 0.5f) * envAD(v.at(i), 0.0005f, 0.012f) * 0.11f;
        advance(v.ph[0], inc);
    }
}

// ===========================================================================
//  Монстр, крыша, стекло
// ===========================================================================

// Тяжёлое приземление: глухой удар сквозь металл. Синус с падающей частотой
// (70 -> 35 Гц), низкий шумовой «пух», резонансы кузова 120-200 Гц и второй,
// более слабый удар (сначала ноги, затем руки).
void initRoofThud(Voice& v) {
    v.dur = 0.85f;
    const float d = v.rng.range(0.92f, 1.08f);
    v.p[0] = v.rng.range(0.92f, 1.08f); // масштаб частоты удара
    v.p[1] = v.rng.range(0.07f, 0.12f); // задержка второго удара
    v.pings[0].start(127.0f * d, 0.09f, 0.30f, v.dtl);
    v.pings[1].start(168.0f * d, 0.065f, 0.24f, v.dtl);
    v.pings[2].start(203.0f * d, 0.05f, 0.20f, v.dtl);
    v.pings[3].start(415.0f * d, 0.018f, 0.12f, v.dtl);
    v.pings[4].start(590.0f * d, 0.013f, 0.09f, v.dtl);
    v.op[0].tune(260.0f * v.dtl);
    v.op[1].tune(260.0f * v.dtl);
}

void renderRoofThud(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        const float inc = phaseInc((35.0f + 35.0f * std::exp(-t / 0.11f)) * v.p[0], v.dtl);
        float s = sinPh(v.ph[0]) * envAD(t, 0.004f, 0.2f) * 0.47f;
        advance(v.ph[0], inc);
        const float t2 = t - v.p[1];
        if (t2 > 0.0f) {
            s += sinPh(v.ph[1]) * envAD(t2, 0.003f, 0.12f) * 0.22f;
            advance(v.ph[1], phaseInc((40.0f + 22.0f * std::exp(-t2 / 0.06f)) * v.p[0], v.dtl));
        }
        s += v.op[1].lp(v.op[0].lp(v.white())) * envAD(t, 0.002f, 0.05f) * 1.9f;
        for (int k = 0; k < 5; ++k) s += v.pings[k].tick();
        out[i] = s;
    }
}

// Шаг/перехват по крыше: меньший глухой удар, каждый раз немного другой;
// иногда — тонкий «цок» когтя по металлу.
void initRoofStep(Voice& v) {
    v.dur = 0.42f;
    v.p[0] = v.rng.range(72.0f, 98.0f);   // начальная частота удара
    v.p[1] = v.rng.range(0.05f, 0.09f);   // спад
    v.p[2] = v.rng.range(0.7f, 1.0f);     // сила
    v.p[3] = v.rng.chance(0.45f) ? v.rng.range(0.005f, 0.04f) : -1.0f; // задержка «цока»
    v.p[4] = v.rng.range(2100.0f, 3600.0f);
    const float lpHz = v.rng.range(220.0f, 420.0f);
    v.op[0].tune(lpHz * v.dtl);
    v.op[1].tune(lpHz * v.dtl);
    v.pings[0].start(v.rng.range(135.0f, 190.0f), 0.04f * v.p[2], v.rng.range(0.08f, 0.13f), v.dtl);
}

void renderRoofStep(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        const float inc = phaseInc(v.p[0] * (0.55f + 0.45f * std::exp(-t / 0.035f)), v.dtl);
        float s = sinPh(v.ph[0]) * envAD(t, 0.003f, v.p[1]) * 0.3f * v.p[2];
        advance(v.ph[0], inc);
        s += v.op[1].lp(v.op[0].lp(v.white())) * envAD(t, 0.001f, 0.025f) * 1.2f * v.p[2];
        if (v.p[3] >= 0.0f && t >= v.p[3]) {
            v.pings[1].start(v.p[4], 0.035f, 0.006f, v.dtl);
            v.p[3] = -1.0f;
        }
        s += v.pings[0].tick() + v.pings[1].tick();
        out[i] = s;
    }
}

// Скрежет когтей по жести: «прилипание-срыв» когтя (серия импульсов с
// плавающей частотой) возбуждает два узких резонанса 1.5-4 кГц, центры
// которых дрожат, плюс низкий отклик панели крыши и зернистая модуляция.
void initRoofScrape(Voice& v) {
    v.dur = v.rng.range(0.5f, 0.9f);
    v.p[0] = v.rng.range(1500.0f, 2500.0f); // центр первого резонанса
    v.p[1] = v.rng.range(2700.0f, 4000.0f); // центр второго
    v.p[2] = v.rng.range(-0.3f, 0.3f);      // дрейф центров за время скрежета
    v.p[3] = v.rng.range(85.0f, 160.0f);    // частота срывов когтя
    v.p[4] = 1.0f;                          // сила последнего срыва (зернистость)
    v.p[5] = 1.0f;                          // она же, сглаженная
    v.svf[2].tune(v.rng.range(600.0f, 900.0f) * v.dtl, 4.0f);
}

void renderRoofScrape(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        const float drift = 1.0f + v.p[2] * (t / v.dur - 0.5f);
        const float w0 = v.wd[0].next(v.rng, 14.0f * v.dtl);
        const float w1 = v.wd[1].next(v.rng, 19.0f * v.dtl);
        const float w2 = v.wd[2].next(v.rng, 38.0f * v.dtl);
        const float w3 = v.wd[3].next(v.rng, 3.0f * v.dtl);
        v.svf[0].tune(v.p[0] * drift * (1.0f + 0.25f * w0) * v.dtl, 10.0f);
        v.svf[1].tune(v.p[1] * drift * (1.0f + 0.2f * w1) * v.dtl, 14.0f);
        float imp = 0.0f;
        v.ph[0] += v.p[3] * (1.0f + 0.4f * w2) * v.dtl;
        if (v.ph[0] >= 1.0f) {
            v.ph[0] -= 1.0f;
            imp = v.rng.range(0.4f, 1.0f) * (v.rng.chance(0.5f) ? 1.0f : -1.0f);
            v.p[4] = v.rng.range(0.35f, 1.0f);
        }
        v.p[5] += (v.p[4] - v.p[5]) * 0.02f;
        const float exc = imp * 4.0f + v.white() * 0.6f;
        const float r = v.svf[0].bp(exc) + 0.7f * v.svf[1].bp(exc) + 0.35f * v.svf[2].bp(exc);
        const float env = envFade(t, 0.04f, v.dur * 0.3f, v.dur) * (0.8f + 0.2f * w3);
        out[i] = r * v.p[5] * env * 1.5f;
    }
}

// Стук костяшками по стеклу: щелчок + короткие высокие резонансы 2-3.5 кГц.
void initGlassKnock(Voice& v) {
    v.dur = 0.26f;
    const float d = v.rng.range(0.94f, 1.06f);
    v.pings[0].start(430.0f * d, 0.17f, 0.012f, v.dtl); // глухой «ток» костяшки
    v.pings[1].start(2150.0f * d, 0.085f, 0.034f, v.dtl);
    v.pings[2].start(2870.0f * d * v.rng.range(0.98f, 1.02f), 0.06f, 0.027f, v.dtl);
    v.pings[3].start(3420.0f * d, 0.045f, 0.021f, v.dtl);
    v.pings[4].start(6100.0f * d, 0.02f, 0.004f, v.dtl);
    v.op[0].tune(1800.0f * v.dtl);
}

void renderGlassKnock(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        float s = v.op[0].hp(v.white()) * std::exp(-t / 0.0015f) * 0.2f;
        for (int k = 0; k < 5; ++k) s += v.pings[k].tick();
        out[i] = s;
    }
}

// Сильный удар по стеклу: тупой удар ладони, шлепок, дребезг рамы и короткий звон.
void initGlassBang(Voice& v) {
    v.dur = 0.85f;
    const float d = v.rng.range(0.93f, 1.07f);
    v.pings[0].start(1250.0f * d, 0.05f, 0.24f, v.dtl);
    v.pings[1].start(1830.0f * d * v.rng.range(0.97f, 1.03f), 0.045f, 0.2f, v.dtl);
    v.pings[2].start(2710.0f * d, 0.035f, 0.16f, v.dtl);
    v.pings[3].start(3650.0f * d, 0.025f, 0.12f, v.dtl);
    v.pings[4].start(265.0f * d, 0.22f, 0.03f, v.dtl);
    v.p[0] = v.rng.range(28.0f, 40.0f); // частота дребезга рамы
    v.svf[0].tune(v.rng.range(800.0f, 1000.0f) * v.dtl, 3.0f);
    v.svf[1].tune(v.rng.range(2000.0f, 2500.0f) * v.dtl, 4.0f);
    v.op[0].tune(700.0f * v.dtl);
    v.op[1].tune(1200.0f * v.dtl);
}

void renderGlassBang(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        float s = sinPh(v.ph[0]) * envAD(t, 0.002f, 0.12f) * 0.5f;
        advance(v.ph[0], phaseInc(48.0f + 47.0f * std::exp(-t / 0.05f), v.dtl));
        s += v.op[0].lp(v.white()) * envAD(t, 0.001f, 0.035f) * 1.6f;
        s += v.op[1].hp(v.white()) * std::exp(-t / 0.006f) * 0.45f;
        // Дребезг: нерегулярные удары рамы о стекло.
        const float w0 = v.wd[0].next(v.rng, 25.0f * v.dtl);
        float imp = 0.0f;
        v.ph[1] += v.p[0] * (1.0f + 0.3f * w0) * v.dtl;
        if (v.ph[1] >= 1.0f) {
            v.ph[1] -= 1.0f;
            imp = v.rng.range(0.3f, 1.0f);
        }
        const float ex = imp * 3.0f + v.white() * 0.25f;
        const float rattle = v.svf[0].bp(ex) + 0.6f * v.svf[1].bp(ex);
        s += rattle * smoothstep(0.0f, 0.015f, t) * std::exp(-t / 0.17f) * 1.2f;
        for (int k = 0; k < 5; ++k) s += v.pings[k].tick();
        out[i] = s * 0.46f;
    }
}

// Треск стекла: россыпь крошечных острых щелчков + высокий шум.
void initGlassCrack(Voice& v) {
    v.dur = 0.46f;
    v.op[0].tune(2500.0f * v.dtl);
    v.op[1].tune(3500.0f * v.dtl);
    v.op[2].tune(3500.0f * v.dtl);
}

void renderGlassCrack(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        const float rate = t < 0.36f ? 380.0f * std::exp(-t / 0.1f) + 22.0f : 0.0f;
        float imp = 0.0f;
        if (v.rng.next01() < rate * v.dtl) {
            imp = v.rng.range(0.3f, 1.0f) * (v.rng.chance(0.5f) ? 1.0f : -1.0f);
            quietestPing(v.pings).start(v.rng.range(3500.0f, 9500.0f), v.rng.range(0.05f, 0.15f),
                                        v.rng.range(0.0012f, 0.004f), v.dtl);
        }
        float s = v.op[0].hp(imp) * 0.7f;
        s += v.op[2].hp(v.op[1].hp(v.white())) * envAD(t, 0.004f, 0.11f) * 0.2f;
        for (Ping& pg : v.pings) s += pg.tick();
        out[i] = s * 0.6f;
    }
}

// Разбитое стекло: удар, шорох осыпающейся массы и множество звенящих
// осколков (каждый — пара затухающих синусов с «стеклянным» соотношением).
void initGlassShatter(Voice& v) {
    v.dur = 1.65f;
    v.op[0].tune(1200.0f * v.dtl);
    v.op[1].tune(2600.0f * v.dtl);
}

void renderGlassShatter(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        float s = v.op[0].hp(v.white()) * envAD(t, 0.0008f, 0.07f) * 0.55f;
        s += v.op[1].lp(v.white()) * envAD(t, 0.002f, 0.25f) * 0.35f;
        s += sinPh(v.ph[0]) * envAD(t, 0.003f, 0.12f) * 0.4f;
        advance(v.ph[0], phaseInc(45.0f + 35.0f * std::exp(-t / 0.08f), v.dtl));
        const float rate = t < 1.35f ? 230.0f * std::exp(-t / 0.22f) + 16.0f * std::exp(-t / 0.8f) : 0.0f;
        if (v.rng.next01() < rate * v.dtl) {
            const float f = 2200.0f * std::pow(4.2f, v.rng.next01());
            const float a = v.rng.range(0.04f, 0.13f) * (0.45f + 0.55f * std::exp(-t / 0.4f));
            const float tau = v.rng.range(0.015f, 0.09f);
            quietestPing(v.pings).start(f, a, tau, v.dtl);
            const float f2 = f * v.rng.range(2.3f, 2.9f);
            if (f2 < 15000.0f) quietestPing(v.pings).start(f2, a * 0.4f, tau * 0.6f, v.dtl);
        }
        for (Ping& pg : v.pings) s += pg.tick();
        out[i] = s * 0.6f;
    }
}

// Рычание: хриплый неровный голосовой источник (дрожание периода) + шум
// дыхания через движущиеся форманты, суб-бас 45-62 Гц, медленное «клокотание».
void initGrowl(Voice& v) {
    v.dur = v.rng.range(1.0f, 1.4f);
    v.p[0] = v.rng.range(55.0f, 70.0f);    // основной тон
    v.p[1] = v.rng.range(45.0f, 62.0f);    // суб-бас
    v.p[2] = v.rng.range(3.5f, 6.0f);      // частота клокотания
    v.p[3] = v.rng.range(280.0f, 340.0f);  // F1: начало
    v.p[4] = v.rng.range(480.0f, 580.0f);  // F1: середина
    v.p[5] = v.rng.range(850.0f, 950.0f);  // F2: начало
    v.p[6] = v.rng.range(1200.0f, 1400.0f); // F2: середина
    v.p[7] = 1.0f;                         // амплитуда текущего периода
    v.svf[2].tune(180.0f * v.dtl, 2.0f);
}

void renderGrowl(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        const float b = sinPh(0.5f * saturate(t / v.dur));
        const float w0 = v.wd[0].next(v.rng, 7.0f * v.dtl);
        const float w1 = v.wd[1].next(v.rng, 4.0f * v.dtl);
        const float w2 = v.wd[2].next(v.rng, 26.0f * v.dtl);
        const float inc = phaseInc(v.p[0] * (0.9f + 0.18f * b) * (1.0f + 0.1f * w0), v.dtl);
        v.ph[0] += inc;
        if (v.ph[0] >= 1.0f) {
            v.ph[0] -= 1.0f;
            v.p[7] = v.rng.range(0.55f, 1.25f);
        }
        const float src = sawWave(v.ph[0], inc) * v.p[7] * 0.8f + v.white() * 0.5f;
        v.svf[0].tune(lerpf(v.p[3], v.p[4], b) * (1.0f + 0.08f * w1) * v.dtl, 5.0f);
        v.svf[1].tune(lerpf(v.p[5], v.p[6], b) * (1.0f + 0.06f * w1) * v.dtl, 7.0f);
        const float y = v.svf[0].bp(src) + 0.5f * v.svf[1].bp(src) + 0.8f * v.svf[2].bp(src);
        const float gurgle = 0.5f + 0.5f * sinPh(v.ph[2]);
        advance(v.ph[2], phaseInc(v.p[2], v.dtl));
        const float am = (1.0f - 0.4f * gurgle) * (0.75f + 0.25f * w2);
        const float sub = sinPh(v.ph[1]) * 0.25f * (0.7f + 0.3f * gurgle);
        advance(v.ph[1], phaseInc(v.p[1], v.dtl));
        out[i] = softSat(2.0f * (y * am + sub)) * 0.42f * envFade(t, 0.18f, 0.38f, v.dur);
    }
}

// Визг: две ЧМ-пары с негармоническим соотношением (одна — на полтона выше,
// отсюда «шершавость»), тон взлетает и опадает; сверху — шум дыхания.
void initScreech(Voice& v) {
    v.dur = 0.92f;
    v.p[0] = v.rng.range(0.94f, 1.06f);
    v.p[1] = std::log(1650.0f / 640.0f); // взлёт тона
    v.p[2] = std::log(760.0f / 1650.0f); // спад тона
}

void renderScreech(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        float f = t < 0.24f ? 640.0f * std::exp(v.p[1] * smoothstep(0.0f, 0.24f, t))
                            : 1650.0f * std::exp(v.p[2] * (t - 0.24f) / 0.68f);
        const float w0 = v.wd[0].next(v.rng, 12.0f * v.dtl);
        f *= v.p[0] * (1.0f + 0.03f * sinPh(v.ph[4]) + 0.02f * w0);
        advance(v.ph[4], phaseInc(9.5f, v.dtl));
        const float env = envFade(t, 0.02f, 0.4f, v.dur);
        const float index = 1.6f + 1.4f * env;
        const float s1 = sinPh(v.ph[0] + index / kTau * sinPh(v.ph[1]));
        const float s2 = sinPh(v.ph[2] + index * 0.8f / kTau * sinPh(v.ph[3]));
        advance(v.ph[0], phaseInc(f, v.dtl));
        advance(v.ph[1], phaseInc(f * 1.47f, v.dtl));
        advance(v.ph[2], phaseInc(f * 1.059f, v.dtl));
        advance(v.ph[3], phaseInc(f * 1.059f * 1.41f, v.dtl));
        v.svf[0].tune(f * 2.0f * v.dtl, 3.0f);
        const float breath = v.svf[0].bp(v.white()) * 0.9f;
        out[i] = softSat(1.5f * (s1 + 0.5f * s2 + breath) * env) * 0.36f;
    }
}

// ===========================================================================
//  Камера
// ===========================================================================

// Затвор: щелчок, хлопок вспышки, «клак» шторки и тонкий писк заряда конденсатора.
void initShutter(Voice& v) {
    v.dur = 1.15f;
    v.pings[0].start(3100.0f, 0.15f, 0.006f, v.dtl);
    v.op[0].tune(2000.0f * v.dtl);
    v.op[1].tune(1100.0f * v.dtl);
    v.svf[0].tune(1700.0f * v.dtl, 2.0f);
    v.p[0] = 0.0f; // 1 — «клак» уже запущен
}

void renderShutter(Voice& v, float* out, int n) {
    constexpr float kPop = 0.012f, kClack = 0.085f, kWhine = 0.14f;
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        float s = v.op[0].hp(v.white()) * std::exp(-t / 0.0025f) * 0.45f;
        const float tp = t - kPop;
        if (tp > 0.0f) {
            s += v.op[1].lp(v.white()) * envAD(tp, 0.001f, 0.022f) * 1.5f;
            s += sinPh(v.ph[0]) * envAD(tp, 0.002f, 0.03f) * 0.25f;
            advance(v.ph[0], phaseInc(150.0f, v.dtl));
        }
        const float tc = t - kClack;
        if (tc > 0.0f) {
            if (v.p[0] < 0.5f) {
                v.pings[1].start(2250.0f, 0.16f, 0.009f, v.dtl);
                v.pings[2].start(880.0f, 0.12f, 0.014f, v.dtl);
                v.p[0] = 1.0f;
            }
            s += v.svf[0].bp(v.white()) * std::exp(-tc / 0.005f) * 0.9f;
        }
        for (int k = 0; k < 3; ++k) s += v.pings[k].tick();
        if (t > kWhine) {
            const float f = 3200.0f * std::exp(1.0986123f * (t - kWhine) / 0.95f); // x3 за 0.95 с
            s += sinPh(v.ph[1]) * 0.03f * smoothstep(kWhine, 0.3f, t) * (1.0f - smoothstep(0.85f, 1.12f, t));
            advance(v.ph[1], phaseInc(f, v.dtl));
        }
        out[i] = s * 0.67f;
    }
}

// Вспышка заряжена: тонкий восходящий писк.
void initCameraReady(Voice& v) { v.dur = 0.24f; }

void renderCameraReady(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        const float f = 2400.0f + 1000.0f * smoothstep(0.0f, 0.12f, t);
        const float s = sinPh(v.ph[0]) + 0.15f * sinPh(v.ph[1]);
        advance(v.ph[0], phaseInc(f, v.dtl));
        advance(v.ph[1], phaseInc(f * 2.0f, v.dtl));
        out[i] = s * envFade(t, 0.004f, 0.07f, v.dur) * 0.085f;
    }
}

// Вспышка не заряжена: сухой глухой щелчок кнопки (нажатие и отпускание).
void initCameraEmpty(Voice& v) {
    v.dur = 0.16f;
    v.op[0].tune(1600.0f * v.dtl);
}

void renderCameraEmpty(Voice& v, float* out, int n) {
    constexpr float kRelease = 0.055f;
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        const float t2 = t - kRelease;
        const float nz = v.op[0].lp(v.white());
        float s = nz * std::exp(-t / 0.004f) * 1.4f + sinPh(v.ph[0]) * envAD(t, 0.0008f, 0.01f) * 0.28f;
        advance(v.ph[0], phaseInc(320.0f, v.dtl));
        if (t2 > 0.0f) {
            s += nz * std::exp(-t2 / 0.003f) * 0.7f + sinPh(v.ph[1]) * envAD(t2, 0.0008f, 0.008f) * 0.14f;
            advance(v.ph[1], phaseInc(260.0f, v.dtl));
        }
        out[i] = s * 0.43f;
    }
}

// ===========================================================================
//  Машина
// ===========================================================================

// Чихание двигателя: несколько «кашлей» с провалами тона и хлопками.
// p[0..5] — моменты кашлей (-1: нет), p[6..11] — их сила, p[12] — частота.
void initSputter(Voice& v) {
    v.dur = 0.85f;
    float tc = 0.0f;
    for (int k = 0; k < 6; ++k) {
        v.p[k] = tc < 0.62f ? tc : -1.0f;
        v.p[6 + k] = v.rng.range(0.6f, 1.0f) * (1.0f - 0.12f * fi(k));
        tc += v.rng.range(0.1f, 0.2f);
    }
    v.p[12] = v.rng.range(36.0f, 44.0f);
    v.svf[0].tune(280.0f * v.dtl, 0.9f);
    v.op[0].tune(700.0f * v.dtl);
}

void renderSputter(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        float e = 0.0f, pop = 0.0f;
        for (size_t k = 0; k < 6; ++k) {
            const float tc = v.p[k];
            if (tc < 0.0f || t < tc || t - tc > 0.45f) continue;
            e += v.p[6 + k] * envAD(t - tc, 0.01f, 0.07f);
            pop += v.p[6 + k] * std::exp((tc - t) / 0.012f);
        }
        e = std::min(e, 1.2f);
        const float f = v.p[12] * (0.68f + 0.32f * std::min(e, 1.0f));
        const float inc = phaseInc(f, v.dtl);
        const float tone = sawWave(v.ph[0], inc) * 0.6f + sinPh(v.ph[1]) * 0.5f;
        advance(v.ph[0], inc);
        advance(v.ph[1], phaseInc(f * 0.5f, v.dtl));
        out[i] = v.svf[0].lp(tone) * e * 0.66f + v.op[0].lp(v.white()) * pop * 1.4f;
    }
}

// Двигатель глохнет: вспышки в цилиндрах всё реже (сливаются в гул, потом
// распадаются на отдельные «чух»), в конце — глухой стук и металлический тик.
void initEngineDie(Voice& v) {
    v.dur = 1.6f;
    v.p[0] = v.rng.range(40.0f, 46.0f); // начальная частота вспышек
    v.p[1] = 0.0f;                      // огибающая хлопка текущей вспышки
    v.p[2] = 0.0f;                      // 1 — финальный стук уже был
    v.svf[0].tune(220.0f * v.dtl, 0.8f);
    v.op[0].tune(500.0f * v.dtl);
}

void renderEngineDie(Voice& v, float* out, int n) {
    constexpr float kStop = 1.22f, kClunk = 1.27f;
    const float popMul = std::exp(-v.dtl / 0.01f);
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        const float x = t / kStop;
        float s = 0.0f;
        if (x < 1.0f) {
            const float rate = 4.0f + (v.p[0] - 4.0f) * (1.0f - x) * std::sqrt(1.0f - x);
            const float inc = phaseInc(rate, v.dtl);
            v.ph[0] += inc;
            if (v.ph[0] >= 1.0f) {
                v.ph[0] -= 1.0f;
                const float lvl = 0.75f - 0.35f * x;
                quietestPing(v.pings).start(v.rng.range(58.0f, 72.0f), 0.32f * lvl, 0.04f, v.dtl);
                v.p[1] = 0.5f * lvl;
            }
            const float k = (1.0f - x) * (1.0f - x);
            s += v.svf[0].lp(sawWave(v.ph[0], inc)) * 0.3f * k;
        } else if (t >= kClunk && v.p[2] < 0.5f) {
            v.pings[0].start(95.0f, 0.3f, 0.06f, v.dtl);
            v.pings[1].start(140.0f, 0.15f, 0.05f, v.dtl);
            v.pings[2].start(1900.0f, 0.04f, 0.01f, v.dtl);
            v.p[2] = 1.0f;
        }
        s += v.op[0].lp(v.white()) * v.p[1] * 1.4f;
        v.p[1] *= popMul;
        flushTiny(v.p[1]);
        for (Ping& pg : v.pings) s += pg.tick();
        out[i] = s * 0.88f;
    }
}

// Проезд под фонарём: мягкий воздушный «вжух» (центр полосы взлетает и опадает).
void initWhoosh(Voice& v) {
    v.dur = 0.62f;
    v.op[0].tune(4000.0f * v.dtl);
}

void renderWhoosh(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        const float x = saturate(t / v.dur);
        const float b = sinPh(0.5f * x);
        v.svf[0].tune((600.0f + 2200.0f * b * b) * v.dtl, 1.1f);
        out[i] = v.op[0].lp(v.svf[0].bp(v.white())) * b * b * 0.13f;
    }
}

// ===========================================================================
//  «Бубнёж» родителей: речеподобный звук, слышный сквозь спинки сидений.
//  Голосовой источник (пила + наклон спектра) с интонацией -> три
//  полосовые форманты, переходящие между гласными -> взрывные согласные ->
//  сильное НЧ-«приглушение». Длительность задаёт аргумент pitch.
// ===========================================================================

// Форманты гласных (F1, F2, F3) мужского голоса: а, о, э, и, у.
constexpr float kVowels[5][3] = {
    {700.0f, 1200.0f, 2500.0f},
    {500.0f, 850.0f, 2450.0f},
    {480.0f, 1750.0f, 2500.0f},
    {290.0f, 2250.0f, 2900.0f},
    {300.0f, 650.0f, 2300.0f},
};

// Индексы в Voice::p для «бубнежа».
enum MumbleSlot : size_t {
    kMF0 = 0,      // базовый тон
    kMRange,       // размах интонации
    kMAccentMax,   // максимум ударения
    kMScale,       // масштаб формант (женский голос выше)
    kMSylStart,    // начало текущего слога
    kMSylLen,      // длина слога
    kMSylAmp,      // громкость слога (0 — пауза)
    kMAccent,      // ударение слога
    kMPhrase,      // начало текущей фразы (для понижения тона к концу)
    kMBurst,       // огибающая согласной
    kMSpeechEnd,   // конец речи
    kMVowel,       // предыдущая гласная
    kMF1,          // текущие форманты (3 шт.)
    kMT1 = kMF1 + 3, // целевые форманты (3 шт.)
    kMGate = kMT1 + 3, // сглаженная громкость голоса
};

void initMumble(Voice& v, bool dad, float seconds) {
    const float speech = sanitize(seconds, 0.3f, 8.0f, 1.5f);
    v.dur = speech + 0.12f;
    v.p[kMF0] = dad ? v.rng.range(100.0f, 118.0f) : v.rng.range(190.0f, 228.0f);
    v.p[kMRange] = dad ? 0.10f : 0.16f;
    v.p[kMAccentMax] = dad ? 0.10f : 0.18f;
    v.p[kMScale] = dad ? 1.0f : 1.17f;
    v.p[kMSpeechEnd] = speech;
    v.p[kMVowel] = -1.0f;
    v.p[kMSylLen] = 1.0f;
    for (size_t j = 0; j < 3; ++j) {
        v.p[kMF1 + j] = kVowels[0][j] * v.p[kMScale];
        v.p[kMT1 + j] = v.p[kMF1 + j];
    }
    v.evNext = 0.0f;
    v.op[0].tune(700.0f * v.dtl);  // наклон спектра голосового источника
    v.op[1].tune(1500.0f * v.dtl); // ВЧ для шума согласных
    v.svf[3].tune(1700.0f * v.dtl, 0.7f);
}

void startSyllable(Voice& v, float t) {
    v.p[kMSylStart] = t;
    const float remain = v.p[kMSpeechEnd] - t;
    if (remain < 0.09f) { // речь окончена: тишина до конца голоса
        v.p[kMSylAmp] = 0.0f;
        v.p[kMSylLen] = 1.0f;
        v.evNext = v.dur + 1.0f;
        return;
    }
    if (t > 0.35f && remain > 0.6f && v.rng.chance(0.13f)) { // вдох между фразами
        v.p[kMSylAmp] = 0.0f;
        v.p[kMSylLen] = v.rng.range(0.14f, 0.32f);
        v.p[kMPhrase] = t + v.p[kMSylLen];
        v.evNext = t + v.p[kMSylLen];
        return;
    }
    const float len = std::min(1.0f / v.rng.range(4.0f, 7.0f), remain);
    v.p[kMSylLen] = len;
    v.p[kMSylAmp] = v.rng.range(0.55f, 1.0f);
    v.p[kMAccent] = v.rng.range(-0.25f, 1.0f) * v.p[kMAccentMax];
    int vowel = v.rng.rangeInt(0, 4);
    if (vowel == static_cast<int>(v.p[kMVowel])) vowel = (vowel + 1 + v.rng.rangeInt(0, 3)) % 5;
    v.p[kMVowel] = fi(vowel);
    for (size_t j = 0; j < 3; ++j)
        v.p[kMT1 + j] = kVowels[vowel][j] * v.p[kMScale] * v.rng.range(0.95f, 1.05f);
    if (v.rng.chance(0.6f)) v.p[kMBurst] = v.rng.range(0.4f, 1.0f);
    v.evNext = t + len;
}

void renderMumble(Voice& v, float* out, int n) {
    const float formK = 1.0f - std::exp(-v.dtl / 0.028f);
    const float gateK = 1.0f - std::exp(-v.dtl / 0.012f);
    const float burstMul = std::exp(-v.dtl / 0.018f);
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        if (t >= v.evNext) startSyllable(v, t);
        // Огибающая слога с мягкими краями. Внутри фразы голос между слогами
        // не обрывается (слитная речь), в паузах между фразами — затихает.
        const float u = (t - v.p[kMSylStart]) / v.p[kMSylLen];
        float shape = 0.0f;
        if (u >= 0.0f && u < 1.0f) {
            if (u < 0.22f) {
                const float s = sinPh(0.25f * u / 0.22f);
                shape = s * s;
            } else if (u > 0.62f) {
                const float c = sinPh(0.25f + 0.25f * (u - 0.62f) / 0.38f); // косинус
                shape = c * c;
            } else {
                shape = 1.0f;
            }
        }
        v.p[kMGate] += (v.p[kMSylAmp] * (0.3f + 0.7f * shape) - v.p[kMGate]) * gateK;
        const float gate = v.p[kMGate];
        // Интонация: тон понижается к концу фразы, ударные слоги выше.
        const float sincePhrase = std::max(0.0f, t - v.p[kMPhrase]);
        const float contour = 1.0f + v.p[kMRange] * (0.7f * std::exp(-sincePhrase / 1.3f) - 0.25f) +
                              v.p[kMAccent] * shape;
        const float jitter = v.wd[0].next(v.rng, 9.0f * v.dtl);
        const float f0 = v.p[kMF0] * contour * (1.0f + 0.012f * sinPh(v.ph[1]) + 0.006f * jitter);
        advance(v.ph[1], phaseInc(5.3f, v.dtl));
        const float inc = phaseInc(f0, v.dtl);
        const float src = v.op[0].lp(sawWave(v.ph[0], inc)) * 3.0f + v.white() * 0.04f;
        advance(v.ph[0], inc);
        // Форманты плавно переходят к гласной текущего слога.
        for (size_t j = 0; j < 3; ++j) v.p[kMF1 + j] += (v.p[kMT1 + j] - v.p[kMF1 + j]) * formK;
        v.svf[0].tune(v.p[kMF1] * v.dtl, 6.0f);
        v.svf[1].tune(v.p[kMF1 + 1] * v.dtl, 9.0f);
        v.svf[2].tune(v.p[kMF1 + 2] * v.dtl, 11.0f);
        float y = (v.svf[0].bp(src) + 0.55f * v.svf[1].bp(src) + 0.2f * v.svf[2].bp(src)) * gate;
        y += v.op[1].hp(v.white()) * v.p[kMBurst] * 0.25f;
        v.p[kMBurst] *= burstMul;
        y = v.svf[3].lp(y);
        out[i] = y * envFade(t, 0.03f, 0.1f, v.dur) * 0.2f;
    }
}

// ===========================================================================
//  Сюжетные и меню
// ===========================================================================

// Скример: искажённый крик (три расстроенные пилы через форманту «а»),
// низкий удар и шумовой взрыв. Пик ~0.9 после мягкого ограничителя.
void initJumpscare(Voice& v) {
    v.dur = 1.85f;
    v.svf[0].tune(850.0f * v.dtl, 4.0f);
    v.svf[1].tune(1300.0f * v.dtl, 5.0f);
    v.svf[2].tune(2700.0f * v.dtl, 6.0f);
    v.op[0].tune(300.0f * v.dtl);
}

constexpr float kScream[3] = {520.0f, 790.0f, 1180.0f}; // три голоса крика, Гц

void renderJumpscare(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        const float w0 = v.wd[0].next(v.rng, 15.0f * v.dtl);
        const float glide =
            (1.0f + 0.25f * smoothstep(0.0f, 0.15f, t)) * (1.0f - 0.12f * smoothstep(0.5f, 1.5f, t));
        const float vib = 1.0f + 0.05f * sinPh(v.ph[4]) + 0.03f * w0;
        advance(v.ph[4], phaseInc(11.0f, v.dtl));
        float raw = 0.0f;
        for (size_t k = 0; k < 3; ++k) {
            const float inc = phaseInc(kScream[k] * glide * vib, v.dtl);
            raw += sawWave(v.ph[k], inc);
            advance(v.ph[k], inc);
        }
        raw += v.white() * 0.4f;
        const float voiced =
            v.svf[0].bp(raw) + 0.8f * v.svf[1].bp(raw) + 0.5f * v.svf[2].bp(raw) + 0.25f * raw;
        const float scream = softSat(3.0f * voiced) * envFade(t, 0.008f, 0.75f, 1.5f);
        const float boom = sinPh(v.ph[5]) * envAD(t, 0.003f, 0.4f);
        advance(v.ph[5], phaseInc(28.0f + 30.0f * std::exp(-t / 0.25f), v.dtl));
        const float burst = v.op[0].hp(v.white()) * envAD(t, 0.0005f, 0.12f);
        out[i] = softSat(0.75f * scream + 0.9f * boom + 0.7f * burst) * 1.02f;
    }
}

// Проигрыш: мрачный диссонансный аккорд, медленно нарастающий из глубины.
constexpr float kLoseLow[4] = {55.0f, 58.27f, 82.41f, 155.56f}; // Ля1 Си-бемоль1 Ми2 Ми-бемоль3
constexpr float kLoseHigh[2] = {493.88f, 523.25f};              // малая секунда наверху

void initLoseSting(Voice& v) { v.dur = 2.7f; }

void renderLoseSting(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        float chord = 0.0f;
        for (size_t k = 0; k < 4; ++k) {
            const float f = kLoseLow[k];
            const float incA = phaseInc(f * 0.9965f, v.dtl);
            const float incB = phaseInc(f * 1.0035f, v.dtl);
            chord += sawWave(v.ph[2 * k], incA) + sawWave(v.ph[2 * k + 1], incB);
            advance(v.ph[2 * k], incA);
            advance(v.ph[2 * k + 1], incB);
        }
        const float open = smoothstep(0.0f, 1.1f, t) * (1.0f - smoothstep(1.3f, 2.6f, t));
        v.svf[0].tune((140.0f + 950.0f * open) * v.dtl, 1.2f);
        float s = v.svf[0].lp(chord) * 0.085f;
        const float trem = 0.6f + 0.4f * sinPh(v.ph[12]);
        advance(v.ph[12], phaseInc(0.7f, v.dtl));
        s += (sinPh(v.ph[8]) + sinPh(v.ph[9]) * trem) * 0.02f;
        advance(v.ph[8], phaseInc(kLoseHigh[0], v.dtl));
        advance(v.ph[9], phaseInc(kLoseHigh[1], v.dtl));
        s *= smoothstep(0.0f, 0.9f, t) * (1.0f - smoothstep(1.5f, 2.65f, t));
        s += sinPh(v.ph[10]) * envAD(t, 0.005f, 0.55f) * 0.45f;
        advance(v.ph[10], phaseInc(32.0f + 13.0f * std::exp(-t / 0.3f), v.dtl));
        out[i] = s;
    }
}

// Победа: тёплое арпеджио ре мажора «электропианино» (ЧМ-синтез) поверх
// мягкого пэда.
constexpr float kWinNotes[6] = {293.66f, 369.99f, 440.0f, 587.33f, 739.99f, 880.0f};
constexpr float kWinOnsets[6] = {0.0f, 0.12f, 0.24f, 0.36f, 0.48f, 0.62f};
constexpr float kWinPad[3] = {146.83f, 185.0f, 220.0f};

// p[0..5] — затухание громкости нот, p[6..11] — затухание яркости (индекса ЧМ).
void initWinJingle(Voice& v) {
    v.dur = 2.7f;
    for (size_t k = 0; k < 6; ++k) {
        v.p[k] = 1.0f;
        v.p[6 + k] = 1.0f;
    }
    v.p[12] = std::exp(-v.dtl / 0.9f);
    v.p[13] = std::exp(-v.dtl / 0.3f);
}

void renderWinJingle(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        float s = 0.0f;
        for (size_t k = 0; k < 6; ++k) {
            const float tt = t - kWinOnsets[k];
            if (tt <= 0.0f) continue;
            const float index = 1.2f * v.p[6 + k] + 0.15f;
            const float env = std::min(1.0f, tt / 0.004f) * v.p[k] * (k == 5 ? 0.13f : 0.1f);
            s += sinPh(v.ph[2 * k] + index / kTau * sinPh(v.ph[2 * k + 1])) * env;
            v.p[k] *= v.p[12];
            v.p[6 + k] *= v.p[13];
            const float inc = phaseInc(kWinNotes[k], v.dtl);
            advance(v.ph[2 * k], inc);
            advance(v.ph[2 * k + 1], inc);
        }
        const float padEnv = smoothstep(0.0f, 0.6f, t) * (1.0f - smoothstep(1.6f, 2.65f, t));
        float pad = 0.0f;
        for (size_t k = 0; k < 3; ++k) {
            const float ph = v.ph[12 + k];
            pad += sinPh(ph) + 0.2f * sinPh(2.0f * ph);
            advance(v.ph[12 + k], phaseInc(kWinPad[k], v.dtl));
        }
        out[i] = (s + pad * padEnv * 0.045f) * (1.0f - smoothstep(2.0f, 2.68f, t));
    }
}

// Перемещение по меню: мягкий тик.
void initMenuMove(Voice& v) { v.dur = 0.07f; }

void renderMenuMove(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        out[i] = sinPh(v.ph[0]) * envAD(t, 0.001f, 0.01f) * 0.11f +
                 sinPh(v.ph[1]) * envAD(t, 0.0005f, 0.004f) * 0.04f;
        advance(v.ph[0], phaseInc(1450.0f, v.dtl));
        advance(v.ph[1], phaseInc(2900.0f, v.dtl));
    }
}

// Подтверждение: два быстрых треугольных тона вверх (Ля - Ми).
void initMenuSelect(Voice& v) { v.dur = 0.3f; }

void renderMenuSelect(Voice& v, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        const float t = v.at(i);
        const bool first = t < 0.06f;
        const float env = first ? envAD(t, 0.002f, 0.05f) : envAD(t - 0.06f, 0.002f, 0.06f);
        out[i] = triWave(v.ph[0]) * env * 0.15f;
        advance(v.ph[0], phaseInc(first ? 880.0f : 1318.5f, v.dtl));
    }
}

// ---------------------------------------------------------------------------
//  Диспетчеризация по типу эффекта
// ---------------------------------------------------------------------------
bool isConsoleSfx(Sfx s) {
    switch (s) {
    case Sfx::ConsolePickupFuel:
    case Sfx::ConsolePickupItem:
    case Sfx::ConsoleCrash:
    case Sfx::ConsoleSkid:
    case Sfx::ConsoleBoot:
    case Sfx::ConsoleBlip: return true;
    default: return false;
    }
}

// arg — pitch из play() (для «бубнежа» это длительность реплики).
void initSfx(Voice& v, float arg) {
    switch (v.type) {
    case Sfx::ConsolePickupFuel: initPickupFuel(v); break;
    case Sfx::ConsolePickupItem: initPickupItem(v); break;
    case Sfx::ConsoleCrash: initCrash(v); break;
    case Sfx::ConsoleSkid: initSkid(v); break;
    case Sfx::ConsoleBoot: initBoot(v); break;
    case Sfx::ConsoleBlip: initBlip(v); break;
    case Sfx::RoofThud: initRoofThud(v); break;
    case Sfx::RoofStep: initRoofStep(v); break;
    case Sfx::RoofScrape: initRoofScrape(v); break;
    case Sfx::GlassKnock: initGlassKnock(v); break;
    case Sfx::GlassBang: initGlassBang(v); break;
    case Sfx::GlassCrack: initGlassCrack(v); break;
    case Sfx::GlassShatter: initGlassShatter(v); break;
    case Sfx::MonsterGrowl: initGrowl(v); break;
    case Sfx::MonsterScreech: initScreech(v); break;
    case Sfx::CameraShutter: initShutter(v); break;
    case Sfx::CameraReady: initCameraReady(v); break;
    case Sfx::CameraEmpty: initCameraEmpty(v); break;
    case Sfx::EngineSputter: initSputter(v); break;
    case Sfx::EngineDie: initEngineDie(v); break;
    case Sfx::StreetlightWhoosh: initWhoosh(v); break;
    case Sfx::MumbleDad: initMumble(v, true, arg); break;
    case Sfx::MumbleMom: initMumble(v, false, arg); break;
    case Sfx::Jumpscare: initJumpscare(v); break;
    case Sfx::LoseSting: initLoseSting(v); break;
    case Sfx::WinJingle: initWinJingle(v); break;
    case Sfx::MenuMove: initMenuMove(v); break;
    case Sfx::MenuSelect: initMenuSelect(v); break;
    case Sfx::Count: v.dur = 0.0f; break;
    }
}

void renderSfx(Voice& v, float* out, int n) {
    switch (v.type) {
    case Sfx::ConsolePickupFuel: renderPickupFuel(v, out, n); break;
    case Sfx::ConsolePickupItem: renderPickupItem(v, out, n); break;
    case Sfx::ConsoleCrash: renderCrash(v, out, n); break;
    case Sfx::ConsoleSkid: renderSkid(v, out, n); break;
    case Sfx::ConsoleBoot: renderBoot(v, out, n); break;
    case Sfx::ConsoleBlip: renderBlip(v, out, n); break;
    case Sfx::RoofThud: renderRoofThud(v, out, n); break;
    case Sfx::RoofStep: renderRoofStep(v, out, n); break;
    case Sfx::RoofScrape: renderRoofScrape(v, out, n); break;
    case Sfx::GlassKnock: renderGlassKnock(v, out, n); break;
    case Sfx::GlassBang: renderGlassBang(v, out, n); break;
    case Sfx::GlassCrack: renderGlassCrack(v, out, n); break;
    case Sfx::GlassShatter: renderGlassShatter(v, out, n); break;
    case Sfx::MonsterGrowl: renderGrowl(v, out, n); break;
    case Sfx::MonsterScreech: renderScreech(v, out, n); break;
    case Sfx::CameraShutter: renderShutter(v, out, n); break;
    case Sfx::CameraReady: renderCameraReady(v, out, n); break;
    case Sfx::CameraEmpty: renderCameraEmpty(v, out, n); break;
    case Sfx::EngineSputter: renderSputter(v, out, n); break;
    case Sfx::EngineDie: renderEngineDie(v, out, n); break;
    case Sfx::StreetlightWhoosh: renderWhoosh(v, out, n); break;
    case Sfx::MumbleDad:
    case Sfx::MumbleMom: renderMumble(v, out, n); break;
    case Sfx::Jumpscare: renderJumpscare(v, out, n); break;
    case Sfx::LoseSting: renderLoseSting(v, out, n); break;
    case Sfx::WinJingle: renderWinJingle(v, out, n); break;
    case Sfx::MenuMove: renderMenuMove(v, out, n); break;
    case Sfx::MenuSelect: renderMenuSelect(v, out, n); break;
    case Sfx::Count: std::fill(out, out + n, 0.0f); break;
    }
}

// ===========================================================================
//  Мелодия консоли: оригинальная тема в ля миноре, 16 тактов по 16 шагов
//  (шестнадцатые). Ведущий голос задан нотами, арпеджио, бас и ударные
//  строятся по таблице аккордов.
// ===========================================================================
constexpr int kStepsPerBar = 16;
constexpr int kSongBars = 16;
constexpr int kSongSteps = kStepsPerBar * kSongBars;
constexpr float kSongBpm = 118.0f;

struct NoteEv {
    int8_t note; // MIDI-нота, 0 — пауза
    int8_t len;  // длительность в шестнадцатых
};

// Ведущая партия (первый квадратный канал).
constexpr NoteEv kLead[] = {
    // Часть A: Am | F | C | G | Am | F | G | E
    {76, 3}, {76, 1}, {74, 2}, {72, 2}, {71, 2}, {72, 2}, {69, 4},
    {69, 2}, {72, 2}, {77, 4}, {76, 2}, {74, 2}, {72, 4},
    {67, 2}, {72, 2}, {76, 2}, {79, 4}, {77, 2}, {76, 2}, {74, 2},
    {74, 4}, {71, 2}, {67, 2}, {71, 2}, {74, 4}, {0, 2},
    {76, 3}, {76, 1}, {74, 2}, {72, 2}, {71, 2}, {72, 2}, {76, 4},
    {77, 2}, {76, 2}, {77, 2}, {81, 4}, {79, 2}, {77, 2}, {76, 2},
    {74, 2}, {76, 2}, {74, 2}, {71, 4}, {67, 2}, {71, 2}, {74, 2},
    {68, 4}, {71, 4}, {76, 6}, {0, 2},
    // Часть B: Dm | Am | F | C | Dm | Am | E | E
    {77, 2}, {0, 2}, {77, 2}, {76, 2}, {74, 4}, {69, 4},
    {72, 2}, {0, 2}, {72, 2}, {71, 2}, {69, 4}, {64, 4},
    {69, 2}, {72, 2}, {77, 2}, {81, 2}, {79, 4}, {77, 2}, {76, 2},
    {76, 4}, {74, 2}, {72, 2}, {67, 4}, {0, 4},
    {74, 2}, {77, 2}, {81, 4}, {79, 2}, {77, 2}, {76, 2}, {74, 2},
    {72, 2}, {76, 2}, {81, 4}, {79, 2}, {76, 2}, {72, 2}, {76, 2},
    {71, 2}, {68, 2}, {71, 2}, {76, 4}, {74, 2}, {71, 2}, {68, 2},
    {76, 2}, {74, 2}, {72, 2}, {71, 2}, {68, 2}, {69, 2}, {71, 2}, {74, 2},
};
constexpr int kLeadCount = static_cast<int>(sizeof(kLead) / sizeof(kLead[0]));

constexpr int totalLength(const NoteEv* ev, int count) {
    int sum = 0;
    for (int i = 0; i < count; ++i) sum += ev[i].len;
    return sum;
}
static_assert(totalLength(kLead, kLeadCount) == kSongSteps, "lead part must fill the whole song");

// Аккорд такта: корень арпеджио (3-я октава), минор ли он, нота баса.
struct ChordDef {
    int8_t root;
    int8_t minor;
    int8_t bass;
};
constexpr ChordDef kChords[kSongBars] = {
    {57, 1, 45}, {53, 0, 41}, {60, 0, 48}, {55, 0, 43}, // Am F C G
    {57, 1, 45}, {53, 0, 41}, {55, 0, 43}, {52, 0, 40}, // Am F G E
    {50, 1, 38}, {57, 1, 45}, {53, 0, 41}, {60, 0, 48}, // Dm Am F C
    {50, 1, 38}, {57, 1, 45}, {52, 0, 40}, {52, 0, 40}, // Dm Am E E
};
// Бас восьмыми: прима и октава, на седьмой восьмой — квинта.
constexpr int kBassPattern[8] = {0, 12, 0, 12, 0, 12, 7, 12};

// Тактовая сетка и каналы мелодии.
struct Music {
    std::array<int8_t, kSongSteps> leadNote{}; // -1 нет события, 0 пауза, иначе нота
    std::array<int8_t, kSongSteps> leadLen{};
    int step = kSongSteps - 1; // первый же шаг станет 0
    float stepClock = 1.0f;    // доля текущего шага; >= 1 — начать следующий

    // Ведущий (квадрат 50%/25%), арпеджио (квадрат 12.5%), бас (4-битный треугольник).
    float leadPh = 0.0f, leadHz = 440.0f, leadGate = 0.0f, leadDec = 0.0f, leadAmp = 0.0f, leadT = 0.0f;
    float leadDuty = 0.5f, vibPh = 0.0f;
    float arpPh = 0.0f, arpHz = 220.0f, arpGate = 0.0f, arpDec = 0.0f, arpAmp = 0.0f;
    float bassPh = 0.0f, bassHz = 110.0f, bassGate = 0.0f, bassAmp = 0.0f;
    // Шумовой канал (хэт / малый барабан).
    Lfsr noise;
    float noiseClock = 0.0f, noiseLevel = 0.0f, noiseMul = 0.0f;
    // Коэффициенты огибающих за кадр (см. setRate).
    float atk = 0.0f, rel = 0.0f, leadDecMul = 0.0f, arpDecMul = 0.0f;

    Music() {
        leadNote.fill(-1);
        int pos = 0;
        for (const NoteEv& e : kLead) {
            if (pos < kSongSteps) {
                leadNote[static_cast<size_t>(pos)] = e.note;
                leadLen[static_cast<size_t>(pos)] = e.len;
            }
            pos += e.len;
        }
    }

    void setRate(float dt) {
        atk = 1.0f - std::exp(-dt / 0.0015f);
        rel = 1.0f - std::exp(-dt / 0.02f);
        leadDecMul = std::exp(-dt / 0.09f);
        arpDecMul = std::exp(-dt / 0.05f);
    }

    void hit(float clockHz, float level, float tau, float dt) {
        noiseClock = clockHz;
        noiseLevel = level;
        noiseMul = std::exp(-dt / tau);
    }

    // Начало шага s: новые ноты всех каналов.
    void onStep(int s, float dt) {
        const int bar = s / kStepsPerBar;
        const int pos = s % kStepsPerBar;
        const ChordDef& ch = kChords[bar];
        const int8_t ln = leadNote[static_cast<size_t>(s)];
        if (ln > 0) {
            leadHz = midiHz(ln);
            leadGate = fi(leadLen[static_cast<size_t>(s)]) - 0.2f;
            leadDec = 1.0f;
            leadT = 0.0f;
            leadDuty = bar < 8 ? 0.5f : 0.25f;
        } else if (ln == 0) {
            leadGate = 0.0f;
        }
        const int tones[4] = {0, ch.minor ? 3 : 4, 7, 12};
        arpHz = midiHz(ch.root + tones[pos % 4]);
        arpGate = 0.7f;
        arpDec = 1.0f;
        if (pos % 2 == 0) {
            bassHz = midiHz(ch.bass + kBassPattern[pos / 2]);
            bassGate = 1.5f;
        }
        const bool fill = (bar == 7 || bar == 15) && pos >= 12;
        if (fill || (bar >= 8 && (pos == 4 || pos == 12))) {
            hit(5500.0f, fill ? 0.8f : 1.0f, 0.09f, dt); // малый барабан
        } else if (pos % 2 == 0) {
            hit(22000.0f, pos % 4 == 2 ? 1.0f : 0.55f, 0.02f, dt); // хэт
        }
    }

    // Один кадр мелодии (моно, без учёта громкости слоя).
    float tick(float dt, float stepsPerSec) {
        const float dSteps = stepsPerSec * dt;
        stepClock += dSteps;
        if (stepClock >= 1.0f) {
            stepClock -= std::floor(stepClock);
            step = (step + 1) % kSongSteps;
            onStep(step, dt);
        }
        leadGate -= dSteps;
        arpGate -= dSteps;
        bassGate -= dSteps;
        // Огибающие: быстрая атака, мягкое отпускание (без щелчков между нотами).
        const float leadTarget = leadGate > 0.0f ? 0.62f + 0.38f * leadDec : 0.0f;
        leadAmp += (leadTarget - leadAmp) * (leadTarget > leadAmp ? atk : rel);
        leadDec *= leadDecMul;
        const float arpTarget = arpGate > 0.0f ? arpDec : 0.0f;
        arpAmp += (arpTarget - arpAmp) * (arpTarget > arpAmp ? atk : rel);
        arpDec *= arpDecMul;
        const float bassTarget = bassGate > 0.0f ? 1.0f : 0.0f;
        bassAmp += (bassTarget - bassAmp) * (bassTarget > bassAmp ? atk : rel);
        // Ведущий голос: отложенное вибрато на длинных нотах.
        leadT += dt;
        const float vib = leadT > 0.16f ? 1.0f + 0.007f * sinPh(vibPh) : 1.0f;
        advance(vibPh, phaseInc(5.5f, dt));
        const float leadInc = phaseInc(leadHz * vib, dt);
        const float lead = pulseWave(leadPh, leadInc, leadDuty) * leadAmp;
        advance(leadPh, leadInc);
        const float arpInc = phaseInc(arpHz, dt);
        const float arp = pulseWave(arpPh, arpInc, 0.125f) * arpAmp;
        advance(arpPh, arpInc);
        // Бас: треугольник, квантованный до 16 уровней (как волновой канал консоли).
        const float bass = std::floor(triWave(bassPh) * 7.5f + 0.5f) / 7.5f * bassAmp;
        advance(bassPh, phaseInc(bassHz, dt));
        const float nz = noise.tick(noiseClock * dt, false) * noiseLevel;
        noiseLevel *= noiseMul;
        return lead * 0.085f + arp * 0.03f + bass * 0.12f + nz * 0.035f;
    }
};

// ---------------------------------------------------------------------------
//  Фоновые слои
// ---------------------------------------------------------------------------

// Удар сердца: синус с быстро падающей частотой + негармонический обертон,
// чтобы стук был слышен и на маленьких динамиках.
struct Thump {
    float t = -1.0f; // время с начала удара; < 0 — тишина
    float ph = 0.0f, ph2 = 0.0f, amp = 0.0f, hz = 55.0f;
    void start(float a, float f) {
        t = 0.0f;
        ph = 0.0f;
        ph2 = 0.0f;
        amp = a;
        hz = f;
    }
    float tick(float dt) {
        if (t < 0.0f) return 0.0f;
        const float f = hz * (0.72f + 0.45f * std::exp(-t / 0.03f));
        const float s = (sinPh(ph) + 0.35f * sinPh(ph2)) * envAD(t, 0.006f, 0.065f) * amp;
        advance(ph, phaseInc(f, dt));
        advance(ph2, phaseInc(f * 2.6f, dt));
        t += dt;
        if (t > 0.5f) t = -1.0f;
        return s;
    }
};

// Капля дождя: короткий звон с собственной панорамой.
struct Drop {
    Ping ping;
    float gl = 0.7f, gr = 0.7f;
};

// Частоты тревожного кластера (малые секунды): си-бемоль, си, до.
constexpr float kCluster[3] = {233.08f, 246.94f, 261.63f};

} // namespace

// ===========================================================================
//  Состояние синтезатора
// ===========================================================================
struct Synth::State {
    explicit State(int sampleRateHz);

    void trigger(Sfx s, float volume, float pan, float pitch);
    void stopAll();
    void render(float* out, int frames);
    Voice& allocVoice();
    void startFade(Voice& v);
    void renderVoice(Voice& v, int n);
    void updateBlockControls(int n);
    void mixBlock(float* out, int n);
    void smoothParams();
    void resetDsp();

    int rate = cfg::kSampleRate;
    float sr = static_cast<float>(cfg::kSampleRate);
    float dt = 1.0f / static_cast<float>(cfg::kSampleRate);
    AmbientParams target; // заданные игрой значения слоёв
    Rng rng{0xA0D10u};
    uint32_t serial = 0;
    std::array<Voice, kPoolSize> voices{};
    // Буферы блока: одиночный голос и шины (эффекты / консоль), L и R.
    float voiceBuf[kBlock] = {};
    float sfxL[kBlock] = {}, sfxR[kBlock] = {}, conL[kBlock] = {}, conR[kBlock] = {};

    AmbientParams cur;      // сглаженные значения
    float paramK = 0.0f;    // коэффициент сглаживания за кадр
    Music music;

    // Двигатель.
    float engPh = 0.0f, engPh2 = 0.0f, engSubPh = 0.0f, engJit = 1.0f, engJitTarget = 1.0f;
    float engWobble = 0.0f;
    Wander engWob;
    Svf engLp;
    // Дорога: независимый «коричневый» шум в каждом канале + шипение шин + стыки.
    float brownL = 0.0f, brownR = 0.0f, roadMod = 1.0f;
    Svf roadLpL, roadLpR;
    OnePole hissHp, hissLp;
    Wander roadAmp;
    float bumpIn = 1.5f, bumpNoise = 0.0f, bumpMul = 0.0f;
    Ping bump;
    OnePole bumpLp;
    // Ветер.
    Svf windBp, whistleBp;
    Wander windCentre, windGust, windPan;
    float windGain = 0.0f, windWhistle = 0.0f, windGl = 1.0f, windGr = 1.0f;
    // Дождь.
    OnePole rainHpL1, rainHpL2, rainHpR1, rainHpR2, rainLpL, rainLpR;
    Drop drops[8];
    // Сердце.
    float hbClock = 0.0f, dubAt = 0.0f;
    bool dubPending = false;
    Thump lub, dub;
    // Тревожный дрон.
    float drPhA = 0.0f, drPhB = 0.0f, drSawPh = 0.0f, breathPh = 0.0f, breathEnv = 0.0f;
    float clPh[3] = {}, clGain[3] = {};
    Wander clTrem[3];
    Svf droneLp, breathBp;
    // Мастер: приглушение консоли, подавление постоянной составляющей.
    Svf mufL, mufR;
    float conGain = 1.0f;
    float dcR = 0.999f;
    float dcXL = 0.0f, dcYL = 0.0f, dcXR = 0.0f, dcYR = 0.0f;
};

Synth::State::State(int sampleRateHz) {
    rate = clampi(sampleRateHz, 8000, 192000);
    sr = static_cast<float>(rate);
    dt = 1.0f / sr;
    paramK = 1.0f - std::exp(-dt / kParamTau);
    music.setRate(dt);
    dcR = 1.0f - kTau * 5.0f / sr; // ~5 Гц
    target.master = 1.0f;
    cur = target;
    cur.engine = cur.road = cur.wind = cur.rain = cur.heartbeat = cur.dread = cur.consoleMusic = 0.0f;
    roadLpL.tune(420.0f * dt, 0.7f);
    roadLpR.tune(420.0f * dt, 0.7f);
    hissHp.tune(900.0f * dt);
    hissLp.tune(3000.0f * dt);
    bumpLp.tune(300.0f * dt);
    bumpMul = std::exp(-dt / 0.05f);
    rainHpL1.tune(2200.0f * dt);
    rainHpL2.tune(2200.0f * dt);
    rainHpR1.tune(2200.0f * dt);
    rainHpR2.tune(2200.0f * dt);
    rainLpL.tune(7000.0f * dt);
    rainLpR.tune(7000.0f * dt);
    droneLp.tune(140.0f * dt, 0.8f);
    breathBp.tune(380.0f * dt, 1.6f);
    engLp.tune(250.0f * dt, 0.8f);
    windBp.tune(650.0f * dt, 1.3f);
    whistleBp.tune(1700.0f * dt, 7.0f);
    mufL.tune(18000.0f * dt, 0.707f);
    mufR.tune(18000.0f * dt, 0.707f);
}

// ---------------------------------------------------------------------------
//  Голоса
// ---------------------------------------------------------------------------
void Synth::State::startFade(Voice& v) {
    if (v.fading) return;
    v.fading = true;
    v.fadeStep = 1.0f / std::max(1.0f, kStealFade * sr);
}

Voice& Synth::State::allocVoice() {
    int freeIdx = -1, live = 0;
    for (int i = 0; i < kPoolSize; ++i) {
        const Voice& v = voices[static_cast<size_t>(i)];
        if (!v.active) {
            if (freeIdx < 0) freeIdx = i;
        } else if (!v.fading) {
            ++live;
        }
    }
    // Лимит живых голосов: самый тихий (с поправкой на остаток длительности
    // и возраст) начинает быстро затухать в своём слоте.
    if (live >= kMaxVoices) {
        Voice* victim = nullptr;
        float best = 0.0f;
        for (Voice& v : voices) {
            if (!v.active || v.fading) continue;
            const float remain = (v.dur - v.t) / std::max(v.dtl * sr, 1e-3f);
            const float score = v.level * (0.5f + 0.5f * saturate(remain / 0.5f));
            if (!victim || score < best || (score == best && v.serial < victim->serial)) {
                victim = &v;
                best = score;
            }
        }
        if (victim) startFade(*victim);
    }
    if (freeIdx >= 0) return voices[static_cast<size_t>(freeIdx)];
    // Свободных слотов нет: заменяем самый тихий из уже затухающих.
    Voice* quiet = &voices[0];
    for (Voice& v : voices)
        if (v.fading && v.level * v.fade < quiet->level * quiet->fade) quiet = &v;
    return *quiet;
}

void Synth::State::trigger(Sfx s, float volume, float pan, float pitch) {
    const int id = static_cast<int>(s);
    if (id < 0 || id >= static_cast<int>(Sfx::Count)) return;
    const float vol = sanitize(volume, 0.0f, 1.5f, 0.0f);
    if (vol < 1e-4f) return;
    const float pn = sanitize(pan, -1.0f, 1.0f, 0.0f) * kPanWidth;
    const bool mumble = s == Sfx::MumbleDad || s == Sfx::MumbleMom;
    const float speed = mumble ? 1.0f : sanitize(pitch, 0.25f, 4.0f, 1.0f);

    Voice& v = allocVoice();
    v = Voice{};
    v.active = true;
    v.type = s;
    v.console = isConsoleSfx(s);
    v.serial = ++serial;
    v.rng.reseed(rng.nextU32() | 1u);
    v.dtl = speed * dt;
    v.endFade = kEndFade * speed;
    v.vol = vol;
    // Равномощностная панорама, центр = 1.0 в каждом канале.
    const float angle = (pn + 1.0f) * 0.25f * kPi;
    v.gainL = std::cos(angle) * kSqrt2;
    v.gainR = std::sin(angle) * kSqrt2;
    v.level = 1.0f; // новый голос защищён от немедленного вытеснения
    initSfx(v, pitch);
    if (v.dur <= 0.0f) v.active = false;
}

void Synth::State::stopAll() {
    for (Voice& v : voices)
        if (v.active) startFade(v);
}

void Synth::State::renderVoice(Voice& v, int n) {
    float* buf = voiceBuf;
    renderSfx(v, buf, n);
    float* bl = v.console ? conL : sfxL;
    float* br = v.console ? conR : sfxR;
    float peak = 0.0f;
    for (int i = 0; i < n; ++i) {
        float g = v.vol;
        // Финальное затухание (приподнятый косинус): голос всегда кончается ровно в ноль.
        const float remain = v.dur - v.at(i);
        if (remain < v.endFade) g *= smoothstep(0.0f, v.endFade, remain);
        if (v.fading) {
            g *= v.fade;
            v.fade = std::max(0.0f, v.fade - v.fadeStep);
        }
        const float s = buf[i] * g;
        peak = std::max(peak, std::fabs(s));
        bl[i] += s * v.gainL;
        br[i] += s * v.gainR;
    }
    v.t += v.dtl * fi(n);
    v.flushFilters();
    v.level = std::max(peak, v.level * 0.9f);
    if (v.t >= v.dur || (v.fading && v.fade <= 0.0f)) v.active = false;
}

// ---------------------------------------------------------------------------
//  Фон и мастер
// ---------------------------------------------------------------------------
void Synth::State::smoothParams() {
    const float k = paramK;
    cur.engine += (target.engine - cur.engine) * k;
    cur.engineRpm += (target.engineRpm - cur.engineRpm) * k;
    cur.road += (target.road - cur.road) * k;
    cur.wind += (target.wind - cur.wind) * k;
    cur.rain += (target.rain - cur.rain) * k;
    cur.heartbeat += (target.heartbeat - cur.heartbeat) * k;
    cur.dread += (target.dread - cur.dread) * k;
    cur.consoleMusic += (target.consoleMusic - cur.consoleMusic) * k;
    cur.consoleMuffle += (target.consoleMuffle - cur.consoleMuffle) * k;
    cur.musicTempo += (target.musicTempo - cur.musicTempo) * k;
    cur.master += (target.master - cur.master) * k;
}

// Медленные управляющие сигналы: раз в блок (перестройка фильтров, блуждания).
void Synth::State::updateBlockControls(int n) {
    const float bdt = dt * fi(n);
    // Почти достигнутые цели — точно в цель (без бесконечных хвостов).
    auto snap = [](float& c, float t) {
        if (std::fabs(c - t) < 1e-5f) c = t;
    };
    snap(cur.engine, target.engine);
    snap(cur.engineRpm, target.engineRpm);
    snap(cur.road, target.road);
    snap(cur.wind, target.wind);
    snap(cur.rain, target.rain);
    snap(cur.heartbeat, target.heartbeat);
    snap(cur.dread, target.dread);
    snap(cur.consoleMusic, target.consoleMusic);
    snap(cur.consoleMuffle, target.consoleMuffle);
    snap(cur.musicTempo, target.musicTempo);
    snap(cur.master, target.master);

    engWobble = engWob.next(rng, 2.5f * bdt);
    engLp.tune((170.0f + 200.0f * cur.engineRpm) * dt, 0.8f);
    roadMod = 0.85f + 0.15f * roadAmp.next(rng, 0.7f * bdt);

    const float centre = 650.0f * std::pow(2.0f, 1.1f * windCentre.next(rng, 0.12f * bdt));
    windBp.tune(centre * dt, 1.3f);
    whistleBp.tune(centre * 2.6f * dt, 7.0f);
    // Порывы: громкость шума; тонкий свист в щелях появляется только в сильные порывы.
    const float gust = windGust.next(rng, 0.2f * bdt);
    windGain = 0.65f + 0.35f * gust;
    windWhistle = 0.15f * smoothstep(0.3f, 0.9f, gust);
    const float wAngle = (0.45f * windPan.next(rng, 0.07f * bdt) + 1.0f) * 0.25f * kPi;
    windGl = std::cos(wAngle) * kSqrt2;
    windGr = std::sin(wAngle) * kSqrt2;

    for (int k = 0; k < 3; ++k) clGain[k] = 0.5f + 0.5f * clTrem[k].next(rng, (0.08f + 0.03f * fi(k)) * bdt);
    // Дыхание: вдох (ярче), выдох (ниже), пауза; цикл ~5.2 с.
    breathPh += bdt / 5.2f;
    if (breathPh >= 1.0f) breathPh -= 1.0f;
    if (breathPh < 0.4f) {
        const float s = std::sin(kPi * breathPh / 0.4f);
        breathEnv = 0.6f * s * s;
        breathBp.tune(520.0f * dt, 1.6f);
    } else if (breathPh >= 0.45f && breathPh < 0.95f) {
        const float s = std::sin(kPi * (breathPh - 0.45f) / 0.5f);
        breathEnv = s * s;
        breathBp.tune(330.0f * dt, 1.6f);
    } else {
        breathEnv = 0.0f;
    }

    // Приглушение консоли: от полной полосы до ~1.2 кГц и 45% громкости.
    const float m = saturate(cur.consoleMuffle);
    const float cutoff = std::exp(lerpf(std::log(18000.0f), std::log(1200.0f), m));
    mufL.tune(cutoff * dt, 0.707f);
    mufR.tune(cutoff * dt, 0.707f);
    conGain = lerpf(1.0f, 0.45f, m);
}

void Synth::State::mixBlock(float* out, int n) {
    for (int i = 0; i < n; ++i) {
        smoothParams();
        float l = sfxL[i], r = sfxR[i];
        float cl = conL[i], cr = conR[i];

        // --- Мелодия консоли ---
        const float tempo = clampf(cur.musicTempo, 0.5f, 2.0f);
        const float mel = music.tick(dt, kSongBpm / 60.0f * 4.0f * tempo) * cur.consoleMusic;
        cl += mel;
        cr += mel;

        // --- Двигатель: импульс + пила + субгармоника, нерегулярность вспышек ---
        if (cur.engine > 1e-4f) {
            const float f = (32.0f + 23.0f * cur.engineRpm) * (1.0f + 0.012f * engWobble);
            const float inc = phaseInc(f, dt);
            const float inc2 = phaseInc(f * 1.003f, dt);
            float src = pulseWave(engPh, inc, 0.3f) * 0.45f + sawWave(engPh2, inc2) * 0.4f +
                        sinPh(engSubPh) * 0.35f;
            engPh += inc;
            if (engPh >= 1.0f) {
                engPh -= 1.0f;
                engJitTarget = 1.0f + 0.18f * rng.signedUnit();
            }
            advance(engPh2, inc2);
            advance(engSubPh, inc * 0.5f);
            engJit += (engJitTarget - engJit) * 0.01f;
            src *= engJit;
            const float e = engLp.lp(src) * cur.engine * 0.165f;
            l += e;
            r += e;
        }

        // --- Дорога ---
        if (cur.road > 1e-4f) {
            brownL = brownL * 0.997f + rng.signedUnit() * 0.06f;
            brownR = brownR * 0.997f + rng.signedUnit() * 0.06f;
            const float hiss = hissLp.lp(hissHp.hp(rng.signedUnit())) * 0.05f;
            bumpIn -= dt;
            if (bumpIn <= 0.0f) {
                bumpIn = rng.range(0.7f, 4.0f);
                bump.start(rng.range(48.0f, 75.0f), rng.range(0.08f, 0.2f), 0.07f, dt);
                bumpNoise = rng.range(0.5f, 1.0f);
            }
            const float bmp = bump.tick() + bumpLp.lp(rng.signedUnit()) * bumpNoise * 0.6f;
            bumpNoise *= bumpMul;
            const float g = cur.road * roadMod * 0.5f;
            l += (roadLpL.lp(brownL) * 0.32f + hiss + bmp) * g;
            r += (roadLpR.lp(brownR) * 0.32f + hiss + bmp) * g;
        }

        // --- Ветер ---
        if (cur.wind > 1e-4f) {
            const float wn = rng.signedUnit();
            const float w = (windBp.bp(wn) + whistleBp.bp(wn) * windWhistle) * windGain * cur.wind * 0.4f;
            l += w * windGl;
            r += w * windGr;
        }

        // --- Дождь: шелест + редкие звонкие капли по крыше ---
        if (cur.rain > 1e-4f) {
            const float bedL = rainLpL.lp(rainHpL2.hp(rainHpL1.hp(rng.signedUnit())));
            const float bedR = rainLpR.lp(rainHpR2.hp(rainHpR1.hp(rng.signedUnit())));
            l += bedL * cur.rain * 0.06f;
            r += bedR * cur.rain * 0.06f;
            if (rng.next01() < 28.0f * cur.rain * dt) {
                Drop* d = &drops[0];
                for (Drop& dd : drops)
                    if (dd.ping.amp < d->ping.amp) d = &dd;
                const bool metal = rng.chance(0.3f);
                const float f = metal ? rng.range(350.0f, 700.0f) : 1800.0f * std::pow(3.0f, rng.next01());
                const float tau = metal ? 0.008f : rng.range(0.002f, 0.006f);
                d->ping.start(f, rng.range(0.015f, 0.05f) * cur.rain, tau, dt);
                const float a = (rng.signedUnit() + 1.0f) * 0.25f * kPi;
                d->gl = std::cos(a) * kSqrt2;
                d->gr = std::sin(a) * kSqrt2;
            }
            for (Drop& d : drops) {
                const float s = d.ping.tick();
                l += s * d.gl;
                r += s * d.gr;
            }
        }

        // --- Сердцебиение: «тук-тук», 60 -> 140 уд/мин ---
        if (cur.heartbeat > 1e-3f) {
            const float hb = saturate(cur.heartbeat);
            const float period = 60.0f / (60.0f + 80.0f * hb);
            hbClock += dt;
            if (hbClock >= period) {
                hbClock -= period;
                if (hbClock > period) hbClock = 0.0f;
                const float a = 0.3f * std::pow(hb, 0.8f);
                lub.start(a, 58.0f);
                dubPending = true;
                dubAt = std::min(0.32f, 0.36f * period);
            }
            if (dubPending && hbClock >= dubAt) {
                dub.start(0.7f * 0.3f * std::pow(hb, 0.8f), 66.0f);
                dubPending = false;
            }
        } else {
            dubPending = false;
        }
        const float heart = lub.tick(dt) + dub.tick(dt);
        l += heart;
        r += heart;

        // --- Тревожный дрон: биения 41/43.5 Гц, кластер малых секунд, дыхание ---
        if (cur.dread > 1e-4f) {
            const float a = sinPh(drPhA), b = sinPh(drPhB);
            advance(drPhA, phaseInc(41.0f, dt));
            advance(drPhB, phaseInc(43.5f, dt));
            const float sawInc = phaseInc(41.2f, dt);
            const float grit = droneLp.lp(sawWave(drSawPh, sawInc)) * 0.05f;
            advance(drSawPh, sawInc);
            float cL = 0.0f, cR = 0.0f;
            for (int k = 0; k < 3; ++k) {
                const float s = sinPh(clPh[k]) * clGain[k] * 0.012f;
                advance(clPh[k], phaseInc(kCluster[k], dt));
                cL += s * (1.2f - 0.4f * fi(k)); // кластер разнесён по панораме
                cR += s * (0.4f + 0.4f * fi(k));
            }
            const float breath = breathBp.bp(rng.signedUnit()) * breathEnv * 0.5f;
            const float d = cur.dread;
            l += d * ((0.7f * a + 0.3f * b) * 0.09f + grit + cL + breath);
            r += d * ((0.3f * a + 0.7f * b) * 0.09f + grit + cR + breath);
        }

        // --- Мастер ---
        l += mufL.lp(cl) * conGain;
        r += mufR.lp(cr) * conGain;
        // Подавление постоянной составляющей: y = x - x1 + R*y1.
        const float yl = l - dcXL + dcR * dcYL;
        const float yr = r - dcXR + dcR * dcYR;
        dcXL = l;
        dcXR = r;
        dcYL = yl;
        dcYR = yr;
        float ol = softClip(yl) * cur.master;
        float orr = softClip(yr) * cur.master;
        // Защита: при сбое (NaN/inf) — тишина и сброс фильтров.
        if (!(std::fabs(ol) <= 1.0f) || !(std::fabs(orr) <= 1.0f)) {
            ol = 0.0f;
            orr = 0.0f;
            resetDsp();
        }
        out[2 * i] = ol;
        out[2 * i + 1] = orr;
    }
}

// Аварийный сброс состояний фильтров (только если где-то появился NaN).
void Synth::State::resetDsp() {
    for (Voice& v : voices) v.active = false;
    for (Svf* f : {&engLp, &roadLpL, &roadLpR, &windBp, &whistleBp, &droneLp, &breathBp, &mufL, &mufR})
        f->clear();
    hissHp.z = hissLp.z = bumpLp.z = 0.0f;
    rainHpL1.z = rainHpL2.z = rainHpR1.z = rainHpR2.z = rainLpL.z = rainLpR.z = 0.0f;
    brownL = brownR = 0.0f;
    dcXL = dcYL = dcXR = dcYR = 0.0f;
    bump = Ping{};
    for (Drop& d : drops) d.ping = Ping{};
    lub = Thump{};
    dub = Thump{};
}

void Synth::State::render(float* out, int frames) {
    if (!out || frames <= 0) return;
    int done = 0;
    while (done < frames) {
        const int n = std::min(kBlock, frames - done);
        std::fill(sfxL, sfxL + n, 0.0f);
        std::fill(sfxR, sfxR + n, 0.0f);
        std::fill(conL, conL + n, 0.0f);
        std::fill(conR, conR + n, 0.0f);
        updateBlockControls(n);
        for (Voice& v : voices)
            if (v.active) renderVoice(v, n);
        mixBlock(out + 2 * done, n);
        // Хвосты фильтров, которые могли «затихнуть» до денормалов.
        mufL.flush();
        mufR.flush();
        flushTiny(dcYL);
        flushTiny(dcYR);
        done += n;
    }
}

// ===========================================================================
//  Публичный интерфейс
// ===========================================================================
Synth::Synth(int sampleRate) : st_(std::make_unique<State>(sampleRate)) {}
Synth::~Synth() = default;

void Synth::trigger(Sfx s, float volume, float pan, float pitch) { st_->trigger(s, volume, pan, pitch); }

void Synth::setAmbient(const AmbientParams& p) {
    AmbientParams& t = st_->target;
    t.engine = sanitize(p.engine, 0.0f, 1.0f, 0.0f);
    t.engineRpm = sanitize(p.engineRpm, 0.0f, 1.0f, 0.5f);
    t.road = sanitize(p.road, 0.0f, 1.0f, 0.0f);
    t.wind = sanitize(p.wind, 0.0f, 1.0f, 0.0f);
    t.rain = sanitize(p.rain, 0.0f, 1.0f, 0.0f);
    t.heartbeat = sanitize(p.heartbeat, 0.0f, 1.0f, 0.0f);
    t.dread = sanitize(p.dread, 0.0f, 1.0f, 0.0f);
    t.consoleMusic = sanitize(p.consoleMusic, 0.0f, 1.0f, 0.0f);
    t.consoleMuffle = sanitize(p.consoleMuffle, 0.0f, 1.0f, 0.0f);
    t.musicTempo = sanitize(p.musicTempo, 0.5f, 2.0f, 1.0f);
    t.master = sanitize(p.master, 0.0f, 1.0f, 1.0f);
}

void Synth::stopAllSfx() { st_->stopAll(); }

void Synth::render(float* out, int frames) { st_->render(out, frames); }

int Synth::sampleRate() const { return st_->rate; }
