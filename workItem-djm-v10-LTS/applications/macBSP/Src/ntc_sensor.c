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
#include <rtdevice.h>
#include <math.h>

/* ============================================================================
 *  ADC Device Handle (RT-Thread ADC framework)
 * ===========================================================================*/
static rt_adc_device_t s_adc_dev = RT_NULL;

/* ============================================================================
 *  Sensor Instance Data
 * ===========================================================================*/
static ntc_sensor_t s_ntc[NTC_CH_COUNT];
static uint8_t s_ntc_initialized = 0;

/* ADC channel mapping: channel index -> ADC channel number */
static const rt_uint32_t s_adc_channel[NTC_CH_COUNT] = {
    10,     /* NTC_CH_LARGE -> PC0 (ADC1_CH10) */
    11      /* NTC_CH_SMALL -> PC1 (ADC1_CH11) */
};

/* ============================================================================
 *  Internal: Read ADC via RT-Thread device framework
 * ===========================================================================*/

/**
 * @brief  Read a single ADC channel value using RT-Thread ADC API.
 * @param  channel  ADC channel number (e.g., 10 or 11).
 * @return 12-bit ADC value (0~4095), or 0 on error.
 */
static uint16_t adc_read_channel(rt_uint32_t channel)
{
    if (s_adc_dev == RT_NULL) {
        return 0;
    }

    rt_uint32_t value = rt_adc_read(s_adc_dev, channel);
    return (uint16_t)(value & 0xFFFF);
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

/* ============================================================================
 *  NTC Lookup Table (Rnor / nominal resistance, kOhms)
 *  Index 0 = -50 deg.C, index 175 = 125 deg.C, step = 1 deg.C
 *  Source: Weiheng Electronics datasheet for R25=100k, B25/50=3950
 *  Stored in Flash (.rodata) to save RAM.
 * ===========================================================================*/
#if NTC_USE_LOOKUP_TABLE
#define NTC_LUT_TEMP_MIN    (-50)       /* Lowest temperature in table (deg.C) */
#define NTC_LUT_TEMP_MAX    125         /* Highest temperature in table (deg.C) */
#define NTC_LUT_SIZE        (NTC_LUT_TEMP_MAX - NTC_LUT_TEMP_MIN + 1)  /* 176 */

static const float s_ntc_lut_r_kohm[NTC_LUT_SIZE] = {
    /* -50 to -31 (index 0~19) */
    6878.030f, 6410.610f, 5977.290f, 5575.470f, 5202.730f,
    4856.840f, 4535.760f, 4237.610f, 3960.650f, 3703.290f,
    3464.050f, 3241.580f, 3034.620f, 2842.020f, 2662.720f,
    2495.740f, 2340.170f, 2195.180f, 2060.000f, 1933.920f,
    /* -30 to -11 (index 20~39) */
    1816.280f, 1706.470f, 1603.940f, 1508.160f, 1418.660f,
    1335.000f, 1256.750f, 1183.550f, 1115.050f, 1050.910f,
     990.841f,  934.558f,  881.805f,  832.341f,  785.945f,
     742.410f,  701.544f,  663.170f,  627.123f,  593.249f,
    /* -10 to 9 (index 40~59) */
     561.406f,  531.461f,  503.291f,  476.781f,  451.824f,
     428.322f,  406.182f,  385.317f,  365.647f,  347.098f,
     329.600f,  313.086f,  297.498f,  282.777f,  268.871f,
     255.731f,  243.310f,  231.566f,  220.456f,  209.945f,
    /* 10 to 29 (index 60~79) */
     200.000f,  190.577f,  181.655f,  173.204f,  165.194f,
     157.601f,  150.401f,  143.572f,  137.092f,  130.941f,
     125.102f,  119.556f,  114.288f,  109.282f,  104.524f,
     100.000f,   95.697f,   91.603f,   87.708f,   84.000f,
    /* 30 to 49 (index 80~99) */
      80.470f,   77.108f,   73.905f,   70.853f,   67.944f,
      65.171f,   62.526f,   60.002f,   57.595f,   55.297f,
      53.104f,   51.010f,   49.009f,   47.098f,   45.272f,
      43.526f,   41.857f,   40.261f,   38.735f,   37.274f,
    /* 50 to 69 (index 100~119) */
      35.880f,   34.539f,   33.258f,   32.031f,   30.856f,
      29.730f,   28.652f,   27.618f,   26.626f,   25.676f,
      24.764f,   23.890f,   23.051f,   22.245f,   21.472f,
      20.730f,   20.017f,   19.332f,   18.674f,   18.042f,
    /* 70 to 89 (index 120~139) */
      17.434f,   16.850f,   16.289f,   15.749f,   15.229f,
      14.730f,   14.249f,   13.786f,   13.340f,   12.911f,
      12.498f,   12.101f,   11.717f,   11.348f,   10.992f,
      10.649f,   10.319f,   10.000f,    9.693f,    9.396f,
    /* 90 to 109 (index 140~159) */
       9.110f,    8.834f,    8.568f,    8.311f,    8.063f,
       7.824f,    7.592f,    7.369f,    7.153f,    6.945f,
       6.744f,    6.549f,    6.361f,    6.179f,    6.003f,
       5.833f,    5.669f,    5.510f,    5.356f,    5.207f,
    /* 110 to 125 (index 160~175) */
       5.063f,    4.923f,    4.788f,    4.658f,    4.531f,
       4.409f,    4.290f,    4.175f,    4.064f,    3.956f,
       3.851f,    3.750f,    3.651f,    3.556f,    3.464f,
       3.374f
};

/* ============================================================================
 *  Lookup Table Conversion Functions
 * ===========================================================================*/

float ntc_resistance_to_temperature_lut(float resistance)
{
    /* Convert Ohms to kOhms for table lookup */
    float r_kohm = resistance / 1000.0f;

    /* Clamp to table range */
    if (r_kohm >= s_ntc_lut_r_kohm[0]) {
        return (float)NTC_LUT_TEMP_MIN;  /* Resistance too high, below -50C */
    }
    if (r_kohm <= s_ntc_lut_r_kohm[NTC_LUT_SIZE - 1]) {
        return (float)NTC_LUT_TEMP_MAX;  /* Resistance too low, above 125C */
    }

    /*
     * Binary search: table is sorted descending (high R = low temp).
     * Find index i such that lut[i] >= r_kohm > lut[i+1].
     * Then temperature is between (TEMP_MIN + i) and (TEMP_MIN + i + 1).
     */
    int lo = 0;
    int hi = NTC_LUT_SIZE - 1;

    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;
        if (s_ntc_lut_r_kohm[mid] >= r_kohm) {
            lo = mid;
        } else {
            hi = mid;
        }
    }

    /* lo and hi are adjacent; lut[lo] >= r_kohm > lut[hi] */
    float r_lo = s_ntc_lut_r_kohm[lo];
    float r_hi = s_ntc_lut_r_kohm[hi];

    /* Linear interpolation factor (0.0 at lo, 1.0 at hi) */
    float frac = (r_lo - r_kohm) / (r_lo - r_hi);

    float temp = (float)(NTC_LUT_TEMP_MIN + lo) + frac;
    return temp;
}

