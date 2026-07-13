/*
 * bsp_key.c
 *
 *  Created on: May 16, 2024
 *      Author: zphu
 */
#include "bsp_key.h"
#include "power_task.h"

/*---------------------------------------------------------------------------------------------------------------*/
/* Key scan driver (portable)                                                                                                                                                      */
/*---------------------------------------------------------------------------------------------------------------*/

extern uint8_t  KEY_GetState(uint8_t keyName);
extern uint8_t  KEY_GetNumber(void);
//----------------------------------------------------------------------------
#define		KEY_MAX						(1)			// Maximum supported keys
//----------------------------------------------------------------------------
#define		KEY_VAL_BUF_SIZE			(KEY_MAX+3)	// Key value buffer size
#define		KEY_SCAN_FILTER_TIMES		(5)			// Debounce filter count (filter time = count * scan period; e.g. 10ms scan period => 50ms debounce)
//----------------------------------------------------------------------------
#define		KEY_Val_First				(0x01)		// First key value in port_key.c, other keys increment sequentially
//----------------------------------------------------------------------------
static	uint8_t	_keyValBuf[KEY_VAL_BUF_SIZE];		// Key value buffer
static	uint8_t	_keyValBufR = 0;					// Buffer read index (read==write means empty)
static	uint8_t	_keyValBufW = 0;					// Buffer write index (read==write means full)
static	uint8_t	_keyValBufCnt = 0;					// Buffer pending read count





/*****************************************************************************
* @brief  Write key value into buffer.
* @param  value: key value to write.
*****************************************************************************/
void KEY_Write(uint8_t value)
{
	if (_keyValBufCnt < KEY_VAL_BUF_SIZE) {			// Buffer not full (discard if full)
		_keyValBufCnt++;							// Increment count on each write
		_keyValBuf[_keyValBufW] = value;				// Write value to buffer
		if (++_keyValBufW >= KEY_VAL_BUF_SIZE) {		// Advance write position
			_keyValBufW = 0;							// Wrap around
		}
	}
}



/*****************************************************************************
* @brief  Read key value from buffer.
* @return Key value (0 if buffer empty).
*****************************************************************************/
uint8_t KEY_Read(void)
{
	uint8_t	value = 0;
	if (_keyValBufCnt) {							// Buffer has unread values
		_keyValBufCnt--;							// Decrement count on each read
		value = _keyValBuf[_keyValBufR];				// Read value from buffer
		if (++_keyValBufR >= KEY_VAL_BUF_SIZE) {		// Advance read position
			_keyValBufR = 0;							// Wrap around
		}
	}
	return value;
}






/*****************************************************************************
* @brief  Key scan driver - called periodically (10ms scan period).
*         Debounce: 50ms. Short press only (no long press detection).
*****************************************************************************/
void KEY_DrvScan(void)
{
	static	uint8_t	    step[KEY_MAX] = { 0 };						// Scan state
	static	uint8_t	    filterCnt[KEY_MAX] = { 0 };					// Filter timer
	static	uint8_t	    pressLast[KEY_MAX] = { 0 };					// Previous press state
	uint8_t	i, num, pressCurr;										// Current press state

	num = KEY_GetNumber();											// Get key count
	if (num > KEY_MAX)	num = KEY_MAX;								// Clamp to max
	for (i = 0; i < num; i++) {										// Scan each key
		pressCurr = KEY_GetState(KEY_Val_First + i);				// Get current press state
		switch (step[i]) {											// States: stable and filter
		case 0:														// Stable state (released)
			if (pressCurr != pressLast[i]) {						// State changed
				pressLast[i] = pressCurr;							// Update last state
				filterCnt[i] = 0;									// Start debounce timer
				step[i] = 1;										// Enter filter state
			}
			break;
		case 1:														// Filter state (debounce)
			if (pressCurr == pressLast[i]) {
				if (++filterCnt[i] >= KEY_SCAN_FILTER_TIMES) {		// N consecutive same states = confirmed
					if (pressCurr) {								// Confirmed pressed
						KEY_Write((KEY_Val_First + i) + KEY_Evt_Press);
					}
					else {											// Confirmed released
						KEY_Write((KEY_Val_First + i) + KEY_Evt_Release);
					}
					step[i] = 0;									// Return to stable state
				}
			}
			else {													// State changed during debounce = noise
				pressLast[i] = pressCurr;							// Update last state
				step[i] = 0;										// Return to stable state
			}
			break;
		default:
			step[i] = 0;											// Invalid state, reset
			break;
		}
	}
}





/*****************************************************************************
* @brief  Get key press state.
* @return 0: not pressed; non-zero: pressed.
*****************************************************************************/
uint8_t KEY_GetState(uint8_t keyName)
{
	uint8_t	stat = 0;
	switch (keyName)
	{
		case KeyA_PRESS:stat = (HAL_GPIO_ReadPin(POWER_ON_OFF_GPIO_Port, POWER_ON_OFF_Pin) ? 0 : 1);break;
	}
	return stat;
}





/*****************************************************************************
* @brief  Get number of keys.
*****************************************************************************/
uint8_t KEY_GetNumber(void)
{
	return KEY_NUM;
}



#include "bsp_typedef.h"

/*****************************************************************************
* @brief  Key event handler - processes key events from buffer.
*         KeyA (power button):
*           - Short press when OFF: set Flag.power_boot_request
*           - Short press when ON: set Flag.power_shutdown_request
*         Flag bits are polled by power_task thread.
*****************************************************************************/
void KEY_Scan(void)
{
	uint8_t key, event;
	for (key = KEY_Read(); key; key = KEY_Read())
	{
		event = key & 0xf0;
		key   = key & 0x0f;
		switch (key)
		{
			case KeyA_PRESS:
			{
				switch (event)
				{
					case KEY_Evt_Press:
						if (power_get_state() == POWER_STATE_OFF) {
							Flag.power_boot_request = 1;
							rt_kprintf("[KEY] Power button pressed, boot requested\n");
						} else {
							Flag.power_shutdown_request = 1;
							rt_kprintf("[KEY] Power button pressed, shutdown requested\n");
						}
						break;
				}
			} break;
		}
	}
}




/*---------------------------------------------------------------------------------------------------------------*/
/* Key scan timer thread                                                                                                                                          */
/*---------------------------------------------------------------------------------------------------------------*/
/**
  * @brief  keyTimer Callback Function
  * @retval void
  */
static void keyTimer_callback(void* parameter)
{
    KEY_DrvScan();
    KEY_Scan();
}



/**
  * @brief  keyTimer initialize
  * @retval int
  */
int keyTimer_Init(void)
{
    static rt_timer_t keyTimer;
    /* Create key software timer */
    keyTimer = rt_timer_create("keyTimer_callback", keyTimer_callback, RT_NULL, 10, RT_TIMER_FLAG_SOFT_TIMER | RT_TIMER_FLAG_PERIODIC);
    /* Start timer if created successfully */
    if(keyTimer != RT_NULL)
    {
        rt_kprintf("[KEY] Key timer initialized\n");
        rt_timer_start(keyTimer);
    }

    return RT_EOK;
}
INIT_APP_EXPORT(keyTimer_Init);
