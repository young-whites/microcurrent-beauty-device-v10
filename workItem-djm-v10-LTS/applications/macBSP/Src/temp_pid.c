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
#include "protocol.h"      /* For protocol_send_ack, FUNC_PID_AUTOTUNE */
#include "main.h"       /* For GPIO pin definitions */
#include <math.h>

/* Using rt_kprintf for VOFA+ debug output (console on uart2) */

/* VOFA+ FireWater protocol: target,current,output,p,i,d\n */
/* WARNING: HAL_UART_Transmit is blocking. Set VOFA_ENABLE=0 for production! */
#define VOFA_ENABLE  1

#if VOFA_ENABLE
static void vofa_output(uint8_t pid_idx, float p_term, float i_term, float d_term)
{
    temp_pid_t *pid = temp_pid_get(pid_idx);
    if (pid == RT_NULL) return;

    /* Only output the active (enabled) PID, skip inactive */
    if (!pid->enabled) return;

    rt_kprintf("%d:%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\r\n",
               pid_idx,
               pid->target_temp,
               pid->current_temp,
               pid->output,
               p_term, i_term, d_term);
}
#endif
#include <rtthread.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ============================================================================
 *  Auto-Tune State Structure
 * ===========================================================================*/
typedef struct {
    pid_mode_t mode;                /* Current operating mode */
    float target_temp;              /* Oscillation center temperature */
    float hysteresis;               /* Relay hysteresis (half-amplitude) */
    uint8_t cycle_count;            /* Completed oscillation cycles */
    uint8_t heater_state;           /* Relay output: 0=off, 1=on */
    uint32_t cycle_start_tick;      /* Tick at last zero-crossing */
    uint32_t period_sum;            /* Sum of measured periods (in ticks) */
    float temp_high;                /* Peak temperature in current cycle */
    float temp_low;                 /* Trough temperature in current cycle */
    float amplitude_sum;            /* Sum of measured amplitudes */
    uint8_t rising;                 /* 1 if temperature is rising */
    uint8_t complete;               /* 1 if tuning is finished */
    uint8_t error;                  /* 1 if tuning failed */
    uint32_t timeout_counter;       /* Timeout watchdog */
} autotune_state_t;

/* ============================================================================
 *  PID Instance Data
 * ===========================================================================*/
