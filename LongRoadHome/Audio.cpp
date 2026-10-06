// Audio.cpp - синтезатор и вывод через waveOut.
#include "Audio.h"
#include <cmath>
#include <cstring>
#include "Common.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <thread>
#pragma comment(lib, "winmm.lib")
#endif

namespace lrh {

namespace {
constexpr float kRate = 44100.0f;
constexpr int   kBufFrames = 640;
constexpr int   kBufCount = 4;
constexpr int   kMaxVoices = 16;
constexpr float k2Pi = 6.2831853f;

// Фильтр с переменным состоянием (Chamberlin); берём полосовой выход.
struct Svf {
    float lp = 0, bp = 0;
    float bandpass(float in, float f, float damp) {
        lp += f * bp;
        float hp = in - lp - damp * bp;
        bp += f * hp;
        return bp;
    }
};

// Голос «ссорящегося родителя»: гортанный импульс + формантные фильтры + слоговая огибающая.
struct Talker {
    float phase = 0, pitch = 120, pitchTarget = 120, level = 0;
    float sylT = 0, env = 0;
    bool voiced = false;
    float c1 = 0.1f, c2 = 0.2f;
    Svf f1, f2;
};

struct SfxVoice {
    bool active = false;
    Sfx type = Sfx::Click;
    float t = 0, vol = 1, pan = 0, ph = 0, lp = 0;
    Svf f;
};

float sfxDuration(Sfx s) {
    switch (s) {
        case Sfx::Step: return 0.35f;   case Sfx::Knock: return 0.2f;
        case Sfx::Beep: return 0.08f;   case Sfx::BeepLow: return 0.12f;
        case Sfx::Pickup: return 0.21f; case Sfx::Crash: return 0.5f;
        case Sfx::Flash: return 0.8f;   case Sfx::Hiss: return 1.0f;
        case Sfx::Scream: return 1.7f;  case Sfx::Whoosh: return 1.0f;
        case Sfx::Crack: return 0.3f;   case Sfx::Switch: return 0.06f;
        case Sfx::Win: return 0.9f;     case Sfx::Click: return 0.02f;
        case Sfx::Skid: return 0.7f;    case Sfx::Stall: return 1.0f;
    }
    return 0.1f;
}
}  // namespace

// ---- Состояние синтезатора (трогает только аудио-поток) ----------------------------
struct Audio::Synth {
    uint32_t rng = 0x9E3779B9u;
    float noise() {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return float(int32_t(rng)) * (1.0f / 2147483648.0f);
    }
    float rnd01() { return noise() * 0.5f + 0.5f; }

