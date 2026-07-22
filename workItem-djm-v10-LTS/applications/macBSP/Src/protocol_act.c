/*
 * Protocol action handlers for DJM-V10
 * Command dispatch, waveform control, treatment start/stop
 *
 * Hardware mapping:
 *   Handle A (0x0A) -> NNC6521_CHIP_1, CH0
 *   Handle B (0x0B) -> NNC6521_CHIP_1, CH1
 *   Handle C (0x0C) -> NNC6521_CHIP_2, CH0
 */

#include "protocol_act.h"
#include "protocol.h"
#include "bsp_hard.h"
#include "bsp_typedef.h"
#include "bsp_beep.h"
#include "nnc6521.h"
#include "nnc6521_waveform_config.h"
#include "nnc6521_waveform_config.h"
#include "temp_pid.h"
#include "dac7311.h"
#include "ntc_sensor.h"
#include <string.h>

/* Ramp abort flag: set by any new current/waveform command to interrupt ongoing ramp */
static volatile uint8_t s_ramp_abort = 0;

/* ============================================================================
 *  Hardware Helper: get NNC6521 chip ID from handle index
 * ===========================================================================*/

static uint8_t handle_to_chip(int handle_idx)
{
    /* Handle A(0), B(1) -> CHIP_1; Handle C(2) -> CHIP_2 */
    return (handle_idx <= 1) ? NNC6521_CHIP_1 : NNC6521_CHIP_2;
}

static uint8_t handle_to_channel(int handle_idx)
{
    /* Handle A(0) -> CH0, Handle B(1) -> CH1, Handle C(2) -> CH0 */
    return (handle_idx == 1) ? WAVEFORM_GEN_CH1 : WAVEFORM_GEN_CH0;
}

/* Handle A -> small heater, Handle B -> large heater, Handle C -> no heater */
static const int8_t s_handle_to_pid[3] = {
    TEMP_PID_SMALL,  /* Handle A (0) -> small handle heater */
    TEMP_PID_LARGE,  /* Handle B (1) -> large handle heater */
    -1               /* Handle C (2) -> no PID (pump only) */
};

/* NTC channel mapping: handle index -> NTC channel (-1 = no sensor) */
static const int8_t s_handle_to_ntc[3] = {
    NTC_CH_SMALL,    /* Handle A (0) -> small handle NTC */
    NTC_CH_LARGE,    /* Handle B (1) -> large handle NTC */
    -1               /* Handle C (2) -> no NTC */
};

/* ============================================================================
 *  Temperature Periodic Report Timer
 * ===========================================================================*/
#define TEMP_REPORT_INTERVAL_MS  2000   /* 2 seconds */

static rt_timer_t s_temp_report_timer = RT_NULL;
static uint8_t s_temp_report_handle = 0;   /* Handle ID for temperature report */

/**
 * @brief  Timer callback for periodic temperature reporting.
 *         Reads current temperature from the active handle's NTC sensor
 *         and sends a FUNC_TEMP_REPORT frame to the host.
 */
static void temp_report_timer_cb(void *parameter)
{
    int hi = protocol_handle_index(s_temp_report_handle);
    if (hi < 0 || hi > 1) return;   /* Only handles A/B have NTC */

    int8_t ntc_ch = s_handle_to_ntc[hi];
    if (ntc_ch < 0) return;

    float temp = ntc_sensor_get_temperature((uint8_t)ntc_ch);

    /* Pack temperature as 2 bytes: temp * 10 as uint16_t, big-endian */
    int16_t temp_x10 = (int16_t)(temp * 10.0f);
    uint8_t params[3];
    params[0] = s_temp_report_handle;               /* handle_id */
    params[1] = (uint8_t)((temp_x10 >> 8) & 0xFF); /* temp_high */
    params[2] = (uint8_t)(temp_x10 & 0xFF);         /* temp_low */

    protocol_send_ack(FUNC_TEMP_REPORT, params, 3);
}

