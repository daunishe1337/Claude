#pragma once
// ============================================================================
//  Common.h — общие заголовки WinAPI и мелкие утилиты HackTerm
// ============================================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00   // Windows 10: нужен для VT-последовательностей и списка наследуемых хэндлов
#endif
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace util {

// --- Кодировки ---------------------------------------------------------------
std::wstring fromUtf8(const std::string& s);
std::string  toUtf8(const std::wstring& s);
// Байты из внешнего процесса: сначала строгий UTF-8, при ошибке — OEM-кодировка консоли
std::wstring decodeBytes(const std::string& s);
// Байты файла: убирает BOM, строгий UTF-8, при ошибке — ANSI-кодировка (cp1251 и т.п.)
std::wstring decodeFileText(std::string s);

// Потоковый декодер UTF-8: не рвёт многобайтовые символы на границе чанков из пайпа
class StreamDecoder {
public:
    std::wstring feed(const std::string& bytes);
    std::wstring flush();
    void reset() { pending_.clear(); }
private:
    std::string pending_;
};

// --- Строки ------------------------------------------------------------------
std::wstring trim(const std::wstring& s);
std::wstring toLower(std::wstring s);
bool startsWith(const std::wstring& s, const std::wstring& prefix);
std::vector<std::wstring> splitLines(const std::wstring& text);  // \r\n, \n, \r; минимум одна строка
std::wstring expandTabs(const std::wstring& s, int tabSize = 4);
std::wstring formatSeconds(ULONGLONG ms);                        // "1.23s"
std::wstring hex32(DWORD v);                                     // "0xC0000005"

// --- Файлы и окружение -------------------------------------------------------
bool fileExists(const std::wstring& path);
bool dirExists(const std::wstring& path);
bool readFileBytes(const std::wstring& path, std::string& out);
bool writeFileBytes(const std::wstring& path, const std::string& data);
std::wstring joinPath(const std::wstring& a, const std::wstring& b);
std::wstring dirOf(const std::wstring& path);
std::wstring baseName(const std::wstring& path);
std::wstring getEnv(const wchar_t* name);
std::wstring expandEnv(const std::wstring& s);
std::wstring currentDir();
bool isAbsolutePath(const std::wstring& p);
std::wstring lastErrorText(DWORD code = GetLastError());

// --- RAII-обёртка для HANDLE: гарантирует CloseHandle и исключает утечки ------
class Handle {
public:
    Handle() = default;
    explicit Handle(HANDLE h) : h_(h) {}
    ~Handle() { reset(); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& o) noexcept : h_(o.release()) {}
    Handle& operator=(Handle&& o) noexcept {
        if (this != &o) reset(o.release());
        return *this;
    }
    HANDLE get() const { return h_; }
    // Закрывает текущий хэндл и отдаёт адрес для функций вида CreatePipe(&h, ...)
    HANDLE* put() { reset(); return &h_; }
    bool valid() const { return h_ != nullptr && h_ != INVALID_HANDLE_VALUE; }
    explicit operator bool() const { return valid(); }
    HANDLE release() { HANDLE t = h_; h_ = nullptr; return t; }
    void reset(HANDLE h = nullptr) {
        if (valid()) CloseHandle(h_);
        h_ = h;
    }
private:
    HANDLE h_ = nullptr;
};

} // namespace util
