/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-06-29     auto-gen     PID temperature controller for DJM-V10
 *
 * Algorithm Overview:
 *   1. NTC temperature readings updated every 10ms (ntc_sensor_update)
 *   2. PID control computed every 100ms (10 ticks of 10ms)
 *   3. Software PWM period = 1000ms (10 control periods)
 *   4. GPIO on/off duty cycle controls average heating power
 *
 * PID Formula (Positional form with derivative-on-measurement):
 *   error = target - current
 *   P = Kp * error
 *   I = I + Ki * error   (with anti-windup clamping)
 *   D = Kd * (prev_measurement - current)   (derivative-on-measurement to avoid kick)
 *   output = P + I + D
 *   output = clamped to [OUT_MIN, OUT_MAX]
 *
 * Safety Features:
 *   - Dead band: if |error| < 0.3C, output = 0 (avoid relay chatter)
 *   - Overheat protection: if current > 45C, force heater off
 *   - Sensor fault detection: ADC stuck or out-of-range
 *   - Integral anti-windup: clamps integral term
 *   - Output clamping: 0~100% duty cycle
 */

#include "temp_pid.h"
#include "ntc_sensor.h"
#include "main.h"       /* For GPIO pin definitions */
#include <math.h>

/* ============================================================================
 *  PID Instance Data
 * ===========================================================================*/
static temp_pid_t s_pid[TEMP_PID_COUNT];

/* GPIO port/pin mapping for heater control */
static GPIO_TypeDef * const s_heater_port[TEMP_PID_COUNT] = {
    LARGE_HAND_TEMP_CTRL_GPIO_Port,     /* PC11 - Large handle */
    SMALL_HAND_TEMP_CTRL_GPIO_Port      /* PC12 - Small handle */
};

static const uint16_t s_heater_pin[TEMP_PID_COUNT] = {
    LARGE_HAND_TEMP_CTRL_Pin,           /* GPIO_PIN_11 */
    SMALL_HAND_TEMP_CTRL_Pin            /* GPIO_PIN_12 */
};

/* NTC channel mapping: PID index -> NTC channel */
static const uint8_t s_ntc_channel[TEMP_PID_COUNT] = {
    NTC_CH_LARGE,   /* TEMP_PID_LARGE -> NTC_CH_LARGE */
    NTC_CH_SMALL    /* TEMP_PID_SMALL -> NTC_CH_SMALL */
};

/* ============================================================================
 *  Internal: GPIO Heater Control
 * ===========================================================================*/

/**
 * @brief  Set heater GPIO state.
 * @param  pid_idx  PID instance index.
 * @param  on       1=heater on, 0=heater off.
 */
