# BetterRGB ITE8291 Adaptive

English | [简体中文](README.md)

A native Windows controller for compatible Tongfang-style laptops with ITE8291 per-key RGB keyboards. It streams lighting frames through the user-mode HID interface, without flashing BIOS, EC or keyboard firmware.

**Version 3.7.0** adds a persistent Chinese/English language selector, updates the upstream references, and separates localization and color calculations into modules. It includes the volume Bar fix from 3.6.1 and per-channel white calibration from 3.6.0.

## Features

- 16 effects: Breathing, Wave, Sparkle, Reactive, Color Wheel, Lightning, Flame, Rain, Matrix, Static, Reactive Ripple, Rainbow, Quicksand, Current, Reactive Current and Screen Ambient.
- Separate AC and battery profiles for effects, brightness, RGB frame rate and the top-row Bar.
- A top-row Bar for battery level, system volume, microphone, CPU, GPU or playback audio. Volume changes temporarily show volume for 1.8 seconds, then restore the configured metric.
- Smooth startup, exit, stop, idle and wake transitions, including fades, directional ripples and twin spirals.
- Idle timeout and Fn+F6/F7 brightness control. Fn+Space is not supported on the tested machine.
- White calibration with independent R/G/B gains, applied to effects, the Bar and transitions.
- Screen Ambient maps a blurred view of the main display onto the keyboard; frames stay in memory and are not saved.
- Tray operation, start at login, exclusive HID access, reconnection and adaptive RGB frame pacing.

The FPS controls refer to the RGB engine, not game frame rate. Experimental game-FPS capture is disabled in this release.

## Download and run

1. Download `BetterRGB_Adaptive_v3.7.0_Windows_x64.zip` from [Releases](https://github.com/ZavierChen/BetterRGB-ITE8291-Adaptive/releases/latest), verify the SHA-256 checksum and extract the entire folder.
2. Close the vendor control center's lighting controls and launch `Better RGB Adaptive v3.7.0.exe`. Administrator permission may be requested to acquire exclusive RGB access.
3. Open **Settings → Language / 语言 → English**. The change applies immediately and is saved. On first launch, Chinese Windows defaults to Chinese; other display languages default to English. Windows color pickers and permission dialogs may retain the Windows display language.
4. Choose an effect, adjust its parameters, and configure the AC profile, battery profile and top-row Bar in their tabs.

The executable is unsigned; Windows SmartScreen may show an unknown-publisher warning. The portable folder contains `better_rgb.cfg` and `rgb_engine_log.txt` after use. Keep your config when upgrading. Exit the previous instance first; update the start-at-login setting if you move or rename the executable.

## White calibration

Open **Settings → Calibrate white**. The keyboard displays raw full-brightness white. In the color picker, choose the color you actually see on the keyboard, rather than the white you want. BetterRGB computes gains in linear light and attenuates stronger channels. Cancel leaves the previous calibration unchanged; **Reset calibration** restores neutral gains.

## Compatibility and recovery

Validated hardware: Mechrevo 苍龙 16 X Pro, Windows 11 x64, USB `VID_048D&PID_600B`, RGB Usage Page `0xFF03`, interface MI_01. Other models remain experimental: matching chassis or branding does not guarantee the same protocol or key mapping.

BetterRGB holds the RGB interface exclusively. To restore vendor lighting, use **Exit** from the tray menu, disable BetterRGB's start-at-login task if enabled, and restart the vendor control center or Windows. Avoid running competing RGB controllers together.

## Build and test

Use 64-bit LLVM-MinGW:

```powershell
$env:LLVM_MINGW_BIN = 'C:\path\to\llvm-mingw\bin'
PowerShell -ExecutionPolicy Bypass -File .\build\build.ps1
PowerShell -ExecutionPolicy Bypass -File .\build\verify_binary.ps1 -Executable '.\out\Better RGB Adaptive v3.7.0.exe'
PowerShell -ExecutionPolicy Bypass -File .\tests\run.ps1 -ToolchainBin $env:LLVM_MINGW_BIN -BuildDirectory .\out
```

First-party modules are built with warnings treated as errors. The UI smoke test creates test windows and uses files beside its test executable; it does not initialize HID or start lighting workers. It exercises both languages, config persistence, profile preservation, calibration and color conversion.

Localization lives in `source/i18n.c` and `source/ui_strings.def`; linear-light conversion and calibration live in `source/color_math.c`. The remaining application is still in `source/better_rgb.c`; modularization is incremental.

## History, credits and license

The [complete changelog](docs/CHANGELOG.md) retains the history from v1.0 onward, with experimental builds explicitly labeled. Additional technical documentation and the [Chinese manual](MANUAL_ZH.md) are available in the repository.

This project derives from TheYamo (EpicYamo)'s MIT-licensed [Better RGB for Tongfang/ITE8291](https://github.com/EpicYamo/Better-RGB-for-Tongfang-ITE8291). The new upstream uses a modular layout, CMake and GitHub Actions; the [old repository](https://github.com/EpicYamo/Better_RGB_Old-Repo) is archived. Thank you to TheYamo for the original work and the suggestions in [issue #5](https://github.com/ZavierChen/BetterRGB-ITE8291-Adaptive/issues/5).

BetterRGB Adaptive retains the upstream MIT notices. Bundled hidapi is subject to its own BSD 3-Clause terms. See [LICENSE](LICENSE), [NOTICE.md](NOTICE.md) and [licenses](licenses/). This project is not affiliated with or endorsed by Mechrevo, Tongfang, ITE or their control-center vendors.
