#!/usr/bin/env python3
"""YdgodOS serial debugging terminal.

Continuously receives device output in a background thread and, unless
--listen is used, forwards keyboard input to the serial port immediately.
"""

from __future__ import annotations

import argparse
import codecs
import os
import select
import sys
import threading
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import BinaryIO

try:
    import serial
    from serial.tools import list_ports
except ImportError as exc:  # pragma: no cover - exercised when dependency is absent
    raise SystemExit(
        "缺少 pyserial，请先执行: python -m pip install -r tools/requirements.txt"
    ) from exc


EXIT_KEY = b"\x1d"  # Ctrl+]
OLED_SCROLL_UP_KEY = b"\x10"    # Ctrl+P, consumed by the MCU OLED app.
OLED_SCROLL_DOWN_KEY = b"\x0e"  # Ctrl+N, consumed by the MCU OLED app.


@dataclass(frozen=True)
class TerminalConfig:
    port: str
    baudrate: int = 115200
    encoding: str = "utf-8"
    hexadecimal: bool = False
    timestamps: bool = False
    reconnect: bool = True
    reconnect_interval: float = 1.0
    dtr: bool = False
    rts: bool = False
    log_path: Path | None = None


def diagnostic(message: str) -> None:
    """Print terminal status without mixing it with captured stdout."""
    print(f"\n[terminal] {message}", file=sys.stderr, flush=True)


def timestamp() -> str:
    """Return a local timestamp with millisecond precision."""
    now = datetime.now()
    return f"[{now:%H:%M:%S}.{now.microsecond // 1000:03d}] "


def available_ports() -> list[serial.tools.list_ports_common.ListPortInfo]:
    return sorted(list_ports.comports(), key=lambda item: item.device)


def print_ports() -> None:
    ports = available_ports()
    if not ports:
        print("未发现串口。")
        return
    print("可用串口：")
    for item in ports:
        hardware = ""
        if item.vid is not None and item.pid is not None:
            hardware = f" VID:PID={item.vid:04X}:{item.pid:04X}"
        print(f"  {item.device:<12} {item.description}{hardware}")


def resolve_port(requested: str | None) -> str:
    if requested:
        return requested
    ports = available_ports()
    if len(ports) == 1:
        diagnostic(f"自动选择串口 {ports[0].device}")
        return ports[0].device
    print_ports()
    if not ports:
        raise SystemExit("未指定串口且没有检测到串口，请使用 --port COMx。")
    raise SystemExit("检测到多个串口，请使用 --port 指定要监听的端口。")


