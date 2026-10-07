# STM32 ISP Downloader (C/Win32)

与 STM32 片内 ROM Bootloader（AN3155 协议）对接的串口下载上位机。纯 C + Win32 API 实现，
单文件 exe 约 150 KB，静态链接，仅依赖系统 DLL。

- 版本：v0.6
- 运行平台：Windows 7 及以上（x64）
- 仓库：https://gitee.com/xiexinsui/stm32-isp-bootloader.git

## 一、当前实现的功能

### 1. 串口管理
- 自动枚举本机串口（设备接口枚举），支持手动刷新
- 波特率预设下拉 + 自定义输入；固定 8E1 数据格式（AN3155 协议要求）
- 打开/关闭串口；DTR/RTS 电平手动翻转，实时状态指示

### 2. 自动进入 Bootloader（DTR/RTS 模式 0–16）
- 内置 16 种 DTR/RTS 时序组合，覆盖：RTS 控 NRST + DTR 控制 BOOT0、DTR 控制 NRST +
  RTS 控制 BOOT0、单信号复位、单信号 BOOT0 等常见接法；每步延时可调（默认 100 ms）
- 模式 0：不操作 DTR/RTS，由用户手动将 BOOT0 置 1 并复位后连接
- 下载完成后按对应时序自动恢复，使芯片从 Flash 启动

### 3. AN3155 协议命令
- 已实现：同步(0x7F)、GET(0x00)、GET Version(0x01)、GET ID(0x02)、Read Memory(0x11)、
  Go(0x21)、Write Memory(0x31)、Erase(0x43)、Extended Erase(0x44)、
  Write Unprotect(0x73)、Readout Unprotect(0x92，触发全片擦除)、读取 Flash 容量寄存器
- 全程支持超时控制与中途取消

### 4. 芯片识别
- 内置芯片数据库（94 条型号）：覆盖 STM32 C0 / F0 / F1 / F2 / F3 / F4 / F7 / G0 / G4 /
  H5 / H7 / L0 / L1 / L4 / L5 / U5 / WB / WL 系列
- 按 GET ID 返回的 PID 识别型号，并读取 Flash 容量寄存器实测细分容量；F429（PID 0x419）已收录
- 连接后显示：PID、型号、系列、Flash 容量、Bootloader 版本、读保护状态（RDP 等级判断）

### 5. 固件下载
- 文件格式：Intel HEX（自带地址信息）、BIN（需指定起始地址，默认 0x08000000）
- 下载前地址校验：起始地址不得低于 0x08000000，结束地址不得超出芯片 Flash 容量
- 一键下载流程：进 Bootloader → 识别芯片 →（检测到写保护则先解除）→ 全片擦除 →
  重入 Bootloader → 逐页写入 →（可选）逐页回读校验 →（可选）退出并运行
- 擦除失败自动解除写保护并重试一次；写入实时进度显示，可随时取消
- 擦除、校验（与固件文件回读比对）、运行三个操作也可单独执行

### 6. 选项字节与 Flash 回读
- 选项字节读取/写入（16 字节；地址按芯片库查询，F1 系列默认 0x1FFFF800，写后需复位生效）
- 整片 Flash 读取并导出为 Intel HEX 文件（256 字节分块读取，带进度与取消）

### 7. 调试面板
- 单步命令按钮：同步、GET、GET ID、GET Version、标准擦除、扩展擦除、GO、
  解写保护、解读保护（警告将全片擦除）、读 Flash 容量寄存器
- 手动发送任意十六进制帧并显示应答，可用于协议级调试

### 8. 日志与界面
- 主日志窗口（[OK] / [!] / [ERR] 分级显示），可导出日志文件
- 协议 HEX 日志面板（完整收发明细，上限 2500 行，可导出 txt）
- 下载进度条 + 状态栏；中/英文界面切换；跟随系统深/浅色主题
- 下载、擦除、校验、运行、调试、取消均支持快捷键

### 9. 产线 CLI 模式
- 带命令行参数启动 exe 即进入命令行模式（不弹 GUI），输出参数摘要与烧录进度
- 退出码：0 = 成功，1 = 失败，2 = 参数错误，便于产线脚本判断
- 自动在 exe 同目录生成 `ISP_CLI_v0.6_日期_时间.log`，内含完整协议收发记录
- 详细用法见下文《二、用 CMD 烧录固件》

### 10. 工程与构建
- CMake + Ninja + MinGW-w64（GCC 13.1）构建；VS Code 一键编译（Ctrl+Shift+B）与 F5 断点调试
- Release 产物 `-static` 静态链接；GUI 与 CLI 为同一个 exe

## 二、用 CMD 烧录固件（产线 / 脚本）