static temp_pid_t s_pid[TEMP_PID_COUNT];
static uint8_t s_pid_initialized = 0;
static autotune_state_t s_autotune[TEMP_PID_COUNT];

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
            char hl = (pid_idx == TEMP_PID_LARGE) ? 'B' : 'A';
            rt_kprintf("[PID] Handle %c sensor fault! temp=%d.%d\n",
                       hl, (int)temp, ((int)(temp*10))%10);
        }
        return 0;
    }

    /* Overheat protection */
    if (temp > TEMP_OVERHEAT_CELSIUS) {
        pid->enabled = 0;
        pid->overheat_shutdown = 1;
        heater_set(pid_idx, 0);
        char hl2 = (pid_idx == TEMP_PID_LARGE) ? 'B' : 'A';
        rt_kprintf("[PID] Handle %c overheat! temp=%d.%d > %d\n",
                   hl2, (int)temp, ((int)(temp*10))%10, TEMP_OVERHEAT_CELSIUS);
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

    /* Periodic debug log every ~2s (20 cycles x 100ms) */
    static uint8_t s_dbg_counter[TEMP_PID_COUNT] = {0};
    if (++s_dbg_counter[pid_idx] >= 20) {
        s_dbg_counter[pid_idx] = 0;
        char hl = (pid_idx == TEMP_PID_LARGE) ? 'B' : 'A';
        /* Use integer math to avoid float printf stack cost in timer context */
        int tgt_x10 = (int)(pid->target_temp * 10);
        int cur_x10 = (int)(pid->current_temp * 10);
        int out_x10 = (int)(pid->output * 10);
        rt_kprintf("[PID] %c: tgt=%d.%d cur=%d.%d out=%d.%d en=%d pre=%d\n",
                   hl, tgt_x10/10, tgt_x10%10, cur_x10/10, cur_x10%10,
                   out_x10/10, out_x10%10,
                   pid->enabled, pid->preheat_active);
    }

    /* Safety check */
    if (!safety_check(pid_idx)) {
        pid->output = 0;
    pid->preheat_active = 0;
    pid->preheat_power = 0;
        return;
    }

    /* If target is 0, heating is disabled */
    if (pid->target_temp <= 0.0f) {
        pid->output = 0;
    pid->preheat_active = 0;
    pid->preheat_power = 0;
        pid->integral = 0;
        pid->prev_error = 0;
        pid->prev_measurement = pid->current_temp;
        return;
    }


    /* ---- Preheat Phase ---- */
    /* When temp is far from target, limit power for smooth rise.
     * Gradually ramp up power as temperature approaches target.
     * Switch to full PID when within PREHEAT_THRESHOLD of target. */
    if (pid->preheat_active) {
        float error_pre = pid->target_temp - pid->current_temp;

        /* Switch to PID when close enough to target */
        if (error_pre <= TEMP_PID_PREHEAT_THRESHOLD) {
            pid->preheat_active = 0;
            /* Reset integral for clean PID start */
            pid->integral = 0;
            pid->prev_measurement = pid->current_temp;
            char hl = (pid_idx == TEMP_PID_LARGE) ? 'B' : 'A';
            rt_kprintf("[PID] Handle %c preheat -> PID at %d.%d C\n",
                       hl, (int)pid->current_temp, ((int)(pid->current_temp*10))%10);
        } else {
            /* Ramp up preheat power gradually (per-handle limit) */
            uint8_t preheat_max = (pid_idx == TEMP_PID_LARGE)
                                  ? TEMP_PID_PREHEAT_MAX_POWER_LARGE
                                  : TEMP_PID_PREHEAT_MAX_POWER_SMALL;
            if (pid->preheat_power < preheat_max) {
                pid->preheat_power += TEMP_PID_PREHEAT_RAMP_STEP;
                if (pid->preheat_power > preheat_max) {
                    pid->preheat_power = preheat_max;
                }
            }
            pid->output = (float)pid->preheat_power;
            pid->prev_error = error_pre;
            pid->prev_measurement = pid->current_temp;
#if VOFA_ENABLE
            /* Still output during preheat for debugging */
            vofa_output(pid_idx, 0, 0, 0);
#endif
            return;  /* Skip full PID computation during preheat */
        }
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

    /* VOFA+ debug output */
#if VOFA_ENABLE
    vofa_output(pid_idx, p_term, i_term, d_term);
#endif

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
        s_pid[i].preheat_active = 0;
        s_pid[i].preheat_power = 0;
        s_pid[i].tick_divider = 0;
        s_pid[i].sensor_fault = 0;
        s_pid[i].fault_count = 0;

        /* Initialize autotune state */
        rt_memset(&s_autotune[i], 0, sizeof(autotune_state_t));
        s_autotune[i].mode = PID_MODE_NORMAL;
        s_autotune[i].hysteresis = TEMP_PID_AUTOTUNE_HYSTERESIS;

        /* Ensure heater is off */
        heater_set(i, 0);
    }

    rt_kprintf("[PID] Temperature PID controller initialized, instances=%d\n", TEMP_PID_COUNT);
    s_pid_initialized = 1;
    rt_kprintf("[PID] Kp=%.1f Ki=%.4f Kd=%.1f, CtrlPeriod=%dms, PWMPeriod=%dms\n",
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

    /* Save target and prepare preheat state, but do NOT enable PID here.
     * PID will be enabled only when treatment actually starts (handle_start_pause).
     * Setting temp to 0 performs a full reset. */
    if (temp_c > 0) {
        pid->preheat_active = 1;
        pid->preheat_power = TEMP_PID_PREHEAT_RAMP_STEP;
        pid->sensor_fault = 0;
        pid->fault_count = 0;
    } else {
        /* Full reset: clean all PID state including integral, PWM counter, etc. */
        temp_pid_full_reset(pid_idx);
    }

    /* PID[0]=LARGE=Handle B, PID[1]=SMALL=Handle A */
    char handle_letter = (pid_idx == TEMP_PID_LARGE) ? 'B' : 'A';
    rt_kprintf("[PID] Handle %c target: %.1f C, enabled=%d, preheat=%d\n",
               handle_letter, temp_c, pid->enabled, pid->preheat_active);
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
    } else {
        /* Clear overheat flag when user explicitly enables PID */
        s_pid[pid_idx].overheat_shutdown = 0;
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
    pid->preheat_active = 0;
    pid->preheat_power = 0;
    pid->pwm_counter = 0;
    heater_set(pid_idx, 0);
}

void temp_pid_full_reset(uint8_t pid_idx)
{
    if (pid_idx >= TEMP_PID_COUNT) return;
    temp_pid_t *pid = &s_pid[pid_idx];

    /* Turn off heater first */
    heater_set(pid_idx, 0);

    /* Clear all PID state */
    pid->integral = 0;
    pid->prev_error = 0;
    pid->prev_measurement = 0;
    pid->output = 0;
    pid->pwm_counter = 0;
    pid->pwm_duty = 0;
    pid->tick_divider = 0;
    pid->enabled = 0;
    pid->sensor_fault = 0;
    pid->fault_count = 0;
    pid->preheat_active = 0;
    pid->overheat_shutdown = 0;
    pid->preheat_power = 0;
}

/* ============================================================================
 *  PID Auto-Tune Public API
 * ===========================================================================*/

void temp_pid_autotune_start(uint8_t pid_idx, float target_temp)
{
    if (pid_idx >= TEMP_PID_COUNT) return;

    /* Validate target temperature range */
    if (target_temp < 20.0f || target_temp > TEMP_MAX_CELSIUS) {
        rt_kprintf("[PID-AT] Handle %c invalid target %.1f C\n",
                   'A' + pid_idx, target_temp);
        return;
    }

    autotune_state_t *at = &s_autotune[pid_idx];
    temp_pid_t *pid = &s_pid[pid_idx];

    /* Stop normal PID control */
    pid->enabled = 0;
    pid->output = 0;
    pid->preheat_active = 0;
    pid->preheat_power = 0;
    pid->integral = 0;
    heater_set(pid_idx, 0);

    /* Initialize autotune state */
    rt_memset(at, 0, sizeof(autotune_state_t));
    at->mode = PID_MODE_AUTOTUNE;
    at->target_temp = target_temp;
    at->hysteresis = TEMP_PID_AUTOTUNE_HYSTERESIS;
    at->heater_state = 1;           /* Start with heater ON */
    at->rising = 1;
    at->cycle_start_tick = 0;

    /* Start heating immediately */
    heater_set(pid_idx, 1);

    rt_kprintf("[PID-AT] Handle %c autotune STARTED, target=%.1f C, hyst=%.1f C\n",
               'A' + pid_idx, target_temp, at->hysteresis);
}

void temp_pid_autotune_stop(uint8_t pid_idx)
{
    if (pid_idx >= TEMP_PID_COUNT) return;

    autotune_state_t *at = &s_autotune[pid_idx];

    /* Turn off heater and reset mode */
    heater_set(pid_idx, 0);
    at->mode = PID_MODE_NORMAL;

    rt_kprintf("[PID-AT] Handle %c autotune STOPPED\n", 'A' + pid_idx);
}

uint8_t temp_pid_autotune_is_running(uint8_t pid_idx)
{
    if (pid_idx >= TEMP_PID_COUNT) return 0;
    return (s_autotune[pid_idx].mode == PID_MODE_AUTOTUNE) ? 1 : 0;
}

uint8_t temp_pid_autotune_complete(uint8_t pid_idx)
{
    if (pid_idx >= TEMP_PID_COUNT) return 0;
    return s_autotune[pid_idx].complete;
}

/* ============================================================================
 *  Internal: Auto-Tune Tick (Relay Feedback Method)
 * ===========================================================================*/

/**
 * @brief  Auto-tune control tick for one PID instance.
 *         Uses relay feedback (bang-bang) to induce sustained oscillation.
 *         Measures period and amplitude over TEMP_PID_AUTOTUNE_CYCLES cycles,
 *         then calculates PID parameters using Ziegler-Nichols formula.
 *
 * @param  pid_idx  PID instance index.
 */
static void autotune_tick(uint8_t pid_idx)
{
    autotune_state_t *at = &s_autotune[pid_idx];
    temp_pid_t *pid = &s_pid[pid_idx];
    uint8_t ntc_ch = s_ntc_channel[pid_idx];

    /* Read current temperature */
    float temp = ntc_sensor_get_temperature(ntc_ch);
    pid->current_temp = temp;

    /* Safety check: overheat protection still active during autotune */
    if (temp > TEMP_OVERHEAT_CELSIUS) {
        heater_set(pid_idx, 0);
        at->error = 1;
        at->mode = PID_MODE_NORMAL;
        pid->enabled = 0;
        uint8_t err[7] = { AUTOTUNE_STATUS_ERROR, 0,0,0,0,0,0 };
        protocol_send_ack(FUNC_PID_AUTOTUNE, err, 7);
        rt_kprintf("[PID-AT] Handle %c ABORT: overheat %.1f C\n",
                   'A' + pid_idx, temp);
        return;
    }

    /* Safety check: sensor fault detection during autotune */
    if (temp < TEMP_SENSOR_ERROR_LOW || temp > TEMP_SENSOR_ERROR_HIGH) {
        heater_set(pid_idx, 0);
        at->error = 1;
        at->mode = PID_MODE_NORMAL;
        pid->enabled = 0;
        uint8_t err[7] = { AUTOTUNE_STATUS_ERROR, 0,0,0,0,0,0 };
        protocol_send_ack(FUNC_PID_AUTOTUNE, err, 7);
        rt_kprintf("[PID-AT] Handle %c ABORT: sensor fault temp=%.1f C\n",
                   'A' + pid_idx, temp);
        return;
    }

    /* Timeout watchdog */
    at->timeout_counter++;
    if (at->timeout_counter > TEMP_PID_AUTOTUNE_TIMEOUT) {
        heater_set(pid_idx, 0);
        at->error = 1;
        at->mode = PID_MODE_NORMAL;
        uint8_t err[7] = { AUTOTUNE_STATUS_ERROR, 0,0,0,0,0,0 };
        protocol_send_ack(FUNC_PID_AUTOTUNE, err, 7);
        rt_kprintf("[PID-AT] Handle %c ABORT: timeout\n", 'A' + pid_idx);
        return;
    }

    /* Relay feedback control: bang-bang with hysteresis */
    float upper = at->target_temp + at->hysteresis;
    float lower = at->target_temp - at->hysteresis;

    if (at->heater_state) {
        /* Heater ON: turn OFF when temp exceeds upper threshold */
        if (temp >= upper) {
            heater_set(pid_idx, 0);
            at->heater_state = 0;
            at->temp_high = temp;
            at->rising = 0;
        }
    } else {
        /* Heater OFF: turn ON when temp drops below lower threshold */
        if (temp <= lower) {
            heater_set(pid_idx, 1);
            at->heater_state = 1;
            at->temp_low = temp;

            /* Falling -> Rising transition completes one full cycle */
            if (!at->rising) {
                uint32_t now = rt_tick_get();
                if (at->cycle_start_tick > 0) {
                    uint32_t period = now - at->cycle_start_tick;
                    at->period_sum += period;
                    at->amplitude_sum += (at->temp_high - at->temp_low);
                    at->cycle_count++;
                    rt_kprintf("[PID-AT] Handle %c cycle %d: period=%u ticks, "
                               "amp=%.2f C\n",
                               'A' + pid_idx, at->cycle_count,
                               (unsigned)period,
                               at->temp_high - at->temp_low);
                }
                at->cycle_start_tick = now;
            }
            at->rising = 1;
        }
    }

    /* Check if enough cycles have been measured */
    if (at->cycle_count >= TEMP_PID_AUTOTUNE_CYCLES) {
        float Tu_sec = (float)at->period_sum / at->cycle_count
                       / (float)RT_TICK_PER_SECOND;
        float Au = at->amplitude_sum / at->cycle_count;

        /* Ziegler-Nichols relay method:
         *   Ku = 4*d / (pi * a)
         *   d = output amplitude (100% = full on/off, so d=100)
         *   a = temperature oscillation half-amplitude (peak-to-peak / 2)
         *   Kp = 0.6 * Ku
         *   Ki = 2 * Kp / Tu   (integral gain per second)
         *   Kd = Kp * Tu / 8   (derivative gain)
         */
        float a = Au / 2.0f;
        float d = 100.0f;

        if (a < 0.1f) {
            heater_set(pid_idx, 0);
            at->error = 1;
            at->mode = PID_MODE_NORMAL;
            uint8_t err[7] = { AUTOTUNE_STATUS_ERROR, 0,0,0,0,0,0 };
            protocol_send_ack(FUNC_PID_AUTOTUNE, err, 7);
            rt_kprintf("[PID-AT] Handle %c ERROR: amplitude too small (%.3f)\n",
                       'A' + pid_idx, a);
            return;
        }

        float Ku = (4.0f * d) / (M_PI * a);
        float Kp = 0.6f * Ku;
        float Ki_per_sec = 2.0f * Kp / Tu_sec;
        float Kd = Kp * Tu_sec / 8.0f;

        /* Convert Ki from per-second to per-control-period (100ms) */
        float Ki = Ki_per_sec * (TEMP_PID_CTRL_PERIOD * 10.0f / 1000.0f);

        /* Apply new PID parameters */
        pid->kp = Kp;
        pid->ki = Ki;
        pid->kd = Kd;
        pid->integral = 0;
        pid->prev_error = 0;
        pid->prev_measurement = temp;

        at->complete = 1;
        at->mode = PID_MODE_NORMAL;
        heater_set(pid_idx, 0);

        /* Send autotune complete notification via protocol */
        /* Pack Kp, Ki, Kd as 16-bit values (x100) big-endian */
        int16_t kp_x100 = (int16_t)(Kp * 100.0f);
        int16_t ki_x100 = (int16_t)(Ki * 100.0f);
        int16_t kd_x100 = (int16_t)(Kd * 100.0f);
        uint8_t notify[7];
        notify[0] = AUTOTUNE_STATUS_COMPLETE;
        notify[1] = (uint8_t)((kp_x100 >> 8) & 0xFF);
        notify[2] = (uint8_t)(kp_x100 & 0xFF);
        notify[3] = (uint8_t)((ki_x100 >> 8) & 0xFF);
        notify[4] = (uint8_t)(ki_x100 & 0xFF);
        notify[5] = (uint8_t)((kd_x100 >> 8) & 0xFF);
        notify[6] = (uint8_t)(kd_x100 & 0xFF);
        protocol_send_ack(FUNC_PID_AUTOTUNE, notify, 7);

        rt_kprintf("[PID-AT] Handle %c DONE: Tu=%.2fs Au=%.2fC "
                   "Ku=%.2f => Kp=%.2f Ki=%.4f Kd=%.2f\n",
                   'A' + pid_idx, Tu_sec, Au, Ku, Kp, Ki, Kd);
    }
}

/* ============================================================================
 *  Main PID Tick (called from 10ms system timer)
 * ===========================================================================*/

void temp_pid_tick(void)
{
    if (!s_pid_initialized) return;
    for (uint8_t i = 0; i < TEMP_PID_COUNT; i++) {
        temp_pid_t *pid = &s_pid[i];

        /* Auto-tune mode takes priority */
        if (s_autotune[i].mode == PID_MODE_AUTOTUNE) {
            pid->tick_divider++;
            if (pid->tick_divider >= TEMP_PID_CTRL_PERIOD) {
                pid->tick_divider = 0;
                autotune_tick(i);
            }
            continue;
        }

        /* Overheat recovery: re-enable PID when temp drops to target - 1 */
        if (!pid->enabled && pid->overheat_shutdown && pid->target_temp > 0) {
            float cur = ntc_sensor_get_temperature(s_ntc_channel[i]);
            if (cur <= pid->target_temp - TEMP_OVERHEAT_RECOVERY_OFFSET && cur > TEMP_SENSOR_ERROR_LOW
                && cur < TEMP_OVERHEAT_CELSIUS) {
                pid->enabled = 1;
                pid->overheat_shutdown = 0;
                pid->preheat_active = 1;
                pid->preheat_power = TEMP_PID_PREHEAT_RAMP_STEP;
                pid->integral = 0;
                pid->prev_measurement = cur;
                pid->sensor_fault = 0;
                pid->fault_count = 0;
                char hl = (i == TEMP_PID_LARGE) ? 'B' : 'A';
                int rc = (int)(cur * 10);
                int rt = (int)(pid->target_temp * 10);
                rt_kprintf("[PID] Handle %c overheat recovered at %d.%d C, "
                           "target=%d.%d C\n", hl, rc/10, rc%10, rt/10, rt%10);
            }
            /* Still skip PID computation this cycle, resume next cycle */
            if (pid->heater_on) {
                heater_set(i, 0);
            }
            continue;
        }

        /* Skip if not enabled (no target set) */
        if (!pid->enabled) {
            if (pid->heater_on) {
                heater_set(i, 0);
            }
            continue;
        }

        /* Divider: run PID control every 100ms (10 x 10ms ticks) */
        pid->tick_divider++;
        if (pid->tick_divider >= TEMP_PID_CTRL_PERIOD) {
            pid->tick_divider = 0;
            pid_compute(i);
            pwm_update(i);
        }
    }
}
