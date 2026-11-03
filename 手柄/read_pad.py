# -*- coding: utf-8 -*-
"""
零依赖读手柄（Windows）

优先走 XInput（Xbox 兼容 / 大多数 2.4G 接收器），
找不到再退回 winmm 的经典手柄 API（DInput）。

用法:
    python read_pad.py          # 打印一次快照
    python read_pad.py -w       # 约 50 Hz 连续刷新，Ctrl+C 退出
    python read_pad.py -w -t 5  # 连续刷新 5 秒后自动退出（自测用）

不依赖任何第三方库。
"""
import ctypes
import sys
import time
from ctypes import wintypes

# 重定向时强制 UTF-8；Windows 控制台里用原生编码，避免乱码
if not sys.stdout.isatty():
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

VERBOSE = "-v" in sys.argv


# ---------------------------------------------------------------- XInput ---
XUSER_MAX_COUNT = 4
ERROR_SUCCESS = 0
ERROR_DEVICE_NOT_CONNECTED = 1167

XINPUT_BUTTONS = [
    (0x0001, "上"), (0x0002, "下"), (0x0004, "左"), (0x0008, "右"),
    (0x0010, "START"), (0x0020, "BACK"),
    (0x0040, "LS"), (0x0080, "RS"),
    (0x0100, "LB"), (0x0200, "RB"),
    (0x1000, "A"), (0x2000, "B"), (0x4000, "X"), (0x8000, "Y"),
]


class XINPUT_GAMEPAD(ctypes.Structure):
    _fields_ = [
        ("wButtons", wintypes.WORD),
        ("bLeftTrigger", wintypes.BYTE),
        ("bRightTrigger", wintypes.BYTE),
        ("sThumbLX", ctypes.c_short),
        ("sThumbLY", ctypes.c_short),
        ("sThumbRX", ctypes.c_short),
        ("sThumbRY", ctypes.c_short),
    ]


class XINPUT_STATE(ctypes.Structure):
    _fields_ = [("dwPacketNumber", wintypes.DWORD), ("Gamepad", XINPUT_GAMEPAD)]


def load_xinput():
    for name in ("XInput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"):
        try:
            return name, ctypes.WinDLL(name)
        except OSError:
            continue
    return None, None


def xinput_scan(dll):
    """返回 (slot, state) 列表"""
    found = []
    for slot in range(XUSER_MAX_COUNT):
        st = XINPUT_STATE()
        if dll.XInputGetState(slot, ctypes.byref(st)) == ERROR_SUCCESS:
            found.append((slot, st))
    return found


def norm(v):
    """short -> [-1, 1]，32767 满量程；小于死区的漂移不处理（本轮不做死区）"""
    if v >= 32767:
        return 1.0
    if v <= -32768:
        return -1.0
    return v / 32767.0


def fmt_xinput(name, slot, st):
    g = st.Gamepad
    lx, ly = norm(g.sThumbLX), norm(g.sThumbLY)
    rx, ry = norm(g.sThumbRX), norm(g.sThumbRY)
    lt = g.bLeftTrigger / 255.0
    rt = g.bRightTrigger / 255.0

    names = [label for mask, label in XINPUT_BUTTONS if g.wButtons & mask]
    btn = " ".join(names) if names else "--"

    # 十字键单独拆出来更好读
    dpad = [l for m, l in XINPUT_BUTTONS[:4] if g.wButtons & m]
    dpad_s = "/".join(dpad) if dpad else "居中"

    return (f"[{name} slot {slot}] "
            f"X {lx:+.2f}  Y {ly:+.2f}   RX {rx:+.2f}  RY {ry:+.2f}   "
            f"LT {lt:.2f}  RT {rt:.2f}   键 {btn:<28} 十字 {dpad_s}")


# ----------------------------------------------------------------- winmm ---
class JOYINFOEX(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD), ("dwFlags", wintypes.DWORD),
        ("dwXpos", wintypes.DWORD), ("dwYpos", wintypes.DWORD),
        ("dwZpos", wintypes.DWORD), ("dwRpos", wintypes.DWORD),
        ("dwUpos", wintypes.DWORD), ("dwVpos", wintypes.DWORD),
        ("dwButtons", wintypes.DWORD), ("dwButtonNumber", wintypes.DWORD),
        ("dwPOV", wintypes.DWORD),
        ("dwReserved1", ctypes.c_ulong), ("dwReserved2", ctypes.c_ulong),
    ]


