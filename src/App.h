#pragma once
// ============================================================================
//  App.h — приложение: главный цикл, обработка клавиш, команды,
//  конвейер «сохранить → скомпилировать → запустить».
// ============================================================================
#include "Compiler.h"
#include "Console.h"
#include "Editor.h"
#include "OutputPane.h"
#include "Runner.h"
#include "Theme.h"
#include "UI.h"

class App {
public:
    App();
    int run();
    void emergencySave();   // вызывается из обработчика закрытия окна консоли

private:
    enum class KillReason { None, User, Timeout };

    // --- ввод ---
    void handleInput(const InputBatch& b);
    void handleKey(const KeyEvent& k, bool paste);
    void handleCommandKey(const KeyEvent& k);
    void handleStdinKey(const KeyEvent& k);
    void handleWheel(const MouseWheel& w);
    void toggleFocus();
    void completeCommand();
    void pasteClipboard();

    // --- команды ---
    void execCommand(const std::wstring& line);
    void cmdSave(const std::wstring& arg);
    void cmdLoad(const std::wstring& arg);
    void cmdFiles();
    void cmdCompiler(const std::wstring& arg);
    void cmdTimeout(const std::wstring& arg);
    void cmdStd(const std::wstring& arg);
    void cmdTheme(const std::wstring& arg);
    void cmdGoto(const std::wstring& arg);
    void printHelp();
    void printAbout();
    void printWelcome();

    // --- сборка и запуск ---
    void runCode();
    void stopProcess(KillReason reason);
    bool pollProcesses();
    void onCompileFinished(DWORD code);
    void onRunFinished(DWORD code);
    void nextError();

    // --- прочее ---
    void render();
    StatusInfo makeStatus() const;
    void say(const std::wstring& text, OutKind kind, bool typed = true);
    void loadSession();
    void saveSession();
    void prepareWorkDir();
    void cleanupWorkDir();
    std::wstring resolveUserPath(const std::wstring& name, bool forLoad) const;

    // Порядок важен: UI хранит ссылки на console_ и theme_
    Theme theme_;
    Console console_;
    UI ui_;
    Editor editor_;
    OutputPane out_;
    Compiler compiler_;
    Runner runner_;

    RunState state_ = RunState::Idle;
    KillReason kill_ = KillReason::None;
    Focus focus_ = Focus::Editor;

    std::wstring cmd_;
    size_t cmdCursor_ = 0;
    std::vector<std::wstring> history_;
    size_t histPos_ = 0;
    std::wstring stdin_;

    std::string compileBuf_;
    util::StreamDecoder decOut_, decErr_;
    std::wstring workDir_, exePath_, sessionPath_;
    unsigned buildNo_ = 0;
    ULONGLONG stageStart_ = 0;
    DWORD timeoutSec_ = 10;
    bool sessionRestored_ = false;

    bool running_ = true;
    bool blink_ = true;
    ULONGLONG blinkTick_ = 0;
};
