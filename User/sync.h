/*直线行驶左右轮交叉补偿*/
#ifndef __SYNC_H
#define __SYNC_H
#include <stdint.h>

#define SYNC_KC         0.5f//耦合强度以控制收敛速度
#define SYNC_MAX_CORR   30.0f//单次速度上限，保险阀

void sync_correct(float tl, float tr, float ml, float mr, float *out_l, float *out_r);

#endif

