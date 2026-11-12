#include "imu.h"
#include "i2c.h"

float g_imu_yaw_dps = 0.0f;/*yaw轴角速度*/
float g_imu_yaw     = 0.0f;/*yaw轴角度*/
static float s_bias_gz = 0.0f;/*z轴零偏*/

uint8_t imu_init(void)
{
    /*检测陀螺仪i2c通信*/
    uint8_t id= 0;
    HAL_StatusTypeDef imu_status;
    imu_status= HAL_I2C_Mem_Read(&hi2c1,  (uint16_t)(MPU_I2C_ADDR  << 1),   MPU6050_WHOAMI,  I2C_MEMADD_SIZE_8BIT,  &id, 1, 100);
    if(imu_status != HAL_OK)
    {
        return 0xFF;
    }
    if (id != MPU_WHO_AM_I_6500) return id; 
    
    /*唤醒装置，并对陀螺仪与加速度量程进行配置*/
    
    uint8_t val;
    val = 0x01;
    
    if ( HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(MPU_I2C_ADDR << 1), MPU_REG_PWR_MGMT_1, I2C_MEMADD_SIZE_8BIT, &val, 1, 100) != HAL_OK) return 0xFE;
    HAL_Delay(100); //唤醒延时
    
    val = 0x00;//陀螺量程 ±250 °/s
    if (HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(MPU_I2C_ADDR << 1), MPU_REG_GYRO_CONFIG, I2C_MEMADD_SIZE_8BIT, &val, 1, 100) != HAL_OK) return 0xFD;   

    val = 0x00;//加速度量程 ±2g
    if (HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(MPU_I2C_ADDR << 1), MPU_REG_ACCEL_CONFIG, I2C_MEMADD_SIZE_8BIT, &val, 1, 100)!= HAL_OK) return 0xFC;
    

    return 0;
    
}

uint8_t imu_read(imu_raw_t *out)
{
    uint8_t  buf[14];
    HAL_StatusTypeDef imu_status;
    imu_status= HAL_I2C_Mem_Read(&hi2c1,  (uint16_t)(MPU_I2C_ADDR  << 1), MPU_REG_ACCEL_XOUT_H,  I2C_MEMADD_SIZE_8BIT, buf, 14, 100);//检验
    if (imu_status != HAL_OK) return 1;

/*数据解算*/
    out->ax = (int16_t)((buf[0]  << 8) | buf[1]);
    out->ay = (int16_t)((buf[2]  << 8) | buf[3]);
    out->az = (int16_t)((buf[4]  << 8) | buf[5]);
    /* buf[6] buf[7] 是温度，跳过 */
    out->gx = (int16_t)((buf[8]  << 8) | buf[9]);
    out->gy = (int16_t)((buf[10] << 8) | buf[11]);
    out->gz = (int16_t)((buf[12] << 8) | buf[13]);

 return 0;
}

uint8_t imu_calibrate(void)
{
    int32_t   sum = 0;
    imu_raw_t r;

    for (uint16_t i = 0; i < 200; i++)
    {
        if (imu_read(&r) != 0) return 1;
        sum += r.gz;
        HAL_Delay(5);
    }

    s_bias_gz = (float)sum / 200.0f;//两百个数据取平均得到零偏

    /* 校准完成时把航向清零：车头当前朝向就是 0° */
    g_imu_yaw     = 0.0f;
    g_imu_yaw_dps = 0.0f;
    return 0;
}

void imu_update(uint16_t dt_ms, uint8_t stationary)//角速度换算角度
{
    imu_raw_t r;

    if (imu_read(&r) != 0) return;

   float dps = ((r.gz - s_bias_gz) / IMU_GYRO_LSB_PER_DPS) * IMU_YAW_SIGN;

    if (stationary && dps > -IMU_ZUPT_RATE_DPS && dps < IMU_ZUPT_RATE_DPS)
    {
    s_bias_gz += IMU_ZUPT_ALPHA * (r.gz - s_bias_gz);
    g_imu_yaw_dps = 0.0f;
    return;
    }

    g_imu_yaw_dps = dps;
    g_imu_yaw += dps * dt_ms/1000;//角速度积分

    if (g_imu_yaw >  180.0f) g_imu_yaw -= 360.0f;
    if (g_imu_yaw < -180.0f) g_imu_yaw += 360.0f;//限幅
}
