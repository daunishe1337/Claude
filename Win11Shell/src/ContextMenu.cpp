#include "ContextMenu.h"

namespace
{
constexpr float kPad = 4.0f;
constexpr float kItemH = 32.0f;
constexpr float kSepH = 9.0f;
constexpr float kTopH = 40.0f;
constexpr float kTopW = 44.0f;
constexpr float kClassicItemH = 24.0f;
constexpr float kClassicSepH = 7.0f;
} // namespace

float ContextMenu::ItemHeight(const MenuItem& m) const
{
    if (m.separator)
        return m_classic ? kClassicSepH : kSepH;
    return m_classic ? kClassicItemH : kItemH;
}

float ContextMenu::TopRowHeight(const Level& l) const { return l.top.empty() ? 0.0f : kTopH + kSepH; }

Rect ContextMenu::ItemRect(const Level& l, int index) const
{
    float y = l.rect.y + kPad + TopRowHeight(l);
    for (int i = 0; i < index; ++i)
        y += ItemHeight(l.items[static_cast<size_t>(i)]);
    return {l.rect.x, y, l.rect.w, ItemHeight(l.items[static_cast<size_t>(index)])};
}

Rect ContextMenu::TopRect(const Level& l, int index) const
{
    return {l.rect.x + kPad + static_cast<float>(index) * kTopW, l.rect.y + kPad, kTopW, kTopH};
}

void ContextMenu::Measure(Level& l, float minWidth)
{
    Renderer& r = Renderer::Get();
    const float fs = m_classic ? 12.0f : 14.0f;
    bool anyIcon = false, anySub = false;
    float textMax = 0.0f, scMax = 0.0f, h = kPad * 2.0f + TopRowHeight(l);
    for (const MenuItem& m : l.items)
    {
        h += ItemHeight(m);
        if (m.separator)
            continue;
        if (m.icon != Icon::None || m.checked || m.radio)
            anyIcon = true;
        if (!m.submenu.empty())
            anySub = true;
        textMax = std::max(textMax, r.TextWidth(m.text, fs));
        if (!m.shortcut.empty())
            scMax = std::max(scMax, r.TextWidth(m.shortcut, fs));
    }
    const float left = m_classic ? 30.0f : (anyIcon ? 44.0f : 14.0f);
    float w = left + textMax + (scMax > 0.0f ? 36.0f + scMax : 0.0f) + (anySub ? 30.0f : 0.0f) + 20.0f;
    w = std::max(w, minWidth);
    w = std::max(w, static_cast<float>(l.top.size()) * kTopW + kPad * 2.0f);
    w = std::max(w, m_classic ? 170.0f : 180.0f);
    l.rect.w = std::ceil(w);
    l.rect.h = h;
}

void ContextMenu::Place(Level& l, float x, float y, bool allowUp)
{
    if (x + l.rect.w > m_screenW - 4.0f)
        x = m_screenW - 4.0f - l.rect.w;
    x = std::max(4.0f, x);
    l.upward = false;
    if (y + l.rect.h > m_screenH - 4.0f)
    {
        if (allowUp && y - l.rect.h >= 4.0f)
        {
            y -= l.rect.h;
            l.upward = true;
        }
        else
            y = std::max(4.0f, m_screenH - 4.0f - l.rect.h);
    }
    l.rect.x = x;
    l.rect.y = y;
}

void ContextMenu::Open(Point screenPos, std::vector<MenuItem> items, std::vector<MenuAction> top, bool classic,
                       float minWidth)
{
    Close();
    m_classic = classic;
    Level l;
    l.items = std::move(items);
    l.top = std::move(top);
    Measure(l, minWidth);
    float x = screenPos.x;
    if (x + l.rect.w > m_screenW - 4.0f)
        x = screenPos.x - l.rect.w;
    Place(l, x, screenPos.y, true);
    l.anim.Start(0.0f, 1.0f, 0.18f, Ease::OutCubic);
    m_levels.push_back(std::move(l));
}