class SerialTerminal:
    def __init__(self, config: TerminalConfig) -> None:
        self.config = config
        self._serial: serial.SerialBase | None = None
        self._serial_lock = threading.Lock()
        self._output_lock = threading.Lock()
        self._stop = threading.Event()
        self._connected = threading.Event()
        self._thread: threading.Thread | None = None
        self._decoder = codecs.getincrementaldecoder(config.encoding)(errors="replace")
        self._line_start = True
        self._log_file: BinaryIO | None = None

    @property
    def stopped(self) -> bool:
        return self._stop.is_set()

    def start(self) -> None:
        if self.config.log_path is not None:
            self.config.log_path.parent.mkdir(parents=True, exist_ok=True)
            self._log_file = self.config.log_path.open("ab")
        self._thread = threading.Thread(
            target=self._reader_loop, name="serial-reader", daemon=True
        )
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        self._close_serial()
        if self._thread is not None:
            self._thread.join(timeout=2.0)
        if self._log_file is not None:
            self._log_file.close()
            self._log_file = None

    def wait(self) -> None:
        while not self._stop.wait(0.2):
            pass

    def write(self, data: bytes) -> bool:
        if not data:
            return True
        with self._serial_lock:
            connection = self._serial
            if connection is None or not connection.is_open:
                diagnostic("串口尚未连接，输入未发送")
                return False
            try:
                connection.write(data)
                connection.flush()
                return True
            except (serial.SerialException, serial.SerialTimeoutException, OSError) as exc:
                diagnostic(f"发送失败: {exc}")
                self._connected.clear()
                return False

    def _open_serial(self) -> bool:
        try:
            connection = serial.serial_for_url(
                self.config.port,
                baudrate=self.config.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.1,
                write_timeout=1.0,
                xonxoff=False,
                rtscts=False,
                dsrdtr=False,
            )
            try:
                connection.dtr = self.config.dtr
                connection.rts = self.config.rts
            except (OSError, serial.SerialException):
                # Some virtual serial drivers do not implement modem controls.
                pass
        except (serial.SerialException, OSError, ValueError) as exc:
            diagnostic(f"无法打开 {self.config.port}: {exc}")
            return False

        with self._serial_lock:
            self._serial = connection
        self._connected.set()
        diagnostic(
            f"已连接 {self.config.port} @ {self.config.baudrate} 8N1，正在监听"
        )
        return True

    def _close_serial(self) -> None:
        with self._serial_lock:
            connection, self._serial = self._serial, None
        self._connected.clear()
        if connection is not None:
            try:
                connection.close()
            except (serial.SerialException, OSError):
                pass

    def _reader_loop(self) -> None:
        while not self._stop.is_set():
            if not self._connected.is_set():
                if not self._open_serial():
                    if not self.config.reconnect:
                        self._stop.set()
                        return
                    self._stop.wait(self.config.reconnect_interval)
                    continue

            with self._serial_lock:
                connection = self._serial
            if connection is None:
                continue
            try:
                waiting = connection.in_waiting
                data = connection.read(max(1, min(waiting, 4096)))
                if data:
                    self._display(data)
            except (serial.SerialException, OSError) as exc:
                diagnostic(f"连接中断: {exc}")
                self._close_serial()
                if not self.config.reconnect:
                    self._stop.set()
                    return

    def _display(self, data: bytes) -> None:
        if self._log_file is not None:
            self._log_file.write(data)
            self._log_file.flush()

        with self._output_lock:
            if self.config.hexadecimal:
                prefix = ""
                if self.config.timestamps:
                    prefix = timestamp()
                sys.stdout.write(prefix + data.hex(" ").upper() + "\n")
            else:
                text = self._decoder.decode(data)
                if self.config.timestamps:
                    text = self._add_timestamps(text)
                sys.stdout.write(text)
            sys.stdout.flush()

    def _add_timestamps(self, text: str) -> str:
        output: list[str] = []
        for character in text:
            if self._line_start:
                output.append(timestamp())
                self._line_start = False
            output.append(character)
            if character == "\n":
                self._line_start = True
        return "".join(output)


def interactive_keyboard(terminal: SerialTerminal) -> None:
    diagnostic("上下方向键翻阅 OLED 历史内容；Ctrl+] 退出")
    diagnostic("交互模式：按 Ctrl+] 退出，输入会立即发送到开发板")
    if os.name == "nt":
        _windows_keyboard(terminal)
    elif sys.stdin.isatty():
        _posix_keyboard(terminal)
    else:
        _line_keyboard(terminal)


def _windows_keyboard(terminal: SerialTerminal) -> None:
    import msvcrt

    while not terminal.stopped:
        if not msvcrt.kbhit():
            time.sleep(0.01)
            continue
        character = msvcrt.getwch()
        if character in ("\x00", "\xe0"):
            scan_code = msvcrt.getwch()
            if scan_code == "H":
                terminal.write(OLED_SCROLL_UP_KEY)
            elif scan_code == "P":
                terminal.write(OLED_SCROLL_DOWN_KEY)
            continue
        encoded = character.encode(terminal.config.encoding, errors="replace")
        if encoded == EXIT_KEY:
            return
        terminal.write(encoded)


