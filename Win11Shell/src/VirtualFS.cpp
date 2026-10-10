#include "VirtualFS.h"

unsigned long long TextBytes(const std::wstring& s)
{
    unsigned long long n = 3; // BOM
    for (wchar_t c : s)
    {
        if (c < 0x80)
            n += (c == L'\n') ? 2 : 1;
        else if (c < 0x800)
            n += 2;
        else
            n += 3;
    }
    return n;
}

namespace
{
std::wstring Extension(const std::wstring& name)
{
    const size_t dot = name.find_last_of(L'.');
    if (dot == std::wstring::npos || dot == 0)
        return L"";
    return ToLower(name.substr(dot + 1));
}
} // namespace

VirtualFS::VirtualFS()
{
    wchar_t buf[256] = {};
    DWORD len = 256;
    if (GetUserNameW(buf, &len) && buf[0])
        m_user = buf;
    else
        m_user = L"Пользователь";

    m_root = Make(L"Этот компьютер", FileKind::Folder, 400, 0);
    m_root->system = true;
    m_bin = Make(L"Корзина", FileKind::Folder, 400, 0);
    m_bin->system = true;

    auto drive = [&](const std::wstring& name, unsigned long long capGb, unsigned long long freeMb) {
        auto d = Make(name, FileKind::Drive, 400, 0);
        d->system = true;
        d->capacity = capGb * 1024ULL * 1024ULL * 1024ULL;
        d->freeSpace = freeMb * 1024ULL * 1024ULL;
        return Add(m_root.get(), std::move(d));
    };
    m_c = drive(L"Локальный диск (C:)", 476, 218540);
    FsNode* d = drive(L"Данные (D:)", 931, 626380);

    auto folder = [&](FsNode* parent, const std::wstring& name, int daysAgo, bool sys = false) {
        auto f = Make(name, FileKind::Folder, daysAgo, 0);
        f->system = sys;
        return Add(parent, std::move(f));
    };
    auto file = [&](FsNode* parent, const std::wstring& name, int daysAgo, unsigned long long size, int seed = 0) {
        auto f = Make(name, KindFor(name), daysAgo, size);
        f->seed = seed;
        return Add(parent, std::move(f));
    };
    auto text = [&](FsNode* parent, const std::wstring& name, int daysAgo, const std::wstring& content) {
        auto f = Make(name, FileKind::Text, daysAgo, 0);
        f->content = content;
        return Add(parent, std::move(f));
    };

    // Системные папки диска C:
    FsNode* pf = folder(m_c, L"Program Files", 300, true);
    folder(pf, L"Common Files", 300, true);
    folder(pf, L"Утилиты", 120);
    FsNode* pf86 = folder(m_c, L"Program Files (x86)", 300, true);
    folder(pf86, L"Common Files", 300, true);
    FsNode* win = folder(m_c, L"Windows", 300, true);
    folder(win, L"System32", 300, true);
    folder(win, L"Fonts", 300, true);
    folder(win, L"Temp", 2, true);
    FsNode* users = folder(m_c, L"Пользователи", 300, true);
    folder(users, L"Общие", 300, true);
    m_home = folder(users, m_user, 300, true);

    m_desktop = folder(m_home, L"Рабочий стол", 200, true);
    m_docs = folder(m_home, L"Документы", 200, true);
    m_downloads = folder(m_home, L"Загрузки", 200, true);
    m_pictures = folder(m_home, L"Изображения", 200, true);
    m_music = folder(m_home, L"Музыка", 200, true);
    m_videos = folder(m_home, L"Видео", 200, true);
    m_screens = folder(m_pictures, L"Снимки экрана", 30);

    // Рабочий стол
    text(m_desktop, L"Заметки.txt", 0,
         L"Добро пожаловать!\n\nЭто демонстрация интерфейса Windows 11, написанная на C++ (WinAPI + GDI+).\n\n"
         L"Попробуйте:\n"
         L"  • перетаскивать окна за заголовок и прижимать их к краям экрана;\n"
         L"  • навести курсор на кнопку «Развернуть» — появятся макеты прикрепления;\n"
         L"  • открыть меню «Пуск» клавишей Win и начать вводить текст;\n"
         L"  • переключить тему в «Параметры > Персонализация > Цвета»;\n"
         L"  • щёлкнуть правой кнопкой по рабочему столу.\n");
    FsNode* proj = folder(m_desktop, L"Проект", 3);
    text(proj, L"README.txt", 3, L"Проект «Окна»\n\nСписок задач:\n1. Оконный менеджер\n2. Панель задач\n3. Меню «Пуск»\n");
    text(proj, L"Идеи.txt", 5, L"— Тёмная тема\n— Акриловые меню\n— Макеты прикрепления\n");
    file(proj, L"Макет.png", 4, 284311, 7);

    // Документы
    text(m_docs, L"План на неделю.txt", 1,
         L"Понедельник — созвон с командой\nВторник — обзор дизайна\nСреда — тестирование\n"
         L"Четверг — релиз\nПятница — ретроспектива\n");
    file(m_docs, L"Отчёт за квартал.docx", 6, 48213);
    file(m_docs, L"Бюджет 2026.xlsx", 9, 23310);
    file(m_docs, L"Презентация.pptx", 14, 2304512);
    file(m_docs, L"Инструкция.pdf", 30, 812044);
    FsNode* recipes = folder(m_docs, L"Рецепты", 20);
    text(recipes, L"Блины.txt", 20,
         L"Блины\n\nМолоко — 500 мл\nЯйца — 2 шт.\nМука — 200 г\nСахар — 1 ст. л.\nСоль — щепотка\n"
         L"Масло — 2 ст. л.\n\nСмешать, дать постоять 15 минут и жарить на горячей сковороде.\n");
    text(recipes, L"Борщ.txt", 22,
         L"Борщ\n\nСвёкла, капуста, картофель, морковь, лук, томатная паста.\nВарить на медленном огне.\n");
    FsNode* work = folder(m_docs, L"Работа", 12);
    file(work, L"Договор.pdf", 40, 158220);
    text(work, L"Контакты.txt", 12, L"Анна — +7 900 000-00-01\nБорис — +7 900 000-00-02\n");

    // Загрузки
    file(m_downloads, L"setup.exe", 2, 5823412);
    file(m_downloads, L"Фото с отпуска.zip", 8, 104857600);
    file(m_downloads, L"Документация.pdf", 10, 3355443);
    file(m_downloads, L"Обои.png", 3, 1812011, 11);

    // Изображения
    const wchar_t* pics[] = {L"Горы.jpg", L"Озеро.jpg", L"Закат.jpg", L"Лес.jpg", L"Город ночью.jpg", L"Море.jpg",
                             L"Поле.jpg", L"Северное сияние.jpg"};
    for (int i = 0; i < 8; ++i)
        file(m_pictures, pics[i], 5 + i * 3, 2400000ULL + static_cast<unsigned long long>(i) * 371233ULL, i + 1);

    // Музыка и видео
    file(m_music, L"Утренний джаз.mp3", 45, 7340032);
    file(m_music, L"Дождь за окном.mp3", 50, 5242880);
    file(m_music, L"Тишина.mp3", 60, 3145728);
    file(m_videos, L"Отпуск 2025.mp4", 70, 734003200);
    file(m_videos, L"Запись экрана.mp4", 15, 52428800);

    // Диск D:
    FsNode* movies = folder(d, L"Фильмы", 100);
    file(movies, L"Документальный фильм.mkv", 100, 2147483648ULL);
    FsNode* backup = folder(d, L"Резервные копии", 50);
    file(backup, L"Архив документов.zip", 50, 314572800);
    folder(d, L"Игры", 90);

    // Корзина
    auto old = Make(L"Старый черновик.txt", FileKind::Text, 4, 0);
    old->content = L"Этот файл лежит в корзине. Его можно восстановить.";
    old->originalParent = m_docs;
    old->originalPath = PathOf(m_docs);
    old->deleted = AddDays(LocalNow(), -1);
    Add(m_bin.get(), std::move(old));
}

