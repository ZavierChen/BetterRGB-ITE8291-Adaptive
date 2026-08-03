# 授权与第三方组件

## BetterRGB

项目源自 Better RGB for Tongfang/ITE8291，按包内 `licenses/BetterRGB_MIT_LICENSE.txt` 和相关上游许可文件处理。交付或再发布前，接收方应再次核对上游仓库的著作权声明与商标使用要求。

## hidapi 0.16.0

- 用途：Windows HID 枚举、Feature report 与 Output report。
- 包含文件：`third_party/hidapi/hidapi/hidapi.h`、`third_party/hidapi/windows/`。
- 许可证：BSD 3-Clause，见 `licenses/hidapi_BSD-3-Clause.txt`。
- 本项目对 Windows 打开策略有适配，审阅时应对照 `reference/changes-from-6287317.patch`。

## Windows SDK/API

程序使用 Windows 系统提供的 HID、SetupAPI、WMI/COM、Core Audio、PDH、Shell、GDI/User32 等接口，仅动态链接系统 DLL，不随包再分发微软运行库文件。

## 商标与硬件

机械革命、Tongfang、ITE、Windows 等名称仅用于兼容性描述，权利归各自所有者。本包不代表上述厂商官方发布或背书。

## 发布前清单

- 保留 BetterRGB 和 hidapi 许可文本。
- 不删除源码文件头中的版权声明。
- 不将用户日志、设备实例路径、密钥或签名证书打包。
- 若加入新的库，记录精确版本、来源、修改内容和许可证。
- 若进行商业分发，由法务确认最终授权义务；本文不是法律意见。
