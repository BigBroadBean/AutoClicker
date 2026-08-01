#include "overlay.h"
#include <Windows.h>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>

constexpr int TOAST_W = 240;
constexpr int TOAST_H = 64;
constexpr int TOAST_MARGIN = 6;      // room for the soft shadow
constexpr int TOAST_STAY_MS = 1000;

static std::atomic<int> g_toastGen{ 0 };

static COLORREF LerpC(COLORREF a, COLORREF b, float t) {
    return RGB((int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t),
               (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t),
               (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t));
}

// 32bpp top-down DIB (transparent initially, alpha set per pixel after drawing)
static HBITMAP CreateAlphaSurface(int w, int h, void** bits)
{
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;   // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    return CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, bits, nullptr, 0);
}

static void ApplyAlpha(void* bits, int w, int h)
{
    unsigned char* p = (unsigned char*)bits;
    for (int i = 0; i < w * h; i++) {
        if (p[0] || p[1] || p[2]) p[3] = 255;
        p += 4;
    }
}

// Neumorphic toast body (theme-aware)
static void DrawToast(HDC memDC, const wchar_t* title, const wchar_t* status, COLORREF statusColor)
{
    int w = TOAST_W, h = TOAST_H;
    COLORREF bg = CARD();

    // soft shadow (bottom-right)
    for (int i = TOAST_MARGIN; i >= 1; --i) {
        RECT rr = { i, i, w + i, h + i };
        HBRUSH hb = CreateSolidBrush(LerpC(bg, SHADOW_DARK(), (float)i / (TOAST_MARGIN + 1)));
        HGDIOBJ ob = SelectObject(memDC, hb);
        SelectObject(memDC, GetStockObject(NULL_PEN));
        RoundRect(memDC, rr.left, rr.top, rr.right, rr.bottom, 14, 14);
        SelectObject(memDC, ob);
        DeleteObject(hb);
    }
    // soft highlight (top-left)
    for (int i = 2; i >= 1; --i) {
        RECT rr = { -i, -i, w - i, h - i };
        HBRUSH hb = CreateSolidBrush(LerpC(bg, SHADOW_LIGHT(), i / 3.0f));
        HGDIOBJ ob = SelectObject(memDC, hb);
        SelectObject(memDC, GetStockObject(NULL_PEN));
        RoundRect(memDC, rr.left, rr.top, rr.right, rr.bottom, 14, 14);
        SelectObject(memDC, ob);
        DeleteObject(hb);
    }
    // body
    {
        HBRUSH hb = CreateSolidBrush(bg);
        HGDIOBJ ob = SelectObject(memDC, hb);
        SelectObject(memDC, GetStockObject(NULL_PEN));
        RoundRect(memDC, 0, 0, w, h, 14, 14);
        SelectObject(memDC, ob);
        DeleteObject(hb);
    }
    // accent border
    {
        HPEN hp = CreatePen(PS_SOLID, 1, ACCENT());
        SelectObject(memDC, hp);
        SelectObject(memDC, GetStockObject(NULL_BRUSH));
        RoundRect(memDC, 1, 1, w - 1, h - 1, 13, 13);
        DeleteObject(hp);
    }

    SetBkMode(memDC, TRANSPARENT);

    HFONT hFont = CreateFontW(22, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0,
                              CLEARTYPE_QUALITY, 0, g_uiFontName);
    if (!hFont) hFont = CreateFontW(22, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0,
                                    CLEARTYPE_QUALITY, 0, L"Segoe UI");
    SelectObject(memDC, hFont);
    SetTextColor(memDC, TXT());
    RECT rt = { 0, 4, w, 28 };
    DrawTextW(memDC, title, -1, &rt, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(hFont);

    hFont = CreateFontW(26, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0,
                        CLEARTYPE_QUALITY, 0, g_uiFontName);
    if (!hFont) hFont = CreateFontW(26, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0,
                                    CLEARTYPE_QUALITY, 0, L"Segoe UI");
    SelectObject(memDC, hFont);
    SetTextColor(memDC, statusColor);
    RECT rs = { 0, 26, w, 60 };
    DrawTextW(memDC, status, -1, &rs, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(hFont);
}

static void DrawAndFade(HWND hToast, HDC screenDC, int x, int y,
                        const wchar_t* title, const wchar_t* status, COLORREF statusColor,
                        int gen)
{
    const int W = TOAST_W + TOAST_MARGIN * 2;
    const int H = TOAST_H + TOAST_MARGIN * 2;

    void* bits = nullptr;
    HBITMAP hbm = CreateAlphaSurface(W, H, &bits);
    HDC memDC = CreateCompatibleDC(nullptr);
    HGDIOBJ oldBM = SelectObject(memDC, hbm);

    auto Render = [&](BYTE alpha) {
        memset(bits, 0, (size_t)W * H * 4);
        DrawToast(memDC, title, status, statusColor);
        ApplyAlpha(bits, W, H);
        BLENDFUNCTION blend = { AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA };
        POINT ptDst = { x, y };
        SIZE sz = { W, H };
        POINT ptSrc = { 0, 0 };
        UpdateLayeredWindow(hToast, screenDC, &ptDst, &sz, memDC, &ptSrc, 0, &blend, ULW_ALPHA);
    };

    // fade in
    for (int step = 0; step <= 5; ++step) {
        if (g_toastGen != gen) break;
        Render((BYTE)(51 * step));
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    if (g_toastGen != gen) goto cleanup;
    std::this_thread::sleep_for(std::chrono::milliseconds(TOAST_STAY_MS));
    if (g_toastGen != gen) goto cleanup;
    // fade out
    for (int step = 20; step >= 0; --step) {
        if (g_toastGen != gen) break;
        Render((BYTE)(255 * step / 20));
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

cleanup:
    SelectObject(memDC, oldBM);
    DeleteObject(hbm);
    DeleteDC(memDC);
}

static void ToastThreadProc(const wchar_t* title, const wchar_t* status, COLORREF statusColor,
                            int gen)
{
    const int W = TOAST_W + TOAST_MARGIN * 2;
    const int H = TOAST_H + TOAST_MARGIN * 2;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = screenW - W - 24;
    int y = screenH - H - 80;

    HINSTANCE hInst = GetModuleHandle(nullptr);

    HWND hToast = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
        "Static", "", WS_POPUP,
        0, 0, W, H,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(hToast, SW_SHOW);
    UpdateWindow(hToast);

    HDC screenDC = GetDC(nullptr);
    DrawAndFade(hToast, screenDC, x, y, title, status, statusColor, gen);
    ReleaseDC(nullptr, screenDC);

    DestroyWindow(hToast);
}

void ShowToast(const wchar_t* title, const wchar_t* status, COLORREF statusColor)
{
    int gen = ++g_toastGen;
    std::thread(ToastThreadProc, title, status, statusColor, gen).detach();
}
