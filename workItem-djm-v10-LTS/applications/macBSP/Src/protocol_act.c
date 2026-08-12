/*
 * Protocol action handlers for DJM-V10
 * Command dispatch, waveform control, treatment start/stop
 *
 * V4.1: Current output is global (not per-handle).
 *       Handle switching does NOT stop current/waveform.
 *       Only treatment start/stop (0x05) and level=0 control current output.
 *       54V boost (PB1) is global, controlled by treatment start/stop.
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

/* ============================================================================
 *  Hardware Helper: get NNC6521 chip ID from handle index
 * ===========================================================================*/

static uint8_t handle_to_chip(int handle_idx)
{
    /* V4.2: CHIP_1 disabled - all handles use CHIP_2
     * Handle A(0) -> CHIP_2 CH0
     * Handle B(1) -> CHIP_2 CH1
     * Handle C(2) -> CHIP_2 CH0 */
    (void)handle_idx;
    return NNC6521_CHIP_2;
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

static void handle_stop_output(int handle_idx)
{
    uint8_t chip_id = handle_to_chip(handle_idx);

    /* Full chip shutdown: zero everything on both channels */
    nnc6521_write_reg(chip_id, WAVEGEN_GLOBAL_REG_0, 0x00);
    nnc6521_awg_enable_disable(chip_id, WAVEFORM_GEN_CH0, 0);
    nnc6521_awg_enable_disable(chip_id, WAVEFORM_GEN_CH1, 0);
    nnc6521_analog_disable(chip_id, WAVEFORM_GEN_CH0);
    nnc6521_analog_disable(chip_id, WAVEFORM_GEN_CH1);

    /* V4.1: Boost is global (PB1), controlled by treatment start/stop only.
     * Do NOT disable boost here - it's handled by handle_start_pause(). */

    rt_kprintf("[STOP] chip=%d ANA_EN_1=0x%02X ANA_EN_2=0x%02X\n",
               chip_id,
               nnc6521_read_reg(chip_id, 0x41),
               nnc6521_read_reg(chip_id, 0x42));
}

static void handle_apply_output(int handle_idx)
{
    uint8_t chip_id = handle_to_chip(handle_idx);
    uint8_t wf_id   = g_dev_state.waveform_id;
    uint8_t level   = g_dev_state.current_level;
    uint8_t channel = handle_to_channel(handle_idx);

    /* Lookup actual current from global level table */
    const uint32_t *level_map = (handle_idx == 0) ? g_current_level_map_a[wf_id - 1]
                                                   : g_current_level_map_bc[wf_id - 1];
    uint32_t actual_ua = level_map[level];

    if (actual_ua == 0) {
        nnc6521_awg_enable_disable(chip_id, channel, 0);
        nnc6521_analog_disable(chip_id, channel);
        return;
    }

    /* Step 1: Shut down EVERYTHING on this chip */
    nnc6521_write_reg(chip_id, WAVEGEN_GLOBAL_REG_0, 0x00);
    nnc6521_awg_enable_disable(chip_id, WAVEFORM_GEN_CH0, 0);
    nnc6521_awg_enable_disable(chip_id, WAVEFORM_GEN_CH1, 0);
    nnc6521_analog_disable(chip_id, WAVEFORM_GEN_CH0);
    nnc6521_analog_disable(chip_id, WAVEFORM_GEN_CH1);

    rt_kprintf("[APPLY] chip=%d ch=%d wf=%d lv=%d -> %u uA (map[%d][%d])\n",
               chip_id, channel, wf_id, level, actual_ua, wf_id - 1, level);

    /* Step 2: Enable ONLY the target channel */
    nnc6521_analog_enable(chip_id, channel);
    waveform_apply_current(chip_id, channel, wf_id, actual_ua);

    rt_kprintf("[APPLY] After: ANA_EN_1=0x%02X ANA_EN_2=0x%02X\n",
               nnc6521_read_reg(chip_id, 0x41),
               nnc6521_read_reg(chip_id, 0x42));
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

    /* V4.1: Do NOT stop current/waveform output during handle switch.
     * Current and waveform are global functions, only controlled by
     * treatment start/stop (0x05) and level=0. */

    /* Stop temperature periodic report */
    protocol_temp_report_stop();

    /* Turn off all heaters via PID reset (mutual exclusion) and pump */
    temp_pid_set_target(TEMP_PID_LARGE, 0);
    temp_pid_set_target(TEMP_PID_SMALL, 0);
    dac7311_set_pump_speed(0);
    bsp_pump_set(0);

    /* V4.1: Only clear heating/pump params, KEEP current and waveform params */
    for (int i = 0; i < 3; i++) {
        g_dev_state.handle[i].temperature = 0;
        g_dev_state.handle[i].pump_speed = 0;
    }
    /* current_level and waveform_id are GLOBAL - do NOT clear them */

    /* Switch handle */
    g_dev_state.current_handle = handle_id;
    g_dev_state.is_running = 0;

    rt_kprintf("[PROTO] Switch to handle %c (0x%02X), heating/pump cleared, current/waveform preserved\n",
               'A' + hi, handle_id);

    uint8_t ack_params[1] = { handle_id };
    protocol_send_ack(FUNC_HANDLE_SWITCH, ack_params, 1);
}

static void handle_current_ctrl(const uint8_t *params, uint8_t param_len)
{
    if (param_len < 1) {
        protocol_send_error(FUNC_CURRENT_CTRL, ERR_PARAM);
        return;
    }

    uint8_t level = params[0];  /* V4.1: gear level 0~10, global */

    if (level > 10) {
        protocol_send_error(FUNC_CURRENT_CTRL, ERR_PARAM);
        return;
    }

    /* V4.1: Save to global current_level */
    g_dev_state.current_level = level;
    rt_kprintf("[PROTO] current_level saved = %u\n", level);

    /* If treatment is running, apply new current directly */
    if (g_dev_state.is_running) {
        int hi = protocol_handle_index(g_dev_state.current_handle);
        if (hi >= 0) {
            handle_apply_output(hi);
        }
    }

    rt_kprintf("[PROTO] Current set: level=%u (global)\n", level);

    /* V4.1: ACK returns [档位] only, no handle_id */
    uint8_t ack_params[1] = { level };
    protocol_send_ack(FUNC_CURRENT_CTRL, ack_params, 1);
}

/**
 * @brief  Handle 0x03: Temperature control.
 *         para[0] = temperature percent (0~100, 0=off, 1~100 maps to 20~41C)
 *         para[1] = target handle ID (0x0A/0x0B)
 */
static void handle_temp_ctrl(const uint8_t *params, uint8_t param_len)
{
    if (param_len < 2) {
        protocol_send_error(FUNC_TEMP_CTRL, ERR_PARAM);
        return;
    }

    uint8_t percent = params[0];
    uint8_t handle_id = params[1];

    rt_kprintf("[PROTO] Temp cmd: percent=%u, handle=0x%02X, current=0x%02X\n",
               percent, handle_id, g_dev_state.current_handle);

    if (percent > 100) {
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

    g_dev_state.handle[hi].temperature = percent;

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

    /* Map percent to temperature */
    float target_temp;
    if (percent == 0) {
        target_temp = 0.0f;  /* Disable heating */
    } else {
        target_temp = 20.0f + (float)(percent - 1) * (43.0f - 20.0f) / 99.0f;
    }

    /* Set PID target for this handle */
    temp_pid_set_target(pid_idx, target_temp);

    /* If treatment is already running and target is valid, enable PID now.
     * This handles the case: switch handle -> start -> set temperature. */
    if (g_dev_state.is_running && target_temp > 0) {
        temp_pid_set_enable(pid_idx, 1);
    } else if (target_temp <= 0 && g_dev_state.is_running) {
        /* User set temp to 0 while running: disable PID heating */
        temp_pid_set_enable(pid_idx, 0);
    }

    rt_kprintf("[PROTO] Temp set: handle %c = %u%% -> %.1f C, PID[%d] enabled=%d, running=%d\n",
               'A' + hi, percent, target_temp, pid_idx,
               temp_pid_is_enabled(pid_idx), g_dev_state.is_running);

    uint8_t ack_params[2] = { percent, handle_id };
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
        rt_kprintf("[PROTO] >>> START: handle=0x%02X wf=%d level=%d\n",
                   g_dev_state.current_handle, g_dev_state.waveform_id,
                   g_dev_state.current_level);

        g_dev_state.is_running = 1;

        /* V4.1: Enable current output only if current_level > 0 */
        if (g_dev_state.current_level > 0) {
            /* Enable global boost (PB1) for current output */
            bsp_boost_2_enable(1);  /* PB1 = global 54V boost */
            rt_thread_mdelay(10);   /* Soft-start delay */

            /* Apply waveform with current from global level */
            handle_apply_output(hi);
        } else {
            rt_kprintf("[PROTO] Current output skipped: level=0\n");
        }

        /* Enable pump (PB10) for handle C (independent of current level) */
        if (hi == 2) {
            bsp_pump_set(1);
            rt_kprintf("[PROTO] Pump enabled (handle C)\n");
        }

        /* Enable PID temperature control if target is set (independent of current) */
        int8_t pid_idx = s_handle_to_pid[hi];
        if (pid_idx >= 0 && temp_pid_get_target(pid_idx) > 0) {
            temp_pid_set_enable(pid_idx, 1);
            rt_kprintf("[PROTO] PID enabled for handle %c, target=%.1f\n",
                       'A' + hi, temp_pid_get_target(pid_idx));
        }

        /* Start periodic temperature reporting for handles with NTC */
        if (hi <= 1) {
            protocol_temp_report_start(g_dev_state.current_handle);
        }

        /* Debug: log PID state after start */
        {
            int8_t pid_chk = s_handle_to_pid[hi];
            if (pid_chk >= 0) {
                rt_kprintf("[PROTO] PID state: enabled=%d target=%.1f preheat=%d\n",
                           temp_pid_is_enabled(pid_chk),
                           temp_pid_get_target(pid_chk),
                           temp_pid_get(pid_chk)->preheat_active);
            }
        }

        rt_kprintf("[PROTO] Treatment started\n");
    } else {
        /* Pause: stop waveform output */
        handle_stop_output(hi);
        g_dev_state.is_running = 0;

        /* V4.1: Disable global boost (PB1) */
        bsp_boost_2_enable(0);

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

        rt_kprintf("[PROTO] Treatment paused (global boost disabled)\n");
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
    rt_kprintf("[PROTO] waveform_id saved = %u\n", waveform_id);

    /* V4.1: If treatment is running, apply new waveform immediately.
     * If not running, just save - will take effect on next start. */
    if (g_dev_state.is_running) {
        int hi = protocol_handle_index(g_dev_state.current_handle);
        if (hi >= 0 && g_dev_state.current_level > 0) {
            handle_apply_output(hi);
        }
    }

    rt_kprintf("[PROTO] Waveform selected: #%u (global)\n", waveform_id);

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

    /* Beep disabled: only beep on power on/off, not on every command */
    // BEEP_Blink(1, 0, 0);

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
    protocol_temp_report_stop();
    nnc6521_write_reg(NNC6521_CHIP_2, WAVEGEN_GLOBAL_REG_0, 0x00);
    nnc6521_awg_enable_disable(NNC6521_CHIP_2, WAVEFORM_GEN_CH0, 0);
    nnc6521_awg_enable_disable(NNC6521_CHIP_2, WAVEFORM_GEN_CH1, 0);
    nnc6521_analog_disable(NNC6521_CHIP_2, WAVEFORM_GEN_CH0);
    rt_kprintf("[PROTO] All waveform output stopped (CHIP_2 only)\n");
}

/**
 * @brief  Start waveform output on the current handle's chip/channel.
 *         Called by power_task or protocol module when needed.
 */
void protocol_start_waveform(void)
{
    int hi = protocol_handle_index(g_dev_state.current_handle);
    if (hi >= 0 && g_dev_state.is_running) {
        uint8_t level = g_dev_state.current_level;
        if (level == 0) {
            rt_kprintf("[PROTO] Waveform start skipped: level=0\n");
            return;
        }
        handle_apply_output(hi);
        rt_kprintf("[PROTO] Waveform started on handle %c\n", 'A' + hi);
    }
}
