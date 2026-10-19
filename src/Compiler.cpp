// ============================================================================
//  Compiler.cpp
// ============================================================================
#include "Compiler.h"

#include <cwctype>

#include "Runner.h"

namespace {

std::wstring quote(const std::wstring& s) { return L"\"" + s + L"\""; }

bool searchPath(const wchar_t* exe, std::wstring& out) {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = SearchPathW(nullptr, exe, nullptr, static_cast<DWORD>(sizeof(buf) / sizeof(buf[0])), buf, nullptr);
    if (n == 0 || n >= sizeof(buf) / sizeof(buf[0])) return false;
    out = buf;
    return true;
}

bool searchDirs(const std::vector<std::wstring>& dirs, const wchar_t* exe, std::wstring& out) {
    for (const auto& d : dirs) {
        std::wstring p = util::joinPath(util::expandEnv(d), exe);
        if (util::fileExists(p)) {
            out = p;
            return true;
        }
    }
    return false;
}

// Если компилятор найден не через PATH — добавляем его каталог в PATH нашего процесса.
// Дочерние процессы унаследуют его: g++ найдёт as/ld, а собранная программа — DLL рантайма.
void prependToPath(const std::wstring& dir) {
    std::wstring path = util::getEnv(L"PATH");
    if (util::toLower(path).find(util::toLower(dir)) != std::wstring::npos) return;
    SetEnvironmentVariableW(L"PATH", (dir + L";" + path).c_str());
}

std::wstring firstLine(const std::string& bytes) {
    std::wstring w = util::decodeBytes(bytes);
    auto lines = util::splitLines(w);
    for (auto& l : lines) {
        std::wstring t = util::trim(l);
        if (!t.empty()) return t;
    }
    return {};
}

// Достаёт число-версию после слова (например "version 17.0.6" или "Version 19.38.33134")
std::wstring versionAfter(const std::wstring& text, const std::wstring& word) {
    std::wstring low = util::toLower(text);
    size_t p = low.find(util::toLower(word));
    if (p == std::wstring::npos) return {};
    p += word.size();
    while (p < text.size() && text[p] == L' ') ++p;
    size_t e = p;
    while (e < text.size() && (std::iswdigit(text[e]) || text[e] == L'.')) ++e;
    return text.substr(p, e - p);
}

bool findVisualStudio(std::wstring& vcvars, std::wstring& version) {
    std::wstring vswhere = util::expandEnv(L"%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere.exe");
    if (!util::fileExists(vswhere))
        vswhere = util::expandEnv(L"%ProgramFiles%\\Microsoft Visual Studio\\Installer\\vswhere.exe");
    if (!util::fileExists(vswhere)) return false;

    std::string out;
    DWORD code = 0;
    std::wstring base = quote(vswhere) +
                        L" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property ";
    if (!Runner::capture(base + L"installationPath", L"", 15000, out, code) || code != 0) return false;
    std::wstring install = firstLine(out);
    if (install.empty()) return false;

    for (const wchar_t* bat : {L"\\VC\\Auxiliary\\Build\\vcvars64.bat", L"\\VC\\Auxiliary\\Build\\vcvars32.bat"}) {
        if (util::fileExists(install + bat)) {
            vcvars = install + bat;
            break;
        }
    }
    if (vcvars.empty()) return false;
    if (Runner::capture(base + L"catalog_productDisplayVersion", L"", 15000, out, code)) version = firstLine(out);
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
std::wstring CompilerInfo::shortName() const {
    switch (kind) {
        case CompilerKind::Gcc:   return L"gcc";
        case CompilerKind::Msvc:  return L"msvc";
        case CompilerKind::Clang: return L"clang";
    }
    return L"?";
}

std::wstring CompilerInfo::displayName() const {
    switch (kind) {
        case CompilerKind::Gcc:   return L"g++ " + version;
        case CompilerKind::Clang: return L"clang++ " + version;
        case CompilerKind::Msvc:
            return vcvars.empty() ? L"MSVC cl " + version : L"MSVC (VS " + version + L")";
    }
    return L"?";
}

// ---------------------------------------------------------------------------
void Compiler::detect() {
    list_.clear();
    current_ = -1;
    std::string out;
    DWORD code = 0;
    std::wstring path;

    // 1. g++ (MinGW-w64): сначала PATH, затем типичные каталоги установки
    static const std::vector<std::wstring> gccDirs = {
        L"C:\\msys64\\ucrt64\\bin",     L"C:\\msys64\\mingw64\\bin", L"C:\\mingw64\\bin",
        L"C:\\MinGW\\bin",              L"C:\\TDM-GCC-64\\bin",      L"%ProgramFiles%\\CodeBlocks\\MinGW\\bin",
        L"C:\\Strawberry\\c\\bin",      L"%ProgramFiles%\\mingw64\\bin"};
    bool inPath = searchPath(L"g++.exe", path);
    if (inPath || searchDirs(gccDirs, L"g++.exe", path)) {
        if (!inPath) prependToPath(util::dirOf(path));
        CompilerInfo c;
        c.kind = CompilerKind::Gcc;
        c.exe = path;
        if (Runner::capture(quote(path) + L" -dumpfullversion -dumpversion", L"", 10000, out, code))
            c.version = firstLine(out);
        list_.push_back(c);
    }

    // 2. MSVC: cl.exe в PATH (Developer Prompt) или поиск Visual Studio через vswhere
    if (searchPath(L"cl.exe", path)) {
        CompilerInfo c;
        c.kind = CompilerKind::Msvc;
        c.exe = path;
        if (Runner::capture(quote(path), L"", 10000, out, code)) c.version = versionAfter(util::decodeBytes(out), L"Version");
        list_.push_back(c);
    } else {
        CompilerInfo c;
        c.kind = CompilerKind::Msvc;
        if (findVisualStudio(c.vcvars, c.version)) {
            c.exe = L"cl.exe";
            list_.push_back(c);
        }
    }

    // 3. clang++ (LLVM)
    static const std::vector<std::wstring> clangDirs = {L"%ProgramFiles%\\LLVM\\bin", L"C:\\msys64\\clang64\\bin"};
    inPath = searchPath(L"clang++.exe", path);
    if (inPath || searchDirs(clangDirs, L"clang++.exe", path)) {
        if (!inPath) prependToPath(util::dirOf(path));
        CompilerInfo c;
        c.kind = CompilerKind::Clang;
        c.exe = path;
        if (Runner::capture(quote(path) + L" --version", L"", 10000, out, code))
            c.version = versionAfter(firstLine(out), L"version");
        list_.push_back(c);
    }

    if (!list_.empty()) current_ = 0;
}

const CompilerInfo* Compiler::current() const {
    return current_ >= 0 ? &list_[static_cast<size_t>(current_)] : nullptr;
}

bool Compiler::select(const std::wstring& name) {
    std::wstring n = util::toLower(name);
    CompilerKind kind;
    if (n == L"gcc" || n == L"g++" || n == L"mingw") kind = CompilerKind::Gcc;
    else if (n == L"msvc" || n == L"cl" || n == L"vs") kind = CompilerKind::Msvc;
    else if (n == L"clang" || n == L"clang++" || n == L"llvm") kind = CompilerKind::Clang;
    else return false;
    for (size_t i = 0; i < list_.size(); ++i) {
        if (list_[i].kind == kind) {
            current_ = static_cast<int>(i);
            return true;
        }
    }
    return false;
}

bool Compiler::setStandard(int s) {
    if (s != 17 && s != 20 && s != 23) return false;
    std_ = s;
    return true;
}

// ---------------------------------------------------------------------------
bool Compiler::writeSources(const std::wstring& dir, const std::wstring& code, std::wstring& error) const {
    // Вспомогательная единица трансляции: при выводе в пайп CRT включает полную
    // буферизацию, и вывод появлялся бы только по завершении программы.
    // Статический объект отключает буферизацию до вызова main().
    static const char* runtime =
        "// Автоматически создано HackTerm: вывод в реальном времени\n"
        "#include <cstdio>\n"
        "#include <iostream>\n"
        "namespace {\n"
        "struct HackTermUnbuffer {\n"
        "    HackTermUnbuffer() {\n"
        "        std::setvbuf(stdout, nullptr, _IONBF, 0);\n"
        "        std::setvbuf(stderr, nullptr, _IONBF, 0);\n"
        "        std::cout.setf(std::ios::unitbuf);\n"
        "    }\n"
        "} hackterm_unbuffer_instance;\n"
        "}\n";
    if (!util::writeFileBytes(util::joinPath(dir, kMainFile), util::toUtf8(code)) ||
        !util::writeFileBytes(util::joinPath(dir, kRuntimeFile), runtime)) {
        error = util::lastErrorText();
        return false;
    }
    return true;
}

std::wstring Compiler::buildCommand(const std::wstring& exeName) const {
    const CompilerInfo* c = current();
    if (!c) return {};
    const std::wstring sources = std::wstring(L" ") + kMainFile + L" " + kRuntimeFile;
    switch (c->kind) {
        case CompilerKind::Gcc: {
            std::wstring stdFlag = std_ == 23 ? L"c++2b" : L"c++" + std::to_wstring(std_);
            // -static: собранный exe не зависит от libstdc++-6.dll и запускается откуда угодно
            return quote(c->exe) + L" -std=" + stdFlag + L" -O2 -Wall -fdiagnostics-color=never -static -o " +
                   quote(exeName) + sources;
        }
        case CompilerKind::Clang: {
            std::wstring stdFlag = std_ == 23 ? L"c++2b" : L"c++" + std::to_wstring(std_);
            return quote(c->exe) + L" -std=" + stdFlag + L" -O2 -Wall -fno-color-diagnostics -o " + quote(exeName) +
                   sources;
        }
        case CompilerKind::Msvc: {
            std::wstring stdFlag = std_ == 23 ? L"/std:c++latest" : L"/std:c++" + std::to_wstring(std_);
            std::wstring cl = (c->vcvars.empty() ? quote(c->exe) : std::wstring(L"cl")) + L" /nologo /EHsc " +
                              stdFlag + L" /utf-8 /W3 /O2 /Fe" + quote(exeName) + sources;
            if (c->vcvars.empty()) return cl;
            // cl вне Developer Prompt: сначала настраиваем окружение через vcvars64.bat
            std::wstring comspec = util::getEnv(L"ComSpec");
            if (comspec.empty()) comspec = L"cmd.exe";
            return quote(comspec) + L" /d /s /c \"" + quote(c->vcvars) + L" >nul && " + cl + L"\"";
        }
    }
    return {};
}

// ---------------------------------------------------------------------------
namespace {

// GCC/Clang: "main.cpp:LINE:COL: error: ..."; MSVC: "main.cpp(LINE[,COL]): error C....: ..."
bool parseLocation(const std::wstring& l, int& line, size_t& rest) {
    const std::wstring file = Compiler::kMainFile;
    size_t p = l.find(file);
    if (p == std::wstring::npos) return false;
    if (p > 0 && l[p - 1] != L'\\' && l[p - 1] != L'/' && l[p - 1] != L' ') return false;
    size_t i = p + file.size();
    auto readNum = [&](int& v) {
        size_t s = i;
        v = 0;
        while (i < l.size() && std::iswdigit(l[i])) v = v * 10 + (l[i++] - L'0');
        return i > s;
    };
    int col = 0;
    if (i < l.size() && l[i] == L':') {
        ++i;
        if (!readNum(line)) return false;
        if (i < l.size() && l[i] == L':') { ++i; readNum(col); }
        if (i < l.size() && l[i] == L':') ++i;
    } else if (i < l.size() && l[i] == L'(') {
        ++i;
        if (!readNum(line)) return false;
        if (i < l.size() && l[i] == L',') { ++i; readNum(col); }
        if (i >= l.size() || l[i] != L')') return false;
        ++i;
        if (i < l.size() && l[i] == L':') ++i;
    } else {
        return false;
    }
    rest = i;
    return line > 0;
}

Severity severityOf(const std::wstring& s) {
    std::wstring t = util::toLower(util::trim(s));
    if (util::startsWith(t, L"error") || util::startsWith(t, L"fatal error")) return Severity::Error;
    if (util::startsWith(t, L"warning")) return Severity::Warning;
    if (util::startsWith(t, L"note")) return Severity::Note;
    return Severity::None;
}

// Строки-контекст gcc: "   12 |     std::cout << x" и "      |     ^~~~"
bool isContextLine(const std::wstring& s) {
    size_t i = s.find_first_not_of(L' ');
    if (i == std::wstring::npos) return false;
    while (i < s.size() && std::iswdigit(s[i])) ++i;
    while (i < s.size() && s[i] == L' ') ++i;
    return i < s.size() && s[i] == L'|';
}

} // namespace

std::vector<Diagnostic> Compiler::parse(const std::wstring& output) const {
    std::vector<Diagnostic> res;
    for (const auto& raw : util::splitLines(output)) {
        std::wstring line = raw;
        while (!line.empty() && (line.back() == L' ' || line.back() == L'\r')) line.pop_back();
        if (line.empty()) continue;
        // cl печатает имена компилируемых файлов — это шум
        if (line == kMainFile || line == kRuntimeFile) continue;

        Diagnostic d;
        d.text = line;
        size_t rest = 0;
        int ln = 0;
        if (parseLocation(line, ln, rest)) {
            d.line = ln;
            d.sev = severityOf(line.substr(rest));
        }
        if (d.sev == Severity::None) {
            std::wstring low = util::toLower(line);
            if (isContextLine(line)) d.context = true;
            else if (low.find(L"error") != std::wstring::npos || low.find(L"undefined reference") != std::wstring::npos)
                d.sev = Severity::Error;
            else if (low.find(L"warning") != std::wstring::npos) d.sev = Severity::Warning;
            else if (low.find(L"note:") != std::wstring::npos) d.sev = Severity::Note;
        }
        res.push_back(d);
    }
    return res;
}

std::wstring Compiler::installHelp() {
    return L"[!] Компилятор C++ не найден (искал g++, cl, clang++).\n"
           L"    Установите любой из вариантов и перезапустите HackTerm:\n"
           L"    1) MinGW-w64 через MSYS2 — https://www.msys2.org\n"
           L"         pacman -S mingw-w64-ucrt-x86_64-gcc\n"
           L"         HackTerm сам найдёт C:\\msys64\\ucrt64\\bin (или добавьте его в PATH)\n"
           L"       либо одной командой: winget install BrechtSanders.WinLibs.POSIX.UCRT\n"
           L"    2) Visual Studio / Build Tools с нагрузкой «Разработка классических приложений на C++»\n"
           L"         https://visualstudio.microsoft.com/downloads/ — cl.exe будет найден через vswhere\n"
           L"    3) LLVM/Clang: winget install LLVM.LLVM\n"
           L"    Проверка: в новой консоли выполните  g++ --version";
}
