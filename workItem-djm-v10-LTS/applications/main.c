/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-07-10     RT-Thread    first version
 */

#include <rtthread.h>

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#include "bsp_sys.h"
#include "nnc6521.h"
#include "nnc6521_waveform_config.h"
#include "power_task.h"
#include "ntc_sensor.h"
#include "temp_pid.h"


/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* UART1/UART2 already initialized by RT-Thread driver + protocol module */
  MX_ADC1_Init();

  /* Initialize NTC temperature sensors (RT-Thread ADC framework) */
  ntc_sensor_init();

  /* Initialize temperature PID controller */
  temp_pid_init();

  /* Initialize NNC6521 waveform generator */
  nnc6521_gpio_init();
  nnc6521_init(NNC6521_CHIP_1);
  nnc6521_init(NNC6521_CHIP_2);
  nnc6521_analog_enable(NNC6521_CHIP_1, WAVEFORM_GEN_CH0);  /* Handle A */
  nnc6521_analog_enable(NNC6521_CHIP_1, WAVEFORM_GEN_CH1);  /* Handle B */
  nnc6521_analog_enable(NNC6521_CHIP_2, WAVEFORM_GEN_CH0);  /* Handle C */
  rt_kprintf("[MAIN] NNC6521 initialized (dual chip, 3 channels)\n");

  /* Initialize power management task (must be after NNC6521 init) */
  power_task_init();

  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
      rt_thread_mdelay(1000);
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}
