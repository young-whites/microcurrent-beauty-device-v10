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
 * - 16-bit frame: [M1 M0 D13 D12 ... D1 D0 R R]
 * - VOUT = VREF * D / 16384 (VREF = VDD)
 *
 * Hardware:
 *   PB7  -> SYNC (chip select, active low)
 *   PB8  -> SCLK (serial clock)
 *   PB9  -> DIN  (serial data input)
 */

#include "dac7311.h"
#include "main.h"

/* ============================================================================
 *  GPIO Pin Definitions (direct BSRR/BRR register access for speed)
 * ===========================================================================*/
#define DAC_GPIO_PORT       GPIOB
#define DAC_SYNC_PIN        GPIO_PIN_7
#define DAC_SCLK_PIN        GPIO_PIN_8
#define DAC_DIN_PIN         GPIO_PIN_9

/* ============================================================================
 *  Internal State
 * ===========================================================================*/
static float s_dac_voltage = 0.0f;  /* Current output voltage */
static uint16_t s_dac_raw = 0;      /* Current raw 14-bit value */

/* ============================================================================
 *  Internal: GPIO Bit-Bang SPI
 * ===========================================================================*/

/**
 * @brief  Write 16-bit frame to DAC via software SPI.
 *         Frame format: [M1 M0 D13 D12 ... D1 D0 R R]
 *         SPI Mode 0: CPOL=0, CPHA=0, MSB first.
 *         Data latched on SCLK rising edge by DAC.
 *
 * @param  data  16-bit frame data.
 */
static void dac7311_write_frame(uint16_t data)
{
    GPIO_TypeDef *port = DAC_GPIO_PORT;

    /* Pull SYNC low to start transaction */
    port->BRR = DAC_SYNC_PIN;
    rt_hw_us_delay(1);  /* SYNC setup time */

    /* Clock out 16 bits, MSB first */
    for (int8_t bit = 15; bit >= 0; bit--) {
        /* Set DIN before SCLK rising edge */
        if (data & (1 << bit)) {
            port->BSRR = DAC_DIN_PIN;    /* DIN = HIGH */
        } else {
            port->BRR  = DAC_DIN_PIN;    /* DIN = LOW  */
        }

        rt_hw_us_delay(1);  /* Data setup time (~1us) */

        /* SCLK rising edge (DAC latches data here) */
        port->BSRR = DAC_SCLK_PIN;
        rt_hw_us_delay(1);  /* Clock high time (~1us) */

        /* SCLK falling edge */
        port->BRR = DAC_SCLK_PIN;
        rt_hw_us_delay(1);  /* Clock low time (~1us) */
    }

    /* Pull SYNC high to latch frame */
    rt_hw_us_delay(1);
    port->BSRR = DAC_SYNC_PIN;
}

/* ============================================================================
 *  Public API
 * ===========================================================================*/

void dac7311_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* Enable GPIOB clock */
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Configure SYNC (PB7), SCLK (PB8), DIN (PB9) as push-pull outputs */
    gpio.Pin   = DAC_SYNC_PIN | DAC_SCLK_PIN | DAC_DIN_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* Set idle state: SYNC=HIGH, SCLK=LOW, DIN=LOW */
    DAC_GPIO_PORT->BSRR = DAC_SYNC_PIN;       /* SYNC = HIGH */
    DAC_GPIO_PORT->BRR  = DAC_SCLK_PIN;       /* SCLK = LOW  */
    DAC_GPIO_PORT->BRR  = DAC_DIN_PIN;        /* DIN  = LOW  */

    /* Output 0V */
    dac7311_set_voltage(0.0f);

    rt_kprintf("[DAC7311] Initialized, output=0.00V\n");
}

void dac7311_set_voltage(float voltage)
{
    /* Clamp to valid range */
    if (voltage < 0.0f) voltage = 0.0f;
    if (voltage > DAC7311_VOUT_MAX) voltage = DAC7311_VOUT_MAX;

    /* Convert voltage to 14-bit value: D = V * 16384 / VREF */
    uint16_t value = (uint16_t)((voltage / DAC7311_VREF) * (DAC7311_RESOLUTION - 1) + 0.5f);
    if (value > 16383) value = 16383;

    /* Build 16-bit frame: [MODE(2) DATA(14) RSVD(2)]
     * D15:D14 = mode, D13:D0 = value<<2, D1:D0 = 0 */
    uint16_t frame = (DAC7311_PD_NORMAL << 14) | (value << 2);

    /* Write to DAC */
    dac7311_write_frame(frame);

    /* Update state */
    s_dac_raw = value;
    s_dac_voltage = voltage;

    rt_kprintf("[DAC7311] set %.3fV -> raw=%d frame=0x%04X\n", voltage, value, frame);
}

void dac7311_set_raw(uint16_t value)
{
    if (value > 16383) value = 16383;

    uint16_t frame = (DAC7311_PD_NORMAL << 14) | (value << 2);
    dac7311_write_frame(frame);

    s_dac_raw = value;
    s_dac_voltage = (float)value * DAC7311_VREF / (DAC7311_RESOLUTION - 1);
}

void dac7311_set_percent(uint8_t percent)
{
    if (percent > 100) percent = 100;

    float voltage = DAC7311_VREF * (float)percent / 100.0f;
    dac7311_set_voltage(voltage);
}

void dac7311_power_down(uint8_t mode)
{
    /* Power-down frame: [MODE(2) 0...0 RSVD(2)] */
    uint16_t frame = ((uint16_t)(mode & 0x03) << 14);
    dac7311_write_frame(frame);

    s_dac_raw = 0;
    s_dac_voltage = 0.0f;

    rt_kprintf("[DAC7311] Power-down mode=%d\n", mode);
}

float dac7311_get_voltage(void)
{
    return s_dac_voltage;
}

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
void dac7311_set_pump_speed(uint8_t percent)
{
    float voltage;

    if (percent == 0) {
        /* Off: output 0V */
        voltage = 0.0f;
    } else if (percent <= PUMP_CTRL_MAX_PCT) {
        /*
         * Control zone: 1%~90% -> 0.6V~4.5V
         * Linear interpolation:
         *   V = PUMP_CTRL_MIN_V + (pct-1) / (PUMP_CTRL_MAX_PCT-1) * (PUMP_CTRL_MAX_V - PUMP_CTRL_MIN_V)
         */
        float ratio = (float)(percent - 1) / (float)(PUMP_CTRL_MAX_PCT - 1);
        voltage = PUMP_CTRL_MIN_V + ratio * (PUMP_CTRL_MAX_V - PUMP_CTRL_MIN_V);
    } else {
        /*
         * Saturation zone: 91%~100% -> 4.6V~5.0V
         * Linear interpolation:
         *   V = PUMP_SAT_MIN_V + (pct-91) / (100-91) * (PUMP_SAT_MAX_V - PUMP_SAT_MIN_V)
         */
        float ratio = (float)(percent - PUMP_CTRL_MAX_PCT - 1) / (float)(100 - PUMP_CTRL_MAX_PCT - 1);
        voltage = PUMP_SAT_MIN_V + ratio * (PUMP_SAT_MAX_V - PUMP_SAT_MIN_V);
    }

    dac7311_set_voltage(voltage);

    rt_kprintf("[PUMP] speed=%u%% -> voltage=%.2fV\n", percent, voltage);
}