VirtualFS::~VirtualFS() = default;

std::unique_ptr<FsNode> VirtualFS::Make(const std::wstring& name, FileKind kind, int daysAgo, unsigned long long size)
{
    auto n = std::make_unique<FsNode>();
    n->id = m_nextId++;
    n->name = name;
    n->kind = kind;
    n->size = size;
    SYSTEMTIME st = AddDays(LocalNow(), -daysAgo);
    if (daysAgo > 0)
    {
        st.wHour = static_cast<WORD>(9 + (static_cast<int>(n->id) * 7) % 10);
        st.wMinute = static_cast<WORD>((static_cast<int>(n->id) * 17) % 60);
    }
    n->modified = st;
    return n;
}

FsNode* VirtualFS::Add(FsNode* parent, std::unique_ptr<FsNode> n)
{
    n->parent = parent;
    FsNode* raw = n.get();
    parent->children.push_back(std::move(n));
    ++m_version;
    return raw;
}

std::unique_ptr<FsNode> VirtualFS::Detach(FsNode* n)
{
    if (!n || !n->parent)
        return nullptr;
    auto& v = n->parent->children;
    for (auto it = v.begin(); it != v.end(); ++it)
    {
        if (it->get() == n)
        {
            std::unique_ptr<FsNode> out = std::move(*it);
            v.erase(it);
            out->parent = nullptr;
            ++m_version;
            return out;
        }
    }
    return nullptr;
}

