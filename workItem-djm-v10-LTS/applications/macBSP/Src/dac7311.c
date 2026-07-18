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
 *  GPIO Pin Definitions (direct register access)
 * ===========================================================================*/
#define DAC_SYNC_PIN        GPIO_PIN_7
#define DAC_SCLK_PIN        GPIO_PIN_8
#define DAC_DIN_PIN         GPIO_PIN_9
#define DAC_GPIO_PORT       GPIOB

/* ============================================================================
 *  Internal State
 * ===========================================================================*/
static float s_dac_voltage = 0.0f;
static uint16_t s_dac_raw = 0;
static uint32_t s_dac_delay_us = 5;  /* default 5us per phase for stable DAC latch */

/* ============================================================================
 *  Configurable Delay
 * ===========================================================================*/

/**
 * @brief  Set SPI clock delay. Controls SCLK high/low time.
 * @param  delay_us  Delay per step in microseconds. 0 = fast mode (~140ns).
 */
void dac7311_set_delay(uint32_t delay_us)
{
    s_dac_delay_us = delay_us;
}

uint32_t dac7311_get_delay(void)
{
    return s_dac_delay_us;
}

/**
 * @brief  SPI clock step delay.
 *         If s_dac_delay_us == 0: fast mode, ~140ns NOP-based.
 *         If s_dac_delay_us > 0:  blocking delay in microseconds.
 */
static void dac_delay(void)
{
    if (s_dac_delay_us == 0) {
        /* Fast mode: ~10 NOPs ~= 140ns at 72MHz */
        __asm__ volatile("nop"); __asm__ volatile("nop");
        __asm__ volatile("nop"); __asm__ volatile("nop");
        __asm__ volatile("nop"); __asm__ volatile("nop");
        __asm__ volatile("nop"); __asm__ volatile("nop");
        __asm__ volatile("nop"); __asm__ volatile("nop");
    } else {
        /* Configurable delay: use busy-wait loop */
        volatile uint32_t count = s_dac_delay_us * 12;  /* ~12 loops/us at 72MHz */
        while (count--);
    }
}

/* ============================================================================
 *  Internal: GPIO Bit-Bang SPI (BSRR register, no HAL overhead)
 * ===========================================================================*/

/**
 * @brief  Write 16-bit frame to DAC via software SPI.
 *         Uses direct BSRR/BRR register access for deterministic timing.
 *         SPI Mode 0: CPOL=0, CPHA=0, MSB first.
 */
static void dac7311_write_frame(uint16_t data)
{
    GPIO_TypeDef *port = DAC_GPIO_PORT;

    /* ---- Start: Pull SYNC low ---- */
    port->BRR = DAC_SYNC_PIN;
    dac_delay(); dac_delay();

    /* ---- Clock out 16 bits, MSB first ---- */
    for (int8_t bit = 15; bit >= 0; bit--) {

        /* Set DIN before SCLK rising edge */
        if (data & (1 << bit)) {
            port->BSRR = DAC_DIN_PIN;    /* DIN = HIGH */
        } else {
            port->BRR  = DAC_DIN_PIN;    /* DIN = LOW  */
        }

        dac_delay();  /* Data setup time */

        /* SCLK rising edge (latch data) */
        port->BSRR = DAC_SCLK_PIN;
        dac_delay();  /* Clock high time */

        /* SCLK falling edge */
        port->BRR = DAC_SCLK_PIN;
        dac_delay();  /* Clock low time */
    }

    /* ---- Latch: Pull SYNC high ---- */
    dac_delay();
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

    /* Configure SYNC (PB7), SCLK (PB8), DIN (PB9) as push-pull outputs.
     * Direct 3.3V drive, no external pull-ups needed.
     * VIH threshold may be marginal but often works in practice. */
    gpio.Pin   = DAC_SYNC_PIN | DAC_SCLK_PIN | DAC_DIN_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(DAC_GPIO_PORT, &gpio);

    /* Set idle state: SYNC=HIGH, SCLK=LOW, DIN=LOW */
    DAC_GPIO_PORT->BSRR = DAC_SYNC_PIN;       /* SYNC = HIGH */
    DAC_GPIO_PORT->BRR  = DAC_SCLK_PIN;       /* SCLK = LOW  */
    DAC_GPIO_PORT->BRR  = DAC_DIN_PIN;        /* DIN  = LOW  */

    /* Debug: dump GPIOB CRH register to verify pin config */
    rt_kprintf("[DAC7311] GPIOB CRL=0x%08X CRH=0x%08X ODR=0x%04X\n",
               (unsigned)DAC_GPIO_PORT->CRL, (unsigned)DAC_GPIO_PORT->CRH,
               (unsigned)DAC_GPIO_PORT->ODR);

    /* Output 2.75V for testing */
    dac7311_set_voltage(2.75f);

    rt_kprintf("[DAC7311] Initialized, output=2.750V\n");
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

void dac7311_write_raw_frame(uint16_t frame)
{
    dac7311_write_frame(frame);
}

/* ============================================================================
 *  Pump Speed Control (non-linear voltage mapping)
 * ===========================================================================*/

void dac7311_set_pump_speed(uint8_t percent)
{
    float voltage;

    if (percent == 0) {
        voltage = 0.0f;
    } else if (percent <= PUMP_CTRL_MAX_PCT) {
        /* Control zone: 1%~90% -> 0.6V~4.5V */
        float ratio = (float)(percent - 1) / (float)(PUMP_CTRL_MAX_PCT - 1);
        voltage = PUMP_CTRL_MIN_V + ratio * (PUMP_CTRL_MAX_V - PUMP_CTRL_MIN_V);
    } else {
        /* Saturation zone: 91%~100% -> 4.6V~5.0V */
        float ratio = (float)(percent - PUMP_CTRL_MAX_PCT - 1) / (float)(100 - PUMP_CTRL_MAX_PCT - 1);
        voltage = PUMP_SAT_MIN_V + ratio * (PUMP_SAT_MAX_V - PUMP_SAT_MIN_V);
    }

    dac7311_set_voltage(voltage);

    rt_kprintf("[PUMP] speed=%u%% -> voltage=%.2fV\n", percent, voltage);
}
