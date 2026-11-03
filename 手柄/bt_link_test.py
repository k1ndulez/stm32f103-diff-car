# -*- coding: utf-8 -*-
"""
蓝牙链路穿透测试（蓝牙口只开一次，扫 USB-TTL 的波特率）

    PL = USB-TTL（有线接模块 TX/RX）
    BT = 蓝牙串口（无线连着同一个模块）

    B 方向: 写 PL → 模块 RX → 无线 → BT 收到
    A 方向: 写 BT → 无线 → 模块 TX → PL 收到

用法: python bt_link_test.py COM12 COM14
"""
import sys
import time

import serial

if not sys.stdout.isatty():
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

BAUDS = [9600, 19200, 38400, 57600, 115200, 4800, 2400, 1200]


def open_bt(port, tries=3):
    for i in range(tries):
        try:
            return serial.Serial(port, 9600, timeout=0.5, write_timeout=0.8)
        except Exception as e:
            print(f"  打开 {port} 第 {i+1} 次失败: {e}")
            time.sleep(1.0)
    return None


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    pl_port, bt_port = sys.argv[1], sys.argv[2]

    print(f"打开蓝牙口 {bt_port} ...")
    bt = open_bt(bt_port)
    if bt is None:
        print("[X] 蓝牙口打不开")
        print("    → 手机还连着？先在手机里取消配对")
        print("    → 或者模块已经掉线，重新配对一次")
        return 2
    print("[OK] 蓝牙口已打开\n")

    payload_b = b"HELLO-UART-1234"
    payload_a = b"HELLO-BT-5678"
    good = []

    try:
        for baud in BAUDS:
            try:
                pl = serial.Serial(pl_port, baud, timeout=0.5, write_timeout=0.8)
            except Exception as e:
                print(f"  {baud:>6}  PL 打不开: {e}")
                continue

            # B: PL -> 模块 RX -> 蓝牙
            try:
                bt.reset_input_buffer()
                pl.reset_input_buffer()
                time.sleep(0.05)
                pl.write(payload_b)
                time.sleep(0.3)
                got_b = bt.read(len(payload_b) + 32)
            except Exception as e:
                got_b = b""
                print(f"  {baud:>6}  B 方向异常: {e}")

            # A: 蓝牙 -> 模块 TX -> PL
            try:
                bt.reset_input_buffer()
                pl.reset_input_buffer()
                time.sleep(0.05)
                bt.write(payload_a)
                time.sleep(0.3)
                got_a = pl.read(len(payload_a) + 32)
            except Exception as e:
                got_a = b""
                print(f"  {baud:>6}  A 方向异常: {e}")

            mark_b = "OK " if payload_b in got_b else "   "
            mark_a = "OK " if payload_a in got_a else "   "
            print(f"  {baud:>6}  B[{mark_b}]{got_b!r:<30}  A[{mark_a}]{got_a!r}")

            if payload_b in got_b or payload_a in got_a:
                good.append(baud)
            pl.close()
    finally:
        bt.close()

    print()
    if good:
        print(f"[OK] 这些波特率下链路是通的: {good}")
        print("    → 用其中一个改 STM32 的 USART3 波特率即可")
    else:
        print("[X] 没有干净的收发")
        print("    → 若全是乱码，说明波特率都不对，或模块时钟异常")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
