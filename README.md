# AutoClicker

A lightweight Windows auto-clicker with a modern dark UI, built with C++20 and GDI.

## Features

- **Left/Right click automation** with adjustable CPS (clicks per second)
- **Multi-click mode** with configurable multiplier and delay
- **Customizable hotkeys** for quick start/stop
- **Keep-click mode** - auto-click without holding a key
- **Dark themed UI** with smooth animations
- **Minimal footprint** - no external dependencies beyond Windows SDK

## Requirements

- Windows 10 or later
- Visual Studio 2022 (v143+ toolset) with C++ Desktop Development workload

## Build

1. Open `AutoClicker.sln` in Visual Studio 2022
2. Select `Release | x64` configuration
3. Build → Build Solution (Ctrl+Shift+B)

Or build from command line:

```powershell
msbuild AutoClicker.sln /p:Configuration=Release /p:Platform=x64
```

## Usage

- **Left/Right**: Toggle clicker for left/right mouse button, set CPS via slider
- **Hotkey**: Press the hotkey button to bind a new key, use it to start/stop the clicker
- **Keep mode**: Enable to auto-click without holding the hotkey
- **Multi mode**: Click multiple times per press with configurable multiplier (1-5x) and delay (1-200ms)

## License

MIT
