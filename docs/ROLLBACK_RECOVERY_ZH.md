# 回滚与恢复

## 恢复官方控制中心

1. 在 BetterRGB 托盘菜单中正常退出，让关闭过渡执行完毕。
2. 取消 BetterRGB 的开机自启；或以管理员 PowerShell 运行：

```powershell
schtasks /End /TN "Better RGB by TheYamo"
schtasks /Delete /TN "Better RGB by TheYamo" /F
```

3. 确认没有 `Better RGB Adaptive v3.4.5.exe` 残留进程。
4. 启动官方控制中心；必要时启动/重启其灯光桥接服务 `GCUBridge`，或重启 Windows。

## 重置 BetterRGB

正常退出后，将 EXE 同目录的 `better_rgb.cfg` 改名备份，再启动程序即可生成默认配置。`rgb_engine_log.txt` 可安全归档或删除，不影响灯效设置。

## 完整卸载

取消计划任务、退出程序后删除 BetterRGB 文件夹即可。本程序不安装驱动、不写 EC/BIOS 固件、不修改系统 HID 驱动。保留官方控制中心本体与厂商 ACPI/WMI 驱动。

## 故障状态

若灯光黑屏但系统可用，先完全关机，断开电源数十秒后再开机，让 EC 完成冷启动；随后只启动官方控制中心验证。不要在故障状态尝试未知 Feature report、EC 端口写入或固件刷写。

## 开发回滚点

交付前完整项目和工作安装已备份到项目外层 `backup/pre-package-20260803-051428`。该备份未放入交接 ZIP，以避免把个人运行日志、历史构建物或机器路径传播给第三方。
