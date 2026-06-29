/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06-29     auto-gen     NTC temperature sensor driver for DJM-V10
 *
 * NTC Specifications:
 *   - R0 = 100k Ohms @ 25 deg.C
 *   - B  = 3950K (25/50 deg.C)
 *   - Tolerance: R +/-1%, B +/-1%
 *
 * Hardware:
 *   - ADC1_CH10 (PC0): Large handle NTC
 *   - ADC1_CH11 (PC1): Small handle NTC
 *   - ADC clock: 12 MHz, 12-bit resolution
 *   - Voltage divider: VCC -- R_series(100k) -- ADC_pin -- NTC(100k) -- GND
 */

#include "ntc_sensor.h"
#include "bsp_sys.h"
#include <math.h>

/* ============================================================================
 *  ADC Handle (extern from CubeMX generated code)
 * ===========================================================================*/
extern ADC_HandleTypeDef hadc1;

/* ============================================================================
 *  Sensor Instance Data
 * ===========================================================================*/
static ntc_sensor_t s_ntc[NTC_CH_COUNT];

/* ADC channel mapping: channel index -> ADC channel number */
static const uint32_t s_adc_channel[NTC_CH_COUNT] = {
    ADC_CHANNEL_10,     /* NTC_CH_LARGE -> PC0 */
    ADC_CHANNEL_11      /* NTC_CH_SMALL -> PC1 */
};

/* ============================================================================
 *  Internal: Read ADC with channel switching
 * ===========================================================================*/

/**
 * @brief  Read a single ADC channel value.
 *         Configures ADC channel, starts conversion, waits for result.
 * @param  adc_channel  ADC channel number (e.g., ADC_CHANNEL_10).
 * @return 12-bit ADC value (0~4095).
 */
static uint16_t adc_read_channel(uint32_t adc_channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    sConfig.Channel = adc_channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;  /* ~20us sampling for stability */

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return 0;
    }

    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        return (uint16_t)HAL_ADC_GetValue(&hadc1);
    }

    return 0;
}

/* ============================================================================
 *  Internal: Sliding Average Filter
 * ===========================================================================*/

/**
 * @brief  Push a new ADC value into the sliding average buffer.
 * @param  sensor  Pointer to sensor instance.
 * @param  value   New ADC value.
 * @return Filtered (averaged) ADC value.
 */
static uint16_t filter_push(ntc_sensor_t *sensor, uint16_t value)
{
    sensor->adc_buf[sensor->buf_idx] = value;
    sensor->buf_idx = (sensor->buf_idx + 1) & (NTC_FILTER_SIZE - 1);

    if (sensor->buf_idx == 0) {
        sensor->buf_full = 1;
    }

    /* Calculate average */
    uint32_t sum = 0;
    uint8_t count = sensor->buf_full ? NTC_FILTER_SIZE : sensor->buf_idx;
    if (count == 0) count = 1;

    for (uint8_t i = 0; i < count; i++) {
        sum += sensor->adc_buf[i];
    }

    return (uint16_t)(sum >> NTC_FILTER_SHIFT);
}

/* ============================================================================
 *  NTC Conversion Functions
 * ===========================================================================*/

float ntc_adc_to_resistance(uint16_t adc_value)
{
    /*
     * Voltage divider circuit:
     *   VCC(3.3V) --- R_series(100k) ---+--- NTC(100k) --- GND
     *                                    |
     *                                  ADC pin
     *
     * V_adc = VCC * R_ntc / (R_series + R_ntc)
     * => R_ntc = R_series * V_adc / (VCC - V_adc)
     * => R_ntc = R_series * adc_value / (ADC_RESOLUTION - adc_value)
     *
     * For typical NTC at 25C (R_ntc = 100k, R_series = 100k):
     *   adc_value = 4096 * 100k / (100k + 100k) = 2048
     *   R_ntc = 100k * 2048 / (4096 - 2048) = 100k  (correct)
     */

    if (adc_value == 0) {
        return NTC_RDIV * 100.0f;  /* Open circuit protection: return very high R */
    }
    if (adc_value >= NTC_ADC_RESOLUTION) {
        return 0.001f;             /* Short circuit protection: return very low R */
    }

    float r_ntc = NTC_RDIV * (float)adc_value / (float)(NTC_ADC_RESOLUTION - adc_value);
    return r_ntc;
}

float ntc_adc_to_temperature(uint16_t adc_value)
{
    /*
     * B-parameter equation (simplified Steinhart-Hart):
     *
     *   1/T = 1/T0 + (1/B) * ln(R/R0)
     *
     * Where:
     *   T  = temperature in Kelvin
     *   T0 = 298.15K (25 deg.C)
     *   R0 = 100k Ohms (resistance at T0)
     *   B  = 3950K
     *   R  = measured NTC resistance
     *
     * Convert to Celsius: T_celsius = T - 273.15
     */

    float r_ntc = ntc_adc_to_resistance(adc_value);

    if (r_ntc <= 0.0f) {
        return -99.9f;  /* Error: invalid resistance */
    }

    float ln_ratio = logf(r_ntc / NTC_R0);
    float inv_T = (1.0f / NTC_T0) + (1.0f / NTC_B) * ln_ratio;

    if (inv_T <= 0.0f) {
        return -99.9f;  /* Error: invalid calculation */
    }

    float temp_k = 1.0f / inv_T;
    float temp_c = temp_k - 273.15f;

    /* Clamp to reasonable range */
    if (temp_c < -40.0f) temp_c = -40.0f;
    if (temp_c > 350.0f) temp_c = 350.0f;

    return temp_c;
}

/* ============================================================================
 *  Public API
 * ===========================================================================*/

int ntc_sensor_init(void)
{
    for (uint8_t i = 0; i < NTC_CH_COUNT; i++) {
        s_ntc[i].adc_raw = 0;
        s_ntc[i].buf_idx = 0;
        s_ntc[i].buf_full = 0;
        s_ntc[i].temperature = 25.0f;   /* Default to room temperature */
        s_ntc[i].resistance = NTC_R0;

        for (uint8_t j = 0; j < NTC_FILTER_SIZE; j++) {
            s_ntc[i].adc_buf[j] = 2048; /* Default to mid-range (25C) */
        }
    }

    rt_kprintf("[NTC] Sensor module initialized, channels=%d\n", NTC_CH_COUNT);
    return RT_EOK;
}

uint16_t ntc_sensor_read_adc(uint8_t channel)
{
    if (channel >= NTC_CH_COUNT) return 0;

    uint16_t raw = adc_read_channel(s_adc_channel[channel]);
    s_ntc[channel].adc_raw = raw;
    return raw;
}

void ntc_sensor_update(void)
{
    for (uint8_t ch = 0; ch < NTC_CH_COUNT; ch++) {
        /* Read raw ADC */
        uint16_t raw = ntc_sensor_read_adc(ch);

        /* Apply sliding average filter */
        uint16_t filtered = filter_push(&s_ntc[ch], raw);

        /* Convert to temperature */
        s_ntc[ch].temperature = ntc_adc_to_temperature(filtered);
        s_ntc[ch].resistance = ntc_adc_to_resistance(filtered);
    }
}

float ntc_sensor_get_temperature(uint8_t channel)
{
    if (channel >= NTC_CH_COUNT) return 25.0f;
    return s_ntc[channel].temperature;
}

ntc_sensor_t *ntc_sensor_get(uint8_t channel)
{
    if (channel >= NTC_CH_COUNT) return RT_NULL;
    return &s_ntc[channel];
}
