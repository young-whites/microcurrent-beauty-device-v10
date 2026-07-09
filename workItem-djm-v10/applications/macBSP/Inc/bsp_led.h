/*
 * bsp_led.h
 *
 *  Created on: May 16, 2024
 *      Author: zphu
 */

#ifndef MACBSP_INC_BSP_LED_H_

#define MACBSP_INC_BSP_LED_H_
#include "bsp_sys.h"

#define             LED_NUM             (30)
/****************************** LED 函数宏定义***************************************/
#define             macLED_GREEN_OFF()                          HAL_GPIO_WritePin ( LED_GREEN_GPIO_Port, LED_GREEN_Pin , GPIO_PIN_RESET )
#define             macLED_GREEN_ON()                           HAL_GPIO_WritePin ( LED_GREEN_GPIO_Port, LED_GREEN_Pin , GPIO_PIN_SET )

/*LED名称类型*/
typedef	enum
{
    LED_Name_Green = (0x01),
}LED_Name_TypeDef;



/************************** PAD 函数声明********************************/
void LED_Init(void);
void LED_Out(int8_t ledName, int8_t ledState);
int8_t 	LED_GetNumber(void);
void LED_Off(int8_t ledName);
void LED_On(int8_t ledName);
void LED_Toggle(int8_t ledName);
void LED_Grad(int8_t ledName);
void LED_BlinkSetCycleDuty(int8_t ledName, int8_t Cycle, int8_t Duty);
void LED_Blink(int8_t ledName, int8_t cry, int8_t mute, int8_t repeat);
void LED_Fancy(int8_t mode);
void LED_DrvScan(void);




#endif /* MACBSP_INC_BSP_LED_H_ */
