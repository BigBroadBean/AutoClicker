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
#include <atomic>

#pragma comment(lib, "winmm.lib")

using namespace std::chrono;

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
std::atomic<long long> g_debounceUntil{ 0 };

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

    static std::atomic<bool> busyMulti{ false };
    static std::atomic<bool> busyKey{ false };

    bool prevMulti = false;
    bool prevStart = false;

    // non-blocking click state
    enum ClickState { CS_IDLE, CS_WAIT_UP, CS_WAIT_DOWN };
    ClickState leftSt  = CS_IDLE;
    ClickState rightSt = CS_IDLE;
    auto nextLeftTime  = steady_clock::now();
    auto nextRightTime = steady_clock::now();
    POINT lastPt = {};

    for (;;) {
        auto now = steady_clock::now();

        if (!flag) {
            flag = true;
            Sleep(1);
            continue;
        }

        // debounce after key rebind
        if (GetTickCount64() < (unsigned long long)g_debounceUntil) {
            Sleep(1);
            continue;
        }

        // multi-click hotkey - edge detect + async wait-release
        bool curMulti = (GetAsyncKeyState(vk_multi_key) & 0x8000) != 0;
        if (curMulti && !prevMulti && !busyMulti.exchange(true)) {
            std::thread([]() {
                while (GetAsyncKeyState(vk_multi_key) & 0x8000) Sleep(1);
                isMultiActive = !isMultiActive;
                PlayMultiClickSound(isMultiActive);
                ShowToggleToast(L"\x591a\x500d\x70b9", isMultiActive);
                SaveConfig();
                busyMulti = false;
            }).detach();
        }
        prevMulti = curMulti;

        // auto-clicker hotkey - edge detect + async wait-release
        bool curStart = (GetAsyncKeyState(vk_key) & 0x8000) != 0;
        if (curStart && !prevStart && !busyKey.exchange(true)) {
            std::thread([]() {
                while (GetAsyncKeyState(vk_key) & 0x8000) Sleep(1);
                isstart = !isstart;
                if (isstart) {
                    GetAsyncKeyState(VK_LBUTTON);
                    GetAsyncKeyState(VK_RBUTTON);
                    timeBeginPeriod(1);
                } else {
                    timeEndPeriod(1);
                }
                PlayClickerSound(isstart);
                ShowToggleToast(L"\x8fde\x70b9\x5668", isstart);
                SaveConfig();
                busyKey = false;
            }).detach();
        }
        prevStart = curStart;

        if (isMultiActive) {
            leftSt = CS_IDLE; rightSt = CS_IDLE;
            Sleep(1);
            continue;
        }

        bool leftActive = isstart && leftenabled && mhwnd != nullptr;
        bool rightActive = isstart && rightenabled && mhwnd != nullptr;

        // left click state machine
        if ((leftActive && (GetAsyncKeyState(VK_LBUTTON) & 0x8000)) || (leftActive && keepClicke)) {
            if (now >= nextLeftTime) {
                GetCursorPos(&lastPt);
                ScreenToClient(mhwnd, &lastPt);
                LPARAM lp = MAKELPARAM(lastPt.x, lastPt.y);
                if (leftSt != CS_WAIT_UP) {
                    MyPostMessageA(mhwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
                    leftSt = CS_WAIT_UP;
                } else {
                    MyPostMessageA(mhwnd, WM_LBUTTONUP, 0, lp);
                    leftSt = CS_IDLE;
                }
                int del = randDelay(leftms, cpsLeft10);
                nextLeftTime = now + milliseconds(del);
            }
        } else {
            leftSt = CS_IDLE;
        }

        // right click state machine
        if ((rightActive && (GetAsyncKeyState(VK_RBUTTON) & 0x8000)) || (rightActive && keepClicke)) {
            if (now >= nextRightTime) {
                GetCursorPos(&lastPt);
                ScreenToClient(mhwnd, &lastPt);
                LPARAM lp = MAKELPARAM(lastPt.x, lastPt.y);
                if (rightSt != CS_WAIT_UP) {
                    MyPostMessageA(mhwnd, WM_RBUTTONDOWN, MK_RBUTTON, lp);
                    rightSt = CS_WAIT_UP;
                } else {
                    MyPostMessageA(mhwnd, WM_RBUTTONUP, 0, lp);
                    rightSt = CS_IDLE;
                }
                int del = randDelay(rightms, cpsRight10);
                nextRightTime = now + milliseconds(del);
            }
        } else {
            rightSt = CS_IDLE;
        }

        Sleep(1);
    }
}