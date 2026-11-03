# -*- coding: utf-8 -*-
"""
串口调试助手：自动扫波特率 / 回环自测 / 发 AT

用法:
    python com_scan.py --list               # 列出所有串口
    python com_scan.py COM5 --loop          # 回环测试（先短接 TXD 和 RXD）
    python com_scan.py COM5 --scan          # 逐个波特率发 AT，看哪个有回应
    python com_scan.py COM5 --at 9600       # 用指定波特率交互发 AT
"""
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("缺少 pyserial，先运行: python -m pip install pyserial")
    raise SystemExit(1)

if not sys.stdout.isatty():
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# 常见波特率，扫描顺序按可能性排
BAUDS = [9600, 38400, 115200, 19200, 57600, 4800, 2400, 1200]

# AT 命令的几种行尾写法，不同模块认的不同（最后一个是完全不带行尾）
TERMINATORS = [b"\r\n", b"\n", b"\r", b""]


def cmd_list():
    ports = list(list_ports.comports())
    if not ports:
        print("没有任何串口")
        return 1
    for p in ports:
        print(f"{p.device:<8} {p.description}")
        print(f"         {p.hwid}")
    return 0


def cmd_loop(port, baud=115200):
    """回环测试：需要先把 TXD 和 RXD 短接"""
    payload = b"LOOPBACK-TEST-123"
    print(f"回环测试 {port} @ {baud}")
    print("  前提：TXD 和 RXD 已经用一根线短接")
    try:
        with serial.Serial(port, baud, timeout=0.5, write_timeout=0.5) as sp:
            sp.reset_input_buffer()
            sp.write(payload)
            time.sleep(0.2)
            got = sp.read(len(payload) + 16)
    except Exception as e:
        print(f"  [X] 打开失败: {e}")
        return 2

    print(f"  发: {payload!r}")
    print(f"  收: {got!r}")
    if got == payload:
        print("  [OK] 回环成功 —— USB-TTL、驱动、工具都正常，问题在模块那边")
        return 0
    if got:
        print("  [!] 收到了东西但内容不对，可能是波特率不匹配")
        return 3
    print("  [X] 什么都没收到")
    print("      → 线没接好 / 短接没落到实处 / PL2303 驱动有问题")
    return 3


def try_at(port, baud, quiet=False):
    """在指定波特率下发 AT，返回收到的原始字节"""
    for term in TERMINATORS:
        try:
            with serial.Serial(port, baud, timeout=0.4, write_timeout=0.4) as sp:
                sp.reset_input_buffer()
                sp.write(b"AT" + term)
                time.sleep(0.15)
                data = sp.read(64)
        except Exception as e:
            if not quiet:
                print(f"  {baud:>6}  打开失败: {e}")
            return None
        if data:
            if not quiet:
                print(f"  {baud:>6}  {term!r:>10}  →  {data!r}")
            return data
    if not quiet:
        print(f"  {baud:>6}  无回应")
    return b""


def cmd_scan(port):
    print(f"扫描 {port}，逐个波特率发 AT ...\n")
    hit = []
    opened = 0
    for b in BAUDS:
        data = try_at(port, b)
        if data is None:          # 打不开，不是"没回应"
            continue
        opened += 1
        if data:
            hit.append((b, data))

    print()
    if opened == 0:
        print("[X] 一个波特率都没能打开端口")
        print("    → 端口被别的程序占用：网页串口工具、串口助手、Keil 的调试器")
        print("    → 把那些程序关掉，或者点它们的『断开』")
        return 5

    if hit:
        print("[OK] 有回应的波特率:")
        for b, d in hit:
            print(f"     {b}  →  {d!r}")
    else:
        print(f"[X] {opened} 个波特率都试过了，全部没有回应")
        print()
        print("按这个顺序排查：")
        print("  1. TXD 和 RXD 对调（模块 TX → 转接板 RX）")
        print("  2. 模块 VCC 是 3.3V 还是 5V？查一下型号要求")
        print("  3. 先跑 --loop 确认 USB-TTL 本身是好的")
    return 0 if hit else 4


def cmd_at(port, baud):
    print(f"交互模式 {port} @ {baud}，输入命令回车发送，Ctrl+C 退出")
    try:
        with serial.Serial(port, baud, timeout=0.3, write_timeout=0.3) as sp:
            sp.reset_input_buffer()
            while True:
                line = input("> ")
                sp.reset_input_buffer()
                sp.write(line.encode() + b"\r\n")
                time.sleep(0.2)
                data = sp.read(256)
                print(f"  收: {data!r}")
    except KeyboardInterrupt:
        print("\n退出")
    except Exception as e:
        print(f"[X] {e}")
        return 2
    return 0


def main():
    args = [a for a in sys.argv[1:]]
    if not args or args[0] in ("-h", "--help"):
        print(__doc__)
        return 1

    if args[0] == "--list":
        return cmd_list()

    port = args[0]
    rest = args[1:]

    if "--loop" in rest:
        baud = 115200
        if "--at" in rest:
            baud = int(rest[rest.index("--at") + 1])
        return cmd_loop(port, baud)
    if "--scan" in rest:
        return cmd_scan(port)
    if "--at" in rest:
        return cmd_at(port, int(rest[rest.index("--at") + 1]))

    print(__doc__)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
