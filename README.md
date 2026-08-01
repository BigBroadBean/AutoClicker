[English](README.md) | [中文](README_CN.md)

# AutoClicker

A lightweight Windows auto-clicker with a **sidebar navigation + Neumorphism UI**, built with C++20 and GDI. Dark & light themes included.

- **Landscape window**: 640×480 classic 4:3 aspect ratio, two-column card layout
- **Light theme by default**: soft light neumorphism, dark theme one click away
- **High-performance clicking**: 1ms system timer + sub-millisecond precise sleep (fine spin), click interval error < 0.1ms, exact CPS; ~0% idle CPU
- **Modern fonts**: auto-picks the nicest CJK font installed (Noto Sans SC > HarmonyOS Sans SC > MiSans > Source Han Sans > Microsoft YaHei), Latin glyphs benefit too
- **Soft visuals**: buttons get a layered rounded glow on hover; all surfaces cast soft shadows (exact region geometry, corners perfectly aligned)
- **IME-hook resistant**: window title set via ANSI APIs; clicks sent through dynamically-resolved PostMessage to bypass IME IAT hooks

## Features

- **Sidebar navigation**: Click / Multi / Scroll / Advanced pages, all features configurable independently
- **Left/Right click automation** with adjustable CPS and live delay readout
- **CPS presets**: one-click 6 / 10 / 15 / 20 clicks per second
- **Multi-click mode** with configurable multiplier (1-5x) and delay (1-200ms), plus 2x/3x/4x/5x and 10/25/50/100ms presets
- **Scroll-to-click**: converts wheel scrolls into left/right clicks
- **Random CPS** jitter to mimic human behavior
- **CPS limit** to prevent clicking too fast (type a value directly)
- **Auto-stop timer**: stops the clicker after N seconds
- **Realtime CPS readout**: bottom-right chip shows the live click rate (1s sliding window)
- **Custom hotkeys**: press any key to bind (Esc cancels), with guide toast
- **Keep-click mode**: auto-click without holding the hotkey
- **Always-on-top** pin button
- **Neumorphism UI**: soft extruded/inset surfaces, dark & light themes, theme-aware toasts
- **Responsive window**: resizable, cards scale to fit small screens
- **Keyboard navigation**: arrow keys switch pages
- **Minimal footprint** - no external dependencies beyond Windows SDK

## Requirements

- Windows 10 or later
- Visual Studio 2022+ (v143+ toolset) with C++ Desktop Development workload

## Build

1. Open `AutoClicker.sln` in Visual Studio
2. Select `Release | x64` configuration
3. Build → Build Solution (Ctrl+Shift+B)

Or build from command line:

```powershell
msbuild AutoClicker.sln /p:Configuration=Release /p:Platform=x64
```

## Usage

- **Sidebar**: click the icons to switch pages (Click / Multi / Scroll / Advanced), or use arrow keys
- **Click page**: left/right toggles + CPS sliders + presets + clicker hotkey + keep mode
- **Multi page**: multiplier/delay sliders + presets + multi hotkey
- **Scroll page**: scroll-click toggle + left/right selector + two hotkeys
- **Advanced page**: CPS limit, random CPS, auto-stop timer
- **Hotkeys**: click a hotkey button, then press any key to bind (Esc cancels)
- **Counter**: shows total clicks of the session; click it to reset

## License

MIT