float ntc_adc_to_temperature_lut(uint16_t adc_value)
{
    float resistance = ntc_adc_to_resistance(adc_value);
    return ntc_resistance_to_temperature_lut(resistance);
}
#endif /* NTC_USE_LOOKUP_TABLE */

float ntc_adc_to_temperature(uint16_t adc_value)
{
#if NTC_USE_LOOKUP_TABLE
    return ntc_adc_to_temperature_lut(adc_value);
#else
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
#endif
}

/* ============================================================================
 *  Public API
 * ===========================================================================*/

int ntc_sensor_init(void)
{
    /* Find ADC device via RT-Thread framework */
    s_adc_dev = (rt_adc_device_t)rt_device_find("adc1");
    if (s_adc_dev == RT_NULL) {
        rt_kprintf("[NTC] ERROR: ADC device 'adc1' not found!\n");
        return -RT_ERROR;
    }

    /* Enable ADC channels */
    for (uint8_t i = 0; i < NTC_CH_COUNT; i++) {
        if (rt_adc_enable(s_adc_dev, s_adc_channel[i]) != RT_EOK) {
            rt_kprintf("[NTC] ERROR: Failed to enable ADC channel %d\n", s_adc_channel[i]);
            return -RT_ERROR;
        }
    }

    /* Initialize sensor data */
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

    rt_kprintf("[NTC] Sensor module initialized via RT-Thread ADC, channels=%d\n", NTC_CH_COUNT);
    s_ntc_initialized = 1;
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
    if (!s_ntc_initialized) return;
    // static uint8_t dbg_cnt = 0;  /* disabled - use VOFA+ for monitoring */
    for (uint8_t ch = 0; ch < NTC_CH_COUNT; ch++) {
        /* Read raw ADC */
        uint16_t raw = ntc_sensor_read_adc(ch);

        /* Apply sliding average filter */
        uint16_t filtered = filter_push(&s_ntc[ch], raw);

        /* Convert to temperature */
        s_ntc[ch].temperature = ntc_adc_to_temperature(filtered);
        s_ntc[ch].resistance = ntc_adc_to_resistance(filtered);
    }
    /* Debug: print raw ADC values every ~2s (disabled - use VOFA+ for monitoring) */
    // if (++dbg_cnt >= 200) {
    //     dbg_cnt = 0;
    //     rt_kprintf("[NTC] ch0(PC0) raw=%u temp=%.1f | ch1(PC1) raw=%u temp=%.1f\n",
    //                s_ntc[0].adc_raw, s_ntc[0].temperature,
    //                s_ntc[1].adc_raw, s_ntc[1].temperature);
    // }
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
