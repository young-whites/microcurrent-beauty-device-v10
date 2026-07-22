/*
 * Protocol action handlers for DJM-V10
 * Command dispatch, waveform control, treatment start/stop
 */
#ifndef __PROTOCOL_ACT_H__
#define __PROTOCOL_ACT_H__

#include "protocol.h"

/**
 * @brief  Dispatch a validated command frame to the appropriate handler.
 *         Called by protocol.c after CRC verification.
 * @param  buf  Command buffer: [len] [addrH] [addrL] [type] [state] [func] [params...]
 * @param  cmd_len  Length field from the frame (value of buf[0]).
 */
void protocol_dispatch(uint8_t *buf, uint8_t cmd_len);

/**
 * @brief  Update NNC6521 waveform output based on current device state.
 * @param  handle_idx  Handle index (0~2).
 */
void protocol_update_current_output(uint8_t handle_idx);

/**
 * @brief  Start NNC6521 waveform output for the active handle.
 */
void protocol_start_waveform(void);

/**
 * @brief  Stop NNC6521 waveform output for the active handle.
 */
void protocol_stop_waveform(void);

/**
 * @brief  Map current percentage to actual current (mA).
 * @param  waveform_id  Waveform ID (1~9).
 * @param  percent      Current percentage (0~100).
 * @return Actual current in mA.
 */
uint16_t protocol_map_percent_to_current(uint8_t waveform_id, uint8_t percent);

/**
 * @brief  Start periodic temperature reporting for the active handle.
 *         Creates a 2-second RT-Thread software timer that sends FUNC_TEMP_REPORT.
 * @param  handle_id  Handle ID (HANDLE_A / HANDLE_B / HANDLE_C).
 */
void protocol_temp_report_start(uint8_t handle_id);

/**
 * @brief  Stop periodic temperature reporting.
 *         Deletes the software timer if active.
 */
void protocol_temp_report_stop(void);

#endif /* __PROTOCOL_ACT_H__ */
