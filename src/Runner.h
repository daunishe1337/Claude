#pragma once
// ============================================================================
//  Runner.h — запуск внешнего процесса (компилятора или собранной программы)
//  с перехватом stdout/stderr через пайпы, передачей stdin, Job Object
//  для гарантированного завершения всего дерева процессов.
// ============================================================================
#include "Common.h"

#include <deque>

class Runner {
public:
    enum class Stream { Out, Err };
    struct Chunk {
        Stream stream;
        std::string data;
    };

    Runner() = default;
    ~Runner();
    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;

    // Асинхронный запуск. Вывод читают фоновые потоки, забирать его — через poll()
    bool start(const std::wstring& cmdline, const std::wstring& cwd, std::wstring& error);
    bool active() const { return process_.valid(); }

    // Забирает накопленный вывод. Возвращает true, когда процесс завершён
    // и все ресурсы освобождены (exitCode заполнен).
    bool poll(std::vector<Chunk>& out, DWORD& exitCode);

    void kill();                                 // убивает всё дерево процессов; финализация — в poll()
    bool writeStdin(const std::string& data);
    void closeStdin() { stdinW_.reset(); }       // EOF для программы
    bool stdinOpen() const { return stdinW_.valid(); }
    ULONGLONG elapsedMs() const { return active() ? GetTickCount64() - startTick_ : 0; }

    // Синхронный запуск с захватом вывода (для определения версий компиляторов)
    static bool capture(const std::wstring& cmdline, const std::wstring& cwd, DWORD timeoutMs,
                        std::string& output, DWORD& exitCode);
    static std::wstring describeExitCode(DWORD code);

private:
    struct Reader {
        Runner* owner = nullptr;
        HANDLE pipe = nullptr;
        Stream stream = Stream::Out;
    };
    static DWORD WINAPI readerProc(LPVOID param);
    void push(Stream s, const char* data, DWORD n);
    void drain(std::vector<Chunk>& out);
    void joinReaders();
    void release();

    util::Handle process_, job_, stdinW_, outR_, errR_;
    util::Handle threadOut_, threadErr_;
    Reader rOut_, rErr_;
    SRWLOCK lock_ = SRWLOCK_INIT;
    std::deque<Chunk> queue_;
    ULONGLONG startTick_ = 0;
};
