/*
 * Protocol action handlers for DJM-V10
 * Command dispatch, waveform control, treatment start/stop
 */

#include "protocol_act.h"
#include "protocol.h"
#include "nnc6521_waveform_config.h"
#include "ntc_sensor.h"
#include "temp_pid.h"
#include "bsp_hard.h"
#include "power_task.h"
#include <string.h>

/* ============================================================================
 *  Waveform Current Ranges (per waveform ID, in mA)
 * ===========================================================================*/

static const uint16_t waveform_current_min[] = {
    0,  /* placeholder index 0 */
    30, /* WAVEFORM_POWER_SMOOTH   */
    30, /* WAVEFORM_BURST_TRAIN    */
    20, /* WAVEFORM_GENTLE_SMOOTH  */
    30, /* WAVEFORM_DEEP_SCULPT    */
    30, /* WAVEFORM_SOFT_SCULPT    */
    20, /* WAVEFORM_CIRCUL_SCULPT  */
    15, /* WAVEFORM_SMOOTH_FIRM    */
    15, /* WAVEFORM_LYMPH_DRAIN    */
    10  /* WAVEFORM_SOOTHING_END   */
};

static const uint16_t waveform_current_max[] = {
    0,  /* placeholder index 0 */
    80, /* WAVEFORM_POWER_SMOOTH   */
    80, /* WAVEFORM_BURST_TRAIN    */
    60, /* WAVEFORM_GENTLE_SMOOTH  */
    80, /* WAVEFORM_DEEP_SCULPT    */
    80, /* WAVEFORM_SOFT_SCULPT    */
    60, /* WAVEFORM_CIRCUL_SCULPT  */
    50, /* WAVEFORM_SMOOTH_FIRM    */
    40, /* WAVEFORM_LYMPH_DRAIN    */
    30  /* WAVEFORM_SOOTHING_END   */
};

/* ============================================================================
 *  Waveform Info String Tables
 * ===========================================================================*/

static const char *waveform_names[] = {
    "",                /* index 0: placeholder */
    "Power Smooth",    /* 1 */
    "Burst Train",     /* 2 */
    "Gentle Smooth",   /* 3 */
    "Deep Sculpt",     /* 4 */
    "Soft Sculpt",     /* 5 */
    "Circulation Sculpt", /* 6 */
    "Smooth & Firm",   /* 7 */
    "Lymphatic Drainage", /* 8 */
    "Soothing Ending"  /* 9 */
};

static const char *waveform_types[] = {
    "",                /* index 0: placeholder */
    "Preloaded Pulse",  /* 1 */
    "Customized Burst", /* 2 */
    "Preloaded Pulse",  /* 3 */
    "Customized Carrier", /* 4 */
    "Customized Sine",  /* 5 */
    "Custom SPI AM",    /* 6 */
    "Preloaded Triangle", /* 7 */
    "Customized Sine",  /* 8 */
    "Preloaded Sine"    /* 9 */
};

static const uint16_t waveform_freq_hz[] = {
    0,    /* index 0: placeholder */
    50,   /* 1: Power Smooth */
    50,   /* 2: Burst Train */
    35,   /* 3: Gentle Smooth */
    50,   /* 4: Deep Sculpt */
    40,   /* 5: Soft Sculpt */
    10,   /* 6: Circulation Sculpt */
    100,  /* 7: Smooth & Firm */
    5,    /* 8: Lymphatic Drainage */
    10    /* 9: Soothing Ending */
};

/* ============================================================================
 *  Waveform Helper Functions
 * ===========================================================================*/

/**
 * @brief  Map current percentage to actual current (mA) for the active waveform.
 *         actual_current = min + (max - min) * percent / 100
 */
uint16_t protocol_map_percent_to_current(uint8_t waveform_id, uint8_t percent)
{
    if (waveform_id < 1 || waveform_id > 9) return 0;
    if (percent == 0) return 0;

    uint16_t cmin = waveform_current_min[waveform_id];
    uint16_t cmax = waveform_current_max[waveform_id];
    return cmin + (uint16_t)((uint32_t)(cmax - cmin) * percent / 100);
}

