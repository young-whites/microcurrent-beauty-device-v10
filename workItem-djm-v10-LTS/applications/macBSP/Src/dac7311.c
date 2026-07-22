/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-07-14     Administrator       DAC driver for pump speed control
 * 2026-07-22     refactor           Simplified: voltage control only, removed wave/test modes
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
#include <stdlib.h>

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

/* ============================================================================
 *  Internal: SPI Delay (fast NOP mode, ~140ns per step)
 * ===========================================================================*/
static void dac_delay(void)
{
    __asm__ volatile("nop"); __asm__ volatile("nop");
    __asm__ volatile("nop"); __asm__ volatile("nop");
    __asm__ volatile("nop"); __asm__ volatile("nop");
    __asm__ volatile("nop"); __asm__ volatile("nop");
    __asm__ volatile("nop"); __asm__ volatile("nop");
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

    /* Configure SYNC (PB7), SCLK (PB8), DIN (PB9) as push-pull outputs. */
    gpio.Pin   = DAC_SYNC_PIN | DAC_SCLK_PIN | DAC_DIN_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(DAC_GPIO_PORT, &gpio);

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

    /* Convert voltage to 14-bit value: D = V * 16383 / VREF */
    uint16_t value = (uint16_t)((voltage / DAC7311_VREF) * (DAC7311_RESOLUTION - 1) + 0.5f);
    if (value > 16383) value = 16383;

    /* Build 16-bit frame: [MODE(2)=00 DATA(14) RSVD(2)=0] */
    uint16_t frame = (value << 2) & 0x3FFC;

    /* Write to DAC */
    dac7311_write_frame(frame);

    /* Update state */
    s_dac_raw = value;
    s_dac_voltage = voltage;
}

void dac7311_set_raw(uint16_t value)
{
    if (value > 16383) value = 16383;

    uint16_t frame = (value << 2) & 0x3FFC;
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
 *  Pump Speed Control (linear: 0%=0V, 100%=5.0V)
 * ===========================================================================*/

void dac7311_set_pump_speed(uint8_t percent)
{
    if (percent > 100) percent = 100;

    /* Linear mapping: 0% -> 0V, 100% -> 5.0V */
    float voltage = DAC7311_VREF * (float)percent / 100.0f;

    dac7311_set_voltage(voltage);
}

/* ============================================================================
 *  RT-Thread Shell Command: dac
 *  Usage:
 *    dac volt <value>     - Set output voltage (0.0 ~ 5.0V)
 *    dac raw <value>      - Set raw 14-bit value (0 ~ 16383)
 *    dac pct <value>      - Set percentage (0 ~ 100%)
 *    dac pd <mode>        - Power-down (0=normal, 1=1k, 2=100k, 3=HiZ)
 *    dac info             - Show current status
 * ===========================================================================*/
static int dac(int argc, char **argv)
{
    if (argc < 2) {
        rt_kprintf("Usage:\n");
        rt_kprintf("  dac volt <0.0~5.0>    Set voltage\n");
        rt_kprintf("  dac raw  <0~16383>    Set raw value\n");
        rt_kprintf("  dac pct  <0~100>      Set percentage\n");
        rt_kprintf("  dac pd   <0~3>        Power-down mode\n");
        rt_kprintf("  dac info              Show status\n");
        return -RT_ERROR;
    }

    if (rt_strcmp(argv[1], "volt") == 0) {
        if (argc < 3) { rt_kprintf("Usage: dac volt <voltage>\n"); return -RT_ERROR; }
        float v = atof(argv[2]);
        dac7311_set_voltage(v);
        rt_kprintf("[DAC] Voltage set to %.3fV (raw=%d)\n", dac7311_get_voltage(), s_dac_raw);
    } else if (rt_strcmp(argv[1], "raw") == 0) {
        if (argc < 3) { rt_kprintf("Usage: dac raw <0~16383>\n"); return -RT_ERROR; }
        uint16_t val = atoi(argv[2]);
        dac7311_set_raw(val);
        rt_kprintf("[DAC] Raw set to %d (%.3fV)\n", val, dac7311_get_voltage());
    } else if (rt_strcmp(argv[1], "pct") == 0) {
        if (argc < 3) { rt_kprintf("Usage: dac pct <0~100>\n"); return -RT_ERROR; }
        uint8_t pct = atoi(argv[2]);
        dac7311_set_percent(pct);
        rt_kprintf("[DAC] Percent set to %d%% (%.3fV)\n", pct, dac7311_get_voltage());
    } else if (rt_strcmp(argv[1], "pd") == 0) {
        if (argc < 3) { rt_kprintf("Usage: dac pd <0~3>\n"); return -RT_ERROR; }
        uint8_t mode = atoi(argv[2]);
        dac7311_power_down(mode);
        rt_kprintf("[DAC] Power-down mode=%d\n", mode);
    } else if (rt_strcmp(argv[1], "info") == 0) {
        rt_kprintf("[DAC7311 Status]\n");
        rt_kprintf("  Voltage: %.3fV\n", dac7311_get_voltage());
        rt_kprintf("  Raw:     %d\n", s_dac_raw);
        rt_kprintf("  Pinout:  PB7=SYNC, PB8=SCLK, PB9=DIN\n");
        rt_kprintf("  VREF:    %.1fV (VDD)\n", DAC7311_VREF);
        rt_kprintf("  Mode:    SPI SW Bit-Bang, Mode 0, MSB first\n");
    } else {
        rt_kprintf("Unknown command: %s\n", argv[1]);
        return -RT_ERROR;
    }

    return RT_EOK;
}
MSH_CMD_EXPORT(dac, DAC7311 control: dac volt/raw/pct/pd/info);
