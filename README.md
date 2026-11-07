# stm32f103-diff-car

STM32F103 两轮差速小车。支持**手柄遥控**和**手机遥控**两种输入，同一套固件自动识别。

## 硬件

| 部件 | 说明 |
|---|---|
| 主控 | STM32F103C8T6 |
| 电机 | 双直流减速电机 + 霍尔编码器 |
| 显示 | 0.96" OLED（I²C1，400 kHz）|
| 按键 | 4 个（上一页 / 下一页 / 确认 / 返回）|
| 蓝牙 | JDY-31（经典蓝牙 SPP，USART3，9600 8N1）|

主要引脚：

| 功能 | 引脚 |
|---|---|
| USART3 TX / RX | PB10 / PB11 |
| 蓝牙状态输入 | PA0（高 = 已连接）|
| OLED I²C | PB8 / PB9 |
| 按键 | PA15 / PB3 / PB4 / PB5 |

## 控制方式

### 手机（最简单）

手机装任意**蓝牙串口 App**（安卓；iOS 不支持 SPP），配对 `JDY-31-SPP`（配对码 1234），
然后直接发文本：

```
100,33      左轮 100%，右轮 33%
100         只写一个数 → 两轮同速
-50,-50     倒车
0,0         停
```

### 电脑 + 游戏手柄

见 [手柄/README.md](手柄/README.md)。需要 2.4G 手柄 + USB 接收器。

## 协议

`bt_ctrl` 支持两种格式，靠**首字节自动识别**：

| 首字节 | 格式 |
|---|---|
| `0xAA` | 二进制帧 `AA 02 L R SUM`（5 字节定长，带校验）|
| 数字或 `-` | 文本行 `L,R\n` |

两种格式共用同一个状态机，上层代码无感知。

**链路保护**：超过 300 ms 没收到有效帧，左右目标强制归零。

## 固件结构

```
Core/            CubeMX 生成（时钟、GPIO、USART3、I2C1、TIM）
User/
  ├── bt_ctrl.c    蓝牙指令解析（二进制 + 文本，无 HAL 依赖，可单测）
  ├── motor.c      PWM 输出
  ├── encoder.c    编码器测速
  ├── pid.c        增量式 PID
  ├── menu.c       OLED 菜单与参数编辑
  ├── oled.c       显示驱动
  └── key.c        按键扫描
```

FreeRTOS 两个任务：

| 任务 | 周期 | 职责 |
|---|---|---|
| `StartDefaultTask` | 20 ms | 读蓝牙目标 → PID → 电机；5 Hz 发遥测 |
| `StartTask02` | 10 ms | 按键扫描 + OLED 刷新 |

## 构建

**CMake（推荐）**

```bash
cmake --preset Debug
cmake --build --preset Debug
```

**Keil MDK**：打开 `MDK-ARM/my_first_programe.uvprojx`

## 调试

串口遥测（USART3，5 Hz）：

```
目标L 目标R 实测L 实测R      单位 rpm ×10
```

例如 `1500 490 1480 495` 表示左轮目标 150 rpm、右轮 49 rpm。

OLED 的 `BT_CTRL` 页显示链路状态和左右目标：

```
BT OK L 150 R  49      ← 链路正常
BT -- L   0 R   0      ← 超时断开
```
