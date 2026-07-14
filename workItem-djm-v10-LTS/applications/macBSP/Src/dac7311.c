/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-07-14     Administrator       DAC7311 driver for pump speed control
 *
 * DAC7311: 12-bit, single-channel, SPI DAC (Texas Instruments)
 * - SPI Mode 0 (CPOL=0, CPHA=0), MSB first
 * - 16-bit frame: [X X PD1 PD0 D11 D10 ... D0]
 * - VOUT = VREF * D / 4096 (VREF = VDD = 5V)
 *
 * Hardware:
 *   PB7  -> SYNC (chip select, active low)
 *   PB8  -> SCLK (serial clock)
 *   PB9  -> DIN  (serial data input)
 */

#include "dac7311.h"
#include "main.h"

/* ============================================================================
 *  GPIO Pin Definitions
 * ===========================================================================*/
#define DAC_SYNC_PORT       GPIOB
#define DAC_SYNC_PIN        GPIO_PIN_7

#define DAC_SCLK_PORT       GPIOB
#define DAC_SCLK_PIN        GPIO_PIN_8

#define DAC_DIN_PORT        GPIOB
#define DAC_DIN_PIN         GPIO_PIN_9

/* ============================================================================
 *  Internal State
 * ===========================================================================*/
static float s_dac_voltage = 0.0f;  /* Current output voltage */
static uint16_t s_dac_raw = 0;      /* Current raw 12-bit value */

/* ============================================================================
 *  Internal: GPIO Bit-Bang SPI
 * ===========================================================================*/

/**
 * @brief  Write 16-bit frame to DAC7311 via software SPI.
 *         Frame format: [D15 D14 PD1 PD0 D11 D10 ... D0]
 *         SPI Mode 0: CPOL=0, CPHA=0, MSB first.
 *
 * @param  data  16-bit frame data.
 */
static void dac7311_write_frame(uint16_t data)
{
    /* Pull SYNC low to start transaction */
    HAL_GPIO_WritePin(DAC_SYNC_PORT, DAC_SYNC_PIN, GPIO_PIN_RESET);

    /* Clock out 16 bits, MSB first */
    for (int8_t bit = 15; bit >= 0; bit--) {
        /* Set DIN before rising edge */
        if (data & (1 << bit)) {
            HAL_GPIO_WritePin(DAC_DIN_PORT, DAC_DIN_PIN, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(DAC_DIN_PORT, DAC_DIN_PIN, GPIO_PIN_RESET);
        }

        __NOP(); __NOP();  /* Data setup time */

        /* SCLK rising edge */
        HAL_GPIO_WritePin(DAC_SCLK_PORT, DAC_SCLK_PIN, GPIO_PIN_SET);

        __NOP(); __NOP();  /* Clock high time */

        /* SCLK falling edge */
        HAL_GPIO_WritePin(DAC_SCLK_PORT, DAC_SCLK_PIN, GPIO_PIN_RESET);

        __NOP(); __NOP();  /* Clock low time */
    }

    /* Pull SYNC high to latch data */
    HAL_GPIO_WritePin(DAC_SYNC_PORT, DAC_SYNC_PIN, GPIO_PIN_SET);
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
    HAL_GPIO_WritePin(DAC_SYNC_PORT, DAC_SYNC_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DAC_SCLK_PORT, DAC_SCLK_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DAC_DIN_PORT, DAC_DIN_PIN, GPIO_PIN_RESET);

    /* Output 0V */
    dac7311_set_voltage(0.0f);

    rt_kprintf("[DAC7311] Initialized, output=0.00V\n");
}

void dac7311_set_voltage(float voltage)
{
    /* Clamp to valid range */
    if (voltage < 0.0f) voltage = 0.0f;
    if (voltage > DAC7311_VOUT_MAX) voltage = DAC7311_VOUT_MAX;

    /* Convert voltage to 12-bit value: D = V * 4096 / VREF */
    uint16_t value = (uint16_t)((voltage / DAC7311_VREF) * (DAC7311_RESOLUTION - 1) + 0.5f);
    if (value > 4095) value = 4095;

    /* Build 16-bit frame: [XX PD1=0 PD0=0 D11..D0] */
    uint16_t frame = (DAC7311_PD_NORMAL << 12) | value;

    /* Write to DAC */
    dac7311_write_frame(frame);

    /* Update state */
    s_dac_raw = value;
    s_dac_voltage = voltage;
}

void dac7311_set_raw(uint16_t value)
{
    if (value > 4095) value = 4095;

    uint16_t frame = (DAC7311_PD_NORMAL << 12) | value;
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
    /* Power-down frame: [XX PD1 PD0 0...0] */
    uint16_t frame = ((uint16_t)(mode & 0x03) << 12);
    dac7311_write_frame(frame);

    s_dac_raw = 0;
    s_dac_voltage = 0.0f;

    rt_kprintf("[DAC7311] Power-down mode=%d\n", mode);
}

float dac7311_get_voltage(void)
{
    return s_dac_voltage;
}
