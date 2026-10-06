// UI.cpp
#include "UI.h"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

UI::~UI() {
    if (fontSmall_) DeleteObject(fontSmall_);
    if (fontNormal_) DeleteObject(fontNormal_);
    if (fontLarge_) DeleteObject(fontLarge_);
}

static HFONT MakeFont(int height, int weight) {
    return CreateFontW(-height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

bool UI::Init() {
    fontSmall_ = MakeFont(16, FW_NORMAL);
    fontNormal_ = MakeFont(20, FW_NORMAL);
    fontLarge_ = MakeFont(30, FW_BOLD);
    return fontSmall_ && fontNormal_ && fontLarge_;
}

HFONT UI::FontFor(FontSize size) const {
    switch (size) {
    case FontSize::Small: return fontSmall_;
    case FontSize::Large: return fontLarge_;
    default: return fontNormal_;
    }
}

void UI::DrawString(HDC dc, const std::wstring& text, RECT rc, COLORREF color, UINT format, FontSize size) {
    HGDIOBJ old = SelectObject(dc, FontFor(size));
    SetBkMode(dc, TRANSPARENT);

    RECT shadow = rc;
    OffsetRect(&shadow, 1, 1);
    SetTextColor(dc, RGB(0, 0, 0));
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &shadow, format);

    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rc, format);

    SelectObject(dc, old);
}

int UI::MeasureHeight(HDC dc, const std::wstring& text, int width, FontSize size) {
    HGDIOBJ old = SelectObject(dc, FontFor(size));
    RECT rc = { 0, 0, width, 0 };
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rc, DT_LEFT | DT_WORDBREAK | DT_CALCRECT);
    SelectObject(dc, old);
    return rc.bottom - rc.top;
}

void UI::FillBox(HDC dc, RECT rc, COLORREF fill, COLORREF border, int radius, int borderWidth) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, borderWidth, border);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}