static void heater_set(uint8_t pid_idx, uint8_t on)
{
    if (pid_idx >= TEMP_PID_COUNT) return;

    if (on) {
        HAL_GPIO_WritePin(s_heater_port[pid_idx], s_heater_pin[pid_idx], GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(s_heater_port[pid_idx], s_heater_pin[pid_idx], GPIO_PIN_RESET);
    }
    s_pid[pid_idx].heater_on = on;
}

/* ============================================================================
 *  Internal: Safety Checks
 * ===========================================================================*/

/**
 * @brief  Perform safety checks on temperature reading.
 * @param  pid_idx  PID instance index.
 * @return 1 if safe to operate, 0 if fault detected.
 */
static uint8_t safety_check(uint8_t pid_idx)
{
    temp_pid_t *pid = &s_pid[pid_idx];
    uint8_t ntc_ch = s_ntc_channel[pid_idx];
    float temp = ntc_sensor_get_temperature(ntc_ch);

    /* Check for sensor error values */
    if (temp < TEMP_SENSOR_ERROR_LOW || temp > TEMP_SENSOR_ERROR_HIGH) {
        pid->fault_count++;
        if (pid->fault_count > 10) {    /* 10 consecutive faults = confirmed fault */
            pid->sensor_fault = 1;
            pid->enabled = 0;
            heater_set(pid_idx, 0);
            rt_kprintf("[PID] Handle %c sensor fault! temp=%.1f\n",
                       'A' + pid_idx, temp);
        }
        return 0;
    }

    /* Overheat protection */
    if (temp > TEMP_OVERHEAT_CELSIUS) {
        pid->enabled = 0;
        heater_set(pid_idx, 0);
        rt_kprintf("[PID] Handle %c overheat! temp=%.1f > %d\n",
                   'A' + pid_idx, temp, TEMP_OVERHEAT_CELSIUS);
        return 0;
    }

    /* Reset fault counter on valid reading */
    pid->fault_count = 0;
    pid->sensor_fault = 0;
    return 1;
}

/* ============================================================================
 *  Internal: PID Computation
 * ===========================================================================*/

/**
 * @brief  Compute PID output for one control cycle.
 * @param  pid_idx  PID instance index.
 */
static void pid_compute(uint8_t pid_idx)
{
    temp_pid_t *pid = &s_pid[pid_idx];
    uint8_t ntc_ch = s_ntc_channel[pid_idx];

    /* Read current temperature from NTC sensor */
    pid->current_temp = ntc_sensor_get_temperature(ntc_ch);

    /* Safety check */
    if (!safety_check(pid_idx)) {
        pid->output = 0;
        return;
    }

    /* If target is 0, heating is disabled */
    if (pid->target_temp <= 0.0f) {
        pid->output = 0;
        pid->integral = 0;
        pid->prev_error = 0;
        pid->prev_measurement = pid->current_temp;
        return;
    }

    /* Calculate error */
    float error = pid->target_temp - pid->current_temp;

    /* Dead band: if error is small, don't adjust (avoid chatter) */
    if (fabsf(error) < TEMP_PID_DEADBAND) {
        /* Maintain current output, don't accumulate */
        pid->prev_error = error;
        pid->prev_measurement = pid->current_temp;
        return;
    }

    /* ---- Proportional term ---- */
    float p_term = pid->kp * error;

    /* ---- Integral term (with anti-windup) ---- */
    pid->integral += pid->ki * error;
    if (pid->integral > TEMP_PID_I_MAX) pid->integral = TEMP_PID_I_MAX;
    if (pid->integral < TEMP_PID_I_MIN) pid->integral = TEMP_PID_I_MIN;
    float i_term = pid->integral;

    /* ---- Derivative term (on measurement to avoid setpoint kick) ---- */
    float d_term = pid->kd * (pid->prev_measurement - pid->current_temp);

    /* ---- Compute output ---- */
    float output = p_term + i_term + d_term;

    /* Clamp output */
    if (output > TEMP_PID_OUT_MAX) output = TEMP_PID_OUT_MAX;
    if (output < TEMP_PID_OUT_MIN) output = TEMP_PID_OUT_MIN;

    /* If current temp significantly exceeds target, force output to 0 */
    if (pid->current_temp > pid->target_temp + 1.0f) {
        output = 0;
        /* Wind back integral to prevent re-overshoot */
        pid->integral *= 0.95f;
    }

    pid->output = output;

    /* Save state for next iteration */
    pid->prev_error = error;
    pid->prev_measurement = pid->current_temp;
}

/* ============================================================================
 *  Internal: Software PWM
 * ===========================================================================*/

/**
 * @brief  Software PWM handler for heater control.
 *         Called every control period (100ms). PWM period = 1000ms (10 ticks).
 *
 *   Example: output = 60%, PWM_PERIOD = 10
 *     tick 0~5: heater ON  (6 ticks)
 *     tick 6~9: heater OFF (4 ticks)
 *     => 60% average power
 *
 * @param  pid_idx  PID instance index.
 */
static void pwm_update(uint8_t pid_idx)
{
    temp_pid_t *pid = &s_pid[pid_idx];

    /* Calculate duty: map output (0~100%) to PWM ticks (0~PWM_PERIOD) */
    pid->pwm_duty = (uint8_t)((pid->output * TEMP_PID_PWM_PERIOD + 50) / 100);
    if (pid->pwm_duty > TEMP_PID_PWM_PERIOD) {
        pid->pwm_duty = TEMP_PID_PWM_PERIOD;
    }

    /* PWM: ON in first part, OFF in second part */
    if (pid->pwm_counter < pid->pwm_duty) {
        heater_set(pid_idx, 1);
    } else {
        heater_set(pid_idx, 0);
    }

    /* Advance PWM counter */
    pid->pwm_counter++;
    if (pid->pwm_counter >= TEMP_PID_PWM_PERIOD) {
        pid->pwm_counter = 0;
    }
}

/* ============================================================================
 *  Public API
 * ===========================================================================*/

int temp_pid_init(void)
{
    for (uint8_t i = 0; i < TEMP_PID_COUNT; i++) {
        /* Set default PID parameters */
        s_pid[i].kp = TEMP_PID_KP;
        s_pid[i].ki = TEMP_PID_KI;
        s_pid[i].kd = TEMP_PID_KD;

        /* Clear state */
        s_pid[i].integral = 0;
        s_pid[i].prev_error = 0;
        s_pid[i].prev_measurement = 25.0f;  /* Default room temp */

        /* Clear setpoint and output */
        s_pid[i].target_temp = 0;
        s_pid[i].current_temp = 25.0f;
        s_pid[i].output = 0;

        /* Clear PWM state */
        s_pid[i].pwm_counter = 0;
        s_pid[i].pwm_duty = 0;
        s_pid[i].heater_on = 0;

        /* Clear control state */
        s_pid[i].enabled = 0;
        s_pid[i].tick_divider = 0;
        s_pid[i].sensor_fault = 0;
        s_pid[i].fault_count = 0;

        /* Ensure heater is off */
        heater_set(i, 0);
    }

    rt_kprintf("[PID] Temperature PID controller initialized, instances=%d\n", TEMP_PID_COUNT);
    rt_kprintf("[PID] Kp=%.1f Ki=%.3f Kd=%.1f, CtrlPeriod=%dms, PWMPeriod=%dms\n",
               TEMP_PID_KP, TEMP_PID_KI, TEMP_PID_KD,
               TEMP_PID_CTRL_PERIOD * 10, TEMP_PID_PWM_PERIOD * TEMP_PID_CTRL_PERIOD * 10);
    return RT_EOK;
}

void temp_pid_set_target(uint8_t pid_idx, float temp_c)
{
    if (pid_idx >= TEMP_PID_COUNT) return;

    /* Clamp to valid range */
    if (temp_c < TEMP_MIN_CELSIUS) temp_c = TEMP_MIN_CELSIUS;
    if (temp_c > TEMP_MAX_CELSIUS) temp_c = TEMP_MAX_CELSIUS;

    temp_pid_t *pid = &s_pid[pid_idx];

    /* If target changes significantly, reset integral to prevent windup */
    if (fabsf(pid->target_temp - temp_c) > 2.0f) {
        pid->integral = 0;
    }

    pid->target_temp = temp_c;

    /* Enable/disable based on target */
    if (temp_c > 0) {
        pid->enabled = 1;
        pid->sensor_fault = 0;
        pid->fault_count = 0;
    } else {
        pid->enabled = 0;
        pid->output = 0;
        heater_set(pid_idx, 0);
    }

    rt_kprintf("[PID] Handle %c target: %.1f C, enabled=%d\n",
               'A' + pid_idx, temp_c, pid->enabled);
}

float temp_pid_get_target(uint8_t pid_idx)
{
    if (pid_idx >= TEMP_PID_COUNT) return 0;
    return s_pid[pid_idx].target_temp;
}

void temp_pid_set_enable(uint8_t pid_idx, uint8_t enable)
{
    if (pid_idx >= TEMP_PID_COUNT) return;
    s_pid[pid_idx].enabled = enable ? 1 : 0;

    if (!enable) {
        s_pid[pid_idx].output = 0;
        heater_set(pid_idx, 0);
    }
}

uint8_t temp_pid_is_enabled(uint8_t pid_idx)
{
    if (pid_idx >= TEMP_PID_COUNT) return 0;
    return s_pid[pid_idx].enabled;
}

float temp_pid_get_output(uint8_t pid_idx)
{
    if (pid_idx >= TEMP_PID_COUNT) return 0;
    return s_pid[pid_idx].output;
}

uint8_t temp_pid_sensor_fault(uint8_t pid_idx)
{
    if (pid_idx >= TEMP_PID_COUNT) return 1;
    return s_pid[pid_idx].sensor_fault;
}

temp_pid_t *temp_pid_get(uint8_t pid_idx)
{
    if (pid_idx >= TEMP_PID_COUNT) return (temp_pid_t *)0;
    return &s_pid[pid_idx];
}

void temp_pid_reset(uint8_t pid_idx)
{
    if (pid_idx >= TEMP_PID_COUNT) return;

    temp_pid_t *pid = &s_pid[pid_idx];
    pid->integral = 0;
    pid->prev_error = 0;
    pid->prev_measurement = pid->current_temp;
    pid->output = 0;
    pid->pwm_counter = 0;
    heater_set(pid_idx, 0);
}

/* ============================================================================
 *  Main PID Tick (called from 10ms system timer)
 * ===========================================================================*/

void temp_pid_tick(void)
{
    for (uint8_t i = 0; i < TEMP_PID_COUNT; i++) {
        temp_pid_t *pid = &s_pid[i];

        /* Skip if not enabled */
        if (!pid->enabled) {
            /* Ensure heater stays off when disabled */
            if (pid->heater_on) {
                heater_set(i, 0);
            }
            continue;
        }

        /* Divider: run PID control every 100ms (10 x 10ms ticks) */
        pid->tick_divider++;
        if (pid->tick_divider >= TEMP_PID_CTRL_PERIOD) {
            pid->tick_divider = 0;

            /* Compute PID output */
            pid_compute(i);

            /* Update software PWM */
            pwm_update(i);
        }
    }
}
