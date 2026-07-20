/**
  ******************************************************************************
  * @file    nnc6521_waveform_config.c
  * @brief   NNC6521 waveform configuration file for DJM-V10 beauty device.
  *          Implements 9 predefined waveform current mapping and NNC6521 driver
  *          API integration.
  *          Includes waveform parameter calculation, config query, and waveform
  *          application functions.
  ******************************************************************************
  */

#include "nnc6521.h"
#include "nnc6521_waveform_config.h"
#include <rtthread.h>

/* ============================================================================
 *  Global waveform config array
 *
 *  PCLK = 2 MHz
 *  half_wave_clk = PCLK / (2 * frequency)
 *  silent_time = pulse_width_us * 2  (since PCLK = 2 MHz, 1 us = 2 clocks)
 * ===========================================================================*/

const waveform_config_t g_waveform_configs[WAVEFORM_COUNT] =
{
    /* ---- Waveform 1: Power Smooth ---- */
    {
        .id              = 1,
        .name            = "Power Smooth",
        .description     = "Strong smoothing symmetric square wave",
        .min_current     = 0,
        .max_current     = 80,       /* Maximum output current 80 mA */
        .frequency       = 50,       /* Waveform frequency 50 Hz */
        .pulse_width_us  = 300,      /* Pulse width 300 us */
        .waveform_type   = WAVEFORM_TYPE_SQUARE,     /* Square wave */
        .gen_method      = GEN_METHOD_PRELOADED,      /* Use preloaded waveform */
        .point_num       = 64,       /* 64 points */
        .half_wave_clk   = 20000,   /* 2000000 / (2*50) = 20000 */
        .silent_time     = 600,     /* 300 * 2 = 600 */
        .rest_time       = 0,       /* No dead zone */
        .carrier_clk     = 0,       /* Not AM mode */
        .am_interval     = 0,       /* Not AM mode */
        .waveform_data   = NULL     /* Preloaded mode, no custom data needed */
    },

    /* ---- Waveform 2: Burst Train ---- */
    {
        .id              = 2,
        .name            = "Burst Train",
        .description     = "Burst pulse train at 10 Hz repetition",
        .min_current     = 0,
        .max_current     = 80,
        .frequency       = 50,       /* Carrier frequency 50 Hz */
        .pulse_width_us  = 300,
        .waveform_type   = WAVEFORM_TYPE_BURST,       /* Burst type */
        .gen_method      = GEN_METHOD_CUSTOM_SPI,      /* Use SPI custom waveform */
        .point_num       = 64,
        .half_wave_clk   = 20000,   /* 2000000 / (2*50) = 20000 */
        .silent_time     = 600,     /* 300 * 2 = 600 */
        .rest_time       = 0,
        .carrier_clk     = 0,
        .am_interval     = 0,
        .waveform_data   = burst_pulse_64  /* Burst pulse data (v10 naming) */
    },

    /* ---- Waveform 3: Gentle Smooth ---- */
    {
        .id              = 3,
        .name            = "Gentle Smooth",
        .description     = "Gentle smoothing symmetric square wave",
        .min_current     = 0,
        .max_current     = 80,       /* Lower maximum current */
        .frequency       = 35,       /* Lower frequency 35 Hz */
        .pulse_width_us  = 300,
        .waveform_type   = WAVEFORM_TYPE_SQUARE,
        .gen_method      = GEN_METHOD_PRELOADED,
        .point_num       = 64,
        .half_wave_clk   = 28571,   /* 2000000 / (2*35) = 28571 */
        .silent_time     = 600,     /* 300 * 2 = 600 */
        .rest_time       = 0,
        .carrier_clk     = 0,
        .am_interval     = 0,
        .waveform_data   = NULL
    },

    /* ---- Waveform 4: Deep Sculpt ---- */
    {
        .id              = 4,
        .name            = "Deep Sculpt",
        .description     = "Deep sculpting with 4 kHz carrier",
        .min_current     = 0,
        .max_current     = 80,
        .frequency       = 50,       /* Envelope frequency 50 Hz */
        .pulse_width_us  = 250,      /* 4 kHz carrier half-cycle 250 us */
        .waveform_type   = WAVEFORM_TYPE_BALANCED_SQUARE, /* Balanced square wave */
        .gen_method      = GEN_METHOD_CUSTOM_SPI,
        .point_num       = 128,      /* 128 points high precision */
        .half_wave_clk   = 20000,   /* 2000000 / (2*50) = 20000 */
        .silent_time     = 250,     /* 2000000 / (2*4000) = 250 (carrier period) */
        .rest_time       = 0,
        .carrier_clk     = 0,
        .am_interval     = 0,
        .waveform_data   = deep_sculpt_pulse_128  /* Deep sculpt pulse data */
    },

    /* ---- Waveform 5: Soft Sculpt ---- */
    {
        .id              = 5,
        .name            = "Soft Sculpt",
        .description     = "Soft sculpting sine wave",
        .min_current     = 0,
        .max_current     = 80,
        .frequency       = 40,       /* Sine wave frequency 40 Hz */
        .pulse_width_us  = 0,        /* Sine wave has no pulse width */
        .waveform_type   = WAVEFORM_TYPE_SINE,          /* Sine wave */
        .gen_method      = GEN_METHOD_CUSTOM_SPI,
        .point_num       = 128,      /* 128 points high precision sine */
        .half_wave_clk   = 25000,   /* 2000000 / (2*40) = 25000 */
        .silent_time     = 0,       /* No silent period */
        .rest_time       = 0,
        .carrier_clk     = 0,
        .am_interval     = 0,
        .waveform_data   = normalized_sine_waveform_128  /* 128-point sine data */
    },

    /* ---- Waveform 6: Circulation Sculpt ---- */
    /* Changed from AMPLITUDE_MOD to CUSTOM_SPI: pre-computed AM waveform
     * bypasses hardware AM mode */
    {
        .id              = 6,
        .name            = "Circulation Sculpt",
        .description     = "Circulation sculpting with 4kHz carrier AM",
        .min_current     = 0,
        .max_current     = 80,
        .frequency       = 10,       /* Envelope frequency 10 Hz */
        .pulse_width_us  = 0,
        .waveform_type   = WAVEFORM_TYPE_BALANCED_SINE,
        .gen_method      = GEN_METHOD_CUSTOM_SPI,       /* Changed from AMPLITUDE_MOD */
        .point_num       = 64,
        .half_wave_clk   = 12500,   /* PCLK/8: 250000 / (2*10) = 12500 */
        .silent_time     = 0,
        .rest_time       = 0,
        .carrier_clk     = 0,        /* Not used in SPI mode */
        .am_interval     = 0,        /* Not used in SPI mode */
        .waveform_data   = circulation_sculpt_am_64  /* Pre-computed AM data */
    },

    /* ---- Waveform 7: Smooth & Firm ---- */
    {
        .id              = 7,
        .name            = "Smooth & Firm",
        .description     = "Smooth and firm triangle wave",
        .min_current     = 0,
        .max_current     = 80,
        .frequency       = 100,      /* High frequency 100 Hz */
        .pulse_width_us  = 400,      /* Pulse width 400 us */
        .waveform_type   = WAVEFORM_TYPE_TRIANGLE,       /* Triangle wave */
        .gen_method      = GEN_METHOD_PRELOADED,          /* Preloaded mode */
        .point_num       = 64,
        .half_wave_clk   = 10000,   /* 2000000 / (2*100) = 10000 */
        .silent_time     = 800,     /* 400 * 2 = 800 */
        .rest_time       = 0,
        .carrier_clk     = 0,
        .am_interval     = 0,
        .waveform_data   = NULL
    },

    /* ---- Waveform 8: Lymphatic Drainage ---- */
    /* NOTE: Requires PCLK_DIV_16 (PCLK=125kHz). With PCLK/16:
     *   half_wave_clk = 125000 / (2*5) = 12500 (fits uint16_t)
     *   silent_time   = 125000 * 450e-6 = 56 */
    {
        .id              = 8,
        .name            = "Lymphatic Drainage",
        .description     = "Low-frequency sine for lymphatic drainage",
        .min_current     = 0,
        .max_current     = 80,
        .frequency       = 5,        /* Very low frequency 5 Hz */
        .pulse_width_us  = 450,
        .waveform_type   = WAVEFORM_TYPE_SINE,
        .gen_method      = GEN_METHOD_CUSTOM_SPI,
        .point_num       = 64,
        .half_wave_clk   = 12500,   /* PCLK/16: 125000/(2*5) = 12500 */
        .silent_time     = 56,      /* PCLK/16: 125000*450e-6 = 56 */
        .rest_time       = 0,
        .carrier_clk     = 0,
        .am_interval     = 0,
        .waveform_data   = normalized_sine_waveform_64   /* 64-point sine */
    },

    /* ---- Waveform 9: Soothing Ending ---- */
    /* NOTE: Requires PCLK_DIV_8 (PCLK=250kHz). With PCLK/8:
     *   half_wave_clk = 250000 / (2*10) = 12500 (fits uint16_t)
     *   Uses custom SPI sine (not preloaded) per spec */
    {
        .id              = 9,
        .name            = "Soothing Ending",
        .description     = "Soothing ending sine wave",
        .min_current     = 0,
        .max_current     = 80,
        .frequency       = 500,      /* 500 Hz */
        .pulse_width_us  = 0,        /* No pulse width */
        .waveform_type   = WAVEFORM_TYPE_SINE,
        .gen_method      = GEN_METHOD_CUSTOM_SPI,         /* Custom SPI sine */
        .point_num       = 64,
        .half_wave_clk   = 2000,    /* 2000000/(2*500) = 2000 */
        .silent_time     = 0,       /* Continuous sine wave, no silent period */
        .rest_time       = 0,
        .carrier_clk     = 0,
        .am_interval     = 0,
        .waveform_data   = normalized_sine_waveform_64    /* 64-point sine */
    }
};