class JOYCAPSW(ctypes.Structure):
    _fields_ = [
        ("wMid", wintypes.WORD), ("wPid", wintypes.WORD),
        ("vDriverVersion", wintypes.UINT),
        ("szPname", wintypes.WCHAR * 32),
        ("wXmin", wintypes.DWORD), ("wXmax", wintypes.DWORD),
        ("wYmin", wintypes.DWORD), ("wYmax", wintypes.DWORD),
        ("wZmin", wintypes.DWORD), ("wZmax", wintypes.DWORD),
        ("wNumButtons", wintypes.DWORD),
        ("wPeriodMin", wintypes.DWORD), ("wPeriodMax", wintypes.DWORD),
        ("wRmin", wintypes.DWORD), ("wRmax", wintypes.DWORD),
        ("wUmin", wintypes.DWORD), ("wUmax", wintypes.DWORD),
        ("wVmin", wintypes.DWORD), ("wVmax", wintypes.DWORD),
        ("wCaps", wintypes.DWORD), ("wMaxAxes", wintypes.DWORD),
        ("wNumAxes", wintypes.DWORD), ("wMaxButtons", wintypes.DWORD),
        ("szRegKey", wintypes.WCHAR * 32),
        ("szOEMVxD", wintypes.WCHAR * 260),
    ]


def winmm_scan(winmm):
    found = []
    for dev_id in range(winmm.joyGetNumDevs()):
        caps = JOYCAPSW()
        caps.dwSize = ctypes.sizeof(JOYCAPSW)
        if winmm.joyGetDevCapsW(dev_id, ctypes.byref(caps),
                                ctypes.sizeof(JOYCAPSW)) == 0:
            found.append((dev_id, caps))
    return found


def pct(v, lo, hi):
    if hi <= lo:
        return 0.0
    return (v - lo) / (hi - lo) * 2.0 - 1.0


def fmt_winmm(winmm, dev_id, caps):
    info = JOYINFOEX()
    info.dwSize = ctypes.sizeof(JOYINFOEX)
    info.dwFlags = 0x000000FF
    rc = winmm.joyGetPosEx(dev_id, ctypes.byref(info))
    if rc != 0:
        return f"[{caps.szPname}] 读取失败 rc={rc}"
    return (f"[{caps.szPname} id {dev_id}] "
            f"X {pct(info.dwXpos, caps.wXmin, caps.wXmax):+.2f}  "
            f"Y {pct(info.dwYpos, caps.wYmin, caps.wYmax):+.2f}  "
            f"Z {pct(info.dwZpos, caps.wZmin, caps.wZmax):+.2f}  "
            f"R {pct(info.dwRpos, caps.wRmin, caps.wRmax):+.2f}  "
            f"按键 {info.dwButtons:016b}  POV {info.dwPOV}")


# ------------------------------------------------------------------ main ---
def main():
    watch = "-w" in sys.argv
    seconds = None
    if "-t" in sys.argv:
        i = sys.argv.index("-t")
        if i + 1 < len(sys.argv):
            try:
                seconds = float(sys.argv[i + 1])
            except ValueError:
                pass

    xi_name, xi = load_xinput()
    if VERBOSE:
        print(f"试过 XInput: {xi_name}")

    winmm = ctypes.WinDLL("winmm")
    n_winmm_slots = winmm.joyGetNumDevs()

    xdevs = xinput_scan(xi) if xi else []
    wdevs = winmm_scan(winmm)

    if not xdevs and not wdevs:
        print("[X] 未检测到手柄")
        print(f"    XInput 可用槽位: 0   winmm 槽位: {n_winmm_slots}")
        print("    排查顺序：")
        print("      1. 接收器换到机箱后置 USB 口，别用前置面板或扩展坞")
        print("      2. Win+R 输入 joy.cpl，看 Windows 自己认不认")
        print("      3. 手柄先开电源，再插接收器；换电池")
        print("      4. 部分手柄要在 PC/XInput 模式（不是 Switch/安卓模式）")
        return 1

    print(f"检测到 {len(xdevs)} 个 XInput 设备, {len(wdevs)} 个 winmm 设备")
    if xdevs:
        print(f"使用 XInput: {xi_name}, slot {xdevs[0][0]}")
    else:
        print(f"使用 winmm: {wdevs[0][1].szPname}")
    print()

    t0 = time.time()
    try:
        while True:
            if xdevs:
                slot = xdevs[0][0]
                st = XINPUT_STATE()
                rc = xi.XInputGetState(slot, ctypes.byref(st))
                if rc != 0:
                    print(f"\n[!] 手柄断开 rc={rc}"
                          f"（{ERROR_DEVICE_NOT_CONNECTED} 表示设备未连接）")
                    return 2
                line = fmt_xinput("XInput", slot, st)
            else:
                dev_id, caps = wdevs[0]
                line = fmt_winmm(winmm, dev_id, caps)

            if watch:
                print("\r" + line.ljust(110), end="", flush=True)
            else:
                print(line)
                break

            if seconds is not None and time.time() - t0 >= seconds:
                print()
                break
            time.sleep(0.02)
    except KeyboardInterrupt:
        print("\n已停止")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
