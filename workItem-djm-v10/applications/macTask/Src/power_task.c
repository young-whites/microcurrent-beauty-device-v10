/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-07-09     auto-gen     Power management task implementation
 */
#include "power_task.h"
#include "bsp_hard.h"
#include "protocol.h"

#define DBG_TAG "pwr"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* ============================================================================
 *  Constants
 * ===========================================================================*/

#define POWER_THREAD_PRIORITY       15
#define POWER_THREAD_STACK_SIZE     512
#define POWER_THREAD_TICK           20      /* Polling period in ms */

#define BUTTON_DEBOUNCE_MS          1000    /* Button stable press duration */
#define BUTTON_SHUTDOWN_MS          3000    /* Long press for shutdown */
#define BEEP_DURATION_MS            1000    /* Boot beep duration */

/* ============================================================================
 *  Private Variables
 * ===========================================================================*/

static power_state_t s_power_state = POWER_STATE_OFF;
static rt_thread_t s_power_thread = RT_NULL;

/* ============================================================================
 *  Public API
 * ===========================================================================*/

/**
 * @brief  Get current power state.
 * @return Current power_state_t value.
 */
power_state_t power_get_state(void)
{
    return s_power_state;
}

/* ============================================================================
 *  Boot Sequence
 * ===========================================================================*/

/**
 * @brief  Execute boot sequence.
 *         1. Wait for stable button press (1s)
 *         2. Turn on system LED
 *         3. Beep for 1s then stop
 *         4. Enable 12V power supply
 *         5. Enable 54V boost converters
 *         6. Transition to POWER_STATE_ON
 */
static void power_boot_sequence(void)
{
    uint32_t press_start = 0;
    uint8_t button_confirmed = 0;

    rt_kprintf("[PWR] Waiting for power button press...\n");

    /* Wait for stable button press: LOW for 1 second */
    while (!button_confirmed) {
        if (bsp_power_button_read()) {
            /* Button is pressed (LOW) */
            if (press_start == 0) {
                press_start = rt_tick_get();
            } else if ((rt_tick_get() - press_start) >= rt_tick_from_millisecond(BUTTON_DEBOUNCE_MS)) {
                button_confirmed = 1;
            }
        } else {
            /* Button released, reset counter */
            press_start = 0;
        }
        rt_thread_mdelay(POWER_THREAD_TICK);
    }

    rt_kprintf("[PWR] Power button confirmed, starting boot...\n");

    /* Step 1: Turn on system LED (PA6) */
    bsp_led_set(1);

    /* Step 2: Beep for 1s (PA7) */
    bsp_beep_set(1);
    rt_thread_mdelay(BEEP_DURATION_MS);
    bsp_beep_set(0);

    /* Step 3: Enable 12V system power (PA4) */
    bsp_power_enable(1);
    rt_kprintf("[PWR] 12V power enabled\n");

    /* Step 4: Enable 54V boost converters (PB0 + PB1) */
    bsp_boost_1_enable(1);
    bsp_boost_2_enable(1);
    rt_kprintf("[PWR] 54V boost enabled\n");

    /* Transition to ON state */
    s_power_state = POWER_STATE_ON;
    rt_kprintf("[PWR] System ON\n");
}

/* ============================================================================
 *  Shutdown Sequence
 * ===========================================================================*/

/**
 * @brief  Execute shutdown sequence.
 *         1. Stop all treatment output
 *         2. Disable heating (large + small handle)
 *         3. Disable pump
 *         4. Disable 54V boost converters
 *         5. Turn off LED
 *         6. Disable 12V power
 *         7. Transition to POWER_STATE_OFF
 */
static void power_shutdown_sequence(void)
{
    rt_kprintf("[PWR] Shutdown sequence started\n");

    /* Stop all treatment output */
    protocol_stop_waveform();

    /* Disable heating */
    bsp_heater_large_set(0);
    bsp_heater_small_set(0);

    /* Disable pump */
    bsp_pump_set(0);

    /* Disable 54V boost converters */
    bsp_boost_1_enable(0);
    bsp_boost_2_enable(0);

    /* Turn off LED */
    bsp_led_set(0);

    /* Disable 12V power */
    bsp_power_enable(0);

    /* Transition to OFF state */
    s_power_state = POWER_STATE_OFF;
    rt_kprintf("[PWR] System OFF\n");
}

/* ============================================================================
 *  Power Management Thread
 * ===========================================================================*/

/**
 * @brief  Power management thread entry.
 *         - In OFF state: waits for button press to boot
 *         - In ON state: monitors button for long press to shutdown
 */
static void power_thread_entry(void *parameter)
{
    uint32_t press_start = 0;

    rt_kprintf("[PWR] Power management thread started\n");

    while (1) {
        switch (s_power_state) {

        case POWER_STATE_OFF:
            /* Execute boot sequence (blocks until button pressed) */
            s_power_state = POWER_STATE_BOOTING;
            power_boot_sequence();
            break;

        case POWER_STATE_BOOTING:
            /* Should not stay here long, boot_sequence transitions to ON */
            rt_thread_mdelay(POWER_THREAD_TICK);
            break;

        case POWER_STATE_ON:
            /* Monitor button for long press (3s) to trigger shutdown */
            if (bsp_power_button_read()) {
                /* Button is pressed */
                if (press_start == 0) {
                    press_start = rt_tick_get();
                } else if ((rt_tick_get() - press_start) >= rt_tick_from_millisecond(BUTTON_SHUTDOWN_MS)) {
                    /* Long press detected, initiate shutdown */
                    rt_kprintf("[PWR] Long press detected, shutting down...\n");
                    s_power_state = POWER_STATE_SHUTTING_DOWN;
                    press_start = 0;
                }
            } else {
                /* Button released, reset counter */
                press_start = 0;
            }
            rt_thread_mdelay(POWER_THREAD_TICK);
            break;

        case POWER_STATE_SHUTTING_DOWN:
            power_shutdown_sequence();
            break;

        default:
            s_power_state = POWER_STATE_OFF;
            break;
        }
    }
}

/* ============================================================================
 *  Module Initialization
 * ===========================================================================*/

/**
 * @brief  Create power management thread.
 * @return RT_EOK on success.
 */
int power_task_init(void)
{
    s_power_thread = rt_thread_create("power_task",
                                      power_thread_entry,
                                      RT_NULL,
                                      POWER_THREAD_STACK_SIZE,
                                      POWER_THREAD_PRIORITY,
                                      20);
    if (s_power_thread == RT_NULL) {
        rt_kprintf("[PWR] Failed to create power thread\n");
        return -RT_ERROR;
    }

    rt_thread_startup(s_power_thread);
    rt_kprintf("[PWR] Power task initialized\n");

    return RT_EOK;
}

/* Auto-initialize at APP_INIT level */
INIT_APP_EXPORT(power_task_init);
