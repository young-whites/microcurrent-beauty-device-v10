/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06-16     auto-gen     protocol.c for DJM-V10 microcurrent beauty device
 *                              Full protocol parser, command handler, NNC6521 control
 */

#include "protocol.h"
#include "ntc_sensor.h"
#include "temp_pid.h"
#include <string.h>

/* ============================================================================
 *  Internal Variables
 * ===========================================================================*/

/* UART device handle */
rt_device_t g_serial1 = RT_NULL;

/* UART receive semaphore (released by RX callback) */
static rt_sem_t s_rx_sem = RT_NULL;

/* UART receive circular buffer */
static ring_buffer_t s_rx_ring;

/* UART transmit mutex */
static rt_mutex_t s_tx_mutex = RT_NULL;

/* Decode thread handle */
static rt_thread_t s_decode_thread = RT_NULL;

/* Global device state */
device_state_t g_dev_state;

/* Decode state machine variables (persistent across calls) */
static decode_state_t s_decode_state = DECODE_IDLE;
static uint8_t s_cmd_len = 0;
static uint8_t s_cmd_buf[PROTO_CMD_BUF_SIZE];
static uint8_t s_cmd_cnt = 0;
static uint8_t s_crc_h = 0;
static uint8_t s_crc_l = 0;

/* Waveform min/max current ranges per waveform ID (in mA) */
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
 *  Internal Helper: Ring Buffer Operations
 * ===========================================================================*/

/**
 * @brief  Push one byte into the ring buffer (thread-safe).
 * @return RT_EOK on success, -RT_EFULL if buffer is full.
 */
static rt_err_t ring_push(uint8_t data)
{
    rt_err_t ret = RT_EOK;
    rt_mutex_take(s_rx_ring.lock, RT_WAITING_FOREVER);

    uint16_t next_tail = (s_rx_ring.tail + 1) % PROTO_RX_BUF_SIZE;
    if (next_tail != s_rx_ring.head) {
        s_rx_ring.buffer[s_rx_ring.tail] = data;
        s_rx_ring.tail = next_tail;
    } else {
        rt_kprintf("[PROTO] RX ring buffer full, discarding 0x%02X\n", data);
        ret = -RT_EFULL;
    }

    rt_mutex_release(s_rx_ring.lock);
    return ret;
}

/**
 * @brief  Pop one byte from the ring buffer.
 * @return 1 if a byte was read, 0 if buffer is empty.
 */
static int ring_pop(uint8_t *data)
{
    int ret = 0;
    rt_mutex_take(s_rx_ring.lock, RT_WAITING_FOREVER);

    if (s_rx_ring.head != s_rx_ring.tail) {
        *data = s_rx_ring.buffer[s_rx_ring.head];
        s_rx_ring.head = (s_rx_ring.head + 1) % PROTO_RX_BUF_SIZE;
        ret = 1;
    }

    rt_mutex_release(s_rx_ring.lock);
    return ret;
}

/* ============================================================================
 *  CRC16-MODBUS Implementation
 * ===========================================================================*/

