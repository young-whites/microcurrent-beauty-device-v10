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
        .max_current     = 8,        /* Maximum output current 8 mA */
        .frequency       = 50,       /* Waveform frequency 50 Hz */
        .pulse_width_us  = 300,      /* Pulse width 300 us */
        .waveform_type   = WAVEFORM_TYPE_SQUARE,     /* Square wave (edge-softened) */
        .gen_method      = GEN_METHOD_CUSTOM_SPI,      /* Custom SPI softened waveform */
        .point_num       = 64,       /* 64 points */
        .half_wave_clk   = 20000,   /* 2000000 / (2*50) = 20000 */
        .silent_time     = 600,     /* 300 * 2 = 600 */
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
        .half_wave_clk   = 20000,   /* 2000000 / (2*50) = 20000 */
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
        .pulse_width_us  = 300,
        .waveform_type   = WAVEFORM_TYPE_SQUARE,     /* Square wave (edge-softened) */
        .gen_method      = GEN_METHOD_CUSTOM_SPI,      /* Custom SPI softened waveform */
        .point_num       = 64,
        .half_wave_clk   = 28571,   /* 2000000 / (2*35) = 28571 */
        .silent_time     = 600,     /* 300 * 2 = 600 */
        .rest_time       = 0,
        .carrier_clk     = 0,
        .am_interval     = 0,
        .waveform_data   = softened_square_waveform_64  /* Edge-softened square */
    },

    /* ---- Waveform 4: Deep Sculpt ---- */
    {
        .id              = 4,
        .name            = "Deep Sculpt",
        .description     = "Deep sculpting with 4 kHz carrier",
        .min_current     = 0,
        .max_current     = 8,
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
        .max_current     = 8,
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
        .max_current     = 8,
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
        .max_current     = 8,
        .frequency       = 100,      /* High frequency 100 Hz */
        .pulse_width_us  = 400,      /* Pulse width 400 us */
        .waveform_type   = WAVEFORM_TYPE_TRIANGLE,       /* Triangle wave (edge-softened) */
        .gen_method      = GEN_METHOD_CUSTOM_SPI,          /* Custom SPI softened waveform */
        .point_num       = 64,
        .half_wave_clk   = 10000,   /* 2000000 / (2*100) = 10000 */
        .silent_time     = 800,     /* 400 * 2 = 800 */
        .rest_time       = 0,
        .carrier_clk     = 0,
        .am_interval     = 0,
        .waveform_data   = softened_triangle_waveform_64  /* Edge-softened triangle */
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
        .max_current     = 8,
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
        .max_current     = 8,
        .frequency       = 1000,     /* 1000 Hz */
        .pulse_width_us  = 0,        /* No pulse width */
        .waveform_type   = WAVEFORM_TYPE_SINE,
        .gen_method      = GEN_METHOD_CUSTOM_SPI,         /* Custom SPI sine */
        .point_num       = 64,
        .half_wave_clk   = 17,      /* ~1kHz: 2124800/(2*17*64) = 975Hz */
        .silent_time     = 0,       /* Continuous sine wave, no silent period */
        .rest_time       = 0,
        .carrier_clk     = 0,
        .am_interval     = 0,
        .waveform_data   = normalized_sine_waveform_64    /* 64-point sine */
    }
};

/* ============================================================================
 *  Current level lookup table
 *  9 waveforms × 81 levels (0~80), unit: μA
 *  Level 0 = 0 μA (off), Level 80 = 8000 μA (max)
 *  Step = 100 μA per level
 * ===========================================================================*/

