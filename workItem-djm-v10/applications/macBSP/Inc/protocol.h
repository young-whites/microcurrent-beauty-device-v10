/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06-16     auto-gen     protocol header for DJM-V10 microcurrent beauty device
 */

#ifndef APPLICATIONS_MACBSP_INC_PROTOCOL_H_
#define APPLICATIONS_MACBSP_INC_PROTOCOL_H_

#include <rtthread.h>
#include <rtdevice.h>
#include <stdint.h>
#include "bsp_sys.h"
#include "nnc6521.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 *  Device Identity
 * ===========================================================================*/
#define DEVICE_ID_H         0x60        /* Device address high byte */
#define DEVICE_ID_L         0x66        /* Device address low byte */
#define DEVICE_VERSION_H    0x01        /* Firmware major version */
#define DEVICE_VERSION_L    0x00        /* Firmware minor version */

/* ============================================================================
 *  Frame Protocol Constants
 * ===========================================================================*/
#define FRAME_HEAD1         0x55        /* Frame header byte 1 */
#define FRAME_HEAD2         0xAA        /* Frame header byte 2 */
#define FRAME_TYPE_ACT      0x44        /* Frame type: action command */
#define FRAME_TYPE_GET      0x45        /* Frame type: parameter query */
#define FRAME_STATE_ASK     0x02        /* Frame state: host request */
#define FRAME_STATE_ACK     0x01        /* Frame state: device response */

/* ============================================================================
 *  Functional Code Definitions
 * ===========================================================================*/
#define FUNC_HANDLE_SWITCH  0x01        /* Handle switching (A/B/C) */
#define FUNC_CURRENT_CTRL   0x02        /* Current control (0~100%) */
#define FUNC_TEMP_CTRL      0x03        /* Temperature adjustment (0~41C) */
#define FUNC_PUMP_CTRL      0x04        /* Pump speed control (0~100%) */
#define FUNC_START_PAUSE    0x05        /* Start/Pause treatment */
#define FUNC_OTA_UPGRADE    0x06        /* Remote firmware upgrade (reserved) */
#define FUNC_AGING_MODE     0x07        /* Factory aging test mode */
#define FUNC_READ_VERSION   0x08        /* Read firmware version */
#define FUNC_WAVEFORM_SEL   0x09        /* Waveform selection (1~9) */

/* ============================================================================
 *  Handle ID Definitions
 * ===========================================================================*/
#define HANDLE_A            0x0A        /* Handle A: microcurrent + heating */
#define HANDLE_B            0x0B        /* Handle B: microcurrent + heating */
#define HANDLE_C            0x0C        /* Handle C: microcurrent + pump */

/* ============================================================================
 *  Error Code Definitions
 * ===========================================================================*/
#define ERR_OK              0x00        /* Success */
#define ERR_PARAM           0x01        /* Parameter error */
#define ERR_UNSUPPORTED     0x02        /* Unsupported function code */
#define ERR_BUSY            0x03        /* Device busy */
#define ERR_UNKNOWN         0xFF        /* Unknown error */

/* ============================================================================
 *  Waveform Definitions
 * ===========================================================================*/
#define WAVEFORM_POWER_SMOOTH       1   /* 30~80mA, 50Hz, 300us, symm. square */
#define WAVEFORM_BURST_TRAIN        2   /* 30~80mA, 50Hz burst 10Hz, 300us */
#define WAVEFORM_GENTLE_SMOOTH      3   /* 20~60mA, 35Hz, 300us, symm. square */
#define WAVEFORM_DEEP_SCULPT        4   /* 30~80mA, 40~50Hz, 4kHz balanced sq */
#define WAVEFORM_SOFT_SCULPT        5   /* 30~80mA, 30~40Hz, 2~4kHz sine */
#define WAVEFORM_CIRCUL_SCULPT      6   /* 20~60mA, 2~10Hz, 4kHz bal. sine */
#define WAVEFORM_SMOOTH_FIRM        7   /* 15~50mA, 80~100Hz, 400us, tri */
#define WAVEFORM_LYMPH_DRAIN        8   /* 15~40mA, 5Hz, 450us, low-freq sine */
#define WAVEFORM_SOOTHING_END       9   /* 10~30mA, 10Hz, low-freq sine */

/* ============================================================================
 *  Protocol Buffer Configuration
 * ===========================================================================*/