std::wstring VirtualFS::PathOf(const FsNode* n) const
{
    if (!n)
        return L"";
    if (n == m_root.get())
        return L"Этот компьютер";
    if (n == m_bin.get())
        return L"Корзина";
    std::vector<const FsNode*> chain;
    for (const FsNode* p = n; p && p != m_root.get() && p != m_bin.get(); p = p->parent)
        chain.push_back(p);
    std::wstring path;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        const FsNode* p = *it;
        if (p->kind == FileKind::Drive)
        {
            const size_t open = p->name.find(L'(');
            path = open != std::wstring::npos ? p->name.substr(open + 1, 2) : p->name;
        }
        else
            path += L"\\" + p->name;
    }
    if (path.size() == 2)
        path += L"\\";
    return path;
}

std::vector<FsNode*> VirtualFS::Chain(FsNode* n) const
{
    std::vector<FsNode*> chain;
    for (FsNode* p = n; p; p = p->parent)
        chain.insert(chain.begin(), p);
    return chain;
}

FsNode* VirtualFS::Find(const FsNode* dir, const std::wstring& name) const
{
    if (!dir)
        return nullptr;
    const std::wstring ln = ToLower(name);
    for (auto& c : dir->children)
    {
        if (ToLower(c->name) == ln)
            return c.get();
    }
    return nullptr;
}

FsNode* VirtualFS::Resolve(const std::wstring& rawPath, FsNode* cwd) const
{
    std::wstring path = Trim(rawPath);
    if (path.size() >= 2 && path.front() == L'"' && path.back() == L'"')
        path = path.substr(1, path.size() - 2);
    if (path.empty())
        return cwd;
    FsNode* cur = cwd;
    size_t pos = 0;
    if (path.size() >= 2 && path[1] == L':')
    {
        const wchar_t letter = static_cast<wchar_t>(towupper(path[0]));
        cur = nullptr;
        for (auto& c : m_root->children)
        {
            if (c->kind == FileKind::Drive && c->name.find(std::wstring(1, letter) + L":") != std::wstring::npos)
                cur = c.get();
        }
        if (!cur)
            return nullptr;
        pos = 2;
    }
    else if (!path.empty() && (path[0] == L'\\' || path[0] == L'/'))
    {
        cur = m_c;
    }
    else if (path[0] == L'~')
    {
        cur = m_home;
        pos = 1;
    }
    while (pos < path.size() && cur)
    {
        while (pos < path.size() && (path[pos] == L'\\' || path[pos] == L'/'))
            ++pos;
        if (pos >= path.size())
            break;
        size_t end = path.find_first_of(L"\\/", pos);
        if (end == std::wstring::npos)
            end = path.size();
        const std::wstring part = path.substr(pos, end - pos);
        pos = end;
        if (part == L".")
            continue;
        if (part == L"..")
        {
            if (cur->parent && cur->parent != m_root.get())
                cur = cur->parent;
            continue;
        }
        FsNode* next = Find(cur, part);
        if (!next && ToLower(part) == L"users")
            next = Find(cur, L"Пользователи");
        cur = next;
    }
    return cur;
}

static FsNode* FindIn(FsNode* n, unsigned id)
{
    if (!n)
        return nullptr;
    if (n->id == id)
        return n;
    for (auto& c : n->children)
    {
        if (FsNode* f = FindIn(c.get(), id))
            return f;
    }
    return nullptr;
}

FsNode* VirtualFS::FindById(unsigned id) const
{
    if (FsNode* f = FindIn(m_root.get(), id))
        return f;
    return FindIn(m_bin.get(), id);
}