uint16_t protocol_crc16_modbus(const uint8_t *data, uint8_t len)
{
    uint16_t crc = 0xFFFF;

    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/* ============================================================================
 *  UART TX: Build and Send Frame
 * ===========================================================================*/

/**
 * @brief  Build a complete frame and send it via UART1.
 *         Frame format: 55 AA [len] 60 66 [type] [state] [func] [params...] [crcH] [crcL]
 *
 * @param  type      FRAME_TYPE_ACT or FRAME_TYPE_GET
 * @param  state     FRAME_STATE_ASK or FRAME_STATE_ACK
 * @param  func      Function code
 * @param  params    Parameter data (may be NULL if param_len == 0)
 * @param  param_len Number of parameter bytes
 */
static void protocol_build_and_send(uint8_t type, uint8_t state, uint8_t func,
                                    const uint8_t *params, uint8_t param_len)
{
    uint8_t tx_buf[PROTO_TX_BUF_SIZE];
    uint8_t frame_len;

    /* Frame: len = 5 + param_len */
    frame_len = 5 + param_len;

    /* Build frame */
    tx_buf[0] = FRAME_HEAD1;
    tx_buf[1] = FRAME_HEAD2;
    tx_buf[2] = frame_len;      /* len field */
    tx_buf[3] = DEVICE_ID_H;
    tx_buf[4] = DEVICE_ID_L;
    tx_buf[5] = type;
    tx_buf[6] = state;
    tx_buf[7] = func;

    if (params != NULL && param_len > 0) {
        rt_memcpy(&tx_buf[8], params, param_len);
    }

    /* Calculate CRC over len ~ last param byte (frame_len + 1 bytes starting from tx_buf[2]) */
    uint16_t crc = protocol_crc16_modbus(&tx_buf[2], frame_len + 1);
    tx_buf[8 + param_len]     = (uint8_t)(crc >> 8);     /* CRC high byte */
    tx_buf[8 + param_len + 1] = (uint8_t)(crc & 0xFF);  /* CRC low byte  */

    /* Total frame size: 2(head) + 1(len) + frame_len + 2(crc) = frame_len + 5 */
    uint8_t total = frame_len + 5;

    /* Send frame (critical section to prevent interleaving) */
    if (s_tx_mutex != RT_NULL) {
        rt_mutex_take(s_tx_mutex, RT_WAITING_FOREVER);
    }
    rt_enter_critical();
    rt_device_write(g_serial1, RT_NULL, tx_buf, total);
    rt_exit_critical();
    if (s_tx_mutex != RT_NULL) {
        rt_mutex_release(s_tx_mutex);
    }

    /* Debug log */
    rt_kprintf("[PROTO] TX: ");
    for (uint8_t i = 0; i < total; i++) {
        rt_kprintf("%02X ", tx_buf[i]);
    }
    rt_kprintf("\n");
}

void protocol_send_ack(uint8_t func, const uint8_t *params, uint8_t param_len)
{
    protocol_build_and_send(FRAME_TYPE_ACT, FRAME_STATE_ACK, func, params, param_len);
}

void protocol_send_error(uint8_t func, uint8_t err_code)
{
    uint8_t param[1] = { err_code };
    protocol_build_and_send(FRAME_TYPE_ACT, FRAME_STATE_ACK, func, param, 1);
}

/* ============================================================================
 *  Handle Utility
 * ===========================================================================*/

int protocol_handle_index(uint8_t handle_id)
{
    switch (handle_id) {
        case HANDLE_A: return 0;
        case HANDLE_B: return 1;
        case HANDLE_C: return 2;
        default:       return -1;
    }
}

/* ============================================================================
 *  NNC6521 Waveform Control
 * ===========================================================================*/

/**
 * @brief  Map current percentage to actual current (mA) for the active waveform.
 *         actual_current = min + (max - min) * percent / 100
 */
static uint16_t map_percent_to_current(uint8_t waveform_id, uint8_t percent)
{
    if (waveform_id < 1 || waveform_id > 9) return 0;
    if (percent == 0) return 0;

    uint16_t cmin = waveform_current_min[waveform_id];
    uint16_t cmax = waveform_current_max[waveform_id];
    return cmin + (uint16_t)((uint32_t)(cmax - cmin) * percent / 100);
}

/**
 * @brief  Waveform name string table (indexed by waveform_id 1~9).
 */
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
    "AM Sine",          /* 6 */
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

/**
 * @brief  Apply waveform to NNC6521 based on waveform ID and current percentage.
 *         Follows the NNC6521 waveform design specification exactly.
 * @param  chip_id      NNC6521_CHIP_1 or NNC6521_CHIP_2.
 * @param  channel      Waveform generator channel (WAVEFORM_GEN_CH0/CH1).
 * @param  waveform_id  Waveform ID (1~9).
 * @param  percent      Current percentage (0~100).
 */

/**
 * @brief  Set PCLK divider for a chip. Call before waveform config for low-freq waveforms.
 */
static void set_pclk_divider(uint8_t chip_id, uint8_t divider)
{
    nnc6521_write_reg(chip_id, CLK_CTRL_REG_ADDR, divider);
}

void waveform_apply(uint8_t chip_id, uint8_t channel,
                    uint8_t waveform_id, uint8_t percent)
{
    if (waveform_id < 1 || waveform_id > 9) return;

    if (percent == 0) {
        nnc6521_awg_enable_disable(chip_id, channel, 0);
        return;
    }

    uint16_t current_ma = map_percent_to_current(waveform_id, percent);
    uint32_t current_ua = (uint32_t)current_ma * 1000;  /* mA → μA for NNC6521 API */
    uint8_t ci = (uint8_t)((float)current_ma * 255.0f / waveform_current_max[waveform_id]);

    switch (waveform_id) {
    case 1: /* Power Smooth: Preloaded PULSE, 50Hz, 300us */
        nnc6521_preloaded_waveform(chip_id, channel, WAVEFORM_PULSE, 64, ci,
                                   20000, 20000, 600, 0);
        break;
    case 2: /* Burst Train: Customized SPI burst_pulse_64, 50Hz, 300us */
        nnc6521_customized_waveform(chip_id, channel, 64, burst_pulse_64,
                                    current_ua, 20000, 20000, 600, 0, 0);
        break;
    case 3: /* Gentle Smooth: Preloaded PULSE, 35Hz, 300us */
        nnc6521_preloaded_waveform(chip_id, channel, WAVEFORM_PULSE, 64, ci,
                                   28571, 28571, 600, 0);
        break;
    case 4: /* Deep Sculpt: Customized SPI, 50Hz, 4kHz carrier */
        nnc6521_customized_waveform(chip_id, channel, 128, deep_sculpt_pulse_128,
                                    current_ua, 20000, 20000, 250, 0, 0);
        break;
    case 5: /* Soft Sculpt: Customized SPI sine 128, 40Hz */
        nnc6521_customized_waveform(chip_id, channel, 128, normalized_sine_waveform_128,
                                    current_ua, 25000, 25000, 0, 0, 1);
        break;
    case 6: /* Circulation Sculpt: AM, sine 64, 10Hz envelope, 4kHz carrier */
        nnc6521_amplitude_modulation(chip_id, channel, 64, normalized_sine_waveform_64,
                                     current_ua, 250, 0, 3125);
        break;
    case 7: /* Smooth & Firm: Preloaded TRIANGLE, 100Hz, 400us */
        nnc6521_preloaded_waveform(chip_id, channel, WAVEFORM_TRIANGLE, 64, ci,
                                   10000, 10000, 800, 0);
        break;
    case 8: /* Lymphatic Drainage: 5Hz, needs PCLK/16 */
        set_pclk_divider(chip_id, PCLK_DIV_16);
        nnc6521_customized_waveform(chip_id, channel, 64, normalized_sine_waveform_64,
                                    current_ua, 12500, 12500, 56, 0, 1);
        set_pclk_divider(chip_id, PCLK_DIV_1);
        break;
    case 9: /* Soothing Ending: 10Hz, needs PCLK/8 */
        set_pclk_divider(chip_id, PCLK_DIV_8);
        nnc6521_customized_waveform(chip_id, channel, 64, normalized_sine_waveform_64,
                                    current_ua, 12500, 12500, 0, 0, 1);
        set_pclk_divider(chip_id, PCLK_DIV_1);
        break;
    }
}

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
    uint16_t current_ma = map_percent_to_current(g_dev_state.waveform_id, percent);
    waveform_print_info(g_dev_state.waveform_id, current_ma, percent);
}