/* ============================================================================
 *  Internal helper: PCLK divider setup
 * ===========================================================================*/

/**
 * @brief Set PCLK divider for a NNC6521 chip.
 *        Used for low-frequency waveforms that exceed 16-bit half-wave register.
 */
static void set_pclk_divider(uint8_t chip_id, uint8_t divider)
{
    nnc6521_write_reg(chip_id, CLK_CTRL_REG_ADDR, divider);
}

/* ============================================================================
 *  Internal helper: Map waveform type to NNC6521 preloaded enum
 * ===========================================================================*/

/**
 * @brief Map waveform type to NNC6521 preloaded waveform enum
 *
 * @param[in] waveform_type Waveform type (WAVEFORM_TYPE_xxx)
 * @return NNC6521 preloaded waveform enum (WAVEFORM_PULSE / WAVEFORM_TRIANGLE /
 *         WAVEFORM_SINE)
 *
 * @note Square and burst types map to WAVEFORM_PULSE
 * @note Sine and balanced sine types map to WAVEFORM_SINE
 */
static uint8_t get_preloaded_type(uint8_t waveform_type)
{
    switch (waveform_type) {
        case WAVEFORM_TYPE_SQUARE:
        case WAVEFORM_TYPE_BURST:
            return WAVEFORM_PULSE;
        case WAVEFORM_TYPE_TRIANGLE:
            return WAVEFORM_TRIANGLE;
        case WAVEFORM_TYPE_SINE:
        case WAVEFORM_TYPE_BALANCED_SINE:
            return WAVEFORM_SINE;
        default:
            return WAVEFORM_SINE;
    }
}