void protocol_temp_report_start(uint8_t handle_id)
{
    /* Stop existing timer if running */
    protocol_temp_report_stop();

    s_temp_report_handle = handle_id;

    if (s_temp_report_timer == RT_NULL) {
        s_temp_report_timer = rt_timer_create("tmp_rpt",
                                              temp_report_timer_cb,
                                              RT_NULL,
                                              rt_tick_from_millisecond(TEMP_REPORT_INTERVAL_MS),
                                              RT_TIMER_FLAG_SOFT_TIMER | RT_TIMER_FLAG_PERIODIC);
        if (s_temp_report_timer == RT_NULL) {
            rt_kprintf("[PROTO] Failed to create temp report timer\n");
            return;
        }
    }

    rt_timer_start(s_temp_report_timer);
    rt_kprintf("[PROTO] Temp report started for handle 0x%02X, interval=%dms\n",
               handle_id, TEMP_REPORT_INTERVAL_MS);
}

void protocol_temp_report_stop(void)
{
    if (s_temp_report_timer != RT_NULL) {
        rt_timer_stop(s_temp_report_timer);
    }
    rt_kprintf("[PROTO] Temp report stopped\n");
}

/**
 * @brief  Stop waveform output on the chip associated with the given handle.
 *         Also disables the corresponding 54V boost converter.
 */
static void handle_stop_output(int handle_idx)
{
    uint8_t chip_id = handle_to_chip(handle_idx);
    uint8_t channel = handle_to_channel(handle_idx);
    nnc6521_awg_enable_disable(chip_id, channel, 0);

    /* Disable 54V boost for the handle's chip */
    if (handle_idx <= 1) {
        bsp_boost_1_enable(0);  /* Handle A/B -> CHIP_1 */
    } else {
        bsp_boost_2_enable(0);  /* Handle C -> CHIP_2 */
    }

    rt_kprintf("[PROTO] Waveform stopped, boost disabled on chip %d ch %d\n", chip_id, channel);
}

/**
 * @brief  Apply waveform output on the chip associated with the given handle.
 *         Uses current device state (waveform_id, current_ma in μA).
 */
static void handle_apply_output(int handle_idx)
{
    uint8_t chip_id = handle_to_chip(handle_idx);
    uint8_t wf_id   = g_dev_state.waveform_id;
    uint32_t actual_ua = g_dev_state.handle[handle_idx].current_ma;

    uint8_t channel = handle_to_channel(handle_idx);

    if (actual_ua == 0) {
        nnc6521_awg_enable_disable(chip_id, channel, 0);
        rt_kprintf("[PROTO] Current 0 uA, output disabled on chip %d ch %d\n", chip_id, channel);
        return;
    }

    waveform_apply_current(chip_id, channel, wf_id, actual_ua);
}

/* ============================================================================
 *  Command Handlers
 * ===========================================================================*/

/**
 * @brief  Handle 0x01: Switch active handle.
 *         para[0] = handle ID (0x0A/0x0B/0x0C)
 *
 *         Action: stop waveform, clear all params, switch handle.
 */