### 2.1 工作原理
STM32 出厂时 ROM 内固化了串口 Bootloader。将 **BOOT0 拉高并复位**，芯片即进入该
Bootloader，等待主机通过 115200-8E1 串口发送 AN3155 命令完成擦写。
本工具支持两种进入方式：

1. **自动**（推荐）：通过 USB 转串口芯片的 DTR/RTS 信号自动完成“复位 + BOOT0 置位”时序，
   用 `--mode` 选择与硬件接线匹配的模式（见 2.3）；
2. **手动**：`--mode 0` 跳过时序控制，人工将 BOOT0 拨到 1、按复位键后再执行命令。

烧录完成、`--run` 生效时，工具会自动恢复 BOOT0 电平并复位释放芯片，程序立即从 Flash 运行。

### 2.2 准备工作
1. 安装 USB 串口驱动（本控制板为 CH340），在设备管理器确认 COM 号（示例：COM8）；
2. 串口线已连接、板子已供电；
3. 打开 CMD，进入 exe 所在目录（发布目录为 `dist`，重新编译后新 exe 在 `build`）：

```bat
cd /d E:\XXS\PCB\STM32F429IIT6-ControlBoard\BootloaderUpgrade\ISP_C_V1.3\dist
```

### 2.3 命令格式与全部选项

```
ISP_Downloader_C.exe --port COM8 --file 固件路径 [选项]
```

| 选项 | 说明 | 默认值 |
|---|---|---|
| `--port` / `-p` | 串口号，如 `COM8` | **必填** |
| `--file` / `-f` | 固件路径（.hex 或 .bin），含空格需加英文双引号 | 下载时必填 |
| `--baud` / `-b` | 波特率 | 115200 |
| `--mode` / `-m` | DTR/RTS 进 Bootloader 模式 0–16 | 1 |
| `--delay` | 时序步骤延时（ms） | 100 |
| `--bin-addr` | BIN 烧录起始地址，支持 0x 前缀（HEX 文件忽略此项） | 0x08000000 |
| `--verify` | 逐页回读校验 | 默认开启 |
| `--run` | 下载完成后退出 Bootloader 并运行 | 默认开启 |
| `--no-run` | 下载完成后不运行 | |
| `--opt-read` | 仅连接并读取选项字节（调试用，不下载） | |
| `--help` / `-h` | 显示帮助（本版本帮助内容写入日志文件而非控制台） | |

**本控制板已验证参数：COM8、115200、模式 1、延时 100 ms。**
模式 1 = RTS 先反向（NRST 复位）→ DTR 拉低（BOOT0=1）→ RTS 回稳态释放复位。
若模式 1 进不了 Bootloader（提示连接失败/无 ACK），可尝试 `--mode 2` 并适当增大 `--delay`。

### 2.4 烧录 HEX 固件（推荐）
HEX 自带地址信息，不需要 `--bin-addr`：

```bat
ISP_Downloader_C.exe --port COM8 --baud 115200 --mode 1 --delay 100 --file D:\XXS\Desktop\ELoad-Program.hex --verify --run
```

### 2.5 烧录 BIN 固件
BIN 文件没有地址信息，**必须**用 `--bin-addr` 指定起始地址，且该地址必须与工程链接脚本一致，
否则下载成功但程序无法运行。

由 HEX 转 BIN（可选，用 ARM 工具链的 objcopy）：

```bat
arm-none-eabi-objcopy -I ihex -O binary ELoad-Program.hex ELoad-Program.bin
```

App 位于 Flash 起始 0x08000000 时：

```bat
ISP_Downloader_C.exe --port COM8 --baud 115200 --mode 1 --delay 100 --file D:\XXS\Desktop\ELoad-Program.bin --bin-addr 0x08000000 --verify --run
```

App 烧录在 Bootloader 之后（例如 0x08010000）时：

```bat
ISP_Downloader_C.exe --port COM8 --baud 115200 --mode 1 --delay 100 --file D:\fw\App.bin --bin-addr 0x08010000 --verify --run
```

### 2.6 其它常用命令

```bat
:: 查看帮助（内容在新生成的 ISP_CLI_*.log 中）
ISP_Downloader_C.exe --help

:: 只连接并读取选项字节（确认芯片型号与保护状态，不下载）
ISP_Downloader_C.exe --port COM8 --mode 1 --opt-read

:: 下载但不校验、不运行
ISP_Downloader_C.exe --port COM8 --mode 1 --file D:\xx.hex --no-run

:: 模式 1 连不上时换模式 2 再试
ISP_Downloader_C.exe --port COM8 --mode 2 --delay 200 --file D:\xx.hex --verify --run
```