FsNode* VirtualFS::FromArg(const std::wstring& arg) const
{
    if (arg.empty())
        return nullptr;
    if (arg.rfind(L"::node:", 0) == 0)
        return FindById(static_cast<unsigned>(std::wcstoul(arg.c_str() + 7, nullptr, 10)));
    if (arg == L"::pc")
        return m_root.get();
    if (arg == L"::bin")
        return m_bin.get();
    if (arg.rfind(L"::", 0) == 0)
        return nullptr;
    return Resolve(arg, m_root.get());
}

std::wstring VirtualFS::UniqueName(const FsNode* parent, const std::wstring& name) const
{
    if (!Find(parent, name))
        return name;
    std::wstring base = name, ext;
    const size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos && dot > 0)
    {
        base = name.substr(0, dot);
        ext = name.substr(dot);
    }
    for (int i = 2; i < 1000; ++i)
    {
        const std::wstring cand = base + L" (" + std::to_wstring(i) + L")" + ext;
        if (!Find(parent, cand))
            return cand;
    }
    return name;
}

FsNode* VirtualFS::NewFolder(FsNode* parent, const std::wstring& base)
{
    if (!parent)
        return nullptr;
    return Add(parent, Make(UniqueName(parent, base), FileKind::Folder, 0, 0));
}

FsNode* VirtualFS::NewFile(FsNode* parent, const std::wstring& name, FileKind kind, const std::wstring& content)
{
    if (!parent)
        return nullptr;
    auto n = Make(UniqueName(parent, name), kind, 0, 0);
    n->content = content;
    if (kind == FileKind::Image && n->seed == 0)
        n->seed = RandomInt(1, 50);
    return Add(parent, std::move(n));
}

bool VirtualFS::ValidName(const std::wstring& name)
{
    const std::wstring t = Trim(name);
    if (t.empty() || t == L"." || t == L"..")
        return false;
    return t.find_first_of(L"\\/:*?\"<>|") == std::wstring::npos;
}

bool VirtualFS::Rename(FsNode* n, const std::wstring& newName)
{
    if (!n || n->system || !ValidName(newName))
        return false;
    const std::wstring t = Trim(newName);
    if (t == n->name)
        return true;
    FsNode* other = Find(n->parent, t);
    if (other && other != n)
        return false;
    n->name = t;
    n->modified = LocalNow();
    if (!n->IsFolder() && n->kind != FileKind::Shortcut)
        n->kind = KindFor(t) == FileKind::Other && n->kind == FileKind::Text ? FileKind::Text : KindFor(t);
    ++m_version;
    return true;
}

void VirtualFS::Recycle(FsNode* n)
{
    if (!n || n->system || !n->parent)
        return;
    if (InBin(n))
    {
        DeleteForever(n);
        return;
    }
    FsNode* parent = n->parent;
    const std::wstring path = PathOf(parent);
    auto owned = Detach(n);
    if (!owned)
        return;
    owned->originalParent = parent;
    owned->originalPath = path;
    owned->deleted = LocalNow();
    owned->name = UniqueName(m_bin.get(), owned->name);
    Add(m_bin.get(), std::move(owned));
}

void VirtualFS::Restore(FsNode* n)
{
    if (!n || n->parent != m_bin.get())
        return;
    FsNode* dest = (n->originalParent && IsAlive(n->originalParent)) ? n->originalParent : m_desktop;
    auto owned = Detach(n);
    owned->name = UniqueName(dest, owned->name);
    Add(dest, std::move(owned));
}

void VirtualFS::DeleteForever(FsNode* n)
{
    if (!n || n->system)
        return;
    auto owned = Detach(n);
    if (owned)
        m_graveyard.push_back(std::move(owned));
}

void VirtualFS::EmptyBin()
{
    while (!m_bin->children.empty())
        DeleteForever(m_bin->children.back().get());
    ++m_version;
}

bool VirtualFS::Move(FsNode* n, FsNode* dest)
{
    if (!n || !dest || n->system || !dest->IsFolder() || n->parent == dest || IsAncestor(n, dest) || n == dest)
        return false;
    if (dest == m_bin.get())
    {
        Recycle(n);
        return true;
    }
    auto owned = Detach(n);
    if (!owned)
        return false;
    owned->name = UniqueName(dest, owned->name);
    Add(dest, std::move(owned));
    return true;
}

