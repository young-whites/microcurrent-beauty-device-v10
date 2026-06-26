/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author      Notes
 * 2024-11-19     teati       the first version
 */
#include <rtt_system_work.h>





static void Timing_1ms(void)
{

}




static void Timing_10ms(void)
{


}


static void Timing_50ms(void)
{

}



static void Timing_500ms(void)
{

}




static void Timing_1s(void)
{


}




/*---------------------------------------------------------------------------------------------------------------*/
/* 以下是系统扫描线程的创建以及回调函数                                                                          */
/*---------------------------------------------------------------------------------------------------------------*/
/**
  * @brief  sysTimer Callback Function -- 10ms entry
  * @retval void
  */
static rt_uint32_t sysTimeTick = 0;
static void sysTimer_callback(void* parameter)
{
    sysTimeTick++;

    if(sysTimeTick > 60000){
        sysTimeTick = 0;
    }

    if((sysTimeTick % 1)    == 0)   Timing_1ms();
    if((sysTimeTick % 10)   == 0)   Timing_10ms();
    if((sysTimeTick % 50)   == 0)   Timing_50ms();
    if((sysTimeTick % 500)  == 0)   Timing_500ms();
    if((sysTimeTick % 1000) == 0)   Timing_1s();
}



/**
  * @brief  keyTimer initialize
  * @retval int
  */
rt_timer_t sysTimer;
int sysTimer_Init(void)
{
    /* 创建key软件定时器线程 */
    sysTimer = rt_timer_create("sysTimer_callback", sysTimer_callback, RT_NULL, 1, RT_TIMER_FLAG_SOFT_TIMER | RT_TIMER_FLAG_PERIODIC);
    /* 如果keyTimer句柄创建成功,开启软件定时器 */
    if(sysTimer != RT_NULL)
    {
        rt_kprintf("PRINTF:%d. sysTimer initialize succeed!\r\n",Record.kprintf_cnt++);
        rt_timer_start(sysTimer);
    }

    return RT_EOK;
}
INIT_APP_EXPORT(sysTimer_Init);







