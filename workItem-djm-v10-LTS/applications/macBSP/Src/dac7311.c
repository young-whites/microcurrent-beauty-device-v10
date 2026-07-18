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
#include <math.h>

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
 *  Waveform Generator (for oscilloscope verification)
 * ===========================================================================*/
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WAVE_LUT_SIZE       256

typedef enum {
    WAVE_NONE = 0,
    WAVE_SIN,
    WAVE_SQUARE,
    WAVE_TRI,
    WAVE_SAW
} wave_type_t;

static uint16_t    s_wave_lut[WAVE_LUT_SIZE];
static uint32_t    s_wave_idx    = 0;
static wave_type_t s_wave_type   = WAVE_NONE;
static float       s_wave_freq   = 1.0f;
static float       s_wave_amp    = 2.5f;
static float       s_wave_offset = 2.5f;
static rt_timer_t  s_wave_timer  = RT_NULL;

static void wave_build_lut(wave_type_t type, float amp, float offset)
{
    for (int i = 0; i < WAVE_LUT_SIZE; i++) {
        float t = (float)i / (float)WAVE_LUT_SIZE;
        float v;

        switch (type) {
        case WAVE_SIN:
            v = offset + amp * sinf(2.0f * (float)M_PI * t);
            break;
        case WAVE_SQUARE:
            v = (t < 0.5f) ? (offset + amp) : (offset - amp);
            break;
        case WAVE_TRI:
            v = (t < 0.5f)
                ? (offset - amp + 4.0f * amp * t)
                : (offset + 3.0f * amp - 4.0f * amp * t);
            break;
        case WAVE_SAW:
            v = offset - amp + 2.0f * amp * t;
            break;
        default:
            v = offset;
            break;
        }

        if (v < 0.0f) v = 0.0f;
        if (v > DAC7311_VREF) v = DAC7311_VREF;

        s_wave_lut[i] = (uint16_t)((v / DAC7311_VREF) * 16383.0f + 0.5f);
    }
}

static void wave_timer_cb(void *parameter)
{
    (void)parameter;
    dac7311_set_raw(s_wave_lut[s_wave_idx]);
    s_wave_idx++;
    if (s_wave_idx >= WAVE_LUT_SIZE) {
        s_wave_idx = 0;
    }
}

