/*
 * bsp_key.h
 *
 *  Created on: May 16, 2024
 *      Author: zphu
 */

#ifndef MACBSP_INC_BSP_KEY_H_
#define MACBSP_INC_BSP_KEY_H_

#include "bsp_sys.h"



#define		KEY_NUM			(1)		// Number of keys
//------------------------------------------------------------------------
typedef	enum
{
    KeyA_PRESS= (0x01),
}KEY_Val_TypeDef;
//------------------------------------------------------------------------
typedef	enum {					    // Key event types
    KEY_Evt_Press   = (0x80),		// Pressed
    KEY_Evt_Release = (0x40),		// Released
    KEY_Evt_Long2S  = (0x20),		// Long press 2s
    KEY_Evt_Long4S  = (0x10),		// Long press 4s
}KEY_Evt_TypeDef;
//------------------------------------------------------------------------


void KEY_Scan(void);
void KEY_DrvScan(void);
uint8_t KEY_Read(void);
void KEY_Write(uint8_t value);




#endif /* MACBSP_INC_BSP_KEY_H_ */