void ContextMenu::OpenBelow(const Rect& anchor, std::vector<MenuItem> items, float minWidth, bool above)
{
    Close();
    m_classic = false;
    Level l;
    l.items = std::move(items);
    Measure(l, minWidth);
    const float y = above ? anchor.y - l.rect.h - 4.0f : anchor.Bottom() + 4.0f;
    Place(l, anchor.x, y, false);
    l.upward = above;
    l.anim.Start(0.0f, 1.0f, 0.18f, Ease::OutCubic);
    m_levels.push_back(std::move(l));
}

void ContextMenu::Close()
{
    m_levels.clear();
    m_pendingLevel = -1;
    m_pendingItem = -1;
}

void ContextMenu::OpenSub(int levelIndex, int item)
{
    if (levelIndex < 0 || levelIndex >= static_cast<int>(m_levels.size()))
        return;
    m_levels.resize(static_cast<size_t>(levelIndex) + 1);
    const Level& parent = m_levels[static_cast<size_t>(levelIndex)];
    if (item < 0 || item >= static_cast<int>(parent.items.size()))
        return;
    const MenuItem& m = parent.items[static_cast<size_t>(item)];
    if (m.submenu.empty() || !m.enabled)
        return;
    Level l;
    l.items = m.submenu;
    l.parentItem = item;
    Measure(l, 0.0f);
    const Rect ir = ItemRect(parent, item);
    float x = parent.rect.Right() - 2.0f;
    if (x + l.rect.w > m_screenW - 4.0f)
        x = parent.rect.x - l.rect.w + 2.0f;
    l.rect.x = x;
    l.rect.y = ir.y - kPad;
    if (l.rect.y + l.rect.h > m_screenH - 4.0f)
        l.rect.y = std::max(4.0f, m_screenH - 4.0f - l.rect.h);
    l.anim.Start(0.0f, 1.0f, 0.14f, Ease::OutCubic);
    m_levels.push_back(std::move(l));
}

int ContextMenu::LevelAt(Point p) const
{
    for (int i = static_cast<int>(m_levels.size()) - 1; i >= 0; --i)
    {
        if (m_levels[static_cast<size_t>(i)].rect.Contains(p))
            return i;
    }
    return -1;
}

int ContextMenu::ItemAt(const Level& l, Point p) const
{
    for (int i = 0; i < static_cast<int>(l.items.size()); ++i)
    {
        if (l.items[static_cast<size_t>(i)].separator)
            continue;
        if (ItemRect(l, i).Contains(p))
            return i;
    }
    return -1;
}

int ContextMenu::TopAt(const Level& l, Point p) const
{
    for (int i = 0; i < static_cast<int>(l.top.size()); ++i)
    {
        if (TopRect(l, i).Contains(p))
            return i;
    }
    return -1;
}

bool ContextMenu::HitTest(Point p) const { return LevelAt(p) >= 0; }

const MenuAction* ContextMenu::HoveredAction(Rect& anchor) const
{
    if (m_levels.empty())
        return nullptr;
    const Level& l = m_levels.front();
    if (l.topHover < 0 || l.topHover >= static_cast<int>(l.top.size()))
        return nullptr;
    anchor = TopRect(l, l.topHover);
    return &l.top[static_cast<size_t>(l.topHover)];
}

void ContextMenu::OnMouseMove(Point p)
{
    if (m_levels.empty())
        return;
    const int li = LevelAt(p);
    if (li < 0)
    {
        Level& last = m_levels.back();
        last.hover = -1;
        last.topHover = -1;
        m_pendingLevel = -1;
        return;
    }
    Level& l = m_levels[static_cast<size_t>(li)];
    const int it = ItemAt(l, p);
    l.hover = it;
    l.topHover = TopAt(l, p);
    const bool hasChild = li + 1 < static_cast<int>(m_levels.size());
    if (it >= 0)
    {
        const MenuItem& m = l.items[static_cast<size_t>(it)];
        const bool childIsThis = hasChild && m_levels[static_cast<size_t>(li) + 1].parentItem == it;
        if (!m.submenu.empty() && m.enabled && !childIsThis)
        {
            if (m_pendingLevel != li || m_pendingItem != it)
            {
                m_pendingLevel = li;
                m_pendingItem = it;
                m_pendingTime = NowSeconds();
                RequestFrame();
            }
        }
        else if (m.submenu.empty())
        {
            m_pendingLevel = -1;
            if (hasChild)
                m_levels.resize(static_cast<size_t>(li) + 1);
        }
    }
}