FsNode* VirtualFS::CopyTo(const FsNode* n, FsNode* dest)
{
    if (!n || !dest || !dest->IsFolder() || IsAncestor(n, dest))
        return nullptr;
    auto copy = Make(UniqueName(dest, n->name), n->kind, 0, n->size);
    copy->content = n->content;
    copy->seed = n->seed;
    copy->app = n->app;
    copy->bitmap = n->bitmap;
    copy->modified = n->modified;
    FsNode* raw = Add(dest, std::move(copy));
    for (auto& c : n->children)
        CopyTo(c.get(), raw);
    return raw;
}

void VirtualFS::SaveText(FsNode* n, const std::wstring& text)
{
    if (!n)
        return;
    n->content = text;
    n->modified = LocalNow();
    ++m_version;
}

bool VirtualFS::IsAlive(const FsNode* n) const
{
    for (const FsNode* p = n; p; p = p->parent)
    {
        if (p == m_root.get() || p == m_bin.get())
            return true;
    }
    return false;
}

bool VirtualFS::InBin(const FsNode* n) const
{
    for (const FsNode* p = n; p; p = p->parent)
    {
        if (p == m_bin.get())
            return true;
    }
    return false;
}

bool VirtualFS::IsAncestor(const FsNode* a, const FsNode* b) const
{
    for (const FsNode* p = b ? b->parent : nullptr; p; p = p->parent)
    {
        if (p == a)
            return true;
    }
    return false;
}

unsigned long long VirtualFS::SizeOf(const FsNode* n) const
{
    if (!n)
        return 0;
    if (n->kind == FileKind::Text)
        return TextBytes(n->content);
    if (n->kind == FileKind::Drive)
        return n->capacity - n->freeSpace;
    unsigned long long s = n->size;
    for (auto& c : n->children)
        s += SizeOf(c.get());
    return s;
}

int VirtualFS::CountItems(const FsNode* n, bool folders) const
{
    int count = 0;
    for (auto& c : n->children)
    {
        if (c->IsFolder() == folders)
            ++count;
        count += CountItems(c.get(), folders);
    }
    return count;
}

void VirtualFS::CollectRecent(FsNode* n, std::vector<FsNode*>& out) const
{
    for (auto& c : n->children)
    {
        if (c->IsFolder())
        {
            if (!c->system || c.get() == m_desktop || c.get() == m_docs || c.get() == m_downloads ||
                c.get() == m_pictures || c.get() == m_music || c.get() == m_videos)
                CollectRecent(c.get(), out);
        }
        else
            out.push_back(c.get());
    }
}