def _posix_keyboard(terminal: SerialTerminal) -> None:
    import termios
    import tty

    descriptor = sys.stdin.fileno()
    previous = termios.tcgetattr(descriptor)
    try:
        tty.setraw(descriptor)
        while not terminal.stopped:
            readable, _, _ = select.select([descriptor], [], [], 0.1)
            if not readable:
                continue
            data = os.read(descriptor, 1)
            if data == EXIT_KEY:
                return
            if data == b"\x1b":
                sequence = bytearray(data)
                deadline = time.monotonic() + 0.03
                while len(sequence) < 3:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        break
                    more, _, _ = select.select([descriptor], [], [], remaining)
                    if not more:
                        break
                    sequence.extend(os.read(descriptor, 1))
                if bytes(sequence) in (b"\x1b[A", b"\x1bOA"):
                    data = OLED_SCROLL_UP_KEY
                elif bytes(sequence) in (b"\x1b[B", b"\x1bOB"):
                    data = OLED_SCROLL_DOWN_KEY
                else:
                    data = bytes(sequence)
            terminal.write(data)
    finally:
        termios.tcsetattr(descriptor, termios.TCSADRAIN, previous)


def _line_keyboard(terminal: SerialTerminal) -> None:
    for line in sys.stdin:
        if terminal.stopped:
            return
        terminal.write(line.encode(terminal.config.encoding, errors="replace"))


def positive_baudrate(value: str) -> int:
    result = int(value)
    if result <= 0:
        raise argparse.ArgumentTypeError("波特率必须大于 0")
    return result


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="YdgodOS 串口调试终端（默认 115200 8N1）"
    )
    parser.add_argument("-p", "--port", default=os.getenv("YDGODOS_PORT"),
                        help="串口名，例如 COM3 或 /dev/ttyUSB0；也可设置 YDGODOS_PORT")
    parser.add_argument("-b", "--baudrate", type=positive_baudrate, default=115200,
                        help="波特率，默认 115200")
    parser.add_argument("--list", action="store_true", help="列出串口后退出")
    parser.add_argument("--listen", action="store_true",
                        help="仅监听接收，不读取键盘输入")
    parser.add_argument("--hex", action="store_true", dest="hexadecimal",
                        help="以十六进制显示收到的数据")
    parser.add_argument("--timestamp", action="store_true",
                        help="为接收数据添加时间戳")
    parser.add_argument("--encoding", default="utf-8",
                        help="文本编码，默认 utf-8，可选 gbk")
    parser.add_argument("--log", type=Path, dest="log_path",
                        help="将接收到的原始数据追加保存到文件")
    parser.add_argument("--no-reconnect", action="store_false", dest="reconnect",
                        help="串口断开或打开失败时不自动重连")
    parser.add_argument("--reconnect-interval", type=float, default=1.0,
                        help="自动重连间隔秒数，默认 1.0")
    parser.add_argument("--dtr", action="store_true", help="连接后置位 DTR")
    parser.add_argument("--rts", action="store_true", help="连接后置位 RTS")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if args.list:
        print_ports()
        return 0
    try:
        codecs.lookup(args.encoding)
    except LookupError as exc:
        raise SystemExit(f"未知文本编码: {args.encoding}") from exc
    if args.reconnect_interval <= 0:
        raise SystemExit("--reconnect-interval 必须大于 0")

    config = TerminalConfig(
        port=resolve_port(args.port),
        baudrate=args.baudrate,
        encoding=args.encoding,
        hexadecimal=args.hexadecimal,
        timestamps=args.timestamp,
        reconnect=args.reconnect,
        reconnect_interval=args.reconnect_interval,
        dtr=args.dtr,
        rts=args.rts,
        log_path=args.log_path,
    )
    terminal = SerialTerminal(config)
    terminal.start()
    try:
        if args.listen:
            diagnostic("仅监听模式：按 Ctrl+C 退出")
            terminal.wait()
        else:
            interactive_keyboard(terminal)
    except KeyboardInterrupt:
        pass
    finally:
        terminal.stop()
        diagnostic("已退出")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
