#include "overlay.h"
#include <Windows.h>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>

constexpr int TOAST_WIDTH = 260;
constexpr int TOAST_HEIGHT = 60;
constexpr int TOAST_STAY_MS = 1000;
constexpr int FADE_STEPS = 15;
constexpr int FADE_INTERVAL_MS = 30;

static std::atomic<int> g_toastGen{ 0 };

static void DrawAndFade(HWND hToast, HDC screenDC, int screenW, int screenH,
                        int x, int y,
                        const wchar_t* title, const wchar_t* status, COLORREF statusColor,
                        int gen)
{
    for (int step = 0; step <= 5; ++step) {
        if (g_toastGen != gen) { DestroyWindow(hToast); return; }
        HDC memDC = CreateCompatibleDC(screenDC);
        HBITMAP memBM = CreateCompatibleBitmap(screenDC, TOAST_WIDTH, TOAST_HEIGHT);
        HGDIOBJ oldBM = SelectObject(memDC, memBM);

        RECT rc = { 0, 0, TOAST_WIDTH, TOAST_HEIGHT };
        HBRUSH hbr = CreateSolidBrush(RGB(44, 44, 46));
        FillRect(memDC, &rc, hbr);
        DeleteObject(hbr);

        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(96, 205, 255));
        SelectObject(memDC, hPen);
        SelectObject(memDC, GetStockObject(NULL_BRUSH));
        RoundRect(memDC, 1, 1, TOAST_WIDTH - 1, TOAST_HEIGHT - 1, 10, 10);
        DeleteObject(hPen);

        HFONT hFont = CreateFontW(24, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable Display");
        if (!hFont) hFont = CreateFontW(24, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        SelectObject(memDC, hFont);
        SetBkMode(memDC, TRANSPARENT);

        SIZE ts;
        SetTextColor(memDC, RGB(200, 200, 205));
        int titleLen = (int)wcslen(title);
        GetTextExtentPoint32W(memDC, title, titleLen, &ts);
        TextOutW(memDC, (TOAST_WIDTH - ts.cx) / 2, 5, title, titleLen);

        DeleteObject(hFont);
        hFont = CreateFontW(28, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable Display");
        if (!hFont) hFont = CreateFontW(28, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        SelectObject(memDC, hFont);
        SetTextColor(memDC, statusColor);
        int statusLen = (int)wcslen(status);
        GetTextExtentPoint32W(memDC, status, statusLen, &ts);
        TextOutW(memDC, (TOAST_WIDTH - ts.cx) / 2, 26, status, statusLen);
        DeleteObject(hFont);

        BYTE alpha = (BYTE)(51 * step);
        BLENDFUNCTION blend = { AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA };
        POINT ptDst = { x, y };
        SIZE sz = { TOAST_WIDTH, TOAST_HEIGHT };
        POINT ptSrc = { 0, 0 };
        UpdateLayeredWindow(hToast, screenDC, &ptDst, &sz, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

        SelectObject(memDC, oldBM);
        DeleteObject(memBM);
        DeleteDC(memDC);

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(TOAST_STAY_MS));
    if (g_toastGen != gen) { DestroyWindow(hToast); return; }

    for (int step = 20; step >= 0; --step) {
        if (g_toastGen != gen) { DestroyWindow(hToast); return; }
        HDC memDC = CreateCompatibleDC(screenDC);
        HBITMAP memBM = CreateCompatibleBitmap(screenDC, TOAST_WIDTH, TOAST_HEIGHT);
        HGDIOBJ oldBM = SelectObject(memDC, memBM);

        RECT rc = { 0, 0, TOAST_WIDTH, TOAST_HEIGHT };
        HBRUSH hbr = CreateSolidBrush(RGB(44, 44, 46));
        FillRect(memDC, &rc, hbr);
        DeleteObject(hbr);

        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(96, 205, 255));
        SelectObject(memDC, hPen);
        SelectObject(memDC, GetStockObject(NULL_BRUSH));
        RoundRect(memDC, 1, 1, TOAST_WIDTH - 1, TOAST_HEIGHT - 1, 10, 10);
        DeleteObject(hPen);

        SetBkMode(memDC, TRANSPARENT);

        {
            HFONT hFont = CreateFontW(24, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
            SelectObject(memDC, hFont);
            SetTextColor(memDC, RGB(200, 200, 205));
            SIZE ts;
            int titleLen = (int)wcslen(title);
            GetTextExtentPoint32W(memDC, title, titleLen, &ts);
            TextOutW(memDC, (TOAST_WIDTH - ts.cx) / 2, 5, title, titleLen);
            DeleteObject(hFont);
        }

        {
            HFONT hFont = CreateFontW(28, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
            SelectObject(memDC, hFont);
            SetTextColor(memDC, statusColor);
            SIZE ts;
            int statusLen = (int)wcslen(status);
            GetTextExtentPoint32W(memDC, status, statusLen, &ts);
            TextOutW(memDC, (TOAST_WIDTH - ts.cx) / 2, 26, status, statusLen);
            DeleteObject(hFont);
        }

        BYTE alpha = (BYTE)(255 * step / 20);
        BLENDFUNCTION blend = { AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA };
        POINT ptDst = { x, y };
        SIZE sz = { TOAST_WIDTH, TOAST_HEIGHT };
        POINT ptSrc2 = { 0, 0 };
        UpdateLayeredWindow(hToast, screenDC, &ptDst, &sz, memDC, &ptSrc2, 0, &blend, ULW_ALPHA);

        SelectObject(memDC, oldBM);
        DeleteObject(memBM);
        DeleteDC(memDC);

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

static std::atomic<bool> g_toastActive{ false };

static void ToastThreadProc(const wchar_t* title, const wchar_t* status, COLORREF statusColor,
                             int gen)
{
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = screenW - TOAST_WIDTH - 24;
    int y = screenH - TOAST_HEIGHT - 80;

    HINSTANCE hInst = GetModuleHandle(nullptr);

    HWND hToast = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
        L"Static", L"", WS_POPUP,
        0, 0, TOAST_WIDTH, TOAST_HEIGHT,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(hToast, SW_SHOW);
    UpdateWindow(hToast);

    HDC screenDC = GetDC(nullptr);
    DrawAndFade(hToast, screenDC, screenW, screenH, x, y, title, status, statusColor, gen);
    ReleaseDC(nullptr, screenDC);

    DestroyWindow(hToast);
    g_toastActive = false;
}

void ShowToast(const wchar_t* title, const wchar_t* status, COLORREF statusColor)
{
    int gen = ++g_toastGen;
    std::thread(ToastThreadProc, title, status, statusColor, gen).detach();
}