static void handle_switch(const uint8_t *params, uint8_t param_len)
{
    if (param_len < 1) {
        protocol_send_error(FUNC_HANDLE_SWITCH, ERR_PARAM);
        return;
    }

    uint8_t handle_id = params[0];
    int hi = protocol_handle_index(handle_id);
    if (hi < 0) {
        protocol_send_error(FUNC_HANDLE_SWITCH, ERR_PARAM);
        return;
    }

    /* Same handle - skip switch, just ACK */
    if (handle_id == g_dev_state.current_handle) {
        rt_kprintf("[PROTO] Already on handle %c, skip\n", 'A' + hi);
        uint8_t ack_params[1] = { handle_id };
        protocol_send_ack(FUNC_HANDLE_SWITCH, ack_params, 1);
        return;
    }

    /* Stop waveform on current handle's chip */
    int old_hi = protocol_handle_index(g_dev_state.current_handle);
    if (old_hi >= 0 && g_dev_state.is_running) {
        handle_stop_output(old_hi);
    }

    /* Stop temperature periodic report */
    protocol_temp_report_stop();

    /* Disable boost for the old handle */
    if (old_hi >= 0) {
        if (old_hi <= 1) {
            bsp_boost_1_enable(0);  /* Handle A/B -> CHIP_1 */
        } else {
            bsp_boost_2_enable(0);  /* Handle C -> CHIP_2 */
        }
    }

    /* Turn off all heaters via PID reset (mutual exclusion) and pump */
    temp_pid_set_target(TEMP_PID_LARGE, 0);
    temp_pid_set_target(TEMP_PID_SMALL, 0);
    dac7311_set_pump_speed(0);
    bsp_pump_set(0);

    /* Clear all handles' parameters */
    for (int i = 0; i < 3; i++) {
        const waveform_config_t *cfg = waveform_get_config(g_dev_state.waveform_id);
        g_dev_state.handle[i].current_ma = cfg ? cfg->min_current : 0;
        g_dev_state.handle[i].current_percent = 0;  /* min_current maps to 0% */
        g_dev_state.handle[i].temperature = 0;
        g_dev_state.handle[i].pump_speed = 0;
    }

    /* Switch handle */
    g_dev_state.current_handle = handle_id;
    g_dev_state.is_running = 0;

    rt_kprintf("[PROTO] Switch to handle %c (0x%02X), output stopped, params cleared\n",
               'A' + hi, handle_id);

    uint8_t ack_params[1] = { handle_id };
    protocol_send_ack(FUNC_HANDLE_SWITCH, ack_params, 1);
}

/**
 * @brief  Smoothly ramp current from start_ua to target_ua (in μA).
 *         Uses 200μA steps with 15ms inter-step delay.
 *         Can be interrupted by setting s_ramp_abort flag.
 *
 * @param  chip_id      NNC6521 chip ID
 * @param  channel      Waveform channel
 * @param  waveform_id  Waveform ID (1~9)
 * @param  start_ua     Starting current in μA
 * @param  target_ua    Target current in μA
 *
 * @note   If start_ua == target_ua, returns immediately.
 * @note   The waveform must already be configured via waveform_apply_current() before calling.
 */
static void current_ramp_to(uint8_t chip_id, uint8_t channel,
                            uint8_t waveform_id,
                            uint32_t start_ua, uint32_t target_ua)
{
    if (start_ua == target_ua) {
        return;
    }

    s_ramp_abort = 0;

    int32_t current = (int32_t)start_ua;
    int32_t target  = (int32_t)target_ua;
    int32_t step    = (target > current) ? 200 : -200;

    while (current != target) {
        if (s_ramp_abort) {
            rt_kprintf("[RAMP] Aborted at %d uA\n", current);
            return;
        }

        current += step;

        /* Clamp: don't overshoot target */
        if ((step > 0 && current > target) || (step < 0 && current < target)) {
            current = target;
        }

        waveform_update_amplitude_current(chip_id, channel, waveform_id, (uint32_t)current);

        if (current != target) {
            rt_thread_mdelay(15);
        }
    }

    /* Ensure final value is exactly the target */
    waveform_update_amplitude_current(chip_id, channel, waveform_id, target_ua);
    rt_kprintf("[RAMP] %d uA -> %d uA complete\n", start_ua, target_ua);
}

/**
 * @brief  Handle 0x02: Current control.
 *         para[0] = gear level (0~10, lookup table, unit μA)
 *         para[1] = target handle ID (0x0A/0x0B/0x0C)
 */
