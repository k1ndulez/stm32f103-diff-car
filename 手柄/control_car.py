# -*- coding: utf-8 -*-
"""
手柄 → 电脑 → 蓝牙串口 → 小车  的控制端

映射（按你选的）:
    左摇杆 X   → 转向
    右扳机 RT  → 前进油门
    左扳机 LT  → 后退

指令帧（STM32 端按这个解析）:
    0xAA  LEN  L  R  SUM
    0xAA  0x02 L  R  (L+R)&0xFF
    L / R 为 int8：-128..127，表示最大速度的百分比（-100..100）

用法:
    python control_car.py --list              # 列出串口
    python control_car.py --port COM5         # 正常开车
    python control_car.py --port auto         # 自动找蓝牙串口
    python control_car.py --dry-run           # 不发串口，只打印帧（没硬件时用）
    python control_car.py --selftest          # 自检：混控与组帧
"""
import argparse
import ctypes
import sys
import time
from ctypes import wintypes

# 只有在输出被重定向（非控制台）时才强制 UTF-8；
# 直接跑在 Windows 控制台里时交给 Python 原生处理，否则中文会乱码。
if not sys.stdout.isatty():
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None

# ------------------------------------------------------------------ XInput ---
XUSER_MAX_COUNT = 4
ERROR_SUCCESS = 0


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
            return ctypes.WinDLL(name)
        except OSError:
            continue
    return None


def find_slot(dll):
    for slot in range(XUSER_MAX_COUNT):
        st = XINPUT_STATE()
        if dll.XInputGetState(slot, ctypes.byref(st)) == ERROR_SUCCESS:
            return slot
    return None


def norm(v):
    return max(-1.0, min(1.0, v / 32767.0))


# ------------------------------------------------------------------- 混控 ---
def deadzone(v, dz):
    """死区 + 重映射：dz 以内归零，dz..1 线性铺满 0..1"""
    if abs(v) <= dz:
        return 0.0
    s = 1.0 if v > 0 else -1.0
    return s * (abs(v) - dz) / (1.0 - dz)


def mix(steer, throttle):
    """差速混控 -> (left, right)，单位百分比 -100..100

    steer > 0 时左轮更快（向右转）。

    超出 ±1 时**按比例整体缩小**（除以 max(1,|l|,|r|)），
    而不是各自截断——这样左右轮速比保持不变，满油门时不会把转向裁掉。
    例: 油门 1.0 + 转向 0.5 -> (1.5, 0.5) -> 整体 /1.5 -> (100, 33)
    """
    l = throttle + steer
    r = throttle - steer
    m = max(1.0, abs(l), abs(r))
    l /= m
    r /= m
    return int(round(l * 100)), int(round(r * 100))


def build_frame(left, right):
    lb = left & 0xFF
    rb = right & 0xFF
    return bytes([0xAA, 0x02, lb, rb, (lb + rb) & 0xFF])


def parse_frame(buf):
    """给 --selftest 和调试用：从字节流里切出一帧"""
    if len(buf) < 5 or buf[0] != 0xAA or buf[1] != 0x02:
        return None
    l, r, s = buf[2], buf[3], buf[4]
    if ((l + r) & 0xFF) != s:
        return None
    if l > 127:
        l -= 256
    if r > 127:
        r -= 256
    return l, r


# --------------------------------------------------------------- 串口 ---
def pick_port():
    if not list_ports:
        return None
    keys = ("bluetooth", "hc-05", "hc-06", "spp", "串行")
    cands = []
    for p in list_ports.comports():
        desc = f"{p.description} {p.hwid}".lower()
        if any(k in desc for k in keys):
            cands.append(p.device)
    return cands[0] if cands else None


def open_port(port, baud, tries=3):
    if serial is None:
        print("[X] 没有 pyserial，请先运行: python -m pip install pyserial")
        return None
    name = pick_port() if port == "auto" else port
    if not name:
        print("[X] 找不到串口。用 --list 看可用端口，或直接指定 --port COM5")
        return None

    for i in range(tries):
        print(f"正在打开 {name} ...（蓝牙链路建立可能要几秒，别急着 Ctrl+C）")
        try:
            sp = serial.Serial(name, baud, timeout=0, write_timeout=1.0)
            print(f"[OK] 串口 {name} @ {baud} 已打开")
            time.sleep(0.3)          # 蓝牙链路刚建立，先让驱动喘口气
            return sp
        except Exception as e:
            print(f"[!] 第 {i} 次打开失败: {e}")
            if i + 1 < tries:
                print("    1 秒后重试...")
                time.sleep(1.0)

    print(f"[X] 打开 {name} 失败")
    print("    排查：")
    print("      1. 模块是否上电？LED 常亮/双闪=已连接，快闪=没连上，不亮=没电")
    print("      2. 设置 → 蓝牙和其他设备 → 点 JDY-31-SPP → 连接")
    print("      3. 还不行就删掉设备重新配对（配对码 1234）")
    return None


