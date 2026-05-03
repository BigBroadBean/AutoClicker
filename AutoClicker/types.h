#pragma once

#include <Windows.h>

constexpr int WIN_W = 480;
constexpr int WIN_H = 620;

constexpr int CPS_MIN10 = 5;
constexpr int CPS_MAX10 = 1000;

constexpr int TIMER_RENDER = 1;

constexpr COLORREF CLR_BG = RGB(28, 28, 28);
constexpr COLORREF CLR_CARD = RGB(38, 38, 38);
constexpr COLORREF CLR_BORDER = RGB(55, 55, 57);
constexpr COLORREF CLR_ACCENT = RGB(96, 205, 255);
constexpr COLORREF CLR_TEXT = RGB(240, 240, 242);
constexpr COLORREF CLR_TEXT_DIM = RGB(140, 140, 148);
constexpr COLORREF CLR_BTN = RGB(50, 50, 54);
constexpr COLORREF CLR_BTN_HOVER = RGB(62, 62, 68);
constexpr COLORREF CLR_GREEN = RGB(76, 194, 110);
constexpr COLORREF CLR_RED = RGB(230, 72, 88);
constexpr COLORREF CLR_TRACK = RGB(60, 60, 64);

constexpr int DWMA_DARK = 20;
constexpr int DWMA_CORNER = 33;
constexpr int DWMA_BACKDROP = 38;
constexpr int CORNER_ROUND = 2;
constexpr int BACKDROP_MAIN = 2;