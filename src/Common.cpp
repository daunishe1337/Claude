// ============================================================================
//  Common.cpp — реализация утилит
// ============================================================================
#include "Common.h"

#include <algorithm>
#include <cstdio>
#include <cwctype>

namespace util {

// Универсальная обёртка над MultiByteToWideChar
static std::wstring decode(UINT cp, DWORD flags, const std::string& s, bool* ok) {
    if (ok) *ok = true;
    if (s.empty()) return {};
    int n = MultiByteToWideChar(cp, flags, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) {
        if (ok) *ok = false;
        return {};
    }
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(cp, flags, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

std::wstring fromUtf8(const std::string& s) { return decode(CP_UTF8, 0, s, nullptr); }

std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring decodeBytes(const std::string& s) {
    bool ok = false;
    std::wstring w = decode(CP_UTF8, MB_ERR_INVALID_CHARS, s, &ok);
    if (ok) return w;
    return decode(CP_OEMCP, 0, s, nullptr);
}

std::wstring decodeFileText(std::string s) {
    if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF &&
        static_cast<unsigned char>(s[1]) == 0xBB && static_cast<unsigned char>(s[2]) == 0xBF)
        s.erase(0, 3);
    bool ok = false;
    std::wstring w = decode(CP_UTF8, MB_ERR_INVALID_CHARS, s, &ok);
    if (ok) return w;
    return decode(CP_ACP, 0, s, nullptr);
}

// ---------------------------------------------------------------------------
std::wstring StreamDecoder::feed(const std::string& bytes) {
    pending_ += bytes;
    // Ищем незавершённую UTF-8 последовательность в конце буфера (максимум 3 байта)
    size_t cut = pending_.size();
    for (size_t k = 1; k <= 3 && k <= pending_.size(); ++k) {
        unsigned char c = static_cast<unsigned char>(pending_[pending_.size() - k]);
        if ((c & 0xC0) == 0x80) continue;            // байт-продолжение — смотрим дальше назад
        if (c >= 0xC0) {                             // ведущий байт
            size_t need = c >= 0xF0 ? 4 : (c >= 0xE0 ? 3 : 2);
            if (need > k) cut = pending_.size() - k; // символ ещё не пришёл целиком
        }
        break;
    }
    std::string ready = pending_.substr(0, cut);
    pending_.erase(0, cut);
    return decodeBytes(ready);
}

std::wstring StreamDecoder::flush() {
    std::wstring w = decodeBytes(pending_);
    pending_.clear();
    return w;
}

// ---------------------------------------------------------------------------
std::wstring trim(const std::wstring& s) {
    const wchar_t* ws = L" \t\r\n";
    size_t b = s.find_first_not_of(ws);
    if (b == std::wstring::npos) return {};
    size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

std::wstring toLower(std::wstring s) {
    for (auto& c : s) c = static_cast<wchar_t>(std::towlower(c));
    return s;
}

bool startsWith(const std::wstring& s, const std::wstring& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::vector<std::wstring> splitLines(const std::wstring& text) {
    std::vector<std::wstring> lines;
    std::wstring cur;
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t c = text[i];
        if (c == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') ++i;
            lines.push_back(cur);
            cur.clear();
        } else if (c == L'\n') {
            lines.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    lines.push_back(cur);
    return lines;
}

std::wstring expandTabs(const std::wstring& s, int tabSize) {
    if (s.find(L'\t') == std::wstring::npos) return s;
    std::wstring r;
    for (wchar_t c : s) {
        if (c == L'\t') {
            size_t spaces = tabSize - (r.size() % tabSize);
            r.append(spaces, L' ');
        } else {
            r += c;
        }
    }
    return r;
}

std::wstring formatSeconds(ULONGLONG ms) {
    wchar_t buf[32];
    swprintf(buf, 32, L"%.2fs", static_cast<double>(ms) / 1000.0);
    return buf;
}

std::wstring hex32(DWORD v) {
    wchar_t buf[16];
    swprintf(buf, 16, L"0x%08lX", static_cast<unsigned long>(v));
    return buf;
}

// ---------------------------------------------------------------------------
bool fileExists(const std::wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool dirExists(const std::wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

bool readFileBytes(const std::wstring& path, std::string& out) {
    out.clear();
    Handle f(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!f) return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(f.get(), &size) || size.QuadPart > 64LL * 1024 * 1024) return false;
    out.resize(static_cast<size_t>(size.QuadPart));
    size_t done = 0;
    while (done < out.size()) {
        DWORD n = 0;
        if (!ReadFile(f.get(), &out[done], static_cast<DWORD>(out.size() - done), &n, nullptr) || n == 0) break;
        done += n;
    }
    out.resize(done);
    return true;
}

bool writeFileBytes(const std::wstring& path, const std::string& data) {
    Handle f(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!f) return false;
    size_t done = 0;
    while (done < data.size()) {
        DWORD n = 0;
        if (!WriteFile(f.get(), data.data() + done, static_cast<DWORD>(data.size() - done), &n, nullptr)) return false;
        done += n;
    }
    return true;
}

std::wstring joinPath(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    wchar_t last = a.back();
    if (last == L'\\' || last == L'/') return a + b;
    return a + L"\\" + b;
}

std::wstring dirOf(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    return p == std::wstring::npos ? std::wstring() : path.substr(0, p);
}

std::wstring baseName(const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    return p == std::wstring::npos ? path : path.substr(p + 1);
}

std::wstring getEnv(const wchar_t* name) {
    DWORD n = GetEnvironmentVariableW(name, nullptr, 0);
    if (n == 0) return {};
    std::wstring v(n, L'\0');
    n = GetEnvironmentVariableW(name, &v[0], n);
    v.resize(n);
    return v;
}

std::wstring expandEnv(const std::wstring& s) {
    DWORD n = ExpandEnvironmentStringsW(s.c_str(), nullptr, 0);
    if (n == 0) return s;
    std::wstring r(n, L'\0');
    n = ExpandEnvironmentStringsW(s.c_str(), &r[0], n);
    r.resize(n > 0 ? n - 1 : 0);  // n включает завершающий ноль
    return r;
}

std::wstring currentDir() {
    DWORD n = GetCurrentDirectoryW(0, nullptr);
    std::wstring d(n, L'\0');
    n = GetCurrentDirectoryW(n, &d[0]);
    d.resize(n);
    return d;
}

bool isAbsolutePath(const std::wstring& p) {
    return (p.size() >= 2 && p[1] == L':') || startsWith(p, L"\\\\") || startsWith(p, L"\\") || startsWith(p, L"/");
}

std::wstring lastErrorText(DWORD code) {
    wchar_t* buf = nullptr;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
                             nullptr, code, 0, reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
    std::wstring msg = (n && buf) ? trim(buf) : L"unknown error";
    if (buf) LocalFree(buf);
    return msg + L" (code " + std::to_wstring(code) + L")";
}

} // namespace util
