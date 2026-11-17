# AutoDark

Ultra-lightweight native Win32 application for Windows 10/11 that automatically switches between **Light** and **Dark** mode (and optionally desktop wallpapers) at sunrise/sunset or on a fixed schedule.

- **Minimal RAM footprint in systray:** ~`0.2 MB` Working Set / ~`2.6 MB` Private Memory
- **Portable executable:** ~`86 KB` (zero external dependencies, written in pure C with the Win32 API)
- **Standalone installer:** ~`157 KB` (`AutoDarkSetup.exe`) with *Start with Windows* (systray) and clean *Installed Apps* uninstallation support

## Features

- **Automatic Sunrise / Sunset:** local astronomical calculation (NOAA solar algorithm) with zero network requests or location permissions, including 20 built-in city presets.
- **Fixed Schedule:** quick 15-minute steppers for Light and Dark transition times.
- **Optional Wallpaper Switching:** preconfigured for `C:\Windows\Web\Wallpaper\Windows\img0.jpg` (Light) and `img19.jpg` (Dark), customizable with any image.
- **Quick Theme Switch:** instant `Light` / `Dark` buttons in the popup and systray context menu.
- **Minimal Adaptive UI:** Windows 11 rounded-corner popup that adapts to the active Light/Dark theme with smooth fade-in/fade-out and animated toggles.

## Installation

Download `AutoDarkSetup.exe` from [Releases](../../releases) and run it.

## Building from Source

Requirements: [Zig](https://ziglang.org/) (`zig cc`) on Windows.

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

Outputs `build/AutoDark.exe` and the self-contained `build/AutoDarkSetup.exe` installer.