static void handle_current_ctrl(const uint8_t *params, uint8_t param_len)
{
    if (param_len < 2) {
        protocol_send_error(FUNC_CURRENT_CTRL, ERR_PARAM);
        return;
    }

    uint8_t level = params[0];       /* gear level 0~10 */
    uint8_t handle_id = params[1];

    if (level > 10) {
        protocol_send_error(FUNC_CURRENT_CTRL, ERR_PARAM);
        return;
    }

    int hi = protocol_handle_index(handle_id);
    if (hi < 0) {
        protocol_send_error(FUNC_CURRENT_CTRL, ERR_PARAM);
        return;
    }

    /* Lookup actual current from level table (μA) */
    uint8_t wf_id = g_dev_state.waveform_id;
    uint32_t actual_ua = g_current_level_map[wf_id - 1][level];

    /* Store old current before updating */
    uint32_t old_ua = g_dev_state.handle[hi].current_ma;

    /* Store new current */
    g_dev_state.handle[hi].current_ma = actual_ua;

    /* If active handle is running, update output with ramp */
    if (handle_id == g_dev_state.current_handle && g_dev_state.is_running) {
        uint8_t chip_id = handle_to_chip(hi);
        uint8_t channel = handle_to_channel(hi);

        /* Abort any ongoing ramp before starting a new one */
        s_ramp_abort = 1;
        rt_thread_mdelay(20);  /* Wait for ongoing ramp to notice abort */

        if (actual_ua == 0) {
            nnc6521_awg_enable_disable(chip_id, channel, 0);
            rt_kprintf("[PROTO] Current 0 uA, AWG disabled\n");
        } else {
            /* Ramp from old current to new current */
            s_ramp_abort = 0;
            current_ramp_to(chip_id, channel, wf_id, old_ua, actual_ua);
        }
    }

    rt_kprintf("[PROTO] Current set: handle %c = level %u -> %u uA\n", 'A' + hi, level, actual_ua);

    /* ACK 回复：返回 [档位, handle_id] */
    uint8_t ack_params[2] = { level, handle_id };
    protocol_send_ack(FUNC_CURRENT_CTRL, ack_params, 2);
}

/**
 * @brief  Handle 0x03: Temperature control.
 *         para[0] = target temperature (0~41C, 0=off)
 *         para[1] = target handle ID (0x0A/0x0B)
 */
static void handle_temp_ctrl(const uint8_t *params, uint8_t param_len)
{
    if (param_len < 2) {
        protocol_send_error(FUNC_TEMP_CTRL, ERR_PARAM);
        return;
    }

    uint8_t temperature = params[0];
    uint8_t handle_id = params[1];

    rt_kprintf("[PROTO] Temp cmd: temp=%u, handle=0x%02X, current=0x%02X\n",
               temperature, handle_id, g_dev_state.current_handle);

    if (temperature > 41) {
        protocol_send_error(FUNC_TEMP_CTRL, ERR_PARAM);
        return;
    }

    int hi = protocol_handle_index(handle_id);
    if (hi < 0 || hi > 1) {
        protocol_send_error(FUNC_TEMP_CTRL, ERR_PARAM);
        return;
    }

    /* Reject if requested handle is not the current active handle */
    if (handle_id != g_dev_state.current_handle) {
        rt_kprintf("[PROTO] Temp set rejected: handle %c is not active (current=%c)\n",
                   'A' + hi, 'A' + protocol_handle_index(g_dev_state.current_handle));
        protocol_send_error(FUNC_TEMP_CTRL, ERR_PARAM);
        return;
    }

    g_dev_state.handle[hi].temperature = temperature;

    /* Get PID index for this handle */
    int8_t pid_idx = s_handle_to_pid[hi];

    if (pid_idx < 0) {
        /* Handle C has no heating capability */
        rt_kprintf("[PROTO] Handle C has no heater, temp ignored\n");
        protocol_send_error(FUNC_TEMP_CTRL, ERR_PARAM);
        return;
    }

    /* Mutual exclusion: disable the other handle's PID first */
    uint8_t other_pid = (pid_idx == TEMP_PID_LARGE) ? TEMP_PID_SMALL : TEMP_PID_LARGE;
    temp_pid_set_target(other_pid, 0);

    /* Set PID target for this handle */
    temp_pid_set_target(pid_idx, (float)temperature);

    rt_kprintf("[PROTO] Temp set: handle %c = %u C, PID[%d] target=%.1f, enabled=%d\n",
               'A' + hi, temperature, pid_idx, (float)temperature,
               temp_pid_is_enabled(pid_idx));

    uint8_t ack_params[2] = { temperature, handle_id };
    protocol_send_ack(FUNC_TEMP_CTRL, ack_params, 2);
}