void ContextMenu::OnMouseDown(Point /*p*/) {}

void ContextMenu::OnMouseUp(Point p)
{
    const int li = LevelAt(p);
    if (li < 0)
        return;
    Level& l = m_levels[static_cast<size_t>(li)];
    const int top = TopAt(l, p);
    if (top >= 0)
    {
        auto act = l.top[static_cast<size_t>(top)].action;
        Close();
        if (act)
            PostAction(act);
        return;
    }
    const int it = ItemAt(l, p);
    if (it >= 0)
        Activate(li, it);
}

void ContextMenu::Activate(int levelIndex, int item)
{
    const Level& l = m_levels[static_cast<size_t>(levelIndex)];
    const MenuItem& m = l.items[static_cast<size_t>(item)];
    if (!m.enabled || m.separator)
        return;
    if (!m.submenu.empty())
    {
        m_pendingLevel = -1;
        OpenSub(levelIndex, item);
        return;
    }
    auto act = m.action;
    Close();
    if (act)
        PostAction(act);
}

void ContextMenu::MoveHover(int dir)
{
    Level& l = m_levels.back();
    const int n = static_cast<int>(l.items.size());
    if (n == 0)
        return;
    int i = l.hover;
    for (int guard = 0; guard < n; ++guard)
    {
        i = (i < 0) ? (dir > 0 ? 0 : n - 1) : (i + dir + n) % n;
        const MenuItem& m = l.items[static_cast<size_t>(i)];
        if (!m.separator && m.enabled)
        {
            l.hover = i;
            return;
        }
    }
}

bool ContextMenu::OnKeyDown(UINT vk)
{
    if (m_levels.empty())
        return false;
    Level& l = m_levels.back();
    switch (vk)
    {
    case VK_DOWN:
        MoveHover(1);
        break;
    case VK_UP:
        MoveHover(-1);
        break;
    case VK_RIGHT:
        if (l.hover >= 0 && !l.items[static_cast<size_t>(l.hover)].submenu.empty())
        {
            OpenSub(static_cast<int>(m_levels.size()) - 1, l.hover);
            MoveHover(1);
        }
        break;
    case VK_LEFT:
        if (m_levels.size() > 1)
            m_levels.pop_back();
        break;
    case VK_RETURN:
    case VK_SPACE:
        if (l.hover >= 0)
        {
            const int li = static_cast<int>(m_levels.size()) - 1;
            const bool sub = !l.items[static_cast<size_t>(l.hover)].submenu.empty();
            Activate(li, l.hover);
            if (sub)
                MoveHover(1);
        }
        break;
    case VK_ESCAPE:
        if (m_levels.size() > 1)
            m_levels.pop_back();
        else
            Close();
        break;
    default:
        break;
    }
    return true;
}

