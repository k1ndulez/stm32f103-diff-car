#ifndef __HEADING_H_
#define __HEADING_H_

#define HEADING_KP              (1.20f)
#define HEADING_KD              (0.60f)
#define HEADING_MIN_RPM         (20.0f)
#define HEADING_MAX_RPM         (90.0f)
#define HEADING_TOL_DEG         (3.0f) //容差
#define HEADING_RATE_TOL_DPS    (35.0f)
#define HEADING_TIMEOUT_MS      (5000u)
#define HEADING_OUTPUT_SIGN     (1.0f) //方向校准开关

#include <stdint.h>

typedef enum {
    HEADING_IDLE = 0,//未转向状态
    HEADING_TURN,//正在转向状态
    HEADING_HOLD,//已到位，锁航向状态
    HEADING_TIMEOUT//超时状态
} heading_state_t;

void heading_init(void);
void heading_start(int16_t target_deg, uint32_t now_ms);
void heading_update(float yaw_deg, float yaw_rate_dps, uint32_t now_ms);
void heading_cancel(void);
uint8_t heading_active(void);
int16_t heading_left_rpm(void);
int16_t heading_right_rpm(void);

#endif