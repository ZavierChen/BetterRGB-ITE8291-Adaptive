# 代码导读与维护约定

## 主要入口

- `WinMain`：解析启动参数、加载配置、建立单实例、初始化 HID/WMI/UI 并恢复灯效。
- `resolve_device_path`：枚举和筛选 ITE HID 接口。
- `enter_custom_mode`：发送两段 Feature report，使设备接受逐帧 RGB 数据。
- `send_frame`：唯一的帧合成与 HID 提交出口。
- `start_mode`：停止旧灯效、重置设备状态并启动新灯效线程。
- `show_panel_for_mode`：只显示当前模式对应的 UI 参数。
- `refresh_touchbar_option_visibility`：按 Touch Bar 类型显示指标或音频参数。

## 新增灯效

1. 在模式枚举、名称和线程函数映射中加入新模式。
2. 编写返回 `DWORD WINAPI` 的工作线程，循环条件必须检查 `g_stopFlag`。
3. 每帧先清零 512 字节缓冲，按键位偏移写 RGB，最后只调用 `send_frame`。
4. 使用统一节拍器，禁止高频忙等。
5. 新参数加入默认值、保存、读取、范围钳制和 UI 动态显示。
6. 检查插电/电池切换、空白恢复、休眠和关机过渡。

## 修改协议

协议改动必须集中在设备发现、`enter_custom_mode` 或 `send_frame`，不得让各灯效直接调用 hidapi。新增 PID 应使用白名单并附实机抓包证据。任何可能写入固件的命令都不应合并到本项目。

## 注释原则

交接源码在关键层边界增加了中文导读注释；算法内部只解释非直观约束。避免逐行翻译代码，优先记录协议来源、线程所有权、单位、边界和失败策略。