/**
 * @brief  Handle 0x04: Pump speed control.
 *         para[0] = speed percentage (0~100, 0=off)
 */
static void handle_pump_ctrl(const uint8_t *params, uint8_t param_len)
{
    if (param_len < 1) {
        protocol_send_error(FUNC_PUMP_CTRL, ERR_PARAM);
        return;
    }

    uint8_t speed = params[0];

    if (speed > 100) {
        protocol_send_error(FUNC_PUMP_CTRL, ERR_PARAM);
        return;
    }

    g_dev_state.handle[2].pump_speed = speed;

    /* Only set DAC voltage here. Pump enable (PB10) is controlled by start/pause */
    dac7311_set_pump_speed(speed);

    rt_kprintf("[PROTO] Pump speed set: %u%%, DAC output: %.2fV\n",
               speed, dac7311_get_voltage());

    uint8_t ack_params[1] = { speed };
    protocol_send_ack(FUNC_PUMP_CTRL, ack_params, 1);
}

/**
 * @brief  Handle 0x05: Start/Pause treatment.
 *         para[0] = 0=pause, 1=start
 */
static void handle_start_pause(const uint8_t *params, uint8_t param_len)
{
    if (param_len < 1) {
        protocol_send_error(FUNC_START_PAUSE, ERR_PARAM);
        return;
    }

    uint8_t action = params[0];

    if (action > 1) {
        protocol_send_error(FUNC_START_PAUSE, ERR_PARAM);
        return;
    }

    int hi = protocol_handle_index(g_dev_state.current_handle);
    if (hi < 0) {
        protocol_send_error(FUNC_START_PAUSE, ERR_BUSY);
        return;
    }

    if (action == 1) {
        /* Start: enable 54V boost first, wait for stabilization */
        if (hi <= 1) {
            bsp_boost_1_enable(1);  /* Handle A/B -> CHIP_1 */
        } else {
            bsp_boost_2_enable(1);  /* Handle C -> CHIP_2 */
        }
        rt_thread_mdelay(10);  /* Soft-start delay for boost stabilization */

        g_dev_state.is_running = 1;

        /* Ramp from 0 to target current on start */
        {
            uint8_t chip_id = handle_to_chip(hi);
            uint8_t channel = handle_to_channel(hi);
            uint8_t wf_id   = g_dev_state.waveform_id;
            uint32_t target_ua = g_dev_state.handle[hi].current_ma;
            waveform_apply_current(chip_id, channel, wf_id, 0);  /* Configure waveform at 0 uA */
            current_ramp_to(chip_id, channel, wf_id, 0, target_ua);
        }

        /* Enable pump (PB10) for handle C */
        if (hi == 2) {
            bsp_pump_set(1);
            rt_kprintf("[PROTO] Pump enabled (handle C)\n");
        }

        /* Enable PID temperature control if target is set */
        int8_t pid_idx = s_handle_to_pid[hi];
        if (pid_idx >= 0 && temp_pid_get_target(pid_idx) > 0) {
            temp_pid_set_enable(pid_idx, 1);
        }

        /* Start periodic temperature reporting for handles with NTC */
        if (hi <= 1) {
            protocol_temp_report_start(g_dev_state.current_handle);
        }

        rt_kprintf("[PROTO] Treatment started (boost enabled)\n");
    } else {
        /* Pause: stop waveform output, then disable boost */
        handle_stop_output(hi);
        g_dev_state.is_running = 0;

        /* Disable pump (PB10) for handle C */
        if (hi == 2) {
            bsp_pump_set(0);
            dac7311_set_pump_speed(0);
            g_dev_state.handle[2].pump_speed = 0;
            rt_kprintf("[PROTO] Pump disabled (handle C)\n");
        }

        /* Disable PID heating when paused */
        int8_t pid_idx = s_handle_to_pid[hi];
        if (pid_idx >= 0) {
            temp_pid_set_enable(pid_idx, 0);
        }

        /* Stop periodic temperature reporting */
        protocol_temp_report_stop();

        rt_kprintf("[PROTO] Treatment paused (boost disabled)\n");
    }

    uint8_t ack_params[1] = { action };
    protocol_send_ack(FUNC_START_PAUSE, ack_params, 1);
}