void ContextMenu::Draw(Renderer& r)
{
    if (m_levels.empty())
        return;
    const Palette& p = Theme::P();
    if (m_pendingLevel >= 0)
    {
        if (NowSeconds() - m_pendingTime >= 0.3)
        {
            const int lvl = m_pendingLevel, item = m_pendingItem;
            m_pendingLevel = -1;
            OpenSub(lvl, item);
        }
        else
            RequestFrame();
    }
    const float fs = m_classic ? 12.0f : 14.0f;
    for (size_t li = 0; li < m_levels.size(); ++li)
    {
        Level& l = m_levels[li];
        const float t = l.anim.Value();
        const float dy = (1.0f - t) * (l.upward ? 8.0f : -8.0f);
        r.PushOpacity(Saturate(t * 1.3f));
        r.PushTransform(0.0f, dy);
        const Rect& rc = l.rect;
        const float rad = m_classic ? 0.0f : 8.0f;
        r.Shadow(rc, rad, m_classic ? 6.0f : 18.0f, m_classic ? 2.0f : 6.0f, p.dark ? 0.5f : 0.22f);
        if (m_classic)
        {
            r.FillRect(rc, p.dark ? Color::Hex(0x2B2B2B) : Color::Hex(0xF2F2F2));
            r.StrokeRect(rc, p.dark ? Color::Hex(0x5A5A5A) : Color::Hex(0xA0A0A0));
        }
        else
        {
            r.Acrylic(rc, rad, p.menuTint, p.menuAmount);
            r.StrokeRoundRect(rc, rad, p.flyoutStroke);
        }

        for (int i = 0; i < static_cast<int>(l.top.size()); ++i)
        {
            const Rect tr = TopRect(l, i);
            if (i == l.topHover)
                r.FillRoundRect(tr.Inset(2.0f, 2.0f, 2.0f, 2.0f), 4.0f, p.subtleHover);
            r.Glyph(l.top[static_cast<size_t>(i)].icon, {tr.CenterX() - 8.0f, tr.CenterY() - 8.0f, 16.0f, 16.0f}, p.text);
        }
        if (!l.top.empty())
            r.FillRect({rc.x, rc.y + kPad + kTopH + kSepH * 0.5f - 0.5f, rc.w, 1.0f}, p.divider);

        const int childParent = li + 1 < m_levels.size() ? m_levels[li + 1].parentItem : -1;
        bool anyIcon = false;
        for (const MenuItem& m : l.items)
            anyIcon = anyIcon || m.icon != Icon::None || m.checked || m.radio;
        const float left = m_classic ? 30.0f : (anyIcon ? 44.0f : 14.0f);

        for (int i = 0; i < static_cast<int>(l.items.size()); ++i)
        {
            const MenuItem& m = l.items[static_cast<size_t>(i)];
            const Rect ir = ItemRect(l, i);
            if (m.separator)
            {
                const float inset = m_classic ? 28.0f : 0.0f;
                r.FillRect({ir.x + inset, ir.CenterY() - 0.5f, ir.w - inset - (m_classic ? 2.0f : 0.0f), 1.0f},
                           m_classic ? (p.dark ? Color::Hex(0x5A5A5A) : Color::Hex(0xBDBDBD)) : p.divider);
                continue;
            }
            const bool hot = (i == l.hover || i == childParent) && m.enabled;
            if (m.pill)
            {
                r.FillRoundRect(ir.Inset(4.0f, 1.0f, 4.0f, 1.0f), 4.0f, hot ? p.subtlePressed : p.subtleHover);
                r.FillRoundRect({ir.x + 4.0f, ir.CenterY() - 8.0f, 3.0f, 16.0f}, 1.5f, p.accent);
            }
            else if (hot)
            {
                if (m_classic)
                    r.FillRect(ir.Inset(2.0f, 0.0f, 2.0f, 0.0f), p.dark ? Color::Hex(0x414141) : Color::Hex(0x91C9F7, 0x90));
                else
                    r.FillRoundRect(ir.Inset(4.0f, 1.0f, 4.0f, 1.0f), 4.0f, p.subtleHover);
            }
            const Color fg = m.enabled ? p.text : p.textDisabled;
            const float iconX = ir.x + (m_classic ? 8.0f : 14.0f);
            if (m.icon != Icon::None)
                r.Glyph(m.icon, {iconX, ir.CenterY() - 8.0f, 16.0f, 16.0f}, fg);
            else if (m.checked && m.radio)
                r.FillCircle(iconX + 8.0f, ir.CenterY(), 3.0f, fg);
            else if (m.checked)
                r.Glyph(Icon::Check, {iconX + 1.0f, ir.CenterY() - 7.0f, 14.0f, 14.0f}, fg);
            const float subW = m.submenu.empty() ? 0.0f : 26.0f;
            r.Text(m.text, {ir.x + left, ir.y, ir.w - left - 16.0f - subW, ir.h}, fg, fs, FontWeight::Regular,
                   TextFlags::VCenter);
            if (!m.shortcut.empty())
                r.Text(m.shortcut, {ir.x, ir.y, ir.w - 16.0f - subW, ir.h}, m.enabled ? p.textSecondary : p.textDisabled,
                       fs, FontWeight::Regular, TextFlags::VCenter | TextFlags::Right);
            if (!m.submenu.empty())
                r.Glyph(Icon::ChevronRight, {ir.Right() - 28.0f, ir.CenterY() - 6.0f, 12.0f, 12.0f}, fg);
        }
        r.PopTransform();
        r.PopOpacity();
    }
}
