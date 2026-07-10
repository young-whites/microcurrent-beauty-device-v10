/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-07-08     Administrator       the first version
 * 2026-07-09     auto-gen     Removed LED/beep/button functions (now handled by bsp_led/bsp_beep/bsp_key drivers)
 */
#include "bsp_hard.h"

/* ============================================================================
 *  Power Management
 * ===========================================================================*/

/**
 * @brief  Control 12V system power supply.
 * @param  on  1=enable, 0=disable.
 */
void bsp_power_enable(uint8_t on)
{
    HAL_GPIO_WritePin(SYSTEM_POWER_CTRL_GPIO_Port, SYSTEM_POWER_CTRL_Pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ============================================================================
 *  54V Boost Converter
 * ===========================================================================*/

/**
 * @brief  Enable/disable 54V boost converter #1.
 * @param  on  1=enable, 0=disable.
 */
void bsp_boost_1_enable(uint8_t on)
{
    HAL_GPIO_WritePin(LGS6302EP_1_EN_GPIO_Port, LGS6302EP_1_EN_Pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief  Enable/disable 54V boost converter #2.
 * @param  on  1=enable, 0=disable.
 */
void bsp_boost_2_enable(uint8_t on)
{
    HAL_GPIO_WritePin(LGS6302EP_2_EN_GPIO_Port, LGS6302EP_2_EN_Pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ============================================================================
 *  Pump Control
 * ===========================================================================*/

/**
 * @brief  Control vacuum pump.
 * @param  on  1=enable, 0=disable.
 */
void bsp_pump_set(uint8_t on)
{
    HAL_GPIO_WritePin(VACUUM_PUMP_CTRL_GPIO_Port, VACUUM_PUMP_CTRL_Pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ============================================================================
 *  Heating Control
 * ===========================================================================*/

/**
 * @brief  Control large handle heater.
 * @param  on  1=enable, 0=disable.
 */
void bsp_heater_large_set(uint8_t on)
{
    HAL_GPIO_WritePin(LARGE_HAND_TEMP_CTRL_GPIO_Port, LARGE_HAND_TEMP_CTRL_Pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief  Control small handle heater.
 * @param  on  1=enable, 0=disable.
 */
void bsp_heater_small_set(uint8_t on)
{
    HAL_GPIO_WritePin(SMALL_HAND_TEMP_CTRL_GPIO_Port, SMALL_HAND_TEMP_CTRL_Pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
