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
static uint32_t s_dac_delay_us = 0;  /* 0 = fast mode (NOP), >0 = delay per step in us */

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

    /* Build 16-bit frame: [MODE(2)=00 DATA(14) RSVD(2)=0]
     * Force mode bits to 00 via mask, data in D13:D2 */
    uint16_t frame = (value << 2) & 0x3FFC;

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

/* ============================================================================
 *  DAC Test Function (RT-Thread Shell)
 * ===========================================================================*/

/**
 * @brief  DAC comprehensive test.
 *  Usage:
 *    dactest gpio      - Toggle PB8/PB9 for hardware verification
 *    dactest volt <v>  - Set specific voltage (0.0 ~ 5.0)
 *    dactest sweep     - Sweep 0V -> 1V -> 2V -> 3V -> 4V -> 5V (2s each)
 *    dactest pump <p>  - Test pump speed mapping (0~100)
 *    dactest dump      - Dump GPIO registers
 *    dactest frame <v> - Show frame value for a voltage
 */
static int dactest(int argc, char **argv)
{
    if (argc < 2) {
        rt_kprintf("Usage:\n");
        rt_kprintf("  dactest gpio       Toggle PB8/PB9 (3s each state)\n");
        rt_kprintf("  dactest volt <v>   Set voltage (0.0~5.0V)\n");
        rt_kprintf("  dactest sweep      Sweep 0V->5V in 1V steps\n");
        rt_kprintf("  dactest pump <p>   Test pump speed (0~100%%)\n");
        rt_kprintf("  dactest dump       Dump GPIO registers\n");
        rt_kprintf("  dactest frame <v>  Show frame for voltage\n");
        return -RT_ERROR;
    }

    GPIO_TypeDef *port = DAC_GPIO_PORT;

    if (rt_strcmp(argv[1], "gpio") == 0) {
        /* GPIO toggle test */
        rt_kprintf("[DACTEST] PB8=HIGH PB9=LOW - measure now (3s)\n");
        port->BSRR = DAC_SCLK_PIN;
        port->BRR  = DAC_DIN_PIN;
        rt_thread_mdelay(3000);

        rt_kprintf("[DACTEST] PB8=LOW PB9=HIGH - measure now (3s)\n");
        port->BRR  = DAC_SCLK_PIN;
        port->BSRR = DAC_DIN_PIN;
        rt_thread_mdelay(3000);

        rt_kprintf("[DACTEST] PB8=LOW PB9=LOW\n");
        port->BRR = DAC_SCLK_PIN;
        port->BRR = DAC_DIN_PIN;

    } else if (rt_strcmp(argv[1], "volt") == 0) {
        if (argc < 3) { rt_kprintf("Usage: dactest volt <0.0~5.0>\n"); return -RT_ERROR; }
        float v = atof(argv[2]);
        dac7311_set_voltage(v);
        rt_kprintf("[DACTEST] Output %.3fV, measure VOUT now\n", v);

    } else if (rt_strcmp(argv[1], "sweep") == 0) {
        float voltages[] = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        for (int i = 0; i < 6; i++) {
            dac7311_set_voltage(voltages[i]);
            rt_kprintf("[DACTEST] %.1fV - measure now (2s)\n", voltages[i]);
            rt_thread_mdelay(2000);
        }
        dac7311_set_voltage(0.0f);
        rt_kprintf("[DACTEST] Sweep done, output 0V\n");

    } else if (rt_strcmp(argv[1], "pump") == 0) {
        if (argc < 3) { rt_kprintf("Usage: dactest pump <0~100>\n"); return -RT_ERROR; }
        uint8_t pct = atoi(argv[2]);
        dac7311_set_pump_speed(pct);
        rt_kprintf("[DACTEST] Pump %u%% -> %.3fV, measure VOUT now\n", pct, dac7311_get_voltage());

    } else if (rt_strcmp(argv[1], "dump") == 0) {
        uint32_t apb2enr = RCC->APB2ENR;
        rt_kprintf("=== DAC GPIO Diagnostic ===\n");
        rt_kprintf("[RCC] APB2ENR    = 0x%08X\n", (unsigned)apb2enr);
        rt_kprintf("[RCC] GPIOB EN   = %s\n", (apb2enr & RCC_APB2ENR_IOPBEN) ? "YES" : "NO");
        rt_kprintf("[GPIOB] CRL      = 0x%08X  (pins 0-7)\n", (unsigned)port->CRL);
        rt_kprintf("[GPIOB] CRH      = 0x%08X  (pins 8-15)\n", (unsigned)port->CRH);
        rt_kprintf("[GPIOB] IDR      = 0x%04X  (input state)\n", (unsigned)port->IDR);
        rt_kprintf("[GPIOB] ODR      = 0x%04X  (output state)\n", (unsigned)port->ODR);

        /* Decode PB7/PB8/PB9 modes */
        uint32_t crl = port->CRL;
        uint32_t crh = port->CRH;
        uint32_t mode7 = (crl >> 28) & 0xF;
        uint32_t mode8 = crh & 0xF;
        uint32_t mode9 = (crh >> 4) & 0xF;

        const char *mode_str(uint32_t m) {
            if ((m & 0x3) == 0) return "Input";
            if ((m & 0x3) == 1) return (m & 0x4) ? "Out10MHz Open-Drain" : "Out10MHz Push-Pull";
            if ((m & 0x3) == 2) return (m & 0x4) ? "Out2MHz Open-Drain" : "Out2MHz Push-Pull";
            return (m & 0x4) ? "Out50MHz Open-Drain" : "Out50MHz Push-Pull";
        };
        rt_kprintf("[PB7/SYNC] CRH bits=0x%X -> %s\n", (unsigned)mode7, mode_str(mode7));
        rt_kprintf("[PB8/SCLK] CRH bits=0x%X -> %s\n", (unsigned)mode8, mode_str(mode8));
        rt_kprintf("[PB9/DIN]  CRH bits=0x%X -> %s\n", (unsigned)mode9, mode_str(mode9));

        rt_kprintf("[DAC] voltage=%.3fV raw=%d\n", dac7311_get_voltage(), s_dac_raw);

    } else if (rt_strcmp(argv[1], "frame") == 0) {
        if (argc < 3) { rt_kprintf("Usage: dactest frame <0.0~5.0>\n"); return -RT_ERROR; }
        float v = atof(argv[2]);
        uint16_t value = (uint16_t)((v / DAC7311_VREF) * (DAC7311_RESOLUTION - 1) + 0.5f);
        if (value > 16383) value = 16383;
        uint16_t frame = (value << 2) & 0x3FFC;
        rt_kprintf("[DACTEST] V=%.3fV -> raw=%d -> frame=0x%04X\n", v, value, frame);
        rt_kprintf("[DACTEST] Binary: ");
        for (int8_t bit = 15; bit >= 0; bit--) {
            rt_kprintf("%d", (frame >> bit) & 1);
            if (bit == 14 || bit == 12 || bit == 2) rt_kprintf(" ");
        }
        rt_kprintf("\n");
        rt_kprintf("[DACTEST]   M1M0  D13-D2    R R\n");

    } else {
        rt_kprintf("Unknown command: %s\n", argv[1]);
        return -RT_ERROR;
    }

    return RT_EOK;
}
MSH_CMD_EXPORT(dactest, DAC test: dactest gpio/volt/sweep/pump/dump/frame);