# ----------------------------------------------------------------- 自检 ---
def selftest():
    ok = 0
    fail = 0

    def check(name, got, want):
        nonlocal ok, fail
        if got == want:
            ok += 1
            print(f"  [PASS] {name}: {got}")
        else:
            fail += 1
            print(f"  [FAIL] {name}: got {got}, want {want}")

    print("混控:")
    check("静止", mix(0, 0), (0, 0))
    check("满油门直行", mix(0, 1), (100, 100))
    check("满倒车直行", mix(0, -1), (-100, -100))
    check("右转让左轮快", mix(1, 1), (100, 0))
    check("左转让右轮快", mix(-1, 1), (0, 100))
    check("半转半油（比例缩放 3:1）", mix(0.5, 1), (100, 33))
    check("原地右转", mix(1, 0), (100, -100))

    print("死区:")
    check("0.1 归零", deadzone(0.10, 0.12), 0.0)
    check("1.0 满值", round(deadzone(1.0, 0.12), 3), 1.0)
    check("-1.0 满值", round(deadzone(-1.0, 0.12), 3), -1.0)
    check("死区外一点为正", deadzone(0.13, 0.12) > 0, True)

    print("组帧:")
    check("零帧", build_frame(0, 0), bytes([0xAA, 0x02, 0x00, 0x00, 0x00]))
    check("满速直行", build_frame(100, 100), bytes([0xAA, 0x02, 0x64, 0x64, 0xC8]))
    check("倒车 -100", build_frame(-100, -100),
          bytes([0xAA, 0x02, 0x9C, 0x9C, 0x38]))
    check("解析回读", parse_frame(build_frame(100, 100)), (100, 100))
    check("解析负数", parse_frame(build_frame(-50, 60)), (-50, 60))
    check("坏校验丢弃", parse_frame(bytes([0xAA, 0x02, 0x64, 0x64, 0x00])), None)
    check("帧头错丢弃", parse_frame(bytes([0x55, 0x02, 0x00, 0x00, 0x00])), None)

    print(f"\n结果: {ok} 通过, {fail} 失败")
    return 0 if fail == 0 else 1


# ------------------------------------------------------------------ 主程序 ---
def safe_write(sp, data):
    """写串口失败不抛异常，只返回 False。
    蓝牙 SPP 偶尔会短暂写超时，不该让整个程序崩掉。"""
    try:
        sp.write(data)
        return True
    except Exception:
        return False


def main():
    ap = argparse.ArgumentParser(add_help=True)
    ap.add_argument("--port", default="auto", help="COM5 或 auto")
    ap.add_argument("--baud", type=int, default=9600,
                    help="默认 9600（JDY-31 出厂值，AT 改不动就保持）")
    ap.add_argument("--hz", type=float, default=50.0)
    ap.add_argument("--dz", type=float, default=0.12, help="转向死区")
    ap.add_argument("--t", type=float, default=None, help="运行秒数后退出")
    ap.add_argument("--dry-run", action="store_true", help="不打开串口，只打印帧")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    if args.selftest:
        return selftest()

    if args.list:
        if not list_ports:
            print("[X] 没有 pyserial")
            return 1
        ports = list(list_ports.comports())
        if not ports:
            print("没有任何串口")
        for p in ports:
            print(f"{p.device:<10} {p.description}")
        return 0

    xi = load_xinput()
    if xi is None:
        print("[X] 加载 XInput 失败")
        return 1
    slot = find_slot(xi)
    if slot is None:
        print("[X] 未检测到手柄（先开手柄电源，再插接收器）")
        return 1
    print(f"[OK] 手柄在 XInput slot {slot}")

    sp = None
    if not args.dry_run:
        sp = open_port(args.port, args.baud)
        if sp is None:
            return 2
    else:
        print("[--] dry-run：只打印帧，不发送")

    period = 1.0 / args.hz
    t0 = time.time()
    last_print = 0.0
    fail = 0
    try:
        while True:
            st = XINPUT_STATE()
            if xi.XInputGetState(slot, ctypes.byref(st)) != ERROR_SUCCESS:
                print("\n[!] 手柄断开")
                return 3

            g = st.Gamepad
            steer = deadzone(norm(g.sThumbLX), args.dz)
            throttle = g.bRightTrigger / 255.0 - g.bLeftTrigger / 255.0
            left, right = mix(steer, throttle)
            pkt = build_frame(left, right)

            if sp:
                if safe_write(sp, pkt):
                    fail = 0
                else:
                    fail += 1
                    if fail >= 20:        # 50 Hz 下约 0.4 秒全失败
                        print("\n[X] 连续 20 次写串口失败 —— 蓝牙链路断了")
                        print("    → 看模块 LED：快闪=没连上")
                        print("    → 设置 → 蓝牙 → 点 JDY-31-SPP → 连接")
                        return 4

            now = time.time()
            if now - last_print >= 0.1:
                last_print = now
                print(f"\r转向 {steer:+.2f}  油门 {throttle:+.2f}  "
                      f"L {left:+4d}  R {right:+4d}   "
                      f"帧 {pkt.hex(' ').upper()}"
                      + (f"   写失败 {fail}" if fail else ""),
                      end="", flush=True)

            if args.t is not None and now - t0 >= args.t:
                break
            time.sleep(period)
    except KeyboardInterrupt:
        pass
    finally:
        if sp:
            try:
                sp.write(build_frame(0, 0))     # 尽量发停车帧，失败也不许崩
            except Exception:
                pass
            try:
                sp.close()
            except Exception:
                pass
            print("\n结束（已发送停车帧）" if fail == 0
                  else "\n结束（写串口失败过，停车帧可能没送达）")
        else:
            print("\n结束（dry-run，未发送）")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
