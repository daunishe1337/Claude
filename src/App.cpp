// ============================================================================
//  App.cpp
// ============================================================================
#include "App.h"

#include <algorithm>
#include <cstdlib>
#include <cwctype>

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")   // MessageBoxW, буфер обмена
#endif

namespace {

App* g_app = nullptr;

// Ctrl+C у нас обычная клавиша (ENABLE_PROCESSED_INPUT выключен), а Ctrl+Break
// игнорируем. При закрытии окна успеваем сохранить сессию; дочерние процессы
// будут убиты автоматически благодаря JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE.
BOOL WINAPI ctrlHandler(DWORD type) {
    switch (type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            return TRUE;
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            if (g_app) g_app->emergencySave();
            return FALSE;
        default:
            return FALSE;
    }
}

const wchar_t* kTemplate =
    L"// HackTerm :: payload.cpp\n"
    L"// F5 или Ctrl+Enter — скомпилировать и запустить\n"
    L"#include <iostream>\n"
    L"#include <string>\n"
    L"\n"
    L"int main() {\n"
    L"    std::cout << \"Hello, World!\" << std::endl;\n"
    L"    std::cout << \"Wake up, Neo... The Matrix has you.\" << std::endl;\n"
    L"\n"
    L"    std::string name;\n"
    L"    std::cout << \"Enter your name: \";\n"
    L"    std::cin >> name;\n"
    L"    std::cout << \"Access granted, \" << name << \".\" << std::endl;\n"
    L"    return 0;\n"
    L"}\n";

const std::vector<std::wstring>& commandNames() {
    static const std::vector<std::wstring> c = {L"run",   L"new",   L"save",     L"load",    L"clear", L"help",
                                                L"exit",  L"files", L"goto",     L"stop",    L"timeout",
                                                L"compiler", L"std", L"theme",   L"matrix",  L"about"};
    return c;
}

bool hasExtension(const std::wstring& path) {
    std::wstring base = util::baseName(path);
    return base.find(L'.') != std::wstring::npos;
}

} // namespace

App::App() : theme_(Theme::matrix()), ui_(console_, theme_) {}

// ===========================================================================
//  Главный цикл
// ===========================================================================
int App::run() {
    std::wstring err;
    if (!console_.init(err)) {
        MessageBoxW(nullptr, err.c_str(), L"HackTerm", MB_ICONERROR | MB_OK);
        return 1;
    }
    g_app = this;
    SetConsoleCtrlHandler(ctrlHandler, TRUE);
    // Упавшая программа пользователя не должна показывать системные диалоги (режим наследуется)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    // Сообщения MSVC на английском — их проще разбирать, и нет проблем с кодировкой
    SetEnvironmentVariableW(L"VSLANG", L"1033");
    console_.setTitle(L"HackTerm :: root@hack");

    prepareWorkDir();
    loadSession();

    ui_.splash([this]() {
        compiler_.detect();
        const CompilerInfo* c = compiler_.current();
        return c ? c->displayName() : std::wstring();
    });
    console_.flushInput();
    ui_.invalidate();
    printWelcome();

    ULONGLONG lastFrame = 0;
    bool dirty = true;
    while (running_) {
        if (console_.waitInput(15)) {
            InputBatch b = console_.read();
            if (!b.keys.empty() || !b.wheels.empty() || b.resized) dirty = true;
            if (b.resized) ui_.invalidate();
            handleInput(b);
        }
        if (pollProcesses()) dirty = true;
        if (out_.tick(5)) dirty = true;

        ULONGLONG now = GetTickCount64();
        if (now - blinkTick_ >= 530) {
            blink_ = !blink_;
            blinkTick_ = now;
            dirty = true;
        }
        if (state_ != RunState::Idle && now - lastFrame >= 100) dirty = true;  // таймер в статусе
        if (dirty && running_) {
            render();
            lastFrame = now;
            dirty = false;
        }
    }

    // Корректное завершение: убиваем процесс, дожидаемся освобождения хэндлов
    if (runner_.active()) {
        runner_.kill();
        std::vector<Runner::Chunk> chunks;
        DWORD code = 0;
        ULONGLONG deadline = GetTickCount64() + 3000;
        while (!runner_.poll(chunks, code) && GetTickCount64() < deadline) Sleep(10);
    }
    saveSession();
    cleanupWorkDir();
    SetConsoleCtrlHandler(ctrlHandler, FALSE);
    g_app = nullptr;
    console_.restore();
    return 0;
}

