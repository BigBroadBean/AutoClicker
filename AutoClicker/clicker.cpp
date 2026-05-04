#include "clicker.h"
#include "types.h"
#include "config.h"
#include "sound.h"
#include "overlay.h"

#include <Windows.h>
#include <thread>
#include <chrono>
#include <string>
#include <cstdlib>
#include <ctime>

int cpsLeft10 = 100;
int cpsRight10 = 100;
int cpsMax = 50;
int leftms = 50;
int rightms = 50;
int vk_key = 4;
int vk_multi_key = VK_XBUTTON2;
bool changedKey = false;
HWND mhwnd = nullptr;
bool isstart = false;
bool leftenabled = false;
bool rightenabled = false;
bool keepClicke = false;
bool flag = false;
POINT point = {};

bool multimode = false;
bool isMultiActive = false;
int multiMul = 1;
int multiDelayMs = 20;
bool randomCpsEnabled = false;
int randomCpsRange = 2;

std::wstring getKeyName(int vk)
{
    switch (vk) {
    case VK_LBUTTON:   return L"\x9f20\x6807\x5de6\x952e";
    case VK_RBUTTON:   return L"\x9f20\x6807\x53f3\x952e";
    case VK_MBUTTON:   return L"\x9f20\x6807\x4e2d\x952e";
    case VK_XBUTTON1:  return L"\x9f20\x6807\x4fa7\x952e\x0031";
    case VK_XBUTTON2:  return L"\x9f20\x6807\x4fa7\x952e\x0032";
    }

    wchar_t buffer[256] = { 0 };
    UINT scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);

    switch (vk) {
    case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
    case VK_RCONTROL: case VK_RMENU:
    case VK_LWIN: case VK_RWIN: case VK_APPS:
    case VK_INSERT: case VK_HOME: case VK_PRIOR:
    case VK_DELETE: case VK_END: case VK_NEXT:
    case VK_NUMLOCK: case VK_SCROLL:
    case VK_OEM_NEC_EQUAL:
        scanCode |= 0xE000;
        break;
    }

    GetKeyNameTextW(scanCode << 16, buffer, 255);
    return std::wstring(buffer);
}

void udmWindow()
{
    mhwnd = GetForegroundWindow();
}

static HHOOK g_hMouseHook = nullptr;

static LRESULT CALLBACK MultiClickHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0)
        return CallNextHookEx(nullptr, nCode, wParam, lParam);

    auto* p = (MSLLHOOKSTRUCT*)lParam;

    if (p->flags & LLMHF_INJECTED)
        return CallNextHookEx(nullptr, nCode, wParam, lParam);

    if (!isMultiActive || multiMul <= 1)
        return CallNextHookEx(nullptr, nCode, wParam, lParam);

    bool isLeftDown = (wParam == WM_LBUTTONDOWN);
    bool isRightDown = (wParam == WM_RBUTTONDOWN);

    if (!isLeftDown && !isRightDown)
        return CallNextHookEx(nullptr, nCode, wParam, lParam);

    HWND target = GetForegroundWindow();
    if (!target)
        return CallNextHookEx(nullptr, nCode, wParam, lParam);

    wchar_t cls[64];
    if (GetClassNameW(target, cls, 64) && wcscmp(cls, L"ACgdi") == 0)
        return CallNextHookEx(nullptr, nCode, wParam, lParam);

    POINT pt = p->pt;
    ScreenToClient(target, &pt);

    int count = multiMul - 1;
    int delay = multiDelayMs;
    UINT msgDown = isLeftDown ? WM_LBUTTONDOWN : WM_RBUTTONDOWN;
    UINT msgUp = isLeftDown ? WM_LBUTTONUP : WM_RBUTTONUP;
    WPARAM wpDown = isLeftDown ? MK_LBUTTON : MK_RBUTTON;
    LPARAM lp = MAKELPARAM(pt.x, pt.y);

    std::thread([target, count, delay, msgDown, msgUp, wpDown, lp]() {
        typedef int(WINAPI * pPostMsg)(HWND, UINT, WPARAM, LPARAM);
        pPostMsg PostMsgA = (pPostMsg)GetProcAddress(LoadLibraryA("User32.dll"), "PostMessageA");
        if (!PostMsgA) return;

        for (int i = 0; i < count; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            PostMsgA(target, msgDown, wpDown, lp);
            PostMsgA(target, msgUp, 0, lp);
        }
    }).detach();

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void StartMultiClickHook()
{
    std::thread([]() {
        g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MultiClickHookProc,
                                         GetModuleHandleW(nullptr), 0);
        if (!g_hMouseHook) return;

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        UnhookWindowsHookEx(g_hMouseHook);
        g_hMouseHook = nullptr;
    }).detach();
}

