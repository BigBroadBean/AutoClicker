#pragma once

#include "types.h"
#include <Windows.h>
#include <string>
#include <atomic>

// ---- auto-clicker state ----
extern int cpsLeft10;
extern int cpsRight10;
extern int cpsMax;
extern int leftms;
extern int rightms;
extern int vk_key;
extern int vk_multi_key;
extern int vk_scroll_key;
extern int vk_scroll_lr_key;
extern bool changedKey;
extern HWND mhwnd;
extern bool isstart;
extern bool leftenabled;
extern bool rightenabled;
extern bool keepClicke;
extern bool flag;
extern POINT point;

// ---- multi-click mode ----
extern bool isMultiActive;
extern int multiMul;
extern int multiDelayMs;

// ---- scroll-to-click ----
extern bool isScrollClickActive;
extern int scrollClickButton;

// ---- random CPS ----
extern bool randomCpsEnabled;
extern int randomCpsRange;

// ---- auto-stop timer ----
extern bool autoStopEnabled;
extern int autoStopSeconds;

// ---- window ----
extern bool topmost;

// ---- misc ----
extern std::atomic<long long> g_clickCount;
extern std::atomic<long long> g_debounceUntil;

std::wstring getKeyName(int vk);
void udmWindow();
void ClickerThreadProc();
void StartMultiClickHook();

// realtime CPS: record every click, then query clicks in the last second
void RecordClick();
int GetRealtimeCps();

inline int cpsToMs(int cps10) {
    float cps = cps10 / 10.0f;
    int ms = (int)(500.0f / cps);
    return ms < 1 ? 1 : ms;
}

inline int msToCps10(int ms) {
    if (ms <= 0) return 1000;
    float cps = 500.0f / (float)ms;
    int cps10 = (int)(cps * 10.0f);
    if (cps10 < 5) cps10 = 5;
    if (cps10 > 1000) cps10 = 1000;
    return cps10;
}
