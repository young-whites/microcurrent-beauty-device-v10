#include "bsp_beep.h"
/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2024-08-31     Administrator       the first version
 */

/*---------------------------------------------------------------------------------------------------------------*/
/* Buzzer driver functions (portable)                                                                                                                                                          */
/*---------------------------------------------------------------------------------------------------------------*/
static  int16_t beepClk=0;                          // Buzzer clock (unit: ms)
static  int16_t beepCyc=0;                          // Buzzer single beep cycle (total beep+interval, unit: ms)
static  int16_t beepDty=0;                          // Buzzer single beep duration (beep time within cycle, unit: ms)
static  int8_t  beepCnt=0;                          // Buzzer beep counter
static  int8_t  beepCry=0;                          // Buzzer beep count (0 = no beep)
static  int8_t  beepMut=0;                          // Buzzer pause cycle count (0 = no pause)
static  int8_t  beepCct=0;                          // Buzzer repeat counter
static  int8_t  beepRep=0;                          // Buzzer repeat beep count (0 = no repeat, 100+ = infinite)

/*****************************************************************************
* @brief  Turn buzzer off.
*****************************************************************************/
void BEEP_Off(void)
{
    beepCry=0;                                      // Disable (interval) beep
    macBEEP_OFF();
}

/*****************************************************************************
* @brief  Turn buzzer on (continuous).
*****************************************************************************/
void BEEP_On(void)
{
    beepCry=0;                                      // Disable (interval) beep
    macBEEP_ON();
}

/*****************************************************************************
* @brief  Set buzzer beep cycle and duty.
* @param  Cycle  Cycle time (total beep+interval, unit: ms)
* @param  Duty   Duty time (beep time within cycle, unit: ms)
*****************************************************************************/
void BEEP_SetCycleDuty(int16_t Cycle, int16_t Duty)
{
    if(Cycle <= 0) Cycle = 1;       /* Minimum cycle 1ms */
    if(Duty <= 0) Duty = 1;         /* Minimum duty 1ms */
    if(Duty > Cycle) Duty = Cycle;  /* Duty cannot exceed cycle */
    beepCyc=Cycle;
    beepDty=Duty;
    beepClk=0;
}




/*****************************************************************************
* @brief  Buzzer beep with specified count.
* @param  cry     Beep count (0 = no beep)
* @param  mute    Pause cycle count (0 = no pause)
* @param  repeat  Repeat beep count (0 = no repeat, 100+ = infinite)
*****************************************************************************/
void BEEP_Blink(int8_t cry, int8_t mute, int8_t repeat)
{
    if(cry <= 0) {                  /* No beep requested */
        macBEEP_OFF();
        return;
    }
    if(repeat < 0) repeat = 0;     /* Repeat 0 = one-shot */
    beepCry=cry;
    beepMut=mute;
    beepRep=repeat;
    beepCnt=0;
    beepCct=0;
    beepClk=0;
}


/*****************************************************************************
* @brief  Buzzer scan driver.
* @note   Scan period: 1ms.
*****************************************************************************/
void BEEP_DrvScan(void)
{
    if(beepCry){                                    // Beep active
        /* Turn OFF at duty boundary (must check before cycle end) */
        if(beepClk >= beepDty && beepClk < beepCyc){
            macBEEP_OFF();
        }

        if(++beepClk >= beepCyc){                   // Sub-cycle ended
            beepClk=0;
            if(++beepCnt >= (beepCry+beepMut)){     // Mid-cycle ended
                beepCnt=0;
                if(++beepCct >= beepRep){           // Major-cycle ended
                    beepCct=0;
                    if(beepRep < 100) {
                        rt_kprintf("[BEEP] Done (rep=%d)\n", beepRep);
                        beepCry=0;  // Repeat count reached, stop beeping
                        macBEEP_OFF();  // Ensure buzzer is off
                    }
                }
            }
        }

        /* Turn ON during active beep period (first half) */
        if(beepClk < beepDty && beepCnt < beepCry){
            macBEEP_ON();
        }
    }
}


/*****************************************************************************
* @brief  Buzzer initialization function.
* @note   None
*****************************************************************************/
void Beep_Init ( void )
{
    BEEP_SetCycleDuty(200,100);
}




/*---------------------------------------------------------------------------------------------------------------*/
/* Buzzer thread creation and callback functions                                                                            */
/*---------------------------------------------------------------------------------------------------------------*/


/**
  * @brief  Beep thread entry - scans every 1ms
  */
#define BEEP_THREAD_PRIORITY    5
#define BEEP_THREAD_STACK_SIZE  512

static void beep_thread_entry(void *parameter)
{
    Beep_Init();
    macBEEP_OFF();
    rt_kprintf("[BEEP] Beep thread started\n");

    while (1) {
        BEEP_DrvScan();
        rt_thread_mdelay(1);
    }
}

/**
  * @brief  Beep thread initialize
  * @retval int
  */
int beep_thread_Init(void)
{
    rt_thread_t tid = rt_thread_create("beep", beep_thread_entry, RT_NULL,
                                       BEEP_THREAD_STACK_SIZE,
                                       BEEP_THREAD_PRIORITY, 10);
    if (tid != RT_NULL) {
        rt_thread_startup(tid);
        rt_kprintf("[BEEP] Beep thread initialized\n");
    } else {
        rt_kprintf("[BEEP] Failed to create beep thread\n");
    }
    return RT_EOK;
}
INIT_APP_EXPORT(beep_thread_Init);



int Beep_Link_Test(void)
{

    BEEP_Blink(2, 0, 0);
    return RT_EOK;
}
MSH_CMD_EXPORT(Beep_Link_Test,Test2);
