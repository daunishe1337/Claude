// Упрощённые приложения-заставки (Почта, Магазин, Камера, Карты, Советы).

#include "../Shell.h"
#include "AppFactories.h"

namespace
{
class PlaceholderWindow : public Window
{
public:
    explicit PlaceholderWindow(AppId id) : Window(id, GetAppInfo(id).name)
    {
        SetInitialSize(960.0f, 640.0f);
        m_minSize = {420.0f, 320.0f};
    }

    void DrawContent(Renderer& r, const Rect& client) override
    {
        const Palette& p = Theme::P();
        const float cy = client.CenterY();
        r.DrawAppIcon(GetAppInfo(App()).icon, {client.CenterX() - 48.0f, cy - 96.0f, 96.0f, 96.0f});
        r.Text(Title(), {client.x, cy + 12.0f, client.w, 40.0f}, p.text, 28.0f, FontWeight::Semibold, TextFlags::Center);
        r.Text(L"Это приложение показано в демонстрационном режиме.", {client.x + 24.0f, cy + 58.0f, client.w - 48.0f, 24.0f},
               p.textSecondary, 14.0f, FontWeight::Regular, TextFlags::Center);
        m_ui.Draw(r);
    }
};
} // namespace

std::unique_ptr<Window> CreatePlaceholder(AppId id, const std::wstring& /*arg*/) { return std::make_unique<PlaceholderWindow>(id); }