void App::render() {
    Layout L = ui_.layout();
    if (!L.tooSmall) editor_.layout(L.editorH, std::max(1, L.cols - ui_.gutterWidth(editor_)));
    ui_.render(L, editor_, out_, makeStatus());
}

StatusInfo App::makeStatus() const {
    StatusInfo si;
    const CompilerInfo* c = compiler_.current();
    si.compiler = c ? c->displayName() : L"NO COMPILER";
    si.standard = compiler_.standard();
    si.state = state_;
    ULONGLONG el = GetTickCount64() - stageStart_;
    switch (state_) {
        case RunState::Idle:      si.stateText = L"IDLE"; break;
        case RunState::Compiling: si.stateText = L"COMPILING " + util::formatSeconds(el); break;
        case RunState::Running:
            si.stateText = L"RUNNING " + util::formatSeconds(el);
            if (timeoutSec_ > 0) si.stateText += L" / " + std::to_wstring(timeoutSec_) + L"s";
            break;
    }
    si.focus = focus_;
    si.cmd = cmd_;
    si.cmdCursor = cmdCursor_;
    si.stdinLine = stdin_;
    si.blinkOn = blink_;
    return si;
}

void App::say(const std::wstring& text, OutKind kind, bool typed) {
    out_.scrollToBottom();
    out_.print(text, kind, typed);
}

// ===========================================================================
//  Ввод
// ===========================================================================
void App::handleInput(const InputBatch& b) {
    for (const auto& w : b.wheels) handleWheel(w);
    for (const auto& k : b.keys) {
        handleKey(k, b.paste);
        if (!running_) break;
    }
}

void App::handleWheel(const MouseWheel& w) {
    Layout L = ui_.layout();
    if (L.tooSmall) return;
    if (w.row >= L.editorTop && w.row < L.editorTop + L.editorH) editor_.scrollBy(-w.delta * 3);
    else if (w.row >= L.outTop && w.row < L.outTop + L.outH) out_.scrollBy(w.delta * 3);
}

void App::handleKey(const KeyEvent& k, bool paste) {
    blink_ = true;  // курсор не гаснет во время набора
    blinkTick_ = GetTickCount64();

    // --- Глобальные горячие клавиши ---
    if (k.ctrl && k.vk == 'C') {
        if (state_ != RunState::Idle) stopProcess(KillReason::User);
        else if (focus_ == Focus::Command) { cmd_.clear(); cmdCursor_ = 0; }
        return;
    }
    if (k.vk == VK_F5 || (k.vk == VK_RETURN && k.ctrl) || (k.ctrl && k.vk == 'R')) { runCode(); return; }
    if (k.vk == VK_F1) { printHelp(); return; }
    if (k.vk == VK_F8) { nextError(); return; }
    if (k.vk == VK_ESCAPE || k.vk == VK_F2) { toggleFocus(); return; }
    if (k.ctrl && k.vk == 'L') { out_.clear(); return; }
    if (k.ctrl && k.vk == 'Q') { if (state_ != RunState::Idle) stopProcess(KillReason::User); running_ = false; return; }
    if (k.ctrl && k.vk == 'S') {
        if (editor_.fileName().empty()) {
            focus_ = Focus::Command;
            cmd_ = L":save ";
            cmdCursor_ = cmd_.size();
        } else {
            cmdSave(L"");
        }
        return;
    }
    if (k.ctrl && k.vk == 'O') {
        focus_ = Focus::Command;
        cmd_ = L":load ";
        cmdCursor_ = cmd_.size();
        return;
    }
    if (k.ctrl && k.vk == 'V') { pasteClipboard(); return; }
    // Прокрутка панели вывода
    if (k.ctrl && k.vk == VK_UP) { out_.scrollBy(1); return; }
    if (k.ctrl && k.vk == VK_DOWN) { out_.scrollBy(-1); return; }
    if (k.shift && k.vk == VK_PRIOR) { out_.scrollBy(ui_.layout().outH - 1); return; }
    if (k.shift && k.vk == VK_NEXT) { out_.scrollBy(-(ui_.layout().outH - 1)); return; }

    switch (focus_) {
        case Focus::Editor:  editor_.handleKey(k, ui_.layout().editorH, paste); break;
        case Focus::Command: handleCommandKey(k); break;
        case Focus::Stdin:   handleStdinKey(k); break;
    }
}

