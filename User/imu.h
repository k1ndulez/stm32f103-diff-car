#ifndef __IMU_H
#define __IMU_H

#include <stdint.h>

#define MPU_I2C_ADDR    0x68

#define MPU_WHO_AM_I_6500   0x70u
#define MPU6050_WHOAMI  0x75

#define MPU_REG_PWR_MGMT_1    0x6Bu
#define MPU_REG_GYRO_CONFIG   0x1Bu
#define MPU_REG_ACCEL_CONFIG  0x1Cu
#define MPU_REG_ACCEL_XOUT_H  0x3Bu

#define IMU_GYRO_LSB_PER_DPS   131.07f    /*每一°/s对应131.07，角速度量程±250°/s */

extern float g_imu_yaw_dps;   /* 补偿后的角速度（°/s）*/
extern float g_imu_yaw;       /* 积分后的航向角（度）*/


uint8_t imu_init(void);

typedef struct {
    int16_t ax, ay, az;   /* 加速度原始值 */
    int16_t gx, gy, gz;   /* 陀螺仪原始值 */
} imu_raw_t;

uint8_t imu_read(imu_raw_t *out);

void    imu_update(uint16_t dt_ms);

uint8_t imu_calibrate(void);              /* 静止校准零偏函数 */

#endif