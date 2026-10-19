// ============================================================================
//  selftest.cpp — самопроверка не-UI частей HackTerm (Runner, Compiler, Editor,
//  OutputPane, декодер). Сборка: см. README, раздел «Тесты».
//  Запуск: selftest.exe <путь к child.exe>
// ============================================================================
#include <cstdio>
#include <string>

#include "../src/Compiler.h"
#include "../src/Editor.h"
#include "../src/OutputPane.h"
#include "../src/Runner.h"

#include <shellapi.h>  // CommandLineToArgvW

static int g_fail = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_fail;                                                  \
        }                                                              \
    } while (0)

// Ждёт завершения процесса, собирая весь вывод; firstChunkBeforeExit — пришёл ли вывод до выхода
static DWORD waitAll(Runner& r, std::string& out, std::string& err, ULONGLONG timeoutMs, bool* firstChunkEarly = nullptr) {
    ULONGLONG end = GetTickCount64() + timeoutMs;
    DWORD code = 0;
    for (;;) {
        std::vector<Runner::Chunk> ch;
        bool done = r.poll(ch, code);
        for (auto& c : ch) {
            if (firstChunkEarly && out.empty() && err.empty() && !done) *firstChunkEarly = true;
            (c.stream == Runner::Stream::Out ? out : err) += c.data;
        }
        if (done) return code;
        if (GetTickCount64() > end) { r.kill(); end = GetTickCount64() + 5000; }
        Sleep(5);
    }
}

static void testRunner(const std::wstring& child) {
    // 1. stdout без перевода строки приходит сразу, stdin доходит, stderr отдельно, код выхода
    {
        Runner r;
        std::wstring e;
        CHECK(r.start(L"\"" + child + L"\" echo", L"", e));
        // ждём приглашения до отправки stdin
        std::string got;
        ULONGLONG end = GetTickCount64() + 3000;
        while (got.find("Enter your name") == std::string::npos && GetTickCount64() < end) {
            std::vector<Runner::Chunk> ch;
            DWORD c;
            r.poll(ch, c);
            for (auto& x : ch) got += x.data;
            Sleep(5);
        }
        CHECK(got == "Enter your name: ");
        CHECK(r.writeStdin("Neo\n"));
        std::string out, err;
        DWORD code = waitAll(r, out, err, 5000);
        std::printf("  echo: out=[%s] err=[%s]\n", out.c_str(), err.c_str());
        CHECK(code == 7);
        CHECK(out == "Access granted, Neo.\r\n");  // текстовый режим CRT: \n -> \r\n
        CHECK(err == "warn on stderr\r\n");
        CHECK(!r.active());
    }
    // 2. kill зависшего процесса
    {
        Runner r;
        std::wstring e;
        CHECK(r.start(L"\"" + child + L"\" hang", L"", e));
        Sleep(200);
        CHECK(r.active());
        r.kill();
        std::string out, err;
        waitAll(r, out, err, 5000);
        CHECK(!r.active());
    }
    // 3. падение: код ACCESS_VIOLATION
    {
        Runner r;
        std::wstring e;
        CHECK(r.start(L"\"" + child + L"\" crash", L"", e));
        std::string out, err;
        DWORD code = waitAll(r, out, err, 5000);
        std::printf("  crash exit code = 0x%08lX (%ls)\n", static_cast<unsigned long>(code),
                    Runner::describeExitCode(code).c_str());
        CHECK(code != 0);
    }
    // 4. UTF-8 и потоковый декодер, порезанный посередине символа
    {
        Runner r;
        std::wstring e;
        CHECK(r.start(L"\"" + child + L"\" utf8", L"", e));
        std::string out, err;
        waitAll(r, out, err, 5000);
        util::StreamDecoder d;
        std::wstring w = d.feed(out.substr(0, 3));  // обрываем на середине второго символа
        w += d.feed(out.substr(3));
        w += d.flush();
        CHECK(w == L"\u041F\u0440\u0438\u0432\u0435\u0442\r\n");
    }
    // 5. несуществующая программа
    {
        Runner r;
        std::wstring e;
        CHECK(!r.start(L"\"C:\\nope\\nothing.exe\"", L"", e));
        CHECK(!e.empty());
        CHECK(!r.active());
    }
    // 6. capture
    {
        std::string out;
        DWORD code = 0;
        CHECK(Runner::capture(L"\"" + child + L"\" utf8", L"", 5000, out, code));
        CHECK(code == 0 && out.size() == 14);
    }
}