/* ============================================================================
 *  Public API: Calculate actual output current
 * ===========================================================================*/

/**
 * @brief Calculate actual output current from waveform ID and percentage
 *
 * Formula: actual_current = min_current + (max_current - min_current) * percent / 100
 *
 * @param[in] waveform_id Waveform ID (1~WAVEFORM_COUNT)
 * @param[in] percent     Current percentage (0~100)
 * @return Actual output current (mA), returns 0 for invalid parameters
 *
 * @see waveform_apply()
 */
uint32_t waveform_calc_current(uint8_t waveform_id, uint8_t percent)
{
    if (waveform_id < 1 || waveform_id > WAVEFORM_COUNT) return 0;
    if (percent > 100) percent = 100;

    const waveform_config_t *cfg = &g_waveform_configs[waveform_id - 1];
    uint32_t range = cfg->max_current - cfg->min_current;
    return cfg->min_current + (range * percent) / 100;
}

/* ============================================================================
 *  Public API: Get waveform config
 * ===========================================================================*/

/**
 * @brief Get configuration structure pointer by waveform ID
 *
 * @param[in] waveform_id Waveform ID (1~WAVEFORM_COUNT)
 * @return Configuration struct pointer, returns NULL for invalid ID
 *
 * @see g_waveform_configs
 */
const waveform_config_t* waveform_get_config(uint8_t waveform_id)
{
    if (waveform_id < 1 || waveform_id > WAVEFORM_COUNT) return NULL;
    return &g_waveform_configs[waveform_id - 1];
}

/* ============================================================================
 *  Public API: Apply waveform to NNC6521
 * ===========================================================================*/

/**
 * @brief Apply specified waveform to NNC6521 chip
 *
 * Automatically selects the corresponding NNC6521 API based on generation method:
 * - GEN_METHOD_PRELOADED: calls nnc6521_preloaded_waveform(), uses built-in waveform
 * - GEN_METHOD_CUSTOM_SPI: calls nnc6521_customized_waveform(), transmits custom data
 * - GEN_METHOD_AMPLITUDE_MOD: calls nnc6521_amplitude_modulation(), AM modulation
 *
 * @param[in] chip_id     Chip ID
 * @param[in] channel     Channel number
 * @param[in] waveform_id Waveform ID (1~WAVEFORM_COUNT)
 * @param[in] percent     Current percentage (0~100)
 *
 * @note CI (current index) defaults to 4 for reasonable drive range
 * @note Custom SPI waveforms use asymmetric mode (asymmetric = 0)
 */
