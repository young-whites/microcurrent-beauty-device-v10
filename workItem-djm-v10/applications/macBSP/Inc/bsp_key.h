/*
 * bsp_key.h
 *
 *  Created on: May 16, 2024
 *      Author: zphu
 */

#ifndef MACBSP_INC_BSP_KEY_H_
#define MACBSP_INC_BSP_KEY_H_

#include <applications/macSYS/Inc/macSYS.h>



#define		KEY_NUM			(3)		// 按键数量
//------------------------------------------------------------------------
typedef	enum
{
    KeyA_PRESS= (0x01),
    KeyB_PRESS,
    KeyC_PRESS,
}KEY_Val_TypeDef;
//------------------------------------------------------------------------
typedef	enum {					    // 按键事件类型
    KEY_Evt_Press   = (0x80),		// 按下
    KEY_Evt_Release = (0x40),		// 松开
    KEY_Evt_Long2S  = (0x20),		// 长按2s
    KEY_Evt_Long4S  = (0x10),		// 长按4s
}KEY_Evt_TypeDef;
//------------------------------------------------------------------------


void KEY_Scan(void);
void KEY_DrvScan(void);
uint8_t KEY_Read(void);
void KEY_Write(uint8_t value);




#endif /* MACBSP_INC_BSP_KEY_H_ */
