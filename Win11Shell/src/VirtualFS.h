#pragma once
// Виртуальная файловая система в памяти: диски, папки пользователя, файлы, корзина.

#include "Apps.h"

enum class FileKind
{
    Folder,
    Drive,
    Text,
    Image,
    Audio,
    Video,
    Document,
    Archive,
    App,
    Shortcut,
    Other,
};

struct FsNode
{
    unsigned id = 0;
    std::wstring name;
    FileKind kind = FileKind::Folder;
    std::wstring content;              // текст (для .txt)
    unsigned long long size = 0;       // размер в байтах (для нетекстовых файлов)
    SYSTEMTIME modified{};
    FsNode* parent = nullptr;
    std::vector<std::unique_ptr<FsNode>> children;
    int seed = 0;                       // для процедурных изображений
    AppId app = AppId::None;            // цель ярлыка
    FsNode* originalParent = nullptr;   // для восстановления из корзины
    std::wstring originalPath;
    SYSTEMTIME deleted{};
    unsigned long long capacity = 0;    // для дисков
    unsigned long long freeSpace = 0;
    bool system = false;                // нельзя удалить/переименовать
    std::shared_ptr<Gdiplus::Bitmap> bitmap; // реальное изображение (снимки экрана, Paint)

    bool IsFolder() const { return kind == FileKind::Folder || kind == FileKind::Drive; }
};

class VirtualFS
{
public:
    VirtualFS();
    ~VirtualFS();

    FsNode* Root() const { return m_root.get(); }
    FsNode* Bin() const { return m_bin.get(); }
    FsNode* DriveC() const { return m_c; }
    FsNode* Home() const { return m_home; }
    FsNode* Desktop() const { return m_desktop; }
    FsNode* Documents() const { return m_docs; }
    FsNode* Downloads() const { return m_downloads; }
    FsNode* Pictures() const { return m_pictures; }
    FsNode* Music() const { return m_music; }
    FsNode* Videos() const { return m_videos; }
    FsNode* Screenshots() const { return m_screens; }
    const std::wstring& UserName() const { return m_user; }

    unsigned Version() const { return m_version; }
    void Touch() { ++m_version; }

    std::wstring PathOf(const FsNode* n) const;
    std::vector<FsNode*> Chain(FsNode* n) const;
    FsNode* Find(const FsNode* dir, const std::wstring& name) const;
    FsNode* Resolve(const std::wstring& path, FsNode* cwd) const;
    FsNode* FindById(unsigned id) const;
    // Аргумент запуска: «::node:ID», «::pc», «::bin» или путь.
    FsNode* FromArg(const std::wstring& arg) const;
    static std::wstring NodeArg(const FsNode* n) { return L"::node:" + std::to_wstring(n ? n->id : 0); }

    FsNode* NewFolder(FsNode* parent, const std::wstring& base = L"Новая папка");
    FsNode* NewFile(FsNode* parent, const std::wstring& name, FileKind kind, const std::wstring& content = L"");
    std::wstring UniqueName(const FsNode* parent, const std::wstring& name) const;
    bool Rename(FsNode* n, const std::wstring& newName);
    void Recycle(FsNode* n);
    void Restore(FsNode* n);
    void DeleteForever(FsNode* n);
    void EmptyBin();
    bool Move(FsNode* n, FsNode* dest);
    FsNode* CopyTo(const FsNode* n, FsNode* dest);
    void SaveText(FsNode* n, const std::wstring& text);

    bool IsAlive(const FsNode* n) const;
    bool InBin(const FsNode* n) const;
    bool IsAncestor(const FsNode* a, const FsNode* b) const;
    unsigned long long SizeOf(const FsNode* n) const;
    int CountItems(const FsNode* n, bool folders) const;
    std::vector<FsNode*> Recent(size_t max) const;
    void Search(FsNode* dir, const std::wstring& query, std::vector<FsNode*>& out) const;

    // Имя для отображения (без расширения, если расширения скрыты).
    static std::wstring DisplayName(const FsNode* n, bool showExtensions);
    static std::wstring SplitExtension(const std::wstring& name, std::wstring& ext);

    // Буфер обмена файлов (вырезать/копировать/вставить).
    std::vector<FsNode*> clip;
    bool clipCut = false;
    void Paste(FsNode* dest);

    static FileKind KindFor(const std::wstring& name);
    static bool ValidName(const std::wstring& name);
    AppIcon IconFor(const FsNode* n) const;
    std::wstring TypeName(const FsNode* n) const;

private:
    FsNode* Add(FsNode* parent, std::unique_ptr<FsNode> n);
    std::unique_ptr<FsNode> Detach(FsNode* n);
    std::unique_ptr<FsNode> Make(const std::wstring& name, FileKind kind, int daysAgo, unsigned long long size);
    void CollectRecent(FsNode* n, std::vector<FsNode*>& out) const;

    std::unique_ptr<FsNode> m_root;
    std::unique_ptr<FsNode> m_bin;
    std::vector<std::unique_ptr<FsNode>> m_graveyard;
    FsNode* m_c = nullptr;
    FsNode* m_home = nullptr;
    FsNode* m_desktop = nullptr;
    FsNode* m_docs = nullptr;
    FsNode* m_downloads = nullptr;
    FsNode* m_pictures = nullptr;
    FsNode* m_music = nullptr;
    FsNode* m_videos = nullptr;
    FsNode* m_screens = nullptr;
    unsigned m_version = 1;
    unsigned m_nextId = 1;
    std::wstring m_user;
};

unsigned long long TextBytes(const std::wstring& s);
