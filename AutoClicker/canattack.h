#pragma once

#include <Windows.h>
#include <atomic>

// ============================================================
//  can-attack gating (auxiliary of the direct clicker)
// ============================================================
// MCCanAttackJni.dll is injected into Minecraft Java processes (javaw/java).
// Once attached to the game's JVM it sends one UDP datagram every ~5ms to
// 127.0.0.1:35785 carrying a single byte: '0' (0x30) = the targeted entity is
// NOT attackable right now, '1' (0x31) = it IS attackable.
//
// This module owns two background threads:
//   1. UDP monitor   : binds 127.0.0.1:35785, reads the live 0/1 into
//                      g_canAttack, async loop sleeping 5ms per iteration.
//   2. Injector      : periodically finds Minecraft Java processes that do NOT
//                      have the DLL loaded yet and injects them (LoadLibrary
//                      remote thread), with anti-double-injection handling.

// ---- feature state ----
// atomic so the injector thread can reliably observe UI/hotkey toggles
extern std::atomic<bool> canAttackOnlyClick;   // gate on/off (GUI + hotkey)
extern int  vk_canattack_key;     // toggle hotkey VK code (0 = none)

// ---- live status from the UDP stream ----
// 1 = attackable, 0 = not attackable. Fail-safe: no fresh packet for a while
// (game closed / DLL not injected / port busy) -> 0 (cannot attack).
extern std::atomic<int> g_canAttack;
// GetTickCount64() of the last received packet; 0 = never received any.
extern std::atomic<long long> g_canAttackLastMs;

// true while UDP packets are arriving (game injected & running)
bool CanAttackConnected();

// true when MCCanAttackJni.dll can be located next to the exe / in CWD
bool CanAttackDllAvailable();

// start both background threads (called once from WinMain)
void StartCanAttackMonitor();
void StartInjectorThread();
