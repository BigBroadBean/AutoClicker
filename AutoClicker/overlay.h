#pragma once

#include <Windows.h>

void ShowToast(const wchar_t* title, const wchar_t* status, COLORREF statusColor);

inline void ShowToggleToast(const wchar_t* title, bool enabled) {
    ShowToast(title, enabled ? L"\x5f00" : L"\x5173",
              enabled ? RGB(76, 194, 110) : RGB(230, 72, 88));
}
