/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06-29     auto-gen     NTC temperature sensor driver for DJM-V10
 */

#ifndef APPLICATIONS_MACBSP_INC_NTC_SENSOR_H_
#define APPLICATIONS_MACBSP_INC_NTC_SENSOR_H_

#include <rtthread.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 *  NTC Sensor Parameters
 * ===========================================================================*/
#define NTC_R0              100000.0f   /* Nominal resistance at T0 (Ohms) */
#define NTC_T0              298.15f     /* Nominal temperature T0 (25C in Kelvin) */
#define NTC_B               3950.0f     /* B-value (25/50 deg.C) */
#define NTC_RDIV            100000.0f   /* Series resistor in voltage divider (Ohms) */
#define NTC_VREF            3.3f        /* ADC reference voltage (V) */
#define NTC_ADC_RESOLUTION  4096        /* 12-bit ADC */

/* ============================================================================
 *  Filter Configuration
 * ===========================================================================*/
#define NTC_FILTER_SIZE     8           /* Sliding average window size (power of 2) */
#define NTC_FILTER_SHIFT    3           /* log2(NTC_FILTER_SIZE) for fast division */

/* ============================================================================
 *  Sensor Channel Definition
 * ===========================================================================*/
#define NTC_CH_LARGE        0           /* Large handle NTC (ADC1_CH10, PC0) */
#define NTC_CH_SMALL        1           /* Small handle NTC (ADC1_CH11, PC1) */
#define NTC_CH_COUNT        2           /* Total NTC channels */

/* ============================================================================
 *  NTC Sensor Data Structure
 * ===========================================================================*/
typedef struct {
    uint16_t adc_raw;                       /* Latest raw ADC value (0~4095) */
    uint16_t adc_buf[NTC_FILTER_SIZE];      /* Sliding average buffer */
    uint8_t  buf_idx;                       /* Current buffer write index */
    uint8_t  buf_full;                      /* Buffer fill flag */
    float    temperature;                   /* Current filtered temperature (Celsius) */
    float    resistance;                    /* Current NTC resistance (Ohms) */
} ntc_sensor_t;

/* ============================================================================
 *  Public Functions
 * ===========================================================================*/

/**
 * @brief  Initialize NTC sensor module.
 *         Configures ADC and clears filter buffers.
 * @return RT_EOK on success.
 */
int ntc_sensor_init(void);

/**
 * @brief  Read ADC value for a specific NTC channel and update filter.
 * @param  channel  NTC_CH_LARGE or NTC_CH_SMALL.
 * @return Raw ADC value (0~4095).
 */
uint16_t ntc_sensor_read_adc(uint8_t channel);

/**
 * @brief  Convert ADC value to temperature using B-parameter equation.
 * @param  adc_value  12-bit ADC value (0~4095).
 * @return Temperature in Celsius.
 */
float ntc_adc_to_temperature(uint16_t adc_value);

/**
 * @brief  Convert ADC value to NTC resistance.
 * @param  adc_value  12-bit ADC value (0~4095).
 * @return NTC resistance in Ohms.
 */
float ntc_adc_to_resistance(uint16_t adc_value);

/**
 * @brief  Get the filtered temperature for a channel.
 *         Call ntc_sensor_update() first to refresh readings.
 * @param  channel  NTC_CH_LARGE or NTC_CH_SMALL.
 * @return Filtered temperature in Celsius.
 */
float ntc_sensor_get_temperature(uint8_t channel);

/**
 * @brief  Update all NTC sensor readings (call periodically from timer).
 *         Reads ADC, applies sliding average filter, converts to temperature.
 */
void ntc_sensor_update(void);

/**
 * @brief  Get the sensor data structure for a channel.
 * @param  channel  NTC_CH_LARGE or NTC_CH_SMALL.
 * @return Pointer to ntc_sensor_t structure.
 */
ntc_sensor_t *ntc_sensor_get(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_MACBSP_INC_NTC_SENSOR_H_ */
