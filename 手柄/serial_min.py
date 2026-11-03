# -*- coding: utf-8 -*-
"""
第 ④ 步「把字节写进串口」到底怎么实现的

平时写的三行：
    import serial
    sp = serial.Serial("COM5", 115200, timeout=0)
    sp.write(b"\\xAA\\x02\\x64\\x21\\x85")

pyserial 内部其实按顺序调了这几个 Windows API：

    Serial() 构造时
      1. CreateFileW("\\\\.\\COM5", ...)      -> 打开设备，拿到句柄
      2. SetupComm(h, 4096, 4096)             -> 收发缓冲区大小
      3. GetCommState(h, &dcb)                -> 读出当前参数
      4. dcb.BaudRate = 115200 ...            -> 改参数
         SetCommState(h, &dcb)                -> 写回驱动
      5. SetCommTimeouts(h, &tmo)             -> 读写超时

    sp.write(bytes) 时
      6. WriteFile(h, bytes, len, &written, 0)-> 塞进驱动的发送缓冲

这个文件就是把上面 6 步手写一遍，不用装任何第三方库。

用法:
    python serial_min.py COM5           # 打开并打印当前 DCB，然后写一帧
    python serial_min.py COM5 --loop    # 回环测试（把 TXD 和 RXD 短接）

注意: 现在机器上没有串口时，只能验证到「打开失败」，其余要等真实 COM 口。
"""
import ctypes
import sys
from ctypes import wintypes

if not sys.stdout.isatty():
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

k32 = ctypes.WinDLL("kernel32", use_last_error=True)

INVALID_HANDLE_VALUE = wintypes.HANDLE(-1).value
GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3
ERROR_FILE_NOT_FOUND = 2
ERROR_ACCESS_DENIED = 5


# ----------------------------------------------------------------- 结构体 ---
class DCB(ctypes.Structure):
    """设备控制块。注意 flags 那个 DWORD 里塞了 15 个位域，整体当黑盒用。"""
    _fields_ = [
        ("DCBlength", wintypes.DWORD),   # 必须填 sizeof(DCB)
        ("BaudRate", wintypes.DWORD),
        ("flags", wintypes.DWORD),       # fBinary / fParity / fRtsControl ...
        ("wReserved", wintypes.WORD),
        ("XonLim", wintypes.WORD),
        ("XoffLim", wintypes.WORD),
        ("ByteSize", wintypes.BYTE),     # 数据位，通常 8
        ("Parity", wintypes.BYTE),       # 0=NOPARITY 1=ODD 2=EVEN
        ("StopBits", wintypes.BYTE),     # 0=1位 1=1.5位 2=2位
        ("XonChar", ctypes.c_char),
        ("XoffChar", ctypes.c_char),
        ("ErrorChar", ctypes.c_char),
        ("EofChar", ctypes.c_char),
        ("EvtChar", ctypes.c_char),
        ("wReserved1", wintypes.WORD),
    ]


class COMMTIMEOUTS(ctypes.Structure):
    _fields_ = [
        ("ReadIntervalTimeout", wintypes.DWORD),
        ("ReadTotalTimeoutMultiplier", wintypes.DWORD),
        ("ReadTotalTimeoutConstant", wintypes.DWORD),
        ("WriteTotalTimeoutMultiplier", wintypes.DWORD),
        ("WriteTotalTimeoutConstant", wintypes.DWORD),
    ]


# 函数签名（restype/argtypes 不设对，64 位下句柄会被截断成 32 位）
k32.CreateFileW.restype = wintypes.HANDLE
k32.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                            ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD,
                            wintypes.HANDLE]
k32.GetCommState.argtypes = [wintypes.HANDLE, ctypes.POINTER(DCB)]
k32.SetCommState.argtypes = [wintypes.HANDLE, ctypes.POINTER(DCB)]
k32.SetCommTimeouts.argtypes = [wintypes.HANDLE, ctypes.POINTER(COMMTIMEOUTS)]
k32.SetupComm.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.DWORD]
k32.WriteFile.argtypes = [wintypes.HANDLE, ctypes.c_char_p, wintypes.DWORD,
                          ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p]
k32.ReadFile.argtypes = [wintypes.HANDLE, ctypes.c_char_p, wintypes.DWORD,
                         ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p]
k32.CloseHandle.argtypes = [wintypes.HANDLE]


