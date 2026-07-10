/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-05-15     teati       the first version
 */
#ifndef APPLICATIONS_MACSYS_INC_BSP_TYPEDEF_H_
#define APPLICATIONS_MACSYS_INC_BSP_TYPEDEF_H_

#include "bsp_sys.h"


// 以下为移植时必须需要的结构体等的初始化---------------------------------------------------------------------------------------------------------
typedef struct {
    rt_uint8_t   Empty;                          // 空值
    rt_uint16_t  kprintf_cnt;                    // 用于打印序列
    rt_uint32_t  ulog_cnt;                       // ulog打印序列
    //------------------------------------------------------------

}RecordStruct;
extern RecordStruct Record;








// 以下为移植时必须需要的结构体等的初始化---------------------------------------------------------------------------------------------------------
typedef struct {
    /* Legacy flags */
    rt_uint8_t   air_rate_set;      // 潮气达标率设置标志(0：未处于设置状态   1：处于设置状态)

    /* Power management flags (set by KEY_Scan, read by power_task) */
    volatile rt_uint8_t   power_boot_request;       // 1 = boot requested by key press
    volatile rt_uint8_t   power_shutdown_request;   // 1 = graceful shutdown requested (long press 2s)
    volatile rt_uint8_t   power_force_shutdown;     // 1 = forced shutdown requested (long press 4s)
    volatile rt_uint8_t   power_shutdown_confirmed; // 1 = host ACK received for shutdown
}FlagStruct;
extern FlagStruct Flag;



typedef enum
{
    ON = 1,
    OFF = 0,
}SWITCH_et;



void system_param_init(void);


extern rt_event_t nrf24l01_events;



#endif /* APPLICATIONS_MACSYS_INC_BSP_TYPEDEF_H_ */
