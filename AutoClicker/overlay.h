#pragma once

#include <Windows.h>

void ShowToast(const wchar_t* title, const wchar_t* status, COLORREF statusColor);

inline void ShowToggleToast(const wchar_t* title, bool enabled) {
    ShowToast(title, enabled ? L"\x5f00" : L"\x5173",
              enabled ? RGB(76, 194, 110) : RGB(230, 72, 88));
}

inline void ShowScrollLRToast(int button) {
    ShowToast(L"\x6eda\x8f6e\x70b9\x51fb",
              button == 0 ? L"\x5de6\x952e" : L"\x53f3\x952e",
              RGB(96, 205, 255));
}
