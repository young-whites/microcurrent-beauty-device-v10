/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-07-08     Administrator       the first version
 * 2026-07-09     auto-gen     Added GPIO abstraction implementations
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

/**
 * @brief  Read power button state.
 * @return 1 if button is pressed (LOW), 0 if released (HIGH).
 */
uint8_t bsp_power_button_read(void)
{
    return (HAL_GPIO_ReadPin(POWER_ON_OFF_GPIO_Port, POWER_ON_OFF_Pin) == GPIO_PIN_RESET) ? 1 : 0;
}

/**
 * @brief  Control system LED.
 * @param  on  1=on, 0=off.
 */
void bsp_led_set(uint8_t on)
{
    HAL_GPIO_WritePin(START_LED_CTRL_GPIO_Port, START_LED_CTRL_Pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief  Control buzzer.
 * @param  on  1=on, 0=off.
 */
void bsp_beep_set(uint8_t on)
{
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin,
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
