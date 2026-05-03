#include "ui.h"
#include "types.h"
#include <dwmapi.h>

void ApplyWin11Style(HWND hwnd)
{
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, (DWMWINDOWATTRIBUTE)DWMA_DARK, &dark, sizeof(dark));
    int val = CORNER_ROUND;
    DwmSetWindowAttribute(hwnd, (DWMWINDOWATTRIBUTE)DWMA_CORNER, &val, sizeof(val));
}