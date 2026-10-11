// Калькулятор Windows 11: обычный и инженерный режимы (приоритет операций, скобки, функции),
// строка выражения, журнал и память (боковая панель или всплывающая панель), конвертеры величин.

#include "../Shell.h"
#include "AppFactories.h"

namespace
{
enum class Mode
{
    Standard,
    Scientific,
    Converter,
};

enum class Key
{
    D0, D1, D2, D3, D4, D5, D6, D7, D8, D9,
    Dot, Neg, Add, Sub, Mul, Div, Eq,
    Percent, CE, C, Back, Inv, Sqr, Sqrt,
    // Инженерные
    Second, Pi, E, Abs, Exp, Mod, LParen, RParen, Fact, Pow, Pow10, Log, Ln,
};

struct KeyDef
{
    Key key;
    const wchar_t* label;
    const wchar_t* label2; // подпись в режиме «2nd»
    int kind;              // 0 — цифра, 1 — функция, 2 — «=»
};

// Единицы конвертеров: множитель к базовой единице (для температуры — особая формула).
struct Unit
{
    const wchar_t* name;
    const wchar_t* abbr;
    double factor;
};
struct Category
{
    const wchar_t* name;
    Icon icon;
    std::vector<Unit> units;
    int from, to;
};

const std::vector<Category>& Categories()
{
    static const std::vector<Category> cats = {
        {L"Объём", Icon::Drop,
         {{L"Миллилитры", L"мл", 0.001}, {L"Литры", L"л", 1.0}, {L"Кубические метры", L"м³", 1000.0}, {L"Чайные ложки", L"ч. л.", 0.004928922},
          {L"Столовые ложки", L"ст. л.", 0.014786765}, {L"Стаканы (США)", L"стак.", 0.2365882}, {L"Галлоны (США)", L"гал.", 3.785411784}},
         1, 0},
        {L"Длина", Icon::Line,
         {{L"Миллиметры", L"мм", 0.001}, {L"Сантиметры", L"см", 0.01}, {L"Метры", L"м", 1.0}, {L"Километры", L"км", 1000.0},
          {L"Дюймы", L"дюйм", 0.0254}, {L"Футы", L"фут", 0.3048}, {L"Ярды", L"ярд", 0.9144}, {L"Мили", L"мили", 1609.344},
          {L"Морские мили", L"мор. мили", 1852.0}},
         3, 7},
        {L"Вес и масса", Icon::Storage,
         {{L"Миллиграммы", L"мг", 0.000001}, {L"Граммы", L"г", 0.001}, {L"Килограммы", L"кг", 1.0}, {L"Тонны", L"т", 1000.0},
          {L"Унции", L"унц.", 0.028349523}, {L"Фунты", L"фунт", 0.45359237}, {L"Стоуны", L"ст", 6.35029318}},
         2, 5},
        {L"Температура", Icon::Thermometer, {{L"Цельсий", L"°C", 0.0}, {L"Фаренгейт", L"°F", 0.0}, {L"Кельвин", L"K", 0.0}}, 0, 1},
        {L"Площадь", Icon::RectShape,
         {{L"Квадратные сантиметры", L"см²", 0.0001}, {L"Квадратные метры", L"м²", 1.0}, {L"Гектары", L"га", 10000.0},
          {L"Квадратные километры", L"км²", 1000000.0}, {L"Квадратные футы", L"фут²", 0.09290304}, {L"Акры", L"акр", 4046.8564224}},
         1, 4},
        {L"Скорость", Icon::Performance,
         {{L"Метры в секунду", L"м/с", 1.0}, {L"Километры в час", L"км/ч", 1.0 / 3.6}, {L"Мили в час", L"миль/ч", 0.44704},
          {L"Узлы", L"уз", 0.514444}, {L"Мах", L"М", 340.3}},
         1, 2},
        {L"Время", Icon::Timer,
         {{L"Миллисекунды", L"мс", 0.001}, {L"Секунды", L"с", 1.0}, {L"Минуты", L"мин", 60.0}, {L"Часы", L"ч", 3600.0},
          {L"Дни", L"дн.", 86400.0}, {L"Недели", L"нед.", 604800.0}, {L"Годы", L"г.", 31557600.0}},
         3, 2},
        {L"Данные", Icon::Disk,
         {{L"Биты", L"бит", 0.125}, {L"Байты", L"Б", 1.0}, {L"Килобайты", L"КБ", 1024.0}, {L"Мегабайты", L"МБ", 1048576.0},
          {L"Гигабайты", L"ГБ", 1073741824.0}, {L"Терабайты", L"ТБ", 1099511627776.0}},
         4, 3},
    };
    return cats;
}

// Форматирование числа как в калькуляторе Windows: группы разрядов, запятая, до 16 знаков.
std::wstring FormatNumber(double v, bool group = true)
{
    if (std::isnan(v) || std::isinf(v))
        return L"Ошибка";
    if (v == 0.0)
        return L"0";
    wchar_t buf[64] = {};
    const double a = std::fabs(v);
    if (a >= 1e16 || a < 1e-15)
        swprintf(buf, 64, L"%.15e", v);
    else
        swprintf(buf, 64, L"%.16g", v);
    std::wstring s = buf;
    // Экспоненциальная запись: убрать хвостовые нули мантиссы.
    const size_t epos = s.find(L'e');
    std::wstring mant = epos == std::wstring::npos ? s : s.substr(0, epos);
    std::wstring expo = epos == std::wstring::npos ? L"" : s.substr(epos);
    if (mant.find(L'.') != std::wstring::npos)
    {
        while (!mant.empty() && mant.back() == L'0')
            mant.pop_back();
        if (!mant.empty() && mant.back() == L'.')
            mant.pop_back();
    }
    if (!expo.empty())
    {
        // e+020 → e+20
        std::wstring digits = expo.substr(2);
        while (digits.size() > 1 && digits[0] == L'0')
            digits.erase(0, 1);
        expo = expo.substr(0, 2) + digits;
    }
    // Группировка целой части.
    bool neg = false;
    if (!mant.empty() && mant[0] == L'-')
    {
        neg = true;
        mant.erase(0, 1);
    }
    const size_t dot = mant.find(L'.');
    std::wstring ip = dot == std::wstring::npos ? mant : mant.substr(0, dot);
    std::wstring fp = dot == std::wstring::npos ? L"" : mant.substr(dot + 1);
    std::wstring grouped;
    if (group)
    {
        for (size_t i = 0; i < ip.size(); ++i)
        {
            if (i > 0 && (ip.size() - i) % 3 == 0)
                grouped.push_back(L'\u00A0');
            grouped.push_back(ip[i]);
        }
    }
    else
        grouped = ip;
    std::wstring out = (neg ? L"-" : L"") + grouped;
    if (!fp.empty())
        out += L"," + fp;
    if (!expo.empty())
        out += (fp.empty() ? L"," : L"") + expo;
    return out;
}

// Строка ввода → число («1 234,5»).
double ParseEntry(const std::wstring& s)
{
    std::string a;
    for (wchar_t c : s)
    {
        if (c == L',')
            a.push_back('.');
        else if ((c >= L'0' && c <= L'9') || c == L'-' || c == L'e' || c == L'+')
            a.push_back(static_cast<char>(c));
    }
    if (a.empty() || a == "-")
        return 0.0;
    return std::strtod(a.c_str(), nullptr);
}

// Группировка вводимого числа (без изменения дробной части).
std::wstring GroupEntry(const std::wstring& e)
{
    std::wstring sign, ip = e, fp;
    bool hasDot = false;
    if (!ip.empty() && ip[0] == L'-')
    {
        sign = L"-";
        ip.erase(0, 1);
    }
    const size_t d = ip.find(L',');
    if (d != std::wstring::npos)
    {
        hasDot = true;
        fp = ip.substr(d + 1);
        ip = ip.substr(0, d);
    }
    std::wstring g;
    for (size_t i = 0; i < ip.size(); ++i)
    {
        if (i > 0 && (ip.size() - i) % 3 == 0)
            g.push_back(L'\u00A0');
        g.push_back(ip[i]);
    }
    return sign + g + (hasDot ? L"," + fp : L"");
}

const wchar_t* OpSymbol(wchar_t op)
{
    switch (op)
    {
    case L'+':
        return L"+";
    case L'-':
        return L"−";
    case L'*':
        return L"×";
    case L'/':
        return L"÷";
    case L'^':
        return L"^";
    case L'm':
        return L"mod";
    case L'r':
        return L"yroot";
    default:
        return L"";
    }
}

struct HistoryItem
{
    std::wstring expr;
    double value = 0.0;
};

class CalculatorWindow : public Window
{
public:
    CalculatorWindow() : Window(AppId::Calculator, L"Калькулятор")
    {
        SetInitialSize(340.0f, 540.0f);
        m_minSize = {320.0f, 500.0f};
        m_navBtn = m_ui.Add<Button>(L"", Icon::Hamburger, ButtonStyle::Subtle);
        m_navBtn->SetTooltip(L"Открыть навигацию");
        m_navBtn->onClick = [this]() {
            m_navOpen = !m_navOpen;
            m_navAnim.Set(m_navOpen ? 1.0f : 0.0f, 0.22f, Ease::OutCubic);
        };
        m_topBtn = m_ui.Add<Button>(L"", Icon::Pin, ButtonStyle::Subtle);
        m_topBtn->SetTooltip(L"Поверх других окон");
        m_topBtn->onClick = [this]() { m_compact = !m_compact; };
        m_histBtn = m_ui.Add<Button>(L"", Icon::History, ButtonStyle::Subtle);
        m_histBtn->SetTooltip(L"Открыть журнал (Ctrl+H)");
        m_histBtn->onClick = [this]() { ToggleFlyout(0); };
        // Конвертер
        m_fromCombo = m_ui.Add<ComboBox>();
        m_toCombo = m_ui.Add<ComboBox>();
        m_fromCombo->onChange = [this](int i) { Cat().from = i; };
        m_toCombo->onChange = [this](int i) { Cat().to = i; };
        m_cats = Categories();
        SetMode(Mode::Standard);
    }

private:
    // ================================================================== режимы и раскладка
    Category& Cat() { return m_cats[static_cast<size_t>(m_cat)]; }

