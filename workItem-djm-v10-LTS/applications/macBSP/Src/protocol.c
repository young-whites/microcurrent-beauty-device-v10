/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06-16     auto-gen     protocol.c for DJM-V10 microcurrent beauty device
 *                              Protocol driver: UART, CRC, frame TX/RX, decode state machine
 * 2026-07-09     refactor     Split command handlers into protocol_act.c
 */

#include "protocol.h"
#include "protocol_act.h"
#include "bsp_hard.h"
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



#define RT_UART2_SERIAL_CONFIG_DEFAULT           \
{                                          \
    BAUD_RATE_9600, /* 115200 bits/s */  \
    DATA_BITS_8,      /* 8 databits */     \
    STOP_BITS_1,      /* 1 stopbit */      \
    PARITY_NONE,      /* No parity  */     \
    BIT_ORDER_LSB,    /* LSB first sent */ \
    NRZ_NORMAL,       /* Normal mode */    \
    RT_SERIAL_RB_BUFSZ, /* Buffer size */  \
    RT_SERIAL_FLOWCONTROL_NONE, /* Off flowcontrol */ \
    0                                      \
}




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

    /* Debug log - disabled to keep VOFA+ output clean */
    /* rt_kprintf("[PROTO] TX: ");
    for (uint8_t i = 0; i < total; i++) {
        rt_kprintf("%02X ", tx_buf[i]);
    }
    rt_kprintf("\n"); */
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
                    protocol_dispatch(s_cmd_buf, s_cmd_len);
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
    int rx_count = 0;

    rt_kprintf("[PROTO] Decode thread started\n");

    while (1) {
        /* Wait for data available (semaphore released by RX callback) */
        rt_sem_take(s_rx_sem, RT_WAITING_FOREVER);

        /* Read all available bytes from UART */
        while (rt_device_read(g_serial1, RT_NULL, &rx_byte, 1) == 1) {
            rx_count++;
            // rt_kprintf("[PROTO] RX[%d]: 0x%02X\n", rx_count, rx_byte);

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
    struct serial_configure uart_cfg = RT_UART2_SERIAL_CONFIG_DEFAULT;

    /* Find UART device (already registered by RT-Thread driver) */
    g_serial1 = rt_device_find(PROTO_UART_NAME);
    if (g_serial1 == RT_NULL) {
        rt_kprintf("[PROTO] UART device '%s' not found!\n", PROTO_UART_NAME);
        return -RT_ERROR;
    }

    /* Reconfigure UART to protocol baud rate (CubeMX default may differ) */
    uart_cfg.baud_rate = PROTO_UART_BAUD;  /* 9600 */
    uart_cfg.data_bits = DATA_BITS_8;
    uart_cfg.stop_bits = STOP_BITS_1;
    uart_cfg.parity    = PARITY_NONE;
    uart_cfg.bufsz     = 1024;

    rt_err_t cfg_ret = rt_device_control(g_serial1, RT_DEVICE_CTRL_CONFIG, &uart_cfg);
    if (cfg_ret != RT_EOK) {
        rt_kprintf("[PROTO] UART config failed: %d\n", cfg_ret);
    }

    /* Open device in read-write + interrupt RX mode */
    rt_err_t ret = rt_device_open(g_serial1,
                                  RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX);
    if (ret != RT_EOK) {
        rt_kprintf("[PROTO] Failed to open UART: %d\n", ret);
        return ret;
    }

    /* Set RX indication callback */
    rt_device_set_rx_indicate(g_serial1, uart_rx_callback);

    rt_kprintf("[PROTO] UART1 opened OK (%d bps, 8N1), handle=0x%p\n", PROTO_UART_BAUD, g_serial1);
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
