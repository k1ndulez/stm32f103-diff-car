#ifndef __ENCODER_H
#define __ENCODER_H

#include<stdint.h>

void encoder_init(void);
float encoder_rpm_left(void);
float encoder_rpm_right(void);

#endif