/*
 * Power management task for DJM-V10
 * Handles boot/shutdown sequences with upper machine confirmation
 */
#include "power_task.h"
#include "bsp_hard.h"
#include "bsp_key.h"
#include "bsp_led.h"
#include "bsp_beep.h"
#include "protocol.h"

#define DBG_TAG "pwr"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* ============================================================================
 *  Constants
 * ===========================================================================*/

#define POWER_THREAD_PRIORITY       15
#define POWER_THREAD_STACK_SIZE     512
#define POWER_THREAD_TICK           20

#define BEEP_DURATION_MS            1000
#define SHUTDOWN_CONFIRM_TIMEOUT_MS 5000

/* Shutdown request function code */
#define FUNC_SHUTDOWN_REQ           0x0A

/* ============================================================================
 *  Private Variables
 * ===========================================================================*/

static power_state_t s_power_state = POWER_STATE_OFF;
static rt_thread_t s_power_thread = RT_NULL;

/* Shutdown confirmation flag (set by protocol handler when ACK received) */
static volatile uint8_t s_shutdown_confirmed = 0;

/* ============================================================================
 *  Public API
 * ===========================================================================*/

power_state_t power_get_state(void)
{
    return s_power_state;
}

/**
 * @brief  Called by protocol module when shutdown confirmation received.
 */
void power_shutdown_confirm(void)
{
    s_shutdown_confirmed = 1;
}

/* ============================================================================
 *  Boot Sequence
 * ===========================================================================*/

static void power_boot_sequence(void)
{
    rt_kprintf("[PWR] Waiting for power button...\n");

    /* Wait for KEY_Evt_Press from key driver (blocking on key buffer) */
    uint8_t key = 0;
    while (1) {
        key = KEY_Read();
        if (key != 0) {
            uint8_t event = key & 0xf0;
            uint8_t kval  = key & 0x0f;
            if (kval == KeyA_PRESS && event == KEY_Evt_Press) {
                break;
            }
        }
        rt_thread_mdelay(POWER_THREAD_TICK);
    }

    rt_kprintf("[PWR] Power button pressed, booting...\n");

    /* Step 1: Turn on system LED (PA6) */
    LED_On(LED_Name_Green);

    /* Step 2: Beep for 1s */
    BEEP_On();
    rt_thread_mdelay(BEEP_DURATION_MS);
    BEEP_Off();

    /* Step 3: Enable 12V system power (PA4) */
    bsp_power_enable(1);
    rt_kprintf("[PWR] 12V power enabled\n");

    /* Note: 54V boost is NOT enabled at boot - only when treatment output is needed */

    s_power_state = POWER_STATE_ON;
    rt_kprintf("[PWR] System ON\n");
}

/* ============================================================================
 *  Shutdown Sequence
 * ===========================================================================*/

/**
 * @brief  Send shutdown request to upper machine and wait for confirmation.
 *         Frame: 55 AA 06 60 66 44 01 0A 00 [crcH] [crcL]
 */
static void power_request_shutdown(void)
{
    /* Send shutdown request frame via protocol */
    uint8_t params[1] = { 0x00 };
    protocol_send_ack(FUNC_SHUTDOWN_REQ, params, 1);
    rt_kprintf("[PWR] Shutdown request sent to upper machine\n");
}

static void power_shutdown_sequence(void)
{
    /* Step 1: Send shutdown request to upper machine */
    s_shutdown_confirmed = 0;
    power_request_shutdown();

    /* Step 2: Wait for confirmation (timeout = forced shutdown) */
    uint32_t start_tick = rt_tick_get();
    while (!s_shutdown_confirmed) {
        if ((rt_tick_get() - start_tick) >= rt_tick_from_millisecond(SHUTDOWN_CONFIRM_TIMEOUT_MS)) {
            rt_kprintf("[PWR] Shutdown confirm timeout, forced shutdown\n");
            break;
        }
        rt_thread_mdelay(POWER_THREAD_TICK);
    }

    /* Step 3: Stop all treatment output */
    protocol_stop_waveform();

    /* Step 4: Disable heating */
    bsp_heater_large_set(0);
    bsp_heater_small_set(0);

    /* Step 5: Disable pump */
    bsp_pump_set(0);

    /* Step 6: Disable 54V boost */
    bsp_boost_1_enable(0);
    bsp_boost_2_enable(0);

    /* Step 7: Disable 12V power */
    bsp_power_enable(0);

    /* Step 8: Beep 1s */
    BEEP_On();
    rt_thread_mdelay(BEEP_DURATION_MS);
    BEEP_Off();

    /* Step 9: Turn off LED */
    LED_Off(LED_Name_Green);

    s_power_state = POWER_STATE_OFF;
    rt_kprintf("[PWR] System OFF\n");
}

/* ============================================================================
 *  Power Management Thread
 * ===========================================================================*/

static void power_thread_entry(void *parameter)
{
    uint32_t press_start = 0;

    rt_kprintf("[PWR] Power management thread started\n");

    while (1) {
        switch (s_power_state) {

        case POWER_STATE_OFF:
            s_power_state = POWER_STATE_BOOTING;
            power_boot_sequence();
            break;

        case POWER_STATE_BOOTING:
            rt_thread_mdelay(POWER_THREAD_TICK);
            break;

        case POWER_STATE_ON:
            /* Monitor for long press (3s) via key driver */
            {
                uint8_t key = KEY_Read();
                if (key != 0) {
                    uint8_t event = key & 0xf0;
                    uint8_t kval  = key & 0x0f;
                    if (kval == KeyA_PRESS && event == KEY_Evt_Long2S) {
                        rt_kprintf("[PWR] Long press detected, shutting down...\n");
                        s_power_state = POWER_STATE_SHUTTING_DOWN;
                    }
                }
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
