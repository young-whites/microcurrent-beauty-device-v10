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
 *  AWG clock: ~32.768 kHz (independent of SPI PCLK=2MHz)
 *  For PCLK_DIV_1: AWG_CLK = 32768 Hz
 *  For PCLK_DIV_16: AWG_CLK = 2048 Hz (used by WF6, WF8)
 *  half_wave_clk = AWG_CLK / (2 * frequency)
 * ===========================================================================*/

const waveform_config_t g_waveform_configs[WAVEFORM_COUNT] =
{
    /* ---- Waveform 1: Power Smooth ---- */
    {
        .id              = 1,
        .name            = "Power Smooth",
        .description     = "Strong smoothing symmetric square wave",
        .min_current     = 0,
        .max_current     = 8,        /* Maximum output current 8 mA */
        .frequency       = 50,       /* Waveform frequency 50 Hz */
        .pulse_width_us  = 150,      /* Pulse width 150 us */
        .waveform_type   = WAVEFORM_TYPE_SQUARE,     /* Square wave (edge-softened) */
        .gen_method      = GEN_METHOD_CUSTOM_SPI,      /* Custom SPI softened waveform */
        .point_num       = 64,       /* 64 points */
        .half_wave_clk   = 328,     /* 32768 / (2*50) = 328 */
        .silent_time     = 300,     /* 150 * 2 = 300 */
        .rest_time       = 0,       /* No dead zone */
        .carrier_clk     = 0,       /* Not AM mode */
        .am_interval     = 0,       /* Not AM mode */
        .waveform_data   = softened_square_waveform_64  /* Edge-softened square */
    },

    /* ---- Waveform 2: Burst Train ---- */
    {
        .id              = 2,
        .name            = "Burst Train",
        .description     = "Burst pulse train at 10 Hz repetition",
        .min_current     = 0,
        .max_current     = 8,
        .frequency       = 50,       /* Carrier frequency 50 Hz */
        .pulse_width_us  = 300,
        .waveform_type   = WAVEFORM_TYPE_BURST,       /* Burst type */
        .gen_method      = GEN_METHOD_CUSTOM_SPI,      /* Use SPI custom waveform */
        .point_num       = 64,
        .half_wave_clk   = 328,     /* 32768 / (2*50) = 328 */
        .silent_time     = 600,     /* 300 * 2 = 600 */
        .rest_time       = 0,
        .carrier_clk     = 0,
        .am_interval     = 0,
        .waveform_data   = softened_burst_pulse_64  /* Edge-softened gaussian burst */
    },

    /* ---- Waveform 3: Gentle Smooth ---- */
    {
        .id              = 3,
        .name            = "Gentle Smooth",
        .description     = "Gentle smoothing symmetric square wave",
        .min_current     = 0,
        .max_current     = 8,        /* Maximum output current 8 mA */
        .frequency       = 35,       /* Lower frequency 35 Hz */
        .pulse_width_us  = 150,
        .waveform_type   = WAVEFORM_TYPE_SQUARE,     /* Square wave (edge-softened) */
        .gen_method      = GEN_METHOD_CUSTOM_SPI,      /* Custom SPI softened waveform */
        .point_num       = 64,
        .half_wave_clk   = 468,     /* 32768 / (2*35) = 468 */
        .silent_time     = 300,     /* 150 * 2 = 300 */
        .rest_time       = 0,
        .carrier_clk     = 0,
        .am_interval     = 0,
        .waveform_data   = softened_square_50pct_64  /* 50% duty cycle square */
    },

    /* ---- Waveform 4: Deep Sculpt ---- */
    {
        .id              = 4,
        .name            = "Deep Sculpt",
        .description     = "Deep sculpting with 4 kHz carrier",
        .min_current     = 0,
        .max_current     = 8,
        .frequency       = 50,       /* Envelope frequency 50 Hz */
        .pulse_width_us  = 150,      /* 4 kHz carrier half-cycle 150 us */
        .waveform_type   = WAVEFORM_TYPE_BALANCED_SQUARE, /* Balanced square wave */
        .gen_method      = GEN_METHOD_CUSTOM_SPI,
        .point_num       = 128,      /* 128 points high precision */
        .half_wave_clk   = 328,     /* 32768 / (2*50) = 328 */
        .silent_time     = 250,     /* carrier period */
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
        .max_current     = 8,
        .frequency       = 40,       /* Sine wave frequency 40 Hz */
        .pulse_width_us  = 0,        /* Sine wave has no pulse width */
        .waveform_type   = WAVEFORM_TYPE_SINE,          /* Sine wave */
        .gen_method      = GEN_METHOD_CUSTOM_SPI,
        .point_num       = 128,      /* 128 points high precision sine */
        .half_wave_clk   = 410,     /* 32768 / (2*40) = 410 */
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
        .max_current     = 8,
        .frequency       = 10,       /* Envelope frequency 10 Hz */
        .pulse_width_us  = 0,
        .waveform_type   = WAVEFORM_TYPE_BALANCED_SINE,
        .gen_method      = GEN_METHOD_CUSTOM_SPI,       /* Changed from AMPLITUDE_MOD */
        .point_num       = 64,
        .half_wave_clk   = 102,     /* 2048 / (2*10) = 102 (PCLK/16) */
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
        .max_current     = 8,
        .frequency       = 100,      /* High frequency 100 Hz */
        .pulse_width_us  = 400,      /* Pulse width 400 us */
        .waveform_type   = WAVEFORM_TYPE_TRIANGLE,       /* Triangle wave (edge-softened) */
        .gen_method      = GEN_METHOD_CUSTOM_SPI,          /* Custom SPI softened waveform */
        .point_num       = 64,
        .half_wave_clk   = 164,     /* 32768 / (2*100) = 164 */
        .silent_time     = 800,     /* 400 * 2 = 800 */
        .rest_time       = 0,
        .carrier_clk     = 0,
        .am_interval     = 0,
        .waveform_data   = softened_triangle_waveform_64  /* Edge-softened triangle */
    },

    /* ---- Waveform 8: Lymphatic Drainage ---- */
    /* NOTE: Uses PCLK_DIV_16 (AWG_CLK=2048Hz) */
    {
        .id              = 8,
        .name            = "Lymphatic Drainage",
        .description     = "Low-frequency sine for lymphatic drainage",
        .min_current     = 0,
        .max_current     = 8,
        .frequency       = 5,        /* Very low frequency 5 Hz */
        .pulse_width_us  = 450,
        .waveform_type   = WAVEFORM_TYPE_SINE,
        .gen_method      = GEN_METHOD_CUSTOM_SPI,
        .point_num       = 64,
        .half_wave_clk   = 205,     /* 2048 / (2*5) = 205 (PCLK/16) */
        .silent_time     = 56,      /* PCLK/16 adjusted */
        .rest_time       = 0,
        .carrier_clk     = 0,
        .am_interval     = 0,
        .waveform_data   = normalized_sine_waveform_64   /* 64-point sine */
    },

    /* ---- Waveform 9: Soothing Ending ---- */
    {
        .id              = 9,
        .name            = "Soothing Ending",
        .description     = "Soothing ending sine wave",
        .min_current     = 0,
        .max_current     = 8,
        .frequency       = 1000,     /* 1000 Hz */
        .pulse_width_us  = 0,        /* No pulse width */
        .waveform_type   = WAVEFORM_TYPE_SINE,
        .gen_method      = GEN_METHOD_CUSTOM_SPI,         /* Custom SPI sine */
        .point_num       = 64,
        .half_wave_clk   = 16,      /* 32768 / (2*1000) = 16 */
        .silent_time     = 0,       /* Continuous sine wave, no silent period */
        .rest_time       = 0,
        .carrier_clk     = 0,
        .am_interval     = 0,
        .waveform_data   = normalized_sine_waveform_64    /* 64-point sine */
    }
};

