# YdgodOS STM32F103C8T6 Demo

这是 YdgodOS v2.0 的 STM32F103C8T6（Blue Pill/最小系统板）示例，使用 FreeRTOS V11.1.0、EIDE 和 MDK5 ARM Compiler 5。

## 硬件连接

| 功能 | 引脚 | 配置 |
| --- | --- | --- |
| Shell TX | PA9 / USART1_TX | 115200, 8N1 |
| Shell RX | PA10 / USART1_RX | 115200, 8N1 |
| 运行指示灯 | PC13 | 500 ms 翻转，低电平点亮 |

USB-TTL 必须使用 3.3 V 电平，连接 `PA9 -> RX`、`PA10 -> TX`、`GND -> GND`。

## EIDE + MDK5 构建

1. 使用 VS Code 打开 `YdgodOS_STMF103_demo.code-workspace`。
2. 安装 Embedded IDE（EIDE）扩展。
3. 在 EIDE 中选择 `YdgodOS_STMF103_demo`，工具链选择 `AC5`，工具链目录指向 MDK5 的 `ARM/ARMCC`。
4. 执行 `Rebuild`。固件输出到 `build/YdgodOS_STMF103_demo/`，可烧录其中的 `.hex` 文件。

工程配置为 STM32F103C8：64 KB Flash、20 KB RAM、72 MHz HSE+PLL。也可以直接打开 `MDK-ARM/YdgodOS_STMF103_demo.uvprojx` 构建。

## Shell

复位后串口会输出开机横幅和提示符：

```text
YdgodOS v2.0.0 - STM32F103 edition
[ OK ] System clock: 72 MHz
[ OK ] Kernel: FreeRTOS V11.1.0
[ OK ] Console: USART1 PA9(TX)/PA10(RX), 115200 8N1
[ OK ] Run indicator: PC13 heartbeat (active-low)

ydgodos@f103:~$ 
```

支持 `help`、`clear`、`echo`、`history`、`info`、`uptime`、`tasks`、`heap`、`led` 和 `reboot`。Tab 行为与 Linux Shell 类似：唯一候选直接补全，多个候选补公共前缀，连续两次 Tab 列出候选；`help` 和 `led` 的参数也可补全。上、下方向键浏览历史，`Ctrl-L` 清屏，`Ctrl-C` 取消当前输入。

`led auto` 恢复心跳，`led on`/`led off` 固定灯状态，`led toggle` 翻转灯状态。

仓库同时提供 Python 串口调试终端，回到仓库根目录后运行：

```powershell
python -m pip install -r tools/requirements.txt
python tools/serial_terminal.py --port COM3
```

使用 `--listen` 可以只监听串口，不向开发板发送键盘输入。
