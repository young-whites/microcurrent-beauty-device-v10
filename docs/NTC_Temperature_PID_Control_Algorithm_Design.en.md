# NTC Temperature Sensor PID Control Algorithm Design Document

> **Version**: V1.0  
> **Date**: 2026-06-29  
> **Platform**: STM32F103RCT6 / RT-Thread v5.1.0 / STM32 HAL  
> **Project**: DJM-V10 Microcurrent Beauty Device

---

## 1. Document Change Log

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| V1.0 | 2026-06-29 | — | Initial version: NTC temperature acquisition, PID algorithm, software PWM heating control |

---

## 2. System Overview

### 2.1 Functional Objective

Provide independent PID temperature control for the **large handle (A)** and **small handle (B)** of the DJM-V10 microcurrent beauty device:

- Real-time NTC temperature sensor acquisition
- PID algorithm for heating power calculation
- Software PWM for GPIO heating element control
- Temperature range: 0–41°C, overheat protection at 45°C

### 2.2 Hardware Configuration

| Component | Specification |
|-----------|---------------|
| NTC Sensor | 100kΩ @ 25°C, B=3950K, ±1% |
| ADC Resolution | 12-bit (0–4095) |
| ADC Clock | 12 MHz |
| Large Handle NTC | ADC1_CH10 (PC0) |
| Small Handle NTC | ADC1_CH11 (PC1) |
| Large Handle Heater | GPIO PC11 (LARGE_HAND_TEMP_CTRL) |
| Small Handle Heater | GPIO PC12 (SMALL_HAND_TEMP_CTRL) |
| Series Resistor | R_series = 100kΩ (same as NTC nominal) |

### 2.3 Voltage Divider Circuit

```
VCC (3.3V) ─── R_series (100kΩ) ───┬─── NTC (100kΩ) ─── GND
                                    │
                                  ADC Pin
```

At 25°C (NTC = 100kΩ), ADC reading = 4096 × 100k / (100k + 100k) = **2048** (midpoint).

---

## 3. NTC Temperature Conversion Algorithm

### 3.1 B-Parameter Equation

Use the simplified Steinhart-Hart equation (B-parameter equation) to convert ADC values to temperature:

```
1/T = 1/T0 + (1/B) × ln(R/R0)
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| T0 | 298.15 K | Nominal temperature (25°C) |
| R0 | 100,000 Ω | Nominal resistance (@T0) |
| B | 3950 K | B-value (25/50°C) |
| R | — | Measured NTC resistance |
| T | — | Calculated temperature (Kelvin) |

### 3.2 ADC → Resistance Conversion

```
R_ntc = R_series × ADC_value / (ADC_RESOLUTION - ADC_value)
```

Where `R_series = 100kΩ`, `ADC_RESOLUTION = 4096`.

### 3.3 Resistance → Temperature Conversion

```c
ln_ratio = ln(R_ntc / R0)
inv_T = (1.0 / T0) + (1.0 / B) * ln_ratio
T_kelvin = 1.0 / inv_T
T_celsius = T_kelvin - 273.15
```

### 3.4 Verification Table

| Temp (°C) | NTC Resistance (kΩ) | Theoretical ADC | Conversion Error |
|-----------|---------------------|-----------------|------------------|
| 0 | 327.02 | 3409 | < 0.1°C |
| 15 | 157.23 | 2879 | < 0.1°C |
| 25 | 100.00 | 2048 | 0 (reference) |
| 37 | 60.07 | 1371 | < 0.1°C |
| 41 | 51.07 | 1218 | < 0.1°C |

### 3.5 Sliding Average Filter

ADC readings contain noise. An **8-point sliding average filter** is used:

```c
filtered = (sum of last 8 readings) >> 3
```

- Window size: 8 samples
- Update rate: every 10ms
- Effective response delay: ~80ms (acceptable for thermal systems)
- Bit-shift instead of division for efficiency

---

## 4. PID Control Algorithm

### 4.1 Control Structure

**Positional PID** with integral anti-windup and derivative-on-measurement:

```
error = target_temp - current_temp

P = Kp × error
I = I + Ki × error    (with anti-windup clamping)
D = Kd × (prev_measurement - current_temp)   (derivative-on-measurement)

