#pragma once
// Меню «Пуск»: поиск, закреплённые приложения (страницы), «Рекомендуем», список всех
// приложений, результаты поиска, меню питания и учётной записи.

#include "Flyouts.h"

class StartMenu : public Flyout
{
public:
    StartMenu();

    Rect Bounds() const override;
    void Draw(Renderer& r) override;
    void MouseMove(const MouseEvent& e) override;
    void MouseDown(const MouseEvent& e) override;
    void MouseUp(const MouseEvent& e) override;
    bool MouseWheel(const MouseEvent& e) override;
    void MouseLeave() override;
    bool KeyDown(UINT vk, const KeyMods& mods) override;
    bool Char(wchar_t ch) override;
    CursorType Cursor(Point p) const override;

    void OpenSearch();
    bool IsPinned(AppId app) const;
    void Pin(AppId app);
    void Unpin(AppId app);

protected:
    void OnOpen() override;
    void OnClose() override;

private:
    enum class View
    {
        Home,
        AllApps,
        Search,
    };
    enum class HitKind
    {
        None,
        Pinned,
        Recommended,
        AllAppsButton,
        MoreButton,
        BackButton,
        ListApp,
        ListLetter,
        Result,
        ResultAction,
        User,
        Power,
        PageDot,
    };
    struct Hit
    {
        HitKind kind = HitKind::None;
        int index = -1;
    };
    struct Result
    {
        enum class Type
        {
            App,
            Setting,
            File,
        } type = Type::App;
        AppId app = AppId::None;
        FsNode* node = nullptr;
        std::wstring name;
        std::wstring detail;
        std::wstring arg;
        AppIcon icon = AppIcon::None;
        Icon glyph = Icon::None;
    };
    struct ListRow
    {
        bool letter = false;
        std::wstring text;
        AppId app = AppId::None;
    };

    void SetView(View v);
    void UpdateSearch(const std::wstring& q);
    void OpenResult(int index);
    void Launch(AppId app);
    Hit HitAt(Point p) const;
    Rect PinnedCell(int index) const;
    Rect RecommendedCell(int index) const;
    Rect ButtonRect(HitKind kind) const;
    Rect ListRect() const;
    Rect ResultRect(int index) const;
    Rect ActionRect(int index) const;
    std::vector<FsNode*> RecommendedItems() const;
    void BuildList();
    int PageCount() const;
    void ShowItemMenu(const Hit& h, Point p);
    std::vector<std::wstring> ActionsFor(const Result& res) const;

    View m_view = View::Home;
    Tween m_viewAnim{1.0f};
    TextBox* m_search = nullptr;
    std::vector<AppId> m_pinned;
    int m_page = 0;
    Tween m_pageAnim{0.0f};
    std::vector<ListRow> m_rows;
    Tween m_listScroll;
    std::vector<Result> m_results;
    int m_selResult = 0;
    Hit m_hover;
    Hit m_press;
};