    void SetMode(Mode m, int cat = 0)
    {
        m_mode = m;
        m_cat = cat;
        m_navOpen = false;
        m_navAnim.Snap(0.0f);
        ClearAll();
        m_second = false;
        const bool conv = m == Mode::Converter;
        m_fromCombo->SetVisible(conv);
        m_toCombo->SetVisible(conv);
        m_topBtn->SetVisible(m == Mode::Standard);
        if (conv)
        {
            std::vector<std::wstring> names;
            for (const Unit& u : Cat().units)
                names.push_back(u.name);
            m_fromCombo->items = names;
            m_toCombo->items = names;
            m_fromCombo->selected = Cat().from;
            m_toCombo->selected = Cat().to;
            m_entry = L"0";
            m_typing = true;
        }
        // Инженерный режим требует большего окна.
        if (m == Mode::Scientific && (Width() < 380.0f || Height() < 600.0f) && !IsMaximized())
        {
            Rect f = Frame();
            const Rect wa = GetShell().WorkArea();
            f.w = std::max(f.w, 400.0f);
            f.h = std::min(std::max(f.h, 640.0f), wa.h - 16.0f);
            f.y = std::min(f.y, wa.Bottom() - f.h - 8.0f);
            GetShell().WM().MoveResize(this, f);
        }
        OnResize();
    }

    const wchar_t* ModeTitle() const
    {
        switch (m_mode)
        {
        case Mode::Standard:
            return L"Обычный";
        case Mode::Scientific:
            return L"Инженерный";
        case Mode::Converter:
            return m_cats[static_cast<size_t>(m_cat)].name;
        }
        return L"";
    }

    bool SidePanel() const { return Width() >= 600.0f && m_mode != Mode::Converter; }
    float PanelW() const { return SidePanel() ? std::min(340.0f, Width() * 0.38f) : 0.0f; }
    float MainW() const { return Width() - PanelW(); }

    void OnResize() override
    {
        const float th = TitleBarHeight();
        m_navBtn->SetBounds({4.0f, th + 2.0f, 40.0f, 36.0f});
        const float tw = Renderer::Get().TextWidth(ModeTitle(), 20.0f, FontWeight::Semibold);
        m_topBtn->SetBounds({52.0f + tw + 10.0f, th + 4.0f, 34.0f, 32.0f});
        m_histBtn->SetBounds({MainW() - 44.0f, th + 2.0f, 40.0f, 36.0f});
        m_histBtn->SetVisible(!SidePanel() && m_mode != Mode::Converter);
        if (m_mode == Mode::Converter)
        {
            const float w = std::min(260.0f, MainW() - 40.0f);
            m_fromCombo->SetBounds({16.0f, th + 100.0f, w, 32.0f});
            m_toCombo->SetBounds({16.0f, th + 196.0f, w, 32.0f});
        }
        BuildKeys();
    }

    // ---- клавиатура ----------------------------------------------------------------
    struct KeyRect
    {
        KeyDef def;
        Rect r;
    };

    void BuildKeys()
    {
        m_keys.clear();
        static const std::vector<KeyDef> standard = {
            {Key::Percent, L"%", nullptr, 1}, {Key::CE, L"CE", nullptr, 1},     {Key::C, L"C", nullptr, 1},        {Key::Back, L"", nullptr, 1},
            {Key::Inv, L"¹/x", nullptr, 1}, {Key::Sqr, L"x²", nullptr, 1}, {Key::Sqrt, L"²√x", nullptr, 1}, {Key::Div, L"÷", nullptr, 1},
            {Key::D7, L"7", nullptr, 0},       {Key::D8, L"8", nullptr, 0},        {Key::D9, L"9", nullptr, 0},       {Key::Mul, L"×", nullptr, 1},
            {Key::D4, L"4", nullptr, 0},       {Key::D5, L"5", nullptr, 0},        {Key::D6, L"6", nullptr, 0},       {Key::Sub, L"−", nullptr, 1},
            {Key::D1, L"1", nullptr, 0},       {Key::D2, L"2", nullptr, 0},        {Key::D3, L"3", nullptr, 0},       {Key::Add, L"+", nullptr, 1},
            {Key::Neg, L"+/−", nullptr, 0}, {Key::D0, L"0", nullptr, 0},      {Key::Dot, L",", nullptr, 0},      {Key::Eq, L"=", nullptr, 2},
        };
        static const std::vector<KeyDef> scientific = {
            {Key::Second, L"2nd", nullptr, 1}, {Key::Pi, L"π", nullptr, 1},    {Key::E, L"e", nullptr, 1},         {Key::C, L"C", nullptr, 1},     {Key::Back, L"", nullptr, 1},
            {Key::Sqr, L"x²", L"x³", 1}, {Key::Inv, L"¹/x", nullptr, 1}, {Key::Abs, L"|x|", nullptr, 1}, {Key::Exp, L"exp", nullptr, 1}, {Key::Mod, L"mod", nullptr, 1},
            {Key::Sqrt, L"²√x", L"³√x", 1}, {Key::LParen, L"(", nullptr, 1}, {Key::RParen, L")", nullptr, 1}, {Key::Fact, L"n!", nullptr, 1}, {Key::Div, L"÷", nullptr, 1},
            {Key::Pow, L"xʸ", L"ʸ√x", 1}, {Key::D7, L"7", nullptr, 0}, {Key::D8, L"8", nullptr, 0},  {Key::D9, L"9", nullptr, 0},   {Key::Mul, L"×", nullptr, 1},
            {Key::Pow10, L"10ˣ", L"2ˣ", 1}, {Key::D4, L"4", nullptr, 0}, {Key::D5, L"5", nullptr, 0},    {Key::D6, L"6", nullptr, 0},   {Key::Sub, L"−", nullptr, 1},
            {Key::Log, L"log", nullptr, 1},    {Key::D1, L"1", nullptr, 0},         {Key::D2, L"2", nullptr, 0},        {Key::D3, L"3", nullptr, 0},   {Key::Add, L"+", nullptr, 1},
            {Key::Ln, L"ln", L"eˣ", 1},   {Key::Neg, L"+/−", nullptr, 0}, {Key::D0, L"0", nullptr, 0},        {Key::Dot, L",", nullptr, 0},  {Key::Eq, L"=", nullptr, 2},
        };
        static const std::vector<KeyDef> converter = {
            {Key::CE, L"CE", nullptr, 1},  {Key::Back, L"", nullptr, 1},
            {Key::D7, L"7", nullptr, 0},   {Key::D8, L"8", nullptr, 0},   {Key::D9, L"9", nullptr, 0},
            {Key::D4, L"4", nullptr, 0},   {Key::D5, L"5", nullptr, 0},   {Key::D6, L"6", nullptr, 0},
            {Key::D1, L"1", nullptr, 0},   {Key::D2, L"2", nullptr, 0},   {Key::D3, L"3", nullptr, 0},
            {Key::Neg, L"+/−", nullptr, 0}, {Key::D0, L"0", nullptr, 0}, {Key::Dot, L",", nullptr, 0},
        };
        const float W = MainW();
        const float gap = 2.0f, pad = 4.0f;
        const float bottom = Height() - pad;
        if (m_mode == Mode::Converter)
        {
            const float top = TitleBarHeight() + 270.0f;
            const float bw = (W - pad * 2.0f - gap * 2.0f) / 3.0f;
            const float bh = (bottom - top - gap * 4.0f) / 5.0f;
            int i = 0;
            for (const KeyDef& k : converter)
            {
                int row = 0, col = 0;
                if (i < 2)
                {
                    row = 0;
                    col = i + 1;
                }
                else
                {
                    row = 1 + (i - 2) / 3;
                    col = (i - 2) % 3;
                }
                m_keys.push_back({k, {pad + static_cast<float>(col) * (bw + gap), top + static_cast<float>(row) * (bh + gap), bw, bh}});
                ++i;
            }
            return;
        }
        const std::vector<KeyDef>& defs = m_mode == Mode::Scientific ? scientific : standard;
        const int cols = m_mode == Mode::Scientific ? 5 : 4;
        const int rows = static_cast<int>(defs.size()) / cols;
        const float top = KeysTop();
        const float bw = (W - pad * 2.0f - gap * static_cast<float>(cols - 1)) / static_cast<float>(cols);
        const float bh = (bottom - top - gap * static_cast<float>(rows - 1)) / static_cast<float>(rows);
        for (int i = 0; i < static_cast<int>(defs.size()); ++i)
        {
            const int row = i / cols, col = i % cols;
            m_keys.push_back({defs[static_cast<size_t>(i)],
                              {pad + static_cast<float>(col) * (bw + gap), top + static_cast<float>(row) * (bh + gap), bw, bh}});
        }
    }