### 2.7 运行结果判断
1. **控制台输出**：首行为参数摘要（Port/Baud/Mode/File），随后按序显示打开串口、识别芯片、
   擦除、`[xx%] 进度`、写入用时、成功/失败；不打印协议 hex 明细（避免刷屏）；
2. **日志文件**：exe 同目录自动生成 `ISP_CLI_v0.6_日期_时间.log`，开头含版本与编译日期，
   文件内含完整协议收发，排障时把此文件发出来即可；
3. **退出码**：`0` = 成功；`1` = 失败（串口/固件/擦除/写入等）；`2` = 参数错误
   （缺 `--port`、缺 `--file`、BIN 未指定 `--bin-addr`）。CMD 中用 `echo %ERRORLEVEL%` 查看。

### 2.8 产线脚本模板

```bat
@echo off
cd /d E:\XXS\PCB\STM32F429IIT6-ControlBoard\BootloaderUpgrade\ISP_C_V1.3\dist
ISP_Downloader_C.exe --port COM8 --mode 1 --file D:\fw\App.hex --verify --run
if %ERRORLEVEL%==0 (echo PASS) else (echo FAIL)
pause
```

### 2.9 常见问题

| 现象 | 排查 |
|---|---|
| 打开串口失败 | COM 号不对；CH340 驱动未装；串口被 GUI 版或其它软件占用（先关闭） |
| 进不了 Bootloader / 连接失败 | `--mode` 与板上 DTR/RTS 接线不匹配（本板用模式 1）；改试 `--mode 2`；增大 `--delay` |
| BIN 下载后不运行 | `--bin-addr` 与链接地址不一致；HEX 固件无需该参数 |
| 固件超出 Flash / 地址校验失败 | 固件与目标芯片不匹配；BIN 地址 + 文件长度超过 Flash 容量 |
| 提示参数不足 | `--port` 必填；下载必须 `--file`；BIN 必须加 `--bin-addr` |

## 三、构建

```bat
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

工具链：`D:/Qt/Tools` 下的 CMake 3.30 + MinGW-w64 GCC 13.1 + Ninja；路径不同请同步修改
`.vscode/tasks.json`、`c_cpp_properties.json`、`settings.json`。

## 四、VS Code 一键编译

工程已带 `.vscode/` 配置，VS Code 打开本目录即可：

- **编译**：`Ctrl+Shift+B` → 默认 `Build (Release)`，产物 `build/ISP_Downloader_C.exe`
- **调试**：`F5` → 自动构建 `Build (Debug)`（`build-debug/`，带符号）并用 gdb 断点调试；
  CLI 模式调试选第二个配置
- **其他任务**：Rebuild (Release) / Clean (Release) / Build (Debug)，在终端 → 运行任务中可选
- 智能提示/跳转由 cpptools 提供；推荐安装插件 cpptools、cmake-tools

## 五、上传到 Gitee 与 GitHub

两个远程仓库同时维护，Git 凭据均已存于 Windows 凭据管理器，推送无需重复登录：

- Gitee：`origin` → https://gitee.com/xiexinsui/stm32-isp-bootloader.git
- GitHub：`github` → https://github.com/xiexinsui/STM32-ISP-BootLoader.git

**日常上传**：VS Code 菜单 终端 → 运行任务 → **Upload to Gitee & GitHub (提交并推送)**，
输入提交说明即可自动完成 `git add -A` + `git commit` + 推送到两个仓库；
也可手动：`git add -A && git commit -m "说明" && git push && git push github master`

## 六、源码结构（src/）

| 文件 | 职责 |
|---|---|
| `win_main.c` | 入口：带 CLI 参数走命令行模式，否则创建 GUI |
| `ui_main.c` | 主窗口界面与控件逻辑 |
| `ui_input.c` | 串口/文件输入区 |
| `ui_debug.c` | 调试面板 |
| `ui_optbytes.c` | 选项字节读写界面 |
| `ui_hexlog.c` | 协议 HEX 日志窗口 |
| `ui_lang.c` | 中英文切换 |
| `app_logic.c` | 业务流程（连接/擦除/下载/校验/运行/回读） |
| `an3155.c` | AN3155 协议实现 |
| `bl_control.c` | DTR/RTS 模式 0–16 进出 Bootloader 时序 |
| `serial_port.c` | Win32 串口（枚举、8E1、DTR/RTS） |
| `flash_prog.c` | 擦除/写入/逐页校验 |
| `hex_parser.c` | Intel HEX 解析与保存 |
| `chipdb.c` | 芯片数据库（PID/Flash 容量/选项字节地址） |
| `isp_cli.c` | 产线命令行模式 |
| `isp_config.c` / `isp_logname.c` / `log.c` | 默认配置、日志命名、日志分级 |
