# 手柄遥控（PC 端）

把 2.4G 手柄接到电脑，通过蓝牙串口控制小车。

```
手柄 ──2.4G──> USB接收器 ──> PC(python) ──蓝牙──> JDY-31 ──> STM32 ──> 电机
```

## 环境

- Windows 10/11
- Python 3.8+（本项目在 3.14 上验证过）
- 2.4G 手柄 + USB 接收器（走 XInput）
- JDY-31 蓝牙模块，已和电脑配对（配对码 `1234`）

装依赖：

```powershell
python -m pip install pyserial
```

## 快速开始

```powershell
cd <仓库根目录>

# 1. 看有哪些串口
python .\手柄\com_scan.py --list

# 2. 开车（COM14 换成你实际的蓝牙口）
python .\手柄\control_car.py --port COM14
```

`Ctrl+C` 停止，退出时会自动发一帧停车指令。

**注意**：蓝牙串口第一次打开要 4~5 秒（Windows 在建立 SPP 链路），不是卡死。

## 手柄映射

| 输入 | 控制 |
|---|---|
| 左摇杆 X（左右推）| 转向（差速）|
| 右扳机 RT | 前进油门 |
| 左扳机 LT | 后退 |

因为转向和油门是分开的，**只推摇杆不按扳机 = 原地打转**，这是差速车的正常行为。

混控公式：

```python
throttle = RT - LT          # -1 ~ +1
steer    = 左摇杆X          # -1 ~ +1
L = throttle + steer
R = throttle - steer
超范围时按比例整体缩小，保持左右速比
```

## 文件说明

| 文件 | 作用 |
|---|---|
| `control_car.py` | **开车用的主程序**。读手柄 → 混控 → 组帧 → 发串口 |
| `read_pad.py` | 手柄诊断：打印所有轴/按键原始值 |
| `com_scan.py` | 串口工具：列端口 / 扫波特率 / 回环测试 |
| `bt_link_test.py` | 蓝牙双向穿透测试（模块好坏验证）|
| `serial_min.py` | 教学用：不用 pyserial，手写 Win32 串口 |

## 指令协议

发往 STM32 的二进制帧（5 字节）：

```
0xAA  0x02  L  R  SUM
帧头   长度  左  右  (L+R)&0xFF
```

`L` / `R` 是 **int8 百分比**（-100 ~ +100），不是 rpm。
换算成转速由车端负责（`BT_RPM_MAX = 150`）。

## 自检

不接硬件也能验证混控和组帧：

```powershell
python .\手柄\control_car.py --selftest
```

输出 18 项检查结果。

只看帧、不发串口：

```powershell
python .\手柄\control_car.py --dry-run
```

## 排错

| 现象 | 原因 | 解决 |
|---|---|---|
| `未检测到手柄` | 接收器没插好 / 手柄没开 | 换后置 USB 口，先开手柄再插接收器 |
| `打开 COM14 失败` | 被别的程序占用 | 关掉串口助手 / 网页串口工具 |
| 打开超时（错误 121）| 蓝牙链路没建立 | 看模块 LED：快闪=未连接 |
| 写超时 | 蓝牙掉线 | 检查模块供电和距离 |
| 车不动但帧在变 | 车端没收到 | 查 TX/RX 是否交叉、波特率是否 9600 |

## 已知限制

- 只有 Windows 版（XInput 依赖 Windows API）
- 蓝牙链路单程延迟约 30~60 ms，不适合高速闭环
- 电脑是链路上的必要环节，关掉程序车会在 300 ms 后自动停