static void testParse() {
    Compiler c;
    auto d = c.parse(
        L"main.cpp: In function 'int main()':\n"
        L"main.cpp:7:5: error: 'cout' was not declared in this scope\n"
        L"    7 |     cout << 1;\n"
        L"      |     ^~~~\n"
        L"main.cpp:9:9: warning: unused variable 'x' [-Wunused-variable]\n"
        L"main.cpp(12): error C2065: 'y': undeclared identifier\n"
        L"main.cpp(14,3): warning C4101: 'z': unreferenced local variable\n"
        L"main.cpp\n"
        L"collect2.exe: error: ld returned 1 exit status\n");
    CHECK(d.size() == 8);
    if (d.size() == 8) {
        CHECK(d[0].sev == Severity::None && d[0].line == 0);
        CHECK(d[1].sev == Severity::Error && d[1].line == 7);
        CHECK(d[2].context && d[3].context);
        CHECK(d[4].sev == Severity::Warning && d[4].line == 9);
        CHECK(d[5].sev == Severity::Error && d[5].line == 12);
        CHECK(d[6].sev == Severity::Warning && d[6].line == 14);
        CHECK(d[7].sev == Severity::Error && d[7].line == 0);
    }
}

static KeyEvent key(WORD vk, wchar_t ch = 0, bool ctrl = false) {
    KeyEvent k;
    k.vk = vk;
    k.ch = ch;
    k.ctrl = ctrl;
    return k;
}

static void type(Editor& e, const std::wstring& s) {
    for (wchar_t c : s) e.handleKey(c == L'\n' ? key(VK_RETURN, L'\r') : key(0, c), 20, false);
}

static void testEditor() {
    Editor e;
    type(e, L"int main() {\nreturn 0;\n}");
    CHECK(e.text() == L"int main() {\n    return 0;\n}\n");   // автоотступ и авто-dedent '}'
    e.handleKey(key('Z', 0, true), 20, false);                 // undo
    e.handleKey(key('Y', 0, true), 20, false);                 // redo
    CHECK(e.text() == L"int main() {\n    return 0;\n}\n");
    e.setText(L"a\tb\r\nc");
    CHECK(e.lines().size() == 2 && e.lines()[0] == L"a   b");
    e.handleKey(key(VK_END), 20, false);
    e.handleKey(key(VK_DELETE), 20, false);                    // склейка строк
    CHECK(e.text() == L"a   bc\n");
    e.handleKey(key(VK_HOME), 20, false);
    e.handleKey(key(VK_TAB, L'\t'), 20, false);
    CHECK(e.lines()[0] == L"    a   bc");
    e.handleKey(key(VK_BACK), 20, false);                      // backspace в отступе — до табстопа
    CHECK(e.lines()[0] == L"a   bc");
    e.insertText(L"x\r\ny");
    CHECK(e.text() == L"x\nya   bc\n");
    std::map<int, int> m{{0, Editor::MarkError}, {1, Editor::MarkError}};
    e.setMarkers(m);
    CHECK(e.nextErrorRow(0) == 1 && e.nextErrorRow(1) == 0);
}

static void testOutput() {
    OutputPane o;
    o.appendStream(L"progress 10%\rprogress 100%\nline2\x1b[31m red\x1b[0m\n", OutKind::ProgOut);
    auto rows = o.visualRows(80, 10);
    CHECK(rows.size() == 2);
    if (rows.size() == 2) {
        CHECK(rows[0].text == L"progress 100%");
        CHECK(rows[1].text == L"line2 red");
    }
    o.print(L"typed message", OutKind::System, true);
    CHECK(o.typing());
    while (o.tick(3)) {}
    CHECK(!o.typing());
    o.clear();
    o.print(std::wstring(25, L'x'), OutKind::Normal);
    CHECK(o.visualRows(10, 10).size() == 3);  // перенос длинной строки
}

int wmainImpl(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::printf("usage: selftest <child.exe>\n");
        return 2;
    }
    testParse();
    testEditor();
    testOutput();
    testRunner(argv[1]);
    std::printf(g_fail ? "%d FAILED\n" : "ALL TESTS PASSED\n", g_fail);
    return g_fail ? 1 : 0;
}

int main() {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int rc = wmainImpl(argc, argv);
    LocalFree(argv);
    return rc;
}