/**
 * @brief  Handle 0x06: OTA firmware upgrade (reserved).
 */
static void handle_ota_upgrade(const uint8_t *params, uint8_t param_len)
{
    rt_kprintf("[PROTO] OTA upgrade requested (not implemented)\n");
    protocol_send_error(FUNC_OTA_UPGRADE, ERR_UNSUPPORTED);
}

/**
 * @brief  Handle 0x07: Factory aging mode.
 *         para[0] = 0=exit aging, 1=enter aging
 */
static void handle_aging_mode(const uint8_t *params, uint8_t param_len)
{
    if (param_len < 1) {
        protocol_send_error(FUNC_AGING_MODE, ERR_PARAM);
        return;
    }

    uint8_t action = params[0];

    if (action > 1) {
        protocol_send_error(FUNC_AGING_MODE, ERR_PARAM);
        return;
    }

    /* Stop output when entering aging mode */
    if (action == 1 && g_dev_state.is_running) {
        int hi = protocol_handle_index(g_dev_state.current_handle);
        if (hi >= 0) handle_stop_output(hi);
    }

    g_dev_state.aging_mode = action;
    g_dev_state.is_running = 0;

    rt_kprintf("[PROTO] Aging mode %s\n", action ? "entered" : "exited");

    uint8_t ack_params[1] = { action };
    protocol_send_ack(FUNC_AGING_MODE, ack_params, 1);
}

/**
 * @brief  Handle 0x08: Read firmware version (no params).
 */
static void handle_read_version(void)
{
    uint8_t ack_params[2] = { DEVICE_VERSION_H, DEVICE_VERSION_L };
    protocol_send_ack(FUNC_READ_VERSION, ack_params, 2);
    rt_kprintf("[PROTO] Version -> %d.%d\n", DEVICE_VERSION_H, DEVICE_VERSION_L);
}

/**
 * @brief  Handle 0x09: Waveform selection.
 *         para[0] = waveform ID (1~9)
 */
static void handle_waveform_sel(const uint8_t *params, uint8_t param_len)
{
    if (param_len < 1) {
        protocol_send_error(FUNC_WAVEFORM_SEL, ERR_PARAM);
        return;
    }

    uint8_t waveform_id = params[0];

    if (waveform_id < 1 || waveform_id > 9) {
        protocol_send_error(FUNC_WAVEFORM_SEL, ERR_PARAM);
        return;
    }

    g_dev_state.waveform_id = waveform_id;

    /* All waveforms share 0~8mA range, percent mapping is identical.
     * No recalculation needed on waveform switch. */

    /* If treatment is running, switch waveform with ramp */
    int hi = protocol_handle_index(g_dev_state.current_handle);
    if (hi >= 0 && g_dev_state.is_running) {
        uint8_t chip_id = handle_to_chip(hi);
        uint8_t channel = handle_to_channel(hi);
        uint32_t target_ua = g_dev_state.handle[hi].current_ma;

        /* Abort any ongoing ramp */
        s_ramp_abort = 1;
        rt_thread_mdelay(20);

        /* Stop old waveform, configure new one at 0 uA, ramp to target */
        nnc6521_awg_enable_disable(chip_id, channel, 0);
        waveform_apply_current(chip_id, channel, waveform_id, 0);
        s_ramp_abort = 0;
        current_ramp_to(chip_id, channel, waveform_id, 0, target_ua);
    }

    rt_kprintf("[PROTO] Waveform selected: #%u\n", waveform_id);

    uint8_t ack_params[1] = { waveform_id };
    protocol_send_ack(FUNC_WAVEFORM_SEL, ack_params, 1);
}

