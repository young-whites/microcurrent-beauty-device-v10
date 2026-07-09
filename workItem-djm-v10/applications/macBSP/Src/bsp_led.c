/*
 * bsp_led.c
 *
 *  Created on: May 16, 2024
 *      Author: zphu
 */
#include "bsp_led.h"




/*---------------------------------------------------------------------------------------------------------------*/
/* LED driver functions (portable)                                                                                                                                                          */
/*---------------------------------------------------------------------------------------------------------------*/
#define 	LED_MAX				(1)					// LED's Number
#define 	LED_Name_First		(1)					// First LED value in bsp_led.h must match this, others must increment sequentially (enum type)
#define		OUT_CYCLE			(100)				// LED output period
#define		GRAD_DEC			(5)					// LED fade-out speed (duty decrement interval in ms)
#define		GRAD_INC			(5)					// LED fade-in speed (duty increment interval in ms)
#define		GRAD_OFF_TIME		(50)				// Dwell time after fully off (unit: ms, must be multiple of GRAD_DEC)
#define		GRAD_ON_TIME		(5)					// Dwell time after fully on (unit: ms, must be multiple of GRAD_INC)


static	int8_t	_ledOutStt[LED_MAX]={0};			// LED output state
static	int8_t	_ledOutClk[LED_MAX]={0};			// LED output clock
static	int8_t	_ledOutDty[LED_MAX]={0};			// LED output duty (0~OUT_CYCLE, determines brightness)
//----------------------------------------------------------------------------
static	int8_t	_ledGrad[LED_MAX]={0};				// LED gradient (breathing) enable
static	int8_t	_ledGradDir[LED_MAX]={0};			// LED gradient direction
static	int8_t	_ledGradCnt[LED_MAX]={0};			// LED dwell counter after fully on/off
//----------------------------------------------------------------------------
static	int8_t	_ledBlnkClk[LED_MAX]={0};			// LED blink clock (unit: 0.1s)
static	int8_t	_ledBlnkCyc[LED_MAX]={0};			// LED blink cycle (total on+off time, unit: 0.1s)
static	int8_t	_ledBlnkDty[LED_MAX]={0};			// LED blink duty (on time per cycle, unit: 0.1s)
static	int8_t	_ledBlnkCnt[LED_MAX]={0};			// LED blink counter
static	int8_t	_ledBlnkCry[LED_MAX]={0};			// LED blink count (0 = no blink)
static	int8_t	_ledBlnkMut[LED_MAX]={0};			// LED pause cycle count (0 = no pause)
static	int8_t	_ledBlnkCct[LED_MAX]={0};			// LED repeat counter
static	int8_t	_ledBlnkRep[LED_MAX]={0};			// LED repeat blink count (0 = no repeat, 100+ = infinite)
//----------------------------------------------------------------------------
static	int16_t	msCnt=0;							// Millisecond counter
//----------------------------------------------------------------------------



/*****************************************************************************
* @brief  Turn LED permanently off.
*****************************************************************************/
static void _Off(int8_t ledName)
{
	_ledOutDty[ledName - LED_Name_First] = 0;			// Fully off
}




/*****************************************************************************
* @brief  Turn LED permanently on.
*****************************************************************************/
static void _On(int8_t ledName)
{
	_ledOutDty[ledName - LED_Name_First] = OUT_CYCLE;	// Fully on
}



/*****************************************************************************
* @brief  Turn LED permanently off (public API).
*****************************************************************************/
void LED_Off(int8_t ledName)
{
	int8_t	i = ledName - LED_Name_First;
	_ledGrad[i] = 0;									// Disable breathing
	_ledBlnkCry[i] = 0;									// Disable blinking
	_Off(ledName);
	LED_Out(ledName, 0);								// Turn off immediately
}




/*****************************************************************************
* @brief  Turn LED permanently on (public API).
*****************************************************************************/
void LED_On(int8_t ledName)
{
	int8_t	i = ledName - LED_Name_First;
	_ledGrad[i] = 0;									// Disable breathing
	_ledBlnkCry[i] = 0;									// Disable blinking
	_On(ledName);
	LED_Out(ledName, 1);								// Turn on immediately
}



/*****************************************************************************
* @brief  Toggle LED state.
*****************************************************************************/
void LED_Toggle(int8_t ledName)
{
	int8_t	i = ledName - LED_Name_First;
	if (_ledOutStt[i])	LED_Off(ledName);
	else				LED_On(ledName);
}



/*****************************************************************************
* @brief  LED gradient (breathing) effect.
*****************************************************************************/
void LED_Grad(int8_t ledName)
{
	int8_t	i = ledName - LED_Name_First;
	_ledOutDty[i] = 0;
	_ledGradDir[i] = 0;
	_ledGradCnt[i] = 0;
	_ledGrad[i] = 1;
}


/*****************************************************************************
* @brief  Set LED blink cycle and duty.
* @param  Cycle  Cycle time (total on+off time, unit: 0.1s)
* @param  Duty   Duty time (on time within cycle, unit: 0.1s)
*****************************************************************************/
void LED_BlinkSetCycleDuty(int8_t ledName, int8_t Cycle, int8_t Duty)
{
	int8_t	i = ledName-LED_Name_First;
	_ledBlnkCyc[i]=Cycle;
	_ledBlnkDty[i]=Duty;
	_ledBlnkClk[i]=0;
	msCnt=0;
}


/*****************************************************************************
* @brief  LED blink with specified count.
* @param  cry     Blink count (0 = no blink)
* @param  mute    Pause cycle count (0 = no pause)
* @param  repeat  Repeat blink count (0 = no repeat, 100+ = infinite)
*****************************************************************************/
void LED_Blink(int8_t ledName, int8_t cry, int8_t mute, int8_t repeat)
{
	int8_t	i = ledName-LED_Name_First;
	_ledBlnkCry[i]=cry;
	_ledBlnkMut[i]=mute;
	_ledBlnkRep[i]=repeat;
	_ledBlnkCnt[i]=0;
	_ledBlnkCct[i]=0;
}


