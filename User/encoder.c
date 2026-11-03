#include "encoder.h"
#include "tim.h"

#define ENCODER_CPR  1320.0f //编码器一转的脉冲数

static int16_t last_count_l, last_count_r;
static uint32_t last_tick_l, last_tick_r;
static float rpm_l = 0.0f;   /* 左轮 */
static float rpm_r = 0.0f;   /* 右轮 */

void encoder_init(void)
{
    HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
    last_count_l = __HAL_TIM_GET_COUNTER(&htim1);//读取当前脉冲数记为起点脉冲
    last_count_r = __HAL_TIM_GET_COUNTER(&htim4);
    last_tick_l = last_tick_r = HAL_GetTick();//读取当时时间为起点时间

}

float encoder_rpm_left(void)
{
    int16_t  now_count = (int16_t)__HAL_TIM_GET_COUNTER(&htim1);
    int16_t  delta   = now_count - last_count_l;
    uint32_t now_tick = HAL_GetTick();
    uint32_t delta_tick = now_tick - last_tick_l;
    
    if(delta_tick > 0)
    {
     float counts_pre_second = (float)delta / ((float)delta_tick / 1000.0f);//计算每秒的脉冲数
     rpm_l = counts_pre_second / ENCODER_CPR * 60.0f;//计算每分钟转速
    }
    last_tick_l  = now_tick;
    last_count_l = now_count;
    return rpm_l;
}

float encoder_rpm_right(void)
{
    int16_t  now_count = (int16_t)__HAL_TIM_GET_COUNTER(&htim4);
    int16_t  delta   = now_count - last_count_r;
    uint32_t now_tick = HAL_GetTick();
    uint32_t delta_tick = now_tick - last_tick_r;
    
    if(delta_tick > 0)
    {
     float counts_pre_second = (float)delta / ((float)delta_tick / 1000.0f);//计算每秒的脉冲数
     rpm_r = counts_pre_second / ENCODER_CPR * 60.0f;//计算每分钟转速
    }
    last_tick_r  = now_tick;
    last_count_r = now_count;
    return rpm_r;
}