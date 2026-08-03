# RGB 协议与接口

## 设备发现

- USB/HID 厂商 ID：`0x048D`（ITE）。
- 已验证产品 ID：`0x600B`。
- 首选接口：Usage Page `0xFF03`。
- 回退接口：HID interface number `1`，设备路径通常含 `MI_01`。
- 测试机还存在 `MI_00`，但不是本程序选择的 RGB 数据接口。

枚举逻辑会为 Usage Page、接口号和已知 PID 打分，选择分数最高的候选。新增机型时必须采集完整 HID 枚举日志，不能删除 Usage Page/接口约束后盲试。

## 进入自定义模式

按以下顺序发送：

```text
Feature report: 00 12 00 03 00 00 00 00 00
Output report : 65 字节全零（第 1 字节为 Report ID 0）
Feature report: 00 08 02 33 00 32 00 00 00
```

这组命令只切换当前运行态控制模式，未发现固件刷写行为。

## 提交一帧

1. 生成长度 `512` 的逻辑帧，每个键使用固定偏移处的 RGB 三字节。
2. 发送 Feature report：`00 12 00 00 08 00 00 00 00`。
3. 将 512 字节拆为 8 个 64 字节块。
4. 每块前加 Report ID `00`，以 65 字节 Output report 依次写入。

颜色帧中的键位偏移由 `g_keys`/相关键位表定义。键盘不是规则矩阵，左 Shift、Enter、空格、方向键等宽键必须以实机映射验证，不能简单按行列推导。

## 其他 Windows 接口

- HID：实时灯光的主要数据面，使用 hidapi/SetupAPI/HIDClass。
- WMI：订阅厂商 Fn 热键事件，已观察扫描码 `0xB1`/`0xB2` 用于背光减小/增大。Fn+Space 当前未适配。
- 电源与音频：使用 Win32 电源状态和 Core Audio。
- PDH：采集 CPU/GPU 指标。
- ACPI/EC I/O：当前应用不直接读写 ACPI EC RAM，也不访问 `0x62/0x66` 等 EC 端口。

## 抓包建议

优先使用 USBPcap/Wireshark 对比官方控制中心和本程序切换静态、彩虹及亮度时的数据。抓包前停止其中一个控制程序，避免独占冲突。记录 VID/PID、接口、Feature report、Output report 长度与时间间隔，脱敏设备实例序列后再共享。
