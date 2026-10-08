// ============================================================================
//  Achievements.h — 40 достижений и их сохранение между запусками.
//
//  Модуль не зависит от WinAPI: путь к файлу сохранения передаёт платформа
//  (рядом с .exe). Если путь не задан или файл недоступен, достижения
//  просто живут в памяти до выхода из игры.
// ============================================================================
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

enum class Ach : int {
    FirstTrip,     // начать первую поездку
    WinAny,        // доехать до дома
    WinEasy,       // доехать на лёгкой
    WinNormal,     // доехать на нормальной
    WinHard,       // доехать на сложной
    FirstPhoto,    // первый снимок со вспышкой
    Hits10,        // 10 попаданий за поездку
    Hits50Total,   // 50 попаданий за всё время
    Miss,          // вспышка мимо
    PerfectAim,    // доехать, ни разу не промахнувшись
    Cans30,        // 30 канистр за поездку
    FullTank,      // полный бак
    SputterSave,   // спастись канистрой, когда двигатель уже чихал
    LoseFuel,      // заглохнуть
    LoseMonster,   // не уберечься от гостя
    CleanRun,      // доехать без единой аварии в мини-игре
    Crashes20,     // 20 аварий в мини-игре за поездку
    SuperHit,      // попасть усиленной вспышкой
    LockUsed,      // подобрать замок
    CloseCall,     // доехать, когда угроза доходила до 90%
    EndlessFound,  // найти бесконечную дорогу (секрет)
    Faceless,      // увидеть, как обернутся родители (секрет)
    Endless15km,   // проехать 15 км по бесконечной дороге (секрет)
    Wins5,         // доехать 5 раз
    Console60,     // минута без отрыва от консоли
    Pretender,     // впервые притвориться спящим
    DeepSleep,     // 30 секунд ровного «сна»
    Overheard,     // подслушать родителей «во сне»
    Whispers,      // услышать шёпот безликих (секрет)
    Caught,        // сбиться с дыхания — тебя заметили
    Lullaby,       // гость ушёл, пока ребёнок «спал»
    Marathon,      // 10 минут на бесконечной дороге
    Km50,          // 50 км по бесконечной дороге (секрет)
    Streak5,       // 5 попаданий подряд без промаха
    Collector,     // камера, батарейка и замок за одну поездку
    Untouched,     // доехать, когда угроза не поднималась выше 20%
    SilentRide,    // доехать с выключенным звуком
    Hush,          // убавить громкость до 10%
    Stubborn,      // проиграть 5 раз
    FaceToFace,    // вспышка в лица безликих родителей (секрет)
    Count
};

constexpr int kAchCount = static_cast<int>(Ach::Count);

struct AchievementInfo {
    const char* title; // название (UTF-8)
    const char* desc;  // как получить (UTF-8)
    bool secret;       // пока не получено — показывается как «???»
};

class Achievements {
public:
    Achievements();

    // Задать файл сохранения и сразу загрузить из него прогресс.
    void setSaveFile(const std::filesystem::path& file);

    // Выдать достижение. true — если оно новое (тогда оно же попадёт в очередь
    // всплывающих уведомлений и будет сохранено на диск).
    bool unlock(Ach a);
    bool has(Ach a) const;
    int unlockedCount() const;

    // Накопительные счётчики (сохраняются).
    void addHits(int n);   // попадания вспышкой
    void addWin();         // доезды до дома
    void addLoss();        // проигрыши
    int totalHits() const { return totalHits_; }
    int totalWins() const { return totalWins_; }

    // Очередь всплывающих уведомлений: true и a — следующее новое достижение.
    bool takeUnlocked(Ach& a);

    static const AchievementInfo& info(Ach a);

private:
    void load();
    void save() const;

    std::array<bool, kAchCount> unlocked_{};
    int totalHits_ = 0;
    int totalWins_ = 0;
    int totalLosses_ = 0;
    std::vector<Ach> pending_;
    std::filesystem::path file_;
};
