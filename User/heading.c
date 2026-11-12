#include "heading.h"

static heading_state_t s_state;
static int16_t         s_target;
static uint32_t        s_start_ms;
static int16_t         s_left_rpm;
static int16_t         s_right_rpm;

/* 0=KP 1=KD 2=MIN_RPM 3=MAX_RPM，可由 OLED 菜单在线修改 */
float g_heading_params[4] =
{
    HEADING_KP,
    HEADING_KD,
    HEADING_MIN_RPM,
    HEADING_MAX_RPM
};



static float wrap180(float a)//缩短路径
{
    while (a > 180.0f)  a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

static float abs_f(float x)//绝对值
{
    return (x < 0.0f) ? -x : x;
}

void heading_init(void)
{
    s_state = HEADING_IDLE;
    s_target = 0;
    s_start_ms = 0;
    s_left_rpm = 0;
    s_right_rpm = 0;
}

void heading_start(int16_t target_deg, uint32_t now_ms)
{
int16_t target = wrap180(target_deg);
if((s_state == HEADING_TURN || s_state == HEADING_HOLD || s_state == HEADING_TIMEOUT) && target == s_target)
{
    return;    
}
else
{
    s_target = target;
    s_start_ms = now_ms;
    s_state    = HEADING_TURN;
    s_left_rpm = 0;
    s_right_rpm = 0;
}
}

void heading_update(float yaw_deg, float yaw_rate_dps, uint32_t now_ms)
{
    float err;
    float rate;
    float u;
    
    if (s_state != HEADING_TURN && s_state != HEADING_HOLD) return;
    if (s_state == HEADING_TURN && (uint32_t)(now_ms - s_start_ms) > HEADING_TIMEOUT_MS)// 5 s 超时
    {    
        s_state = HEADING_TIMEOUT;
        s_left_rpm = 0;
        s_right_rpm = 0;
        return;
    }
    
    err  = wrap180((float)s_target - yaw_deg);
    rate = yaw_rate_dps;

    if (abs_f(err) <= HEADING_TOL_DEG && abs_f(rate) <= HEADING_RATE_TOL_DPS)//到位
    {
        if (s_state == HEADING_TURN) s_state = HEADING_HOLD;
        {
        s_left_rpm = 0;
        s_right_rpm = 0;
        return;
        }
    }

    u = g_heading_params[0]*err - g_heading_params[1]*rate;

    if (u >  g_heading_params[3]) u =  g_heading_params[3];
    if (u < -g_heading_params[3]) u = -g_heading_params[3];

    if (u > 0.0f && u < g_heading_params[2])  u =  g_heading_params[2];
    if (u < 0.0f && u > -g_heading_params[2]) u = -g_heading_params[2];//输出转向速度限幅

    u *= HEADING_OUTPUT_SIGN;

    s_left_rpm  = (int16_t)(-u);
    s_right_rpm = (int16_t)(u);
}

void heading_cancel(void)
{
    s_state = HEADING_IDLE;
    s_left_rpm = 0;
    s_right_rpm = 0;
}

uint8_t heading_active(void)
{
    if (s_state == HEADING_TURN || s_state == HEADING_HOLD)
    {
    return 1u;
    }
    return 0u;
}

heading_state_t heading_state(void)
{
    return s_state;
}

int16_t heading_target_deg(void)
{
    return s_target;
}

int16_t heading_left_rpm(void)
{
    return s_left_rpm;
}

int16_t heading_right_rpm(void)
{
    return s_right_rpm;
}