// Esc / F2: редактор → командная строка → (stdin, если программа работает) → редактор
void App::toggleFocus() {
    switch (focus_) {
        case Focus::Editor:  focus_ = Focus::Command; break;
        case Focus::Command: focus_ = (state_ == RunState::Running) ? Focus::Stdin : Focus::Editor; break;
        case Focus::Stdin:   focus_ = Focus::Editor; break;
    }
}

void App::handleCommandKey(const KeyEvent& k) {
    switch (k.vk) {
        case VK_RETURN: {
            std::wstring line = cmd_;
            cmd_.clear();
            cmdCursor_ = 0;
            focus_ = Focus::Editor;
            execCommand(line);
            return;
        }
        case VK_BACK:
            if (cmdCursor_ > 0) cmd_.erase(--cmdCursor_, 1);
            return;
        case VK_DELETE:
            if (cmdCursor_ < cmd_.size()) cmd_.erase(cmdCursor_, 1);
            return;
        case VK_LEFT:  if (cmdCursor_ > 0) --cmdCursor_; return;
        case VK_RIGHT: if (cmdCursor_ < cmd_.size()) ++cmdCursor_; return;
        case VK_HOME:  cmdCursor_ = 0; return;
        case VK_END:   cmdCursor_ = cmd_.size(); return;
        case VK_UP:
            if (histPos_ > 0) {
                cmd_ = history_[--histPos_];
                cmdCursor_ = cmd_.size();
            }
            return;
        case VK_DOWN:
            if (histPos_ + 1 < history_.size()) {
                cmd_ = history_[++histPos_];
            } else {
                histPos_ = history_.size();
                cmd_.clear();
            }
            cmdCursor_ = cmd_.size();
            return;
        case VK_TAB: completeCommand(); return;
        default: break;
    }
    if (k.ch >= 32 && !k.ctrl && !k.alt) cmd_.insert(cmdCursor_++, 1, k.ch);
}

void App::handleStdinKey(const KeyEvent& k) {
    if (k.vk == VK_RETURN) {
        std::wstring line = stdin_;
        stdin_.clear();
        out_.scrollToBottom();
        out_.appendStream(line + L"\n", OutKind::Input);  // эхо ввода в консоль
        if (!runner_.writeStdin(util::toUtf8(line) + "\n")) say(L"[!] stdin программы закрыт", OutKind::Warning, false);
        return;
    }
    if (k.vk == VK_BACK) {
        if (!stdin_.empty()) stdin_.pop_back();
        return;
    }
    if (k.ctrl && (k.vk == 'Z' || k.vk == 'D')) {
        runner_.closeStdin();
        say(L"[*] EOF отправлен в stdin", OutKind::Note, false);
        return;
    }
    if (k.ch >= 32 && !k.ctrl && !k.alt) stdin_ += k.ch;
}

void App::completeCommand() {
    std::wstring lead;
    std::wstring body = cmd_;
    if (!body.empty() && (body[0] == L':' || body[0] == L'/')) {
        lead = body.substr(0, 1);
        body.erase(0, 1);
    }
    if (body.find(L' ') != std::wstring::npos) return;
    std::vector<std::wstring> matches;
    for (const auto& c : commandNames())
        if (util::startsWith(c, util::toLower(body))) matches.push_back(c);
    if (matches.size() == 1) {
        cmd_ = (lead.empty() ? L":" : lead) + matches[0] + L" ";
        cmdCursor_ = cmd_.size();
    } else if (matches.size() > 1) {
        std::wstring list;
        for (const auto& m : matches) list += L":" + m + L"  ";
        say(list, OutKind::Note, false);
    }
}

