/*
 * bsp_led.h
 *
 *  Created on: May 16, 2024
 *      Author: zphu
 */

#ifndef MACBSP_INC_BSP_LED_H_

#define MACBSP_INC_BSP_LED_H_
#include "bsp_sys.h"

#define             LED_NUM             (1)
/****************************** LED macro definitions ***************************************/
#define             macLED_START_OFF()          HAL_GPIO_WritePin ( START_LED_CTRL_GPIO_Port, START_LED_CTRL_Pin , GPIO_PIN_RESET )
#define             macLED_START_ON()           HAL_GPIO_WritePin ( START_LED_CTRL_GPIO_Port, START_LED_CTRL_Pin , GPIO_PIN_SET )

/* LED name type */
typedef	enum
{
    LED_Name_Green = (0x01),
}LED_Name_TypeDef;



/************************** LED function declarations ********************************/
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
