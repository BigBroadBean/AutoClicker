#include "types.h"
#include "ui.h"
#include "clicker.h"
#include "config.h"
#include "overlay.h"
#include "sound.h"

#include <Windows.h>
#include <dwmapi.h>
#include <thread>
#include <cmath>
#include <cstdio>
#include <string>

#pragma comment(lib, "Dwmapi.lib")

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

static HDC      g_hdcMem = nullptr;
static HBITMAP  g_hbmBuf = nullptr;
static HFONT    g_hfTitle = nullptr;
static HFONT    g_hfLabel = nullptr;
static HFONT    g_hfBody  = nullptr;
static HFONT    g_hfCPS   = nullptr;
static int      g_cx = WIN_W, g_cy = WIN_H;

// ---- interactive elements ----
enum Elem {
    E_NONE = -1,
    E_SL_L, E_SL_R, E_SL_MAX, E_SL_RAND,
    E_TGL_L, E_TGL_R,
    E_BTN_KEY, E_BTN_KEEP,
    E_MODE_L, E_MODE_R,
    E_CHK_RAND, E_INP_MAX,
    E_BTN_SCROLL_L, E_BTN_SCROLL_R, E_BTN_SCROLL_KEY, E_BTN_SCROLL_LR_KEY, E_TGL_SCROLL,
    E_BTN_THEME,
    E_COUNT
};
struct HR { RECT r; Elem id; bool hover; };
static HR   g_hr[E_COUNT] = {};
static Elem g_drag = E_NONE;
static int  g_dx   = 0;
static bool g_inputOn = false;
static wchar_t g_inputBuf[8] = {};

// ---- layout rects ----
struct LY {
    RECT title, card[5], track[4], thumb[4];
    RECT tgl[3], btnKey, btnKeep, status;
    RECT modeBtn, inpMax, chkRand, btnScrollLR, btnScrollKey, btnScrollLRKey;
    RECT btnTheme;
} L;