    float DisplayBottom() const { return TitleBarHeight() + (m_mode == Mode::Scientific ? 150.0f : 160.0f); }
    float KeysTop() const
    {
        float y = DisplayBottom() + 40.0f; // строка памяти
        if (m_mode == Mode::Scientific)
            y += 42.0f; // DEG/F-E и выпадающие списки
        return y;
    }

    // ================================================================== вычисления
    void ClearAll()
    {
        m_vals.clear();
        m_ops.clear();
        m_expr.clear();
        m_entry = L"0";
        m_display = 0.0;
        m_typing = false;
        m_ready = false;
        m_error.clear();
        m_lastOp = 0;
        m_lastOperand = 0.0;
        m_justEvaluated = false;
        m_pendingTerm.clear();
        m_parens = 0;
    }

    double Value() const { return m_typing ? ParseEntry(m_entry) : m_display; }

    // Число бинарных операций в стеке (без открывающих скобок).
    size_t BinaryCount() const
    {
        return m_ops.size() - static_cast<size_t>(std::count(m_ops.begin(), m_ops.end(), L'('));
    }
    // Нужен ли новый операнд (после операции или в самом начале).
    bool NeedOperand() const { return m_vals.size() == BinaryCount(); }

    std::wstring DisplayText() const
    {
        if (!m_error.empty())
            return m_error;
        if (m_typing)
            return GroupEntry(m_entry);
        return FormatNumber(m_display);
    }

    int Prec(wchar_t op) const
    {
        if (m_mode == Mode::Standard)
            return 1; // немедленное выполнение слева направо
        switch (op)
        {
        case L'+':
        case L'-':
            return 1;
        case L'*':
        case L'/':
        case L'm':
            return 2;
        case L'^':
        case L'r':
        case L'E':
            return 3;
        default:
            return 0;
        }
    }

    bool Apply(double a, wchar_t op, double b, double& out)
    {
        switch (op)
        {
        case L'+':
            out = a + b;
            break;
        case L'-':
            out = a - b;
            break;
        case L'*':
            out = a * b;
            break;
        case L'/':
            if (b == 0.0)
            {
                m_error = a == 0.0 ? L"Результат не определён" : L"Деление на 0 невозможно";
                return false;
            }
            out = a / b;
            break;
        case L'm':
            if (b == 0.0)
            {
                m_error = L"Деление на 0 невозможно";
                return false;
            }
            out = std::fmod(a, b);
            break;
        case L'^':
            out = std::pow(a, b);
            break;
        case L'r':
            if (b == 0.0)
            {
                m_error = L"Недопустимый ввод";
                return false;
            }
            out = (a < 0.0 && std::fmod(b, 2.0) == 1.0) ? -std::pow(-a, 1.0 / b) : std::pow(a, 1.0 / b);
            break;
        case L'E':
            out = a * std::pow(10.0, b);
            break;
        default:
            out = b;
            break;
        }
        if (std::isnan(out))
        {
            m_error = L"Недопустимый ввод";
            return false;
        }
        if (std::isinf(out))
        {
            m_error = L"Переполнение";
            return false;
        }
        return true;
    }

    bool ReduceOnce()
    {
        if (m_vals.size() < 2 || m_ops.empty() || m_ops.back() == L'(')
            return false;
        const double b = m_vals.back();
        m_vals.pop_back();
        const double a = m_vals.back();
        m_vals.pop_back();
        const wchar_t op = m_ops.back();
        m_ops.pop_back();
        double r = 0.0;
        if (!Apply(a, op, b, r))
            return false;
        m_vals.push_back(r);
        return true;
    }

    std::wstring TermText(double v) const { return m_pendingTerm.empty() ? FormatNumber(v, false) : m_pendingTerm; }

    void PushValue()
    {
        const double v = Value();
        m_vals.push_back(v);
        m_expr += (m_expr.empty() || m_expr.back() == L'(' ? L"" : L" ") + TermText(v);
        m_pendingTerm.clear();
        m_typing = false;
        m_ready = false;
        m_display = v;
    }

    void StartFresh()
    {
        if (m_justEvaluated)
        {
            m_expr.clear();
            m_vals.clear();
            m_ops.clear();
            m_parens = 0;
            m_justEvaluated = false;
        }
    }

    void Digit(wchar_t d)
    {
        if (!m_error.empty())
            ClearAll();
        StartFresh();
        m_pendingTerm.clear();
        if (!m_typing)
        {
            m_entry = L"0";
            m_typing = true;
        }
        size_t digits = 0;
        for (wchar_t c : m_entry)
            digits += (c >= L'0' && c <= L'9') ? 1 : 0;
        if (digits >= 16)
            return;
        if (m_entry == L"0")
            m_entry = std::wstring(1, d);
        else if (m_entry == L"-0")
            m_entry = L"-" + std::wstring(1, d);
        else
            m_entry.push_back(d);
        m_ready = true;
    }

    void Dot()
    {
        if (!m_error.empty())
            ClearAll();
        StartFresh();
        if (!m_typing)
        {
            m_entry = L"0";
            m_typing = true;
        }
        if (m_entry.find(L',') == std::wstring::npos)
            m_entry += L",";
        m_ready = true;
    }

    void Binary(wchar_t op)
    {
        if (!m_error.empty())
            return;
        m_justEvaluated = false;
        if (!m_ready && NeedOperand() && !m_ops.empty() && m_ops.back() != L'(')
        {
            // Повторное нажатие операции заменяет предыдущую.
            m_ops.back() = op;
            const size_t sp = m_expr.find_last_of(L' ');
            if (sp != std::wstring::npos)
                m_expr = m_expr.substr(0, sp) + L" " + OpSymbol(op);
            return;
        }
        if (NeedOperand())
            PushValue();
        while (!m_ops.empty() && m_ops.back() != L'(' && Prec(m_ops.back()) >= Prec(op) && !(op == L'^' && m_ops.back() == L'^'))
        {
            if (!ReduceOnce())
                return;
        }
        m_ops.push_back(op);
        // В обычном режиме выражение сворачивается до промежуточного результата: «19 ×».
        if (m_mode == Mode::Standard && !m_vals.empty())
            m_expr = FormatNumber(m_vals.back(), false) + L" " + OpSymbol(op);
        else
            m_expr += std::wstring(L" ") + OpSymbol(op);
        if (!m_vals.empty())
            m_display = m_vals.back();
        m_ready = false;
        m_typing = false;
    }

    void Equals()
    {
        if (!m_error.empty())
        {
            ClearAll();
            return;
        }
        if (m_justEvaluated)
        {
            // Повтор последней операции.
            if (m_lastOp == 0)
                return;
            double r = 0.0;
            const double a = m_display;
            if (!Apply(a, m_lastOp, m_lastOperand, r))
                return;
            m_expr = FormatNumber(a, false) + L" " + OpSymbol(m_lastOp) + L" " + FormatNumber(m_lastOperand, false) + L" =";
            m_display = r;
            AddHistory(m_expr, r);
            return;
        }
        if (NeedOperand())
            PushValue();
        while (m_parens > 0)
        {
            while (!m_ops.empty() && m_ops.back() != L'(')
            {
                if (!ReduceOnce())
                    return;
            }
            if (!m_ops.empty())
                m_ops.pop_back();
            m_expr += L")";
            --m_parens;
        }
        // Запомнить последнюю операцию для повторного «=».
        if (!m_ops.empty() && m_vals.size() >= 2)
        {
            m_lastOp = m_ops.back();
            m_lastOperand = m_vals.back();
        }
        while (!m_ops.empty())
        {
            if (m_ops.back() == L'(')
            {
                m_ops.pop_back();
                continue;
            }
            if (!ReduceOnce())
                return;
        }
        const double r = m_vals.empty() ? Value() : m_vals.back();
        m_expr += L" =";
        m_display = r;
        m_vals.clear();
        m_ops.clear();
        m_typing = false;
        m_ready = true;
        m_justEvaluated = true;
        AddHistory(m_expr, r);
    }

    void AddHistory(const std::wstring& expr, double v)
    {
        m_history.insert(m_history.begin(), HistoryItem{expr, v});
        if (m_history.size() > 50)
            m_history.pop_back();
    }

    // Унарные функции: применяются к текущему значению, в выражении отображаются как «sqr(5)».
    void Unary(const wchar_t* name, double (*fn)(double), bool validPositive = false)
    {
        if (!m_error.empty())
            return;
        const double x = Value();
        if (validPositive && x < 0.0)
        {
            m_error = L"Недопустимый ввод";
            return;
        }
        const double r = fn(x);
        if (std::isnan(r) || std::isinf(r))
        {
            m_error = std::isinf(r) ? L"Переполнение" : L"Недопустимый ввод";
            return;
        }
        const std::wstring inner = m_pendingTerm.empty() ? FormatNumber(x, false) : m_pendingTerm;
        if (m_justEvaluated)
        {
            m_expr.clear();
            m_justEvaluated = false;
        }
        m_pendingTerm = std::wstring(name) + L"(" + inner + L")";
        m_display = r;
        m_typing = false;
        m_ready = true;
    }

    double ToRad(double x) const
    {
        if (m_angle == 0)
            return x * 3.14159265358979323846 / 180.0;
        if (m_angle == 2)
            return x * 3.14159265358979323846 / 200.0;
        return x;
    }
    double FromRad(double x) const
    {
        if (m_angle == 0)
            return x * 180.0 / 3.14159265358979323846;
        if (m_angle == 2)
            return x * 200.0 / 3.14159265358979323846;
        return x;
    }

