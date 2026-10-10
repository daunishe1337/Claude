#pragma once
// Рабочий стол: процедурные обои, значки (сетка), выделение рамкой, перетаскивание,
// переименование и контекстные меню.

#include "Controls.h"
#include "Shell.h"

// Процедурные обои (собственные градиенты, без изображений Microsoft).
struct WallpaperInfo
{
    const wchar_t* name;
    bool dark;
};
const std::vector<WallpaperInfo>& Wallpapers();
void RenderWallpaper(int variant, int width, int height, std::vector<uint32_t>& out);

class Desktop
{
public:
    Desktop();

    void Draw(Renderer& r);
    void Sync(); // синхронизация значков с папкой «Рабочий стол»

    void MouseDown(const MouseEvent& e);
    void MouseMove(const MouseEvent& e);
    void MouseUp(const MouseEvent& e);
    void MouseLeave();
    bool KeyDown(UINT vk, const KeyMods& mods);
    bool Char(wchar_t ch);
    CursorType Cursor(Point p) const;
    bool Renaming() const { return m_renameIndex >= 0; }
    void CancelRename();
    void FinishRename() { CommitRename(); }
    void Refresh();
    void StartRename(FsNode* node);
    void ClearSelection();

private:
    struct Icon2
    {
        enum class Type
        {
            ThisPC,
            RecycleBin,
            Node,
        } type = Type::Node;
        FsNode* node = nullptr;
        int col = 0;
        int row = 0;
        bool selected = false;
    };

    float CellW() const;
    float CellH() const;
    float IconSize() const;
    int Rows() const;
    int Cols() const;
    Rect CellRect(int col, int row) const;
    Rect IconRect(const Icon2& ic) const;
    std::wstring NameOf(const Icon2& ic) const;
    AppIcon IconOf(const Icon2& ic) const;
    int IconAt(Point p) const;
    bool CellFree(int col, int row, int ignore = -1) const;
    void FreeCell(int& col, int& row) const;
    void Arrange(int sortMode);
    void Open(int index);
    void DeleteSelected();
    void ShowBackgroundMenu(Point p, bool classic);
    void ShowIconMenu(int index, Point p);
    std::vector<FsNode*> SelectedNodes() const;
    void CommitRename();
    void Drop(Point p);

    std::vector<Icon2> m_icons;
    unsigned m_fsVersion = 0;
    int m_hover = -1;
    int m_focus = -1;
    // Выделение рамкой / перетаскивание
    bool m_banding = false;
    bool m_dragging = false;
    bool m_pressed = false;
    int m_pressIndex = -1;
    Point m_pressPos;
    Point m_dragPos;
    std::vector<bool> m_bandBase;
    // Переименование
    int m_renameIndex = -1;
    Panel m_ui;
    TextBox* m_renameBox = nullptr;
    Tween m_refresh{1.0f};
};