/**
 * @brief  Print waveform info in the standard format.
 * @param  waveform_id  Waveform ID (1~9).
 * @param  current_ma   Actual current in mA.
 * @param  percent      Current percentage.
 */
static void waveform_print_info(uint8_t waveform_id, uint16_t current_ma, uint8_t percent)
{
    if (waveform_id < 1 || waveform_id > 9) return;

    rt_kprintf("========================================\n");
    rt_kprintf("Waveform #%u: %s\n", waveform_id, waveform_names[waveform_id]);
    rt_kprintf("  Current: %u mA (%u%%)\n", current_ma, percent);
    rt_kprintf("  Frequency: %u Hz\n", waveform_freq_hz[waveform_id]);
    rt_kprintf("  Type: %s\n", waveform_types[waveform_id]);
    rt_kprintf("========================================\n");
}

/* ============================================================================
 *  NNC6521 Waveform Control
 * ===========================================================================*/

/* waveform_apply() is now provided by nnc6521_waveform_config.c */

void protocol_update_current_output(uint8_t handle_idx)
{
    if (handle_idx > 2) return;

    uint8_t chip_id = (handle_idx == 0) ? NNC6521_CHIP_1 : NNC6521_CHIP_2;
    uint8_t channel = WAVEFORM_GEN_CH0;

    waveform_apply(chip_id, channel, g_dev_state.waveform_id,
                   g_dev_state.handle[handle_idx].current_percent);
}

void protocol_start_waveform(void)
{
    if (g_dev_state.is_running) return;

    g_dev_state.is_running = 1;
    uint8_t hi = protocol_handle_index(g_dev_state.current_handle);
    if (hi < 0) return;

    /* Update current output with saved percentage */
    protocol_update_current_output(hi);

    /* Print waveform info */
    uint8_t percent = g_dev_state.handle[hi].current_percent;
    uint16_t current_ma = protocol_map_percent_to_current(g_dev_state.waveform_id, percent);
    waveform_print_info(g_dev_state.waveform_id, current_ma, percent);
}

void protocol_stop_waveform(void)
{
    if (!g_dev_state.is_running) return;

    g_dev_state.is_running = 0;
    rt_kprintf("[PROTO] Stop treatment\n");

    /* Disable NNC6521 waveform generator based on current handle */
    int hi = protocol_handle_index(g_dev_state.current_handle);
    uint8_t chip_id = (hi == 0) ? NNC6521_CHIP_1 : NNC6521_CHIP_2;
    nnc6521_awg_enable_disable(chip_id, WAVEFORM_GEN_CH0, 0);
}

/* ============================================================================
 *  Command Handlers
 * ===========================================================================*/

/**
 * @brief  Handle 0x01: Switch active handle.
 *         para[0] = handle ID (0x0A/0x0B/0x0C)
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

    /* Stop current output before switching */
    protocol_stop_waveform();

    /* Save current handle state and restore target handle state */
    g_dev_state.current_handle = handle_id;
    rt_kprintf("[PROTO] Switch to handle %c (0x%02X)\n", 'A' + hi, handle_id);

    /* Send ACK with handle ID */
    uint8_t ack_params[1] = { handle_id };
    protocol_send_ack(FUNC_HANDLE_SWITCH, ack_params, 1);
}

/**
 * @brief  Handle 0x02: Current control.
 *         para[0] = current percentage (0~100)
 *         para[1] = target handle ID (0x0A/0x0B/0x0C)
 */
