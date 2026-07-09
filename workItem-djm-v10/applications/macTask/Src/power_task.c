/*
 * Power management task for DJM-V10
 * Handles boot/shutdown sequences with upper machine confirmation.
 *
 * Key events are handled by KEY_Scan() in bsp_key.c, which calls
 * power_request_boot() / power_request_shutdown_by_key() / power_force_shutdown().
 * This thread monitors s_power_state and executes the actual sequences.
 */
#include "power_task.h"
#include "bsp_hard.h"
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

#define FUNC_SHUTDOWN_REQ           0x0A

/* ============================================================================
 *  Private Variables
 * ===========================================================================*/

static power_state_t s_power_state = POWER_STATE_OFF;
static rt_thread_t s_power_thread = RT_NULL;

/* Flags set by KEY_Scan() or protocol handler */
static volatile uint8_t s_boot_requested = 0;
static volatile uint8_t s_shutdown_requested = 0;
static volatile uint8_t s_force_shutdown = 0;
static volatile uint8_t s_shutdown_confirmed = 0;

/* ============================================================================
 *  Public API
 * ===========================================================================*/

power_state_t power_get_state(void)
{
    return s_power_state;
}

/**
 * @brief  Called by KEY_Scan() on short press - request boot.
 */
void power_request_boot(void)
{
    s_boot_requested = 1;
}

/**
 * @brief  Called by KEY_Scan() on long press 2s - request graceful shutdown.
 */
void power_request_shutdown_by_key(void)
{
    s_shutdown_requested = 1;
}

/**
 * @brief  Called by KEY_Scan() on long press 4s - forced shutdown (skip host ACK).
 */
void power_force_shutdown(void)
{
    s_force_shutdown = 1;
}

/**
 * @brief  Called by protocol module when shutdown ACK received.
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
    rt_kprintf("[PWR] Boot sequence started\n");

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

    /* Beep 1s */
    BEEP_On();
    rt_thread_mdelay(BEEP_DURATION_MS);
    BEEP_Off();

    /* Turn off LED */
    LED_Off(LED_Name_Green);

    s_power_state = POWER_STATE_OFF;
    rt_kprintf("[PWR] System OFF\n");
}

/**
 * @brief  Graceful shutdown: send request to host, wait for ACK (5s timeout).
 */
static void power_shutdown_graceful(void)
{
    rt_kprintf("[PWR] Graceful shutdown initiated\n");

    s_shutdown_confirmed = 0;
    power_request_shutdown_to_host();

    /* Wait for host ACK */
    uint32_t start_tick = rt_tick_get();
    while (!s_shutdown_confirmed) {
        if ((rt_tick_get() - start_tick) >= rt_tick_from_millisecond(SHUTDOWN_CONFIRM_TIMEOUT_MS)) {
            rt_kprintf("[PWR] Shutdown confirm timeout, forced shutdown\n");
            break;
        }
        rt_thread_mdelay(POWER_THREAD_TICK);
    }

    power_do_shutdown();
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
            if (s_boot_requested) {
                s_boot_requested = 0;
                s_power_state = POWER_STATE_BOOTING;
                power_boot_sequence();
            }
            rt_thread_mdelay(POWER_THREAD_TICK);
            break;

        case POWER_STATE_BOOTING:
            rt_thread_mdelay(POWER_THREAD_TICK);
            break;

        case POWER_STATE_ON:
            /* Check for shutdown requests from KEY_Scan() */
            if (s_force_shutdown) {
                s_force_shutdown = 0;
                s_shutdown_requested = 0;
                s_power_state = POWER_STATE_SHUTTING_DOWN;
                rt_kprintf("[PWR] Forced shutdown by long press 4s\n");
                power_do_shutdown();
            } else if (s_shutdown_requested) {
                s_shutdown_requested = 0;
                s_power_state = POWER_STATE_SHUTTING_DOWN;
                power_shutdown_graceful();
            }
            rt_thread_mdelay(POWER_THREAD_TICK);
            break;

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
