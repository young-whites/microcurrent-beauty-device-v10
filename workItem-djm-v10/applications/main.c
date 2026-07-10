/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06-08     RT-Thread    first version
 * 2026-06-29     auto-gen     Added NTC sensor and PID controller init
 */

#include <rtthread.h>

#include "bsp_sys.h"
#include "bsp_hard.h"
#include "power_task.h"
#include "ntc_sensor.h"
#include "temp_pid.h"
#include "nnc6521.h"
#include "nnc6521_waveform_config.h"

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

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
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */

  /* Note: bsp_hard.h is included via bsp_sys.h */

  /* Initialize NTC temperature sensor module */
  ntc_sensor_init();

  /* Initialize temperature PID controller */
  temp_pid_init();

  /* Initialize NNC6521 waveform generator */
  nnc6521_gpio_init();
  nnc6521_init(NNC6521_CHIP_1);
  nnc6521_init(NNC6521_CHIP_2);
  nnc6521_analog_enable(NNC6521_CHIP_1, WAVEFORM_GEN_CH0);
  nnc6521_analog_enable(NNC6521_CHIP_2, WAVEFORM_GEN_CH0);

  /* Apply default waveform 1 (Power Smooth) at 50% current */
  waveform_apply(NNC6521_CHIP_1, WAVEFORM_GEN_CH0, 1, WAVEFORM_DEFAULT_PCT);

  /* Print waveform info */
  {
      const waveform_config_t *wfc = waveform_get_config(1);
      if (wfc != NULL) {
          rt_kprintf("Waveform #%d: %s, %d Hz, %lu-%lu mA\n",
                     wfc->id, wfc->name, wfc->frequency,
                     wfc->min_current, wfc->max_current);
      }
      rt_kprintf("NNC6521 initialized, default waveform applied.\n");
  }

  /* Initialize power management task (after key/beep/led drivers) */
  power_task_init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
      rt_thread_mdelay(100);
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}