    void Trig(int which, bool inverse)
    {
        if (!m_error.empty())
            return;
        const double x = Value();
        double r = 0.0;
        const wchar_t* names[] = {L"sin", L"cos", L"tan"};
        const wchar_t* inames[] = {L"sin⁻¹", L"cos⁻¹", L"tan⁻¹"};
        if (!inverse)
        {
            const double a = ToRad(x);
            r = which == 0 ? std::sin(a) : (which == 1 ? std::cos(a) : std::tan(a));
            if (std::fabs(r) < 1e-15)
                r = 0.0;
            if (which == 2 && std::fabs(std::cos(a)) < 1e-15)
            {
                m_error = L"Недопустимый ввод";
                return;
            }
        }
        else
        {
            if (which < 2 && (x < -1.0 || x > 1.0))
            {
                m_error = L"Недопустимый ввод";
                return;
            }
            r = FromRad(which == 0 ? std::asin(x) : (which == 1 ? std::acos(x) : std::atan(x)));
        }
        const std::wstring inner = m_pendingTerm.empty() ? FormatNumber(x, false) : m_pendingTerm;
        const wchar_t* unit = m_angle == 0 ? L"₀" : (m_angle == 1 ? L"ᵣ" : L"g");
        m_pendingTerm = std::wstring(inverse ? inames[which] : names[which]) + unit + L"(" + inner + L")";
        if (m_justEvaluated)
        {
            m_expr.clear();
            m_justEvaluated = false;
        }
        m_display = r;
        m_typing = false;
        m_ready = true;
    }

    void Press(Key k)
    {
        if (m_mode == Mode::Converter)
        {
            PressConverter(k);
            return;
        }
        switch (k)
        {
        case Key::D0:
        case Key::D1:
        case Key::D2:
        case Key::D3:
        case Key::D4:
        case Key::D5:
        case Key::D6:
        case Key::D7:
        case Key::D8:
        case Key::D9:
            Digit(static_cast<wchar_t>(L'0' + (static_cast<int>(k) - static_cast<int>(Key::D0))));
            break;
        case Key::Dot:
            Dot();
            break;
        case Key::Add:
            Binary(L'+');
            break;
        case Key::Sub:
            Binary(L'-');
            break;
        case Key::Mul:
            Binary(L'*');
            break;
        case Key::Div:
            Binary(L'/');
            break;
        case Key::Mod:
            Binary(L'm');
            break;
        case Key::Exp:
            Binary(L'E');
            break;
        case Key::Pow:
            Binary(m_second ? L'r' : L'^');
            break;
        case Key::Eq:
            Equals();
            break;
        case Key::CE:
            if (!m_error.empty())
                ClearAll();
            else
            {
                m_entry = L"0";
                m_typing = true;
                m_ready = true;
                m_pendingTerm.clear();
                if (m_justEvaluated)
                    ClearAll();
            }
            break;
        case Key::C:
            ClearAll();
            break;
        case Key::Back:
            if (!m_error.empty())
            {
                ClearAll();
                break;
            }
            if (m_justEvaluated)
            {
                m_expr.clear();
                break;
            }
            if (m_typing)
            {
                m_entry.pop_back();
                if (m_entry.empty() || m_entry == L"-")
                    m_entry = L"0";
            }
            break;
        case Key::Neg:
            if (!m_error.empty())
                break;
            if (m_typing)
            {
                if (!m_entry.empty() && m_entry[0] == L'-')
                    m_entry.erase(0, 1);
                else if (m_entry != L"0")
                    m_entry = L"-" + m_entry;
            }
            else
                Unary(L"negate", [](double x) { return -x; });
            break;
        case Key::Percent:
            Percent();
            break;
        case Key::Inv:
            if (Value() == 0.0)
                m_error = L"Деление на 0 невозможно";
            else
                Unary(L"1/", [](double x) { return 1.0 / x; });
            break;
        case Key::Sqr:
            if (m_second)
                Unary(L"cube", [](double x) { return x * x * x; });
            else
                Unary(L"sqr", [](double x) { return x * x; });
            break;
        case Key::Sqrt:
            if (m_second)
                Unary(L"cuberoot", [](double x) { return std::cbrt(x); });
            else
                Unary(L"√", [](double x) { return std::sqrt(x); }, true);
            break;
        case Key::Second:
            m_second = !m_second;
            break;
        case Key::Pi:
        case Key::E:
            StartFresh();
            m_display = k == Key::Pi ? 3.14159265358979323846 : 2.71828182845904523536;
            m_pendingTerm = k == Key::Pi ? L"π" : L"e";
            m_typing = false;
            m_ready = true;
            m_error.clear();
            break;
        case Key::Abs:
            Unary(L"abs", [](double x) { return std::fabs(x); });
            break;
        case Key::Fact:
        {
            const double x = Value();
            if (x < 0.0 || x > 170.0)
                m_error = L"Недопустимый ввод";
            else
                Unary(L"fact", [](double v) {
                    if (v != std::floor(v))
                        return std::tgamma(v + 1.0);
                    double f = 1.0;
                    for (int i = 2; i <= static_cast<int>(v); ++i)
                        f *= static_cast<double>(i);
                    return f;
                });
            break;
        }
        case Key::Pow10:
            if (m_second)
                Unary(L"2^", [](double x) { return std::pow(2.0, x); });
            else
                Unary(L"10^", [](double x) { return std::pow(10.0, x); });
            break;
        case Key::Log:
            if (Value() <= 0.0)
                m_error = L"Недопустимый ввод";
            else
                Unary(L"log", [](double x) { return std::log10(x); });
            break;
        case Key::Ln:
            if (m_second)
                Unary(L"e^", [](double x) { return std::exp(x); });
            else if (Value() <= 0.0)
                m_error = L"Недопустимый ввод";
            else
                Unary(L"ln", [](double x) { return std::log(x); });
            break;
        case Key::LParen:
            if (!m_error.empty())
                break;
            StartFresh();
            if (m_ready && NeedOperand())
                Binary(L'*'); // «2(» — неявное умножение
            m_ops.push_back(L'(');
            m_expr += (m_expr.empty() ? L"" : L" ") + std::wstring(L"(");
            ++m_parens;
            m_typing = false;
            m_ready = false;
            break;
        case Key::RParen:
            if (!m_error.empty() || m_parens == 0)
                break;
            if (NeedOperand())
                PushValue();
            while (!m_ops.empty() && m_ops.back() != L'(')
            {
                if (!ReduceOnce())
                    return;
            }
            if (!m_ops.empty())
                m_ops.pop_back();
            --m_parens;
            m_expr += L")";
            if (!m_vals.empty())
            {
                m_display = m_vals.back();
                m_vals.pop_back();
                // Скобочное выражение становится новым операндом.
                const size_t open = MatchingParen(m_expr);
                if (open == std::wstring::npos)
                    m_pendingTerm = FormatNumber(m_display, false);
                else
                {
                    m_pendingTerm = m_expr.substr(open);
                    m_expr = m_expr.substr(0, open);
                }
                while (!m_expr.empty() && m_expr.back() == L' ')
                    m_expr.pop_back();
            }
            m_ready = true;
            break;
        }
    }

    // Позиция «(», парной к последней «)» в строке выражения.
    static size_t MatchingParen(const std::wstring& s)
    {
        int depth = 0;
        for (size_t i = s.size(); i > 0; --i)
        {
            const wchar_t c = s[i - 1];
            if (c == L')')
                ++depth;
            else if (c == L'(' && --depth == 0)
                return i - 1;
        }
        return std::wstring::npos;
    }

    void Percent()
    {
        if (!m_error.empty())
            return;
        const double x = Value();
        double r = 0.0;
        if (!m_ops.empty() && !m_vals.empty() && (m_ops.back() == L'+' || m_ops.back() == L'-'))
            r = m_vals.back() * x / 100.0;
        else if (!m_ops.empty())
            r = x / 100.0;
        m_pendingTerm = FormatNumber(r, false);
        m_display = r;
        m_typing = false;
        m_ready = true;
    }

    // ---- память ------------------------------------------------------------------------
    void MemStore()
    {
        if (m_error.empty())
            m_memory.insert(m_memory.begin(), Value());
    }
    void MemAdd(double sign)
    {
        if (!m_error.empty())
            return;
        if (m_memory.empty())
            m_memory.push_back(0.0);
        m_memory.front() += sign * Value();
    }
    void MemRecall(size_t i = 0)
    {
        if (i >= m_memory.size())
            return;
        StartFresh();
        m_display = m_memory[i];
        m_typing = false;
        m_ready = true;
        m_pendingTerm.clear();
        m_error.clear();
    }

    // ---- конвертер ----------------------------------------------------------------------
    double Convert(double v) const
    {
        const Category& c = m_cats[static_cast<size_t>(m_cat)];
        if (std::wstring(c.name) == L"Температура")
        {
            double k = v; // в Кельвины
            if (c.from == 0)
                k = v + 273.15;
            else if (c.from == 1)
                k = (v - 32.0) * 5.0 / 9.0 + 273.15;
            if (c.to == 0)
                return k - 273.15;
            if (c.to == 1)
                return (k - 273.15) * 9.0 / 5.0 + 32.0;
            return k;
        }
        const double base = v * c.units[static_cast<size_t>(c.from)].factor;
        return base / c.units[static_cast<size_t>(c.to)].factor;
    }

