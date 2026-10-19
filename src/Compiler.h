#pragma once
// ============================================================================
//  Compiler.h — поиск компилятора (g++ → MSVC cl → clang++), формирование
//  командной строки сборки и разбор диагностических сообщений.
// ============================================================================
#include "Common.h"

enum class CompilerKind { Gcc, Msvc, Clang };

struct CompilerInfo {
    CompilerKind kind = CompilerKind::Gcc;
    std::wstring exe;       // полный путь к g++.exe / cl.exe / clang++.exe
    std::wstring vcvars;    // для MSVC вне Developer Prompt: путь к vcvars64.bat
    std::wstring version;
    std::wstring shortName() const;    // gcc / msvc / clang
    std::wstring displayName() const; // "g++ 13.2.0"
};

enum class Severity { None, Error, Warning, Note };

struct Diagnostic {
    std::wstring text;
    Severity sev = Severity::None;
    int line = 0;           // строка в main.cpp (с 1), 0 — нет привязки
    bool context = false;   // строка-контекст gcc вида " 12 |   code"
};

class Compiler {
public:
    static constexpr const wchar_t* kMainFile = L"main.cpp";
    static constexpr const wchar_t* kRuntimeFile = L"hackterm_rt.cpp";

    void detect();
    const CompilerInfo* current() const;
    const std::vector<CompilerInfo>& all() const { return list_; }
    bool select(const std::wstring& name);
    int standard() const { return std_; }
    bool setStandard(int s);

    // Пишет main.cpp (код пользователя, UTF-8) и hackterm_rt.cpp (отключение буферизации)
    bool writeSources(const std::wstring& dir, const std::wstring& code, std::wstring& error) const;
    std::wstring buildCommand(const std::wstring& exeName) const;
    std::vector<Diagnostic> parse(const std::wstring& output) const;
    static std::wstring installHelp();

private:
    std::vector<CompilerInfo> list_;
    int current_ = -1;
    int std_ = 17;
};
