#pragma once

#include <Windows.h>

constexpr int WIN_W = 500;
constexpr int WIN_H = 780;

constexpr int CPS_MIN10 = 5;
constexpr int CPS_MAX10 = 1000;

constexpr int CPS_LIMIT_MIN = 20;
constexpr int CPS_LIMIT_MAX = 500;
constexpr int CPS_LIMIT_DEFAULT = 50;

constexpr int TIMER_RENDER = 1;

// ---- Dark theme ----
constexpr COLORREF CLR_BG_DARK       = RGB(28, 28, 30);
constexpr COLORREF CLR_CARD_DARK     = RGB(34, 34, 38);
constexpr COLORREF CLR_BORDER_DARK   = RGB(52, 52, 56);
constexpr COLORREF CLR_ACCENT_DARK   = RGB(84, 184, 255);
constexpr COLORREF CLR_TEXT_DARK     = RGB(232, 232, 240);
constexpr COLORREF CLR_TEXT_DIM_DARK = RGB(140, 140, 150);
constexpr COLORREF CLR_BTN_DARK      = RGB(46, 46, 52);
constexpr COLORREF CLR_BTN_HOVER_DARK= RGB(58, 58, 66);
constexpr COLORREF CLR_GREEN_DARK    = RGB(76, 194, 110);
constexpr COLORREF CLR_RED_DARK      = RGB(230, 72, 88);
constexpr COLORREF CLR_TRACK_DARK    = RGB(56, 56, 62);

// ---- Light theme ----
constexpr COLORREF CLR_BG_LIGHT       = RGB(245, 245, 248);
constexpr COLORREF CLR_CARD_LIGHT     = RGB(252, 252, 254);
constexpr COLORREF CLR_BORDER_LIGHT   = RGB(218, 218, 224);
constexpr COLORREF CLR_ACCENT_LIGHT   = RGB(0, 120, 212);
constexpr COLORREF CLR_TEXT_LIGHT     = RGB(28, 28, 36);
constexpr COLORREF CLR_TEXT_DIM_LIGHT = RGB(120, 120, 130);
constexpr COLORREF CLR_BTN_LIGHT      = RGB(238, 238, 244);
constexpr COLORREF CLR_BTN_HOVER_LIGHT= RGB(228, 228, 236);
constexpr COLORREF CLR_GREEN_LIGHT    = RGB(48, 164, 80);
constexpr COLORREF CLR_RED_LIGHT      = RGB(210, 52, 68);
constexpr COLORREF CLR_TRACK_LIGHT    = RGB(225, 225, 232);

// ---- Theme enum ----
enum class Theme : int { Dark = 0, Light = 1 };
extern Theme g_theme;

// ---- Theme-aware color accessors ----
inline COLORREF BG()     { return g_theme == Theme::Dark ? CLR_BG_DARK    : CLR_BG_LIGHT; }
inline COLORREF CARD()   { return g_theme == Theme::Dark ? CLR_CARD_DARK  : CLR_CARD_LIGHT; }
inline COLORREF BORDER() { return g_theme == Theme::Dark ? CLR_BORDER_DARK: CLR_BORDER_LIGHT; }
inline COLORREF ACCENT() { return g_theme == Theme::Dark ? CLR_ACCENT_DARK: CLR_ACCENT_LIGHT; }
inline COLORREF TXT()    { return g_theme == Theme::Dark ? CLR_TEXT_DARK  : CLR_TEXT_LIGHT; }
inline COLORREF TXT_DIM() { return g_theme == Theme::Dark ? CLR_TEXT_DIM_DARK: CLR_TEXT_DIM_LIGHT; }
inline COLORREF BTN()    { return g_theme == Theme::Dark ? CLR_BTN_DARK   : CLR_BTN_LIGHT; }
inline COLORREF BTN_HOVER(){ return g_theme == Theme::Dark ? CLR_BTN_HOVER_DARK: CLR_BTN_HOVER_LIGHT; }
inline COLORREF GREEN()  { return g_theme == Theme::Dark ? CLR_GREEN_DARK : CLR_GREEN_LIGHT; }
inline COLORREF RED()    { return g_theme == Theme::Dark ? CLR_RED_DARK   : CLR_RED_LIGHT; }
inline COLORREF TRACK()  { return g_theme == Theme::Dark ? CLR_TRACK_DARK : CLR_TRACK_LIGHT; }

constexpr int DWMA_DARK     = 20;
constexpr int DWMA_CORNER   = 33;
constexpr int DWMA_BACKDROP = 38;
constexpr int CORNER_ROUND  = 2;
constexpr int BACKDROP_MAIN = 2;
