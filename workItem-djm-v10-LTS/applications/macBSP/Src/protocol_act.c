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
#include "nnc6521.h"
#include "nnc6521_waveform_config.h"
#include "nnc6521_waveform_config.h"
#include <string.h>

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
 *         Uses current device state (waveform_id, current_percent).
 */
static void handle_apply_output(int handle_idx)
{
    uint8_t chip_id = handle_to_chip(handle_idx);
    uint8_t wf_id   = g_dev_state.waveform_id;
    uint8_t percent  = g_dev_state.handle[handle_idx].current_percent;

    uint8_t channel = handle_to_channel(handle_idx);

    if (percent == 0) {
        nnc6521_awg_enable_disable(chip_id, channel, 0);
        rt_kprintf("[PROTO] Current 0%%, output disabled on chip %d ch %d\n", chip_id, channel);
        return;
    }

    waveform_apply(chip_id, channel, wf_id, percent);

    uint32_t current_ma = waveform_calc_current(wf_id, percent);
    rt_kprintf("[PROTO] Applied waveform #%u on chip %d ch %d: %u%% (%lu mA)\n",
               wf_id, chip_id, channel, percent, current_ma);
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

    /* Stop waveform on current handle's chip */
    int old_hi = protocol_handle_index(g_dev_state.current_handle);
    if (old_hi >= 0 && g_dev_state.is_running) {
        handle_stop_output(old_hi);
    }

    /* Disable boost for the old handle */
    if (old_hi >= 0) {
        if (old_hi <= 1) {
            bsp_boost_1_enable(0);  /* Handle A/B -> CHIP_1 */
        } else {
            bsp_boost_2_enable(0);  /* Handle C -> CHIP_2 */
        }
    }

    /* Turn off all heaters and pump */
    bsp_heater_large_set(0);
    bsp_heater_small_set(0);
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
 * @brief  Handle 0x02: Current control.
 *         para[0] = current high byte (mA)
 *         para[1] = current low byte (mA)
 *         para[2] = target handle ID (0x0A/0x0B/0x0C)
 */
static void handle_current_ctrl(const uint8_t *params, uint8_t param_len)
{
    if (param_len < 3) {
        protocol_send_error(FUNC_CURRENT_CTRL, ERR_PARAM);
        return;
    }

    uint16_t current_ma = (params[0] << 8) | params[1];  /* mA value from upper machine */
    uint8_t handle_id = params[2];

    int hi = protocol_handle_index(handle_id);
    if (hi < 0) {
        protocol_send_error(FUNC_CURRENT_CTRL, ERR_PARAM);
        return;
    }

    /* Validate against current waveform's range */
    const waveform_config_t *cfg = waveform_get_config(g_dev_state.waveform_id);
    if (cfg == NULL) {
        protocol_send_error(FUNC_CURRENT_CTRL, ERR_PARAM);
        return;
    }

    /* Clamp to waveform range */
    if (current_ma < cfg->min_current) current_ma = cfg->min_current;
    if (current_ma > cfg->max_current) current_ma = cfg->max_current;

    /* Convert mA to percentage for NNC6521 driver */
    uint32_t range = cfg->max_current - cfg->min_current;
    uint8_t percent = (range > 0) ? ((current_ma - cfg->min_current) * 100 / range) : 0;

    /* Store both values */
    g_dev_state.handle[hi].current_ma = current_ma;
    g_dev_state.handle[hi].current_percent = percent;

    /* If this is the active handle and treatment is running, update amplitude */
    if (handle_id == g_dev_state.current_handle && g_dev_state.is_running) {
        uint8_t chip_id = handle_to_chip(hi);
        uint8_t channel = handle_to_channel(hi);
        waveform_update_amplitude(chip_id, channel, g_dev_state.waveform_id, percent);
    }

    rt_kprintf("[PROTO] Current set: handle %c = %u mA (%u%%)\n", 'A' + hi, current_ma, percent);

    uint8_t ack_params[3] = { params[0], params[1], handle_id };
    protocol_send_ack(FUNC_CURRENT_CTRL, ack_params, 3);
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

    /* Control heater hardware (on/off only, no PID) */
    if (hi == 0) {
        bsp_heater_large_set(temperature > 0 ? 1 : 0);  /* Handle A -> large heater */
    } else {
        bsp_heater_small_set(temperature > 0 ? 1 : 0);  /* Handle B -> small heater */
    }

    rt_kprintf("[PROTO] Temp set: handle %c = %u C, heater %s\n",
               'A' + hi, temperature, temperature > 0 ? "ON" : "OFF");

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

    /* Control pump hardware (on/off only) */
    bsp_pump_set(speed > 0 ? 1 : 0);

    rt_kprintf("[PROTO] Pump speed set: %u%%, pump %s\n", speed,
               speed > 0 ? "ON" : "OFF");

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
        handle_apply_output(hi);
        rt_kprintf("[PROTO] Treatment started (boost enabled)\n");
    } else {
        /* Pause: stop waveform output, then disable boost */
        handle_stop_output(hi);
        g_dev_state.is_running = 0;
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

    /* Recalculate percent for all handles based on new waveform range */
    for (int i = 0; i < 3; i++) {
        if (g_dev_state.handle[i].current_ma > 0) {
            const waveform_config_t *new_cfg = waveform_get_config(waveform_id);
            if (new_cfg) {
                uint16_t ma = g_dev_state.handle[i].current_ma;
                if (ma < new_cfg->min_current) ma = new_cfg->min_current;
                if (ma > new_cfg->max_current) ma = new_cfg->max_current;
                uint32_t range = new_cfg->max_current - new_cfg->min_current;
                g_dev_state.handle[i].current_percent = (range > 0) ?
                    ((ma - new_cfg->min_current) * 100 / range) : 0;
                g_dev_state.handle[i].current_ma = ma;
            }
        }
    }

    /* If treatment is running, apply new waveform immediately */
    int hi = protocol_handle_index(g_dev_state.current_handle);
    if (hi >= 0 && g_dev_state.is_running) {
        handle_apply_output(hi);
    }

    rt_kprintf("[PROTO] Waveform selected: #%u\n", waveform_id);

    uint8_t ack_params[1] = { waveform_id };
    protocol_send_ack(FUNC_WAVEFORM_SEL, ack_params, 1);
}

/* ============================================================================
 *  Frame Dispatch (Main Command Router)
 * ===========================================================================*/

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
            case FUNC_HANDLE_SWITCH: handle_switch(&buf[6], param_len);       break;
            case FUNC_CURRENT_CTRL:  handle_current_ctrl(&buf[6], param_len); break;
            case FUNC_TEMP_CTRL:     handle_temp_ctrl(&buf[6], param_len);    break;
            case FUNC_PUMP_CTRL:     handle_pump_ctrl(&buf[6], param_len);    break;
            case FUNC_START_PAUSE:   handle_start_pause(&buf[6], param_len);  break;
            case FUNC_OTA_UPGRADE:   handle_ota_upgrade(&buf[6], param_len);  break;
            case FUNC_AGING_MODE:    handle_aging_mode(&buf[6], param_len);   break;
            case FUNC_READ_VERSION:  handle_read_version();                   break;
            case FUNC_WAVEFORM_SEL:  handle_waveform_sel(&buf[6], param_len); break;
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
        waveform_apply(chip_id, channel,
                       g_dev_state.waveform_id,
                       g_dev_state.handle[hi].current_percent);
        rt_kprintf("[PROTO] Waveform started on chip %d ch %d\n", chip_id, channel);
    }
}