void protocol_stop_waveform(void)
{
    if (!g_dev_state.is_running) return;

    g_dev_state.is_running = 0;
    rt_kprintf("[PROTO] Stop treatment\n");

    /* TODO: Disable NNC6521 waveform generator
     * nnc6521_awg_enable_disable(chip_id, channel, 0);
     */
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

    /* TODO: Control hardware pump motor
     * if (speed == 0) { pump_stop(); }
     * else { pump_set_speed(speed); }
     */

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
    /* TODO: Implement OTA upgrade sequence */
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
        /* TODO: Start aging test sequence */
    } else if (action == 0) {
        g_dev_state.aging_mode = 0;
        rt_kprintf("[PROTO] Exit aging mode\n");
        /* TODO: Stop aging test sequence */
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
    uint16_t current_ma = map_percent_to_current(waveform_id, percent);
    waveform_print_info(waveform_id, current_ma, percent);

    /* TODO: Configure NNC6521 for new waveform type
     * The actual waveform parameters will be applied when treatment starts.
     * Protocol: switch waveform -> current percentage is preserved.
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
 *
 * @param  buf  Command buffer containing: [len] [addrH] [addrL] [type] [state] [func] [params...]
 */
static void protocol_dispatch(uint8_t *buf)
{
    uint8_t type  = buf[3];  /* Offset: type is at index 3 (after len, addrH, addrL) */
    uint8_t state = buf[4];  /* Offset: state at index 4 */
    uint8_t func  = buf[5];  /* Offset: func at index 5 */
    uint8_t param_len = s_cmd_len - 5;  /* params = len - 5 */

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

/* ============================================================================
 *  Protocol State Machine (Decode Engine)
 * ===========================================================================*/

/**
 * @brief  Feed one byte into the protocol state machine.
 *         On complete frame, verify CRC and dispatch command.
 *
 * @param  byte  Received byte.
 */
static void protocol_decode_byte(uint8_t byte)
{
    switch (s_decode_state) {

        case DECODE_IDLE:
            if (byte == FRAME_HEAD1) {
                s_decode_state = DECODE_HEAD2;
            }
            break;

        case DECODE_HEAD2:
            if (byte == FRAME_HEAD2) {
                s_decode_state = DECODE_LENGTH;
            } else {
                s_decode_state = DECODE_IDLE;
            }
            break;

        case DECODE_LENGTH:
            s_cmd_len = byte;
            if (s_cmd_len < 5 || s_cmd_len > (PROTO_CMD_BUF_SIZE - 3)) {
                /* Invalid length, reset */
                s_decode_state = DECODE_IDLE;
            } else {
                s_cmd_buf[0] = byte;
                s_cmd_cnt = 1;
                s_decode_state = DECODE_ADDR_H;
            }
            break;

        case DECODE_ADDR_H:
            if (byte == DEVICE_ID_H) {
                s_cmd_buf[s_cmd_cnt++] = byte;
                s_decode_state = DECODE_ADDR_L;
            } else {
                s_decode_state = DECODE_IDLE;
                s_cmd_cnt = 0;
            }
            break;

        case DECODE_ADDR_L:
            if (byte == DEVICE_ID_L) {
                s_cmd_buf[s_cmd_cnt++] = byte;
                s_decode_state = DECODE_PAYLOAD;
            } else {
                s_decode_state = DECODE_IDLE;
                s_cmd_cnt = 0;
            }
            break;

        case DECODE_PAYLOAD:
            s_cmd_buf[s_cmd_cnt++] = byte;
            /* We need: len + 1 bytes total in cmd_buf (len byte + len bytes of payload) */
            if (s_cmd_cnt >= (s_cmd_len + 1)) {
                s_decode_state = DECODE_CRC_H;
            }
            break;

        case DECODE_CRC_H:
            s_crc_h = byte;
            s_decode_state = DECODE_CRC_L;
            break;

        case DECODE_CRC_L:
            s_crc_l = byte;
            s_decode_state = DECODE_IDLE;

            /* Verify CRC16-MODBUS */
            {
                uint16_t calc_crc = protocol_crc16_modbus(s_cmd_buf, s_cmd_len + 1);
                uint16_t recv_crc = ((uint16_t)s_crc_h << 8) | s_crc_l;

                if (calc_crc == recv_crc) {
                    /* Valid frame, dispatch to handler */
                    rt_kprintf("[PROTO] CRC OK, dispatching\n");
                    protocol_dispatch(s_cmd_buf);
                } else {
                    rt_kprintf("[PROTO] CRC mismatch: calc=0x%04X recv=0x%04X\n",
                               calc_crc, recv_crc);
                }
            }

            /* Reset for next frame */
            s_cmd_cnt = 0;
            s_crc_h = 0;
            s_crc_l = 0;
            break;

        default:
            s_decode_state = DECODE_IDLE;
            break;
    }
}

/* ============================================================================
 *  UART RX Callback
 * ===========================================================================*/

/**
 * @brief  UART receive indication callback (called from ISR context).
 *         Releases the semaphore to wake up the decode thread.
 */
static rt_err_t uart_rx_callback(rt_device_t dev, rt_size_t size)
{
    rt_sem_release(s_rx_sem);
    return RT_EOK;
}

/* ============================================================================
 *  Decode Thread Entry
 * ===========================================================================*/

/**
 * @brief  UART1 decode thread.
 *         Waits for RX semaphore, reads bytes from UART, pushes into ring buffer,
 *         then feeds bytes into the protocol state machine.
 */
static void protocol_decode_thread_entry(void *parameter)
{
    uint8_t rx_byte;

    rt_kprintf("[PROTO] Decode thread started\n");

    while (1) {
        /* Wait for data available (semaphore released by RX callback) */
        rt_sem_take(s_rx_sem, RT_WAITING_FOREVER);

        /* Read all available bytes from UART */
        while (rt_device_read(g_serial1, RT_NULL, &rx_byte, 1) == 1) {
            /* Push into ring buffer */
            ring_push(rx_byte);

            /* Feed into state machine */
            protocol_decode_byte(rx_byte);
        }

        rt_thread_mdelay(5);
    }
}

/* ============================================================================
 *  UART1 Hardware Initialization
 * ===========================================================================*/

/**
 * @brief  Initialize UART1 hardware: 9600bps, 8N1, interrupt RX mode.
 */
static int uart1_hardware_init(void)
{
    struct serial_configure uart_cfg = RT_SERIAL_CONFIG_DEFAULT;

    /* Find UART device */
    g_serial1 = rt_device_find(PROTO_UART_NAME);
    if (g_serial1 == RT_NULL) {
        rt_kprintf("[PROTO] UART device '%s' not found!\n", PROTO_UART_NAME);
        return -RT_ERROR;
    }

    /* Configure UART parameters */
    uart_cfg.baud_rate = PROTO_UART_BAUD;
    uart_cfg.data_bits = DATA_BITS_8;
    uart_cfg.stop_bits = STOP_BITS_1;
    uart_cfg.parity    = PARITY_NONE;
    uart_cfg.bufsz     = 1024;

    rt_device_control(g_serial1, RT_DEVICE_CTRL_CONFIG, &uart_cfg);

    /* Open device in read-only + interrupt RX mode */
    rt_err_t ret = rt_device_open(g_serial1,
                                  RT_DEVICE_OFLAG_RDONLY | RT_DEVICE_FLAG_INT_RX);
    if (ret != RT_EOK) {
        rt_kprintf("[PROTO] Failed to open UART: %d\n", ret);
        return ret;
    }

    /* Set RX indication callback */
    rt_device_set_rx_indicate(g_serial1, uart_rx_callback);

    rt_kprintf("[PROTO] UART1 initialized: %d bps, 8N1\n", PROTO_UART_BAUD);
    return RT_EOK;
}

/* ============================================================================
 *  Module Initialization
 * ===========================================================================*/

int protocol_init(void)
{
    rt_err_t ret;

    rt_kprintf("[PROTO] Protocol module initializing...\n");

    /* Initialize device state */
    rt_memset(&g_dev_state, 0, sizeof(g_dev_state));
    g_dev_state.current_handle = HANDLE_A;  /* Default to handle A */
    g_dev_state.waveform_id    = WAVEFORM_POWER_SMOOTH;  /* Default waveform */
    g_dev_state.is_running     = 0;
    g_dev_state.aging_mode     = 0;

    /* Initialize ring buffer */
    s_rx_ring.head = 0;
    s_rx_ring.tail = 0;
    s_rx_ring.lock = rt_mutex_create("proto_rx_lock", RT_IPC_FLAG_FIFO);
    if (s_rx_ring.lock == RT_NULL) {
        rt_kprintf("[PROTO] Failed to create RX mutex\n");
        return -RT_ERROR;
    }

    /* Create TX mutex */
    s_tx_mutex = rt_mutex_create("proto_tx_lock", RT_IPC_FLAG_FIFO);
    if (s_tx_mutex == RT_NULL) {
        rt_kprintf("[PROTO] Failed to create TX mutex\n");
        return -RT_ERROR;
    }

    /* Create RX semaphore */
    s_rx_sem = rt_sem_create("proto_rx_sem", 0, RT_IPC_FLAG_FIFO);
    if (s_rx_sem == RT_NULL) {
        rt_kprintf("[PROTO] Failed to create RX semaphore\n");
        return -RT_ERROR;
    }

    /* Initialize UART1 hardware */
    ret = uart1_hardware_init();
    if (ret != RT_EOK) {
        return ret;
    }

    /* Create decode thread */
    s_decode_thread = rt_thread_create("proto_decode",
                                       protocol_decode_thread_entry,
                                       RT_NULL,
                                       2048,   /* Stack size */
                                       10,     /* Priority */
                                       200);   /* Tick */
    if (s_decode_thread == RT_NULL) {
        rt_kprintf("[PROTO] Failed to create decode thread\n");
        return -RT_ERROR;
    }

    rt_thread_startup(s_decode_thread);
    rt_kprintf("[PROTO] Protocol module initialized OK\n");

    return RT_EOK;
}

/* Auto-initialize at APP_INIT level */
INIT_APP_EXPORT(protocol_init);
