# BetterRGB ITE8291 Adaptive

面向同模具游戏本的独立 Windows RGB 键盘控制器，适用于采用兼容 ITE8291 HID 接口的单键 RGB 键盘。

项目通过用户态 HID 实时输出逐键灯光帧，在不修改 BIOS、EC 固件或系统驱动的前提下，改善原厂动态灯效卡顿，并提供更完整的灯效、电源和过渡控制。

> 当前源码与应用版本：**3.6.1**。本版修复默认播放设备变化后音量 Bar 临时覆盖失效的问题，并包含 3.6.0 的三通道白色光谱校准。GitHub Releases 中现有的最新预编译公开包仍为 v3.4.5；需要 3.6.1 时请从 `main` 分支按下文构建，或等待新的 Release 包。

> 当前实机验证：机械革命苍龙 16 X Pro，`VID_048D&PID_600B`，Usage Page `0xFF03`，MI_01。其他同模具机型请先核对硬件 ID 和键位映射。

## 主要功能

- 16 种逐键灯效，包括呼吸、波浪、星光、响应、色轮、闪电、火焰、雨滴、矩阵、静态、涟漪、彩虹、流沙、电流、触控电流和同步辨色。
- 3.6.0 白色光谱校准：输出未经校正的满亮度纯白，根据肉眼观察到的实际颜色在线性光空间反算 R/G/B 独立增益，并支持持久化与一键重置。
- 3.6.1 音量 Bar 修复：默认播放设备切换后自动重新绑定音量监测，调节音量仍会临时显示音量并在 1.8 秒后恢复原指标。
- 插电与电池使用两套独立模式、亮度、发送上限和 Touch Bar 配置。
- 启动、关闭、停止、空白恢复和定时休眠过渡，支持渐亮、方向涟漪与双蛇形漩涡路径。
- 无输入自动休眠，恢复操作后平滑点亮。
- Fn+F6/F7 直接调整 BetterRGB 亮度。
- 电量、音量、麦克风、CPU、GPU 和音频可视化条，调节音量时自动临时显示音量。
- 独占 RGB HID 接口，阻止原厂灯光服务周期性覆盖当前灯效。
- 断线重连、慢帧降速、托盘运行、配置持久化与计划任务自启。
- 简体中文 Win32 原生界面，不依赖第三方 UI 框架。

Fn+Space 当前未适配，请在程序界面中切换灯效。

## 下载与使用

1. 从 GitHub Releases 下载 Windows x64 压缩包并解压；当前预编译公开包为 v3.4.5。需要 3.6.1（含白色光谱校准和音量 Bar 修复）时，请从 `main` 分支自行构建或等待新的 Release 包。
2. 退出原厂控制中心的灯光控制，运行 BetterRGB。
3. 首次接管可能需要管理员权限，以释放并独占 RGB HID 接口。
4. 若要恢复原厂灯光，正常退出 BetterRGB，并重新启动原厂控制中心或重启系统。

程序未签名，Windows SmartScreen 或部分安全软件可能提示未知发布者。请从本仓库 Releases 下载并核对 SHA-256；不信任预编译程序时可按下文自行构建。

## 兼容性

| 硬件/环境 | 状态 |
|---|---|
| 机械革命苍龙 16 X Pro | 已完成实机验证 |
| ITE `048D:600B` / Usage Page `FF03` / MI_01 | 已验证 |
| 其他同模具、相同 HID 接口机型 | 实验性支持，需核对键位映射 |
| 其他 ITE PID 或接口 | 默认不支持，禁止盲目发送厂商命令 |
| Windows 11 x64 | 已验证 |

“同模具”不等于所有批次协议完全相同。新增机型前必须确认 VID、PID、Usage Page、接口号和 512 字节键位布局。

## 工作原理

```text
灯效线程
  -> 512 字节逐键 RGB 帧
  -> 指标覆盖、过渡蒙版、白色光谱校准和亮度缩放
  -> 8 个 64 字节 HID 数据块
  -> ITE8291 RGB 接口
  -> 键盘 LED
```

程序只发送运行态 HID Feature/Output report，不读写 EC RAM，也不刷写 BIOS、EC Flash 或键盘固件。协议说明见 [`docs/RGB_PROTOCOL_ZH.md`](docs/RGB_PROTOCOL_ZH.md)。

## 从源码构建

已验证工具链为 64 位 LLVM-MinGW。设置工具链目录后执行：

```powershell
$env:LLVM_MINGW_BIN = "C:\path\to\llvm-mingw\bin"
PowerShell -ExecutionPolicy Bypass -File .\build\build.ps1
PowerShell -ExecutionPolicy Bypass -File .\build\verify_binary.ps1 -Executable ".\out\Better RGB Adaptive v3.6.1.exe"
```

构建脚本会重新编译第一方源码、资源和随仓库提供的 hidapi Windows 后端，不复用旧对象文件。完整环境见 [`docs/BUILD_ENVIRONMENT_ZH.md`](docs/BUILD_ENVIRONMENT_ZH.md)。

## 文档

- [中文使用手册](MANUAL_ZH.md)
- [架构与控制流程](docs/ARCHITECTURE_ZH.md)
- [HID 协议与接口](docs/RGB_PROTOCOL_ZH.md)
- [兼容性矩阵](docs/COMPATIBILITY_MATRIX.md)
- [已知问题](docs/KNOWN_ISSUES_ZH.md)
- [日志与故障排查](docs/LOGGING_TROUBLESHOOTING_ZH.md)
- [回滚与恢复](docs/ROLLBACK_RECOVERY_ZH.md)
- [开发路线](docs/ROADMAP_ZH.md)
- [变更记录](docs/CHANGELOG.md)

## 开源致谢

项目早期协议与实现基于 TheYamo 的开源项目 [Better RGB for Tongfang/ITE8291](https://github.com/EpicYamo/Better-RGB-for-Tongfang_ITE8291)，此后经过多个版本的独立硬件适配、灯效引擎、中文 UI、电源策略、过渡系统、指标显示、快捷键和稳定性重构。感谢原作者公开基础研究成果。

hidapi 由其原项目维护者提供，本仓库保留 BSD 3-Clause 与原始许可证文本。

## 许可证与风险

本项目按 MIT License 发布，并保留上游版权声明；hidapi 按其 BSD 3-Clause 许可使用。详见 [`LICENSE`](LICENSE)、[`NOTICE.md`](NOTICE.md) 和 [`licenses/`](licenses/)。

直接与厂商 HID 接口通信存在兼容性风险。本项目与机械革命、同方、ITE 及其控制中心厂商不存在隶属、授权或背书关系，使用前请阅读兼容性与恢复说明。
