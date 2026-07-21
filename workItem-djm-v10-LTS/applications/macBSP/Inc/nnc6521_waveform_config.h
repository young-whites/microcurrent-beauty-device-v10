/**
  ******************************************************************************
  * @file    nnc6521_waveform_config.h
  * @brief   NNC6521 waveform configuration module header (DJM-V10 beauty device)
  *          Defines 9 preset waveforms with current mapping and NNC6521 driver API
  *          integration.
  ******************************************************************************
  */

#ifndef __NNC6521_WAVEFORM_CONFIG_H__
#define __NNC6521_WAVEFORM_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Preset waveform count ------------------------------------------------------*/
#define WAVEFORM_COUNT          9       /**< Total preset waveforms (ID 1~9) */
#define WAVEFORM_LEVEL_COUNT    11      /**< 0~10 共 11 个档位 */

/* Default current percentage -------------------------------------------------*/
#define WAVEFORM_DEFAULT_PCT    50      /**< Default current percentage (0~100) */

/**
 * @brief Waveform generation method enumeration
 */
typedef enum {
    GEN_METHOD_PRELOADED = 0,       /**< NNC6521 built-in preloaded waveform (sine/pulse/triangle) */
    GEN_METHOD_CUSTOM_SPI = 1,      /**< Custom SPI waveform array */
    GEN_METHOD_AMPLITUDE_MOD = 2    /**< Amplitude modulation (envelope + carrier) */
} waveform_gen_method_t;

/**
 * @brief Waveform type identifier enumeration
 */
typedef enum {
    WAVEFORM_TYPE_SQUARE = 0,           /**< Symmetric square wave */
    WAVEFORM_TYPE_BURST = 1,            /**< Burst pulse train */
    WAVEFORM_TYPE_TRIANGLE = 2,         /**< Triangle wave */
    WAVEFORM_TYPE_SINE = 3,             /**< Sine wave */
    WAVEFORM_TYPE_BALANCED_SQUARE = 4,  /**< Balanced square wave (carrier mode) */
    WAVEFORM_TYPE_BALANCED_SINE = 5     /**< Balanced sine wave (carrier mode) */
} waveform_type_t;

/**
 * @brief Waveform configuration structure
 *
 * Describes all parameters of a preset waveform, including current range,
 * frequency, pulse width, generation method, etc.
 * Can be directly applied to an NNC6521 chip via waveform_apply().
 */
typedef struct {
    uint8_t     id;              /**< Waveform ID (1~9) */
    const char *name;            /**< Waveform name string */
    const char *description;     /**< Brief waveform description */
    uint32_t    min_current;     /**< Minimum output current (mA) */
    uint32_t    max_current;     /**< Maximum output current (mA) */
    uint16_t    frequency;       /**< Main frequency (Hz) */
    uint16_t    pulse_width_us;  /**< Pulse width (microseconds) */
    uint8_t     waveform_type;   /**< Waveform type (waveform_type_t) */
    uint8_t     gen_method;      /**< Generation method (waveform_gen_method_t) */
    uint8_t     point_num;       /**< Samples per cycle (64 or 128) */
    uint16_t    half_wave_clk;   /**< Half-wave clock cycles (PCLK / (2 * freq)) */
    uint32_t    silent_time;     /**< Silent time (clock cycles) */
    uint16_t    rest_time;       /**< Rest time (clock cycles) */
    uint16_t    carrier_clk;     /**< Carrier half-wave clocks (AM mode) */
    uint16_t    am_interval;     /**< Envelope interval clocks (AM mode) */
    float      *waveform_data;   /**< Normalized waveform array pointer (NULL for preloaded) */
} waveform_config_t;

/* Global waveform config array -----------------------------------------------*/
extern const waveform_config_t g_waveform_configs[WAVEFORM_COUNT];

/* 电流档位映射表：每行对应一种波形（ID 1~9），每列对应档位 0~10，单位 mA */
extern const uint32_t g_current_level_map[WAVEFORM_COUNT][WAVEFORM_LEVEL_COUNT];

/* ============================================================================
 *  Public API
 * ===========================================================================*/

/**
 * @brief Apply a waveform to NNC6521 by waveform ID and current percentage
 *
 * Looks up the waveform configuration by ID, calculates actual current,
 * then calls the corresponding low-level driver function to output the waveform.
 * Supports three generation methods: preloaded, custom SPI, amplitude modulation.
 *
 * @param[in] chip_id      Chip ID (NNC6521_CHIP_1 or NNC6521_CHIP_2)
 * @param[in] channel      Waveform channel (WAVEFORM_GEN_CH0 or WAVEFORM_GEN_CH1)
 * @param[in] waveform_id  Waveform ID (1~9)
 * @param[in] percent      Current percentage (0~100)
 *
 * @note Returns immediately without action if waveform_id is out of range
 * @note Percent is clamped to 0~100 range
 *
 * @see waveform_calc_current, waveform_get_config
 */
void waveform_apply(uint8_t chip_id, uint8_t channel,
                    uint8_t waveform_id, uint8_t percent);

/**
 * @brief Calculate actual output current from waveform ID and percentage (档位查表)
 *
 * percent 0~100 内部映射为档位 0~10，然后查 g_current_level_map 表获取电流值。
 *
 * @param[in] waveform_id  Waveform ID (1~9)
 * @param[in] percent      Current percentage (0~100)
 *
 * @return Actual current value (mA), returns 0 for invalid ID
 *
 * @see waveform_apply
 */
uint32_t waveform_calc_current(uint8_t waveform_id, uint8_t percent);

/**
 * @brief Get configuration structure pointer by waveform ID
 *
 * @param[in] waveform_id  Waveform ID (1~9)
 *
 * @return Pointer to const waveform config, returns NULL for invalid ID
 *
 * @see waveform_apply
 */
const waveform_config_t* waveform_get_config(uint8_t waveform_id);

/**
 * @brief Update waveform amplitude only (no timing reconfiguration)
 *
 * More efficient than waveform_apply() when only the current percentage
 * changes during treatment. Updates waveform data or CI register directly.
 *
 * @param[in] chip_id     Chip ID
 * @param[in] channel     Waveform channel
 * @param[in] waveform_id Waveform ID (1~9)
 * @param[in] percent     New current percentage (0~100)
 *
 * @note Waveform must already be configured via waveform_apply() first.
 */
void waveform_update_amplitude(uint8_t chip_id, uint8_t channel,
                               uint8_t waveform_id, uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif /* __NNC6521_WAVEFORM_CONFIG_H__ */
