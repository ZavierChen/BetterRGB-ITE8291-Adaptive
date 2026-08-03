# BetterRGB 技术交接包 v2.0

本包用于机械革命产品、驱动、固件及研发团队复现 BetterRGB 的用户态灯光控制方案。应用内部版本为 3.4.5，交接包版本为 2.0，两者含义不同。

## 快速入口

1. 阅读 `docs/ARCHITECTURE_ZH.md` 和 `docs/RGB_PROTOCOL_ZH.md`。
2. 安装 64 位 LLVM-MinGW，设置环境变量 `LLVM_MINGW_BIN` 指向其 `bin` 目录。
3. 在 PowerShell 中运行 `build/build.ps1`。
4. 运行 `build/verify_binary.ps1 -Executable out/Better RGB Adaptive v3.4.5.exe`。
5. 在受支持测试机上先退出官方控制中心灯光服务，再运行生成的 EXE。

## 目录

- `source/`：BetterRGB 一方源码、资源和清单。
- `third_party/hidapi/`：构建所需 hidapi 0.16.0 Windows 源码。
- `binary/`：已验证的 64 位 Windows 可执行程序。
- `build/`：可复现构建、二进制校验和包审计脚本。
- `docs/`：架构、协议、测试、兼容性、风险及后续路线。
- `samples/`：脱敏配置与日志示例，不会被程序自动载入。
- `reference/`：上游说明、历史手册及相对基线提交的补丁。
- `licenses/`：项目及第三方许可文本。

## 安全边界

本程序只通过 Windows HID 用户态接口发送 RAM 中的实时灯光帧，不写 BIOS、EC Flash 或键盘固件。当前实现对 HID 接口采用独占打开，官方控制中心与本程序不能同时控制同一接口。任何硬件适配必须先验证 VID、PID、Usage Page、接口号和帧布局，禁止仅凭品牌名放宽匹配条件。

## 当前结论

苍龙 16 X Pro 测试机可通过 `VID_048D&PID_600B`、Usage Page `0xFF03` 的 HID 接口接收实时 512 字节 RGB 帧。第三方逐帧驱动后动画明显流畅，说明灯珠及传输路径具备高于原厂预设灯效的表现空间；这不等同于证明 EC 任意刷新率均安全，也不应据此修改固件。
