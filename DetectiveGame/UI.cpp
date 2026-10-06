// UI.cpp
#include "UI.h"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

UI::~UI() {
    if (font_) DeleteObject(font_);
}

bool UI::Init() {
    font_ = CreateFontW(-20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    return font_ != nullptr;
}

void UI::DrawString(HDC dc, const std::wstring& text, RECT rc, COLORREF color, UINT format) {
    HGDIOBJ old = SelectObject(dc, font_);
    SetBkMode(dc, TRANSPARENT);

    RECT shadow = rc;
    OffsetRect(&shadow, 1, 1);
    SetTextColor(dc, RGB(0, 0, 0));
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &shadow, format);

    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rc, format);

    SelectObject(dc, old);
}
