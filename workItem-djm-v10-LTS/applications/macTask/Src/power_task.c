/*
 * Power management task for DJM-V10
 * Handles boot/shutdown sequences with upper machine confirmation.
 *
 * Key events are handled by KEY_Scan() in bsp_key.c, which sets
 * Flag.power_boot_request / Flag.power_shutdown_request.
 * This thread monitors s_power_state and executes the actual sequences.
 */
#include "power_task.h"
#include "bsp_hard.h"
#include "bsp_led.h"
#include "bsp_beep.h"
#include "bsp_typedef.h"
#include "protocol.h"
#include "protocol_act.h"

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
#define SHUTDOWN_CONFIRM_TIMEOUT_MS 10000

#define FUNC_SHUTDOWN_REQ           0x0A

/* ============================================================================
 *  Private Variables
 * ===========================================================================*/

static power_state_t s_power_state = POWER_STATE_OFF;
static rt_thread_t s_power_thread = RT_NULL;

/* ============================================================================
 *  Public API
 * ===========================================================================*/

power_state_t power_get_state(void)
{
    return s_power_state;
}

/**
 * @brief  Called by protocol module when shutdown ACK received.
 *         Sets Flag.power_shutdown_confirmed.
 */
void power_shutdown_confirm(void)
{
    Flag.power_shutdown_confirmed = 1;
}

/* ============================================================================
 *  Boot Sequence
 * ===========================================================================*/

static void power_boot_sequence(void)
{
    rt_kprintf("[PWR] Boot sequence started\n");

    /* Step 1: Turn on system LED (PA6) */
    LED_On(LED_Name_Green);

    /* Step 2: Beep 1s (non-blocking, driven by beep timer) */
    BEEP_SetCycleDuty(BEEP_DURATION_MS, BEEP_DURATION_MS);
    BEEP_Blink(1, 0, 0);

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

static void power_request_shutdown_to_host(void)
{
    uint8_t params[1] = { 0x00 };
    protocol_send_ack(FUNC_SHUTDOWN_REQ, params, 1);
    rt_kprintf("[PWR] Shutdown request sent to upper machine\n");
}

static void power_do_shutdown(void)
{
    /* Stop all treatment output */
    protocol_stop_waveform();

    /* Disable heating */
    bsp_heater_large_set(0);
    bsp_heater_small_set(0);

    /* Disable pump */
    bsp_pump_set(0);

    /* Disable 54V boost */
    bsp_boost_1_enable(0);
    bsp_boost_2_enable(0);

    /* Disable 12V power */
    bsp_power_enable(0);

    /* Beep 1s (non-blocking, wait for completion) */
    BEEP_SetCycleDuty(BEEP_DURATION_MS, BEEP_DURATION_MS);
    BEEP_Blink(1, 0, 0);
    rt_thread_mdelay(BEEP_DURATION_MS);

    /* Turn off LED */
    LED_Off(LED_Name_Green);

    s_power_state = POWER_STATE_OFF;
    rt_kprintf("[PWR] System OFF\n");
}

/* ============================================================================
 *  Power Management Thread
 * ===========================================================================*/

static void power_thread_entry(void *parameter)
{
    rt_kprintf("[PWR] Power management thread started\n");

    while (1) {
        switch (s_power_state) {

        case POWER_STATE_OFF:
            /* Wait for boot request from KEY_Scan() */
            if (Flag.power_boot_request) {
                Flag.power_boot_request = 0;
                s_power_state = POWER_STATE_BOOTING;
                power_boot_sequence();
            }
            rt_thread_mdelay(POWER_THREAD_TICK);
            break;

        case POWER_STATE_BOOTING:
            rt_thread_mdelay(POWER_THREAD_TICK);
            break;

        case POWER_STATE_ON:
            /* Check for shutdown request from KEY_Scan() */
            if (Flag.power_shutdown_request) {
                Flag.power_shutdown_request = 0;
                Flag.power_shutdown_confirmed = 0;
                Flag.power_shutdown_denied = 0;
                power_request_shutdown_to_host();
                s_power_state = POWER_STATE_WAIT_CONFIRM;
                rt_kprintf("[PWR] Shutdown request sent, waiting for host confirmation\n");
            }
            rt_thread_mdelay(POWER_THREAD_TICK);
            break;

        case POWER_STATE_WAIT_CONFIRM:
        {
            static uint32_t wait_start_tick = 0;

            /* Record tick on first entry */
            if (wait_start_tick == 0) {
                wait_start_tick = rt_tick_get();
            }

            /* Check for host confirmation */
            if (Flag.power_shutdown_confirmed) {
                Flag.power_shutdown_confirmed = 0;
                wait_start_tick = 0;
                s_power_state = POWER_STATE_SHUTTING_DOWN;
                rt_kprintf("[PWR] Host confirmed shutdown\n");
                power_do_shutdown();
            }
            /* Check for host denial */
            else if (Flag.power_shutdown_denied) {
                Flag.power_shutdown_denied = 0;
                wait_start_tick = 0;
                s_power_state = POWER_STATE_ON;
                rt_kprintf("[PWR] Host denied shutdown, back to ON\n");
            }
            /* Check for timeout (10s no response -> back to ON) */
            else if ((rt_tick_get() - wait_start_tick) >= rt_tick_from_millisecond(SHUTDOWN_CONFIRM_TIMEOUT_MS)) {
                wait_start_tick = 0;
                s_power_state = POWER_STATE_ON;
                rt_kprintf("[PWR] Shutdown confirm timeout, back to ON\n");
            }
            rt_thread_mdelay(POWER_THREAD_TICK);
            break;
        }

        case POWER_STATE_SHUTTING_DOWN:
            rt_thread_mdelay(POWER_THREAD_TICK);
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