static void handle_current_ctrl(const uint8_t *params, uint8_t param_len)
{
    if (param_len < 2) {
        protocol_send_error(FUNC_CURRENT_CTRL, ERR_PARAM);
        return;
    }

    uint8_t percent = params[0];
    uint8_t handle_id = params[1];

    if (percent > 100) {
        protocol_send_error(FUNC_CURRENT_CTRL, ERR_PARAM);
        return;
    }

    int hi = protocol_handle_index(handle_id);
    if (hi < 0) {
        protocol_send_error(FUNC_CURRENT_CTRL, ERR_PARAM);
        return;
    }

    /* Save current percentage for the target handle */
    g_dev_state.handle[hi].current_percent = percent;

    /* If target handle is the current active handle, update output immediately */
    if (handle_id == g_dev_state.current_handle) {
        protocol_update_current_output(hi);
    }

    rt_kprintf("[PROTO] Current set: handle %c = %u%%\n", 'A' + hi, percent);

    /* Send ACK */
    uint8_t ack_params[2] = { percent, handle_id };
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

    /* Temperature range: 0~41 */
    if (temperature > 41) {
        protocol_send_error(FUNC_TEMP_CTRL, ERR_PARAM);
        return;
    }

    /* Only handle A and B support heating */
    int hi = protocol_handle_index(handle_id);
    if (hi < 0 || hi > 1) {
        protocol_send_error(FUNC_TEMP_CTRL, ERR_PARAM);
        return;
    }

    /* Save temperature for the target handle */
    g_dev_state.handle[hi].temperature = temperature;

    /* Update PID controller target temperature */
    temp_pid_set_target(hi, (float)temperature);

    rt_kprintf("[PROTO] Temp set: handle %c = %u C, PID %s\n",
               'A' + hi, temperature, temperature > 0 ? "enabled" : "disabled");

    /* Send ACK */
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

    /* Pump only applies to handle C */
    g_dev_state.handle[2].pump_speed = speed;

    /* Control vacuum pump via BSP abstraction layer */
    bsp_pump_set(speed > 0 ? 1 : 0);

    rt_kprintf("[PROTO] Pump speed set: %u%%\n", speed);

    /* Send ACK */
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

    if (action == 1) {
        protocol_start_waveform();
    } else if (action == 0) {
        protocol_stop_waveform();
    } else {
        protocol_send_error(FUNC_START_PAUSE, ERR_PARAM);
        return;
    }

    /* Send ACK */
    uint8_t ack_params[1] = { action };
    protocol_send_ack(FUNC_START_PAUSE, ack_params, 1);
}

/**
 * @brief  Handle 0x06: OTA firmware upgrade (reserved).
 */
static void handle_ota_upgrade(const uint8_t *params, uint8_t param_len)
{
    /* OTA upgrade: reserved for future implementation */
    rt_kprintf("[PROTO] OTA upgrade requested (not implemented)\n");

    /* Send ACK - device entering upgrade mode */
    uint8_t ack_params[1] = { 0 };
    protocol_send_ack(FUNC_OTA_UPGRADE, ack_params, 1);
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

    if (action == 1) {
        g_dev_state.aging_mode = 1;
        /* Stop current treatment */
        protocol_stop_waveform();
        rt_kprintf("[PROTO] Enter aging mode\n");
        /* Aging test sequence: reserved for future implementation */
    } else if (action == 0) {
        g_dev_state.aging_mode = 0;
        rt_kprintf("[PROTO] Exit aging mode\n");
        /* Aging test sequence: reserved for future implementation */
    } else {
        protocol_send_error(FUNC_AGING_MODE, ERR_PARAM);
        return;
    }

    /* Send ACK */
    uint8_t ack_params[1] = { action };
    protocol_send_ack(FUNC_AGING_MODE, ack_params, 1);
}

/**
 * @brief  Handle 0x08: Read firmware version (no params).
 *         Response: para[0]=major version, para[1]=minor version
 */
