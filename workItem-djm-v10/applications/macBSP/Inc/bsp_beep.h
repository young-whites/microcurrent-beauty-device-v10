/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-08-31     Administrator       the first version
 */
#ifndef APPLICATIONS_MACBSP_INC_BSP_BEEP_H_

#define APPLICATIONS_MACBSP_INC_BSP_BEEP_H_

#include <applications/macSYS/Inc/macSYS.h>

/******************** BEEP 函数宏定义 **************************/
#define             macBEEP_OFF()                            HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
#define             macBEEP_ON()                             HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_SET);

/************************** BEEP 函数声明********************************/
void Beep_Init(void);
void BEEP_Off(void);
void BEEP_On(void);
void BEEP_SetCycleDuty(int16_t Cycle, int16_t Duty);
void BEEP_Blink(int8_t cry, int8_t mute, int8_t repeat);
void BEEP_DrvScan(void);
int beepTimer_Init(void);

#endif /* APPLICATIONS_MACBSP_INC_BSP_BEEP_H_ */