/* ============================================================================
 *  Current level lookup table
 *  9 waveforms × 11 levels (0~10), unit: μA
 *  Level 0 = 0 μA (off), Level 10 = 8000 μA (max)
 *  Each waveform uses a different distribution curve:
 *    WF1: linear, WF2: sqrt, WF3: exponential,
 *    WF4: S-curve, WF5: reverse-S, WF6: front-steep logarithmic,
 *    WF7: quadratic, WF8: cubic, WF9: parabolic
 * ===========================================================================*/

const uint32_t g_current_level_map[WAVEFORM_COUNT][WAVEFORM_LEVEL_COUNT] = {
    /* Waveform 1: Power Smooth — Linear */
    { 0, 800, 1600, 2400, 3200, 4000, 4800, 5600, 6400, 7200, 8000 },

    /* Waveform 2: Burst Train — Sqrt (front-steep, back-gradual) */
    { 0, 2530, 3578, 4382, 5060, 5657, 6197, 6693, 7155, 7590, 8000 },

    /* Waveform 3: Gentle Smooth — Exponential (front-gradual, back-steep) */
    { 0, 89, 198, 439, 975, 2164, 3429, 5001, 6416, 7488, 8000 },

    /* Waveform 4: Deep Sculpt — S-curve (slow start, fast middle, slow end) */
    { 0, 237, 1054, 2532, 4000, 5258, 6304, 7126, 7736, 7970, 8000 },

    /* Waveform 5: Soft Sculpt — Reverse S (fast start, slow middle, fast end) */
    { 0, 1636, 2928, 3872, 4486, 4742, 5028, 5718, 6720, 7644, 8000 },

    /* Waveform 6: Circulation Sculpt — Front-steep logarithmic */
    { 0, 3326, 4662, 5578, 6276, 6840, 7308, 7620, 7830, 7940, 8000 },

    /* Waveform 7: Smooth & Firm — Quadratic (front-gradual, back-steep) */
    { 0, 80, 320, 720, 1280, 2000, 2880, 3920, 5120, 6480, 8000 },

    /* Waveform 8: Lymphatic Drainage — Cubic (extremely gentle start, steep end) */
    { 0, 500, 700, 900, 1100, 1300, 1500, 1700, 1900, 2100, 2300 },

    /* Waveform 9: Soothing Ending — Parabolic (mid-peak, symmetric taper) */
    { 0, 1536, 2944, 4224, 5376, 6400, 7296, 7552, 7744, 7904, 8000 }
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
 * @brief Calculate actual output current from waveform ID and percentage (档位查表)
 *
 * percent 0~100 映射为档位 0~10，查 g_current_level_map 表获取电流值。
 *
 * @param[in] waveform_id Waveform ID (1~WAVEFORM_COUNT)
 * @param[in] percent     Current percentage (0~100)
 * @return Actual output current (μA), returns 0 for invalid parameters
 *
 * @see waveform_apply()
 */
uint32_t waveform_calc_current(uint8_t waveform_id, uint8_t percent)
{
    if (waveform_id < 1 || waveform_id > WAVEFORM_COUNT) return 0;
    if (percent > 100) percent = 100;

    /* percent 0~100 → level 0~10 */
    uint8_t level = (percent * (WAVEFORM_LEVEL_COUNT - 1) + 50) / 100;
    if (level >= WAVEFORM_LEVEL_COUNT) level = WAVEFORM_LEVEL_COUNT - 1;

    return g_current_level_map[waveform_id - 1][level];
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
                rt_kprintf("[WF] id=%d pts=%d current=%u wave[0..3]=%.2f,%.2f,%.2f,%.2f\n",
                    waveform_id, cfg->point_num, actual_current,
                    cfg->waveform_data[0], cfg->waveform_data[1],
                    cfg->waveform_data[2], cfg->waveform_data[3]);
                /* Low-freq waveforms need reduced PCLK to fit 16-bit register */
                if (waveform_id == 8) {
                    set_pclk_divider(chip_id, PCLK_DIV_16);
                } else if (waveform_id == 6) {
                    set_pclk_divider(chip_id, PCLK_DIV_16);  /* 125kHz for low-freq waveforms */
                }

                nnc6521_customized_waveform(chip_id, channel,
                                            cfg->point_num,
                                            cfg->waveform_data,
                                            actual_current,
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
                                             actual_current,
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
                                             actual_current);
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
