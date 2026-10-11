// Терминал Windows: вкладки PowerShell и командной строки, буфер прокрутки, выделение и копирование,
// история команд, автодополнение путей (Tab), конвейеры (| findstr, sort, clip), перенаправление (>, >>)
// и около пятидесяти команд, работающих с виртуальной файловой системой и оболочкой.

#include "../Shell.h"
#include "AppFactories.h"
#include "TabStrip.h"

namespace
{
constexpr float kTitleH = 40.0f;
constexpr float kPadX = 10.0f;
constexpr float kPadY = 8.0f;

// Цветовая схема Campbell.
const Color kBg = Color::Hex(0x0C0C0C);
const Color kFg = Color::Hex(0xCCCCCC);
const Color kRed = Color::Hex(0xE74856);
const Color kYellow = Color::Hex(0xF9F1A5);
const Color kGreen = Color::Hex(0x16C60C);
const Color kCyan = Color::Hex(0x61D6D6);
const Color kGray = Color::Hex(0x767676);
const Color kWhite = Color::Hex(0xF2F2F2);

enum class ShellKind
{
    PowerShell,
    Cmd,
};

struct Seg
{
    std::wstring text;
    Color color;
};

struct TLine
{
    std::vector<Seg> segs;
    size_t Length() const
    {
        size_t n = 0;
        for (const Seg& s : segs)
            n += s.text.size();
        return n;
    }
    std::wstring Text() const
    {
        std::wstring t;
        for (const Seg& s : segs)
            t += s.text;
        return t;
    }
};

struct Pending
{
    double at = 0.0;
    TLine line;
    std::function<void()> action;
};

struct Session
{
    ShellKind kind = ShellKind::PowerShell;
    bool admin = false;
    std::wstring title;
    std::vector<TLine> lines;
    FsNode* cwd = nullptr;
    std::wstring input;
    size_t caret = 0;
    std::vector<std::wstring> history;
    int histPos = -1;
    std::wstring savedInput;
    float scroll = 0.0f; // строк от нижнего края
    std::vector<Pending> pending;
    std::vector<ShellKind> stack; // вложенные оболочки (powershell внутри cmd)
    // Автодополнение
    std::vector<std::wstring> completions;
    size_t compIndex = 0;
    std::wstring compPrefix;
    std::wstring compLast;
};

// Разбор строки на аргументы с учётом кавычек.
std::vector<std::wstring> Tokenize(const std::wstring& s)
{
    std::vector<std::wstring> out;
    std::wstring cur;
    bool inQuote = false, any = false;
    wchar_t q = 0;
    for (wchar_t c : s)
    {
        if (inQuote)
        {
            if (c == q)
                inQuote = false;
            else
                cur.push_back(c);
            continue;
        }
        if (c == L'"' || c == L'\'')
        {
            inQuote = true;
            q = c;
            any = true;
            continue;
        }
        if (c == L' ' || c == L'\t')
        {
            if (!cur.empty() || any)
                out.push_back(cur);
            cur.clear();
            any = false;
            continue;
        }
        cur.push_back(c);
    }
    if (!cur.empty() || any)
        out.push_back(cur);
    return out;
}

// Разделение по символу вне кавычек (конвейер «|»).
std::vector<std::wstring> SplitTop(const std::wstring& s, wchar_t sep)
{
    std::vector<std::wstring> out;
    std::wstring cur;
    wchar_t q = 0;
    for (wchar_t c : s)
    {
        if (q)
        {
            if (c == q)
                q = 0;
            cur.push_back(c);
            continue;
        }
        if (c == L'"' || c == L'\'')
            q = c;
        if (c == sep)
        {
            out.push_back(cur);
            cur.clear();
            continue;
        }
        cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

std::wstring PadLeft(const std::wstring& s, size_t w) { return s.size() >= w ? s : std::wstring(w - s.size(), L' ') + s; }
std::wstring PadRight(const std::wstring& s, size_t w) { return s.size() >= w ? s : s + std::wstring(w - s.size(), L' '); }

std::wstring PsTime(const SYSTEMTIME& st) { return std::to_wstring(st.wHour) + L":" + Pad2(st.wMinute); }

// Вычисление арифметического выражения (PowerShell печатает результат «2+2»).
class Expr
{
public:
    explicit Expr(const std::wstring& s) : m_s(s) {}
    bool Eval(double& out)
    {
        m_pos = 0;
        m_ok = true;
        out = Sum();
        Skip();
        return m_ok && m_pos == m_s.size();
    }

private:
    void Skip()
    {
        while (m_pos < m_s.size() && m_s[m_pos] == L' ')
            ++m_pos;
    }
    double Sum()
    {
        double v = Prod();
        for (;;)
        {
            Skip();
            if (m_pos < m_s.size() && (m_s[m_pos] == L'+' || m_s[m_pos] == L'-'))
            {
                const wchar_t op = m_s[m_pos++];
                const double b = Prod();
                v = op == L'+' ? v + b : v - b;
            }
            else
                return v;
        }
    }
    double Prod()
    {
        double v = Unary();
        for (;;)
        {
            Skip();
            if (m_pos < m_s.size() && (m_s[m_pos] == L'*' || m_s[m_pos] == L'/' || m_s[m_pos] == L'%'))
            {
                const wchar_t op = m_s[m_pos++];
                const double b = Unary();
                if (op == L'*')
                    v *= b;
                else if (b == 0.0)
                {
                    m_ok = false;
                    return 0.0;
                }
                else
                    v = op == L'/' ? v / b : std::fmod(v, b);
            }
            else
                return v;
        }
    }
    double Unary()
    {
        Skip();
        if (m_pos < m_s.size() && m_s[m_pos] == L'-')
        {
            ++m_pos;
            return -Unary();
        }
        if (m_pos < m_s.size() && m_s[m_pos] == L'(')
        {
            ++m_pos;
            const double v = Sum();
            Skip();
            if (m_pos >= m_s.size() || m_s[m_pos] != L')')
                m_ok = false;
            else
                ++m_pos;
            return v;
        }
        const size_t start = m_pos;
        while (m_pos < m_s.size() && ((m_s[m_pos] >= L'0' && m_s[m_pos] <= L'9') || m_s[m_pos] == L'.'))
            ++m_pos;
        if (start == m_pos)
        {
            m_ok = false;
            return 0.0;
        }
        std::string a;
        for (size_t i = start; i < m_pos; ++i)
            a.push_back(static_cast<char>(m_s[i]));
        return std::strtod(a.c_str(), nullptr);
    }

    std::wstring m_s;
    size_t m_pos = 0;
    bool m_ok = true;
};

std::wstring NumText(double v)
{
    if (v == std::floor(v) && std::fabs(v) < 1e15)
        return std::to_wstring(static_cast<long long>(v));
    wchar_t buf[64] = {};
    swprintf(buf, 64, L"%.15g", v);
    std::wstring s = buf;
    for (wchar_t& c : s)
    {
        if (c == L'.')
            c = L',';
    }
    return s;
}

const wchar_t* ExeName(AppId id)
{
    switch (id)
    {
    case AppId::Explorer:
        return L"explorer";
    case AppId::Notepad:
        return L"notepad";
    case AppId::Settings:
        return L"SystemSettings";
    case AppId::Calculator:
        return L"CalculatorApp";
    case AppId::Terminal:
        return L"WindowsTerminal";
    case AppId::TaskManager:
        return L"Taskmgr";
    case AppId::Paint:
        return L"mspaint";
    case AppId::Photos:
        return L"Photos";
    case AppId::Clock:
        return L"Time";
    case AppId::Weather:
        return L"Weather";
    case AppId::Browser:
        return L"browser";
    case AppId::Mail:
        return L"mail";
    case AppId::Calendar:
        return L"calendar";
    case AppId::Store:
        return L"Store";
    case AppId::Camera:
        return L"Camera";
    case AppId::MediaPlayer:
        return L"Media.Player";
    case AppId::Maps:
        return L"Maps";
    case AppId::Tips:
        return L"Tips";
    case AppId::System:
    case AppId::None:
        break;
    }
    return L"system";
}

class TerminalWindow : public Window
{
public:
    explicit TerminalWindow(const std::wstring& arg) : Window(AppId::Terminal, L"Терминал")
    {
        SetInitialSize(980.0f, 560.0f);
        m_minSize = {420.0f, 260.0f};
        m_strip.extraButtons = 32.0f;
        m_strip.reserveRight = 46.0f * 3.0f + 90.0f;
        ShellKind kind = ShellKind::PowerShell;
        bool admin = false;
        FsNode* cwd = nullptr;
        if (arg == L"::admin")
            admin = true;
        else if (arg == L"::cmd")
            kind = ShellKind::Cmd;
        else if (!arg.empty())
        {
            FsNode* n = GetShell().FS().FromArg(arg);
            if (n && !n->IsFolder())
                n = n->parent;
            if (n && n->IsFolder() && n != GetShell().FS().Root() && !GetShell().FS().InBin(n))
                cwd = n;
        }
        NewSession(kind, admin, cwd);
    }

    float TitleBarHeight() const override { return kTitleH; }
    bool ShowTitle() const override { return false; }
    bool DarkContent() const override { return true; }

private:
    // ================================================================== вкладки
    // Сеансы хранятся по указателю: ссылки на них остаются действительными при добавлении вкладок.
    Session& Cur() { return *m_tabs[static_cast<size_t>(m_cur)]; }
    const Session& Cur() const { return *m_tabs[static_cast<size_t>(m_cur)]; }

    void NewSession(ShellKind kind, bool admin = false, FsNode* cwd = nullptr)
    {
        auto s = std::make_unique<Session>();
        s->kind = kind;
        s->admin = admin;
        s->cwd = cwd ? cwd : GetShell().FS().Home();
        if (admin && !cwd)
            s->cwd = GetShell().FS().Resolve(L"C:\\Windows\\System32", GetShell().FS().Root());
        if (!s->cwd)
            s->cwd = GetShell().FS().Home();
        m_tabs.push_back(std::move(s));
        m_cur = static_cast<int>(m_tabs.size()) - 1;
        Banner(Cur());
        UpdateTitle();
    }

    std::wstring ShellTitle(const Session& s) const
    {
        const std::wstring base = s.kind == ShellKind::PowerShell ? L"Windows PowerShell" : L"Командная строка";
        if (!s.title.empty())
            return s.title;
        return s.admin ? L"Администратор: " + base : base;
    }

    void UpdateTitle() { SetTitle(ShellTitle(Cur())); }

    void CloseTab(int i)
    {
        if (m_tabs.size() <= 1)
        {
            ForceClose();
            return;
        }
        m_tabs.erase(m_tabs.begin() + i);
        if (m_cur >= static_cast<int>(m_tabs.size()))
            m_cur = static_cast<int>(m_tabs.size()) - 1;
        else if (m_cur > i)
            --m_cur;
        m_sel = false;
        UpdateTitle();
    }

    void Banner(Session& s)
    {
        m_target = &s;
        if (s.kind == ShellKind::PowerShell)
        {
            Out(L"Windows PowerShell");
            Out(L"(C) Демонстрационная версия. Все права защищены.");
            Out(L"");
            Out(L"Установите последнюю версию PowerShell для новых функций и улучшения! https://aka.ms/PSWindows");
            Out(L"");
            Out(L"Введите help, чтобы увидеть список доступных команд.");
            Out(L"");
        }
        else
        {
            Out(L"Windows [Version 10.0.22631.4317]");
            Out(L"(c) Демонстрационная версия. Все права защищены.");
            Out(L"");
        }
        m_target = nullptr;
    }

    // ================================================================== вывод
    void Out(const std::wstring& text, Color c = kFg)
    {
        size_t start = 0;
        for (;;)
        {
            const size_t nl = text.find(L'\n', start);
            std::wstring part = text.substr(start, nl == std::wstring::npos ? std::wstring::npos : nl - start);
            // Табуляция → пробелы
            std::wstring expanded;
            for (wchar_t ch : part)
            {
                if (ch == L'\t')
                    expanded.append(8 - expanded.size() % 8, L' ');
                else if (ch != L'\r')
                    expanded.push_back(ch);
            }
            TLine l;
            if (!expanded.empty())
                l.segs.push_back({expanded, c});
            Emit(std::move(l));
            if (nl == std::wstring::npos)
                break;
            start = nl + 1;
        }
    }

    void Emit(TLine l)
    {
        if (m_capture)
        {
            m_capture->push_back(std::move(l));
            return;
        }
        Session& s = m_target ? *m_target : Cur();
        s.lines.push_back(std::move(l));
        if (s.lines.size() > 3000)
            s.lines.erase(s.lines.begin(), s.lines.begin() + 500);
    }

    void Err(const std::wstring& text) { Out(text, kRed); }

    // Ошибка в формате Windows PowerShell 5.1.
    void PsError(const std::wstring& cmd, const std::wstring& message, const std::wstring& line, const std::wstring& category,
                 const std::wstring& errorId)
    {
        Err(cmd + L" : " + message);
        Err(L"строка:1 знак:1");
        Err(L"+ " + line);
        Err(L"+ " + std::wstring(std::max<size_t>(1, std::min<size_t>(line.size(), 60)), L'~'));
        Err(L"    + CategoryInfo          : " + category);
        Err(L"    + FullyQualifiedErrorId : " + errorId);
        Out(L"");
    }

    std::wstring PathStr(const FsNode* n) const { return GetShell().FS().PathOf(n); }

    std::wstring Prompt(const Session& s) const
    {
        return s.kind == ShellKind::PowerShell ? L"PS " + PathStr(s.cwd) + L"> " : PathStr(s.cwd) + L">";
    }

    // ================================================================== выполнение команд
    void Submit()
    {
        Session& s = Cur();
        TLine echo;
        echo.segs.push_back({Prompt(s), kFg});
        if (!s.input.empty())
            echo.segs.push_back({s.input, kWhite});
        m_target = &s;
        Emit(std::move(echo));
        const std::wstring line = s.input;
        s.input.clear();
        s.caret = 0;
        s.histPos = -1;
        s.completions.clear();
        if (!Trim(line).empty() && (s.history.empty() || s.history.back() != line))
            s.history.push_back(line);
        Execute(s, line);
        if (s.kind == ShellKind::Cmd && s.pending.empty() && !Trim(line).empty() && m_tabs.size() > 0)
            Out(L"");
        m_target = nullptr;
        s.scroll = 0.0f;
    }

    void Execute(Session& s, const std::wstring& raw)
    {
        std::wstring line = Trim(raw);
        if (line.empty())
            return;
        // Перенаправление вывода «>», «>>» (вне кавычек).
        std::wstring redirect;
        bool append = false;
        {
            wchar_t q = 0;
            for (size_t i = 0; i < line.size(); ++i)
            {
                const wchar_t c = line[i];
                if (q)
                {
                    if (c == q)
                        q = 0;
                    continue;
                }
                if (c == L'"' || c == L'\'')
                    q = c;
                else if (c == L'>')
                {
                    append = i + 1 < line.size() && line[i + 1] == L'>';
                    const std::vector<std::wstring> t = Tokenize(line.substr(i + (append ? 2 : 1)));
                    redirect = t.empty() ? L"" : t.front();
                    line = Trim(line.substr(0, i));
                    break;
                }
            }
        }
        const std::vector<std::wstring> stages = SplitTop(line, L'|');
        std::vector<TLine> captured;
        const bool needCapture = stages.size() > 1 || !redirect.empty();
        if (needCapture)
            m_capture = &captured;
        RunCommand(s, Trim(stages[0]));
        m_capture = nullptr;
        if (!needCapture)
            return;
        // Фильтры конвейера.
        for (size_t i = 1; i < stages.size(); ++i)
        {
            const std::vector<std::wstring> t = Tokenize(Trim(stages[i]));
            if (t.empty())
                continue;
            const std::wstring cmd = ToLower(t[0]);
            if ((cmd == L"findstr" || cmd == L"sls" || cmd == L"select-string" || cmd == L"where" || cmd == L"?") && t.size() > 1)
            {
                std::wstring pat = t.back();
                std::vector<TLine> f;
                for (TLine& l : captured)
                {
                    if (ContainsNoCase(l.Text(), pat))
                        f.push_back(l);
                }
                captured.swap(f);
            }
            else if (cmd == L"sort" || cmd == L"sort-object")
                std::sort(captured.begin(), captured.end(), [](const TLine& a, const TLine& b) { return CompareNoCase(a.Text(), b.Text()) < 0; });
            else if (cmd == L"measure" || cmd == L"measure-object")
            {
                size_t n = 0;
                for (const TLine& l : captured)
                    n += Trim(l.Text()).empty() ? 0 : 1;
                captured.clear();
                m_capture = &captured;
                Out(L"");
                Out(L"Count    : " + std::to_wstring(n));
                Out(L"Average  :");
                Out(L"Sum      :");
                Out(L"Maximum  :");
                Out(L"Minimum  :");
                Out(L"Property :");
                Out(L"");
                m_capture = nullptr;
            }
            else if (cmd == L"clip")
            {
                std::wstring text;
                for (const TLine& l : captured)
                    text += l.Text() + L"\r\n";
                SetClipboardText(GetShell().Hwnd(), text);
                captured.clear();
            }
            else if (cmd == L"out-file" && t.size() > 1)
                redirect = t.back();
            else if (cmd == L"more" || cmd == L"out-host" || cmd == L"out-default")
            {
            }
            else
            {
                Err(L"Команда конвейера «" + t[0] + L"» не поддерживается в этой демонстрации.");
                captured.clear();
            }
        }
        if (!redirect.empty())
        {
            std::wstring text;
            for (const TLine& l : captured)
                text += l.Text() + L"\n";
            WriteFile(s, redirect, text, append);
            return;
        }
        for (TLine& l : captured)
            Emit(std::move(l));
    }

    void WriteFile(Session& s, const std::wstring& path, const std::wstring& text, bool append)
    {
        VirtualFS& fs = GetShell().FS();
        FsNode* n = fs.Resolve(path, s.cwd);
        if (n && n->IsFolder())
        {
            Err(L"Отказано в доступе по пути \"" + PathStr(n) + L"\".");
            return;
        }
        if (!n)
        {
            const size_t sl = path.find_last_of(L"\\/");
            FsNode* dir = sl == std::wstring::npos ? s.cwd : fs.Resolve(path.substr(0, sl + 1), s.cwd);
            const std::wstring name = sl == std::wstring::npos ? path : path.substr(sl + 1);
            if (!dir || !dir->IsFolder() || !VirtualFS::ValidName(name))
            {
                Err(L"Не удается найти часть пути \"" + path + L"\".");
                return;
            }
            n = fs.NewFile(dir, name, FileKind::Text, L"");
        }
        fs.SaveText(n, append ? n->content + text : text);
    }

    static bool IsFlag(const std::wstring& a) { return a.size() > 1 && (a[0] == L'-' || a[0] == L'/'); }

    static bool HasFlag(const std::vector<std::wstring>& args, std::initializer_list<const wchar_t*> names)
    {
        for (const std::wstring& a : args)
        {
            const std::wstring l = ToLower(a);
            for (const wchar_t* n : names)
            {
                if (l == n)
                    return true;
            }
        }
        return false;
    }

    // Значение именованного параметра («-ItemType Directory»).
    static std::wstring Param(const std::vector<std::wstring>& args, std::initializer_list<const wchar_t*> names)
    {
        for (size_t i = 0; i + 1 < args.size(); ++i)
        {
            const std::wstring l = ToLower(args[i]);
            for (const wchar_t* n : names)
            {
                if (l == n)
                    return args[i + 1];
            }
        }
        return L"";
    }

    // Позиционные аргументы (без флагов и значений именованных параметров).
    static std::vector<std::wstring> Positional(const std::vector<std::wstring>& args, std::initializer_list<const wchar_t*> valued = {})
    {
        std::vector<std::wstring> out;
        for (size_t i = 0; i < args.size(); ++i)
        {
            if (IsFlag(args[i]) && args[i].find_first_of(L"\\:") == std::wstring::npos)
            {
                const std::wstring l = ToLower(args[i]);
                for (const wchar_t* n : valued)
                {
                    if (l == n)
                    {
                        ++i;
                        break;
                    }
                }
                continue;
            }
            out.push_back(args[i]);
        }
        return out;
    }

    void RunCommand(Session& s, const std::wstring& line)
    {
        if (line.empty())
            return;
        // Строка в кавычках — вывод строки (PowerShell).
        if (s.kind == ShellKind::PowerShell && (line.front() == L'"' || line.front() == L'\''))
        {
            const std::vector<std::wstring> t = Tokenize(line);
            Out(t.empty() ? L"" : t.front());
            return;
        }
        // Арифметика: «2+2*3».
        if (s.kind == ShellKind::PowerShell && line.find_first_not_of(L"0123456789.+-*/%() ") == std::wstring::npos &&
            line.find_first_of(L"0123456789") != std::wstring::npos)
        {
            double v = 0.0;
            Expr e(line);
            if (e.Eval(v))
                Out(NumText(v));
            else
                PsError(line, L"Попытка деления на нуль или неверное выражение.", line, L"NotSpecified: (:) [], RuntimeException",
                        L"RuntimeException");
            return;
        }
        std::vector<std::wstring> tok = Tokenize(line);
        if (tok.empty())
            return;
        std::wstring cmd = ToLower(tok[0]);
        // «cd..» и «cd\» в стиле cmd
        if (cmd.rfind(L"cd..", 0) == 0 || cmd == L"cd\\")
        {
            tok = {L"cd", cmd.substr(2)};
            cmd = L"cd";
        }
        if (cmd.size() > 4 && cmd.substr(cmd.size() - 4) == L".exe")
            cmd = cmd.substr(0, cmd.size() - 4);
        const std::vector<std::wstring> args(tok.begin() + 1, tok.end());
        const bool ps = s.kind == ShellKind::PowerShell;
        VirtualFS& fs = GetShell().FS();

        if (cmd == L"cls" || cmd == L"clear" || cmd == L"clear-host")
        {
            s.lines.clear();
            return;
        }
        if (cmd == L"help" || cmd == L"get-help" || cmd == L"man" || cmd == L"?")
        {
            Help(ps);
            return;
        }
        if (cmd == L"exit")
        {
            if (!s.stack.empty())
            {
                s.kind = s.stack.back();
                s.stack.pop_back();
                UpdateTitle();
                return;
            }
            const int idx = m_cur;
            PostAction([this, idx]() { CloseTab(idx); });
            return;
        }
        if (cmd == L"cd" || cmd == L"chdir" || cmd == L"sl" || cmd == L"set-location" || cmd == L"pushd")
        {
            std::vector<std::wstring> pos = Positional(args);
            if (!pos.empty() && ToLower(pos[0]) == L"/d")
                pos.erase(pos.begin());
            if (pos.empty())
            {
                if (!ps)
                    Out(PathStr(s.cwd));
                return;
            }
            FsNode* n = fs.Resolve(pos[0], s.cwd);
            if (!n || !n->IsFolder() || n == fs.Root())
            {
                if (ps)
                    PsError(L"Set-Location", L"Не удается найти путь \"" + pos[0] + L"\", так как он не существует.", line,
                            L"ObjectNotFound: (" + pos[0] + L":String) [Set-Location], ItemNotFoundException",
                            L"PathNotFound,Microsoft.PowerShell.Commands.SetLocationCommand");
                else
                    Out(L"Системе не удается найти указанный путь.");
                return;
            }
            s.cwd = n;
            return;
        }
        if (cmd == L"pwd" || cmd == L"gl" || cmd == L"get-location")
        {
            Out(L"");
            Out(L"Path");
            Out(L"----");
            Out(PathStr(s.cwd));
            Out(L"");
            return;
        }
        if (cmd == L"dir" || cmd == L"ls" || cmd == L"gci" || cmd == L"get-childitem")
        {
            const std::vector<std::wstring> pos = Positional(args, {L"-filter", L"-path"});
            std::wstring target = pos.empty() ? Param(args, {L"-path"}) : pos[0];
            FsNode* dir = target.empty() ? s.cwd : fs.Resolve(target, s.cwd);
            std::wstring filter = Param(args, {L"-filter"});
            if (!dir && !target.empty() && target.find(L'*') != std::wstring::npos)
            {
                // «dir *.txt»
                const size_t sl = target.find_last_of(L"\\/");
                dir = sl == std::wstring::npos ? s.cwd : fs.Resolve(target.substr(0, sl + 1), s.cwd);
                filter = sl == std::wstring::npos ? target : target.substr(sl + 1);
            }
            if (!dir)
            {
                if (ps)
                    PsError(L"Get-ChildItem", L"Не удается найти путь \"" + target + L"\", так как он не существует.", line,
                            L"ObjectNotFound: (" + target + L":String) [Get-ChildItem], ItemNotFoundException",
                            L"PathNotFound,Microsoft.PowerShell.Commands.GetChildItemCommand");
                else
                    Out(L"Файл не найден");
                return;
            }
            const bool bare = HasFlag(args, {L"/b", L"-name"});
            if (ps)
                PsDir(dir, filter, HasFlag(args, {L"-recurse", L"-r", L"/s"}), bare);
            else
                CmdDir(dir, filter, bare);
            return;
        }
        if (cmd == L"tree")
        {
            FsNode* dir = args.empty() || IsFlag(args[0]) ? s.cwd : fs.Resolve(args[0], s.cwd);
            if (!dir || !dir->IsFolder())
            {
                Out(L"Неверный путь - " + (args.empty() ? std::wstring() : args[0]));
                return;
            }
            Out(L"Структура папок");
            Out(L"Серийный номер тома: 5E2A-7C11");
            Out(PathStr(dir));
            Tree(dir, L"", 0, HasFlag(args, {L"/f"}));
            return;
        }
        if (cmd == L"mkdir" || cmd == L"md" || ((cmd == L"new-item" || cmd == L"ni") &&
                                                 ToLower(Param(args, {L"-itemtype", L"-type"})) == L"directory"))
        {
            const std::vector<std::wstring> pos = Positional(args, {L"-itemtype", L"-type", L"-name", L"-path"});
            std::wstring name = pos.empty() ? Param(args, {L"-name"}) : pos[0];
            if (name.empty())
            {
                Err(ps ? L"New-Item : Не указано имя элемента." : L"Синтаксис команды неверен.");
                return;
            }
            const size_t sl = name.find_last_of(L"\\/");
            FsNode* dir = sl == std::wstring::npos ? s.cwd : fs.Resolve(name.substr(0, sl + 1), s.cwd);
            const std::wstring leaf = sl == std::wstring::npos ? name : name.substr(sl + 1);
            if (!dir || !VirtualFS::ValidName(leaf))
            {
                Err(L"Не удается найти часть пути \"" + name + L"\".");
                return;
            }
            if (fs.Find(dir, leaf))
            {
                if (ps)
                    PsError(L"New-Item", L"Элемент с указанным именем " + PathStr(dir) + L"\\" + leaf + L" уже существует.", line,
                            L"ResourceExists: (" + leaf + L":String) [New-Item], IOException",
                            L"DirectoryExist,Microsoft.PowerShell.Commands.NewItemCommand");
                else
                    Out(L"Подпапка или файл " + leaf + L" уже существует.");
                return;
            }
            FsNode* n = fs.NewFolder(dir, leaf);
            if (ps)
                PsListing(dir, {n});
            return;
        }
        if (cmd == L"new-item" || cmd == L"ni" || cmd == L"touch")
        {
            const std::vector<std::wstring> pos = Positional(args, {L"-itemtype", L"-type", L"-name", L"-path", L"-value"});
            const std::wstring name = pos.empty() ? Param(args, {L"-name"}) : pos[0];
            if (name.empty())
            {
                Err(L"New-Item : Не указано имя элемента.");
                return;
            }
            const size_t sl = name.find_last_of(L"\\/");
            FsNode* dir = sl == std::wstring::npos ? s.cwd : fs.Resolve(name.substr(0, sl + 1), s.cwd);
            const std::wstring leaf = sl == std::wstring::npos ? name : name.substr(sl + 1);
            if (!dir || !VirtualFS::ValidName(leaf))
            {
                Err(L"New-Item : Не удается найти часть пути \"" + name + L"\".");
                return;
            }
            if (FsNode* existing = fs.Find(dir, leaf))
            {
                if (cmd == L"touch")
                {
                    existing->modified = LocalNow();
                    fs.Touch();
                    return;
                }
                PsError(L"New-Item", L"Файл \"" + PathStr(existing) + L"\" уже существует.", line,
                        L"WriteError: (" + PathStr(existing) + L":String) [New-Item], IOException",
                        L"NewItemIOError,Microsoft.PowerShell.Commands.NewItemCommand");
                return;
            }
            FsNode* n = fs.NewFile(dir, leaf, VirtualFS::KindFor(leaf) == FileKind::Other ? FileKind::Text : VirtualFS::KindFor(leaf),
                                   Param(args, {L"-value"}));
            if (cmd != L"touch")
                PsListing(dir, {n});
            return;
        }
        if (cmd == L"rm" || cmd == L"del" || cmd == L"erase" || cmd == L"rd" || cmd == L"rmdir" || cmd == L"ri" ||
            cmd == L"remove-item")
        {
            const std::vector<std::wstring> pos = Positional(args);
            if (pos.empty())
            {
                Err(ps ? L"Remove-Item : Не указан путь." : L"Синтаксис команды неверен.");
                return;
            }
            const bool recurse = HasFlag(args, {L"-recurse", L"-r", L"/s", L"-force", L"-rf"});
            for (const std::wstring& p : pos)
            {
                FsNode* n = fs.Resolve(p, s.cwd);
                if (!n || n == fs.Root())
                {
                    if (ps)
                        PsError(L"Remove-Item", L"Не удается найти путь \"" + p + L"\", так как он не существует.", line,
                                L"ObjectNotFound: (" + p + L":String) [Remove-Item], ItemNotFoundException",
                                L"PathNotFound,Microsoft.PowerShell.Commands.RemoveItemCommand");
                    else
                        Out(L"Не удается найти " + p);
                    continue;
                }
                if (n->system)
                {
                    Err(L"Отказано в доступе по пути \"" + PathStr(n) + L"\".");
                    continue;
                }
                if (n->IsFolder() && !n->children.empty() && !recurse)
                {
                    if (ps)
                        Err(L"Remove-Item : Элемент " + PathStr(n) + L" имеет дочерние объекты, а параметр Recurse не указан. "
                                                                    L"Используйте Remove-Item " + p + L" -Recurse.");
                    else
                        Out(L"Папка не пуста.");
                    continue;
                }
                if (s.cwd == n || fs.IsAncestor(n, s.cwd))
                    s.cwd = n->parent;
                fs.DeleteForever(n);
            }
            return;
        }
        if (cmd == L"cat" || cmd == L"type" || cmd == L"gc" || cmd == L"get-content")
        {
            const std::vector<std::wstring> pos = Positional(args, {L"-tail", L"-head", L"-totalcount"});
            if (pos.empty())
            {
                Err(L"Не указан файл.");
                return;
            }
            FsNode* n = fs.Resolve(pos[0], s.cwd);
            if (!n || n->IsFolder())
            {
                if (ps)
                    PsError(L"Get-Content", L"Не удается найти путь \"" + pos[0] + L"\", так как он не существует.", line,
                            L"ObjectNotFound: (" + pos[0] + L":String) [Get-Content], ItemNotFoundException",
                            L"PathNotFound,Microsoft.PowerShell.Commands.GetContentCommand");
                else
                    Out(L"Системе не удается найти указанный путь.");
                return;
            }
            if (n->kind != FileKind::Text && n->content.empty())
                Out(L"(двоичный файл, " + FormatSize(fs.SizeOf(n)) + L")", kGray);
            else
                Out(n->content);
            return;
        }
        if (cmd == L"echo" || cmd == L"echo." || cmd == L"write" || cmd == L"write-output" || cmd == L"write-host")
        {
            std::wstring text;
            if (ps)
            {
                for (size_t i = 0; i < args.size(); ++i)
                    text += (i ? L" " : L"") + args[i];
            }
            else
            {
                const size_t sp = line.find_first_of(L" .");
                text = sp == std::wstring::npos ? L"" : line.substr(sp + 1);
                if (cmd == L"echo" && args.empty())
                    text = L"Режим вывода команд на экран (ECHO) включён.";
            }
            Out(text, cmd == L"write-host" ? kWhite : kFg);
            return;
        }
        if (cmd == L"copy" || cmd == L"cp" || cmd == L"cpi" || cmd == L"copy-item" || cmd == L"move" || cmd == L"mv" ||
            cmd == L"mi" || cmd == L"move-item")
        {
            const bool move = cmd == L"move" || cmd == L"mv" || cmd == L"mi" || cmd == L"move-item";
            const std::vector<std::wstring> pos = Positional(args, {L"-destination"});
            std::wstring dst = Param(args, {L"-destination"});
            if (dst.empty() && pos.size() >= 2)
                dst = pos[1];
            if (pos.empty() || dst.empty())
            {
                Err(L"Синтаксис команды неверен.");
                return;
            }
            FsNode* src = fs.Resolve(pos[0], s.cwd);
            if (!src)
            {
                Err((ps ? std::wstring(move ? L"Move-Item" : L"Copy-Item") + L" : " : L"") + L"Не удается найти путь \"" + pos[0] + L"\".");
                return;
            }
            FsNode* d = fs.Resolve(dst, s.cwd);
            std::wstring newName;
            if (!d || !d->IsFolder())
            {
                // Назначение — новое имя в существующей папке.
                const size_t sl = dst.find_last_of(L"\\/");
                d = sl == std::wstring::npos ? s.cwd : fs.Resolve(dst.substr(0, sl + 1), s.cwd);
                newName = sl == std::wstring::npos ? dst : dst.substr(sl + 1);
            }
            if (!d || !d->IsFolder())
            {
                Err(L"Не удается найти путь назначения \"" + dst + L"\".");
                return;
            }
            FsNode* result = nullptr;
            if (move)
            {
                if (src->parent == d)
                    result = src;
                else if (fs.Move(src, d))
                    result = src;
            }
            else
                result = fs.CopyTo(src, d);
            if (!result)
            {
                Err(L"Не удалось выполнить операцию для \"" + pos[0] + L"\".");
                return;
            }
            if (!newName.empty() && newName != result->name)
                fs.Rename(result, newName);
            if (!ps)
                Out(move ? L"Перемещено файлов:         1." : L"Скопировано файлов:         1.");
            return;
        }
        if (cmd == L"ren" || cmd == L"rename" || cmd == L"rni" || cmd == L"rename-item")
        {
            const std::vector<std::wstring> pos = Positional(args, {L"-newname"});
            std::wstring nn = Param(args, {L"-newname"});
            if (nn.empty() && pos.size() >= 2)
                nn = pos[1];
            FsNode* n = pos.empty() ? nullptr : fs.Resolve(pos[0], s.cwd);
            if (!n || nn.empty())
            {
                Err(ps ? L"Rename-Item : Не удается переименовать указанный объект, так как он не существует." : L"Синтаксис команды неверен.");
                return;
            }
            if (!fs.Rename(n, nn))
                Err(L"Не удается переименовать элемент: недопустимое имя или элемент с таким именем уже существует.");
            return;
        }
        if (cmd == L"whoami")
        {
            Out(ToLower(GetShell().Settings().computerName) + L"\\" + ToLower(fs.UserName()));
            return;
        }
        if (cmd == L"hostname")
        {
            Out(GetShell().Settings().computerName);
            return;
        }
        if (cmd == L"date" || cmd == L"get-date" || cmd == L"time")
        {
            const SYSTEMTIME now = LocalNow();
            if (!ps && cmd == L"date")
                Out(L"Текущая дата: " + FormatDate(now));
            else if (!ps)
                Out(L"Текущее время: " + FormatTime(now) + L":" + Pad2(now.wSecond) + L"," + Pad2(now.wMilliseconds / 10));
            else
            {
                Out(L"");
                Out(std::wstring(WeekdayName(now.wDayOfWeek)) + L", " + std::to_wstring(now.wDay) + L" " + MonthNameGenitive(now.wMonth) + L" " +
                    std::to_wstring(now.wYear) + L" г. " + std::to_wstring(now.wHour) + L":" + Pad2(now.wMinute) + L":" + Pad2(now.wSecond));
                Out(L"");
            }
            return;
        }
        if (cmd == L"ver" || cmd == L"winver")
        {
            if (cmd == L"winver")
            {
                GetShell().ShowMessage(L"Windows: сведения", L"Windows 11 Pro\nВерсия 23H2 (сборка ОС 22631.4317)\n\n"
                                                             L"Демонстрационная копия интерфейса.");
                return;
            }
            Out(L"");
            Out(L"Windows [Version 10.0.22631.4317]");
            return;
        }
        if (cmd == L"$psversiontable" || cmd == L"get-host")
        {
            Out(L"");
            Out(L"Name                           Value");
            Out(L"----                           -----");
            Out(L"PSVersion                      5.1.22621.4111");
            Out(L"PSEdition                      Desktop");
            Out(L"PSCompatibleVersions           {1.0, 2.0, 3.0, 4.0...}");
            Out(L"BuildVersion                   10.0.22621.4111");
            Out(L"CLRVersion                     4.0.30319.42000");
            Out(L"WSManStackVersion              3.0");
            Out(L"PSRemotingProtocolVersion      2.3");
            Out(L"SerializationVersion           1.1.0.1");
            Out(L"");
            return;
        }
        if (cmd == L"ipconfig")
        {
            Ipconfig(HasFlag(args, {L"/all"}));
            return;
        }
        if (cmd == L"ping" || cmd == L"test-connection")
        {
            const std::vector<std::wstring> pos = Positional(args, {L"-n", L"-count"});
            if (pos.empty())
            {
                Out(L"Использование: ping узел");
                return;
            }
            Ping(s, pos[0]);
            return;
        }
        if (cmd == L"systeminfo")
        {
            SystemInfo();
            return;
        }
        if (cmd == L"tasklist" || cmd == L"ps" || cmd == L"gps" || cmd == L"get-process")
        {
            Processes(ps && cmd != L"tasklist");
            return;
        }
        if (cmd == L"taskkill" || cmd == L"kill" || cmd == L"stop-process" || cmd == L"spps")
        {
            std::wstring name = Param(args, {L"/im", L"-name", L"-processname"});
            std::wstring pid = Param(args, {L"/pid", L"-id"});
            const std::vector<std::wstring> pos = Positional(args, {L"/im", L"-name", L"/pid", L"-id", L"-processname"});
            if (name.empty() && pid.empty() && !pos.empty())
            {
                if (pos[0].find_first_not_of(L"0123456789") == std::wstring::npos)
                    pid = pos[0];
                else
                    name = pos[0];
            }
            Kill(name, pid, ps);
            return;
        }
        if (cmd == L"history" || cmd == L"h" || cmd == L"get-history" || cmd == L"doskey")
        {
            Out(L"");
            Out(L"  Id CommandLine");
            Out(L"  -- -----------");
            for (size_t i = 0; i < s.history.size(); ++i)
                Out(PadLeft(std::to_wstring(i + 1), 4) + L" " + s.history[i]);
            Out(L"");
            return;
        }
        if (cmd == L"title")
        {
            s.title = Trim(line.substr(5));
            UpdateTitle();
            return;
        }
        if (cmd == L"color")
        {
            Out(L"Цвета консоли задаются в параметрах Терминала (схема Campbell).");
            return;
        }
        if (cmd == L"set" || cmd == L"env")
        {
            Out(L"ALLUSERSPROFILE=C:\\ProgramData");
            Out(L"COMPUTERNAME=" + GetShell().Settings().computerName);
            Out(L"HOMEDRIVE=C:");
            Out(L"HOMEPATH=" + PathStr(fs.Home()).substr(2));
            Out(L"NUMBER_OF_PROCESSORS=8");
            Out(L"OS=Windows_NT");
            Out(L"PROCESSOR_ARCHITECTURE=AMD64");
            Out(L"SystemRoot=C:\\Windows");
            Out(L"USERNAME=" + fs.UserName());
            Out(L"USERPROFILE=" + PathStr(fs.Home()));
            Out(L"windir=C:\\Windows");
            return;
        }
        if (cmd == L"powershell" || cmd == L"pwsh")
        {
            if (s.kind == ShellKind::Cmd)
            {
                s.stack.push_back(s.kind);
                s.kind = ShellKind::PowerShell;
                Banner(s);
                UpdateTitle();
            }
            return;
        }
        if (cmd == L"cmd")
        {
            if (s.kind == ShellKind::PowerShell)
            {
                s.stack.push_back(s.kind);
                s.kind = ShellKind::Cmd;
                Out(L"Windows [Version 10.0.22631.4317]");
                Out(L"(c) Демонстрационная версия. Все права защищены.");
                Out(L"");
                UpdateTitle();
            }
            return;
        }
        if (cmd == L"get-command" || cmd == L"gcm")
        {
            Out(L"");
            Out(L"CommandType     Name                                               Version    Source");
            Out(L"-----------     ----                                               -------    ------");
            const wchar_t* cmds[] = {L"Clear-Host", L"Copy-Item", L"Get-ChildItem", L"Get-Command", L"Get-Content", L"Get-Date", L"Get-History",
                                     L"Get-Location", L"Get-Process", L"Move-Item", L"New-Item", L"Remove-Item", L"Rename-Item",
                                     L"Set-Location", L"Start-Process", L"Stop-Process", L"Test-Connection", L"Write-Output"};
            for (const wchar_t* c : cmds)
                Out(L"Cmdlet          " + PadRight(c, 51) + L"3.1.0.0    Microsoft.PowerShell.Management");
            Out(L"");
            return;
        }
        if (cmd == L"shutdown")
        {
            if (HasFlag(args, {L"/a"}))
            {
                Out(L"Выход из системы отменён.");
                return;
            }
            const bool restart = HasFlag(args, {L"/r"});
            if (!restart && !HasFlag(args, {L"/s"}))
            {
                Out(L"Использование: shutdown [/s | /r | /a]");
                return;
            }
            Out(restart ? L"Компьютер будет перезагружен." : L"Компьютер будет выключен.", kYellow);
            PostAction([restart]() { GetShell().Shutdown(restart); });
            return;
        }
        // Запуск приложений и файлов.
        if (cmd == L"start" || cmd == L"start-process" || cmd == L"saps" || cmd == L"ii" || cmd == L"invoke-item")
        {
            const std::vector<std::wstring> pos = Positional(args, {L"-filepath", L"-argumentlist"});
            std::wstring target = pos.empty() ? Param(args, {L"-filepath"}) : pos[0];
            if (target.empty())
            {
                const ShellKind kind = s.kind;
                PostAction([this, kind]() { NewSession(kind); });
                return;
            }
            const std::wstring argument = pos.size() > 1 ? pos[1] : Param(args, {L"-argumentlist"});
            if (!LaunchNamed(s, ToLower(target), argument))
            {
                if (FsNode* n = fs.Resolve(target, s.cwd))
                    GetShell().Open(n);
                else
                    Err(L"Не удается найти \"" + target + L"\". Проверьте правильность имени и повторите попытку.");
            }
            return;
        }
        if (LaunchNamed(s, cmd, args.empty() ? L"" : args[0]))
            return;
        // Файл в текущей папке (например, «.\Заметки.txt»).
        if (FsNode* n = fs.Resolve(tok[0], s.cwd))
        {
            if (n->IsFolder())
            {
                if (ps)
                    Out(L"Используйте cd, чтобы перейти в папку.");
                else
                    s.cwd = n;
            }
            else
                GetShell().Open(n);
            return;
        }
        if (ps)
            PsError(tok[0],
                    L"Имя \"" + tok[0] + L"\" не распознано как имя командлета, функции, файла сценария или выполняемой программы. "
                                         L"Проверьте правильность написания имени, а также наличие и правильность пути, после чего повторите попытку.",
                    line, L"ObjectNotFound: (" + tok[0] + L":String) [], CommandNotFoundException", L"CommandNotFoundException");
        else
        {
            Out(L"\"" + tok[0] + L"\" не является внутренней или внешней");
            Out(L"командой, исполняемой программой или пакетным файлом.");
        }
    }

    // Запуск встроенных приложений по имени исполняемого файла.
    bool LaunchNamed(Session& s, const std::wstring& name, const std::wstring& arg)
    {
        VirtualFS& fs = GetShell().FS();
        Shell& sh = GetShell();
        auto argNode = [&]() -> std::wstring {
            if (arg.empty())
                return L"";
            FsNode* n = fs.Resolve(arg, s.cwd);
            if (!n && name == L"notepad" && VirtualFS::ValidName(arg))
            {
                const size_t sl = arg.find_last_of(L"\\/");
                FsNode* dir = sl == std::wstring::npos ? s.cwd : fs.Resolve(arg.substr(0, sl + 1), s.cwd);
                if (dir)
                    n = fs.NewFile(dir, sl == std::wstring::npos ? arg : arg.substr(sl + 1), FileKind::Text, L"");
            }
            return n ? VirtualFS::NodeArg(n) : L"";
        };
        if (name == L"notepad")
            sh.Launch(AppId::Notepad, argNode());
        else if (name == L"calc" || name == L"calculator")
            sh.Launch(AppId::Calculator);
        else if (name == L"explorer")
            sh.Launch(AppId::Explorer, arg.empty() ? L"::home" : (arg == L"." ? VirtualFS::NodeArg(s.cwd) : argNode()));
        else if (name == L"mspaint" || name == L"paint" || name == L"pbrush")
            sh.Launch(AppId::Paint, argNode());
        else if (name == L"taskmgr")
            sh.Launch(AppId::TaskManager);
        else if (name == L"control" || name.rfind(L"ms-settings:", 0) == 0)
        {
            const size_t c = name.find(L':');
            sh.Launch(AppId::Settings, c == std::wstring::npos ? L"" : name.substr(c + 1));
        }
        else if (name == L"wt" || name == L"wt.exe")
            sh.Launch(AppId::Terminal);
        else if (name.rfind(L"http://", 0) == 0 || name.rfind(L"https://", 0) == 0 || name == L"msedge" || name == L"browser")
            sh.Launch(AppId::Browser, name.rfind(L"http", 0) == 0 ? name : arg);
        else if (name == L"photos")
            sh.Launch(AppId::Photos, argNode());
        else if (name == L"clock")
            sh.Launch(AppId::Clock);
        else
            return false;
        return true;
    }

    void Help(bool ps)
    {
        Out(L"");
        Out(L"Доступные команды:", kWhite);
        const std::pair<const wchar_t*, const wchar_t*> rows[] = {
            {L"dir, ls, Get-ChildItem", L"Содержимое папки (-Recurse — вложенные папки)"},
            {L"cd, Set-Location", L"Сменить текущую папку (cd .., cd ~, cd C:\\)"},
            {L"pwd, Get-Location", L"Текущая папка"},
            {L"mkdir, md", L"Создать папку"},
            {L"New-Item, ni", L"Создать файл (-ItemType Directory — папку)"},
            {L"cat, type, Get-Content", L"Показать содержимое файла"},
            {L"echo, Write-Output", L"Вывести текст (поддерживаются > и >> для записи в файл)"},
            {L"copy, move, ren, del", L"Копирование, перемещение, переименование и удаление"},
            {L"tree", L"Дерево папок (/f — с файлами)"},
            {L"start, notepad, calc, explorer", L"Запуск приложений и открытие файлов"},
            {L"tasklist, Get-Process", L"Список процессов (окон)"},
            {L"taskkill /im, Stop-Process", L"Завершить процесс"},
            {L"ipconfig, ping, hostname", L"Сетевые команды"},
            {L"systeminfo, whoami, ver", L"Сведения о системе"},
            {L"history, cls, exit", L"Журнал команд, очистка экрана, выход"},
            {L"cmd, powershell", L"Переключение оболочки"},
            {L"| findstr, | sort, | clip", L"Фильтры конвейера"},
        };
        for (const auto& rw : rows)
        {
            TLine l;
            l.segs.push_back({L"  " + PadRight(rw.first, 34), kCyan});
            l.segs.push_back({rw.second, kFg});
            Emit(std::move(l));
        }
        Out(L"");
        Out(ps ? L"Клавиши: ↑/↓ — журнал, Tab — автодополнение, Ctrl+C — копировать/прервать, Ctrl+V — вставить."
               : L"Клавиши: ↑/↓ — журнал, Tab — автодополнение.",
            kGray);
        Out(L"");
    }

    std::vector<FsNode*> SortedChildren(FsNode* dir, const std::wstring& filter) const
    {
        std::vector<FsNode*> v;
        for (auto& c : dir->children)
        {
            if (!filter.empty() && !WildMatch(filter, c->name))
                continue;
            v.push_back(c.get());
        }
        std::stable_sort(v.begin(), v.end(), [](FsNode* a, FsNode* b) {
            if (a->IsFolder() != b->IsFolder())
                return a->IsFolder();
            return CompareNoCase(a->name, b->name) < 0;
        });
        return v;
    }

    static bool WildMatch(const std::wstring& pat, const std::wstring& s)
    {
        // * и ? без учёта регистра.
        const std::wstring p = ToLower(pat), t = ToLower(s);
        size_t pi = 0, ti = 0, star = std::wstring::npos, mark = 0;
        while (ti < t.size())
        {
            if (pi < p.size() && (p[pi] == L'?' || p[pi] == t[ti]))
            {
                ++pi;
                ++ti;
            }
            else if (pi < p.size() && p[pi] == L'*')
            {
                star = pi++;
                mark = ti;
            }
            else if (star != std::wstring::npos)
            {
                pi = star + 1;
                ti = ++mark;
            }
            else
                return false;
        }
        while (pi < p.size() && p[pi] == L'*')
            ++pi;
        return pi == p.size();
    }

    void PsListing(FsNode* dir, const std::vector<FsNode*>& items)
    {
        VirtualFS& fs = GetShell().FS();
        Out(L"");
        Out(L"");
        Out(L"    Каталог: " + PathStr(dir));
        Out(L"");
        Out(L"");
        Out(L"Mode                 LastWriteTime         Length Name");
        Out(L"----                 -------------         ------ ----");
        for (FsNode* n : items)
        {
            const std::wstring mode = n->IsFolder() ? L"d-----" : L"-a----";
            const std::wstring date = FormatDate(n->modified);
            const std::wstring len = n->IsFolder() ? L"" : std::to_wstring(fs.SizeOf(n));
            TLine l;
            l.segs.push_back({mode + L"        " + date + PadLeft(PsTime(n->modified), 10) + PadLeft(len, 15) + L" ", kFg});
            l.segs.push_back({n->name, n->IsFolder() ? kCyan : kFg});
            Emit(std::move(l));
        }
        Out(L"");
    }

    void PsDir(FsNode* dir, const std::wstring& filter, bool recurse, bool bare)
    {
        const std::vector<FsNode*> items = SortedChildren(dir, filter);
        if (bare)
        {
            for (FsNode* n : items)
                Out(n->name);
        }
        else if (!items.empty())
            PsListing(dir, items);
        if (recurse)
        {
            for (FsNode* n : items)
            {
                if (n->IsFolder())
                    PsDir(n, filter, true, bare);
            }
        }
    }

    void CmdDir(FsNode* dir, const std::wstring& filter, bool bare)
    {
        VirtualFS& fs = GetShell().FS();
        const std::vector<FsNode*> items = SortedChildren(dir, filter);
        if (bare)
        {
            for (FsNode* n : items)
                Out(n->name);
            return;
        }
        Out(L" Том в устройстве C не имеет метки.");
        Out(L" Серийный номер тома: 5E2A-7C11");
        Out(L"");
        Out(L" Содержимое папки " + PathStr(dir));
        Out(L"");
        int files = 0, dirs = 0;
        unsigned long long total = 0;
        auto row = [&](const SYSTEMTIME& st, const std::wstring& mid, const std::wstring& name) {
            Out(FormatDate(st) + L"  " + FormatTime(st) + L"    " + mid + L" " + name);
        };
        if (dir->kind != FileKind::Drive)
        {
            row(dir->modified, L"<DIR>         ", L".");
            row(dir->modified, L"<DIR>         ", L"..");
            dirs += 2;
        }
        for (FsNode* n : items)
        {
            if (n->IsFolder())
            {
                row(n->modified, L"<DIR>         ", n->name);
                ++dirs;
            }
            else
            {
                const unsigned long long sz = fs.SizeOf(n);
                row(n->modified, PadLeft(GroupDigits(static_cast<long long>(sz)), 14), n->name);
                ++files;
                total += sz;
            }
        }
        Out(PadLeft(std::to_wstring(files), 16) + L" файлов " + PadLeft(GroupDigits(static_cast<long long>(total)), 14) + L" байт");
        Out(PadLeft(std::to_wstring(dirs), 16) + L" папок  " + GroupDigits(static_cast<long long>(fs.DriveC()->freeSpace)) + L" байт свободно");
    }

    void Tree(FsNode* dir, const std::wstring& prefix, int depth, bool files)
    {
        if (depth > 6)
            return;
        std::vector<FsNode*> items;
        for (auto& c : dir->children)
        {
            if (c->IsFolder() || files)
                items.push_back(c.get());
        }
        for (size_t i = 0; i < items.size(); ++i)
        {
            const bool last = i + 1 == items.size();
            FsNode* n = items[i];
            if (!n->IsFolder())
            {
                Out(prefix + (last ? L"    " : L"│   ") + n->name);
                continue;
            }
            Out(prefix + (last ? L"└───" : L"├───") + n->name);
            Tree(n, prefix + (last ? L"    " : L"│   "), depth + 1, files);
        }
    }

    void Ipconfig(bool all)
    {
        const ShellSettings& st = GetShell().Settings();
        Out(L"");
        Out(L"Настройка протокола IP для Windows");
        Out(L"");
        if (all)
        {
            Out(L"   Имя компьютера . . . . . . . . . : " + st.computerName);
            Out(L"   Основной DNS-суффикс . . . . . . :");
            Out(L"   Тип узла . . . . . . . . . . . . : Гибридный");
            Out(L"   IP-маршрутизация включена . . . . : Нет");
            Out(L"");
        }
        Out(L"Адаптер беспроводной локальной сети Беспроводная сеть:");
        Out(L"");
        if (!st.wifi || st.airplane)
        {
            Out(L"   Состояние среды. . . . . . . . : Среда передачи недоступна.");
            Out(L"   DNS-суффикс подключения . . . . . :");
        }
        else
        {
            Out(L"   DNS-суффикс подключения . . . . . : home");
            if (all)
            {
                Out(L"   Описание. . . . . . . . . . . . . : Беспроводной сетевой адаптер Wi-Fi 6");
                Out(L"   Физический адрес. . . . . . . . . : 3C-A9-F4-1D-62-7B");
                Out(L"   DHCP включен. . . . . . . . . . . : Да");
            }
            Out(L"   Локальный IPv6-адрес канала . . . : fe80::1c2b:3a4d:5e6f:7081%12");
            Out(L"   IPv4-адрес. . . . . . . . . . . . : 192.168.1.42");
            Out(L"   Маска подсети . . . . . . . . . . : 255.255.255.0");
            Out(L"   Основной шлюз. . . . . . . . . : 192.168.1.1");
            if (all)
                Out(L"   DNS-серверы. . . . . . . . . . . : 192.168.1.1");
        }
        Out(L"");
        Out(L"Адаптер Ethernet Ethernet:");
        Out(L"");
        Out(L"   Состояние среды. . . . . . . . : Среда передачи недоступна.");
        Out(L"   DNS-суффикс подключения . . . . . :");
        Out(L"");
    }

    void Ping(Session& s, const std::wstring& host)
    {
        const ShellSettings& st = GetShell().Settings();
        unsigned h = 2166136261u;
        for (wchar_t c : host)
            h = (h ^ static_cast<unsigned>(c)) * 16777619u;
        const bool local = host == L"localhost" || host == L"127.0.0.1";
        const std::wstring ip = local ? L"127.0.0.1"
                                      : std::to_wstring(20 + h % 200) + L"." + std::to_wstring((h >> 8) % 256) + L"." +
                                            std::to_wstring((h >> 16) % 256) + L"." + std::to_wstring(1 + (h >> 24) % 254);
        const bool online = local || (st.wifi && !st.airplane);
        Out(L"");
        if (!online)
        {
            Out(L"При проверке связи не удалось обнаружить узел " + host + L".");
            Out(L"Проверьте имя узла и повторите попытку.");
            return;
        }
        const bool isIp = host.find_first_not_of(L"0123456789.") == std::wstring::npos;
        Out(L"Обмен пакетами с " + host + (isIp ? L"" : L" [" + ip + L"]") + L" с 32 байтами данных:");
        const double now = NowSeconds();
        int times[4];
        for (int i = 0; i < 4; ++i)
            times[i] = local ? 0 : 12 + static_cast<int>((h >> (i * 3)) % 25);
        for (int i = 0; i < 4; ++i)
        {
            Pending p;
            p.at = now + 0.4 + static_cast<double>(i) * 1.0;
            p.line.segs.push_back({L"Ответ от " + ip + L": число байт=32 время" + (local ? std::wstring(L"<1мс") : L"=" + std::to_wstring(times[i]) + L"мс") +
                                       L" TTL=" + (local ? L"128" : L"56"),
                                   kFg});
            s.pending.push_back(std::move(p));
        }
        const int mn = *std::min_element(times, times + 4), mx = *std::max_element(times, times + 4);
        const int avg = (times[0] + times[1] + times[2] + times[3]) / 4;
        const wchar_t* tail[] = {L"", nullptr, L"    Пакетов: отправлено = 4, получено = 4, потеряно = 0", L"    (0% потерь)",
                                 L"Приблизительное время приема-передачи в мс:", nullptr};
        for (int i = 0; i < 6; ++i)
        {
            Pending p;
            p.at = now + 3.5;
            std::wstring text = tail[i] ? tail[i] : L"";
            if (i == 1)
                text = L"Статистика Ping для " + ip + L":";
            if (i == 5)
                text = L"    Минимальное = " + std::to_wstring(mn) + L"мсек, Максимальное = " + std::to_wstring(mx) + L" мсек, Среднее = " +
                       std::to_wstring(avg) + L" мсек";
            if (!text.empty())
                p.line.segs.push_back({text, kFg});
            s.pending.push_back(std::move(p));
        }
    }

    void SystemInfo()
    {
        const ShellSettings& st = GetShell().Settings();
        MEMORYSTATUSEX ms{};
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);
        const unsigned long long totalMb = ms.ullTotalPhys / (1024ULL * 1024ULL);
        const unsigned long long freeMb = ms.ullAvailPhys / (1024ULL * 1024ULL);
        const std::pair<const wchar_t*, std::wstring> rows[] = {
            {L"Имя узла", st.computerName},
            {L"Название ОС", L"Windows 11 Pro"},
            {L"Версия ОС", L"10.0.22631 Н/Д построение 22631"},
            {L"Конфигурация ОС", L"Изолированная рабочая станция"},
            {L"Тип сборки ОС", L"Multiprocessor Free"},
            {L"Зарегистрированный владелец", GetShell().FS().UserName()},
            {L"Дата установки", L"15.03.2026, 10:24:51"},
            {L"Изготовитель системы", L"Демонстрационный ПК"},
            {L"Тип системы", L"x64-based PC"},
            {L"Папка Windows", L"C:\\Windows"},
            {L"Язык системы", L"ru;Русский"},
            {L"Часовой пояс", L"(UTC+03:00) Москва, Санкт-Петербург"},
            {L"Полный объем физической памяти", GroupDigits(static_cast<long long>(totalMb)) + L" МБ"},
            {L"Доступная физическая память", GroupDigits(static_cast<long long>(freeMb)) + L" МБ"},
            {L"Сетевые адаптеры", L"Установлено сетевых адаптеров: 2."},
        };
        Out(L"");
        for (const auto& rw : rows)
            Out(PadRight(std::wstring(rw.first) + L":", 34) + rw.second);
        Out(L"");
    }

    struct Proc
    {
        std::wstring name;
        int pid;
        int memKb;
        Window* win;
    };

    std::vector<Proc> ProcessList() const
    {
        std::vector<Proc> v = {{L"System", 4, 152, nullptr},     {L"Registry", 132, 48120, nullptr},
                               {L"smss", 520, 1240, nullptr},     {L"csrss", 744, 5880, nullptr},
                               {L"wininit", 852, 6900, nullptr},  {L"services", 924, 10220, nullptr},
                               {L"lsass", 948, 22410, nullptr},   {L"svchost", 1088, 31212, nullptr},
                               {L"dwm", 1356, 88420, nullptr},    {L"explorer", 4120, 132080, nullptr},
                               {L"SearchHost", 5232, 64110, nullptr}};
        for (auto& w : GetShell().WM().Windows())
        {
            if (w->IsClosing() || w->App() == AppId::System)
                continue;
            const int pid = 6000 + static_cast<int>((w->Id() * 2654435761u) % 20000u) / 4 * 4;
            v.push_back({ExeName(w->App()), pid, 30000 + static_cast<int>((w->Id() * 7919u) % 90000u), w.get()});
        }
        return v;
    }

    void Processes(bool psStyle)
    {
        const std::vector<Proc> list = ProcessList();
        Out(L"");
        if (psStyle)
        {
            Out(L"Handles  NPM(K)    PM(K)      WS(K)     CPU(s)     Id  SI ProcessName");
            Out(L"-------  ------    -----      -----     ------     --  -- -----------");
            for (const Proc& p : list)
            {
                const int handles = 200 + (p.pid * 13) % 1800;
                Out(PadLeft(std::to_wstring(handles), 7) + PadLeft(std::to_wstring(8 + p.memKb % 40), 8) + PadLeft(std::to_wstring(p.memKb * 3 / 4), 9) +
                    PadLeft(std::to_wstring(p.memKb), 11) + PadLeft(NumText(static_cast<double>((p.pid % 997) / 100.0)), 11) +
                    PadLeft(std::to_wstring(p.pid), 7) + PadLeft(p.pid < 1000 ? L"0" : L"1", 4) + L" " + p.name);
            }
        }
        else
        {
            Out(L"Имя образа                     PID Имя сессии          № сеанса       Память");
            Out(L"========================= ======== ================ =========== ============");
            for (const Proc& p : list)
                Out(PadRight(p.name + (p.name == L"System" || p.name == L"Registry" ? L"" : L".exe"), 25) + PadLeft(std::to_wstring(p.pid), 9) +
                    L" " + PadRight(p.pid < 1000 ? L"Services" : L"Console", 16) + PadLeft(p.pid < 1000 ? L"0" : L"1", 12) +
                    PadLeft(GroupDigits(p.memKb) + L" КБ", 13));
        }
        Out(L"");
    }

    void Kill(const std::wstring& rawName, const std::wstring& pid, bool ps)
    {
        std::wstring name = ToLower(rawName);
        if (name.size() > 4 && name.substr(name.size() - 4) == L".exe")
            name = name.substr(0, name.size() - 4);
        int killed = 0;
        for (const Proc& p : ProcessList())
        {
            const bool match = (!name.empty() && ToLower(p.name) == name) || (!pid.empty() && std::to_wstring(p.pid) == pid);
            if (!match)
                continue;
            if (!p.win)
            {
                Err(ps ? L"Stop-Process : Не удается остановить процесс \"" + p.name + L" (" + std::to_wstring(p.pid) + L")\": Отказано в доступе."
                       : L"ОШИБКА: Не удается завершить процесс с PID " + std::to_wstring(p.pid) + L". Отказано в доступе.");
                continue;
            }
            Window* w = p.win;
            PostAction([w]() {
                if (GetShell().WM().Contains(w))
                    w->ForceClose();
            });
            if (!ps)
                Out(L"УСПЕШНО: Процесс \"" + p.name + L".exe\" с PID " + std::to_wstring(p.pid) + L" был завершён.", kGreen);
            ++killed;
        }
        if (killed == 0 && !(name.empty() && pid.empty()))
        {
            if (ps)
                Err(L"Stop-Process : Не удается найти процесс с именем \"" + (name.empty() ? pid : rawName) + L"\".");
            else
                Out(L"ОШИБКА: процесс \"" + (name.empty() ? pid : rawName) + L"\" не найден.");
        }
    }

    // ================================================================== автодополнение
    void Complete(Session& s, bool backwards)
    {
        if (!s.completions.empty() && s.input == s.compLast)
        {
            const size_t n = s.completions.size();
            s.compIndex = backwards ? (s.compIndex + n - 1) % n : (s.compIndex + 1) % n;
            s.input = s.compPrefix + s.completions[s.compIndex];
            s.caret = s.input.size();
            s.compLast = s.input;
            return;
        }
        s.completions.clear();
        // Последний аргумент (с учётом кавычек).
        size_t start = 0;
        bool q = false;
        for (size_t i = 0; i < s.input.size(); ++i)
        {
            if (s.input[i] == L'"' || s.input[i] == L'\'')
                q = !q;
            else if (s.input[i] == L' ' && !q)
                start = i + 1;
        }
        std::wstring word = s.input.substr(start);
        if (!word.empty() && (word[0] == L'"' || word[0] == L'\''))
            word.erase(0, 1);
        if (start == 0 && word.find_first_of(L"\\/.") == std::wstring::npos)
        {
            // Имя команды.
            const wchar_t* cmds[] = {L"cd", L"cls", L"copy", L"del", L"dir", L"echo", L"exit", L"explorer", L"Get-ChildItem", L"Get-Content",
                                     L"Get-Date", L"Get-Location", L"Get-Process", L"help", L"history", L"hostname", L"ipconfig", L"ls",
                                     L"mkdir", L"move", L"New-Item", L"notepad", L"ping", L"pwd", L"Remove-Item", L"ren", L"Set-Location",
                                     L"Start-Process", L"Stop-Process", L"systeminfo", L"tasklist", L"tree", L"type", L"whoami"};
            for (const wchar_t* c : cmds)
            {
                if (StartsWithNoCase(c, word))
                    s.completions.push_back(c);
            }
        }
        else
        {
            const size_t sl = word.find_last_of(L"\\/");
            const std::wstring dirPart = sl == std::wstring::npos ? L"" : word.substr(0, sl + 1);
            const std::wstring base = sl == std::wstring::npos ? word : word.substr(sl + 1);
            std::wstring dp = dirPart;
            if (dp.rfind(L".\\", 0) == 0)
                dp = dp.substr(2);
            FsNode* dir = dirPart.empty() ? s.cwd : GetShell().FS().Resolve(dp.empty() ? L"." : dp, s.cwd);
            if (dir && dir->IsFolder())
            {
                // Английские имена системных папок (Documents → «Документы») — как в реальной системе.
                static const std::pair<const wchar_t*, const wchar_t*> aliases[] = {
                    {L"Desktop", L"Рабочий стол"}, {L"Documents", L"Документы"}, {L"Downloads", L"Загрузки"}, {L"Music", L"Музыка"},
                    {L"Pictures", L"Изображения"},  {L"Videos", L"Видео"},        {L"Users", L"Пользователи"},
                };
                for (const auto& a : aliases)
                {
                    if (!base.empty() && StartsWithNoCase(a.first, base) && GetShell().FS().Find(dir, a.second))
                        s.completions.push_back((s.kind == ShellKind::PowerShell && dirPart.empty() ? L".\\" : dirPart) + std::wstring(a.first));
                }
                for (FsNode* c : SortedChildren(dir, L""))
                {
                    if (!StartsWithNoCase(c->name, base))
                        continue;
                    std::wstring full = dirPart + c->name;
                    if (s.kind == ShellKind::PowerShell && dirPart.empty())
                        full = L".\\" + c->name;
                    if (full.find(L' ') != std::wstring::npos)
                        full = s.kind == ShellKind::PowerShell ? L"'" + full + L"'" : L"\"" + full + L"\"";
                    s.completions.push_back(full);
                }
            }
        }
        if (s.completions.empty())
            return;
        s.compPrefix = s.input.substr(0, start);
        s.compIndex = backwards ? s.completions.size() - 1 : 0;
        s.input = s.compPrefix + s.completions[s.compIndex];
        s.caret = s.input.size();
        s.compLast = s.input;
    }

    // ================================================================== отрисовка
    float FontSize() const { return 16.0f * m_zoom; }
    float CharW() const
    {
        Renderer& r = Renderer::Get();
        if (m_cwSize != FontSize() || m_cwScale != r.Scale())
        {
            m_cwSize = FontSize();
            m_cwScale = r.Scale();
            m_cw = r.MonoCharWidth(m_cwSize, FontFace::Terminal);
            m_lh = std::ceil(r.LineHeight(m_cwSize, FontWeight::Regular, FontFace::Terminal));
        }
        return m_cw;
    }
    float LineH() const
    {
        CharW();
        return m_lh;
    }
    Rect TextArea() const { return {kPadX, kTitleH + kPadY, Width() - kPadX - 16.0f, Height() - kTitleH - kPadY * 2.0f}; }
    int Cols() const { return std::max(10, static_cast<int>(std::floor(TextArea().w / CharW()))); }
    int VisibleRows() const { return std::max(1, static_cast<int>(std::floor(TextArea().h / LineH()))); }

    // Визуальная строка: участок логической строки [start, start + len).
    struct VRow
    {
        const TLine* line;
        size_t start;
        size_t len;
    };

    // Логические строки сеанса + строка ввода (если не занят).
    void BuildRows(const Session& s, std::vector<VRow>& rows, TLine& live, size_t& cursorRow, size_t& cursorCol) const
    {
        rows.clear();
        const size_t cols = static_cast<size_t>(Cols());
        auto add = [&](const TLine& l) {
            const size_t n = l.Length();
            if (n == 0)
            {
                rows.push_back({&l, 0, 0});
                return;
            }
            for (size_t st = 0; st < n; st += cols)
                rows.push_back({&l, st, std::min(cols, n - st)});
        };
        for (const TLine& l : s.lines)
            add(l);
        cursorRow = static_cast<size_t>(-1);
        cursorCol = 0;
        if (s.pending.empty())
        {
            live.segs.clear();
            live.segs.push_back({Prompt(s), kFg});
            live.segs.push_back({s.input, kWhite});
            const size_t first = rows.size();
            add(live);
            const size_t pos = Prompt(s).size() + s.caret;
            cursorRow = first + pos / cols;
            cursorCol = pos % cols;
            if (cursorRow >= rows.size())
                rows.push_back({&live, live.Length(), 0});
        }
    }

    void DrawContent(Renderer& r, const Rect& /*client*/) override
    {
        const Palette& p = Theme::P();
        Tick();
        // Вкладки
        std::vector<TabInfo> infos;
        for (const auto& sp : m_tabs)
        {
            TabInfo ti;
            ti.title = ShellTitle(*sp);
            ti.glyph = Icon::Terminal;
            infos.push_back(ti);
        }
        m_strip.Draw(r, Width(), infos, m_cur, kBg, p.text, p.subtleHover, p.divider, kWhite);
        // Консоль
        const Rect area{0.0f, kTitleH, Width(), Height() - kTitleH};
        r.FillRect(area, kBg);
        const Session& s = Cur();
        std::vector<VRow> rows;
        TLine live;
        size_t curRow = 0, curCol = 0;
        BuildRows(s, rows, live, curRow, curCol);
        const Rect ta = TextArea();
        const float lh = LineH(), cw = CharW(), fs = FontSize();
        const int vis = VisibleRows();
        const int total = static_cast<int>(rows.size());
        const int maxScroll = std::max(0, total - vis);
        m_maxScroll = maxScroll;
        const int scroll = std::min(maxScroll, static_cast<int>(s.scroll));
        const int first = std::max(0, total - vis - scroll);
        m_firstRow = first;
        r.PushClip(area);
        for (int i = 0; i < vis && first + i < total; ++i)
        {
            const VRow& vr = rows[static_cast<size_t>(first + i)];
            const float y = ta.y + static_cast<float>(i) * lh;
            // Выделение
            if (m_sel)
            {
                int a0 = m_selA.first, b0 = m_selB.first;
                int ca = m_selA.second, cb = m_selB.second;
                if (a0 > b0 || (a0 == b0 && ca > cb))
                {
                    std::swap(a0, b0);
                    std::swap(ca, cb);
                }
                const int row = first + i;
                if (row >= a0 && row <= b0)
                {
                    const int c0 = row == a0 ? ca : 0;
                    const int c1 = row == b0 ? cb : static_cast<int>(vr.len) + 1;
                    if (c1 > c0)
                        r.FillRect({ta.x + static_cast<float>(c0) * cw, y, static_cast<float>(c1 - c0) * cw, lh}, Color(255, 255, 255, 0x50));
                }
            }
            // Сегменты
            size_t off = 0;
            for (const Seg& sg : vr.line->segs)
            {
                const size_t s0 = off, s1 = off + sg.text.size();
                off = s1;
                const size_t a = std::max(s0, vr.start), b = std::min(s1, vr.start + vr.len);
                if (b <= a)
                    continue;
                const std::wstring piece = sg.text.substr(a - s0, b - a);
                r.Text(piece, {ta.x + static_cast<float>(a - vr.start) * cw, y, static_cast<float>(piece.size()) * cw + 40.0f, lh}, sg.color, fs,
                       FontWeight::Regular, TextFlags::VCenter | TextFlags::NoEllipsis, FontFace::Terminal);
            }
        }
        // Курсор
        if (curRow != static_cast<size_t>(-1) && IsActive() && !HasDialog())
        {
            const int vrow = static_cast<int>(curRow) - first;
            if (vrow >= 0 && vrow < vis)
            {
                r.NoteCaret();
                if (std::fmod(NowSeconds() - m_blink, 1.06) < 0.53)
                {
                    const float cx = std::floor(ta.x + static_cast<float>(curCol) * cw) + 0.5f;
                    const float cy = ta.y + static_cast<float>(vrow) * lh;
                    r.FillRect({cx - 0.5f, cy + 1.0f, 1.5f, lh - 2.0f}, kWhite);
                }
            }
        }
        r.PopClip();
        // Полоса прокрутки
        if (maxScroll > 0)
        {
            const float trackY = kTitleH + 4.0f, trackH = Height() - kTitleH - 8.0f;
            const float th = std::max(24.0f, trackH * static_cast<float>(vis) / static_cast<float>(total));
            const float t = 1.0f - static_cast<float>(scroll) / static_cast<float>(maxScroll);
            const bool wide = m_barHover || m_dragBar;
            const float w = wide ? 6.0f : 2.0f;
            r.FillRoundRect({Width() - 5.0f - w, trackY + (trackH - th) * t, w, th}, w * 0.5f, Color(255, 255, 255, 0x8B));
        }
        m_ui.Draw(r);
    }

    // Отложенный вывод (ping и т. п.).
    void Tick()
    {
        const double now = NowSeconds();
        for (auto& sp : m_tabs)
        {
            Session& s = *sp;
            bool changed = false;
            while (!s.pending.empty() && s.pending.front().at <= now)
            {
                Pending pd = std::move(s.pending.front());
                s.pending.erase(s.pending.begin());
                m_target = &s;
                Emit(std::move(pd.line));
                m_target = nullptr;
                if (pd.action)
                    pd.action();
                changed = true;
            }
            if (changed && s.pending.empty() && s.kind == ShellKind::Cmd)
            {
                m_target = &s;
                Out(L"");
                m_target = nullptr;
            }
            if (!s.pending.empty())
                RequestFrame();
        }
    }

    // ================================================================== ввод
    bool IsDragArea(Point local) const override
    {
        if (local.y >= kTitleH || local.x > Width() - 46.0f * 3.0f)
            return false;
        const int n = static_cast<int>(m_tabs.size());
        if (m_strip.HitTest(local, Width(), n, nullptr) >= 0 || m_strip.NewRect(Width(), n).Contains(local) ||
            m_strip.ExtraRect(Width(), n).Contains(local))
            return false;
        return true;
    }

    std::pair<int, int> CellAt(Point pt) const
    {
        const Rect ta = TextArea();
        const int row = m_firstRow + static_cast<int>(std::floor((pt.y - ta.y) / LineH()));
        const int col = std::max(0, static_cast<int>(std::floor((pt.x - ta.x) / CharW() + 0.5f)));
        return {row, col};
    }

    std::wstring SelectionText() const
    {
        if (!m_sel)
            return L"";
        const Session& s = Cur();
        std::vector<VRow> rows;
        TLine live;
        size_t cr = 0, cc = 0;
        BuildRows(s, rows, live, cr, cc);
        auto a = m_selA, b = m_selB;
        if (a.first > b.first || (a.first == b.first && a.second > b.second))
            std::swap(a, b);
        std::wstring out;
        for (int row = std::max(0, a.first); row <= b.first && row < static_cast<int>(rows.size()); ++row)
        {
            const VRow& vr = rows[static_cast<size_t>(row)];
            const std::wstring full = vr.line->Text();
            const std::wstring text = vr.start < full.size() ? full.substr(vr.start, vr.len) : L"";
            const size_t c0 = row == a.first ? static_cast<size_t>(a.second) : 0;
            const size_t c1 = std::min(text.size(), row == b.first ? static_cast<size_t>(b.second) : text.size());
            if (c1 > c0)
                out += text.substr(c0, c1 - c0);
            // Перенос строки только между логическими строками.
            if (row != b.first && !(row + 1 < static_cast<int>(rows.size()) && rows[static_cast<size_t>(row + 1)].line == vr.line))
                out += L"\r\n";
        }
        return out;
    }

    void OnMouseDown(const MouseEvent& e) override
    {
        if (HasDialog())
        {
            Window::OnMouseDown(e);
            return;
        }
        const Point pt = e.pos;
        if (pt.y < kTitleH)
        {
            const int n = static_cast<int>(m_tabs.size());
            bool onClose = false;
            const int tab = m_strip.HitTest(pt, Width(), n, &onClose);
            if (tab >= 0)
            {
                if (onClose)
                    CloseTab(tab);
                else
                {
                    m_cur = tab;
                    m_sel = false;
                    UpdateTitle();
                }
            }
            else if (m_strip.NewRect(Width(), n).Contains(pt))
                NewSession(Cur().kind);
            else if (m_strip.ExtraRect(Width(), n).Contains(pt))
                ShowProfiles(LocalToScreen(m_strip.ExtraRect(Width(), n)));
            return;
        }
        if (pt.x >= Width() - 14.0f && m_maxScroll > 0 && e.button == MouseButton::Left)
        {
            m_dragBar = true;
            DragBar(pt);
            return;
        }
        if (e.button == MouseButton::Left)
        {
            m_selA = m_selB = CellAt(pt);
            m_sel = false;
            m_selecting = true;
        }
    }

    void DragBar(Point pt)
    {
        const float trackY = kTitleH + 4.0f, trackH = Height() - kTitleH - 8.0f;
        const float t = Saturate((pt.y - trackY) / trackH);
        Cur().scroll = std::floor((1.0f - t) * static_cast<float>(m_maxScroll));
    }

    void OnMouseMove(const MouseEvent& e) override
    {
        Window::OnMouseMove(e);
        const Point pt = e.pos;
        m_strip.UpdateHover(pt, Width(), static_cast<int>(m_tabs.size()));
        m_barHover = pt.x >= Width() - 14.0f && pt.y > kTitleH;
        if (m_dragBar)
        {
            DragBar(pt);
            return;
        }
        if (m_selecting)
        {
            m_selB = CellAt(pt);
            m_sel = m_selB != m_selA;
            // Автопрокрутка при выделении за краем.
            if (pt.y < TextArea().y)
                Cur().scroll = std::min(static_cast<float>(m_maxScroll), Cur().scroll + 1.0f);
            else if (pt.y > TextArea().Bottom())
                Cur().scroll = std::max(0.0f, Cur().scroll - 1.0f);
        }
    }

    void OnMouseUp(const MouseEvent& e) override
    {
        Window::OnMouseUp(e);
        m_selecting = false;
        m_dragBar = false;
        if (e.button == MouseButton::Right && e.pos.y > kTitleH)
        {
            // Как в Терминале: правая кнопка копирует выделение или вставляет текст.
            if (m_sel)
            {
                SetClipboardText(GetShell().Hwnd(), SelectionText());
                m_sel = false;
            }
            else
                PasteText();
        }
    }

    void OnMouseLeave() override
    {
        Window::OnMouseLeave();
        m_strip.ClearHover();
        m_barHover = false;
    }

    bool OnMouseWheel(const MouseEvent& e) override
    {
        if (e.mods.ctrl)
        {
            m_zoom = Clamp(m_zoom + (e.wheel > 0.0f ? 0.1f : -0.1f), 0.6f, 3.0f);
            return true;
        }
        Session& s = Cur();
        s.scroll = Clamp(s.scroll + e.wheel * 3.0f, 0.0f, static_cast<float>(m_maxScroll));
        return true;
    }

    CursorType Cursor(Point local) const override
    {
        if (local.y > kTitleH && local.x < Width() - 14.0f && !HasDialog())
            return CursorType::IBeam;
        return Window::Cursor(local);
    }

    void ShowProfiles(const Rect& anchor)
    {
        std::vector<MenuItem> items = {
            MenuItem(L"Windows PowerShell", Icon::Terminal, [this]() { NewSession(ShellKind::PowerShell); }, L"Ctrl+Shift+1"),
            MenuItem(L"Командная строка", Icon::Terminal, [this]() { NewSession(ShellKind::Cmd); }, L"Ctrl+Shift+2"),
            MenuItem(L"Windows PowerShell (администратор)", Icon::Shield, [this]() { NewSession(ShellKind::PowerShell, true); }),
            MenuItem::Sep(),
            MenuItem(L"Параметры", Icon::Settings, nullptr, L"Ctrl+,").Disabled(),
            MenuItem(L"Палитра команд", Icon::Search, nullptr, L"Ctrl+Shift+P").Disabled(),
            MenuItem(L"О программе", Icon::Info, [this]() {
                ShowDialog(L"О программе", L"Терминал\nВерсия: 1.20.11781.0\n\nДемонстрационная копия приложения.", {{L"ОК", true}}, nullptr);
            }),
        };
        GetShell().ShowMenuBelow(anchor, std::move(items), 300.0f);
    }

    void PasteText()
    {
        Session& s = Cur();
        std::wstring t = GetClipboardText(GetShell().Hwnd());
        // Многострочная вставка: строки выполняются по очереди, последняя остаётся в поле ввода.
        std::wstring cur;
        for (wchar_t c : t)
        {
            if (c == L'\r')
                continue;
            if (c == L'\n')
            {
                s.input.insert(s.caret, cur);
                s.caret += cur.size();
                cur.clear();
                Submit();
                continue;
            }
            if (c == L'\t')
                c = L' ';
            if (c >= 32)
                cur.push_back(c);
        }
        s.input.insert(s.caret, cur);
        s.caret += cur.size();
        s.scroll = 0.0f;
    }

    bool OnKeyDown(UINT vk, const KeyMods& mods) override
    {
        if (HasDialog())
            return true;
        Session& s = Cur();
        m_blink = NowSeconds();
        if (mods.ctrl && mods.shift)
        {
            switch (vk)
            {
            case 'T':
                NewSession(s.kind);
                return true;
            case 'W':
                CloseTab(m_cur);
                return true;
            case '1':
                NewSession(ShellKind::PowerShell);
                return true;
            case '2':
                NewSession(ShellKind::Cmd);
                return true;
            case 'C':
                if (m_sel)
                {
                    SetClipboardText(GetShell().Hwnd(), SelectionText());
                    m_sel = false;
                }
                return true;
            case 'V':
                PasteText();
                return true;
            default:
                break;
            }
        }
        if (mods.ctrl)
        {
            switch (vk)
            {
            case 'C':
                if (m_sel)
                {
                    SetClipboardText(GetShell().Hwnd(), SelectionText());
                    m_sel = false;
                    return true;
                }
                if (!s.pending.empty())
                {
                    s.pending.clear();
                    m_target = &s;
                    Out(L"^C", kFg);
                    Out(L"");
                    m_target = nullptr;
                }
                else
                {
                    TLine echo;
                    echo.segs.push_back({Prompt(s), kFg});
                    echo.segs.push_back({s.input + L"^C", kWhite});
                    m_target = &s;
                    Emit(std::move(echo));
                    m_target = nullptr;
                    s.input.clear();
                    s.caret = 0;
                }
                return true;
            case 'V':
                PasteText();
                return true;
            case 'L':
                s.lines.clear();
                return true;
            case VK_TAB:
                if (m_tabs.size() > 1)
                {
                    m_cur = (m_cur + (mods.shift ? -1 : 1) + static_cast<int>(m_tabs.size())) % static_cast<int>(m_tabs.size());
                    m_sel = false;
                    UpdateTitle();
                }
                return true;
            case VK_OEM_PLUS:
            case VK_ADD:
                m_zoom = std::min(3.0f, m_zoom + 0.1f);
                return true;
            case VK_OEM_MINUS:
            case VK_SUBTRACT:
                m_zoom = std::max(0.6f, m_zoom - 0.1f);
                return true;
            case '0':
                m_zoom = 1.0f;
                return true;
            case VK_LEFT:
            {
                size_t c = s.caret;
                while (c > 0 && s.input[c - 1] == L' ')
                    --c;
                while (c > 0 && s.input[c - 1] != L' ')
                    --c;
                s.caret = c;
                return true;
            }
            case VK_RIGHT:
            {
                size_t c = s.caret;
                while (c < s.input.size() && s.input[c] != L' ')
                    ++c;
                while (c < s.input.size() && s.input[c] == L' ')
                    ++c;
                s.caret = c;
                return true;
            }
            case VK_BACK:
            {
                size_t c = s.caret;
                while (c > 0 && s.input[c - 1] == L' ')
                    --c;
                while (c > 0 && s.input[c - 1] != L' ')
                    --c;
                s.input.erase(c, s.caret - c);
                s.caret = c;
                return true;
            }
            default:
                return true;
            }
        }
        if (!s.pending.empty())
            return true; // команда выполняется
        if (mods.shift && (vk == VK_PRIOR || vk == VK_NEXT))
        {
            const float page = static_cast<float>(VisibleRows() - 1);
            s.scroll = Clamp(s.scroll + (vk == VK_PRIOR ? page : -page), 0.0f, static_cast<float>(m_maxScroll));
            return true;
        }
        m_sel = false;
        s.scroll = 0.0f;
        if (vk != VK_TAB && vk != VK_SHIFT)
            s.completions.clear();
        switch (vk)
        {
        case VK_RETURN:
            Submit();
            return true;
        case VK_BACK:
            if (s.caret > 0)
            {
                s.input.erase(s.caret - 1, 1);
                --s.caret;
            }
            return true;
        case VK_DELETE:
            if (s.caret < s.input.size())
                s.input.erase(s.caret, 1);
            return true;
        case VK_LEFT:
            if (s.caret > 0)
                --s.caret;
            return true;
        case VK_RIGHT:
            if (s.caret < s.input.size())
                ++s.caret;
            return true;
        case VK_HOME:
            s.caret = 0;
            return true;
        case VK_END:
            s.caret = s.input.size();
            return true;
        case VK_ESCAPE:
            s.input.clear();
            s.caret = 0;
            return true;
        case VK_UP:
        case VK_DOWN:
        {
            if (s.history.empty())
                return true;
            if (s.histPos < 0)
            {
                s.savedInput = s.input;
                s.histPos = static_cast<int>(s.history.size());
            }
            s.histPos += vk == VK_UP ? -1 : 1;
            s.histPos = Clamp(s.histPos, 0, static_cast<int>(s.history.size()));
            s.input = s.histPos == static_cast<int>(s.history.size()) ? s.savedInput : s.history[static_cast<size_t>(s.histPos)];
            s.caret = s.input.size();
            return true;
        }
        case VK_TAB:
            Complete(s, mods.shift);
            return true;
        default:
            break;
        }
        return false;
    }

    bool OnChar(wchar_t ch) override
    {
        if (HasDialog())
            return true;
        Session& s = Cur();
        if (ch < 32 || ch == 127 || !s.pending.empty())
            return true;
        if (CurrentMods().ctrl && !CurrentMods().alt)
            return true;
        s.input.insert(s.caret, 1, ch);
        ++s.caret;
        s.histPos = -1;
        s.completions.clear();
        s.scroll = 0.0f;
        m_sel = false;
        m_blink = NowSeconds();
        return true;
    }

    // ================================================================== поля
    std::vector<std::unique_ptr<Session>> m_tabs;
    int m_cur = 0;
    TabStrip m_strip;
    Session* m_target = nullptr;         // сеанс для вывода (по умолчанию — текущий)
    std::vector<TLine>* m_capture = nullptr; // перехват вывода (конвейер, перенаправление)
    float m_zoom = 1.0f;
    mutable float m_cw = 9.0f;
    mutable float m_lh = 19.0f;
    mutable float m_cwSize = 0.0f;
    mutable float m_cwScale = 0.0f;
    int m_maxScroll = 0;
    int m_firstRow = 0;
    double m_blink = 0.0;
    bool m_sel = false;
    bool m_selecting = false;
    std::pair<int, int> m_selA{0, 0};
    std::pair<int, int> m_selB{0, 0};
    bool m_barHover = false;
    bool m_dragBar = false;
};
} // namespace

std::unique_ptr<Window> CreateTerminal(const std::wstring& arg) { return std::make_unique<TerminalWindow>(arg); }