void ClickerThreadProc()
{
    typedef int(WINAPI* pPostMessageA) (HWND, UINT, WPARAM, LPARAM);
    pPostMessageA MyPostMessageA = (pPostMessageA)GetProcAddress(LoadLibraryA("User32.dll"), "PostMessageA");
    srand((unsigned)time(nullptr));

    auto randDelay = [](int baseMs, int baseCps10) -> int {
        if (!randomCpsEnabled) return baseMs;
        int offset = (std::rand() % (randomCpsRange * 20 + 1)) - randomCpsRange * 10;
        int cps10 = baseCps10 + offset;
        if (cps10 < CPS_MIN10) cps10 = CPS_MIN10;
        if (cps10 > cpsMax * 10) cps10 = cpsMax * 10;
        return cpsToMs(cps10);
    };

    for (;;) {
        if (!flag) {
            Sleep(1);
            flag = true;
            continue;
        }

        if (changedKey) {
            Sleep(200);
            changedKey = false;
        }

        if (GetAsyncKeyState(vk_multi_key) & 0x8000) {
            isMultiActive = !isMultiActive;
            PlayToggleSound(isMultiActive);
            ShowToggleToast(L"\x591a\x500d\x70b9", isMultiActive);
            SaveConfig();
            Sleep(200);
        }

        if (GetAsyncKeyState(vk_key) & 0x8000) {
            isstart = !isstart;
            if (isstart) {
                GetAsyncKeyState(VK_LBUTTON);
                GetAsyncKeyState(VK_RBUTTON);
                PlayToggleSound(true);
                ShowToggleToast(L"\x8fde\x70b9\x5668", true);
                timeBeginPeriod(1);
            } else {
                PlayToggleSound(false);
                ShowToggleToast(L"\x8fde\x70b9\x5668", false);
                timeEndPeriod(1);
            }
            Sleep(200);
        }

        if (isMultiActive) {
            Sleep(200);
            continue;
        }

        bool leftActive = isstart && leftenabled && mhwnd != nullptr;
        bool rightActive = isstart && rightenabled && mhwnd != nullptr;

        if ((leftActive && (GetAsyncKeyState(VK_LBUTTON) & 0x8000)) || (leftActive && keepClicke)) {
            GetCursorPos(&point);
            ScreenToClient(mhwnd, &point);
            int del = randDelay(leftms, cpsLeft10);
            MyPostMessageA(mhwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(point.x, point.y));
            std::this_thread::sleep_for(std::chrono::milliseconds(del));
            MyPostMessageA(mhwnd, WM_LBUTTONUP, 0, MAKELPARAM(point.x, point.y));
            std::this_thread::sleep_for(std::chrono::milliseconds(del));
        } else if ((rightActive && (GetAsyncKeyState(VK_RBUTTON) & 0x8000)) || (rightActive && keepClicke)) {
            GetCursorPos(&point);
            ScreenToClient(mhwnd, &point);
            int del = randDelay(rightms, cpsRight10);
            MyPostMessageA(mhwnd, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(point.x, point.y));
            std::this_thread::sleep_for(std::chrono::milliseconds(del));
            MyPostMessageA(mhwnd, WM_RBUTTONUP, 0, MAKELPARAM(point.x, point.y));
            std::this_thread::sleep_for(std::chrono::milliseconds(del));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}