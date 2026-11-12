"""
VOFA+ 双向桥：
  COM14 遥测 -> UDP 127.0.0.1:4213
  UDP 127.0.0.1:4214 命令 -> COM14

用法：
    python tools/vofa_udp_bridge.py
    python tools/vofa_udp_bridge.py COM14 9600 4213 4214

VOFA+：
    数据接口 = UDP
    本地端口 = 4213
    远程IP = 127.0.0.1
    远程端口 = 4214
"""

import socket
import sys
import threading
import time

import serial


def main() -> int:
    port = sys.argv[1] if len(sys.argv) > 1 else "COM14"
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 9600
    telem_port = int(sys.argv[3]) if len(sys.argv) > 3 else 4213
    cmd_port = int(sys.argv[4]) if len(sys.argv) > 4 else 4214

    tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    telem_target = ("127.0.0.1", telem_port)

    rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    rx.bind(("127.0.0.1", cmd_port))
    rx.setblocking(False)

    ser = serial.Serial(port, baud, timeout=0.05)

    stop = threading.Event()

    def serial_reader():
        while not stop.is_set():
            try:
                line = ser.readline()
                if line:
                    tx.sendto(line, telem_target)
            except Exception as exc:
                print(f"serial read error: {exc}")
                time.sleep(0.5)

    t = threading.Thread(target=serial_reader, daemon=True)
    t.start()

    print(f"serial {port} @ {baud}")
    print(f"telemetry -> udp 127.0.0.1:{telem_port}")
    print(f"command   <- udp 127.0.0.1:{cmd_port}")
    print("Ctrl+C 退出")

    try:
        while True:
            try:
                data, addr = rx.recvfrom(1024)
                if data:
                    ser.write(data)
                    rx.sendto(b"OK\n", addr)
                    print(f"CMD <- {addr[0]}:{addr[1]}  {data!r}")
            except BlockingIOError:
                pass
            time.sleep(0.002)
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        t.join(timeout=1.0)
        ser.close()
        tx.close()
        rx.close()
        print("已退出")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
