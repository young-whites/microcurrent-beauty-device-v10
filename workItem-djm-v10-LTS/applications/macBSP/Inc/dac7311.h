/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-07-14     Administrator       DAC driver for pump speed control
 *
 * DAC: 14-bit, single-channel, SPI DAC
 * - SPI Mode 0 (CPOL=0, CPHA=0), MSB first
 * - 16-bit frame format:
 *
 *   Bit: 15 14 │ 13 12 11 10 9 8 7 6 5 4 3 2 │ 1 0
 *        ──────┼──────────────────────────────┼─────
 *        M1 M0 │ D13 D12 D11 ... D1 D0        │ R R
 *        ──────┼──────────────────────────────┼─────
 *        模式   │ 14-bit 有效数据 (value<<2)    │ 保留
 *
 * - M1:M0 (D15:D14) = mode control (00=normal)
 * - D13:D0 (D13:D2) = 14-bit data, value left-shifted by 2
 * - R:R (D1:D0)     = reserved (written as 0)
 * - VOUT = VREF * D / 16384 (VREF = VDD = 5V)
 * - Output range: 0 ~ 5V
 *
 * Hardware connections:
 *   PB7  -> SYNC (chip select, active low)
 *   PB8  -> SCLK (serial clock)
 *   PB9  -> DIN  (serial data input)
 */
#ifndef APPLICATIONS_MACBSP_INC_DAC7311_H_
#define APPLICATIONS_MACBSP_INC_DAC7311_H_

#include <rtthread.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 *  DAC Hardware Configuration
 * ===========================================================================*/
#define DAC7311_VREF            5.0f        /* Reference voltage (V) = VDD */
#define DAC7311_RESOLUTION      16384       /* 14-bit: 0 ~ 16383 (0x3FFF) */
#define DAC7311_VOUT_MAX        DAC7311_VREF /* Maximum output voltage */

/* Mode control (16-bit frame bits [15:14]) */
#define DAC7311_PD_NORMAL       0x00        /* Normal operation */
#define DAC7311_PD_1K           0x01        /* Power-down with 1k to GND */
#define DAC7311_PD_100K         0x02        /* Power-down with 100k to GND */
#define DAC7311_PD_HIZ          0x03        /* Power-down, high impedance */

/*
 * DAC 16-bit frame format:
 *   D15 D14 | D13 D12 D11 ... D1 D0 | (bit1:0)
 *   [MODE]  | [14-bit data << 2]    | [RSVD=0]
 *
 *   value (0~16383) is left-shifted by 2 to occupy D13:D2
 *   frame = (mode << 14) | (value << 2)
 */

/* ============================================================================
 *  Pump Speed Control (non-linear voltage mapping)
 * ===========================================================================*/
/*
 * Pump voltage zones:
 *   0.0V ~ 0.5V  : Dead zone   (pump does not rotate)
 *   0.6V ~ 4.5V  : Control zone (linear speed regulation)
 *   4.6V ~ 5.0V  : Saturation   (full load, constant max speed)
 *
 * Percentage mapping (upper machine 0~100%):
 *   0%       -> 0.0V (off)
 *   1%~90%   -> 0.6V ~ 4.5V  (control zone, linear)
 *   91%~100% -> 4.6V ~ 5.0V  (saturation zone, linear)
 */
#define PUMP_DEAD_ZONE_MAX_V    0.5f    /* Dead zone upper limit (V) */
#define PUMP_CTRL_MIN_V         0.6f    /* Control zone lower limit (V) */
#define PUMP_CTRL_MAX_V         4.5f    /* Control zone upper limit (V) */
#define PUMP_SAT_MIN_V          4.6f    /* Saturation zone lower limit (V) */
#define PUMP_SAT_MAX_V          5.0f    /* Saturation zone upper limit (V) */
#define PUMP_CTRL_MAX_PCT       90      /* Control zone upper percentage */

/* ============================================================================
 *  Public Functions
 * ===========================================================================*/

/**
 * @brief  Initialize DAC GPIO pins (PB7=SYNC, PB8=SCLK, PB9=DIN).
 *         Sets output to 0V.
 */
void dac7311_init(void);

/**
 * @brief  Set DAC output voltage.
 * @param  voltage  Desired output voltage (0.0 ~ DAC7311_VOUT_MAX V).
 *                  Clamped to valid range.
 */
void dac7311_set_voltage(float voltage);

/**
 * @brief  Set DAC output directly by 14-bit raw value.
 * @param  value  14-bit DAC value (0 ~ 16383).
 */
void dac7311_set_raw(uint16_t value);

/**
 * @brief  Set DAC output as percentage (0~100%).
 *         Maps 0~100% to 0~VREF voltage.
 * @param  percent  Output percentage (0 ~ 100).
 */
void dac7311_set_percent(uint8_t percent);

/**
 * @brief  Set power-down mode.
 * @param  mode  Power-down mode (DAC7311_PD_xxx).
 */
void dac7311_power_down(uint8_t mode);

/**
 * @brief  Get current DAC output voltage.
 * @return Current voltage setting (V).
 */
float dac7311_get_voltage(void);

/**
 * @brief  Write raw 16-bit frame directly to DAC.
 *         Frame format: [M1 M0 D13 D12 ... D0 R R]
 * @param  frame  16-bit frame to write.
 */
void dac7311_write_raw_frame(uint16_t frame);

/**
 * @brief  Set SPI clock delay (controls SCLK high/low time).
 * @param  delay_us  Delay in microseconds. 0 = fast mode (~140ns).
 */
void dac7311_set_delay(uint32_t delay_us);

/**
 * @brief  Get current SPI clock delay.
 * @return Delay in microseconds (0 = fast mode).
 */
uint32_t dac7311_get_delay(void);

/**
 * @brief  Set pump speed by percentage with non-linear voltage mapping.
 *         Handles dead zone, control zone, and saturation zone.
 * @param  percent  Pump speed (0 = off, 1~100 = speed level).
 */
void dac7311_set_pump_speed(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_MACBSP_INC_DAC7311_H_ */