static long long StampOf(const SYSTEMTIME& st)
{
    FILETIME ft{};
    SystemTimeToFileTime(&st, &ft);
    return static_cast<long long>((static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime);
}

std::vector<FsNode*> VirtualFS::Recent(size_t max) const
{
    std::vector<FsNode*> all;
    CollectRecent(m_home, all);
    std::sort(all.begin(), all.end(), [](const FsNode* a, const FsNode* b) { return StampOf(a->modified) > StampOf(b->modified); });
    if (all.size() > max)
        all.resize(max);
    return all;
}

void VirtualFS::Search(FsNode* dir, const std::wstring& query, std::vector<FsNode*>& out) const
{
    if (!dir)
        return;
    for (auto& c : dir->children)
    {
        if (ContainsNoCase(c->name, query))
            out.push_back(c.get());
        if (c->IsFolder())
            Search(c.get(), query, out);
    }
}

std::wstring VirtualFS::SplitExtension(const std::wstring& name, std::wstring& ext)
{
    const size_t dot = name.find_last_of(L'.');
    if (dot == std::wstring::npos || dot == 0)
    {
        ext.clear();
        return name;
    }
    ext = name.substr(dot);
    return name.substr(0, dot);
}

std::wstring VirtualFS::DisplayName(const FsNode* n, bool showExtensions)
{
    if (!n)
        return L"";
    if (showExtensions || n->IsFolder() || KindFor(n->name) == FileKind::Other)
        return n->name;
    std::wstring ext;
    return SplitExtension(n->name, ext);
}

void VirtualFS::Paste(FsNode* dest)
{
    if (!dest || !dest->IsFolder())
        return;
    std::vector<FsNode*> items = clip;
    for (FsNode* n : items)
    {
        if (!IsAlive(n) || n == dest)
            continue;
        if (clipCut)
            Move(n, dest);
        else
            CopyTo(n, dest);
    }
    if (clipCut)
        clip.clear();
    ++m_version;
}

FileKind VirtualFS::KindFor(const std::wstring& name)
{
    const std::wstring ext = Extension(name);
    if (ext.empty())
        return FileKind::Other;
    if (ext == L"txt" || ext == L"log" || ext == L"md" || ext == L"ini" || ext == L"csv" || ext == L"cfg" || ext == L"cpp" ||
        ext == L"h" || ext == L"json" || ext == L"xml" || ext == L"bat" || ext == L"ps1")
        return FileKind::Text;
    if (ext == L"jpg" || ext == L"jpeg" || ext == L"png" || ext == L"bmp" || ext == L"gif")
        return FileKind::Image;
    if (ext == L"mp3" || ext == L"wav" || ext == L"flac" || ext == L"ogg")
        return FileKind::Audio;
    if (ext == L"mp4" || ext == L"avi" || ext == L"mkv" || ext == L"mov")
        return FileKind::Video;
    if (ext == L"docx" || ext == L"doc" || ext == L"pdf" || ext == L"xlsx" || ext == L"pptx" || ext == L"rtf")
        return FileKind::Document;
    if (ext == L"zip" || ext == L"rar" || ext == L"7z")
        return FileKind::Archive;
    if (ext == L"exe" || ext == L"msi")
        return FileKind::App;
    if (ext == L"lnk")
        return FileKind::Shortcut;
    return FileKind::Other;
}

AppIcon VirtualFS::IconFor(const FsNode* n) const
{
    if (!n)
        return AppIcon::FileGeneric;
    if (n == m_root.get())
        return AppIcon::ThisPC;
    if (n == m_bin.get())
        return m_bin->children.empty() ? AppIcon::RecycleBinEmpty : AppIcon::RecycleBinFull;
    if (n == m_desktop)
        return AppIcon::FolderDesktop;
    if (n == m_docs)
        return AppIcon::FolderDocuments;
    if (n == m_downloads)
        return AppIcon::FolderDownloads;
    if (n == m_pictures)
        return AppIcon::FolderPictures;
    if (n == m_music)
        return AppIcon::FolderMusic;
    if (n == m_videos)
        return AppIcon::FolderVideos;
    switch (n->kind)
    {
    case FileKind::Folder:
        return AppIcon::Folder;
    case FileKind::Drive:
        return n == m_c ? AppIcon::DriveSystem : AppIcon::Drive;
    case FileKind::Text:
        return AppIcon::FileText;
    case FileKind::Image:
        return AppIcon::FileImage;
    case FileKind::Audio:
        return AppIcon::FileAudio;
    case FileKind::Video:
        return AppIcon::FileVideo;
    case FileKind::Document:
        return AppIcon::FileDoc;
    case FileKind::Archive:
        return AppIcon::FileArchive;
    case FileKind::App:
        return AppIcon::FileApp;
    case FileKind::Shortcut:
        return n->app != AppId::None ? GetAppInfo(n->app).icon : AppIcon::FileGeneric;
    case FileKind::Other:
        return AppIcon::FileGeneric;
    }
    return AppIcon::FileGeneric;
}

std::wstring VirtualFS::TypeName(const FsNode* n) const
{
    if (!n)
        return L"";
    const std::wstring ext = Extension(n->name);
    std::wstring upper = ext;
    for (wchar_t& c : upper)
        c = static_cast<wchar_t>(towupper(c));
    switch (n->kind)
    {
    case FileKind::Folder:
        return L"Папка с файлами";
    case FileKind::Drive:
        return L"Локальный диск";
    case FileKind::Text:
        return L"Текстовый документ";
    case FileKind::Image:
    case FileKind::Audio:
    case FileKind::Video:
        return L"Файл «" + upper + L"»";
    case FileKind::Document:
        if (ext == L"pdf")
            return L"Документ PDF";
        if (ext == L"xlsx")
            return L"Электронная таблица";
        if (ext == L"pptx")
            return L"Презентация";
        return L"Документ";
    case FileKind::Archive:
        return L"Сжатая ZIP-папка";
    case FileKind::App:
        return L"Приложение";
    case FileKind::Shortcut:
        return L"Ярлык";
    case FileKind::Other:
        return upper.empty() ? L"Файл" : L"Файл «" + upper + L"»";
    }
    return L"Файл";
}
