# ISP_Downloader C/Win32

C + Win32 上位机，与 Qt 工程并行。体积约 200+ KB，仅系统 DLL。

## 构建
```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## VS Code 一键编译
工程已带 `.vscode/` 配置，VS Code 打开本目录即可用。工具链使用 `D:/Qt/Tools` 下的
CMake 3.30 + MinGW-w64 GCC 13.1 + Ninja；若安装路径不同，需同步修改
`tasks.json`、`c_cpp_properties.json`、`settings.json` 中的路径。

- **编译**：`Ctrl+Shift+B` → 默认 `Build (Release)`，产物 `build/ISP_Downloader_C.exe`
- **调试**：`F5` → 自动构建 `Build (Debug)`（`build-debug/`，带符号）并用 gdb 断点调试；CLI 模式调试选第二个配置
- **其他任务**：Rebuild (Release) / Clean (Release) / Build (Debug)，在终端 → 运行任务中可选
- 智能提示/跳转由 cpptools 按 `c_cpp_properties.json` 提供；推荐安装插件 cpptools、cmake-tools（打开工程时会提示）


## 功能（v0.3 / P0–P5）
- 串口 8E1、DTR/RTS 模式 0–16、状态指示
- AN3155 协议与下载队列（解保护→擦除→重入→写→[校验]→[运行]）
- HEX/BIN 解析、地址校验；完整 ChipDB（含 F429 PID 0x419）
- **选项字节**读写（F1 反码校验；F4 地址按 ChipDB）
- **读取 Flash** 导出 Intel HEX
- **调试面板**（同步/GET/擦除/保护/GO/手动 hex/DTR/RTS）
- 日志 RichEdit 白底黑字、导出日志、进度与取消
- 文件菜单：刷新串口、导出日志

## 修改代码
- VS Code / Qt Creator 打开本目录 `CMakeLists.txt`
- 界面：`src/ui_main.c`；协议：`src/an3155.c`；业务：`src/app_logic.c`

## 与 Qt 版关系
功能对齐方向与 `docs/Win32_C_Migration.md` 一致；产线可用本 C 版，深度调试仍可对照 Qt 版。
