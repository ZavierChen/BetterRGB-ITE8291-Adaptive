# 项目结构

3.7.0 开始渐进模块化，各模块独立编译并链接：

- `source/i18n.c` / `i18n.h`：默认语言、语言选择和双向文本查找。
- `source/ui_strings.def`：中文/英文翻译目录，格式占位符由测试检查。
- `source/color_math.c` / `color_math.h`：sRGB/线性光转换及三通道白色校准。
- `tests/ui_smoke.c` / `tests/run.ps1`：不连接 HID 的界面与配置回归、颜色计算和翻译覆盖检查。

其余协调逻辑仍集中在 `source/better_rgb.c`，后续可逐步拆出设备、配置和灯效：

- 全局状态与配置：灯效参数、插电/电池配置、过渡、休眠和 Touch Bar 指标。
- HID 设备层：枚举 ITE 设备、选择 RGB 接口、独占打开、重连及发送帧。
- 灯效引擎：每个模式一个工作线程，生成统一的 512 字节键盘帧。
- 合成层：主灯效、Touch Bar 覆盖、启动/关闭/休眠蒙版、亮度缩放。
- 系统指标层：电源状态、音量、麦克风、CPU/GPU、音频峰值。
- 输入层：原始键盘输入及 WMI Fn 热键监听。
- UI 层：Win32 控件、托盘、按当前模式动态显示参数。
- 生命周期：单实例、计划任务自启、设备断线重连和渐变退出。

`resource.rc` 定义版本、图标和 manifest；`resource.h` 保存资源标识；`app.manifest` 声明 Windows 应用属性。`third_party/hidapi` 是唯一直接随源码交付的第三方实现。
