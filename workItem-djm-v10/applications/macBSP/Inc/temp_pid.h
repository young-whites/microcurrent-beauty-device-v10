/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06-29     auto-gen     PID temperature controller for DJM-V10
 *
 * Description:
 *   Independent PID temperature control for large and small handles.
 *   Each handle has its own NTC sensor, PID instance, and heating GPIO.
 *
 * Hardware:
 *   - Large handle heater: PC11 (LARGE_HAND_TEMP_CTRL)
 *   - Small handle heater: PC12 (SMALL_HAND_TEMP_CTRL)
 *   - Control method: GPIO on/off with software PWM (duty cycle)
 *
 * PID Tuning (default for NTC heater system):
 *   - Kp = 8.0   (proportional gain)
 *   - Ki = 0.15  (integral gain, per 100ms)
 *   - Kd = 3.0   (derivative gain)
 *   - Output: 0~100% duty cycle
 *   - Control period: 100ms
 *   - PWM period: 1000ms (10 ticks of 100ms)
 */

#ifndef APPLICATIONS_MACBSP_INC_TEMP_PID_H_
#define APPLICATIONS_MACBSP_INC_TEMP_PID_H_

#include <rtthread.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 *  PID Controller Configuration
 * ===========================================================================*/
#define TEMP_PID_KP             8.0f        /* Proportional gain */
#define TEMP_PID_KI             0.15f       /* Integral gain (per control period) */
#define TEMP_PID_KD             3.0f        /* Derivative gain */
#define TEMP_PID_OUT_MIN        0           /* Minimum output (0%) */
#define TEMP_PID_OUT_MAX        100         /* Maximum output (100%) */
#define TEMP_PID_I_MAX          80.0f       /* Integral anti-windup clamp */
#define TEMP_PID_I_MIN          -10.0f      /* Integral anti-windup lower clamp */
#define TEMP_PID_DEADBAND       0.3f        /* Dead band (Celsius) - avoid chatter */
#define TEMP_PID_CTRL_PERIOD    10          /* Control period in 10ms ticks (= 100ms) */
#define TEMP_PID_PWM_PERIOD     10          /* PWM period in control ticks (= 1000ms) */

/* ============================================================================
 *  Temperature Limits
 * ===========================================================================*/
#define TEMP_MIN_CELSIUS        0           /* Minimum target temperature */
#define TEMP_MAX_CELSIUS        41          /* Maximum target temperature (safety) */
#define TEMP_OVERHEAT_CELSIUS   45          /* Overheat protection threshold */
#define TEMP_SENSOR_ERROR_LOW   -30.0f      /* Sensor error: below this = fault */
#define TEMP_SENSOR_ERROR_HIGH  100.0f      /* Sensor error: above this = fault */

/* ============================================================================
 *  PID Instance Index
 * ===========================================================================*/
#define TEMP_PID_LARGE          0           /* Large handle PID (NTC_CH_LARGE) */
#define TEMP_PID_SMALL          1           /* Small handle PID (NTC_CH_SMALL) */
#define TEMP_PID_COUNT          2           /* Total PID instances */

/* ============================================================================
 *  PID Data Structure
 * ===========================================================================*/
typedef struct {
    /* PID parameters */
    float kp;                   /* Proportional gain */
    float ki;                   /* Integral gain */
    float kd;                   /* Derivative gain */

    /* PID state */
    float integral;             /* Accumulated integral term */
    float prev_error;           /* Previous error for derivative calculation */
    float prev_measurement;     /* Previous measurement for derivative-on-measurement */

    /* Setpoint and output */
    float target_temp;          /* Target temperature (Celsius) */
    float current_temp;         /* Current measured temperature (Celsius) */
    float output;               /* PID output (0~100, duty cycle percentage) */

    /* Software PWM */
    uint8_t pwm_counter;        /* PWM tick counter (0 ~ PWM_PERIOD-1) */
    uint8_t pwm_duty;           /* PWM duty (0 ~ PWM_PERIOD) */
    uint8_t heater_on;          /* Current heater GPIO state */

    /* Control state */
    uint8_t enabled;            /* PID controller enabled flag */
    uint8_t tick_divider;       /* Tick divider for control period */
    uint8_t sensor_fault;       /* Sensor fault flag (1 = fault detected) */
    uint32_t fault_count;       /* Consecutive fault reading count */
} temp_pid_t;

/* ============================================================================
 *  Public Functions
 * ===========================================================================*/

/**
 * @brief  Initialize temperature PID controller module.
 *         Sets up default PID parameters for both handles.
 * @return RT_EOK on success.
 */
int temp_pid_init(void);

/**
 * @brief  Set target temperature for a handle.
 * @param  pid_idx    TEMP_PID_LARGE or TEMP_PID_SMALL.
 * @param  temp_c     Target temperature in Celsius (0~41).
 *                    0 = disable heating (turn off PID).
 */
void temp_pid_set_target(uint8_t pid_idx, float temp_c);

/**
 * @brief  Get target temperature for a handle.
 * @param  pid_idx    TEMP_PID_LARGE or TEMP_PID_SMALL.
 * @return Target temperature in Celsius.
 */
float temp_pid_get_target(uint8_t pid_idx);

/**
 * @brief  Enable or disable a PID controller.
 * @param  pid_idx    TEMP_PID_LARGE or TEMP_PID_SMALL.
 * @param  enable     1=enable, 0=disable.
 */
void temp_pid_set_enable(uint8_t pid_idx, uint8_t enable);

/**
 * @brief  Check if a PID controller is enabled.
 * @param  pid_idx    TEMP_PID_LARGE or TEMP_PID_SMALL.
 * @return 1 if enabled, 0 if disabled.
 */
uint8_t temp_pid_is_enabled(uint8_t pid_idx);

/**
 * @brief  Get current PID output (duty cycle percentage).
 * @param  pid_idx    TEMP_PID_LARGE or TEMP_PID_SMALL.
 * @return Output duty cycle (0~100%).
 */
float temp_pid_get_output(uint8_t pid_idx);

/**
 * @brief  Check sensor fault status.
 * @param  pid_idx    TEMP_PID_LARGE or TEMP_PID_SMALL.
 * @return 1 if sensor fault detected, 0 if OK.
 */
uint8_t temp_pid_sensor_fault(uint8_t pid_idx);

/**
 * @brief  PID control tick - called every 10ms from system timer.
 *         Handles control period division, PID computation, and PWM output.
 *         This is the main entry point for the temperature control loop.
 */
void temp_pid_tick(void);

/**
 * @brief  Get PID instance data for debugging.
 * @param  pid_idx    TEMP_PID_LARGE or TEMP_PID_SMALL.
 * @return Pointer to temp_pid_t structure.
 */
temp_pid_t *temp_pid_get(uint8_t pid_idx);

/**
 * @brief  Reset PID state (integral, derivative history).
 * @param  pid_idx    TEMP_PID_LARGE or TEMP_PID_SMALL.
 */
void temp_pid_reset(uint8_t pid_idx);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_MACBSP_INC_TEMP_PID_H_ */