    void PressConverter(Key k)
    {
        switch (k)
        {
        case Key::CE:
            m_entry = L"0";
            break;
        case Key::Back:
            if (!m_entry.empty())
                m_entry.pop_back();
            if (m_entry.empty() || m_entry == L"-")
                m_entry = L"0";
            break;
        case Key::Dot:
            if (m_entry.find(L',') == std::wstring::npos)
                m_entry += L",";
            break;
        case Key::Neg:
            if (!m_entry.empty() && m_entry[0] == L'-')
                m_entry.erase(0, 1);
            else if (m_entry != L"0")
                m_entry = L"-" + m_entry;
            break;
        default:
            if (k >= Key::D0 && k <= Key::D9)
            {
                const wchar_t d = static_cast<wchar_t>(L'0' + (static_cast<int>(k) - static_cast<int>(Key::D0)));
                size_t digits = 0;
                for (wchar_t c : m_entry)
                    digits += (c >= L'0' && c <= L'9') ? 1 : 0;
                if (digits >= 16)
                    break;
                if (m_entry == L"0")
                    m_entry = std::wstring(1, d);
                else if (m_entry == L"-0")
                    m_entry = L"-" + std::wstring(1, d);
                else
                    m_entry.push_back(d);
            }
            break;
        }
    }

    // ================================================================== отрисовка
    void DrawContent(Renderer& r, const Rect& /*client*/) override
    {
        const Palette& p = Theme::P();
        const float th = TitleBarHeight();
        const float W = MainW();
        r.Text(ModeTitle(), {52.0f, th + 2.0f, W - 100.0f, 36.0f}, p.text, 20.0f, FontWeight::Semibold, TextFlags::VCenter);
        if (m_mode == Mode::Converter)
            DrawConverter(r);
        else
            DrawCalculator(r);
        DrawKeys(r);
        if (SidePanel())
            DrawSidePanel(r);
        m_histBtn->SetVisible(!SidePanel() && m_mode != Mode::Converter);
        m_ui.Draw(r);
        DrawFlyout(r);
        DrawNav(r);
    }

    void DrawCalculator(Renderer& r)
    {
        const Palette& p = Theme::P();
        const float th = TitleBarHeight();
        const float W = MainW();
        // Строка выражения
        std::wstring expr = m_expr;
        if (!m_pendingTerm.empty() && !m_justEvaluated)
            expr += (expr.empty() || expr.back() == L'(' ? L"" : L" ") + m_pendingTerm;
        r.Text(expr, {12.0f, th + 44.0f, W - 24.0f, 24.0f}, p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::Right | TextFlags::VCenter);
        // Основной дисплей — уменьшение шрифта для длинных чисел.
        const std::wstring disp = DisplayText();
        float fs = m_error.empty() ? 46.0f : 28.0f;
        const float avail = W - 24.0f;
        while (fs > 18.0f && r.TextWidth(disp, fs, FontWeight::Semibold) > avail)
            fs -= 2.0f;
        r.Text(disp, {12.0f, th + 68.0f, avail, DisplayBottom() - th - 72.0f}, p.text, fs, FontWeight::Semibold, TextFlags::Right | TextFlags::VCenter);
        // Строка памяти
        const float my = DisplayBottom() + 4.0f;
        const wchar_t* mem[] = {L"MC", L"MR", L"M+", L"M−", L"MS", L"M˅"};
        const float mw = (W - 8.0f) / 6.0f;
        m_memRects.clear();
        for (int i = 0; i < 6; ++i)
        {
            const Rect mr{4.0f + static_cast<float>(i) * mw, my, mw, 32.0f};
            m_memRects.push_back(mr);
            const bool enabled = (i == 0 || i == 1 || i == 5) ? !m_memory.empty() : m_error.empty();
            const bool hot = enabled && m_hoverMem == i;
            if (hot)
                r.FillRoundRect(mr.Inset(2.0f, 2.0f, 2.0f, 2.0f), 4.0f, m_pressMem == i ? p.subtlePressed : p.subtleHover);
            if (i == 5 && SidePanel())
                continue;
            r.Text(mem[i], mr, enabled ? p.text : p.textDisabled, 12.0f, FontWeight::Semibold, TextFlags::Middle);
        }
        if (m_mode == Mode::Scientific)
        {
            // DEG/RAD/GRAD, F-E и выпадающие «Тригонометрия», «Функция».
            const float y = my + 38.0f;
            m_sciRects.clear();
            const wchar_t* angle = m_angle == 0 ? L"DEG" : (m_angle == 1 ? L"RAD" : L"GRAD");
            const std::wstring labels[] = {angle, L"F-E", L"Тригонометрия", L"Функция"};
            float x = 4.0f;
            const float widths[] = {64.0f, 56.0f, (W - 136.0f) * 0.55f, (W - 136.0f) * 0.45f};
            for (int i = 0; i < 4; ++i)
            {
                const Rect br{x, y, widths[i] - 4.0f, 32.0f};
                m_sciRects.push_back(br);
                const bool on = i == 1 && m_fe;
                if (m_hoverSci == i || on)
                    r.FillRoundRect(br, 4.0f, on ? p.controlHover : p.subtleHover);
                if (i < 2)
                    r.Text(labels[i], br, p.text, 12.0f, FontWeight::Semibold, TextFlags::Middle);
                else
                {
                    // Значок, подпись и стрелка раскрытия.
                    const float tw = r.TextWidth(labels[i], 12.0f, FontWeight::Semibold);
                    const float x0 = br.CenterX() - (tw + 40.0f) * 0.5f;
                    if (i == 2)
                        r.Polyline({{x0, br.CenterY() + 5.0f}, {x0 + 12.0f, br.CenterY() + 5.0f}, {x0 + 12.0f, br.CenterY() - 6.0f}}, p.text, 1.0f, true);
                    else
                        r.Text(L"f", {x0, br.y, 14.0f, br.h}, p.text, 14.0f, FontWeight::Semibold, TextFlags::Middle);
                    r.Text(labels[i], {x0 + 18.0f, br.y, tw + 4.0f, br.h}, p.text, 12.0f, FontWeight::Semibold, TextFlags::VCenter);
                    r.Glyph(Icon::ChevronDown, {x0 + tw + 26.0f, br.CenterY() - 4.0f, 8.0f, 8.0f}, p.text);
                }
                x += widths[i];
            }
        }
    }