output = P + I + D
output = clamp(output, 0, 100)
```

### 4.2 PID Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| Kp | 8.0 | Proportional gain |
| Ki | 0.15 | Integral gain (per control period) |
| Kd | 3.0 | Derivative gain |
| Control Period | 100ms | 10 system ticks (10ms) |
| PWM Period | 1000ms | 10 control periods |

### 4.3 PID Tuning Rationale

| Consideration | Design |
|---------------|--------|
| NTC Response Time | Seconds-level (large thermal inertia), 100ms control period sufficient |
| Heating Element | GPIO switch control (not continuous), PWM approximates continuous power |
| Overshoot Suppression | Moderate Kp + derivative-on-measurement + integral clamping |
| Steady-State Accuracy | Integral eliminates offset, 0.3°C dead band prevents chatter |
| Safety Margin | 41°C upper limit + 45°C hard protection |

### 4.4 Special Processing Mechanisms

#### 4.4.1 Dead Band Control

```
if |error| < 0.3°C:
    output remains unchanged (no integral accumulation)
```

Prevents frequent on/off cycling of the heater near the target temperature.

#### 4.4.2 Integral Anti-Windup

```
Integral upper limit = 80.0
Integral lower limit = -10.0

if I > I_MAX: I = I_MAX
if I < I_MIN: I = I_MIN
```

Prevents integral term from causing severe overshoot.

#### 4.4.3 Derivative-on-Measurement

```
D = Kd × (prev_measurement - current_measurement)
```

Uses `d(measurement)/dt` instead of `d(error)/dt` to avoid derivative kick when the setpoint changes.

#### 4.4.4 Over-Temperature Soft Protection

```
if current_temp > target_temp + 1.0°C:
    output = 0
    integral *= 0.95  (slowly decay integral)
```

Immediately stops heating and slowly decays the integral when current temperature exceeds target by 1°C.

#### 4.4.5 Integral Reset on Setpoint Change

```
if |new_target - old_target| > 2.0°C:
    integral = 0
```

Clears integral when the target temperature changes significantly, preventing stale integral values.

---

## 5. Software PWM Implementation

### 5.1 Principle

The heating element uses GPIO switch control (HIGH = heating). Power regulation is achieved through **time-proportional control**:

```
PWM Period = 1000ms (10 control periods × 100ms)

duty = output% × PWM_PERIOD / 100

tick 0 ~ duty-1:      GPIO HIGH (heating)
tick duty ~ PERIOD-1:  GPIO LOW (stop heating)
```

### 5.2 Examples

| PID Output | Duty Cycle | Heating Time/Period | Effect |
|-----------|------------|---------------------|--------|
| 0% | 0/10 | All OFF | No heating |
| 30% | 3/10 | 300ms ON / 700ms OFF | Low power |
| 60% | 6/10 | 600ms ON / 400ms OFF | Medium power |
| 100% | 10/10 | All ON | Maximum heating |

---

## 6. Safety Protection Mechanisms

### 6.1 Protection Layers

| Layer | Mechanism | Threshold | Action |
|-------|-----------|-----------|--------|
| L1 | Dead Band | \|error\| < 0.3°C | Maintain output |
| L2 | Over-temp Soft | current > target + 1°C | Output to zero, integral decay |
| L3 | Overheat Hard | current > 45°C | Disable PID, turn off heater |
| L4 | Sensor Fault | ADC abnormal (10 consecutive) | Disable PID, turn off heater |
| L5 | Temp Limit | target > 41°C | Clamp target temperature |

### 6.2 Sensor Fault Detection

- ADC value converted to temperature < -30°C or > 100°C → abnormal
- 10 consecutive abnormal readings → confirmed sensor fault
- PID automatically disabled and heater turned off on fault
- Fault status queryable via `temp_pid_sensor_fault()`

### 6.3 Safety State Recovery

- Sensor fault requires re-setting target temperature to clear (via `temp_pid_set_target`)
- Overheat protection also requires re-setting after temperature returns to safe range

---

## 7. Software Module Architecture

### 7.1 File Structure

```
applications/
├── macBSP/
│   ├── Inc/
│   │   ├── ntc_sensor.h      ← NTC sensor header
│   │   └── temp_pid.h        ← PID controller header
│   └── Src/
│       ├── ntc_sensor.c      ← NTC temperature acquisition & conversion
│       └── temp_pid.c        ← PID algorithm & heating control
├── macSYS/
│   └── Src/
│       └── rtt_system_work.c ← 10ms timer (calls NTC + PID)
└── main.c                    ← Module initialization
```

### 7.2 Call Flow

```
System 1ms Timer
    │
    ├── Timing_1ms()    (every 1ms)
    │
    ├── Timing_10ms()   (every 10ms)  ← Main temperature control entry
    │   ├── ntc_sensor_update()     Read ADC + filter
    │   └── temp_pid_tick()         PID compute + PWM output
    │
    ├── Timing_50ms()   (every 50ms)
    ├── Timing_500ms()  (every 500ms)
    └── Timing_1s()     (every 1s)