static bool PtIn(const RECT& r, int x, int y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

static void Layout()
{
    int W = g_cx, H = g_cy;

    // ---- title area ----
    L.title = { 0, 6, W, 46 };

    // ---- theme toggle button ----
    L.btnTheme = { W - 180, 12, W - 154, 40 };

    // ---- mode toggle pill ----
    L.modeBtn = { W - 142, 12, W - 12, 40 };

    // ---- 5 cards (0-4) ----
    if (multimode) {
        L.card[0] = { 16,  60, W - 16, 168 };  // 倍率
        L.card[1] = { 16, 182, W - 16, 290 };  // 延迟
        L.card[2] = { 16, 304, W - 16, 412 };  // 快捷键
        L.card[3] = { 0, 0, 0, 0 };
        L.card[4] = { 0, 0, 0, 0 };
    } else {
        L.card[0] = { 16,  60, W - 16, 166 };  // 左键
        L.card[1] = { 16, 180, W - 16, 286 };  // 右键
        L.card[2] = { 16, 300, W - 16, 434 };  // 滚轮点击
        L.card[3] = { 16, 448, W - 16, 554 };  // 快捷键 & 模式
        L.card[4] = { 16, 568, W - 16, 704 };  // CPS上限 & 随机CPS
    }

    // ---- sliders ----
    auto Slider = [](const RECT& c, int yOff) -> RECT {
        return { c.left + 16, c.top + yOff, c.right - 90, c.top + yOff + 5 };
    };
    L.track[0] = Slider(L.card[0], 36);
    L.track[1] = Slider(L.card[1], 36);
    L.track[2] = Slider(L.card[4], 36);
    L.track[3] = { L.card[4].left + 16, L.card[4].top + 96,
                   L.card[4].right - 90, L.card[4].top + 101 };

    // ---- toggles ----
    for (int i = 0; i < 3; i++) {
        RECT& c = L.card[i];
        L.tgl[i] = { c.right - 58, c.top + 16, c.right - 16, c.top + 36 };
    }

    // ---- hotkey button ----
    {
        int ci = multimode ? 2 : 3;
        RECT& c = L.card[ci];
        L.btnKey = { c.left + 16, c.top + 26, c.left + 250, c.top + 50 };
    }

    // ---- keep mode button ----
    {
        RECT& c = L.card[3];
        L.btnKeep = { c.left + 16, c.top + 64, c.left + 230, c.top + 88 };
    }

    // ---- scroll wheel key button ----
    L.btnScrollKey = { L.card[2].left + 16, L.card[2].top + 26,
                       L.card[2].left + 170, L.card[2].top + 50 };

    // ---- scroll left/right selector ----
    L.btnScrollLR = { L.card[2].left + 182, L.card[2].top + 26,
                      L.card[2].right - 70, L.card[2].top + 50 };

    // ---- scroll L/R toggle hotkey button ----
    L.btnScrollLRKey = { L.card[2].left + 182, L.card[2].top + 60,
                         L.card[2].right - 70, L.card[2].top + 84 };

    // ---- scroll toggle ----
    L.tgl[2] = { L.card[2].right - 58, L.card[2].top + 16,
                 L.card[2].right - 16, L.card[2].top + 36 };

    // ---- CPS max input ----
    L.inpMax = { L.track[2].right + 6, L.track[2].top - 6,
                 L.card[4].right - 16, L.track[2].bottom + 6 };

    // ---- random CPS checkbox ----
    L.chkRand = { L.card[4].left + 16, L.card[4].top + 60,
                  L.card[4].left + 170, L.card[4].top + 82 };

    // ---- status bar ----
    L.status = { 16, H - 40, W - 16, H - 12 };

    // ---- thumb rects ----
    for (int i = 0; i < 4; i++) {
        L.thumb[i].top    = L.track[i].top - 7;
        L.thumb[i].bottom = L.track[i].bottom + 7;
    }

    // ---- populate g_hr hit-test array ----
    g_hr[E_SL_L]    = { L.thumb[0], E_SL_L, false };
    g_hr[E_SL_R]    = { L.thumb[1], E_SL_R, false };
    g_hr[E_SL_MAX]  = { L.thumb[2], E_SL_MAX, false };
    g_hr[E_SL_RAND] = { L.thumb[3], E_SL_RAND, false };
    g_hr[E_TGL_L]   = { L.tgl[0],   E_TGL_L, false };
    g_hr[E_TGL_R]   = { L.tgl[1],   E_TGL_R, false };
    g_hr[E_TGL_SCROLL] = { L.tgl[2], E_TGL_SCROLL, false };
    g_hr[E_BTN_KEY] = { L.btnKey,   E_BTN_KEY, false };
    g_hr[E_BTN_KEEP] = { L.btnKeep, E_BTN_KEEP, false };
    g_hr[E_MODE_L] = { { L.modeBtn.left, L.modeBtn.top, (L.modeBtn.left + L.modeBtn.right) / 2, L.modeBtn.bottom }, E_MODE_L, false };
    g_hr[E_MODE_R] = { { (L.modeBtn.left + L.modeBtn.right) / 2, L.modeBtn.top, L.modeBtn.right, L.modeBtn.bottom }, E_MODE_R, false };
    g_hr[E_INP_MAX] = { L.inpMax, E_INP_MAX, false };
    g_hr[E_CHK_RAND] = { L.chkRand, E_CHK_RAND, false };
    {
        RECT& b = L.btnScrollLR;
        int midX = (b.left + b.right) / 2;
        g_hr[E_BTN_SCROLL_L] = { { b.left, b.top, midX, b.bottom }, E_BTN_SCROLL_L, false };
        g_hr[E_BTN_SCROLL_R] = { { midX, b.top, b.right, b.bottom }, E_BTN_SCROLL_R, false };
    }
    g_hr[E_BTN_SCROLL_KEY] = { L.btnScrollKey, E_BTN_SCROLL_KEY, false };
    g_hr[E_BTN_SCROLL_LR_KEY] = { L.btnScrollLRKey, E_BTN_SCROLL_LR_KEY, false };
    g_hr[E_BTN_THEME] = { L.btnTheme, E_BTN_THEME, false };
}

// ---- GDI init ----
static void InitGDI()
{
    auto F = [](int h, int w, int q, const wchar_t* n) -> HFONT {
        HFONT f = CreateFontW(h, 0, 0, 0, w, 0, 0, 0, 0, 0, 0, q, 0, L"Microsoft YaHei UI");
        if (!f) f = CreateFontW(h, 0, 0, 0, w, 0, 0, 0, 0, 0, 0, q, 0, L"Segoe UI");
        return f;
    };
    g_hfTitle = F(24, FW_BOLD,       CLEARTYPE_QUALITY, L"Microsoft YaHei UI");
    g_hfLabel = F(18, FW_SEMIBOLD,   CLEARTYPE_QUALITY, L"Microsoft YaHei UI");
    g_hfBody  = F(18, FW_NORMAL,     CLEARTYPE_QUALITY, L"Microsoft YaHei UI");
    g_hfCPS   = F(18, FW_SEMIBOLD,   CLEARTYPE_QUALITY, L"Microsoft YaHei UI");
}

static void MakeBuf(HWND hwnd)
{
    RECT rc; GetClientRect(hwnd, &rc);
    g_cx = rc.right; g_cy = rc.bottom;
    HDC dc = GetDC(hwnd);
    g_hdcMem = CreateCompatibleDC(dc);
    g_hbmBuf = CreateCompatibleBitmap(dc, g_cx, g_cy);
    SelectObject(g_hdcMem, g_hbmBuf);
    ReleaseDC(hwnd, dc);
}
static void FreeBuf() { if (g_hbmBuf) { DeleteObject(g_hbmBuf); g_hbmBuf = nullptr; } if (g_hdcMem) { DeleteDC(g_hdcMem); g_hdcMem = nullptr; } }

// ---- slider helpers ----
static int ThumbX(int i) {
    if (multimode) {
        if (i == 0) {
            float r = (float)(multiMul - 1) / 4.0f;
            return L.track[i].left + (int)(r * (L.track[i].right - L.track[i].left));
        } else {
            float r = (float)(multiDelayMs - 1) / 199.0f;
            return L.track[i].left + (int)(r * (L.track[i].right - L.track[i].left));
        }
    }
    if (i == 2) {
        float r = (float)(cpsMax - CPS_LIMIT_MIN) / (CPS_LIMIT_MAX - CPS_LIMIT_MIN);
        return L.track[i].left + (int)(r * (L.track[i].right - L.track[i].left));
    }
    if (i == 3) {
        float r = (float)(randomCpsRange - 1) / 4.0f;
        return L.track[i].left + (int)(r * (L.track[i].right - L.track[i].left));
    }
    int maxVal = cpsMax * 10;
    int c = (i == 0) ? cpsLeft10 : cpsRight10;
    float r = (float)(c - CPS_MIN10) / (maxVal - CPS_MIN10);
    return L.track[i].left + (int)(r * (L.track[i].right - L.track[i].left));
}
static void UpThumbs() { for (int i = 0; i < 4; i++) { int cx = ThumbX(i); L.thumb[i].left = cx - 8; L.thumb[i].right = cx + 8; g_hr[i].r = L.thumb[i]; } }

// ---- helpers for drawing rounded rects ----
static void FillRoundRect(HDC dc, const RECT& r, int radius, COLORREF fill) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = (HPEN)GetStockObject(NULL_PEN);
    SelectObject(dc, p); SelectObject(dc, b);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    DeleteObject(b);
}
static void DrawRoundRect(HDC dc, const RECT& r, int radius, COLORREF border, int width) {
    HPEN p = CreatePen(PS_SOLID, width, border);
    SelectObject(dc, p); SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    DeleteObject(p);
}

