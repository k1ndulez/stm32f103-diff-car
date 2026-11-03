/**
  ******************************************************************************
  * @file    bt_ctrl.h
  * @brief   蓝牙指令接收：同时支持二进制帧和文本行
  *
  * ---- 格式一：二进制帧（PC 端 control_car.py 用） ----
  *   0xAA 0x02 L R SUM
  *   帧头  长度 左 右 校验和
  *   L / R 为 int8，单位 %（-100 ~ +100）
  *   SUM = (L + R) & 0xFF
  *
  * ---- 格式二：文本行（手机串口 App 用） ----
  *   "100,33\n"     左轮 100%，右轮 33%
  *   "100\n"        只写一个数 → 两轮同速
  *   "-50,-50\n"    倒车
  *   分隔符可用 , ; :   行尾可用 \n 或 \r
  *
  * 两种格式靠首字节自动识别：0xAA 走二进制，数字或 '-' 走文本。
  *
  * 本模块不依赖任何 HAL，只用 stdint.h，
  * 因此可以在 PC 上直接编译做单元测试。
  ******************************************************************************
  */
#ifndef __BT_CTRL_H
#define __BT_CTRL_H

#include <stdint.h>

#define BT_FRAME_HEAD        0xAAu
#define BT_FRAME_LEN         0x02u
#define BT_FRAME_SIZE        5u

/* 文本行最大长度（含结尾 '\0'） */
#define BT_TEXT_MAX          24u

/* 100% 对应的目标转速（rpm）。改这一个数就能整体调快慢 */
#define BT_RPM_MAX           150

/* 超过这个时间没收到完整帧，目标强制归零（毫秒） */
#define BT_LINK_TIMEOUT_MS   300u

/** 清空状态，上电时调一次 */
void bt_init(void);

/**
 * @brief 喂一个字节（在 UART 接收完成回调里调用）
 * @param byte   刚收到的字节
 * @param now_ms 当前毫秒时基（HAL_GetTick()）
 */
void bt_feed(uint8_t byte, uint32_t now_ms);

/**
 * @brief 超时检测，在主循环里周期性调用（20 ms 一次即可）
 * @param now_ms 当前毫秒时基
 */
void bt_tick(uint32_t now_ms);

/** 左右轮目标转速，单位 rpm */
int16_t bt_get_left_rpm(void);
int16_t bt_get_right_rpm(void);

/** 1 = 链路正常（最近 300 ms 内收到过有效帧），0 = 已超时 */
uint8_t bt_link_ok(void);

/** 统计，调试用：收到的好帧数 / 丢弃的坏帧数 */
uint32_t bt_frame_count(void);
uint32_t bt_error_count(void);

#endif /* __BT_CTRL_H */