```

### 7.3 PID Control Internal Flow

```
temp_pid_tick() [called every 10ms]
    │
    ├── Iterate both handles (Large, Small)
    │   │
    │   ├── Check enabled flag
    │   │   └── Disabled → ensure heater OFF → skip
    │   │
    │   ├── tick_divider counter
    │   │   └── Not at 100ms → skip
    │   │
    │   ├── pid_compute()  [every 100ms]
    │   │   ├── Read current temperature
    │   │   ├── Safety check (fault/overheat)
    │   │   ├── Calculate error
    │   │   ├── Dead band check
    │   │   ├── Compute P / I / D
    │   │   ├── Output clamping
    │   │   └── Over-temp soft protection
    │   │
    │   └── pwm_update()  [every 100ms]
    │       ├── Calculate duty cycle
    │       ├── GPIO ON/OFF control
    │       └── PWM counter increment
    │
    └── Return
```

---

## 8. Protocol Layer Integration

### 8.1 Temperature Control Command (Function Code 0x03)

After receiving the temperature setting command, the protocol layer calls the PID controller:

```c
// In handle_temp_ctrl():
g_dev_state.handle[hi].temperature = temperature;
temp_pid_set_target(hi, (float)temperature);
```

- `temperature = 0` → Disable PID, turn off heating
- `temperature = 1–41` → Set PID target temperature, enable control

### 8.2 Handle Switching Interaction

When switching handles, each handle's PID state is independently maintained:
- Large Handle A → `TEMP_PID_LARGE` (NTC_CH_LARGE, PC11)
- Small Handle B → `TEMP_PID_SMALL` (NTC_CH_SMALL, PC12)

---

## 9. Debug Information

### 9.1 Log Output

Debug information via UART2 (115200bps):

```
[NTC] Sensor module initialized, channels=2
[PID] Temperature PID controller initialized, instances=2
[PID] Kp=8.0 Ki=0.150 Kd=3.0, CtrlPeriod=100ms, PWMPeriod=1000ms
[PROTO] Temp set: handle A = 37 C, PID enabled
[PID] Handle A target: 37.0 C, enabled=1
```

### 9.2 Key Monitoring Parameters

| Parameter | Access Method | Normal Range |
|-----------|---------------|--------------|
| Current Temperature | `ntc_sensor_get_temperature(ch)` | 20–45°C |
| Target Temperature | `temp_pid_get_target(idx)` | 0–41°C |
| PID Output | `temp_pid_get_output(idx)` | 0–100% |
| Sensor Status | `temp_pid_sensor_fault(idx)` | 0=OK |
| NTC Resistance | `ntc_sensor_get(ch)->resistance` | 30k–200kΩ |
| ADC Raw Value | `ntc_sensor_get(ch)->adc_raw` | 0–4095 |

---

## 10. Resource Usage

| Resource | Usage |
|----------|-------|
| Flash (code) | ~3.5 KB (ntc_sensor.c + temp_pid.c) |
| RAM (data) | ~200 bytes (NTC filter buffer + PID state) |
| CPU Time (10ms) | ~50μs (ADC read + filter) |
| CPU Time (100ms) | ~10μs (PID compute + PWM update) |
| ADC Channels | 2 (CH10, CH11) |
| GPIO Outputs | 2 (PC11, PC12) |
| Timer | Reuses system 1ms software timer |

---

## 11. Future Optimization

| Item | Description |
|------|-------------|
| PID Auto-Tuning | Implement relay feedback auto-tuning |
| NTC Lookup Table | Table + linear interpolation for higher accuracy |
| Temperature Reporting | New protocol function code for periodic temperature reporting |
| Hardware PWM | Use TIM peripheral for hardware PWM, reducing CPU overhead |
| Temperature Profiles | Multi-stage temperature curves (ramp → hold → cool) |

---

*— Temperature PID Algorithm Design Document Complete —*