/* ============================================================================
 *  Frame Dispatch (Main Command Router)
 * ===========================================================================*/

/**
 * @brief  Handle 0x0C: PID parameter auto-tuning.
 *         para[0..3] = target temperature as float (big-endian)
 *
 *         Request:  [target_temp_float]
 *         Response: [status] [kp_h] [kp_l] [ki_h] [ki_l] [kd_h] [kd_l]
 *         status: 0x00=started, 0x01=complete, 0x02=error
 */
static void handle_pid_autotune(const uint8_t *params, uint8_t param_len)
{
    if (param_len < 4) {
        protocol_send_error(FUNC_PID_AUTOTUNE, ERR_PARAM);
        return;
    }

    /* Parse target temperature as float (big-endian) */
    uint8_t fbuf[4] = { params[3], params[2], params[1], params[0] };
    float target_temp;
    rt_memcpy(&target_temp, fbuf, sizeof(float));

    /* Validate range */
    if (target_temp < 20.0f || target_temp > TEMP_MAX_CELSIUS) {
        uint8_t err_params[7] = { AUTOTUNE_STATUS_ERROR, 0,0,0,0,0,0 };
        protocol_send_ack(FUNC_PID_AUTOTUNE, err_params, 7);
        return;
    }

    /* Determine which PID index to tune based on current handle */
    int hi = protocol_handle_index(g_dev_state.current_handle);
    if (hi < 0 || hi > 1) {
        /* Handle C has no heater */
        uint8_t err_params[7] = { AUTOTUNE_STATUS_ERROR, 0,0,0,0,0,0 };
        protocol_send_ack(FUNC_PID_AUTOTUNE, err_params, 7);
        return;
    }

    int8_t pid_idx = s_handle_to_pid[hi];
    if (pid_idx < 0) {
        uint8_t err_params[7] = { AUTOTUNE_STATUS_ERROR, 0,0,0,0,0,0 };
        protocol_send_ack(FUNC_PID_AUTOTUNE, err_params, 7);
        return;
    }

    /* Check if already running autotune */
    if (temp_pid_autotune_is_running(pid_idx)) {
        uint8_t err_params[7] = { AUTOTUNE_STATUS_ERROR, 0,0,0,0,0,0 };
        protocol_send_ack(FUNC_PID_AUTOTUNE, err_params, 7);
        return;
    }

    /* Start autotune */
    temp_pid_autotune_start(pid_idx, target_temp);

    /* Send started notification */
    uint8_t ack_params[7] = { AUTOTUNE_STATUS_STARTED, 0,0,0,0,0,0 };
    protocol_send_ack(FUNC_PID_AUTOTUNE, ack_params, 7);

    rt_kprintf("[PROTO] PID autotune started for handle %c, target=%.1f C\n",
               'A' + hi, target_temp);
}

