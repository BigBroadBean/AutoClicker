#include "types.h"
#include "ui.h"
#include "clicker.h"
#include "config.h"
#include "sound.h"
#include "overlay.h"

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
enum Elem { E_NONE = -1, E_SL_L, E_SL_R, E_SL_MAX, E_SL_RAND, E_TGL_L, E_TGL_R, E_BTN_KEY, E_BTN_KEEP, E_MODE_L, E_MODE_R, E_CHK_RAND, E_INP_MAX, E_COUNT };
struct HR { RECT r; Elem id; bool hover; };
static HR   g_hr[E_COUNT] = {};
static Elem g_drag = E_NONE;
static int  g_dx   = 0;
static bool g_inputOn = false;
static wchar_t g_inputBuf[8] = {};

// ---- layout rects ----
struct LY {
    RECT title, card[6], track[4], thumb[4];
    RECT tgl[2], btnKey, btnKeep, info, info2, status, sep;
    RECT modeBtn, inpMax, chkRand;
} L;

static bool PtIn(const RECT& r, int x, int y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

static void Layout()
{
    int W = g_cx, H = g_cy;
    L.title = { 0, 6, W, 46 };
    L.sep   = { 0, 48, W, 49 };

    L.card[0] = { 16,  60, W - 16, 170 };
    L.card[1] = { 16, 186, W - 16, 296 };
    L.card[2] = { 16, 312, W - 16, 378 };
    L.card[3] = { 16, 396, W - 16, 450 };
    L.card[4] = { 16, 468, W - 16, 522 };
    L.card[5] = { 16, 540, W - 16, 610 };

    auto Slider = [](const RECT& c) -> RECT {
        return { c.left + 16, c.top + 32, c.right - 106, c.top + 38 };
    };
    L.track[0] = Slider(L.card[0]);
    L.track[1] = Slider(L.card[1]);
    L.track[2] = Slider(L.card[4]);
    L.track[3] = { L.card[5].left + 16, L.card[5].top + 46,
                   L.card[5].right - 106, L.card[5].top + 52 };

    L.tgl[0] = { L.card[0].right - 90, L.card[0].top + 24,
                 L.card[0].right - 20, L.card[0].top + 54 };
    L.tgl[1] = { L.card[1].right - 90, L.card[1].top + 24,
                 L.card[1].right - 20, L.card[1].top + 54 };

    L.btnKey = { L.card[2].left + 16, L.card[2].top + 26,
                 L.card[2].left + 300, L.card[2].top + 56 };
    L.btnKeep = { L.card[3].left + 16, L.card[3].top + 26,
                  L.card[3].left + 210, L.card[3].top + 46 };

    L.modeBtn = { L.title.right - 120, L.title.top + 8,
                  L.title.right - 16,  L.title.bottom - 8 };

    L.inpMax = { L.track[2].right + 8, L.track[2].top - 6,
                 L.card[4].right - 20, L.track[2].bottom + 6 };

    L.chkRand = { L.card[5].left + 16, L.card[5].top + 26,
                  L.card[5].left + 180, L.card[5].top + 46 };

    L.info   = { 16, H - 94, W - 16, H - 74 };
    L.info2  = { 16, H - 72, W - 16, H - 52 };
    L.status = { 16, H - 44, W - 16, H - 20 };

    for (int i = 0; i < 4; i++) {
        L.thumb[i].top    = L.track[i].top - 10;
        L.thumb[i].bottom = L.track[i].bottom + 10;
    }

    g_hr[E_SL_L]    = { L.thumb[0], E_SL_L, false };
    g_hr[E_SL_R]    = { L.thumb[1], E_SL_R, false };
    g_hr[E_SL_MAX]  = { L.thumb[2], E_SL_MAX, false };
    g_hr[E_SL_RAND] = { L.thumb[3], E_SL_RAND, false };
    g_hr[E_TGL_L]   = { L.tgl[0],   E_TGL_L, false };
    g_hr[E_TGL_R]   = { L.tgl[1],   E_TGL_R, false };
    g_hr[E_BTN_KEY] = { L.btnKey,   E_BTN_KEY, false };
    g_hr[E_BTN_KEEP] = { L.btnKeep, E_BTN_KEEP, false };
    g_hr[E_MODE_L] = { { L.modeBtn.left, L.modeBtn.top, (L.modeBtn.left + L.modeBtn.right) / 2, L.modeBtn.bottom }, E_MODE_L, false };
    g_hr[E_MODE_R] = { { (L.modeBtn.left + L.modeBtn.right) / 2, L.modeBtn.top, L.modeBtn.right, L.modeBtn.bottom }, E_MODE_R, false };
    g_hr[E_INP_MAX] = { L.inpMax, E_INP_MAX, false };
    g_hr[E_CHK_RAND] = { L.chkRand, E_CHK_RAND, false };
}

// ---- GDI init ----
static void InitGDI()
{
    auto F = [](int h, int w, int q, const wchar_t* n) -> HFONT {
        HFONT f = CreateFontW(h, 0, 0, 0, w, 0, 0, 0, 0, 0, 0, q, 0, L"Microsoft YaHei UI");
        if (!f) f = CreateFontW(h, 0, 0, 0, w, 0, 0, 0, 0, 0, 0, q, 0, L"Segoe UI");
        return f;
    };
    g_hfTitle = F(26, FW_BOLD,    CLEARTYPE_QUALITY, L"Microsoft YaHei UI");
    g_hfLabel = F(20, FW_SEMIBOLD, CLEARTYPE_QUALITY, L"Microsoft YaHei UI");
    g_hfBody  = F(20, FW_NORMAL,   CLEARTYPE_QUALITY, L"Microsoft YaHei UI");
    g_hfCPS   = F(20, FW_SEMIBOLD, CLEARTYPE_QUALITY, L"Microsoft YaHei UI");
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
static int  ThumbX(int i) {
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
static void UpThumbs() { for (int i = 0; i < 4; i++) { int cx = ThumbX(i); L.thumb[i].left = cx - 14; L.thumb[i].right = cx + 14; g_hr[i].r = L.thumb[i]; } }

// ---- render ----
static void Paint()
{
    HDC dc = g_hdcMem;
    SetBkMode(dc, TRANSPARENT);

    // background
    { HBRUSH b = CreateSolidBrush(CLR_BG); RECT a = { 0, 0, g_cx, g_cy }; FillRect(dc, &a, b); DeleteObject(b); }

    // cards
    HPEN   pn = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HBRUSH br = CreateSolidBrush(CLR_CARD);
    SelectObject(dc, pn); SelectObject(dc, br);
    for (int i = 0; i < (multimode ? 3 : 6); i++)
        RoundRect(dc, L.card[i].left, L.card[i].top, L.card[i].right, L.card[i].bottom, 10, 10);
    DeleteObject(pn); DeleteObject(br);

    // accent line
    pn = CreatePen(PS_SOLID, 1, CLR_ACCENT);
    SelectObject(dc, pn);
    MoveToEx(dc, 12, 49, nullptr); LineTo(dc, WIN_W - 12, 49);
    DeleteObject(pn);

    // title
    SelectObject(dc, g_hfTitle);
    SetTextColor(dc, CLR_TEXT);
    RECT tr = L.title; tr.top += 2; tr.right = L.modeBtn.left - 8;
    DrawTextW(dc, L"AutoClicker", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // mode toggle - segmented pill
    {
        RECT& b = L.modeBtn;
        int midX = (b.left + b.right) / 2;
        bool hover = g_hr[E_MODE_L].hover || g_hr[E_MODE_R].hover;
        COLORREF bg = hover ? CLR_BTN_HOVER : CLR_BTN;

        // fill inactive bg
        HBRUSH bgBrush = CreateSolidBrush(bg);
        HPEN noPen = (HPEN)GetStockObject(NULL_PEN);
        SelectObject(dc, noPen); SelectObject(dc, bgBrush);
        RoundRect(dc, b.left, b.top, b.right, b.bottom, 14, 14);
        DeleteObject(bgBrush);

        // fill active half with clipping for rounded corners
        SaveDC(dc);
        IntersectClipRect(dc,
            multimode ? midX : b.left,
            b.top,
            multimode ? b.right : midX,
            b.bottom);
        HPEN actPen = CreatePen(PS_SOLID, 1, CLR_ACCENT);
        HBRUSH actBrush = CreateSolidBrush(CLR_ACCENT);
        SelectObject(dc, actPen); SelectObject(dc, actBrush);
        RoundRect(dc, b.left, b.top, b.right, b.bottom, 14, 14);
        DeleteObject(actPen); DeleteObject(actBrush);
        RestoreDC(dc, -1);

        // pill outline
        HPEN outline = CreatePen(PS_SOLID, 1, CLR_BORDER);
        SelectObject(dc, outline); SelectObject(dc, GetStockObject(NULL_BRUSH));
        RoundRect(dc, b.left, b.top, b.right, b.bottom, 14, 14);
        DeleteObject(outline);

        // text
        SelectObject(dc, g_hfLabel);
        RECT rl = { b.left, b.top, midX, b.bottom };
        RECT rr = { midX, b.top, b.right, b.bottom };
        SetTextColor(dc, multimode ? CLR_TEXT_DIM : RGB(0, 0, 0));
        DrawTextW(dc, L"连点", -1, &rl, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(dc, multimode ? RGB(0, 0, 0) : CLR_TEXT_DIM);
        DrawTextW(dc, L"多倍", -1, &rr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // card labels
    SelectObject(dc, g_hfLabel);
    SetTextColor(dc, CLR_TEXT);
    const wchar_t* namesNormal[6] = { L"左键", L"右键", L"快捷键", L"模式", L"CPS上限", L"随机CPS" };
    const wchar_t* namesMulti[3]  = { L"倍率", L"延迟", L"快捷键" };
    const wchar_t** names = multimode ? namesMulti : namesNormal;
    int cardCount = multimode ? 3 : 6;
    for (int i = 0; i < cardCount; i++) {
        RECT r = { L.card[i].left + 16, L.card[i].top + 6,
                   L.card[i].right, L.card[i].top + 26 };
        DrawTextW(dc, names[i], -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // sliders
    int sliderCount = multimode ? 2 : (randomCpsEnabled ? 4 : 3);
    for (int i = 0; i < sliderCount; i++) {
        RECT& trk = L.track[i];
        // track bg
        { HBRUSH b = CreateSolidBrush(CLR_TRACK); FillRect(dc, &trk, b); DeleteObject(b); }
        // filled
        int fx = ThumbX(i);
        if (fx > trk.left + 1) {
            RECT r = { trk.left, trk.top, fx, trk.bottom };
            HBRUSH b = CreateSolidBrush(CLR_ACCENT); FillRect(dc, &r, b); DeleteObject(b);
        }
        // track border
        HPEN p = CreatePen(PS_SOLID, 0, CLR_TRACK);
        SelectObject(dc, p); SelectObject(dc, GetStockObject(NULL_BRUSH));
        RoundRect(dc, trk.left, trk.top, trk.right, trk.bottom, 3, 3);
        DeleteObject(p);
        // thumb
        int cx = ThumbX(i), cy = (trk.top + trk.bottom) / 2, r = 11;
        HPEN pt = CreatePen(PS_SOLID, 3, CLR_ACCENT);
        HBRUSH bt = CreateSolidBrush(g_hr[i].hover || g_drag == g_hr[i].id ? RGB(50, 50, 50) : RGB(40, 40, 40));
        SelectObject(dc, pt); SelectObject(dc, bt);
        Ellipse(dc, cx - r, cy - r, cx + r, cy + r);
        DeleteObject(pt); DeleteObject(bt);
    }

    // toggles (normal mode only)
    if (!multimode) {
    for (int i = 0; i < 2; i++) {
        RECT& tg = L.tgl[i];
        bool on = (i == 0) ? leftenabled : rightenabled;
        COLORREF bg = on ? CLR_GREEN : CLR_BTN;
        HPEN pt = CreatePen(PS_SOLID, 1, bg);
        HBRUSH bt = CreateSolidBrush(bg);
        SelectObject(dc, pt); SelectObject(dc, bt);
        RoundRect(dc, tg.left, tg.top, tg.right, tg.bottom, 16, 16);
        DeleteObject(pt); DeleteObject(bt);
        SelectObject(dc, g_hfLabel);
        SetTextColor(dc, on ? RGB(0, 0, 0) : CLR_TEXT_DIM);
        DrawTextW(dc, on ? L"开" : L"关", -1, (RECT*)&tg, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    }

    // hotkey button
    {
        RECT& b = L.btnKey;
        HPEN p = CreatePen(PS_SOLID, 1, CLR_BORDER);
        HBRUSH brb = CreateSolidBrush(g_hr[E_BTN_KEY].hover ? CLR_BTN_HOVER : CLR_BTN);
        SelectObject(dc, p); SelectObject(dc, brb);
        RoundRect(dc, b.left, b.top, b.right, b.bottom, 8, 8);
        DeleteObject(p); DeleteObject(brb);
        SelectObject(dc, g_hfBody);
        SetTextColor(dc, CLR_TEXT);
        std::wstring t = L"快捷键: " + getKeyName(multimode ? vk_multi_key : vk_key);
        DrawTextW(dc, t.c_str(), -1, (RECT*)&b, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // keep button (normal mode only)
    if (!multimode) {
        RECT& b = L.btnKeep;
        HPEN p = CreatePen(PS_SOLID, 1, keepClicke ? CLR_ACCENT : CLR_BORDER);
        HBRUSH brb = CreateSolidBrush(g_hr[E_BTN_KEEP].hover ? CLR_BTN_HOVER : CLR_BTN);
        SelectObject(dc, p); SelectObject(dc, brb);
        RoundRect(dc, b.left, b.top, b.right, b.bottom, 8, 8);
        DeleteObject(p); DeleteObject(brb);
        SelectObject(dc, g_hfBody);
        SetTextColor(dc, keepClicke ? CLR_ACCENT : CLR_TEXT);
        DrawTextW(dc, keepClicke ? L"不需要按住连点: 开" : L"不需要按住连点: 关",
                  -1, (RECT*)&b, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Value display in cards
    SelectObject(dc, g_hfCPS);
    if (multimode) {
        wchar_t buf[32];
        swprintf(buf, 32, L"%d 倍", multiMul);
        RECT r0 = { L.card[0].left + 16, L.card[0].top + 62,
                    L.track[0].right, L.card[0].top + 90 };
        SetTextColor(dc, CLR_TEXT);
        DrawTextW(dc, buf, -1, &r0, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        swprintf(buf, 32, L"%d 毫秒", multiDelayMs);
        RECT r1 = { L.card[1].left + 16, L.card[1].top + 62,
                    L.track[1].right, L.card[1].top + 90 };
        SetTextColor(dc, CLR_TEXT);
        DrawTextW(dc, buf, -1, &r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {
        for (int i = 0; i < 2; i++) {
            int c10 = (i == 0) ? cpsLeft10 : cpsRight10;
            int ms  = (i == 0) ? leftms : rightms;
            wchar_t buf[32];
            swprintf(buf, 32, L"%.1f 次/秒", c10 / 10.0f);
            RECT r = { L.card[i].left + 16, L.card[i].top + 62,
                       L.track[i].right, L.card[i].top + 90 };
            SetTextColor(dc, CLR_TEXT);
            DrawTextW(dc, buf, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            swprintf(buf, 32, L"%d 毫秒", ms);
            SetTextColor(dc, CLR_TEXT_DIM);
            RECT r2 = { r.right + 4, r.top, L.card[i].right - 8, r.bottom };
            DrawTextW(dc, buf, -1, &r2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
        // input box
        RECT& bi = L.inpMax;
        COLORREF inpBorder = g_hr[E_INP_MAX].hover || g_inputOn ? CLR_ACCENT : CLR_BORDER;
        HPEN ip = CreatePen(PS_SOLID, 1, inpBorder);
        HBRUSH ib = CreateSolidBrush(CLR_BTN);
        SelectObject(dc, ip); SelectObject(dc, ib);
        RoundRect(dc, bi.left, bi.top, bi.right, bi.bottom, 6, 6);
        DeleteObject(ip); DeleteObject(ib);
        SetTextColor(dc, CLR_TEXT);
        if (g_inputOn) {
            DrawTextW(dc, g_inputBuf, -1, &bi, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            wchar_t ibuf[8];
            swprintf(ibuf, 8, L"%d", cpsMax);
            DrawTextW(dc, ibuf, -1, &bi, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // random CPS checkbox
        {
            RECT& cb = L.chkRand;
            int box = cb.left;
            int by = (cb.top + cb.bottom) / 2;
            HPEN cp = CreatePen(PS_SOLID, 1, randomCpsEnabled ? CLR_ACCENT : CLR_BORDER);
            HBRUSH cbFill = CreateSolidBrush(randomCpsEnabled ? CLR_ACCENT : CLR_CARD);
            SelectObject(dc, cp); SelectObject(dc, cbFill);
            RoundRect(dc, box, by - 7, box + 14, by + 7, 4, 4);
            DeleteObject(cp); DeleteObject(cbFill);
            if (randomCpsEnabled) {
                // check mark
                SelectObject(dc, (HPEN)GetStockObject(WHITE_PEN));
                MoveToEx(dc, box + 3, by, nullptr);
                LineTo(dc, box + 6, by + 4);
                LineTo(dc, box + 12, by - 3);
            }
            RECT txt = { box + 20, cb.top, cb.right, cb.bottom };
            SetTextColor(dc, CLR_TEXT);
            SelectObject(dc, g_hfBody);
            DrawTextW(dc, L"随机CPS", -1, &txt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        // random CPS range slider
        if (randomCpsEnabled) {
            wchar_t bufR[32];
            swprintf(bufR, 32, L"±%d CPS", randomCpsRange);
            RECT rRand = { L.card[5].left + 16, L.card[5].top + 52,
                           L.card[5].right - 8, L.card[5].top + 64 };
            SetTextColor(dc, CLR_TEXT);
            SelectObject(dc, g_hfCPS);
            DrawTextW(dc, bufR, -1, &rRand, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }

    // bottom info lines
    SelectObject(dc, g_hfBody);
    {
        // clicker info
        float lc = leftenabled ? cpsLeft10 / 10.0f : 0;
        float rc = rightenabled ? cpsRight10 / 10.0f : 0;
        wchar_t buf[128];
        if (randomCpsEnabled)
            swprintf(buf, 128, L"连点器: 左 %.1f cps (%d ms)  右 %.1f cps (%d ms)  上限 %d  随机 ±%d",
                     lc, leftms, rc, rightms, cpsMax, randomCpsRange);
        else
            swprintf(buf, 128, L"连点器: 左 %.1f cps (%d ms)  右 %.1f cps (%d ms)  上限 %d",
                     lc, leftms, rc, rightms, cpsMax);
        SetTextColor(dc, CLR_TEXT_DIM);
        DrawTextW(dc, buf, -1, &L.info, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // multi-click info
        swprintf(buf, 128, L"多倍点: %d 倍  间隔 %d ms", multiMul, multiDelayMs);
        DrawTextW(dc, buf, -1, &L.info2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // status - two indicators
    {
        auto Dot = [&](int x, int y, COLORREF c) {
            HBRUSH b = CreateSolidBrush(c);
            SelectObject(dc, GetStockObject(NULL_PEN)); SelectObject(dc, b);
            Ellipse(dc, x, y, x + 10, y + 10);
            DeleteObject(b);
        };
        int baseY = L.status.top + 3;
        int leftX = L.status.left + 4;
        int rightX = L.status.left + (L.status.right - L.status.left) / 2;

        // clicker
        COLORREF clrC = isstart ? CLR_GREEN : CLR_RED;
        Dot(leftX, baseY, clrC);
        SetTextColor(dc, clrC);
        RECT rl = { leftX + 14, L.status.top, rightX - 8, L.status.bottom };
        DrawTextW(dc, isstart ? L"\x8fde\x70b9\x5668 \x8fd0\x884c\x4e2d" : L"\x8fde\x70b9\x5668 \x5df2\x505c\x6b62",
                  -1, &rl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // multi-click
        COLORREF clrM = isMultiActive ? CLR_ACCENT : CLR_RED;
        Dot(rightX, baseY, clrM);
        SetTextColor(dc, clrM);
        RECT rr = { rightX + 14, L.status.top, L.status.right - 4, L.status.bottom };
        DrawTextW(dc, isMultiActive ? L"\x591a\x500d\x70b9 \x8fd0\x884c\x4e2d" : L"\x591a\x500d\x70b9 \x5df2\x505c\x6b62",
                  -1, &rr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
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
    case E_BTN_KEY: changedKey = true;
        for (;;) {
            for (int i = 1; i < 256; i++) {
                if (GetAsyncKeyState(i) & 0x8000) {
                    if (i == 1 || i == 2) continue;
                    if (multimode)
                        vk_multi_key = i;
                    else
                        vk_key = i;
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

    ApplyWin11Style(hwnd);
    LoadConfig();
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
    case WM_KEYDOWN: if (w == VK_ESCAPE) DestroyWindow(h); return 0;
    case WM_DESTROY: SaveConfig(); FreeBuf(); PostQuitMessage(0); return 0;
    }
    return DefWindowProc(h, m, w, l);
}