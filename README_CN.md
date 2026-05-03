[English](README.md) | [中文](README_CN.md)

# AutoClicker

一个轻量级 Windows 连点器，采用现代暗色 UI，基于 C++20 和 GDI 开发。

## 功能

- **鼠标左/右键连点**：可分别调节 CPS（每秒点击次数）
- **多倍点击模式**：可配置倍数（1-5 倍）和间隔延迟（1-200ms）
- **自定义快捷键**：一键绑定，快速启停连点器
- **保持模式**：无需长按快捷键即可自动连点
- **暗色主题 UI**：流畅动画，现代化外观
- **极致轻量**：除 Windows SDK 外无任何外部依赖

## 环境要求

- Windows 10 或更高版本
- Visual Studio 2022（v143+ 工具集），安装 C++ 桌面开发工作负载

## 构建

1. 用 Visual Studio 2022 打开 `AutoClicker.sln`
2. 选择 `Release | x64` 配置
3. 生成 → 生成解决方案（Ctrl+Shift+B）

或通过命令行构建：

```powershell
msbuild AutoClicker.sln /p:Configuration=Release /p:Platform=x64
```

## 使用说明

- **左键/右键**：分别开关左/右键连点，通过滑块调节 CPS
- **快捷键**：点击快捷键按钮绑定按键，用于启停连点器
- **保持模式**：开启后无需按住快捷键即可持续连点
- **多倍模式**：每次按下触发多次点击，可调节倍数和延迟

## 许可证

MIT
