# YdgodOS 串口调试终端

安装依赖：

```powershell
python -m pip install -r tools/requirements.txt
```

列出串口：

```powershell
python tools/serial_terminal.py --list
```

连接 F103 并进行交互：

```powershell
python tools/serial_terminal.py --port COM3
```

程序默认使用 `115200 8N1`。交互模式会持续监听开发板输出，并将每次按键立即发给开发板，因此可以正常使用 YdgodOS Shell 的回显、退格、Tab 和 Ctrl-L。按 `Ctrl+]` 退出终端。

仅监听并保存原始数据：

```powershell
python tools/serial_terminal.py --port COM3 --listen --timestamp --log receive.log
```

十六进制监听：

```powershell
python tools/serial_terminal.py --port COM3 --listen --hex
```

串口断开后默认每秒尝试自动重连；使用 `--no-reconnect` 可以关闭。未指定 `--port` 且系统只发现一个串口时，程序会自动选择它。