static void handle_read_version(void)
{
    uint8_t ack_params[2] = { DEVICE_VERSION_H, DEVICE_VERSION_L };
    protocol_send_ack(FUNC_READ_VERSION, ack_params, 2);
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

    /* Stop current output before switching waveform */
    protocol_stop_waveform();

    /* Save new waveform selection */
    g_dev_state.waveform_id = waveform_id;

    /* Print waveform info on selection change */
    uint8_t percent = 0;
    uint8_t hi = protocol_handle_index(g_dev_state.current_handle);
    if (hi >= 0) {
        percent = g_dev_state.handle[hi].current_percent;
    }
    uint16_t current_ma = protocol_map_percent_to_current(waveform_id, percent);
    waveform_print_info(waveform_id, current_ma, percent);

    /* Waveform config will be applied via waveform_apply() when treatment
     * starts via protocol_start_waveform(). Current percentage is preserved.
     */

    /* Send ACK */
    uint8_t ack_params[1] = { waveform_id };
    protocol_send_ack(FUNC_WAVEFORM_SEL, ack_params, 1);
}

/* ============================================================================
 *  Frame Dispatch (Main Command Router)
 * ===========================================================================*/

/**
 * @brief  Dispatch a validated command frame to the appropriate handler.
 *         Called by protocol.c after CRC verification.
 *
 * @param  buf  Command buffer: [len] [addrH] [addrL] [type] [state] [func] [params...]
 * @param  cmd_len  Length field from the frame (value of buf[0]).
 */
void protocol_dispatch(uint8_t *buf, uint8_t cmd_len)
{
    uint8_t type  = buf[3];  /* Offset: type is at index 3 (after len, addrH, addrL) */
    uint8_t state = buf[4];  /* Offset: state at index 4 */
    uint8_t func  = buf[5];  /* Offset: func at index 5 */
    uint8_t param_len = cmd_len - 5;  /* params = len - 5 */

    /* Only process host requests (ASK), ignore device ACKs */
    if (state != FRAME_STATE_ASK) {
        return;
    }

    /* Check frame type */
    if (type != FRAME_TYPE_ACT && type != FRAME_TYPE_GET) {
        rt_kprintf("[PROTO] Unknown frame type: 0x%02X\n", type);
        return;
    }

    rt_kprintf("[PROTO] Dispatch: type=0x%02X func=0x%02X param_len=%u\n",
               type, func, param_len);

    /* Route to handler based on type + func */
    if (type == FRAME_TYPE_ACT) {
        switch (func) {
            case FUNC_HANDLE_SWITCH: handle_switch(&buf[6], param_len);      break;
            case FUNC_CURRENT_CTRL:  handle_current_ctrl(&buf[6], param_len); break;
            case FUNC_TEMP_CTRL:     handle_temp_ctrl(&buf[6], param_len);    break;
            case FUNC_PUMP_CTRL:     handle_pump_ctrl(&buf[6], param_len);    break;
            case FUNC_START_PAUSE:   handle_start_pause(&buf[6], param_len);  break;
            case FUNC_OTA_UPGRADE:   handle_ota_upgrade(&buf[6], param_len);  break;
            case FUNC_AGING_MODE:    handle_aging_mode(&buf[6], param_len);   break;
            case FUNC_READ_VERSION:  handle_read_version();                    break;
            case FUNC_WAVEFORM_SEL:  handle_waveform_sel(&buf[6], param_len); break;
            case FUNC_SHUTDOWN_REQ:
                if (param_len >= 1 && buf[6] == 0x01) {
                    power_shutdown_confirm();
                }
                break;
            default:
                rt_kprintf("[PROTO] Unsupported function code: 0x%02X\n", func);
                protocol_send_error(func, ERR_UNSUPPORTED);
                break;
        }
    } else if (type == FRAME_TYPE_GET) {
        /* Query commands: only version query is currently defined */
        switch (func) {
            case FUNC_READ_VERSION:  handle_read_version(); break;
            default:
                rt_kprintf("[PROTO] Unsupported query func: 0x%02X\n", func);
                protocol_send_error(func, ERR_UNSUPPORTED);
                break;
        }
    }
}