#define PROTO_RX_BUF_SIZE   512        /* UART receive circular queue size */
#define PROTO_TX_BUF_SIZE   64         /* UART transmit buffer size */
#define PROTO_MAX_FRAME_LEN 64         /* Maximum single frame length */
#define PROTO_CMD_BUF_SIZE  32         /* Command decode buffer size */

/* ============================================================================
 *  UART Configuration
 * ===========================================================================*/
#define PROTO_UART_NAME     "uart1"    /* UART device name */
#define PROTO_UART_BAUD     9600       /* UART baud rate */

/* ============================================================================
 *  Decode State Machine States
 * ===========================================================================*/
typedef enum {
    DECODE_IDLE = 0,            /* Waiting for frame header 1 (0x55) */
    DECODE_HEAD2,               /* Waiting for frame header 2 (0xAA) */
    DECODE_LENGTH,              /* Receiving length byte */
    DECODE_ADDR_H,              /* Receiving device address high byte */
    DECODE_ADDR_L,              /* Receiving device address low byte */
    DECODE_PAYLOAD,             /* Receiving type + state + func + params */
    DECODE_CRC_H,               /* Receiving CRC high byte */
    DECODE_CRC_L                /* Receiving CRC low byte */
} decode_state_t;

/* ============================================================================
 *  Handle Parameter Structure (per-handle state)
 * ===========================================================================*/
typedef struct {
    uint8_t current_percent;    /* Current output percentage (0~100) */
    uint8_t temperature;        /* Target temperature (0~41C) */
    uint8_t pump_speed;         /* Pump speed (0~100%) */
} handle_params_t;

/* ============================================================================
 *  Device Global State Structure
 * ===========================================================================*/
typedef struct {
    uint8_t current_handle;             /* Current active handle (HANDLE_A/B/C) */
    uint8_t waveform_id;                /* Current waveform selection (1~9) */
    uint8_t is_running;                 /* Treatment running flag (0=pause, 1=running) */
    uint8_t aging_mode;                 /* Factory aging mode flag */
    handle_params_t handle[3];          /* Parameters for handle A/B/C (index 0/1/2) */
} device_state_t;

/* ============================================================================
 *  Circular Queue Structure for UART RX
 * ===========================================================================*/
typedef struct {
    uint8_t buffer[PROTO_RX_BUF_SIZE];  /* Data buffer */
    volatile uint16_t head;             /* Read index */
    volatile uint16_t tail;             /* Write index */
    rt_mutex_t lock;                    /* Mutex for thread-safe access */
} ring_buffer_t;

/* ============================================================================
 *  Public Variables
 * ===========================================================================*/
extern device_state_t g_dev_state;     /* Global device state */
extern rt_device_t g_serial1;          /* UART1 device handle */

/* ============================================================================
 *  Public Function Declarations
 * ===========================================================================*/

/**
 * @brief  Initialize UART1 protocol module (hardware + thread + semaphore).
 * @return RT_EOK on success, negative error code on failure.
 */
int protocol_init(void);

/**
 * @brief  CRC16-MODBUS calculation.
 * @param  data  Pointer to data buffer.
 * @param  len   Number of bytes to calculate.
 * @return CRC16 value (Big-Endian in frame).
 */
uint16_t protocol_crc16_modbus(const uint8_t *data, uint8_t len);

/**
 * @brief  Build and send an ACK response frame.
 * @param  func      Function code.
 * @param  params    Response parameter buffer (may be NULL).
 * @param  param_len Number of parameter bytes.
 */
void protocol_send_ack(uint8_t func, const uint8_t *params, uint8_t param_len);

/**
 * @brief  Build and send an error response frame.
 * @param  func      Function code that caused the error.
 * @param  err_code  Error code (ERR_PARAM / ERR_UNSUPPORTED / etc.).
 */
void protocol_send_error(uint8_t func, uint8_t err_code);

/**
 * @brief  Get handle index (0/1/2) from handle ID byte (0x0A/0x0B/0x0C).
 * @param  handle_id  Handle ID byte.
 * @return Index 0~2 on valid, -1 on invalid.
 */
int protocol_handle_index(uint8_t handle_id);

/**
 * @brief  Update NNC6521 waveform output based on current device state.
 *         Maps current_percent to the active waveform's current range.
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

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_MACBSP_INC_PROTOCOL_H_ */
