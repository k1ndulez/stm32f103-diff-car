/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
#include "motor.h"
#include "oled.h"
#include "menu.h"  
#include <stdio.h>
#include <string.h>
#include "encoder.h" 
#include "pid.h"
#include "bt_ctrl.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
volatile uint32_t test_cnt= 0;
uint8_t g_bt_rx_byte;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for TEST */
osThreadId_t TESTHandle;
const osThreadAttr_t TEST_attributes = {
  .name = "TEST",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartTask02(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of TEST */
  TESTHandle = osThreadNew(StartTask02, NULL, &TEST_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**0
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
	
	motor_init();
	encoder_init();
  motor_stop();
  bt_init();
  HAL_UART_Receive_IT(&huart3, &g_bt_rx_byte, 1);
  
  pid_t pid_l, pid_r;

  pid_init(&pid_l, 3.0f, 0.8f, 0.03f, -7200.0f, 7200.0f);
  pid_init(&pid_r, 5.0f, 0.9f, 0.03f, -7200.0f, 7200.0f);
  pid_set_integral_limit(&pid_l, -3600.0f, 3600.0f);
  pid_set_integral_limit(&pid_r, -3600.0f, 3600.0f);

  uint32_t next = osKernelGetTickCount();

  for(;;)
  {
    pid_l.kp = g_params[0];
    pid_l.ki = g_params[1];
    pid_l.kd = g_params[2];

    pid_r.kp = g_params[0];
    pid_r.ki = g_params[1];
    pid_r.kd = g_params[2];

    bt_tick(HAL_GetTick());

    float target_l = (float)bt_get_left_rpm();
    float target_r = (float)bt_get_right_rpm();
    float meas_l = encoder_rpm_left();
    float meas_r = encoder_rpm_right();

    float duty_l = pid_calc(&pid_l, target_l, meas_l);
    float duty_r = pid_calc(&pid_r, target_r, meas_r);

    
    motor_set((int16_t)duty_l, (int16_t)duty_r);

    /* 遥测：5 Hz 发送。
       telem_buf 必须是 static —— DMA 在本函数返回后还会继续读它，
       放栈上会被下一轮循环覆盖成垃圾。                                          */
    static char     telem_buf[48];
    static uint32_t telem_div = 0;

    if (++telem_div >= 10)          /* 20 ms × 10 = 200 ms，给 21 ms 的传输留足时间 */
    {
      telem_div = 0;
      int n = snprintf(telem_buf, sizeof(telem_buf), "%d %d %d %d\n",
                       (int)(target_l * 10.0f),
                       (int)(target_r * 10.0f),
                       (int)(meas_l * 10.0f),
                       (int)(meas_r * 10.0f));

      /* 用阻塞发送：5 Hz 下每 200 ms 才卡 21 ms，没必要上 DMA。
         注意：本工程的 .ioc 里已经没有 USART3 的 DMA 配置，
         调 HAL_UART_Transmit_DMA 会因 huart3.hdmatx == NULL 直接 HardFault。 */
      HAL_UART_Transmit(&huart3, (uint8_t *)telem_buf, (uint16_t)n, 50);
    }

    next += 20;  
    osDelayUntil(next);//相隔绝对20ms
	}
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTask02 */
/**
* @brief Function implementing the TEST thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask02 */
void StartTask02(void *argument)
{
  /* USER CODE BEGIN StartTask02 */
  key_init();
  osDelay(50);            /* 等 OLED 上电稳定 */
  OLED_Init();
  menu_init();  
  static uint32_t last_ui = 0;
  /* Infinite loop */
  for(;;)
  {
    key_event_t e = key_scan();
    if (e != KEY_NONE)
    {
      menu_handle(e);/* 内部改状态 + 重绘 */
      last_ui = HAL_GetTick();
    }
    else if (HAL_GetTick() - last_ui > 200)   /* 5 Hz 自动刷新 */
    {
      last_ui = HAL_GetTick();
      menu_refresh();
    }
    
    osDelay(10);
  }
  /* USER CODE END StartTask02 */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        bt_feed(g_bt_rx_byte, HAL_GetTick());
        HAL_UART_Receive_IT(&huart3, &g_bt_rx_byte, 1);
    }
}
/* USER CODE END Application */

