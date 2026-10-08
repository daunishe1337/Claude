// ============================================================================
//  Achievements.cpp — список достижений, выдача и сохранение в текстовый файл.
//
//  Формат файла (одна строка на поле, легко читать глазами):
//      BACKSEAT-ACH 1
//      0100110...   — 25 символов '0'/'1', по порядку Ach
//      <попаданий всего>
//      <доездов всего>
//  Повреждённый или чужой файл игнорируется (прогресс начинается заново).
// ============================================================================
#include "Achievements.h"

#include "Common.h"

#include <fstream>
#include <string>

namespace {

const char* const kHeader = "BACKSEAT-ACH 1";

// Порядок строго совпадает с enum class Ach.
const AchievementInfo kInfo[kAchCount] = {
    {T8("В путь"), T8("Начать первую поездку."), false},
    {T8("Дома"), T8("Доехать до дома."), false},
    {T8("Лёгкая прогулка"), T8("Доехать на лёгком уровне."), false},
    {T8("Как обычно"), T8("Доехать на нормальном уровне."), false},
    {T8("Железные нервы"), T8("Доехать на сложном уровне."), false},
    {T8("Щёлк!"), T8("Сделать первый снимок со вспышкой."), false},
    {T8("Фотограф"), T8("10 попаданий вспышкой за одну поездку."), false},
    {T8("Папарацци"), T8("50 попаданий вспышкой за всё время."), false},
    {T8("Мимо"), T8("Сверкнуть вспышкой в пустое окно."), false},
    {T8("Снайпер"), T8("Доехать, ни разу не промахнувшись."), false},
    {T8("Заправщик"), T8("Собрать 30 канистр за одну поездку."), false},
    {T8("Полный бак"), T8("Заправить бак до 100%."), false},
    {T8("На последних каплях"), T8("Спасти чихающий двигатель канистрой."), false},
    {T8("Заглохли"), T8("Остаться без бензина посреди дороги."), false},
    {T8("Оно рядом"), T8("Не уберечься от гостя на крыше."), false},
    {T8("Чистая трасса"), T8("Доехать без единой аварии в мини-игре."), false},
    {T8("Аварийщик"), T8("20 аварий в мини-игре за одну поездку."), false},
    {T8("Ослепительно"), T8("Попасть усиленной вспышкой."), false},
    {T8("На замок"), T8("Подобрать в игре замок."), false},
    {T8("На волоске"), T8("Доехать, когда угроза доходила до 90%."), false},
    {T8("Бесконечная дорога"), T8("Найти секретный бесконечный режим."), true},
    {T8("У них нет лиц"), T8("Продержаться на бесконечной дороге 3 минуты."), true},
    {T8("Дальнобойщик"), T8("Проехать 15 км по бесконечной дороге."), true},
    {T8("Постоянный пассажир"), T8("Доехать до дома 5 раз."), false},
    {T8("Не отрываясь"), T8("Минуту подряд не опускать консоль."), false},
};

} // namespace

Achievements::Achievements() { pending_.reserve(kAchCount); }

const AchievementInfo& Achievements::info(Ach a) {
    const int i = clampi(static_cast<int>(a), 0, kAchCount - 1);
    return kInfo[i];
}

void Achievements::setSaveFile(const std::filesystem::path& file) {
    file_ = file;
    load();
}

bool Achievements::unlock(Ach a) {
    const int i = static_cast<int>(a);
    if (i < 0 || i >= kAchCount || unlocked_[static_cast<size_t>(i)]) return false;
    unlocked_[static_cast<size_t>(i)] = true;
    pending_.push_back(a);
    save();
    return true;
}

bool Achievements::has(Ach a) const {
    const int i = static_cast<int>(a);
    return i >= 0 && i < kAchCount && unlocked_[static_cast<size_t>(i)];
}

int Achievements::unlockedCount() const {
    int n = 0;
    for (bool u : unlocked_)
        if (u) ++n;
    return n;
}

void Achievements::addHits(int n) {
    if (n <= 0) return;
    totalHits_ += n;
    if (totalHits_ >= 50) unlock(Ach::Hits50Total);
    save();
}

void Achievements::addWin() {
    ++totalWins_;
    if (totalWins_ >= 5) unlock(Ach::Wins5);
    save();
}

bool Achievements::takeUnlocked(Ach& a) {
    if (pending_.empty()) return false;
    a = pending_.front();
    pending_.erase(pending_.begin());
    return true;
}

void Achievements::load() {
    if (file_.empty()) return;
    std::ifstream in(file_);
    if (!in) return;
    std::string header, bits;
    int hits = 0, wins = 0;
    if (!std::getline(in, header) || header != kHeader) return;
    if (!std::getline(in, bits) || static_cast<int>(bits.size()) < kAchCount) return;
    if (!(in >> hits >> wins)) return;
    for (int i = 0; i < kAchCount; ++i) unlocked_[static_cast<size_t>(i)] = bits[static_cast<size_t>(i)] == '1';
    totalHits_ = std::max(0, hits);
    totalWins_ = std::max(0, wins);
}

void Achievements::save() const {
    if (file_.empty()) return;
    std::ofstream out(file_, std::ios::trunc);
    if (!out) return; // нет прав на запись — прогресс останется в памяти
    out << kHeader << '\n';
    for (bool u : unlocked_) out << (u ? '1' : '0');
    out << '\n' << totalHits_ << '\n' << totalWins_ << '\n';
}