void waveform_apply(uint8_t chip_id, uint8_t channel,
                    uint8_t waveform_id, uint8_t percent)
{
    if (waveform_id < 1 || waveform_id > WAVEFORM_COUNT) return;

    const waveform_config_t *cfg = &g_waveform_configs[waveform_id - 1];
    uint32_t actual_current = waveform_calc_current(waveform_id, percent);

    /* Map waveform type to NNC6521 preloaded waveform enum */
    uint8_t nnc_waveform = get_preloaded_type(cfg->waveform_type);

    /* CI (current index) for drive strength control.
     * Default value 4 provides reasonable drive range. */
    uint8_t ci = 4;

    switch (cfg->gen_method) {
        case GEN_METHOD_PRELOADED:
            nnc6521_preloaded_waveform(chip_id, channel,
                                       nnc_waveform,
                                       cfg->point_num,
                                       ci,
                                       cfg->half_wave_clk,
                                       cfg->half_wave_clk,
                                       cfg->silent_time,
                                       cfg->rest_time);
            break;

        case GEN_METHOD_CUSTOM_SPI:
            if (cfg->waveform_data != NULL) {
                /* Low-freq waveforms need reduced PCLK to fit 16-bit register */
                if (waveform_id == 8) {
                    set_pclk_divider(chip_id, PCLK_DIV_16);
                } else if (waveform_id == 6 || waveform_id == 9) {
                    set_pclk_divider(chip_id, PCLK_DIV_16);  /* 125kHz for low-freq waveforms */
                }

                nnc6521_customized_waveform(chip_id, channel,
                                            cfg->point_num,
                                            cfg->waveform_data,
                                            actual_current * 1000,
                                            cfg->half_wave_clk,
                                            cfg->half_wave_clk,
                                            cfg->silent_time,
                                            cfg->rest_time,
                                            0);  /* asymmetric */

                /* Restore PCLK to default */
                if (waveform_id == 6 || waveform_id == 8) {
                    set_pclk_divider(chip_id, PCLK_DIV_1);
                }
            }
            break;

        case GEN_METHOD_AMPLITUDE_MOD:
            if (cfg->waveform_data != NULL) {
                nnc6521_amplitude_modulation(chip_id, channel,
                                             cfg->point_num,
                                             cfg->waveform_data,
                                             actual_current * 1000,
                                             cfg->carrier_clk,
                                             cfg->silent_time,
                                             cfg->am_interval);
            }
            break;

        default:
            break;
    }
}

/* ============================================================================ *  Public API: Update waveform amplitude only (no timing reconfiguration)
 * ===========================================================================*/

/**
 * @brief Update waveform amplitude without reconfiguring timing parameters.
 *        More efficient than waveform_apply() when only the current changes.
 *
 * For CUSTOM_SPI waveforms: calls nnc6521_customized_amplitude() to rewrite
 *   the waveform data array with new amplitude.
 * For PRELOADED waveforms: updates the CI (current index) register directly.
 * For AMPLITUDE_MOD waveforms: falls back to full waveform_apply().
 *
 * @param[in] chip_id     Chip ID (NNC6521_CHIP_1 or NNC6521_CHIP_2)
 * @param[in] channel     Waveform channel (WAVEFORM_GEN_CH0 or WAVEFORM_GEN_CH1)
 * @param[in] waveform_id Waveform ID (1~WAVEFORM_COUNT)
 * @param[in] percent     New current percentage (0~100)
 *
 * @note The waveform must already be configured via waveform_apply() first.
 *       This function only updates amplitude, not timing or waveform type.
 */
void waveform_update_amplitude(uint8_t chip_id, uint8_t channel,
                               uint8_t waveform_id, uint8_t percent)
{
    if (waveform_id < 1 || waveform_id > WAVEFORM_COUNT) return;

    const waveform_config_t *cfg = &g_waveform_configs[waveform_id - 1];
    uint32_t actual_current = waveform_calc_current(waveform_id, percent);

    /* Stop AWG before updating to prevent reading mixed data */
    nnc6521_awg_enable_disable(chip_id, channel, 0);

    switch (cfg->gen_method) {
        case GEN_METHOD_PRELOADED:
        {
            uint8_t ci = 4;
            int addr = WG_REG_ADDR(channel, WG_DRIVE_REG_CTRL2_OFFSET);
            nnc6521_write_wave_reg(chip_id, addr, ci);
            break;
        }

        case GEN_METHOD_CUSTOM_SPI:
            if (cfg->waveform_data != NULL) {
                nnc6521_customized_amplitude(chip_id, channel,
                                             cfg->point_num,
                                             cfg->waveform_data,
                                             actual_current * 1000);
            }
            break;

        case GEN_METHOD_AMPLITUDE_MOD:
            waveform_apply(chip_id, channel, waveform_id, percent);
            return;  /* waveform_apply already re-enables AWG */

        default:
            break;
    }

    /* Re-enable AWG after amplitude update */
    nnc6521_awg_enable_disable(chip_id, channel, 1);
}
