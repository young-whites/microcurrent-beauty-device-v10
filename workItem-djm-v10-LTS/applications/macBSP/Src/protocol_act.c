/*
 * Protocol action handlers for DJM-V10
 * Command dispatch, waveform control, treatment start/stop
 *
 * LTS version: all handlers use state-only logic (no hardware deps).
 * Hardware control (NNC6521, heater, pump, PID) will be added later.
 */

#include "protocol_act.h"
#include "protocol.h"
#include <string.h>

/* ============================================================================
 *  Command Handlers (state-only, no hardware dependencies)
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

    /* Clear all handles' parameters (mutually exclusive) */
    for (int i = 0; i < 3; i++) {
        g_dev_state.handle[i].current_percent = 0;
        g_dev_state.handle[i].temperature = 0;
        g_dev_state.handle[i].pump_speed = 0;
    }

    /* Set new active handle */
    g_dev_state.current_handle = handle_id;
    g_dev_state.is_running = 0;

    rt_kprintf("[PROTO] Switch to handle %c (0x%02X), params cleared\n",
               'A' + hi, handle_id);

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

    g_dev_state.handle[hi].current_percent = percent;

    rt_kprintf("[PROTO] Current set: handle %c = %u%%\n", 'A' + hi, percent);

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

    if (temperature > 41) {
        protocol_send_error(FUNC_TEMP_CTRL, ERR_PARAM);
        return;
    }

    int hi = protocol_handle_index(handle_id);
    if (hi < 0 || hi > 1) {
        protocol_send_error(FUNC_TEMP_CTRL, ERR_PARAM);
        return;
    }

    g_dev_state.handle[hi].temperature = temperature;

    rt_kprintf("[PROTO] Temp set: handle %c = %u C\n", 'A' + hi, temperature);

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

    rt_kprintf("[PROTO] Pump speed set: %u%%\n", speed);

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

    g_dev_state.is_running = action;

    rt_kprintf("[PROTO] Treatment %s\n", action ? "started" : "paused");

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

    rt_kprintf("[PROTO] Waveform selected: #%u\n", waveform_id);

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

    if (type == FRAME_TYPE_ACT || type == FRAME_TYPE_GET) {
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
            default:
                rt_kprintf("[PROTO] Unsupported function code: 0x%02X\n", func);
                protocol_send_error(func, ERR_UNSUPPORTED);
                break;
        }
    }
}
