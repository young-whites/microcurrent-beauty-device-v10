#include <applications/macBSP/Inc/bsp_beep.h>
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
/* 以下是蜂鸣器驱动函数（可移植通用）                                                                                                                                                          */
/*---------------------------------------------------------------------------------------------------------------*/
static  int16_t beepClk=0;                          // 蜂鸣器鸣叫时钟（单位：ms）
static  int16_t beepCyc=0;                          // 蜂鸣器单次鸣叫的周期（一次完整鸣叫+间隔的总时间，单位：ms）
static  int16_t beepDty=0;                          // 蜂鸣器单次鸣叫的时间（周期中鸣叫所占的时间，单位：ms）
static  int8_t  beepCnt=0;                          // 蜂鸣器鸣叫计数
static  int8_t  beepCry=0;                          // 蜂鸣器鸣叫次数（0表示不鸣叫）
static  int8_t  beepMut=0;                          // 蜂鸣器停顿周期数（0表示不停顿）
static  int8_t  beepCct=0;                          // 蜂鸣器重复计数
static  int8_t  beepRep=0;                          // 蜂鸣器重复鸣叫次数（0表示不重复，100以上表示无限重复）

/*****************************************************************************
* 功能:       蜂鸣器关闭
*****************************************************************************/
void BEEP_Off(void)
{
    beepCry=0;                                      // 禁止（间隔）鸣叫
    macBEEP_OFF();
}

/*****************************************************************************
* 功能:       蜂鸣器长鸣
*****************************************************************************/
void BEEP_On(void)
{
    beepCry=0;                                      // 禁止（间隔）鸣叫
    macBEEP_ON();
}

/*****************************************************************************
* 功能:       设置蜂鸣器鸣叫周期及占空比
* 参数:       Cycle   周期（一次完整鸣叫+间隔的总时间，单位：ms）
*           Duty    占空比（周期中鸣叫所占的时间，单位：ms）
*****************************************************************************/
void BEEP_SetCycleDuty(int16_t Cycle, int16_t Duty)
{
    beepCyc=Cycle;
    beepDty=Duty;
    beepClk=0;
}




/*****************************************************************************
* 功能:       蜂鸣器鸣叫（指定次数）
* 参数:       cry     蜂鸣器鸣叫次数（0表示不鸣叫）
*           mute    蜂鸣器停顿周期数（0表示不停顿）
*           repeat  蜂鸣器重复鸣叫次数（0表示不重复，100以上表示无限重复）
*****************************************************************************/
void BEEP_Blink(int8_t cry, int8_t mute, int8_t repeat)
{
    beepCry=cry;
    beepMut=mute;
    beepRep=repeat;
    beepCnt=0;
    beepCct=0;
}


/*****************************************************************************
* 功能:       蜂鸣器扫描
* 说明:       扫描周期：1ms。
*****************************************************************************/
void BEEP_DrvScan(void)
{
    if(beepCry){                                    // 需要鸣叫
        if(++beepClk >= beepCyc){                   // 鸣叫小周期结束
            beepClk=0;
            if(++beepCnt >= (beepCry+beepMut)){     // 鸣叫中周期结束
                beepCnt=0;
                if(++beepCct >= beepRep){           // 鸣叫大周期结束
                    beepCct=0;
                    if(beepRep < 100)   beepCry=0;  // 重复次数到达，结束鸣叫
                }
            }
        }else if(beepClk >= beepDty){               // 后半小周 不叫
            macBEEP_OFF();
        }else if(beepCnt < beepCry){                // （仅非停顿期间的）前半小周 鸣叫
            macBEEP_ON();
        }
    }
}


/*****************************************************************************
* 功能:       蜂鸣器初始化函数
* 说明:       None
*****************************************************************************/
void Beep_Init ( void )
{
    BEEP_SetCycleDuty(200,100);
}




/*---------------------------------------------------------------------------------------------------------------*/
/* 以下是蜂鸣器线程的创建以及回调函数                                                                            */
/*---------------------------------------------------------------------------------------------------------------*/


/**
  * @brief  beepTimer Callback Function
  * @retval void
  */
static void beepTimer_callback(void* parameter)
{
    BEEP_DrvScan();
}



/**
  * @brief  beepTimer initialize
  * @retval int
  */
int beepTimer_Init(void)
{
    static rt_timer_t   beepTimer = RT_NULL;
    /* 创建beep的软件定时器线程 */
    beepTimer = rt_timer_create("beepTimer_callback", beepTimer_callback, RT_NULL, 1, RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);

    /* 启动beep软件定时器 */
    if(beepTimer != RT_NULL )
    {
        rt_kprintf("PRINTF:%d. Beep initialize succeed!\r\n",Record.kprintf_cnt++);
        Beep_Init();
        macBEEP_OFF();
        rt_timer_start(beepTimer);
    }

    return RT_EOK;
}
INIT_APP_EXPORT(beepTimer_Init);



int Beep_Link_Test(void)
{

    BEEP_Blink(2, 0, 0);
    return RT_EOK;
}
MSH_CMD_EXPORT(Beep_Link_Test,Test2);