    void DrawConverter(Renderer& r)
    {
        const Palette& p = Theme::P();
        const float th = TitleBarHeight();
        const float W = MainW();
        const double v = ParseEntry(m_entry);
        const double out = Convert(v);
        const Category& c = Cat();
        const Unit& uf = c.units[static_cast<size_t>(c.from)];
        const Unit& ut = c.units[static_cast<size_t>(c.to)];
        const std::wstring a = GroupEntry(m_entry), b = FormatNumber(out);
        float fs = 40.0f;
        while (fs > 18.0f && std::max(r.TextWidth(a, fs, FontWeight::Semibold), r.TextWidth(b, fs, FontWeight::Semibold)) > W - 40.0f)
            fs -= 2.0f;
        r.Text(a, {16.0f, th + 44.0f, W - 32.0f, 54.0f}, p.text, fs, FontWeight::Semibold, TextFlags::VCenter);
        r.Text(b, {16.0f, th + 140.0f, W - 32.0f, 54.0f}, p.text, fs, FontWeight::Semibold, TextFlags::VCenter);
        // Подсказка с другими единицами
        if (std::wstring(c.name) == L"Температура")
            r.Text(FormatNumber(v) + L" " + uf.abbr + L" = " + FormatNumber(out) + L" " + ut.abbr, {16.0f, th + 236.0f, W - 32.0f, 24.0f},
                   p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        else if (c.units.size() > 2)
        {
            std::wstring about = L"Примерно равно: ";
            int shown = 0;
            for (size_t i = 0; i < c.units.size() && shown < 2; ++i)
            {
                if (static_cast<int>(i) == c.to || static_cast<int>(i) == c.from)
                    continue;
                const double base = v * uf.factor;
                about += (shown ? L"   " : L"") + FormatNumber(base / c.units[i].factor) + L" " + c.units[i].abbr;
                ++shown;
            }
            r.Text(about, {16.0f, th + 236.0f, W - 32.0f, 24.0f}, p.textSecondary, 12.0f, FontWeight::Regular, TextFlags::VCenter);
        }
    }

    // Глиф «Backspace».
    static void DrawBackspace(Renderer& r, const Rect& b, Color c)
    {
        const float cx = b.CenterX(), cy = b.CenterY();
        const float w = 18.0f, h = 13.0f;
        r.Polyline({{cx - w * 0.5f, cy}, {cx - w * 0.5f + 5.0f, cy - h * 0.5f}, {cx + w * 0.5f, cy - h * 0.5f}, {cx + w * 0.5f, cy + h * 0.5f},
                    {cx - w * 0.5f + 5.0f, cy + h * 0.5f}},
                   c, 1.1f, true);
        r.Line(cx - 1.0f, cy - 3.0f, cx + 5.0f, cy + 3.0f, c, 1.1f);
        r.Line(cx + 5.0f, cy - 3.0f, cx - 1.0f, cy + 3.0f, c, 1.1f);
    }

    bool KeyEnabled(Key k) const
    {
        if (m_error.empty() || m_mode == Mode::Converter)
            return true;
        return (k >= Key::D0 && k <= Key::D9) || k == Key::C || k == Key::CE || k == Key::Back || k == Key::Eq || k == Key::Dot;
    }

    void DrawKeys(Renderer& r)
    {
        const Palette& p = Theme::P();
        const bool dark = p.dark;
        for (size_t i = 0; i < m_keys.size(); ++i)
        {
            const KeyRect& kr = m_keys[i];
            const int kind = kr.def.kind;
            const bool enabled = KeyEnabled(kr.def.key);
            const bool hot = enabled && m_hoverKey == static_cast<int>(i);
            const bool down = hot && m_pressKey == static_cast<int>(i);
            const bool on = kr.def.key == Key::Second && m_second;
            Color fill, fg = enabled ? p.text : p.textDisabled;
            if (kind == 2 || on)
            {
                fill = down ? p.accentPressed : (hot ? p.accentHover : p.accent);
                fg = p.textOnAccent;
            }
            else if (kind == 0)
                fill = dark ? Color(255, 255, 255, down ? 0x08 : (hot ? 0x15 : 0x0F)) : Color(255, 255, 255, down ? 0x4D : (hot ? 0x80 : 0xB3));
            else
                fill = dark ? Color(255, 255, 255, down ? 0x05 : (hot ? 0x0F : 0x08)) : Color(249, 249, 249, down ? 0x4D : (hot ? 0xB3 : 0x80));
            const Rect& b = kr.r;
            r.FillRoundRect(b, 4.0f, fill);
            r.StrokeRoundRect(b, 4.0f, p.controlStroke);
            if (kind != 2)
                r.Line(b.x + 4.0f, b.Bottom() - 0.5f, b.Right() - 4.0f, b.Bottom() - 0.5f, dark ? Color(0, 0, 0, 0x30) : Color(0, 0, 0, 0x10), 1.0f, false);
            if (kr.def.key == Key::Back)
            {
                DrawBackspace(r, b, fg);
                continue;
            }
            const wchar_t* label = (m_second && kr.def.label2) ? kr.def.label2 : kr.def.label;
            const bool digit = kind == 0 && kr.def.key != Key::Neg;
            r.Text(label, b, fg, digit ? 20.0f : (kind == 2 ? 20.0f : 16.0f), digit ? FontWeight::Semibold : FontWeight::Regular, TextFlags::Middle);
        }
    }

    Rect PanelRect() const { return {MainW(), TitleBarHeight(), PanelW(), Height() - TitleBarHeight()}; }

    // Содержимое журнала/памяти в прямоугольнике rc.
    void DrawList(Renderer& r, const Rect& rc, int tab)
    {
        const Palette& p = Theme::P();
        m_listRects.clear();
        r.PushClip(rc);
        float y = rc.y + 8.0f - m_listScroll;
        if (tab == 0)
        {
            if (m_history.empty())
                r.Text(L"Журнала пока нет", {rc.x + 16.0f, rc.y + 12.0f, rc.w - 32.0f, 24.0f}, p.text, 14.0f);
            for (size_t i = 0; i < m_history.size(); ++i)
            {
                const Rect row{rc.x + 4.0f, y, rc.w - 8.0f, 64.0f};
                m_listRects.push_back(row);
                if (m_hoverList == static_cast<int>(i))
                    r.FillRoundRect(row, 4.0f, p.subtleHover);
                r.Text(m_history[i].expr, {row.x + 12.0f, row.y + 8.0f, row.w - 24.0f, 20.0f}, p.textSecondary, 14.0f, FontWeight::Regular,
                       TextFlags::Right);
                r.Text(FormatNumber(m_history[i].value), {row.x + 12.0f, row.y + 28.0f, row.w - 24.0f, 30.0f}, p.text, 20.0f, FontWeight::Semibold,
                       TextFlags::Right);
                y += 68.0f;
            }
        }
        else
        {
            if (m_memory.empty())
                r.Text(L"В памяти ничего не сохранено", {rc.x + 16.0f, rc.y + 12.0f, rc.w - 32.0f, 24.0f}, p.text, 14.0f);
            for (size_t i = 0; i < m_memory.size(); ++i)
            {
                const Rect row{rc.x + 4.0f, y, rc.w - 8.0f, 76.0f};
                m_listRects.push_back(row);
                const bool hot = m_hoverList == static_cast<int>(i);
                if (hot)
                    r.FillRoundRect(row, 4.0f, p.subtleHover);
                r.Text(FormatNumber(m_memory[i]), {row.x + 12.0f, row.y + 6.0f, row.w - 24.0f, 32.0f}, p.text, 20.0f, FontWeight::Semibold,
                       TextFlags::Right);
                if (hot)
                {
                    const wchar_t* ops[] = {L"MC", L"M+", L"M−"};
                    for (int k = 0; k < 3; ++k)
                    {
                        const Rect mb{row.Right() - 12.0f - static_cast<float>(3 - k) * 40.0f, row.y + 42.0f, 36.0f, 28.0f};
                        r.FillRoundRect(mb, 4.0f, p.control);
                        r.StrokeRoundRect(mb, 4.0f, p.controlStroke);
                        r.Text(ops[k], mb, p.text, 12.0f, FontWeight::Semibold, TextFlags::Middle);
                    }
                }
                y += 80.0f;
            }
        }
        r.PopClip();
        // Корзина (очистка)
        const bool any = tab == 0 ? !m_history.empty() : !m_memory.empty();
        m_trashRect = any ? Rect{rc.Right() - 48.0f, rc.Bottom() - 44.0f, 40.0f, 36.0f} : Rect{};
        if (any)
        {
            if (m_hoverTrash)
                r.FillRoundRect(m_trashRect, 4.0f, p.subtleHover);
            r.Glyph(Icon::Delete, {m_trashRect.CenterX() - 8.0f, m_trashRect.CenterY() - 8.0f, 16.0f, 16.0f}, p.text);
        }
    }

    void DrawTabs(Renderer& r, const Rect& rc)
    {
        const Palette& p = Theme::P();
        const wchar_t* names[] = {L"Журнал", L"Память"};
        float x = rc.x + 12.0f;
        m_tabRects.clear();
        for (int i = 0; i < 2; ++i)
        {
            const float w = r.TextWidth(names[i], 14.0f, FontWeight::Semibold) + 16.0f;
            const Rect tr{x, rc.y + 6.0f, w, 34.0f};
            m_tabRects.push_back(tr);
            r.Text(names[i], tr, m_tab == i ? p.text : p.textSecondary, 14.0f, m_tab == i ? FontWeight::Semibold : FontWeight::Regular, TextFlags::Middle);
            if (m_tab == i)
                r.FillRoundRect({tr.CenterX() - 8.0f, tr.Bottom() - 3.0f, 16.0f, 3.0f}, 1.5f, p.accent);
            x += w + 8.0f;
        }
    }

    void DrawSidePanel(Renderer& r)
    {
        const Rect pr = PanelRect();
        DrawTabs(r, pr);
        DrawList(r, {pr.x, pr.y + 48.0f, pr.w, pr.h - 48.0f}, m_tab);
    }

    Rect FlyoutRect() const
    {
        const float top = DisplayBottom() + 40.0f;
        return {0.0f, top, MainW(), Height() - top};
    }

    void ToggleFlyout(int tab)
    {
        if (m_flyout && m_tab == tab)
            m_flyout = false;
        else
        {
            m_flyout = true;
            m_tab = tab;
            m_listScroll = 0.0f;
            m_flyAnim.Start(0.0f, 1.0f, 0.2f, Ease::OutCubic);
        }
    }

    void DrawFlyout(Renderer& r)
    {
        if (!m_flyout || SidePanel())
            return;
        const Palette& p = Theme::P();
        const float t = m_flyAnim.Value();
        const Rect fr = FlyoutRect();
        r.PushOpacity(t);
        r.PushTransform(0.0f, (1.0f - t) * 40.0f);
        r.Shadow(fr, 8.0f, 20.0f, -4.0f, p.dark ? 0.5f : 0.2f);
        r.Acrylic(fr, 8.0f, p.acrylicTint, p.acrylicAmount);
        r.StrokeRoundRect(fr, 8.0f, p.flyoutStroke);
        DrawTabs(r, fr);
        DrawList(r, {fr.x, fr.y + 48.0f, fr.w, fr.h - 48.0f}, m_tab);
        r.PopTransform();
        r.PopOpacity();
    }

    // Панель навигации (режимы и конвертеры).
    struct NavRow
    {
        std::wstring text;
        Icon icon;
        int mode; // -1 — заголовок; 0 обычный; 1 инженерный; 2+i — конвертер i; 99 — параметры
    };
    std::vector<NavRow> NavRows() const
    {
        std::vector<NavRow> v = {{L"Калькулятор", Icon::None, -1}, {L"Обычный", Icon::Grid, 0}, {L"Инженерный", Icon::Chart, 1},
                                 {L"Конвертер", Icon::None, -1}};
        for (size_t i = 0; i < m_cats.size(); ++i)
            v.push_back({m_cats[i].name, m_cats[i].icon, 2 + static_cast<int>(i)});
        v.push_back({L"Параметры", Icon::Settings, 99});
        return v;
    }

    Rect NavPaneRect() const { return {0.0f, TitleBarHeight(), std::min(300.0f, Width() - 40.0f), Height() - TitleBarHeight()}; }

    Rect NavRowRect(int i, const std::vector<NavRow>& rows) const
    {
        const Rect np = NavPaneRect();
        if (rows[static_cast<size_t>(i)].mode == 99)
            return {np.x + 4.0f, np.Bottom() - 44.0f, np.w - 8.0f, 40.0f};
        float y = np.y + 48.0f - m_navScroll;
        for (int k = 0; k < i; ++k)
            y += rows[static_cast<size_t>(k)].mode == -1 ? 34.0f : 38.0f;
        return {np.x + 4.0f, y, np.w - 8.0f, rows[static_cast<size_t>(i)].mode == -1 ? 34.0f : 38.0f};
    }
    Rect NavListClip() const
    {
        const Rect np = NavPaneRect();
        return {np.x, np.y + 44.0f, np.w, np.h - 44.0f - 50.0f};
    }
    float NavMaxScroll() const
    {
        const std::vector<NavRow> rows = NavRows();
        float h = 0.0f;
        for (const NavRow& row : rows)
            h += row.mode == 99 ? 0.0f : (row.mode == -1 ? 34.0f : 38.0f);
        return std::max(0.0f, h + 8.0f - NavListClip().h);
    }

    void DrawNav(Renderer& r)
    {
        const float t = m_navAnim.Value();
        if (t <= 0.003f)
            return;
        const Palette& p = Theme::P();
        const Rect np = NavPaneRect();
        r.FillRect({0.0f, TitleBarHeight(), Width(), Height() - TitleBarHeight()}, p.smoke.MulAlpha(t * 0.3f));
        r.PushTransform((t - 1.0f) * np.w, 0.0f);
        r.Shadow(np, 0.0f, 24.0f, 0.0f, p.dark ? 0.6f : 0.25f);
        r.Acrylic(np, 0.0f, p.acrylicTint, p.acrylicAmount);
        r.FillRect({np.Right() - 1.0f, np.y, 1.0f, np.h}, p.flyoutStroke);
        const std::vector<NavRow> rows = NavRows();
        r.FillRect({np.x + 12.0f, np.Bottom() - 50.0f, np.w - 24.0f, 1.0f}, p.divider);
        for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        {
            const NavRow& row = rows[static_cast<size_t>(i)];
            const Rect rr = NavRowRect(i, rows);
            const bool clipped = row.mode != 99;
            if (clipped)
                r.PushClip(NavListClip());
            if (row.mode == -1)
            {
                r.Text(row.text, {rr.x + 12.0f, rr.y, rr.w, rr.h}, p.text, 14.0f, FontWeight::Semibold, TextFlags::VCenter);
                r.PopClip();
                continue;
            }
            const bool sel = (row.mode == 0 && m_mode == Mode::Standard) || (row.mode == 1 && m_mode == Mode::Scientific) ||
                             (row.mode >= 2 && row.mode < 99 && m_mode == Mode::Converter && row.mode - 2 == m_cat);
            const Rect hr = rr.Inset(0.0f, 1.0f, 0.0f, 1.0f);
            if (sel || m_hoverNav == i)
                r.FillRoundRect(hr, 4.0f, p.subtleHover);
            if (sel)
                r.FillRoundRect({hr.x, hr.CenterY() - 8.0f, 3.0f, 16.0f}, 1.5f, p.accent);
            r.Glyph(row.icon, {rr.x + 14.0f, rr.CenterY() - 8.0f, 16.0f, 16.0f}, p.text);
            r.Text(row.text, {rr.x + 44.0f, rr.y, rr.w - 50.0f, rr.h}, p.text, 14.0f, FontWeight::Regular, TextFlags::VCenter);
            if (clipped)
                r.PopClip();
        }
        // Кнопка меню остаётся видимой поверх панели.
        const Rect nb = m_navBtn->Bounds();
        if (m_navBtn->Hovered())
            r.FillRoundRect(nb, 4.0f, p.subtleHover);
        r.Glyph(Icon::Hamburger, {nb.CenterX() - 8.0f, nb.CenterY() - 8.0f, 16.0f, 16.0f}, p.text);
        r.PopTransform();
    }

    // ================================================================== ввод
    int KeyAt(Point pt) const
    {
        for (size_t i = 0; i < m_keys.size(); ++i)
        {
            if (m_keys[i].r.Contains(pt))
                return static_cast<int>(i);
        }
        return -1;
    }

    static int IndexIn(const std::vector<Rect>& v, Point pt)
    {
        for (size_t i = 0; i < v.size(); ++i)
        {
            if (v[i].Contains(pt))
                return static_cast<int>(i);
        }
        return -1;
    }

    bool ListVisible() const { return SidePanel() || m_flyout; }

    void OnMouseMove(const MouseEvent& e) override
    {
        if (HasDialog())
        {
            Window::OnMouseMove(e);
            return;
        }
        const Point pt = e.pos;
        if (m_navOpen)
        {
            const std::vector<NavRow> rows = NavRows();
            m_hoverNav = -1;
            for (int i = 0; i < static_cast<int>(rows.size()); ++i)
            {
                const bool visible = rows[static_cast<size_t>(i)].mode == 99 || NavListClip().Contains(pt);
                if (rows[static_cast<size_t>(i)].mode != -1 && visible && NavRowRect(i, rows).Contains(pt))
                    m_hoverNav = i;
            }
            if (!NavPaneRect().Contains(pt))
                Window::OnMouseMove(e);
            return;
        }
        Window::OnMouseMove(e);
        const bool overFly = m_flyout && !SidePanel() && FlyoutRect().Contains(pt);
        m_hoverKey = overFly ? -1 : KeyAt(pt);
        m_hoverMem = overFly ? -1 : IndexIn(m_memRects, pt);
        m_hoverSci = overFly ? -1 : IndexIn(m_sciRects, pt);
        m_hoverList = ListVisible() ? IndexIn(m_listRects, pt) : -1;
        m_hoverTrash = ListVisible() && !m_trashRect.Empty() && m_trashRect.Contains(pt);
    }

    void OnMouseLeave() override
    {
        Window::OnMouseLeave();
        m_hoverKey = m_hoverMem = m_hoverSci = m_hoverList = m_hoverNav = -1;
        m_hoverTrash = false;
    }

    void OnMouseDown(const MouseEvent& e) override
    {
        if (HasDialog())
        {
            Window::OnMouseDown(e);
            return;
        }
        const Point pt = e.pos;
        if (m_navOpen)
        {
            if (m_navBtn->Bounds().Contains(pt))
            {
                Window::OnMouseDown(e);
                return;
            }
            if (!NavPaneRect().Contains(pt))
            {
                m_navOpen = false;
                m_navAnim.Set(0.0f, 0.18f, Ease::OutCubic);
                return;
            }
            const std::vector<NavRow> rows = NavRows();
            for (int i = 0; i < static_cast<int>(rows.size()); ++i)
            {
                const NavRow& row = rows[static_cast<size_t>(i)];
                const bool visible = row.mode == 99 || NavListClip().Contains(pt);
                if (row.mode == -1 || !visible || !NavRowRect(i, rows).Contains(pt))
                    continue;
                if (row.mode == 0)
                    SetMode(Mode::Standard);
                else if (row.mode == 1)
                    SetMode(Mode::Scientific);
                else if (row.mode == 99)
                {
                    m_navOpen = false;
                    m_navAnim.Snap(0.0f);
                    ShowDialog(L"О калькуляторе", L"Калькулятор — демонстрационная копия приложения Windows 11.\nВерсия 11.2409.0.0", {{L"ОК", true}},
                               nullptr);
                }
                else
                    SetMode(Mode::Converter, row.mode - 2);
                return;
            }
            return;
        }
        // Всплывающий журнал
        if (m_flyout && !SidePanel())
        {
            if (FlyoutRect().Contains(pt))
            {
                ListClick(pt, e);
                return;
            }
            if (!m_histBtn->Bounds().Contains(pt) && !(m_memRects.size() > 5 && m_memRects[5].Contains(pt)))
            {
                m_flyout = false;
                return;
            }
        }
        if (SidePanel() && PanelRect().Contains(pt))
        {
            ListClick(pt, e);
            return;
        }
        if (m_ui.OnMouseDown(e))
            return;
        if (e.button != MouseButton::Left)
            return;
        m_pressKey = KeyAt(pt);
        m_pressMem = IndexIn(m_memRects, pt);
        const int sci = IndexIn(m_sciRects, pt);
        if (sci >= 0 && m_mode == Mode::Scientific)
            SciClick(sci);
    }

    void ListClick(Point pt, const MouseEvent& e)
    {
        if (e.button != MouseButton::Left)
            return;
        const int tab = IndexIn(m_tabRects, pt);
        if (tab >= 0)
        {
            m_tab = tab;
            m_listScroll = 0.0f;
            return;
        }
        if (!m_trashRect.Empty() && m_trashRect.Contains(pt))
        {
            if (m_tab == 0)
                m_history.clear();
            else
                m_memory.clear();
            return;
        }
        const int i = IndexIn(m_listRects, pt);
        if (i < 0)
            return;
        if (m_tab == 0)
        {
            const HistoryItem h = m_history[static_cast<size_t>(i)];
            ClearAll();
            m_expr = h.expr;
            m_display = h.value;
            m_ready = true;
            m_justEvaluated = true;
            m_flyout = false;
        }
        else
        {
            const Rect row = m_listRects[static_cast<size_t>(i)];
            for (int k = 0; k < 3; ++k)
            {
                const Rect mb{row.Right() - 12.0f - static_cast<float>(3 - k) * 40.0f, row.y + 42.0f, 36.0f, 28.0f};
                if (mb.Contains(pt))
                {
                    if (k == 0)
                        m_memory.erase(m_memory.begin() + i);
                    else
                        m_memory[static_cast<size_t>(i)] += (k == 1 ? 1.0 : -1.0) * Value();
                    return;
                }
            }
            MemRecall(static_cast<size_t>(i));
            m_flyout = false;
        }
    }

    void SciClick(int i)
    {
        if (i == 0)
            m_angle = (m_angle + 1) % 3;
        else if (i == 1)
            m_fe = !m_fe;
        else if (i == 2)
        {
            auto trig = [this](int w, bool inv) {
                return [this, w, inv]() { Trig(w, inv); };
            };
            std::vector<MenuItem> items = {
                MenuItem(L"sin", Icon::None, trig(0, false)),           MenuItem(L"cos", Icon::None, trig(1, false)),
                MenuItem(L"tan", Icon::None, trig(2, false)),           MenuItem::Sep(),
                MenuItem(L"sin⁻¹", Icon::None, trig(0, true)), MenuItem(L"cos⁻¹", Icon::None, trig(1, true)),
                MenuItem(L"tan⁻¹", Icon::None, trig(2, true)),
            };
            GetShell().ShowMenuBelow(LocalToScreen(m_sciRects[2]), std::move(items), m_sciRects[2].w);
        }
        else if (i == 3)
        {
            std::vector<MenuItem> items = {
                MenuItem(L"|x|", Icon::None, [this]() { Unary(L"abs", [](double x) { return std::fabs(x); }); }),
                MenuItem(L"⌊x⌋", Icon::None, [this]() { Unary(L"floor", [](double x) { return std::floor(x); }); }),
                MenuItem(L"⌈x⌉", Icon::None, [this]() { Unary(L"ceil", [](double x) { return std::ceil(x); }); }),
                MenuItem(L"rand", Icon::None, [this]() {
                    StartFresh();
                    m_display = static_cast<double>(RandomFloat(0.0f, 1.0f));
                    m_pendingTerm.clear();
                    m_typing = false;
                    m_ready = true;
                }),
            };
            GetShell().ShowMenuBelow(LocalToScreen(m_sciRects[3]), std::move(items), m_sciRects[3].w);
        }
    }

    void OnMouseUp(const MouseEvent& e) override
    {
        if (HasDialog())
        {
            Window::OnMouseUp(e);
            return;
        }
        Window::OnMouseUp(e);
        const Point pt = e.pos;
        if (m_pressKey >= 0 && KeyAt(pt) == m_pressKey && KeyEnabled(m_keys[static_cast<size_t>(m_pressKey)].def.key))
            Press(m_keys[static_cast<size_t>(m_pressKey)].def.key);
        if (m_pressMem >= 0 && IndexIn(m_memRects, pt) == m_pressMem)
        {
            switch (m_pressMem)
            {
            case 0:
                m_memory.clear();
                break;
            case 1:
                MemRecall();
                break;
            case 2:
                MemAdd(1.0);
                break;
            case 3:
                MemAdd(-1.0);
                break;
            case 4:
                MemStore();
                break;
            case 5:
                if (!m_memory.empty())
                    ToggleFlyout(1);
                break;
            default:
                break;
            }
        }
        m_pressKey = -1;
        m_pressMem = -1;
    }

    bool OnMouseWheel(const MouseEvent& e) override
    {
        if (m_navOpen)
        {
            m_navScroll = Clamp(m_navScroll - e.wheel * 60.0f, 0.0f, NavMaxScroll());
            return true;
        }
        if (ListVisible())
        {
            m_listScroll = std::max(0.0f, m_listScroll - e.wheel * 60.0f);
            const float rowH = m_tab == 0 ? 68.0f : 80.0f;
            const float total = static_cast<float>(m_tab == 0 ? m_history.size() : m_memory.size()) * rowH;
            m_listScroll = std::min(m_listScroll, std::max(0.0f, total - 200.0f));
            return true;
        }
        return Window::OnMouseWheel(e);
    }

    bool OnKeyDown(UINT vk, const KeyMods& mods) override
    {
        if (HasDialog())
            return true;
        if (mods.ctrl)
        {
            switch (vk)
            {
            case 'H':
                if (!SidePanel())
                    ToggleFlyout(0);
                return true;
            case 'M':
                MemStore();
                return true;
            case 'P':
                MemAdd(1.0);
                return true;
            case 'Q':
                MemAdd(-1.0);
                return true;
            case 'R':
                MemRecall();
                return true;
            case 'L':
                m_memory.clear();
                return true;
            case 'C':
                SetClipboardText(GetShell().Hwnd(), m_mode == Mode::Converter ? FormatNumber(Convert(ParseEntry(m_entry)), false)
                                                                               : FormatNumber(Value(), false));
                return true;
            case 'V':
            {
                const std::wstring s = Trim(GetClipboardText(GetShell().Hwnd()));
                bool first = true;
                for (wchar_t c : s)
                {
                    if (c >= L'0' && c <= L'9')
                        Press(static_cast<Key>(static_cast<int>(Key::D0) + (c - L'0')));
                    else if (c == L',' || c == L'.')
                        Press(Key::Dot);
                    else if (c == L'-' && first)
                        continue;
                    first = false;
                }
                return true;
            }
            default:
                break;
            }
            if (vk >= '1' && vk <= '3' && mods.alt)
            {
                SetMode(vk == '1' ? Mode::Standard : Mode::Scientific);
                return true;
            }
        }
        if (mods.alt && (vk == '1' || vk == '2'))
        {
            SetMode(vk == '1' ? Mode::Standard : Mode::Scientific);
            return true;
        }
        switch (vk)
        {
        case VK_RETURN:
            Press(Key::Eq);
            return true;
        case VK_ESCAPE:
            if (m_navOpen)
            {
                m_navOpen = false;
                m_navAnim.Set(0.0f, 0.18f, Ease::OutCubic);
            }
            else if (m_flyout)
                m_flyout = false;
            else
                Press(Key::C);
            return true;
        case VK_BACK:
            Press(Key::Back);
            return true;
        case VK_DELETE:
            Press(Key::CE);
            return true;
        case VK_F9:
            Press(Key::Neg);
            return true;
        case VK_ADD:
            Press(Key::Add);
            return true;
        case VK_SUBTRACT:
            Press(Key::Sub);
            return true;
        case VK_MULTIPLY:
            Press(Key::Mul);
            return true;
        case VK_DIVIDE:
            Press(Key::Div);
            return true;
        case VK_DECIMAL:
            Press(Key::Dot);
            return true;
        default:
            break;
        }
        if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
        {
            Press(static_cast<Key>(static_cast<int>(Key::D0) + static_cast<int>(vk - VK_NUMPAD0)));
            return true;
        }
        return false;
    }

    bool OnChar(wchar_t ch) override
    {
        if (HasDialog())
            return true;
        if (ch >= L'0' && ch <= L'9')
        {
            Press(static_cast<Key>(static_cast<int>(Key::D0) + (ch - L'0')));
            return true;
        }
        switch (ch)
        {
        case L'+':
            Press(Key::Add);
            break;
        case L'-':
            Press(Key::Sub);
            break;
        case L'*':
            Press(Key::Mul);
            break;
        case L'/':
            Press(Key::Div);
            break;
        case L'=':
            Press(Key::Eq);
            break;
        case L',':
        case L'.':
            Press(Key::Dot);
            break;
        case L'%':
            Press(Key::Percent);
            break;
        case L'r':
        case L'R':
            Press(Key::Inv);
            break;
        case L'@':
            Press(Key::Sqrt);
            break;
        case L'q':
        case L'Q':
            Press(Key::Sqr);
            break;
        case L'(':
            if (m_mode == Mode::Scientific)
                Press(Key::LParen);
            break;
        case L')':
            if (m_mode == Mode::Scientific)
                Press(Key::RParen);
            break;
        case L'^':
            if (m_mode == Mode::Scientific)
                Press(Key::Pow);
            break;
        case L'!':
            if (m_mode == Mode::Scientific)
                Press(Key::Fact);
            break;
        case L'p':
        case L'P':
            if (m_mode == Mode::Scientific)
                Press(Key::Pi);
            break;
        default:
            break;
        }
        return true;
    }

    // ================================================================== поля
    Mode m_mode = Mode::Standard;
    int m_cat = 0;
    std::vector<Category> m_cats;
    bool m_compact = false;
    // Вычисления
    std::vector<double> m_vals;
    std::vector<wchar_t> m_ops;
    std::wstring m_expr;
    std::wstring m_pendingTerm;
    std::wstring m_entry = L"0";
    double m_display = 0.0;
    bool m_typing = false;
    bool m_ready = false;
    bool m_justEvaluated = false;
    std::wstring m_error;
    wchar_t m_lastOp = 0;
    double m_lastOperand = 0.0;
    int m_parens = 0;
    bool m_second = false;
    int m_angle = 0; // 0 — градусы, 1 — радианы, 2 — грады
    bool m_fe = false;
    std::vector<HistoryItem> m_history;
    std::vector<double> m_memory;
    // Интерфейс
    std::vector<KeyRect> m_keys;
    std::vector<Rect> m_memRects;
    std::vector<Rect> m_sciRects;
    std::vector<Rect> m_listRects;
    std::vector<Rect> m_tabRects;
    Rect m_trashRect;
    int m_hoverKey = -1;
    int m_pressKey = -1;
    int m_hoverMem = -1;
    int m_pressMem = -1;
    int m_hoverSci = -1;
    int m_hoverList = -1;
    int m_hoverNav = -1;
    bool m_hoverTrash = false;
    int m_tab = 0;
    float m_listScroll = 0.0f;
    bool m_flyout = false;
    Tween m_flyAnim;
    bool m_navOpen = false;
    Tween m_navAnim;
    float m_navScroll = 0.0f;
    Button* m_navBtn = nullptr;
    Button* m_topBtn = nullptr;
    Button* m_histBtn = nullptr;
    ComboBox* m_fromCombo = nullptr;
    ComboBox* m_toCombo = nullptr;
};
} // namespace

std::unique_ptr<Window> CreateCalculator(const std::wstring& /*arg*/) { return std::make_unique<CalculatorWindow>(); }