const uint32_t g_current_level_map[WAVEFORM_COUNT][WAVEFORM_LEVEL_COUNT] = {
    /* Waveform 1: Power Smooth — 线性递增 */
    { 0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800, 2900, 3000, 3100, 3200, 3300, 3400, 3500, 3600, 3700, 3800, 3900, 4000, 4100, 4200, 4300, 4400, 4500, 4600, 4700, 4800, 4900, 5000, 5100, 5200, 5300, 5400, 5500, 5600, 5700, 5800, 5900, 6000, 6100, 6200, 6300, 6400, 6500, 6600, 6700, 6800, 6900, 7000, 7100, 7200, 7300, 7400, 7500, 7600, 7700, 7800, 7900, 8000 },

    /* Waveform 2: Burst Train — 低档陡、高档缓 */
    { 0, 372, 605, 803, 983, 1149, 1305, 1454, 1596, 1733, 1866, 1995, 2120, 2242, 2362, 2479, 2593, 2705, 2816, 2925, 3031, 3137, 3241, 3343, 3444, 3544, 3643, 3740, 3837, 3932, 4026, 4120, 4212, 4304, 4395, 4485, 4574, 4663, 4751, 4838, 4925, 5010, 5096, 5180, 5264, 5348, 5431, 5513, 5595, 5676, 5757, 5837, 5917, 5997, 6076, 6154, 6232, 6310, 6387, 6464, 6541, 6617, 6693, 6768, 6843, 6918, 6992, 7066, 7140, 7213, 7286, 7359, 7431, 7503, 7575, 7647, 7718, 7789, 7859, 7930, 8000 },

    /* Waveform 3: Gentle Smooth — 对数（低档柔和） */
    { 0, 170, 336, 500, 659, 816, 970, 1121, 1269, 1414, 1557, 1697, 1835, 1970, 2103, 2234, 2363, 2490, 2615, 2738, 2859, 2978, 3096, 3212, 3326, 3438, 3549, 3659, 3767, 3874, 3979, 4083, 4185, 4286, 4386, 4485, 4582, 4679, 4774, 4868, 4961, 5053, 5144, 5233, 5322, 5410, 5497, 5583, 5668, 5752, 5836, 5918, 6000, 6080, 6160, 6239, 6318, 6395, 6472, 6548, 6624, 6699, 6773, 6846, 6919, 6991, 7062, 7133, 7203, 7273, 7342, 7410, 7478, 7545, 7611, 7678, 7743, 7808, 7873, 7937, 8000 },

    /* Waveform 4: Deep Sculpt — 前陡后平 */
    { 0, 3, 10, 22, 36, 54, 76, 100, 127, 157, 189, 225, 263, 304, 347, 393, 442, 492, 546, 602, 660, 720, 783, 848, 916, 986, 1058, 1132, 1209, 1288, 1369, 1452, 1537, 1625, 1715, 1807, 1901, 1997, 2095, 2195, 2297, 2402, 2508, 2617, 2727, 2840, 2955, 3071, 3190, 3310, 3433, 3558, 3684, 3813, 3943, 4075, 4210, 4346, 4484, 4624, 4767, 4910, 5056, 5204, 5354, 5505, 5659, 5814, 5971, 6130, 6291, 6453, 6618, 6784, 6953, 7123, 7294, 7468, 7644, 7821, 8000 },

    /* Waveform 5: Soft Sculpt — S 曲线（中间档变化大） */
    { 0, 7, 15, 24, 35, 47, 60, 75, 92, 111, 132, 156, 183, 214, 248, 287, 330, 379, 433, 493, 561, 636, 719, 811, 912, 1024, 1146, 1280, 1425, 1582, 1752, 1933, 2126, 2331, 2547, 2773, 3007, 3249, 3496, 3747, 4000, 4253, 4504, 4751, 4993, 5227, 5453, 5669, 5874, 6067, 6248, 6418, 6575, 6720, 6854, 6976, 7088, 7189, 7281, 7364, 7439, 7507, 7567, 7621, 7670, 7713, 7752, 7786, 7817, 7844, 7868, 7889, 7908, 7925, 7940, 7953, 7965, 7976, 7985, 7993, 8000 },

    /* Waveform 6: Circulation Sculpt — 缓起快升 */
    { 0, 1386, 1829, 2151, 2414, 2639, 2839, 3019, 3185, 3338, 3482, 3618, 3746, 3868, 3984, 4095, 4202, 4306, 4405, 4501, 4595, 4685, 4773, 4859, 4942, 5024, 5103, 5181, 5257, 5331, 5404, 5475, 5545, 5614, 5681, 5748, 5813, 5877, 5940, 6002, 6063, 6123, 6182, 6241, 6298, 6355, 6411, 6467, 6522, 6576, 6629, 6682, 6734, 6785, 6836, 6887, 6936, 6986, 7034, 7083, 7130, 7178, 7225, 7271, 7317, 7362, 7407, 7452, 7496, 7540, 7584, 7627, 7670, 7712, 7754, 7796, 7838, 7879, 7919, 7960, 8000 },

    /* Waveform 7: Smooth & Firm — 均匀递增 */
    { 0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800, 2900, 3000, 3100, 3200, 3300, 3400, 3500, 3600, 3700, 3800, 3900, 4000, 4100, 4200, 4300, 4400, 4500, 4600, 4700, 4800, 4900, 5000, 5100, 5200, 5300, 5400, 5500, 5600, 5700, 5800, 5900, 6000, 6100, 6200, 6300, 6400, 6500, 6600, 6700, 6800, 6900, 7000, 7100, 7200, 7300, 7400, 7500, 7600, 7700, 7800, 7900, 8000 },

    /* Waveform 8: Lymphatic Drainage — 极低档起步、缓慢递增 */
    { 0, 0, 1, 2, 4, 8, 12, 18, 25, 34, 44, 56, 70, 85, 102, 122, 143, 167, 192, 220, 250, 282, 317, 355, 394, 437, 482, 529, 580, 633, 689, 748, 810, 874, 942, 1013, 1087, 1164, 1244, 1327, 1414, 1504, 1598, 1694, 1795, 1898, 2006, 2116, 2231, 2349, 2471, 2596, 2725, 2858, 2995, 3135, 3280, 3428, 3580, 3737, 3897, 4062, 4230, 4403, 4579, 4760, 4946, 5135, 5329, 5527, 5729, 5936, 6147, 6363, 6583, 6808, 7037, 7271, 7509, 7752, 8000 },

    /* Waveform 9: Soothing Ending — 反 S 曲线（快起慢收） */
    { 0, 2149, 2645, 2987, 3257, 3482, 3678, 3852, 4009, 4154, 4287, 4411, 4528, 4638, 4742, 4842, 4936, 5027, 5114, 5197, 5278, 5356, 5431, 5504, 5575, 5643, 5710, 5775, 5839, 5900, 5961, 6020, 6077, 6134, 6189, 6243, 6296, 6348, 6399, 6449, 6498, 6546, 6594, 6641, 6686, 6732, 6776, 6820, 6863, 6906, 6948, 6989, 7030, 7070, 7110, 7149, 7188, 7226, 7264, 7302, 7339, 7375, 7411, 7447, 7482, 7517, 7551, 7586, 7619, 7653, 7686, 7719, 7751, 7783, 7815, 7847, 7878, 7909, 7939, 7970, 8000 }
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
 * percent 0~100 映射为档位 0~80，查 g_current_level_map 表获取电流值。
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

    /* percent 0~100 → level 0~80 */
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