    // сглаженные параметры
    float rpm = 0, engVol = 0, stut = 0, road = 0, scrape = 0, scrapeP = 0, hbVol = 0, amb = 0.25f;
    float ePh = 0, lopePh = 0, eLp = 0, gate = 1, gateTarget = 1; int gateT = 0;
    float brown = 0, windLp = 0, windPh = 0;
    float scrPh1 = 0, scrPh2 = 0; Svf scrF;
    float hbT = 0;
    Talker dad, mom;
    float voiceLp = 0;
    SfxVoice voices[kMaxVoices];
};

// ---- Платформа ----------------------------------------------------------------------
#ifdef _WIN32
struct Audio::Platform {
    HWAVEOUT h = nullptr;
    HANDLE ev = nullptr;
    WAVEHDR hdr[kBufCount];
    std::vector<int16_t> data[kBufCount];
    std::thread th;
    std::atomic<bool> run{false};
};
#else
struct Audio::Platform {};
#endif

Audio::Audio() : syn_(new Synth), plat_(new Platform) {}
Audio::~Audio() { shutdown(); }

void Audio::play(Sfx s, float vol, float pan) {
    std::lock_guard<std::mutex> lk(qMutex_);
    if (queue_.size() < 64) queue_.push_back({s, vol, pan});
}

void Audio::silenceAll() {
    engineVol = 0; engineRpm = 0; stutter = 0; roadVol = 0; scrapeVol = 0;
    heartVol = 0; voiceDad = 0; voiceMom = 0;
}

namespace {
// Один отсчёт одноразового эффекта. Может менять pan (для «проезда» мимо).
float sfxSample(SfxVoice&, float, float&);
}

void Audio::render(int16_t* out, int frames) {
    Synth& S = *syn_;

    // 1) принять новые эффекты
    {
        std::lock_guard<std::mutex> lk(qMutex_);
        for (const Event& e : queue_) {
            SfxVoice* slot = nullptr;
            for (SfxVoice& v : S.voices) if (!v.active) { slot = &v; break; }
            if (!slot) {  // нет места: вытесняем самый старый
                slot = &S.voices[0];
                for (SfxVoice& v : S.voices) if (v.t > slot->t) slot = &v;
            }
            *slot = SfxVoice();
            slot->active = true; slot->type = e.type; slot->vol = e.vol; slot->pan = e.pan;
        }
        queue_.clear();
    }

    // 2) целевые параметры (один раз на блок)
    const float tRpm = engineRpm.load(std::memory_order_relaxed);
    const float tEng = engineVol.load(std::memory_order_relaxed);
    const float tStut = stutter.load(std::memory_order_relaxed);
    const float tRoad = roadVol.load(std::memory_order_relaxed);
    const float tScr = scrapeVol.load(std::memory_order_relaxed);
    const float tScrP = scrapePan.load(std::memory_order_relaxed);
    const float bpm = std::max(30.0f, heartBpm.load(std::memory_order_relaxed));
    const float tHb = heartVol.load(std::memory_order_relaxed);
    const float tDad = voiceDad.load(std::memory_order_relaxed);
    const float tMom = voiceMom.load(std::memory_order_relaxed);
    const float tAmb = ambience.load(std::memory_order_relaxed);
    const float mst = master.load(std::memory_order_relaxed);

    static const float kVow[5][2] = {{730, 1090}, {530, 1840}, {270, 2290}, {570, 840}, {300, 870}};
    const float dt = 1.0f / kRate;

    for (int i = 0; i < frames; ++i) {
        // сглаживание
        S.rpm += (tRpm - S.rpm) * 0.0006f;
        S.engVol += (tEng - S.engVol) * 0.0010f;
        S.stut += (tStut - S.stut) * 0.0010f;
        S.road += (tRoad - S.road) * 0.0010f;
        S.scrape += (tScr - S.scrape) * 0.0020f;
        S.scrapeP += (tScrP - S.scrapeP) * 0.0020f;
        S.hbVol += (tHb - S.hbVol) * 0.0010f;
        S.amb += (tAmb - S.amb) * 0.0005f;
        S.dad.level += (tDad - S.dad.level) * 0.0008f;
        S.mom.level += (tMom - S.mom.level) * 0.0008f;

        float L = 0, R = 0;
        const float n = S.noise();

        // --- двигатель ---
        if (S.engVol > 0.002f) {
            float f0 = 28.0f + S.rpm * 55.0f;
            S.ePh += f0 * dt; if (S.ePh >= 1.0f) S.ePh -= 1.0f;
            S.lopePh += f0 * 0.5f * dt; if (S.lopePh >= 1.0f) S.lopePh -= 1.0f;
            float p = S.ePh * k2Pi;
            float e = std::sin(p) + 0.55f * std::sin(2 * p + 0.6f) + 0.35f * std::sin(3 * p + 1.1f) +
                      0.18f * std::sin(4 * p + 2.0f) + 0.08f * std::sin(6 * p);
            e *= 0.78f + 0.22f * std::sin(S.lopePh * k2Pi);
            e += n * 0.10f;
            S.eLp += (e - S.eLp) * 0.20f;
            if (--S.gateT <= 0) {  // чихание: случайные провалы громкости
                S.gateT = 700 + int(S.rnd01() * 2600.0f);
                S.gateTarget = (S.rnd01() < S.stut * 0.75f) ? 0.0f : 1.0f;
            }
            S.gate += (S.gateTarget - S.gate) * 0.004f;
            float eo = S.eLp * S.engVol * S.gate * 0.30f;
            L += eo; R += eo;
        }
        // --- дорога / ветер ---
        S.brown = S.brown * 0.985f + n * 0.10f;
        S.windLp += (n - S.windLp) * 0.02f;
        S.windPh += 0.35f * dt; if (S.windPh > 1000.0f) S.windPh = 0;
        float wind = S.windLp * S.amb * 1.6f * (0.75f + 0.25f * std::sin(S.windPh * k2Pi));
        float roadN = S.brown * S.road * 0.35f + n * 0.010f * S.road;
        L += wind + roadN; R += wind + roadN;

        // --- скрежет по крыше ---
        if (S.scrape > 0.003f) {
            S.scrPh1 += 3.1f * dt; S.scrPh2 += 0.9f * dt;
            float fc = 1100.0f + 650.0f * std::sin(S.scrPh1 * k2Pi) + 250.0f * n;
            float f = 2.0f * std::sin(kPi * fc / kRate);
            float bp = S.scrF.bandpass(n, f, 0.22f);
            float am = (0.55f + 0.45f * std::sin(S.scrPh2 * k2Pi)) * (0.6f + 0.4f * std::sin(S.scrPh1 * 1.7f * k2Pi));
            float so = bp * am * S.scrape * 0.9f;
            float a = (S.scrapeP + 1.0f) * kPi * 0.25f;
            L += so * std::cos(a); R += so * std::sin(a);
        }

        // --- сердцебиение ---
        if (S.hbVol > 0.01f) {
            S.hbT += dt;
            float period = 60.0f / bpm;
            if (S.hbT >= period) S.hbT -= period;
            float t = S.hbT, h = 0;
            if (t < 0.16f) h += std::sin(k2Pi * 52.0f * t) * std::exp(-t * 26.0f);
            float t2 = t - 0.19f;
            if (t2 >= 0 && t2 < 0.16f) h += 0.75f * std::sin(k2Pi * 44.0f * t2) * std::exp(-t2 * 28.0f);
            h *= S.hbVol * 0.75f;
            L += h; R += h;
        }

        // --- голоса родителей ---
        {
            Talker* ts[2] = {&S.dad, &S.mom};
            const float base[2] = {105.0f, 205.0f};
            const float pan[2] = {-0.35f, 0.35f};
            float vm = 0, vl = 0, vr = 0;
            for (int k = 0; k < 2; ++k) {
                Talker& T = *ts[k];
                if (T.level < 0.004f) { T.env *= 0.999f; continue; }
                T.sylT -= dt;
                if (T.sylT <= 0) {
                    if (S.rnd01() < 0.2f) { T.voiced = false; T.sylT = 0.12f + S.rnd01() * 0.30f; }
                    else {
                        T.voiced = true; T.sylT = 0.08f + S.rnd01() * 0.12f;
                        int v = int(S.rnd01() * 4.99f);
                        T.c1 = 2.0f * std::sin(kPi * kVow[v][0] / kRate);
                        T.c2 = 2.0f * std::sin(kPi * kVow[v][1] / kRate);
                        T.pitchTarget = base[k] * (0.85f + S.rnd01() * 0.45f);
                    }
                }
                T.env += ((T.voiced ? 1.0f : 0.0f) - T.env) * (T.voiced ? 0.004f : 0.002f);
                T.pitch += (T.pitchTarget - T.pitch) * 0.0006f;
                T.phase += T.pitch * dt; if (T.phase >= 1.0f) T.phase -= 1.0f;
                float src = (2.0f * T.phase - 1.0f) * 0.8f + n * 0.12f;
                float o = (T.f1.bandpass(src, T.c1, 0.25f) + 0.7f * T.f2.bandpass(src, T.c2, 0.22f)) *
                          T.env * T.level * 0.09f;
                vm += o;
                float a = (pan[k] + 1.0f) * kPi * 0.25f;
                vl += o * std::cos(a); vr += o * std::sin(a);
            }
            // «глухой» голос: срезаем верха
            S.voiceLp += (vm - S.voiceLp) * 0.22f;
            float k = (std::fabs(vm) > 1e-6f) ? (S.voiceLp / vm) : 0.3f;
            L += vl * std::min(1.0f, std::fabs(k)); R += vr * std::min(1.0f, std::fabs(k));
        }

        // --- одноразовые эффекты ---
        for (SfxVoice& v : S.voices) {
            if (!v.active) continue;
            float pan = v.pan;
            float s = sfxSample(v, n, pan) * v.vol;
            float a = (clampf(pan, -1.0f, 1.0f) + 1.0f) * kPi * 0.25f;
            L += s * std::cos(a); R += s * std::sin(a);
        }

        // мягкий лимитер и вывод
        L = std::tanh(L * mst * 0.9f); R = std::tanh(R * mst * 0.9f);
        out[i * 2] = int16_t(L * 30000.0f);
        out[i * 2 + 1] = int16_t(R * 30000.0f);
    }
}

namespace {
float sfxSample(SfxVoice& v, float n, float& pan) {
    const float dt = 1.0f / kRate;
    const float dur = sfxDuration(v.type);
    const float t = v.t;
    v.t += dt;
    if (t >= dur) { v.active = false; return 0.0f; }
    auto sq = [](float ph) { return (ph - std::floor(ph)) < 0.5f ? 1.0f : -1.0f; };
    float s = 0.0f;
    switch (v.type) {
        case Sfx::Step: {  // глухой удар лапы по крыше
            v.ph += (30.0f + 32.0f * std::exp(-t * 3.0f)) * dt;
            v.lp += (n - v.lp) * 0.07f;
            s = std::sin(v.ph * k2Pi) * std::exp(-t * 17.0f) * 1.1f + v.lp * std::exp(-t * 32.0f) * 1.6f;
            break;
        }
        case Sfx::Knock: {
            v.ph += 190.0f * dt;
            s = std::sin(v.ph * k2Pi) * std::exp(-t * 45.0f) * 0.9f + n * std::exp(-t * 300.0f) * 0.5f;
            break;
        }
        case Sfx::Beep: v.ph += 1318.0f * dt; s = sq(v.ph) * 0.16f; break;
        case Sfx::BeepLow: v.ph += 330.0f * dt; s = sq(v.ph) * 0.18f * (1.0f - t / dur); break;
        case Sfx::Pickup: {
            static const float notes[3] = {659.0f, 880.0f, 1318.0f};
            int idx = std::min(2, int(t / 0.07f));
            v.ph += notes[idx] * dt;
            s = sq(v.ph) * 0.16f * (1.0f - 0.3f * fract(t / 0.07f));
            break;
        }
        case Sfx::Crash: {
            v.lp += (n - v.lp) * 0.15f;
            v.ph += (45.0f + 25.0f * std::exp(-t * 8.0f)) * dt;
            s = v.lp * std::exp(-t * 7.0f) * 1.4f + std::sin(v.ph * k2Pi) * std::exp(-t * 12.0f) * 0.9f;
            break;
        }
        case Sfx::Flash: {
            float f = 1000.0f + 3500.0f * smoothstep(t / 0.5f);
            v.ph += f * dt;
            s = n * std::exp(-t * 120.0f) * 0.7f +
                std::sin(v.ph * k2Pi) * 0.11f * std::exp(-t * 3.5f) * std::min(1.0f, t * 50.0f);
            break;
        }
        case Sfx::Hiss: {
            float hp = n - v.lp; v.lp += (n - v.lp) * 0.3f;
            float env = std::pow(std::sin(kPi * t / dur), 1.5f);
            s = hp * env * (0.7f + 0.3f * std::sin(k2Pi * 13.0f * t)) * 0.55f;
            break;
        }
        case Sfx::Scream: {
            float f = 380.0f + 700.0f * std::min(1.0f, t * 3.0f) + 80.0f * std::sin(k2Pi * 9.0f * t);
            v.ph += f * dt;
            float saw = 2.0f * fract(v.ph) - 1.0f;
            float env = std::min(1.0f, t * 80.0f) * (t < 1.3f ? 1.0f : (dur - t) / (dur - 1.3f));
            s = (std::tanh(4.0f * saw) * 0.45f + n * 0.30f + std::sin(v.ph * 2.01f * k2Pi) * 0.2f) * env * 0.8f;
            break;
        }
        case Sfx::Whoosh: {
            float u = t / dur;
            float fc = 250.0f + 1400.0f * std::sin(kPi * u);
            float f = 2.0f * std::sin(kPi * fc / kRate);
            float bp = v.f.bandpass(n, f, 0.35f);
            float env = std::sin(kPi * u); env *= env;
            s = bp * env * 0.9f;
            pan = v.pan * (1.0f - 2.0f * u);
            break;
        }
        case Sfx::Crack: {
            float hp = n - v.lp; v.lp += (n - v.lp) * 0.25f;
            float env = std::exp(-t * 14.0f) + (t > 0.09f ? 0.6f * std::exp(-(t - 0.09f) * 40.0f) : 0.0f);
            s = hp * env * 1.1f;
            break;
        }
        case Sfx::Switch:
            v.ph += 500.0f * dt;
            s = n * std::exp(-t * 90.0f) * 0.5f + std::sin(v.ph * k2Pi) * std::exp(-t * 60.0f) * 0.3f;
            break;
        case Sfx::Win: {
            static const float notes[4] = {523.0f, 659.0f, 784.0f, 1046.0f};
            int idx = std::min(3, int(t / 0.17f));
            v.ph += notes[idx] * dt;
            float tri = 2.0f * std::fabs(2.0f * fract(v.ph) - 1.0f) - 1.0f;
            float env = idx < 3 ? 1.0f - 0.4f * fract(t / 0.17f) : std::exp(-(t - 0.51f) * 4.0f);
            s = tri * env * 0.3f;
            break;
        }
        case Sfx::Click: s = n * 0.4f * (1.0f - t / dur); break;
        case Sfx::Skid: {
            float f = 2.0f * std::sin(kPi * 2200.0f / kRate);
            float bp = v.f.bandpass(n, f, 0.3f);
            s = bp * std::sin(kPi * t / dur) * (0.7f + 0.3f * std::sin(k2Pi * 30.0f * t)) * 0.35f;
            break;
        }
        case Sfx::Stall: {
            v.ph += 45.0f * dt;
            v.lp += (n - v.lp) * 0.1f;
            s = std::sin(v.ph * k2Pi) * std::exp(-t * 6.0f) * 0.9f + v.lp * std::exp(-t * 20.0f) * 0.7f;
            break;
        }
    }
    return s;
}
}  // namespace

// ---- Windows: открытие устройства и поток подкачки -----------------------------------
#ifdef _WIN32
bool Audio::init() {
    Platform& P = *plat_;
    if (P.h) return true;
    WAVEFORMATEX fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = 44100;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = fmt.nChannels * fmt.wBitsPerSample / 8;
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

    P.ev = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    if (!P.ev) return false;
    if (waveOutOpen(&P.h, WAVE_MAPPER, &fmt, reinterpret_cast<DWORD_PTR>(P.ev), 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        P.h = nullptr;
        CloseHandle(P.ev); P.ev = nullptr;
        return false;
    }
    for (int i = 0; i < kBufCount; ++i) {
        P.data[i].assign(size_t(kBufFrames) * 2, 0);
        std::memset(&P.hdr[i], 0, sizeof(WAVEHDR));
        P.hdr[i].lpData = reinterpret_cast<LPSTR>(P.data[i].data());
        P.hdr[i].dwBufferLength = DWORD(P.data[i].size() * sizeof(int16_t));
        waveOutPrepareHeader(P.h, &P.hdr[i], sizeof(WAVEHDR));
        render(P.data[i].data(), kBufFrames);
        waveOutWrite(P.h, &P.hdr[i], sizeof(WAVEHDR));
    }
    P.run = true;
    P.th = std::thread([this, &P]() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        while (P.run) {
            WaitForSingleObject(P.ev, 50);
            for (int i = 0; i < kBufCount && P.run; ++i) {
                if (P.hdr[i].dwFlags & WHDR_DONE) {
                    render(P.data[i].data(), kBufFrames);
                    waveOutWrite(P.h, &P.hdr[i], sizeof(WAVEHDR));
                }
            }
        }
    });
    return true;
}

void Audio::shutdown() {
    Platform& P = *plat_;
    if (!P.h) return;
    P.run = false;
    SetEvent(P.ev);
    if (P.th.joinable()) P.th.join();
    waveOutReset(P.h);
    for (int i = 0; i < kBufCount; ++i) waveOutUnprepareHeader(P.h, &P.hdr[i], sizeof(WAVEHDR));
    waveOutClose(P.h);
    P.h = nullptr;
    CloseHandle(P.ev); P.ev = nullptr;
}
#else
bool Audio::init() { return false; }  // не Windows: звук недоступен (для тестов)
void Audio::shutdown() {}
#endif

}  // namespace lrh