// ---- render ----
static void Paint()
{
    HDC dc = g_hdcMem;
    SetBkMode(dc, TRANSPARENT);

    // ---- background ----
    { HBRUSH b = CreateSolidBrush(BG()); RECT a = { 0, 0, g_cx, g_cy }; FillRect(dc, &a, b); DeleteObject(b); }

    // ---- cards: subtle fill + 1px border ----
    int cardCount = multimode ? 3 : 5;
    for (int i = 0; i < cardCount; i++) {
        FillRoundRect(dc, L.card[i], 10, CARD());
        DrawRoundRect(dc, L.card[i], 10, BORDER(), 1);
    }

    // ---- title ----
    SelectObject(dc, g_hfTitle);
    SetTextColor(dc, TXT());
    RECT tr = L.title;
    tr.left += 8; tr.top += 2; tr.right = L.btnTheme.left - 12;
    DrawTextW(dc, L"AutoClicker", -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // ---- theme toggle button (sun/moon) ----
    {
        RECT& b = L.btnTheme;
        bool hover = g_hr[E_BTN_THEME].hover;
        FillRoundRect(dc, b, 15, hover ? BTN_HOVER() : BTN());
        DrawRoundRect(dc, b, 15, BORDER(), 1);
        SelectObject(dc, g_hfBody);
        SetTextColor(dc, TXT());
        DrawTextW(dc, g_theme == Theme::Dark ? L"\x2600" : L"\x263E",
                  -1, (RECT*)&b, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // ---- mode toggle pill ----
    {
        RECT& b = L.modeBtn;
        int midX = (b.left + b.right) / 2;
        bool hover = g_hr[E_MODE_L].hover || g_hr[E_MODE_R].hover;
        COLORREF bg = hover ? BTN_HOVER() : BTN();

        FillRoundRect(dc, b, 16, bg);
        DrawRoundRect(dc, b, 16, BORDER(), 1);

        // active half
        SaveDC(dc);
        IntersectClipRect(dc, multimode ? midX : b.left, b.top,
                          multimode ? b.right : midX, b.bottom);
        FillRoundRect(dc, b, 16, ACCENT());
        RestoreDC(dc, -1);

        SelectObject(dc, g_hfLabel);
        RECT rl = { b.left, b.top, midX, b.bottom };
        RECT rr = { midX, b.top, b.right, b.bottom };
        SetTextColor(dc, multimode ? TXT_DIM() : RGB(255, 255, 255));
        DrawTextW(dc, L"\x8fde\x70b9", -1, &rl, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(dc, multimode ? RGB(255, 255, 255) : TXT_DIM());
        DrawTextW(dc, L"\x591a\x500d", -1, &rr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // ---- card labels ----
    SelectObject(dc, g_hfLabel);
    SetTextColor(dc, TXT());
    const wchar_t* namesNormal[5] = {
        L"\x5de6\x952e", L"\x53f3\x952e",
        L"\x6eda\x8f6e\x70b9\x51fb",
        L"\x5feb\x6377\x952e \x4e0e \x6a21\x5f0f",
        L"CPS \x4e0a\x9650"
    };
    const wchar_t* namesMulti[3] = {
        L"\x500d\x7387", L"\x5ef6\x8fdf",
        L"\x5feb\x6377\x952e"
    };
    const wchar_t** names = multimode ? namesMulti : namesNormal;
    for (int i = 0; i < cardCount; i++) {
        RECT r = { L.card[i].left + 20, L.card[i].top + 12,
                   L.card[i].right, L.card[i].top + 30 };
        DrawTextW(dc, names[i], -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // ---- sliders ----
    int sliderCount = multimode ? 2 : (randomCpsEnabled ? 4 : 3);
    for (int i = 0; i < sliderCount; i++) {
        RECT& trk = L.track[i];

        // track background (full width, subtle color)
        FillRoundRect(dc, trk, 3, TRACK());

        // filled portion (accent)
        int fx = ThumbX(i);
        if (fx > trk.left + 1) {
            RECT r = { trk.left, trk.top, fx, trk.bottom };
            FillRoundRect(dc, r, 3, ACCENT());
        }

        // thumb
        int cx = ThumbX(i), cy = (trk.top + trk.bottom) / 2, r = 8;
        bool thumbHover = g_hr[i].hover || g_drag == g_hr[i].id;
        HPEN pt = CreatePen(PS_SOLID, 2, ACCENT());
        HBRUSH bt = CreateSolidBrush(thumbHover ? ACCENT() : BTN());
        SelectObject(dc, pt); SelectObject(dc, bt);
        Ellipse(dc, cx - r, cy - r, cx + r, cy + r);
        DeleteObject(pt); DeleteObject(bt);
    }

    // ---- toggles: minimal pill style ----
    auto DrawToggle = [&](const RECT& tg, bool on, Elem elem) {
        COLORREF fill = on ? GREEN() : TRACK();
        FillRoundRect(dc, tg, 10, fill);
        // knob
        int kw = tg.right - tg.left;
        int cy = (tg.top + tg.bottom) / 2, kr = (tg.bottom - tg.top) / 2 - 3;
        int kx = on ? tg.right - kr - 3 : tg.left + kr + 3;
        HBRUSH bk = CreateSolidBrush(RGB(255, 255, 255));
        SelectObject(dc, GetStockObject(NULL_PEN)); SelectObject(dc, bk);
        Ellipse(dc, kx - kr, cy - kr, kx + kr, cy + kr);
        DeleteObject(bk);
    };

    if (!multimode) {
        DrawToggle(L.tgl[0], leftenabled, E_TGL_L);
        DrawToggle(L.tgl[1], rightenabled, E_TGL_R);
    }

    // ---- hotkey button ----
    {
        RECT& b = L.btnKey;
        FillRoundRect(dc, b, 8, g_hr[E_BTN_KEY].hover ? BTN_HOVER() : BTN());
        DrawRoundRect(dc, b, 8, BORDER(), 1);
        SelectObject(dc, g_hfBody);
        SetTextColor(dc, TXT());
        std::wstring t = L"\x5feb\x6377\x952e: " + getKeyName(multimode ? vk_multi_key : vk_key);
        DrawTextW(dc, t.c_str(), -1, (RECT*)&b, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // ---- keep mode button (normal mode only) ----
    if (!multimode) {
        RECT& b = L.btnKeep;
        bool hover = g_hr[E_BTN_KEEP].hover;
        FillRoundRect(dc, b, 8, hover ? BTN_HOVER() : BTN());
        DrawRoundRect(dc, b, 8, keepClicke ? ACCENT() : BORDER(), keepClicke ? 2 : 1);
        SelectObject(dc, g_hfBody);
        SetTextColor(dc, keepClicke ? ACCENT() : TXT());
        DrawTextW(dc, keepClicke ? L"\x4e0d\x9700\x8981\x6309\x4f4f\x8fde\x70b9: \x5f00"
                                 : L"\x4e0d\x9700\x8981\x6309\x4f4f\x8fde\x70b9: \x5173",
                  -1, (RECT*)&b, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // ---- value displays ----
    SelectObject(dc, g_hfCPS);
    if (multimode) {
        wchar_t buf[32];
        swprintf(buf, 32, L"%d \x500d", multiMul);
        RECT r0 = { L.card[0].left + 20, L.card[0].top + 64,
                    L.track[0].right, L.card[0].top + 90 };
        SetTextColor(dc, TXT());
        DrawTextW(dc, buf, -1, &r0, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        swprintf(buf, 32, L"%d \x6beb\x79d2", multiDelayMs);
        RECT r1 = { L.card[1].left + 20, L.card[1].top + 64,
                    L.track[1].right, L.card[1].top + 90 };
        DrawTextW(dc, buf, -1, &r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {
        for (int i = 0; i < 2; i++) {
            int c10 = (i == 0) ? cpsLeft10 : cpsRight10;
            int ms  = (i == 0) ? leftms : rightms;
            wchar_t buf[32];
            swprintf(buf, 32, L"%.1f \x6b21/\x79d2", c10 / 10.0f);
            RECT r = { L.card[i].left + 20, L.card[i].top + 64,
                       L.track[i].right, L.card[i].top + 90 };
            SetTextColor(dc, TXT());
            DrawTextW(dc, buf, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            swprintf(buf, 32, L"%d \x6beb\x79d2", ms);
            SetTextColor(dc, TXT_DIM());
            RECT r2 = { r.right + 8, r.top, L.card[i].right - 8, r.bottom };
            DrawTextW(dc, buf, -1, &r2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        // ---- scroll wheel click controls ----
        // scroll hotkey button
        {
            RECT& b = L.btnScrollKey;
            FillRoundRect(dc, b, 8, g_hr[E_BTN_SCROLL_KEY].hover ? BTN_HOVER() : BTN());
            DrawRoundRect(dc, b, 8, BORDER(), 1);
            SelectObject(dc, g_hfBody);
            SetTextColor(dc, TXT());
            std::wstring t = L"\x5feb\x6377\x952e: " + getKeyName(vk_scroll_key);
            DrawTextW(dc, t.c_str(), -1, (RECT*)&b, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // scroll left/right segmented selector
        {
            RECT& b = L.btnScrollLR;
            int midX = (b.left + b.right) / 2;
            bool lHover = g_hr[E_BTN_SCROLL_L].hover;
            bool rHover = g_hr[E_BTN_SCROLL_R].hover;
            COLORREF bg = (lHover || rHover) ? BTN_HOVER() : BTN();
            FillRoundRect(dc, b, 8, bg);
            DrawRoundRect(dc, b, 8, BORDER(), 1);

            SaveDC(dc);
            IntersectClipRect(dc,
                scrollClickButton == 0 ? b.left : midX, b.top,
                scrollClickButton == 0 ? midX : b.right, b.bottom);
            FillRoundRect(dc, b, 8, ACCENT());
            RestoreDC(dc, -1);

            SelectObject(dc, g_hfBody);
            RECT rl = { b.left, b.top, midX, b.bottom };
            RECT rr = { midX, b.top, b.right, b.bottom };
            SetTextColor(dc, scrollClickButton == 0 ? RGB(255, 255, 255) : TXT_DIM());
            DrawTextW(dc, L"\x5de6\x952e", -1, &rl, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SetTextColor(dc, scrollClickButton == 1 ? RGB(255, 255, 255) : TXT_DIM());
            DrawTextW(dc, L"\x53f3\x952e", -1, &rr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // scroll L/R toggle hotkey button
        {
            RECT& b = L.btnScrollLRKey;
            FillRoundRect(dc, b, 8, g_hr[E_BTN_SCROLL_LR_KEY].hover ? BTN_HOVER() : BTN());
            DrawRoundRect(dc, b, 8, BORDER(), 1);
            SelectObject(dc, g_hfBody);
            SetTextColor(dc, TXT());
            std::wstring t = L"\x5207\x6362 L/R: " + getKeyName(vk_scroll_lr_key);
            DrawTextW(dc, t.c_str(), -1, (RECT*)&b, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // scroll toggle
        DrawToggle(L.tgl[2], isScrollClickActive, E_TGL_SCROLL);

        // ---- CPS max input box ----
        {
            RECT& bi = L.inpMax;
            COLORREF inpBorder = g_hr[E_INP_MAX].hover || g_inputOn ? ACCENT() : BORDER();
            FillRoundRect(dc, bi, 6, BTN());
            DrawRoundRect(dc, bi, 6, inpBorder, 1);
            SetTextColor(dc, TXT());
            SelectObject(dc, g_hfBody);
            if (g_inputOn) {
                DrawTextW(dc, g_inputBuf, -1, &bi, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } else {
                wchar_t ibuf[8];
                swprintf(ibuf, 8, L"%d", cpsMax);
                DrawTextW(dc, ibuf, -1, &bi, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }

        // ---- random CPS checkbox ----
        {
            RECT& cb = L.chkRand;
            int box = cb.left;
            int by = (cb.top + cb.bottom) / 2;
            FillRoundRect(dc, { box, by - 8, box + 16, by + 8 }, 4,
                          randomCpsEnabled ? ACCENT() : CARD());
            DrawRoundRect(dc, { box, by - 8, box + 16, by + 8 }, 4,
                          randomCpsEnabled ? ACCENT() : BORDER(), 1);
            if (randomCpsEnabled) {
                // check mark
                HPEN wp = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
                SelectObject(dc, wp);
                MoveToEx(dc, box + 3, by, nullptr);
                LineTo(dc, box + 7, by + 4);
                LineTo(dc, box + 13, by - 3);
                DeleteObject(wp);
            }
            RECT txt = { box + 22, cb.top, cb.right, cb.bottom };
            SetTextColor(dc, TXT());
            SelectObject(dc, g_hfBody);
            DrawTextW(dc, L"\x968f\x673a CPS", -1, &txt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        // ---- random CPS range slider ----
        if (randomCpsEnabled) {
            wchar_t bufR[32];
            swprintf(bufR, 32, L"\xb1%d CPS", randomCpsRange);
            RECT rRand = { L.card[4].left + 20, L.card[4].top + 112,
                           L.card[4].right - 20, L.card[4].top + 136 };
            SetTextColor(dc, TXT());
            SelectObject(dc, g_hfLabel);
            DrawTextW(dc, bufR, -1, &rRand, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            wchar_t bufHint[64];
            swprintf(bufHint, 64, L"\x968f\x673a\x8303\x56f4: \xb1%d CPS", randomCpsRange);
            RECT rHint = { L.card[4].left + 20, L.card[4].top + 70,
                           L.card[4].right - 20, L.card[4].top + 90 };
            SetTextColor(dc, TXT_DIM());
            SelectObject(dc, g_hfBody);
            DrawTextW(dc, bufHint, -1, &rHint, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }

    // ---- status bar ----
    {
        int baseY = L.status.top;
        int w = (L.status.right - L.status.left) / 3;

        auto Dot = [&](int x, int y, COLORREF c) {
            HBRUSH b = CreateSolidBrush(c);
            SelectObject(dc, GetStockObject(NULL_PEN)); SelectObject(dc, b);
            Ellipse(dc, x, y, x + 8, y + 8);
            DeleteObject(b);
        };

        int x0 = L.status.left;
        int x1 = L.status.left + w;
        int x2 = L.status.left + w * 2;

        // clicker status
        {
            COLORREF clr = isstart ? GREEN() : RED();
            Dot(x0, baseY, clr);
            SetTextColor(dc, clr);
            SelectObject(dc, g_hfBody);
            RECT r = { x0 + 12, L.status.top, x1 - 4, L.status.bottom };
            DrawTextW(dc, isstart ? L"\x8fde\x70b9 \x5f00" : L"\x8fde\x70b9 \x5173",
                      -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        // multi-click status
        {
            COLORREF clr = isMultiActive ? ACCENT() : RED();
            Dot(x1, baseY, clr);
            SetTextColor(dc, clr);
            SelectObject(dc, g_hfBody);
            RECT r = { x1 + 12, L.status.top, x2 - 4, L.status.bottom };
            DrawTextW(dc, isMultiActive ? L"\x591a\x500d \x5f00" : L"\x591a\x500d \x5173",
                      -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        // scroll-click status
        {
            COLORREF clr = isScrollClickActive ? ACCENT() : RED();
            Dot(x2, baseY, clr);
            SetTextColor(dc, clr);
            SelectObject(dc, g_hfBody);
            RECT r = { x2 + 12, L.status.top, L.status.right - 4, L.status.bottom };
            DrawTextW(dc, isScrollClickActive ? L"\x6eda\x8f6e\x70b9 \x5f00"
                                               : L"\x6eda\x8f6e\x70b9 \x5173",
                      -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }
}

static void Redraw(HWND hwnd) { UpThumbs(); Layout(); Paint(); InvalidateRect(hwnd, nullptr, FALSE); UpdateWindow(hwnd); }

// ---- hit test ----
static Elem Hit(POINT pt) { for (auto& h : g_hr) if (PtIn(h.r, pt.x, pt.y)) return h.id; return E_NONE; }

static void Hover(POINT pt)
{
    bool ch = false;
    for (auto& h : g_hr) {
        bool hv = PtIn(h.r, pt.x, pt.y);
        if (h.hover != hv) { h.hover = hv; ch = true; }
    }
    if (ch) { Paint(); InvalidateRect(nullptr, nullptr, FALSE); }
}

// ---- actions ----
static void Drag(int i, int mx)
{
    RECT& tr = L.track[i]; int tw = tr.right - tr.left; if (tw <= 0) return;
    float r = (float)(mx - tr.left) / tw; if (r < 0) r = 0; if (r > 1) r = 1;
    if (multimode) {
        if (i == 0) {
            multiMul = 1 + (int)(r * 4.0f + 0.5f);
            if (multiMul < 1) multiMul = 1;
            if (multiMul > 5) multiMul = 5;
        } else {
            multiDelayMs = 1 + (int)(r * 199.0f);
            if (multiDelayMs < 1) multiDelayMs = 1;
            if (multiDelayMs > 200) multiDelayMs = 200;
        }
    } else if (i == 2) {
        cpsMax = CPS_LIMIT_MIN + (int)(r * (CPS_LIMIT_MAX - CPS_LIMIT_MIN) + 0.5f);
        if (cpsMax < CPS_LIMIT_MIN) cpsMax = CPS_LIMIT_MIN;
        if (cpsMax > CPS_LIMIT_MAX) cpsMax = CPS_LIMIT_MAX;
        int max10 = cpsMax * 10;
        if (cpsLeft10 > max10) { cpsLeft10 = max10; leftms = cpsToMs(max10); }
        if (cpsRight10 > max10) { cpsRight10 = max10; rightms = cpsToMs(max10); }
    } else if (i == 3) {
        randomCpsRange = 1 + (int)(r * 4.0f + 0.5f);
        if (randomCpsRange < 1) randomCpsRange = 1;
        if (randomCpsRange > 5) randomCpsRange = 5;
    } else {
        int maxVal = cpsMax * 10;
        int c10 = CPS_MIN10 + (int)(r * (maxVal - CPS_MIN10));
        if (c10 < CPS_MIN10) c10 = CPS_MIN10; if (c10 > maxVal) c10 = maxVal;
        if (i == 0) { cpsLeft10 = c10; leftms = cpsToMs(c10); }
        else        { cpsRight10 = c10; rightms = cpsToMs(c10); }
    }
}

static void Click(HWND hwnd, Elem e)
{
    switch (e) {
    case E_TGL_L: if (multimode) return; leftenabled = !leftenabled; break;
    case E_TGL_R: if (multimode) return; rightenabled = !rightenabled; break;
    case E_BTN_SCROLL_L: scrollClickButton = 0; break;
    case E_BTN_SCROLL_R: scrollClickButton = 1; break;
    case E_TGL_SCROLL:
        isScrollClickActive = !isScrollClickActive;
        PlayScrollClickSound(isScrollClickActive);
        ShowToggleToast(L"\x6eda\x8f6e\x70b9\x51fb", isScrollClickActive);
        break;
    case E_BTN_KEEP: if (multimode) return; keepClicke = !keepClicke; break;
    case E_CHK_RAND:
        randomCpsEnabled = !randomCpsEnabled;
        break;
    case E_INP_MAX:
        g_inputOn = true;
        swprintf(g_inputBuf, 8, L"%d", cpsMax);
        break;
    case E_MODE_L: if (multimode) goto doModeSwitch; break;
    case E_MODE_R: if (!multimode) goto doModeSwitch; break;
    case E_BTN_THEME:
        g_theme = (g_theme == Theme::Dark) ? Theme::Light : Theme::Dark;
        ApplyWin11Style(hwnd);
        break;
    case E_BTN_KEY:
        for (;;) {
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                if (multimode) vk_multi_key = 0;
                else vk_key = 0;
                g_debounceUntil = GetTickCount64() + 200;
                SaveConfig(); Redraw(hwnd); return;
            }
            for (int i = 1; i < 256; i++) {
                if (GetAsyncKeyState(i) & 0x8000) {
                    if (i == 1 || i == 2) continue;
                    if (multimode)
                        vk_multi_key = i;
                    else
                        vk_key = i;
                    g_debounceUntil = GetTickCount64() + 200;
                    SaveConfig(); Redraw(hwnd); return;
                }
            }
            Sleep(1);
        }
    case E_BTN_SCROLL_KEY:
        for (;;) {
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                vk_scroll_key = 0;
                g_debounceUntil = GetTickCount64() + 200;
                SaveConfig(); Redraw(hwnd); return;
            }
            for (int i = 1; i < 256; i++) {
                if (GetAsyncKeyState(i) & 0x8000) {
                    if (i == 1 || i == 2) continue;
                    vk_scroll_key = i;
                    g_debounceUntil = GetTickCount64() + 200;
                    SaveConfig(); Redraw(hwnd); return;
                }
            }
            Sleep(1);
        }
    case E_BTN_SCROLL_LR_KEY:
        for (;;) {
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                vk_scroll_lr_key = 0;
                g_debounceUntil = GetTickCount64() + 200;
                SaveConfig(); Redraw(hwnd); return;
            }
            for (int i = 1; i < 256; i++) {
                if (GetAsyncKeyState(i) & 0x8000) {
                    if (i == 1 || i == 2) continue;
                    vk_scroll_lr_key = i;
                    g_debounceUntil = GetTickCount64() + 200;
                    SaveConfig(); Redraw(hwnd); return;
                }
            }
            Sleep(1);
        }
    doModeSwitch:
        multimode = !multimode;
        break;
    default: break;
    }
    SaveConfig(); Redraw(hwnd);
}

// ---- win main ----
int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR, int nShow)
{
    InitGDI();
    const wchar_t* cn = L"ACgdi";
    WNDCLASSW wc = {};
    wc.hInstance = hI; wc.lpfnWndProc = WndProc; wc.lpszClassName = cn;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    // load icon from resources
    wc.hIcon = LoadIconW(hI, MAKEINTRESOURCEW(101));

    RegisterClassW(&wc);
    RECT wr = { 0, 0, WIN_W, WIN_H };
    AdjustWindowRectEx(&wr, WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME, FALSE, 0);
    HWND hwnd = CreateWindowExW(0, cn, (const wchar_t*)"AutoClicker",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, hI, nullptr);

    LoadConfig();
    ApplyWin11Style(hwnd);
    MakeBuf(hwnd);
    Layout(); UpThumbs(); Paint();
    ShowWindow(hwnd, nShow); UpdateWindow(hwnd);
    std::thread(ClickerThreadProc).detach();
    StartMultiClickHook();
    SetTimer(hwnd, TIMER_RENDER, 16, nullptr);
    MSG msg = {};
    for (;;) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        udmWindow();
        if (g_drag == E_NONE) Sleep(5);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_SIZE: g_cx = LOWORD(l); g_cy = HIWORD(l); FreeBuf(); MakeBuf(h); Layout(); UpThumbs(); Paint(); InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_TIMER: if (w == TIMER_RENDER && g_drag == E_NONE) { UpThumbs(); Paint(); InvalidateRect(h, nullptr, FALSE); } return 0;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); if (g_hdcMem) BitBlt(dc, 0, 0, g_cx, g_cy, g_hdcMem, 0, 0, SRCCOPY); EndPaint(h, &ps); return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN: { POINT pt = { LOWORD(l), HIWORD(l) }; Elem e = Hit(pt);
        if (g_inputOn && e != E_INP_MAX) { g_inputOn = false; Redraw(h); }
        if (e == E_SL_L) { g_drag = e; g_dx = pt.x - ThumbX(0); SetCapture(h); return 0; }
        if (e == E_SL_R) { g_drag = e; g_dx = pt.x - ThumbX(1); SetCapture(h); return 0; }
        if (e == E_SL_MAX) { g_drag = e; g_dx = pt.x - ThumbX(2); SetCapture(h); return 0; }
        if (e == E_SL_RAND) { g_drag = e; g_dx = pt.x - ThumbX(3); SetCapture(h); return 0; }
        Click(h, e); return 0; }
    case WM_LBUTTONUP: { if (g_drag != E_NONE) SaveConfig(); g_drag = E_NONE; ReleaseCapture(); return 0; }
    case WM_MOUSEMOVE: { POINT pt = { LOWORD(l), HIWORD(l) };
        if (g_drag == E_SL_L) { Drag(0, pt.x - g_dx); Redraw(h); }
        else if (g_drag == E_SL_R) { Drag(1, pt.x - g_dx); Redraw(h); }
        else if (g_drag == E_SL_MAX) { Drag(2, pt.x - g_dx); Redraw(h); }
        else if (g_drag == E_SL_RAND) { Drag(3, pt.x - g_dx); Redraw(h); }
        else Hover(pt);
        return 0; }
    case WM_CHAR:
        if (g_inputOn) {
            if (w == VK_RETURN || w == VK_ESCAPE) {
                if (w == VK_RETURN) {
                    int v = _wtoi(g_inputBuf);
                    if (v >= CPS_LIMIT_MIN && v <= CPS_LIMIT_MAX) {
                        cpsMax = v;
                        int max10 = cpsMax * 10;
                        if (cpsLeft10 > max10) { cpsLeft10 = max10; leftms = cpsToMs(max10); }
                        if (cpsRight10 > max10) { cpsRight10 = max10; rightms = cpsToMs(max10); }
                        SaveConfig();
                    }
                }
                g_inputOn = false;
                Redraw(h);
            } else if (w == VK_BACK) {
                int len = (int)wcslen(g_inputBuf);
                if (len > 0) g_inputBuf[len - 1] = L'\0';
                Redraw(h);
            } else if (w >= L'0' && w <= L'9') {
                int len = (int)wcslen(g_inputBuf);
                if (len < 6) { g_inputBuf[len] = (wchar_t)w; g_inputBuf[len + 1] = L'\0'; }
                Redraw(h);
            }
            return 0;
        }
        break;
    case WM_KEYDOWN: return 0;
    case WM_DESTROY: SaveConfig(); FreeBuf(); PostQuitMessage(0); return 0;
    }
    return DefWindowProc(h, m, w, l);
}
