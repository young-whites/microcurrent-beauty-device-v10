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
// #include "ntc_sensor.h"
// #include "temp_pid.h"
// #include "nnc6521.h"
// #include "nnc6521_waveform_config.h"

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

int main(void)
{
  /* RT-Thread hw_board_init() already handled:
   * HAL_Init, SystemClock_Config, GPIO, UART1, UART2
   * DO NOT call them again here.
   */

  rt_kprintf("[MAIN] System started\n");

#if 0
  /* NTC / PID / NNC6521 - temporarily disabled for debug */
  ntc_sensor_init();
  temp_pid_init();
  nnc6521_gpio_init();
  nnc6521_init(NNC6521_CHIP_1);
  nnc6521_init(NNC6521_CHIP_2);
  nnc6521_analog_enable(NNC6521_CHIP_1, WAVEFORM_GEN_CH0);
  nnc6521_analog_enable(NNC6521_CHIP_2, WAVEFORM_GEN_CH0);
  waveform_apply(NNC6521_CHIP_1, WAVEFORM_GEN_CH0, 1, WAVEFORM_DEFAULT_PCT);
  {
      const waveform_config_t *wfc = waveform_get_config(1);
      if (wfc != NULL) {
          rt_kprintf("Waveform #%d: %s, %d Hz, %lu-%lu mA\n",
                     wfc->id, wfc->name, wfc->frequency,
                     wfc->min_current, wfc->max_current);
      }
      rt_kprintf("NNC6521 initialized, default waveform applied.\n");
  }
#endif

  /* Initialize power management task (after key/beep/led drivers) */
  power_task_init();

  rt_kprintf("[MAIN] Init done, entering idle loop\n");

  return RT_EOK;
}