void protocol_dispatch(uint8_t *buf, uint8_t cmd_len)
{
    uint8_t type  = buf[3];
    uint8_t state = buf[4];
    uint8_t func  = buf[5];
    uint8_t param_len = cmd_len - 5;

    if (state != FRAME_STATE_ASK) {
        return;
    }

    if (type != FRAME_TYPE_ACT && type != FRAME_TYPE_GET) {
        rt_kprintf("[PROTO] Unknown frame type: 0x%02X\n", type);
        return;
    }

    rt_kprintf("[PROTO] Dispatch: type=0x%02X func=0x%02X param_len=%u\n",
               type, func, param_len);

    /* Beep once on each valid command received */
    BEEP_Blink(1, 0, 0);

    if (type == FRAME_TYPE_ACT || type == FRAME_TYPE_GET) {
        switch (func) {
            case FUNC_HANDLE_SWITCH: handle_switch(&buf[6], param_len);       break;
            case FUNC_CURRENT_CTRL:  handle_current_ctrl(&buf[6], param_len); break;
            case FUNC_TEMP_CTRL:     handle_temp_ctrl(&buf[6], param_len);    break;
            case FUNC_PUMP_CTRL:     handle_pump_ctrl(&buf[6], param_len);    break;
            case FUNC_START_PAUSE:   handle_start_pause(&buf[6], param_len);  break;
            case FUNC_OTA_UPGRADE:   handle_ota_upgrade(&buf[6], param_len);  break;
            case FUNC_AGING_MODE:    handle_aging_mode(&buf[6], param_len);   break;
            case FUNC_READ_VERSION:  handle_read_version();                   break;
            case FUNC_WAVEFORM_SEL:  handle_waveform_sel(&buf[6], param_len); break;
            case FUNC_PID_AUTOTUNE:  handle_pid_autotune(&buf[6], param_len); break;
            case FUNC_SHUTDOWN_REQ:
            {
                uint8_t para0 = (param_len >= 1) ? buf[6] : 0x00;
                if (para0 == 0x01) {
                    Flag.power_shutdown_confirmed = 1;
                    rt_kprintf("[PROTO] Shutdown confirmed by host\n");
                } else {
                    Flag.power_shutdown_denied = 1;
                    rt_kprintf("[PROTO] Shutdown denied by host\n");
                }
                /* Send ACK back to host */
                uint8_t ack_params[1] = { para0 };
                protocol_send_ack(FUNC_SHUTDOWN_REQ, ack_params, 1);
            } break;
            default:
                rt_kprintf("[PROTO] Unsupported function code: 0x%02X\n", func);
                protocol_send_error(func, ERR_UNSUPPORTED);
                break;
        }
    }
}

/* ============================================================================ *  Waveform Control (called by power_task for shutdown)
 * ===========================================================================*/

/**
 * @brief  Stop all waveform output on all NNC6521 chips.
 *         Called by power_task during shutdown sequence.
 */
void protocol_stop_waveform(void)
{
    /* Stop temperature periodic report */
    protocol_temp_report_stop();

    /* Disable AWG on all channels of both chips */
    nnc6521_awg_enable_disable(NNC6521_CHIP_1, WAVEFORM_GEN_CH0, 0);
    nnc6521_awg_enable_disable(NNC6521_CHIP_1, WAVEFORM_GEN_CH1, 0);
    nnc6521_awg_enable_disable(NNC6521_CHIP_2, WAVEFORM_GEN_CH0, 0);
    rt_kprintf("[PROTO] All waveform output stopped\n");
}

/**
 * @brief  Start waveform output on the current handle's chip/channel.
 *         Called by power_task or protocol module when needed.
 */
void protocol_start_waveform(void)
{
    int hi = protocol_handle_index(g_dev_state.current_handle);
    if (hi >= 0 && g_dev_state.is_running) {
        uint8_t chip_id = handle_to_chip(hi);
        uint8_t channel = handle_to_channel(hi);
        waveform_apply_current(chip_id, channel,
                               g_dev_state.waveform_id,
                               g_dev_state.handle[hi].current_ma);
        rt_kprintf("[PROTO] Waveform started on chip %d ch %d\n", chip_id, channel);
    }
}
