/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-07-09     auto-gen     Power management task header
 */
#ifndef __POWER_TASK_H__
#define __POWER_TASK_H__

#include "bsp_sys.h"

/* Power state machine */
typedef enum {
    POWER_STATE_OFF = 0,        // Waiting for button press
    POWER_STATE_BOOTING,        // Button pressed, boot sequence in progress
    POWER_STATE_ON,             // System running
    POWER_STATE_SHUTTING_DOWN   // Shutdown requested
} power_state_t;

int power_task_init(void);              // Create power management thread
power_state_t power_get_state(void);    // Get current power state

#endif /* __POWER_TASK_H__ */
