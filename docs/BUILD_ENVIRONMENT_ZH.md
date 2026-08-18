# 构建环境

## 已验证环境

- Windows 11 Pro x64，版本 10.0.26200。
- LLVM-MinGW `20260616-ucrt-x86_64`。
- C11、Win32 API、hidapi 0.16.0 Windows 后端。
- 输出架构：x86-64，GUI 子系统，无控制台窗口。

## 构建步骤

```powershell
$env:LLVM_MINGW_BIN = "C:\path\to\llvm-mingw\bin"
PowerShell -ExecutionPolicy Bypass -File .\build\build.ps1
PowerShell -ExecutionPolicy Bypass -File .\build\verify_binary.ps1 -Executable ".\out\Better RGB Adaptive v3.6.0.exe"
```

也可将工具链目录直接传给脚本：

```powershell
.\build\build.ps1 -ToolchainBin "C:\path\to\llvm-mingw\bin" -OutputDirectory ".\out-clean"
```

脚本从源码重新生成 `resource.o`、`better_rgb.o` 和 `hid.o`，不会复用包外对象文件。第一方代码启用 `-Wall -Wextra -Wpedantic -Werror`；hidapi 0.16.0 的 Windows 后端保留三个上游旧式空参数原型警告，因此第三方文件不启用 `-Werror`。

## 链接依赖

`setupapi`、`hid`、`comdlg32`、`comctl32`、`shell32`、`advapi32`、`ole32`、`oleaut32`、`wbemuuid`、`pdh`、`gdi32`、`user32`、`kernel32` 和数学库。已发布 EXE 仅导入 Windows 系统 DLL，不需要随包分发 LLVM 或 Visual C++ Runtime DLL。

## 可复现范围

同一源码和工具链可稳定生成可运行程序。PE 时间戳、链接器版本或工具链升级可能使 SHA-256 不同，因此应同时校验文件版本、导入表、功能测试和源码提交状态，不应只比较哈希。
