# STM32 ISP Web 烧录器

基于 **Web Serial API** 的 STM32 串口烧录上位机，与片内 ROM Bootloader（ST 官方
AN3155 协议）对接。纯前端单文件实现：`index.html` 即全部程序，零依赖、无需构建、
无需安装，双击即可在浏览器中使用。

- 版本：v1.0
- 界面：自动跟随系统深浅色 · 中/英文切换
- 运行要求：**Chromium 系桌面浏览器**（Chrome / Edge / Chromium / Brave；
  Web Serial API 为 Chromium 系专属，Firefox / Safari 不支持），Windows 与 Linux 均可

## 一、工作原理

STM32 出厂时 ROM 内固化了串口 Bootloader。将 **BOOT0 拉高并复位**，芯片即进入该
Bootloader，等待主机通过 115200-8E1 串口发送 AN3155 命令完成擦除/写入/校验。
本工具通过 USB 转串口芯片的 DTR/RTS 控制线自动完成"复位 + BOOT0 置位"时序
（内置 17 种模式覆盖常见接法），也可用模式 0 手动操作。

## 二、Linux 桌面端使用

功能与 Windows 完全一致（串口收发、DTR/RTS 时序均走标准 Web Serial，Linux 下由
termios 实现），需满足三个前提：

1. **浏览器**：Chrome / Chromium / Edge / Brave 任一桌面版；
2. **串口权限**：当前用户加入 `dialout` 组（部分发行版为 `uucp`），否则能扫到
   设备但打开失败，加组后需注销重新登录：
   ```bash
   sudo usermod -aG dialout $USER
   ```
3. **驱动**：CH340/CH341 由内核自带 `ch341` 串口驱动支持，即插即用。两个常见坑：
   - Ubuntu 系 `brltty`（盲文服务）会把 CH340 误认成蓝牙设备抢走（设备"一闪就没"），
     执行 `sudo apt remove brltty` 解决；
   - `ModemManager` 偶尔探测干扰串口，顽固时可 `sudo systemctl disable --now ModemManager`。

## 三、使用步骤

1. 安装 USB 串口驱动（常见为 CH340/CP210x），设备管理器确认端口存在；
2. 用 Chrome/Edge 打开 `index.html`；
3. 点「选择端口并连接」，在浏览器弹窗中选择设备（浏览器只显示 VID:PID，不显示 COM 号）；
4. 选择 DTR/RTS 模式（常见接法先试模式 1，失败试模式 2 并加大延时）；
5. 选择固件文件（.hex 或 .bin，BIN 需填起始地址，须与链接地址一致）；
6. 点「一键下载」：进 Bootloader → 识别芯片 → 解保护 → 擦除 → 逐页写入+校验 → 退出运行。

快捷键：`Alt+D` 下载 · `Alt+E` 擦除 · `Alt+K` 校验 · `Alt+R` 运行 · `Alt+C` 取消。

## 四、功能一览

- **串口管理**：连接/断开、波特率预设+自定义、固定 8E1、DTR/RTS 手动翻转与状态指示
- **自动进 Bootloader**：模式 0–16 DTR/RTS 时序，每步延时可调，完成后自动恢复从 Flash 启动
- **AN3155 全命令**：同步、GET、GET ID、GET Version、读/写内存、标准/扩展擦除、
  解写保护（0x73+0xFF 握手）、解读保护（0x92+0xE3 握手）、GO、读 Flash 容量寄存器
- **芯片识别**：内置 94 条芯片库（C0/F0/F1/F2/F3/F4/F7/G0/G4/H5/H7/L0/L1/L4/L5/U5/WB/WL），
  同 PID 冲突按扩展擦除支持+容量寄存器实测值自动消歧
- **固件下载**：Intel HEX / BIN、地址范围校验、256B/页写入（4 字节对齐补 0xFF）、
  逐页回读校验、实时进度、随时取消
- **单独操作**：擦除 / 校验 / 运行 / 整片 Flash 回读导出 Intel HEX
- **选项字节**：按芯片库定位地址，16 字节读/写（写后复位生效）
- **调试面板**：单步命令按钮 + 手动发送任意 HEX 帧并显示应答
- **日志**：主日志分级（毫秒时间戳）+ 协议 HEX 完整明细（2500 行上限），均可导出/清空

## 五、设计要点

- **操作独占锁**：所有串口操作排队执行，一次只允许一个操作占用串口，无并发竞态；
- **可取消**：ACK 等待、写页循环、Bootloader 时序延时均分片检查取消标志；
- **浏览器兼容**：`setSignals` 同时传新旧两套字典键名
  （`dataTerminalReady`/`requestToSend` 与旧名 `dataTerminalControl`/`requesterComputerControl`），
  兼容 Chromium 139 前后版本；
- **发送时序**：每帧写入后等待上线再读 ACK（Web Serial 无 Flush API，固定 5ms 冲刷窗口）。

## 六、FAQ

| 现象 | 排查 |
|---|---|
| 提示浏览器不支持 Web Serial | 需 Chrome/Edge 桌面版 89+；Firefox/Safari 不支持 |
| 端口列表为空 | 未插 USB 线或驱动未装 |
| 打开串口失败 | 端口被其它软件占用（串口工具、监视器等），先关闭 |
| DTR/RTS 设置失败 | 适配器不支持控制线（如蓝牙串口），改用模式 0 手动进 BL |
| 进不了 Bootloader | 模式与接线不匹配：试模式 2 并加大延时；或模式 0 手动 BOOT0=1+复位 |
| 同步成功但 GET 异常 | 查看协议 HEX 日志的实际字节流；确认芯片未运行占用串口的程序 |
| BIN 下载后不运行 | 起始地址与工程链接地址不一致；HEX 文件无此问题 |
| 固件超出 Flash | 固件与芯片容量不匹配，或 BIN 地址+长度越界 |

## 七、文件说明

| 文件 | 说明 |
|---|---|
| `index.html` | 全部程序（界面 + 协议 + 芯片库 + HEX 解析，约 1650 行，含中文注释） |
| `README.md` | 本文档 |
