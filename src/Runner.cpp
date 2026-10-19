// ============================================================================
//  Runner.cpp
// ============================================================================
#include "Runner.h"

namespace {

// Job Object: при закрытии хэндла (в т.ч. при падении HackTerm) все процессы
// задания будут убиты; необработанное исключение не вызывает окно WER.
HANDLE createJob() {
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) return nullptr;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info));
    return job;
}

// Создаёт процесс без окна, в приостановленном состоянии, помещает в Job и запускает.
// Наследуются ТОЛЬКО переданные хэндлы (PROC_THREAD_ATTRIBUTE_HANDLE_LIST),
// чтобы пайпы одного процесса не утекали в другой.
bool spawn(const std::wstring& cmdline, const std::wstring& cwd, HANDLE hIn, HANDLE hOut, HANDLE hErr,
           HANDLE job, PROCESS_INFORMATION& pi, std::wstring& error) {
    std::vector<HANDLE> inherit;
    for (HANDLE h : {hIn, hOut, hErr}) {
        if (!h) continue;
        bool dup = false;
        for (HANDLE x : inherit) dup = dup || (x == h);
        if (!dup) inherit.push_back(h);
    }

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = hIn;
    si.StartupInfo.hStdOutput = hOut;
    si.StartupInfo.hStdError = hErr;

    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    std::vector<char> attrBuf(attrSize);
    auto attrs = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrBuf.data());
    if (!InitializeProcThreadAttributeList(attrs, 1, 0, &attrSize)) {
        error = L"InitializeProcThreadAttributeList: " + util::lastErrorText();
        return false;
    }
    if (!inherit.empty() &&
        !UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit.data(),
                                   inherit.size() * sizeof(HANDLE), nullptr, nullptr)) {
        error = L"UpdateProcThreadAttribute: " + util::lastErrorText();
        DeleteProcThreadAttributeList(attrs);
        return false;
    }
    si.lpAttributeList = attrs;

    std::vector<wchar_t> cmd(cmdline.begin(), cmdline.end());
    cmd.push_back(L'\0');
    DWORD flags = CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT;
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, flags, nullptr,
                             cwd.empty() ? nullptr : cwd.c_str(), &si.StartupInfo, &pi);
    DWORD err = GetLastError();
    DeleteProcThreadAttributeList(attrs);
    if (!ok) {
        error = util::lastErrorText(err);
        return false;
    }
    // Если процесс нельзя поместить в Job (очень старая Windows) — убьём через TerminateProcess
    if (job) AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    pi.hThread = nullptr;
    return true;
}

struct CaptureCtx {
    HANDLE pipe;
    std::string* out;
};

DWORD WINAPI captureProc(LPVOID p) {
    auto* c = static_cast<CaptureCtx*>(p);
    char buf[4096];
    DWORD n = 0;
    while (ReadFile(c->pipe, buf, sizeof(buf), &n, nullptr) && n > 0) c->out->append(buf, n);
    return 0;
}

} // namespace

// ---------------------------------------------------------------------------
Runner::~Runner() {
    if (process_) {
        kill();
        WaitForSingleObject(process_.get(), 3000);
        stdinW_.reset();
        joinReaders();
        release();
    }
}

