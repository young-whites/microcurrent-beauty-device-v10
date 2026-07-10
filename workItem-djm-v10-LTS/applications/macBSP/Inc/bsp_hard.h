/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-07-08     Administrator       the first version
 * 2026-07-09     auto-gen     Added GPIO abstraction layer
 */
#ifndef APPLICATIONS_MACBSP_INC_BSP_HARD_H_
#define APPLICATIONS_MACBSP_INC_BSP_HARD_H_

#include "bsp_sys.h"

/* Power supply control */
void bsp_power_enable(uint8_t on);          // PA4: 12V system power (HIGH=on)

/* 54V boost converter */
void bsp_boost_1_enable(uint8_t on);        // PB0: LGS6302EP-1 (HIGH=enable)
void bsp_boost_2_enable(uint8_t on);        // PB1: LGS6302EP-2 (HIGH=enable)

/* Pump control */
void bsp_pump_set(uint8_t on);              // PB10: vacuum pump (HIGH=enable)

/* Heating control */
void bsp_heater_large_set(uint8_t on);      // PC11: large handle heater
void bsp_heater_small_set(uint8_t on);      // PC10: small handle heater

#endif