/*****************************************************************************
* @brief  LED fancy display mode.
*****************************************************************************/
void LED_Fancy(int8_t mode)
{
	switch(mode){
	case 1:
		break;
	case 2:
		break;
	}
}


/*****************************************************************************
* @brief  LED scan driver.
* @note   Scan period: 1ms.
*****************************************************************************/
void LED_DrvScan(void)
{
	int8_t	i,num;

	if(++msCnt>=60000)	msCnt=0;

	num=LED_GetNumber();											// Get LED count
	if(num>LED_MAX)	num=LED_MAX;									// Clamp to max
	for(i=0;i<num;i++){												// Scan each LED
		if(_ledBlnkCry[i]){											// Blinking active
			if((msCnt%OUT_CYCLE)==0){								// Blink resolution 0.1s
				if(++_ledBlnkClk[i] >= _ledBlnkCyc[i]){ 			// Blink sub-cycle ended
					_ledBlnkClk[i]=0;
					if(++_ledBlnkCnt[i] >= (_ledBlnkCry[i]+_ledBlnkMut[i])){	// Blink mid-cycle ended
						_ledBlnkCnt[i]=0;
						if(++_ledBlnkCct[i] >= _ledBlnkRep[i]){		// Blink major-cycle ended
							_ledBlnkCct[i]=0;
							if(_ledBlnkRep[i] < 100)	_ledBlnkCry[i]=0;	// Repeat count reached, stop blinking
						}
					}
				}else if(_ledBlnkClk[i] >= _ledBlnkDty[i]){			// Second half of sub-cycle: off
					_Off(LED_Name_First+i);
				}else if(_ledBlnkCnt[i] < _ledBlnkCry[i]){			// First half (non-pause): on
					_On(LED_Name_First+i);
				}
			}
		}

		if(_ledGrad[i]){											// Breathing gradient control
			if(_ledGradDir[i]){										// Fading out
				if((msCnt%GRAD_DEC)==0){							// Fade-out speed control
					if(_ledOutDty[i]>0){
						_ledOutDty[i]-=1;
						_ledGradCnt[i]=0;
					}
					else {// Dwell time after fully off
						_ledGradCnt[i]+=1;
						if(_ledGradCnt[i]>=(GRAD_OFF_TIME/GRAD_DEC)) _ledGradDir[i]=0;
					}
				}
			}else{													// Fading in
				if((msCnt%GRAD_INC)==0){							// Fade-in speed control
					if(_ledOutDty[i]<OUT_CYCLE){
						_ledOutDty[i]+=1;
						_ledGradCnt[i]=0;
					}
					else {// Dwell time after fully on
						_ledGradCnt[i]+=1;
						if(_ledGradCnt[i]>=(GRAD_ON_TIME/GRAD_INC))	_ledGradDir[i]=1;
					}
				}
			}
		}

		_ledOutClk[i]+=1;
		if(_ledOutClk[i]>=OUT_CYCLE)	_ledOutClk[i]=0;			// Output timing
		if(_ledOutClk[i]>=_ledOutDty[i]){							// Output control (duty determines brightness)
			_ledOutStt[i]=0;										// LED off
		}else{
			_ledOutStt[i]=1;										// LED on
		}
		LED_Out(LED_Name_First+i,_ledOutStt[i]);					// Output
	}
}



/***************************************
 * @brief  LED initialization function
 * @param  None
 * @retval None
 ***************************************/
void LED_Init(void)
{
	int8_t	i,num;

	num=LED_GetNumber();											// Get LED count
	if(num>LED_MAX)	num=LED_MAX;									// Clamp to max
	for(i=0;i<num;i++){												// Scan each LED
		LED_BlinkSetCycleDuty(LED_Name_First+i,3, 2);
	}
}



/*****************************************************************************
* @brief  LED output.
*****************************************************************************/
void LED_Out(int8_t ledName, int8_t ledState)
{
	#if 1
	switch(ledName)
	{
		case LED_Name_Green:
			if(ledState)	macLED_GREEN_ON();
			else			macLED_GREEN_OFF();
			break;
	}
	#endif
}



/*****************************************************************************
* @brief  Get LED count.
*****************************************************************************/
int8_t LED_GetNumber(void)
{
	return LED_NUM;
}




/*---------------------------------------------------------------------------------------------------------------*/
/* LED scan thread creation and callback functions                                                                          */
/*---------------------------------------------------------------------------------------------------------------*/
/**
  * @brief  This thread entry is used for LED scan
  * @retval void
  */
void LED_Thread_entry(void* parameter)
{
    LED_Init();
    for(;;)
    {
        LED_DrvScan();
        rt_thread_mdelay(10);
    }
}



/**
  * @brief  LED thread initialization
  * @retval int
  */
rt_thread_t LED_Task_Handle = RT_NULL;
int LED_Thread_Init(void)
{
    LED_Task_Handle = rt_thread_create("LED_Thread_entry", LED_Thread_entry, RT_NULL, 4096, 9, 20);
    if(LED_Task_Handle != RT_NULL)
    {
        rt_kprintf("PRINTF:%d. LED_Thread_entry is Succeed!! \r\n",Record.kprintf_cnt++);
        rt_thread_startup(LED_Task_Handle);
    }
    else {
        rt_kprintf("PRINTF:%d. LED_Thread_entry is Failed \r\n",Record.kprintf_cnt++);
    }

    return RT_EOK;
}
INIT_APP_EXPORT(LED_Thread_Init);