# ------------------------------------------------------------------ 封装 ---
class RawSerial:
    """只用 win32 API 的最小串口"""

    def __init__(self, port, baud=115200):
        # Windows 要求 COM10 以上必须加 \\.\ 前缀；统一都加，省得踩坑
        path = port if port.startswith("\\\\.\\") else "\\\\.\\" + port
        self.handle = k32.CreateFileW(
            path,
            GENERIC_READ | GENERIC_WRITE,
            0,                       # 独占，不允许别人同时打开
            None,
            OPEN_EXISTING,
            0,                       # 同步 I/O
            None,
        )
        if self.handle == INVALID_HANDLE_VALUE:
            err = ctypes.get_last_error()
            raise OSError(err, self._explain(err))
        self.port, self.baud = port, baud
        self._setup()

    @staticmethod
    def _explain(err):
        return {
            ERROR_FILE_NOT_FOUND: "端口不存在（没插好，或 COM 号写错）",
            ERROR_ACCESS_DENIED: "端口被别的程序占用（串口助手没关？）",
        }.get(err, "打开串口失败")

    def _setup(self):
        # ① 收发缓冲区各 4 KB
        k32.SetupComm(self.handle, 4096, 4096)

        # ② 读出当前 DCB，改三个参数再写回
        dcb = DCB()
        dcb.DCBlength = ctypes.sizeof(DCB)
        if not k32.GetCommState(self.handle, ctypes.byref(dcb)):
            raise OSError(ctypes.get_last_error(), "GetCommState 失败")

        self.orig_baud = dcb.BaudRate

        dcb.BaudRate = self.baud
        dcb.ByteSize = 8
        dcb.Parity = 0        # NOPARITY
        dcb.StopBits = 0      # 1 位停止位
        # flags 里的 fBinary 必须是 1，否则按文本模式处理
        dcb.flags |= 0x1

        if not k32.SetCommState(self.handle, ctypes.byref(dcb)):
            raise OSError(ctypes.get_last_error(), "SetCommState 失败")

        # ③ 超时：写不出去最多等 200 ms，读不阻塞
        tmo = COMMTIMEOUTS(0, 0, 0, 0, 200)
        k32.SetCommTimeouts(self.handle, ctypes.byref(tmo))

    def write(self, data: bytes):
        written = wintypes.DWORD(0)
        ok = k32.WriteFile(self.handle, data, len(data),
                           ctypes.byref(written), None)
        if not ok:
            raise OSError(ctypes.get_last_error(), "WriteFile 失败")
        return written.value

    def read(self, n=64):
        buf = ctypes.create_string_buffer(n)
        got = wintypes.DWORD(0)
        k32.ReadFile(self.handle, buf, n, ctypes.byref(got), None)
        return buf.raw[: got.value]

    def close(self):
        if self.handle and self.handle != INVALID_HANDLE_VALUE:
            k32.CloseHandle(self.handle)
            self.handle = None

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.close()


# ------------------------------------------------------------------- main ---
def main():
    if len(sys.argv) < 2:
        print(__doc__)
        # 结构体大小自检：这两个数字对不上，后面全乱
        print(f"sizeof(DCB)          = {ctypes.sizeof(DCB):>3}  (应为 28)")
        print(f"sizeof(COMMTIMEOUTS) = {ctypes.sizeof(COMMTIMEOUTS):>3}  (应为 20)")
        return 1

    port = sys.argv[1]
    loop = "--loop" in sys.argv

    try:
        with RawSerial(port, 115200) as sp:
            print(f"[OK] {port} 已打开，原波特率 {sp.orig_baud}，已设为 {sp.baud}")

            frame = bytes([0xAA, 0x02, 100, 33, (100 + 33) & 0xFF])
            n = sp.write(frame)
            print(f"[OK] WriteFile 写入 {n} 字节: {frame.hex(' ').upper()}")

            if loop:
                # 短接 TXD 和 RXD 时，写进去的会立刻被自己读回来
                import time
                time.sleep(0.1)
                back = sp.read(64)
                print(f"[>>] 读回: {back.hex(' ').upper() if back else '(空)'}")
                print("     回环成功" if back == frame else "     回环失败，检查 TXD/RXD 是否短接")
    except OSError as e:
        print(f"[X] errno={e.errno}  {e.strerror}")
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
