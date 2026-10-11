#pragma once
// Реальные сведения о системе через WinAPI: процессор, память, загрузка ЦП, время работы.

#include "../Common.h"

namespace SysInfo
{
inline std::wstring ProcessorName()
{
    wchar_t buf[256] = {};
    DWORD size = static_cast<DWORD>(sizeof(buf));
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString",
                     RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS)
    {
        const std::wstring s = Trim(buf);
        if (!s.empty())
            return s;
    }
    return L"Процессор x64";
}

// Номинальная частота (МГц) из реестра.
inline DWORD ProcessorMhz()
{
    DWORD mhz = 0, size = static_cast<DWORD>(sizeof(mhz));
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"~MHz", RRF_RT_REG_DWORD, nullptr,
                     &mhz, &size) != ERROR_SUCCESS ||
        mhz == 0)
        mhz = 2900;
    return mhz;
}

inline int LogicalProcessors()
{
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    return static_cast<int>(std::max<DWORD>(1, si.dwNumberOfProcessors));
}

inline MEMORYSTATUSEX Memory()
{
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms))
    {
        ms.ullTotalPhys = 16ULL << 30;
        ms.ullAvailPhys = 9ULL << 30;
        ms.dwMemoryLoad = 44;
    }
    return ms;
}

// «16,0 ГБ»
inline std::wstring GbText(unsigned long long bytes)
{
    const double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    const long long tenths = static_cast<long long>(gb * 10.0 + 0.5);
    return std::to_wstring(tenths / 10) + L"," + std::to_wstring(tenths % 10) + L"\u00A0ГБ";
}

inline std::wstring InstalledMemoryText()
{
    const MEMORYSTATUSEX ms = Memory();
    const unsigned long long gb = (ms.ullTotalPhys + (1ULL << 30) - 1) >> 30;
    return std::to_wstring(gb) + L",0\u00A0ГБ (доступно: " + GbText(ms.ullTotalPhys) + L")";
}

// Время работы системы «д:чч:мм:сс».
inline std::wstring UptimeText()
{
    const unsigned long long s = GetTickCount64() / 1000ULL;
    const unsigned long long d = s / 86400ULL, h = (s / 3600ULL) % 24ULL, m = (s / 60ULL) % 60ULL, sec = s % 60ULL;
    return std::to_wstring(d) + L":" + Pad2(static_cast<int>(h)) + L":" + Pad2(static_cast<int>(m)) + L":" + Pad2(static_cast<int>(sec));
}

// Загрузка ЦП между вызовами Sample() (0..1).
class CpuSampler
{
public:
    float Sample()
    {
        FILETIME idle{}, kernel{}, user{};
        if (!GetSystemTimes(&idle, &kernel, &user))
            return m_last;
        const unsigned long long i = ToU64(idle), k = ToU64(kernel), u = ToU64(user);
        if (m_have)
        {
            const unsigned long long di = i - m_idle, dk = k - m_kernel, du = u - m_user;
            const unsigned long long total = dk + du;
            if (total > 0)
                m_last = Saturate(1.0f - static_cast<float>(static_cast<double>(di) / static_cast<double>(total)));
        }
        m_idle = i;
        m_kernel = k;
        m_user = u;
        m_have = true;
        return m_last;
    }

private:
    static unsigned long long ToU64(const FILETIME& f) { return (static_cast<unsigned long long>(f.dwHighDateTime) << 32) | f.dwLowDateTime; }
    unsigned long long m_idle = 0, m_kernel = 0, m_user = 0;
    bool m_have = false;
    float m_last = 0.05f;
};
} // namespace SysInfo