bool Runner::start(const std::wstring& cmdline, const std::wstring& cwd, std::wstring& error) {
    if (process_) {
        error = L"process is already running";
        return false;
    }
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    util::Handle inR, inW, outR, outW, errR, errW;
    if (!CreatePipe(inR.put(), inW.put(), &sa, 1 << 20) || !CreatePipe(outR.put(), outW.put(), &sa, 0) ||
        !CreatePipe(errR.put(), errW.put(), &sa, 0)) {
        error = L"CreatePipe: " + util::lastErrorText();
        return false;
    }
    // Наши концы пайпов не должны наследоваться дочерним процессом
    SetHandleInformation(inW.get(), HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(outR.get(), HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(errR.get(), HANDLE_FLAG_INHERIT, 0);

    util::Handle job(createJob());
    PROCESS_INFORMATION pi{};
    if (!spawn(cmdline, cwd, inR.get(), outW.get(), errW.get(), job.get(), pi, error)) return false;

    process_.reset(pi.hProcess);
    job_ = std::move(job);
    stdinW_ = std::move(inW);
    outR_ = std::move(outR);
    errR_ = std::move(errR);
    // Детские концы (inR, outW, errW) закроются при выходе из функции —
    // это обязательно, иначе ReadFile никогда не получит EOF.
    startTick_ = GetTickCount64();

    rOut_ = {this, outR_.get(), Stream::Out};
    rErr_ = {this, errR_.get(), Stream::Err};
    threadOut_.reset(CreateThread(nullptr, 0, readerProc, &rOut_, 0, nullptr));
    threadErr_.reset(CreateThread(nullptr, 0, readerProc, &rErr_, 0, nullptr));
    if (!threadOut_ || !threadErr_) {
        error = L"CreateThread: " + util::lastErrorText();
        kill();
        WaitForSingleObject(process_.get(), 3000);
        stdinW_.reset();
        joinReaders();
        release();
        return false;
    }
    return true;
}

DWORD WINAPI Runner::readerProc(LPVOID param) {
    auto* r = static_cast<Reader*>(param);
    char buf[4096];
    for (;;) {
        DWORD n = 0;
        if (!ReadFile(r->pipe, buf, sizeof(buf), &n, nullptr) || n == 0) break;  // EOF или broken pipe
        r->owner->push(r->stream, buf, n);
    }
    return 0;
}

void Runner::push(Stream s, const char* data, DWORD n) {
    AcquireSRWLockExclusive(&lock_);
    if (!queue_.empty() && queue_.back().stream == s && queue_.back().data.size() < 65536)
        queue_.back().data.append(data, n);
    else
        queue_.push_back({s, std::string(data, n)});
    ReleaseSRWLockExclusive(&lock_);
}

void Runner::drain(std::vector<Chunk>& out) {
    AcquireSRWLockExclusive(&lock_);
    while (!queue_.empty()) {
        out.push_back(std::move(queue_.front()));
        queue_.pop_front();
    }
    ReleaseSRWLockExclusive(&lock_);
}

bool Runner::poll(std::vector<Chunk>& out, DWORD& exitCode) {
    drain(out);
    if (!process_) return false;
    if (WaitForSingleObject(process_.get(), 0) != WAIT_OBJECT_0) return false;

    exitCode = 0;
    GetExitCodeProcess(process_.get(), &exitCode);
    // Добиваем «осиротевших» потомков: они могли унаследовать пайпы и держать их открытыми
    if (job_) TerminateJobObject(job_.get(), exitCode);
    stdinW_.reset();
    joinReaders();
    drain(out);
    release();
    return true;
}

void Runner::kill() {
    if (!process_) return;
    if (job_) TerminateJobObject(job_.get(), 1);
    TerminateProcess(process_.get(), 1);  // на случай, если процесс не попал в Job
}

bool Runner::writeStdin(const std::string& data) {
    if (!stdinW_) return false;
    size_t done = 0;
    while (done < data.size()) {
        DWORD n = 0;
        if (!WriteFile(stdinW_.get(), data.data() + done, static_cast<DWORD>(data.size() - done), &n, nullptr)) {
            stdinW_.reset();  // программа закрыла stdin
            return false;
        }
        done += n;
    }
    return true;
}

void Runner::joinReaders() {
    HANDLE hs[2];
    DWORD n = 0;
    if (threadOut_) hs[n++] = threadOut_.get();
    if (threadErr_) hs[n++] = threadErr_.get();
    if (n > 0 && WaitForMultipleObjects(n, hs, TRUE, 2000) == WAIT_TIMEOUT) {
        // Кто-то всё ещё держит пайп — прерываем блокирующий ReadFile
        for (DWORD i = 0; i < n; ++i) CancelSynchronousIo(hs[i]);
        WaitForMultipleObjects(n, hs, TRUE, 2000);
    }
    threadOut_.reset();
    threadErr_.reset();
}

void Runner::release() {
    stdinW_.reset();
    outR_.reset();
    errR_.reset();
    process_.reset();
    job_.reset();  // KILL_ON_JOB_CLOSE: всё, что осталось в задании, будет завершено
}

// ---------------------------------------------------------------------------
bool Runner::capture(const std::wstring& cmdline, const std::wstring& cwd, DWORD timeoutMs,
                     std::string& output, DWORD& exitCode) {
    output.clear();
    exitCode = static_cast<DWORD>(-1);
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    util::Handle outR, outW;
    if (!CreatePipe(outR.put(), outW.put(), &sa, 0)) return false;
    SetHandleInformation(outR.get(), HANDLE_FLAG_INHERIT, 0);

    util::Handle job(createJob());
    PROCESS_INFORMATION pi{};
    std::wstring err;
    if (!spawn(cmdline, cwd, nullptr, outW.get(), outW.get(), job.get(), pi, err)) return false;
    util::Handle proc(pi.hProcess);
    outW.reset();

    CaptureCtx ctx{outR.get(), &output};
    util::Handle th(CreateThread(nullptr, 0, captureProc, &ctx, 0, nullptr));
    if (!th) captureProc(&ctx);  // поток не создался — читаем синхронно

    if (WaitForSingleObject(proc.get(), timeoutMs) == WAIT_TIMEOUT) {
        TerminateJobObject(job.get(), 1);
        TerminateProcess(proc.get(), 1);
        WaitForSingleObject(proc.get(), 2000);
    }
    GetExitCodeProcess(proc.get(), &exitCode);
    TerminateJobObject(job.get(), exitCode);
    if (th && WaitForSingleObject(th.get(), 2000) == WAIT_TIMEOUT) {
        CancelSynchronousIo(th.get());
        WaitForSingleObject(th.get(), 2000);
    }
    return true;
}

std::wstring Runner::describeExitCode(DWORD code) {
    switch (code) {
        case 0xC0000005: return L"ACCESS_VIOLATION — обращение к неверному адресу (segfault)";
        case 0xC00000FD: return L"STACK_OVERFLOW — переполнение стека (бесконечная рекурсия?)";
        case 0xC0000094: return L"INTEGER_DIVIDE_BY_ZERO — целочисленное деление на ноль";
        case 0xC000008E: return L"FLOAT_DIVIDE_BY_ZERO";
        case 0xC0000409: return L"STACK_BUFFER_OVERRUN / abort / fail-fast";
        case 0xC0000374: return L"HEAP_CORRUPTION — повреждение кучи";
        case 0xC000001D: return L"ILLEGAL_INSTRUCTION";
        case 0xC0000017: return L"NO_MEMORY";
        case 0xC0000135: return L"DLL_NOT_FOUND — не найдена DLL (libstdc++?)";
        case 0xC000007B: return L"INVALID_IMAGE_FORMAT";
        case 0xC000013A: return L"CONTROL_C_EXIT";
        case 0x80000003: return L"BREAKPOINT";
        case 0xE06D7363: return L"необработанное C++ исключение";
        case 3:          return L"abort() / terminate()";
        default:         return L"";
    }
}