static void wave_start(wave_type_t type, float freq, float amp, float offset)
{
    if (freq < 0.1f)    freq = 0.1f;
    if (freq > 1000.0f) freq = 1000.0f;
    if (amp < 0.0f)     amp = 0.0f;
    if (amp > DAC7311_VREF) amp = DAC7311_VREF;
    if (offset < 0.0f)  offset = 0.0f;
    if (offset > DAC7311_VREF) offset = DAC7311_VREF;
    if (offset + amp > DAC7311_VREF) amp = DAC7311_VREF - offset;
    if (offset - amp < 0.0f) amp = offset;

    if (s_wave_timer != RT_NULL) {
        rt_timer_stop(s_wave_timer);
        rt_timer_delete(s_wave_timer);
        s_wave_timer = RT_NULL;
    }

    s_wave_type   = type;
    s_wave_freq   = freq;
    s_wave_amp    = amp;
    s_wave_offset = offset;
    s_wave_idx    = 0;
    wave_build_lut(type, amp, offset);

    rt_tick_t period_ms = (rt_tick_t)(1000.0f / ((float)WAVE_LUT_SIZE * freq));
    if (period_ms < 1) period_ms = 1;

    s_wave_timer = rt_timer_create("wave", wave_timer_cb,
                                    RT_NULL, period_ms,
                                    RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
    if (s_wave_timer != RT_NULL) {
        rt_timer_start(s_wave_timer);
    } else {
        rt_kprintf("[WAVE] ERROR: Failed to create timer!\n");
    }
}

static void wave_stop(void)
{
    if (s_wave_timer != RT_NULL) {
        rt_timer_stop(s_wave_timer);
        rt_timer_delete(s_wave_timer);
        s_wave_timer = RT_NULL;
    }
    s_wave_type = WAVE_NONE;
    s_wave_idx  = 0;
}

static const char* wave_type_name(wave_type_t t)
{
    switch (t) {
    case WAVE_SIN:    return "sin";
    case WAVE_SQUARE: return "square";
    case WAVE_TRI:    return "tri";
    case WAVE_SAW:    return "saw";
    default:          return "none";
    }
}

/* ============================================================================
 *  SPI Protocol Test Mode (repeat fixed frame for oscilloscope capture)
 * ===========================================================================*/
static rt_timer_t  s_test_timer  = RT_NULL;
static uint16_t    s_test_frame  = 0x2000;
static uint32_t    s_test_count  = 0;

static void test_timer_cb(void *parameter)
{
    (void)parameter;
    dac7311_write_raw_frame(s_test_frame);
    s_test_count++;
}

static void test_start(uint16_t frame, uint32_t interval_ms)
{
    if (s_test_timer != RT_NULL) {
        rt_timer_stop(s_test_timer);
        rt_timer_delete(s_test_timer);
        s_test_timer = RT_NULL;
    }

    s_test_frame = frame;
    s_test_count = 0;

    s_test_timer = rt_timer_create("spitest", test_timer_cb,
                                    RT_NULL, interval_ms,
                                    RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
    if (s_test_timer != RT_NULL) {
        rt_timer_start(s_test_timer);
    } else {
        rt_kprintf("[TEST] ERROR: Failed to create timer!\n");
    }
}

static void test_stop(void)
{
    if (s_test_timer != RT_NULL) {
        rt_timer_stop(s_test_timer);
        rt_timer_delete(s_test_timer);
        s_test_timer = RT_NULL;
    }
    s_test_count = 0;
}

/* ============================================================================ *  RT-Thread Shell Command: dac
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
        rt_kprintf("  dac raw  <0~16383>     Set raw value\n");
        rt_kprintf("  dac pct  <0~100>      Set percentage\n");
        rt_kprintf("  dac pd   <0~3>        Power-down mode\n");
        rt_kprintf("  dac info              Show status\n");
        rt_kprintf("  dac wave <type> [freq] [amp] [offset]  Waveform output\n");
        rt_kprintf("  dac test [interval_ms] [hex_frame]  SPI protocol test\n");
        return -RT_ERROR;
    }

    if (rt_strcmp(argv[1], "volt") == 0) {
        if (argc < 3) { rt_kprintf("Usage: dac volt <voltage>\n"); return -RT_ERROR; }
        float v = atof(argv[2]);
        dac7311_set_voltage(v);
        rt_kprintf("[DAC] Voltage set to %.3fV (raw=%d)\n", dac7311_get_voltage(), (int)(v / DAC7311_VREF * 16383));
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
        rt_kprintf("  Pinout:  PB7=SYNC, PB8=SCLK, PB9=DIN\n");
        rt_kprintf("  VREF:    %.1fV (VDD)\n", DAC7311_VREF);
        rt_kprintf("  Mode:    SPI SW Bit-Bang, Mode 0, MSB first\n");
    } else if (rt_strcmp(argv[1], "wave") == 0) {
        if (argc < 3) {
            rt_kprintf("Usage:\n");
            rt_kprintf("  dac wave sin    [freq] [amp] [offset]  Sine wave\n");
            rt_kprintf("  dac wave square [freq] [amp] [offset]  Square wave\n");
            rt_kprintf("  dac wave tri    [freq] [amp] [offset]  Triangle wave\n");
            rt_kprintf("  dac wave saw    [freq] [amp] [offset]  Sawtooth wave\n");
            rt_kprintf("  dac wave stop                           Stop waveform\n");
            rt_kprintf("  dac wave info                           Show status\n");
            rt_kprintf("  Defaults: freq=1Hz, amp=2.5V, offset=2.5V\n");
            return -RT_ERROR;
        }

        if (rt_strcmp(argv[2], "stop") == 0) {
            wave_stop();
            rt_kprintf("[WAVE] Stopped\n");
        } else if (rt_strcmp(argv[2], "info") == 0) {
            if (s_wave_type == WAVE_NONE) {
                rt_kprintf("[WAVE] Idle (no waveform active)\n");
            } else {
                rt_tick_t period_ms = (rt_tick_t)(1000.0f / ((float)WAVE_LUT_SIZE * s_wave_freq));
                rt_kprintf("[WAVE] Active: %s\n", wave_type_name(s_wave_type));
                rt_kprintf("  Freq:   %.2f Hz\n", s_wave_freq);
                rt_kprintf("  Amp:    %.3f V\n", s_wave_amp);
                rt_kprintf("  Offset: %.3f V\n", s_wave_offset);
                rt_kprintf("  LUT:    %d points, %d ms/sample\n", WAVE_LUT_SIZE, (int)period_ms);
                rt_kprintf("  Range:  %.3f ~ %.3f V\n",
                           s_wave_offset - s_wave_amp,
                           s_wave_offset + s_wave_amp);
            }
        } else {
            wave_type_t type = WAVE_NONE;
            if (rt_strcmp(argv[2], "sin") == 0)          type = WAVE_SIN;
            else if (rt_strcmp(argv[2], "square") == 0)  type = WAVE_SQUARE;
            else if (rt_strcmp(argv[2], "tri") == 0)     type = WAVE_TRI;
            else if (rt_strcmp(argv[2], "saw") == 0)     type = WAVE_SAW;
            else {
                rt_kprintf("[WAVE] Unknown type: %s\n", argv[2]);
                return -RT_ERROR;
            }

            float freq   = (argc > 3) ? atof(argv[3]) : 1.0f;
            float amp    = (argc > 4) ? atof(argv[4]) : 2.5f;
            float offset = (argc > 5) ? atof(argv[5]) : 2.5f;

            wave_start(type, freq, amp, offset);
            rt_kprintf("[WAVE] Started: %s, %.2fHz, amp=%.3fV, offset=%.3fV\n",
                       wave_type_name(type), freq, amp, offset);
            rt_kprintf("[WAVE] Range: %.3f ~ %.3f V\n",
                       offset - amp < 0.0f ? 0.0f : offset - amp,
                       offset + amp > DAC7311_VREF ? DAC7311_VREF : offset + amp);
        }
    } else if (rt_strcmp(argv[1], "test") == 0) {
        if (argc < 3) {
            rt_kprintf("Usage:\n");
            rt_kprintf("  dac test [interval_ms] [hex_frame] [clk_us]  Repeat SPI frame\n");
            rt_kprintf("  dac test stop                                Stop test mode\n");
            rt_kprintf("  dac test info                                Show status\n");
            rt_kprintf("  Defaults: interval=10ms, frame=0x2000 (2.5V), clk=0 (fast)\n");
            rt_kprintf("  clk_us: SCLK high/low time in us (0=fast, 1000=1ms per half-clock)\n");
            rt_kprintf("\n  Probe: PB7=SYNC, PB8=SCLK, PB9=DIN\n");
            return -RT_ERROR;
        }

        if (rt_strcmp(argv[2], "stop") == 0) {
            test_stop();
            dac7311_set_delay(0);  /* restore fast mode */
            rt_kprintf("[TEST] Stopped (sent %d frames)\n", (int)s_test_count);
        } else if (rt_strcmp(argv[2], "info") == 0) {
            if (s_test_timer == RT_NULL) {
                rt_kprintf("[TEST] Idle\n");
            } else {
                rt_kprintf("[TEST] Active\n");
                rt_kprintf("  Frame:    0x%04X\n", s_test_frame);
                rt_kprintf("  Sent:     %d frames\n", (int)s_test_count);
                if (dac7311_get_delay() == 0)
                    rt_kprintf("  SCLK:     fast (~140ns)\n");
                else
                    rt_kprintf("  SCLK:     %d us per half-clock\n", (int)dac7311_get_delay());
                rt_kprintf("  Probe:    PB7=SYNC, PB8=SCLK, PB9=DIN\n");
                rt_kprintf("  Trigger:  Set scope to trigger on SYNC falling edge\n");
            }
        } else {
            uint32_t interval = (argc > 2) ? atoi(argv[2]) : 10;
            uint16_t frame    = (argc > 3) ? (uint16_t)strtol(argv[3], RT_NULL, 16) : 0x2000;
            uint32_t clk_us   = (argc > 4) ? atoi(argv[4]) : 0;
            if (interval < 1) interval = 1;
            if (interval > 10000) interval = 10000;

            dac7311_set_delay(clk_us);
            test_start(frame, interval);

            /* Calculate actual frame duration: 16 bits * 3 delays/bit */
            uint32_t frame_us = clk_us > 0 ? (16 * 3 * clk_us) : (16 * 3 * 140 / 1000);
            rt_kprintf("[TEST] Started: frame=0x%04X, interval=%dms\n", frame, (int)interval);
            if (clk_us == 0)
                rt_kprintf("[TEST] SCLK delay: fast (~140ns)\n");
            else {
                rt_kprintf("[TEST] SCLK delay: %d us per half-clock\n", (int)clk_us);
                rt_kprintf("[TEST] Frame time: ~%d us (16 bits x 3 steps)\n", (int)(16 * 3 * clk_us));
            }
            rt_kprintf("[TEST] Probe: PB7=SYNC, PB8=SCLK, PB9=DIN\n");
            rt_kprintf("[TEST] Scope trigger: SYNC falling edge\n");
            rt_kprintf("[TEST] Frame bits: ");
            for (int b = 15; b >= 0; b--) {
                rt_kprintf("%d", (frame >> b) & 1);
                if (b == 14 || b == 2) rt_kprintf(" ");
            }
            rt_kprintf("\n");
            rt_kprintf("[TEST]          [M1 M0][D11 D10 .. D1 D0][R R]\n");
        }
    } else {
        rt_kprintf("Unknown command: %s\n", argv[1]);
        return -RT_ERROR;
    }

    return RT_EOK;
}
MSH_CMD_EXPORT(dac, DAC7311 control: dac volt/raw/pct/pd/info <value>);