static std::wstring readClipboard() {
    std::wstring r;
    if (!OpenClipboard(nullptr)) return r;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        auto* p = static_cast<const wchar_t*>(GlobalLock(h));
        if (p) {
            r = p;
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return r;
}

void App::pasteClipboard() {
    std::wstring text = readClipboard();
    if (text.empty()) return;
    if (focus_ == Focus::Editor) {
        editor_.insertText(text);
        return;
    }
    // В однострочные поля вставляем только первую строку
    std::wstring first = util::splitLines(text)[0];
    if (focus_ == Focus::Command) {
        cmd_.insert(cmdCursor_, first);
        cmdCursor_ += first.size();
    } else {
        stdin_ += first;
    }
}

// ===========================================================================
//  Команды
// ===========================================================================
void App::execCommand(const std::wstring& line) {
    std::wstring s = util::trim(line);
    if (s.empty()) return;
    if (history_.empty() || history_.back() != s) history_.push_back(s);
    histPos_ = history_.size();

    out_.scrollToBottom();
    out_.print(L"root@hack:~# " + s, OutKind::Accent);
    if (s[0] == L':' || s[0] == L'/') s.erase(0, 1);
    size_t sp = s.find(L' ');
    std::wstring cmd = util::toLower(sp == std::wstring::npos ? s : s.substr(0, sp));
    std::wstring arg = sp == std::wstring::npos ? L"" : util::trim(s.substr(sp + 1));

    if (cmd == L"run" || cmd == L"r") runCode();
    else if (cmd == L"new") {
        editor_.clear();
        editor_.setFileName(L"");
        say(L"[*] Editor wiped. Clean slate. (Ctrl+Z — вернуть)", OutKind::System);
    }
    else if (cmd == L"save" || cmd == L"w") cmdSave(arg);
    else if (cmd == L"load" || cmd == L"open" || cmd == L"e") cmdLoad(arg);
    else if (cmd == L"clear" || cmd == L"cls") out_.clear();
    else if (cmd == L"help" || cmd == L"h" || cmd == L"?") printHelp();
    else if (cmd == L"exit" || cmd == L"quit" || cmd == L"q") {
        if (state_ != RunState::Idle) stopProcess(KillReason::User);
        running_ = false;
    }
    else if (cmd == L"files" || cmd == L"ls" || cmd == L"dir") cmdFiles();
    else if (cmd == L"goto" || cmd == L"g") cmdGoto(arg);
    else if (cmd == L"stop" || cmd == L"kill") {
        if (state_ != RunState::Idle) stopProcess(KillReason::User);
        else say(L"[*] Nothing to kill.", OutKind::Dim);
    }
    else if (cmd == L"timeout") cmdTimeout(arg);
    else if (cmd == L"compiler" || cmd == L"cc") cmdCompiler(arg);
    else if (cmd == L"std") cmdStd(arg);
    else if (cmd == L"theme") cmdTheme(arg);
    else if (cmd == L"matrix") {
        ui_.resetSkip();
        ui_.matrixRain(4000);
        console_.flushInput();
        ui_.invalidate();
    }
    else if (cmd == L"about") printAbout();
    else say(L"[-] command not found: " + cmd + L". Наберите :help", OutKind::Error);
}

std::wstring App::resolveUserPath(const std::wstring& name, bool forLoad) const {
    std::wstring p = util::isAbsolutePath(name) ? name : util::joinPath(util::currentDir(), name);
    if (!hasExtension(p)) {
        if (!forLoad || !util::fileExists(p)) p += L".cpp";
    }
    return p;
}

void App::cmdSave(const std::wstring& arg) {
    std::wstring name = arg.empty() ? editor_.fileName() : arg;
    if (name.empty()) {
        say(L"[-] Использование: :save <имя>   (например :save hack.cpp)", OutKind::Error);
        return;
    }
    std::wstring path = arg.empty() ? name : resolveUserPath(name, false);
    if (!util::writeFileBytes(path, util::toUtf8(editor_.text()))) {
        say(L"[-] Не удалось сохранить " + path + L": " + util::lastErrorText(), OutKind::Error);
        return;
    }
    editor_.setFileName(path);
    editor_.setModified(false);
    say(L"[+] Saved " + std::to_wstring(editor_.lines().size()) + L" lines -> " + path, OutKind::Success);
}

void App::cmdLoad(const std::wstring& arg) {
    if (arg.empty()) {
        say(L"[-] Использование: :load <имя>   (список файлов — :files)", OutKind::Error);
        return;
    }
    std::wstring path = resolveUserPath(arg, true);
    std::string bytes;
    if (!util::fileExists(path) || !util::readFileBytes(path, bytes)) {
        say(L"[-] Файл не найден или недоступен: " + path, OutKind::Error);
        return;
    }
    editor_.setText(util::decodeFileText(bytes));
    editor_.setFileName(path);
    say(L"[+] Loaded " + std::to_wstring(editor_.lines().size()) + L" lines <- " + path, OutKind::Success);
}

void App::cmdFiles() {
    std::wstring dir = util::currentDir();
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(util::joinPath(dir, L"*.cpp").c_str(), &fd);
    say(L"[*] *.cpp in " + dir + L":", OutKind::System, false);
    if (h == INVALID_HANDLE_VALUE) {
        say(L"    (пусто)", OutKind::Dim, false);
        return;
    }
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        ULONGLONG size = (static_cast<ULONGLONG>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
        say(L"    " + std::wstring(fd.cFileName) + L"  (" + std::to_wstring(size) + L" bytes)", OutKind::Note, false);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

void App::cmdCompiler(const std::wstring& arg) {
    if (arg.empty()) {
        if (compiler_.all().empty()) {
            say(Compiler::installHelp(), OutKind::Warning, false);
            return;
        }
        say(L"[*] Найденные компиляторы:", OutKind::System, false);
        for (const auto& c : compiler_.all()) {
            bool cur = compiler_.current() && compiler_.current()->kind == c.kind;
            say(std::wstring(cur ? L"  > " : L"    ") + c.shortName() + L"  " + c.displayName() + L"  [" +
                    (c.vcvars.empty() ? c.exe : c.vcvars) + L"]",
                cur ? OutKind::Success : OutKind::Note, false);
        }
        say(L"    Переключение: :compiler gcc|msvc|clang", OutKind::Dim, false);
        return;
    }
    if (compiler_.select(arg)) say(L"[+] Compiler switched: " + compiler_.current()->displayName(), OutKind::Success);
    else say(L"[-] Компилятор '" + arg + L"' не найден. Список — :compiler", OutKind::Error);
}

void App::cmdTimeout(const std::wstring& arg) {
    if (!arg.empty()) {
        int v = _wtoi(arg.c_str());
        if (v < 0 || (v == 0 && arg != L"0")) {
            say(L"[-] Использование: :timeout <секунды>  (0 — без ограничения)", OutKind::Error);
            return;
        }
        timeoutSec_ = static_cast<DWORD>(v);
    }
    say(timeoutSec_ ? L"[*] Execution timeout: " + std::to_wstring(timeoutSec_) + L"s" : L"[*] Execution timeout: disabled",
        OutKind::System);
}

void App::cmdStd(const std::wstring& arg) {
    if (!arg.empty() && !compiler_.setStandard(_wtoi(arg.c_str()))) {
        say(L"[-] Использование: :std 17|20|23", OutKind::Error);
        return;
    }
    say(L"[*] Language standard: C++" + std::to_wstring(compiler_.standard()), OutKind::System);
}

void App::cmdTheme(const std::wstring& arg) {
    Theme t;
    if (!Theme::byName(util::toLower(arg), t)) {
        std::wstring list;
        for (const auto& n : Theme::names()) list += n + L" ";
        say(L"[-] Использование: :theme <" + util::trim(list) + L">", OutKind::Error);
        return;
    }
    theme_ = t;
    ui_.invalidate();
    say(L"[+] Theme loaded: " + t.name, OutKind::Success);
}

void App::cmdGoto(const std::wstring& arg) {
    int n = _wtoi(arg.c_str());
    if (n <= 0) {
        say(L"[-] Использование: :goto <номер строки>", OutKind::Error);
        return;
    }
    editor_.gotoLine(n);
}

void App::printHelp() {
    static const wchar_t* help =
        L"╔══[ HACKTERM :: HELP ]════════════════\n"
        L"  ГОРЯЧИЕ КЛАВИШИ\n"
        L"    F5 / Ctrl+Enter / Ctrl+R   скомпилировать и запустить\n"
        L"    Ctrl+C                     остановить компиляцию / программу\n"
        L"    Esc / F2                   редактор -> командная строка -> stdin программы\n"
        L"    F1                         эта справка\n"
        L"    F8                         к следующей ошибке компиляции\n"
        L"    Ctrl+S / Ctrl+O            сохранить / открыть файл\n"
        L"    Ctrl+Z / Ctrl+Y            отменить / повторить\n"
        L"    Ctrl+D / Ctrl+K            дублировать / удалить строку\n"
        L"    Ctrl+V                     вставить из буфера обмена\n"
        L"    Shift+Tab                  уменьшить отступ строки\n"
        L"    Ctrl+L                     очистить вывод\n"
        L"    Ctrl+Up/Down, Shift+PgUp/PgDn, колесо мыши — прокрутка\n"
        L"    Ctrl+Z (в режиме stdin)    послать EOF программе\n"
        L"    Ctrl+Q                     выход\n"
        L"  КОМАНДЫ (префикс ':' или '/', Tab — автодополнение, Up/Down — история)\n"
        L"    :run                 компиляция и запуск\n"
        L"    :new                 очистить редактор\n"
        L"    :save <имя>          сохранить код (без расширения добавится .cpp)\n"
        L"    :load <имя>          загрузить код\n"
        L"    :files               список *.cpp в текущей папке\n"
        L"    :clear               очистить вывод\n"
        L"    :goto <N>            перейти к строке\n"
        L"    :stop                убить запущенный процесс\n"
        L"    :timeout <сек>       лимит времени выполнения (0 — без лимита)\n"
        L"    :compiler [имя]      список / выбор компилятора: gcc, msvc, clang\n"
        L"    :std <17|20|23>      стандарт языка\n"
        L"    :theme <имя>         тема: matrix, amber, ice\n"
        L"    :matrix              ...\n"
        L"    :about               о программе\n"
        L"    :exit                выход\n"
        L"╚═══════════════════════════════════";
    say(help, OutKind::Note, false);
}

void App::printAbout() {
    say(L"HackTerm v1.0 — хакерский терминал с живой компиляцией C++\n"
        L"  C++17 · WinAPI · VT-консоль · без сторонних библиотек\n"
        L"  Рабочая папка сборки: " + workDir_ + L"\n"
        L"  Файл сессии: " + sessionPath_,
        OutKind::System, false);
}

void App::printWelcome() {
    say(L"[+] ACCESS GRANTED. Welcome back, root.", OutKind::Success);
    if (sessionRestored_) say(L"[*] Previous session restored.", OutKind::System);
    if (const CompilerInfo* c = compiler_.current()) {
        say(L"[*] Compiler online: " + c->displayName() + L"  [" + (c->vcvars.empty() ? c->exe : c->vcvars) + L"]",
            OutKind::System);
    } else {
        say(Compiler::installHelp(), OutKind::Warning, false);
    }
    say(L"[*] F5 / Ctrl+Enter — compile & run · Esc — command line · F1 — help", OutKind::Dim);
}

// ===========================================================================
//  Сборка и запуск
// ===========================================================================
void App::runCode() {
    if (state_ != RunState::Idle) {
        say(L"[!] Busy: процесс ещё работает. Ctrl+C — остановить.", OutKind::Warning);
        return;
    }
    const CompilerInfo* c = compiler_.current();
    if (!c) {
        say(Compiler::installHelp(), OutKind::Warning, false);
        return;
    }
    saveSession();
    std::wstring err;
    if (!compiler_.writeSources(workDir_, editor_.text(), err)) {
        say(L"[-] Не удалось записать исходники: " + err, OutKind::Error);
        return;
    }
    // Каждая сборка — новое имя exe: старый файл может быть ещё заблокирован антивирусом
    if (!exePath_.empty()) DeleteFileW(exePath_.c_str());
    std::wstring exeName = L"prog_" + std::to_wstring(++buildNo_) + L".exe";
    exePath_ = util::joinPath(workDir_, exeName);
    DeleteFileW(exePath_.c_str());

    editor_.clearMarkers();
    compileBuf_.clear();
    say(L"[*] Compiling main.cpp :: " + c->displayName() + L" -std=c++" + std::to_wstring(compiler_.standard()) + L" ...",
        OutKind::System);

    if (!runner_.start(compiler_.buildCommand(exeName), workDir_, err)) {
        say(L"[-] Не удалось запустить компилятор: " + err, OutKind::Error);
        return;
    }
    runner_.closeStdin();  // компилятору ввод не нужен
    state_ = RunState::Compiling;
    kill_ = KillReason::None;
    stageStart_ = GetTickCount64();
}

void App::stopProcess(KillReason reason) {
    if (!runner_.active()) return;
    if (kill_ == KillReason::None) kill_ = reason;
    runner_.kill();  // итоговое сообщение выведет onCompileFinished / onRunFinished
}

bool App::pollProcesses() {
    if (state_ == RunState::Idle) return false;
    std::vector<Runner::Chunk> chunks;
    DWORD code = 0;
    bool done = runner_.poll(chunks, code);
    bool changed = !chunks.empty() || done;

    if (state_ == RunState::Compiling) {
        for (const auto& ch : chunks) compileBuf_ += ch.data;
        if (done) onCompileFinished(code);
        else if (runner_.elapsedMs() > 60000) stopProcess(KillReason::Timeout);  // зависший компилятор
        return changed;
    }

    // Running: вывод программы появляется в реальном времени
    if (!chunks.empty()) out_.scrollToBottom();
    for (const auto& ch : chunks) {
        bool isOut = ch.stream == Runner::Stream::Out;
        std::wstring text = (isOut ? decOut_ : decErr_).feed(ch.data);
        out_.appendStream(text, isOut ? OutKind::ProgOut : OutKind::ProgErr);
    }
    if (done) {
        onRunFinished(code);
    } else if (timeoutSec_ > 0 && runner_.elapsedMs() > static_cast<ULONGLONG>(timeoutSec_) * 1000) {
        stopProcess(KillReason::Timeout);
    }
    return changed;
}

void App::onCompileFinished(DWORD code) {
    state_ = RunState::Idle;
    ULONGLONG elapsed = GetTickCount64() - stageStart_;
    if (kill_ != KillReason::None) {
        say(kill_ == KillReason::Timeout ? L"[!] Compilation timed out (60s) — aborted." : L"[!] Compilation aborted by user.",
            OutKind::Warning);
        kill_ = KillReason::None;
        return;
    }

    // Разбор диагностики: красные ошибки, жёлтые предупреждения, маркеры в редакторе
    auto diags = compiler_.parse(util::decodeBytes(compileBuf_));
    std::map<int, int> markers;
    int errors = 0, warnings = 0, firstErr = 0;
    for (const auto& d : diags) {
        OutKind kind = OutKind::Normal;
        switch (d.sev) {
            case Severity::Error:   kind = OutKind::Error; break;
            case Severity::Warning: kind = OutKind::Warning; break;
            case Severity::Note:    kind = OutKind::Note; break;
            case Severity::None:    kind = d.context ? OutKind::Context : OutKind::Dim; break;
        }
        out_.print(d.text, kind);
        if (d.line > 0 && d.line <= static_cast<int>(editor_.lines().size())) {
            int r = d.line - 1;
            if (d.sev == Severity::Error) {
                markers[r] = Editor::MarkError;
                ++errors;
                if (!firstErr) firstErr = d.line;
            } else if (d.sev == Severity::Warning) {
                if (!markers.count(r)) markers[r] = Editor::MarkWarning;
                ++warnings;
            }
        } else if (d.sev == Severity::Error) {
            ++errors;
        }
    }
    editor_.setMarkers(markers);

    if (code != 0 || !util::fileExists(exePath_)) {
        say(L"[-] BUILD FAILED: " + std::to_wstring(errors) + L" error(s), " + std::to_wstring(warnings) +
                L" warning(s), exit code " + std::to_wstring(code),
            OutKind::Error);
        if (firstErr) {
            editor_.gotoLine(firstErr);
            say(L"[>] Курсор на строке " + std::to_wstring(firstErr) + L" (F8 — следующая ошибка)", OutKind::Note);
        }
        return;
    }
    say(L"[+] Build OK in " + util::formatSeconds(elapsed) +
            (warnings ? L" (" + std::to_wstring(warnings) + L" warning(s))" : std::wstring()),
        OutKind::Success);
    say(L"┌─[ exec " + util::baseName(exePath_) + L" ]──────────",
        OutKind::Accent, false);

    decOut_.reset();
    decErr_.reset();
    std::wstring err;
    if (!runner_.start(L"\"" + exePath_ + L"\"", workDir_, err)) {
        say(L"[-] Не удалось запустить программу: " + err, OutKind::Error);
        return;
    }
    state_ = RunState::Running;
    kill_ = KillReason::None;
    stageStart_ = GetTickCount64();
    focus_ = Focus::Stdin;  // ввод с клавиатуры идёт в программу
    stdin_.clear();
}

void App::onRunFinished(DWORD code) {
    ULONGLONG elapsed = GetTickCount64() - stageStart_;
    out_.appendStream(decOut_.flush(), OutKind::ProgOut);
    out_.appendStream(decErr_.flush(), OutKind::ProgErr);
    out_.closeOpenLine();
    state_ = RunState::Idle;
    if (focus_ == Focus::Stdin) focus_ = Focus::Editor;
    stdin_.clear();

    std::wstring footer = L"└─[ ";
    if (kill_ == KillReason::User) {
        say(footer + L"killed by user (Ctrl+C) after " + util::formatSeconds(elapsed) + L" ]", OutKind::Warning, false);
    } else if (kill_ == KillReason::Timeout) {
        say(footer + L"TIME LIMIT EXCEEDED: " + std::to_wstring(timeoutSec_) + L"s — process killed ]", OutKind::Error,
            false);
        say(L"[*] Лимит меняется командой :timeout <сек> (0 — без ограничения)", OutKind::Dim);
    } else {
        std::wstring desc = Runner::describeExitCode(code);
        std::wstring codeText = (code > 0xFFFF) ? util::hex32(code) : std::to_wstring(code);
        say(footer + L"exit code " + codeText + (desc.empty() ? L"" : L" — " + desc) + L" · " +
                util::formatSeconds(elapsed) + L" ]",
            code == 0 ? OutKind::Success : OutKind::Error, false);
    }
    kill_ = KillReason::None;
    DeleteFileW(exePath_.c_str());  // может не получиться сразу — тогда уберём при выходе
}

void App::nextError() {
    int r = editor_.nextErrorRow(editor_.row());
    if (r < 0) {
        say(L"[*] Ошибок нет.", OutKind::Dim);
        return;
    }
    editor_.gotoLine(r + 1);
    focus_ = Focus::Editor;
}

// ===========================================================================
//  Сессия и рабочая папка
// ===========================================================================
void App::loadSession() {
    std::wstring base = util::getEnv(L"LOCALAPPDATA");
    if (base.empty()) base = util::getEnv(L"TEMP");
    std::wstring dir = util::joinPath(base, L"HackTerm");
    CreateDirectoryW(dir.c_str(), nullptr);
    sessionPath_ = util::joinPath(dir, L"session.cpp");

    std::string bytes;
    if (util::readFileBytes(sessionPath_, bytes) && !util::trim(util::decodeFileText(bytes)).empty()) {
        editor_.setText(util::decodeFileText(bytes));
        sessionRestored_ = true;
    } else {
        editor_.setText(kTemplate);  // первый запуск — шаблон Hello World
    }
}

void App::saveSession() {
    if (!sessionPath_.empty()) util::writeFileBytes(sessionPath_, util::toUtf8(editor_.text()));
}

void App::emergencySave() { saveSession(); }

void App::prepareWorkDir() {
    wchar_t tmp[MAX_PATH + 1];
    DWORD n = GetTempPathW(MAX_PATH + 1, tmp);
    std::wstring base = util::joinPath(n ? std::wstring(tmp, n) : util::currentDir(), L"HackTerm");
    CreateDirectoryW(base.c_str(), nullptr);
    workDir_ = util::joinPath(base, L"run_" + std::to_wstring(GetCurrentProcessId()));
    CreateDirectoryW(workDir_.c_str(), nullptr);
}

void App::cleanupWorkDir() {
    if (workDir_.empty()) return;
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(util::joinPath(workDir_, L"*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            DeleteFileW(util::joinPath(workDir_, fd.cFileName).c_str());
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(workDir_.c_str());
}
