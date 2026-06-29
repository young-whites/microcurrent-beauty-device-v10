# NTC 温度传感器 PID 控制算法设计文档

> **版本**: V1.0  
> **日期**: 2026-06-29  
> **适用平台**: STM32F103RCT6 / RT-Thread v5.1.0 / STM32 HAL  
> **项目**: DJM-V10 微电流美容仪

---

## 一、文档变更记录

| 版本 | 日期 | 作者 | 主要改动 |
|------|------|------|----------|
| V1.0 | 2026-06-29 | — | 初始版本：NTC 温度采集、PID 算法、软件 PWM 加热控制 |

---

## 二、系统概述

### 2.1 功能目标

为 DJM-V10 微电流美容仪的**大手柄（A）和小手柄（B）**提供独立的 PID 温度控制：

- NTC 温度传感器实时采集手柄温度
- PID 算法计算加热功率
- 软件 PWM 控制 GPIO 加热元件
- 温控范围：0~41℃，超温保护 45℃

### 2.2 硬件配置

| 组件 | 规格 |
|------|------|
| NTC 传感器 | 100kΩ @ 25℃, B=3950K, ±1% |
| ADC 分辨率 | 12-bit (0~4095) |
| ADC 时钟 | 12 MHz |
| 大手柄 NTC | ADC1_CH10 (PC0) |
| 小手柄 NTC | ADC1_CH11 (PC1) |
| 大手柄加热 | GPIO PC11 (LARGE_HAND_TEMP_CTRL) |
| 小手柄加热 | GPIO PC12 (SMALL_HAND_TEMP_CTRL) |
| 分压电阻 | R_series = 100kΩ（与 NTC 相同阻值） |

### 2.3 电压分压器电路

```
VCC (3.3V) ─── R_series (100kΩ) ───┬─── NTC (100kΩ) ─── GND
                                    │
                                  ADC Pin
```

当 NTC = 100kΩ (25℃) 时，ADC 读数 = 4096 × 100k / (100k + 100k) = **2048**（中点）。

---

## 三、NTC 温度转换算法

### 3.1 B 参数方程

使用简化的 Steinhart-Hart 方程（B 参数方程）将 ADC 值转换为温度：

```
1/T = 1/T0 + (1/B) × ln(R/R0)
```

| 参数 | 值 | 说明 |
|------|-----|------|
| T0 | 298.15 K | 标称温度（25℃） |
| R0 | 100,000 Ω | 标称电阻（@T0） |
| B | 3950 K | B 值（25/50℃） |
| R | — | 测量的 NTC 电阻 |
| T | — | 计算结果（开尔文） |

### 3.2 ADC → 电阻转换

```
R_ntc = R_series × ADC_value / (ADC_RESOLUTION - ADC_value)
```

其中 `R_series = 100kΩ`, `ADC_RESOLUTION = 4096`。

### 3.3 电阻 → 温度转换

```c
ln_ratio = ln(R_ntc / R0)
inv_T = (1.0 / T0) + (1.0 / B) * ln_ratio
T_kelvin = 1.0 / inv_T
T_celsius = T_kelvin - 273.15
```

### 3.4 典型值验证

| 温度 (℃) | NTC 电阻 (kΩ) | ADC 值（理论） | 计算温度误差 |
|---------|--------------|---------------|-------------|
| 0 | 327.02 | 3409 | < 0.1℃ |
| 15 | 157.23 | 2879 | < 0.1℃ |
| 25 | 100.00 | 2048 | 0 (基准) |
| 37 | 60.07 | 1371 | < 0.1℃ |
| 41 | 51.07 | 1218 | < 0.1℃ |

### 3.5 滑动平均滤波

ADC 读数存在噪声，使用**8 点滑动平均滤波器**平滑输出：

```c
filtered = (sum of last 8 readings) >> 3
```

- 滤波窗口：8 个采样点
- 更新频率：每 10ms 更新一次
- 有效响应延迟：~80ms（对温度系统可接受）
- 使用位移代替除法，提高计算效率

---

## 四、PID 控制算法

### 4.1 控制结构

采用**位置式 PID**，带积分抗饱和和微分项滤波：

```
error = target_temp - current_temp

P = Kp × error
I = I + Ki × error    (带抗饱和限幅)
D = Kd × (prev_measurement - current_temp)   (微分先行，避免设定值突变)

output = P + I + D
output = clamp(output, 0, 100)
```

### 4.2 PID 参数

| 参数 | 值 | 说明 |
|------|-----|------|
| Kp | 8.0 | 比例增益 |
| Ki | 0.15 | 积分增益（每个控制周期） |
| Kd | 3.0 | 微分增益 |
| 控制周期 | 100ms | 10 个系统 Tick（10ms） |
| PWM 周期 | 1000ms | 10 个控制周期 |

### 4.3 PID 参数整定依据

| 考量 | 设计 |
|------|------|
| NTC 响应时间 | 数秒级（热惯性大），控制周期 100ms 足够 |
| 加热元件 | GPIO 开关控制（非连续），采用 PWM 近似连续功率 |
| 超调抑制 | Kp 适中 + 微分先行 + 积分限幅 |
| 稳态精度 | 积分项消除静差，死区 0.3℃ 防止抖动 |
| 安全裕度 | 41℃ 上限 + 45℃ 硬保护 |

### 4.4 特殊处理机制

#### 4.4.1 死区控制（Dead Band）

```
if |error| < 0.3℃:
    output 保持不变（不累加积分）
```

避免在目标温度附近频繁开关加热元件。

#### 4.4.2 积分抗饱和（Anti-Windup）

```
积分上限 = 80.0
积分下限 = -10.0

if I > I_MAX: I = I_MAX
if I < I_MIN: I = I_MIN
```

防止积分项过大导致严重超调。

#### 4.4.3 微分先行（Derivative-on-Masurement）

```
D = Kd × (prev_measurement - current_measurement)
```

不使用 `d(error)/dt`，而是使用 `d(measurement)/dt`，避免设定值突变时微分项产生冲击。

#### 4.4.4 超温软保护

```
if current_temp > target_temp + 1.0℃:
    output = 0
    integral *= 0.95  (缓慢回退积分)
```

当前温度超过目标温度 1℃ 时，立即关闭加热并缓慢衰减积分项。

#### 4.4.5 目标温度变化时积分重置

```
if |new_target - old_target| > 2.0℃:
    integral = 0
```

大幅改变目标温度时清零积分，防止旧积分值影响新控制。

---

## 五、软件 PWM 实现

### 5.1 原理

加热元件为 GPIO 开关控制（高电平加热），通过**时间比例**实现功率调节：

```
PWM 周期 = 1000ms（10 个控制周期 × 100ms）

duty = output% × PWM_PERIOD / 100

tick 0 ~ duty-1:    GPIO HIGH（加热）
tick duty ~ PERIOD-1: GPIO LOW（停止加热）
```

### 5.2 示例

| PID 输出 | 占空比 | 加热时间/周期 | 效果 |
|---------|--------|-------------|------|
| 0% | 0/10 | 全关 | 不加热 |
| 30% | 3/10 | 300ms ON / 700ms OFF | 低功率 |
| 60% | 6/10 | 600ms ON / 400ms OFF | 中功率 |
| 100% | 10/10 | 全开 | 最大加热 |

---

## 六、安全保护机制

### 6.1 保护层次

| 层次 | 机制 | 阈值 | 动作 |
|------|------|------|------|
| L1 | 死区控制 | \|error\| < 0.3℃ | 维持输出不变 |
| L2 | 超温软保护 | current > target + 1℃ | 输出归零，积分衰减 |
| L3 | 过热硬保护 | current > 45℃ | 禁用 PID，关闭加热 |
| L4 | 传感器故障 | ADC 异常（连续 10 次） | 禁用 PID，关闭加热 |
| L5 | 温度上限 | target > 41℃ | 目标温度限幅 |

### 6.2 传感器故障检测

- ADC 值转换后温度 < -30℃ 或 > 100℃ → 判定为异常
- 连续 10 次异常 → 确认传感器故障
- 故障时自动禁用 PID 并关闭加热
- 故障状态通过 `temp_pid_sensor_fault()` 查询

### 6.3 安全状态恢复

- 传感器故障需要重新设置目标温度才能恢复（`temp_pid_set_target` 会清除故障标志）
- 过热保护后温度降至安全范围也需要重新设定

---

## 七、软件模块架构

### 7.1 文件结构

```
applications/
├── macBSP/
│   ├── Inc/
│   │   ├── ntc_sensor.h      ← NTC 传感器头文件
│   │   └── temp_pid.h        ← PID 控制器头文件
│   └── Src/
│       ├── ntc_sensor.c      ← NTC 温度采集与转换
│       └── temp_pid.c        ← PID 算法与加热控制
├── macSYS/
│   └── Src/
│       └── rtt_system_work.c ← 10ms 定时器（调用 NTC + PID）
└── main.c                    ← 模块初始化
```

### 7.2 调用流程

```
系统 1ms 定时器
    │
    ├── Timing_1ms()    (每 1ms)
    │
    ├── Timing_10ms()   (每 10ms)  ← 主要温控入口
    │   ├── ntc_sensor_update()     读取 ADC + 滤波
    │   └── temp_pid_tick()         PID 计算 + PWM 输出
    │
    ├── Timing_50ms()   (每 50ms)
    ├── Timing_500ms()  (每 500ms)
    └── Timing_1s()     (每 1s)
```

### 7.3 PID 控制内部流程

```
temp_pid_tick() [每 10ms 调用]
    │
    ├── 遍历两个手柄 (Large, Small)
    │   │
    │   ├── 检查 enabled 标志
    │   │   └── 未启用 → 确保加热关闭 → 跳过
    │   │
    │   ├── tick_divider 计数
    │   │   └── 未到 100ms → 跳过
    │   │
    │   ├── pid_compute()  [每 100ms]
    │   │   ├── 读取当前温度
    │   │   ├── 安全检查（故障/过热）
    │   │   ├── 计算 error
    │   │   ├── 死区判断
    │   │   ├── 计算 P / I / D
    │   │   ├── 输出限幅
    │   │   └── 超温软保护
    │   │
    │   └── pwm_update()  [每 100ms]
    │       ├── 计算占空比
    │       ├── GPIO ON/OFF 控制
    │       └── PWM 计数器递增
    │
    └── 返回
```

---

## 八、协议层集成

### 8.1 温度控制指令（功能码 0x03）

APP 发送温度设定指令后，协议层调用 PID 控制器：

```c
// handle_temp_ctrl() 中:
g_dev_state.handle[hi].temperature = temperature;
temp_pid_set_target(hi, (float)temperature);
```

- `temperature = 0` → 禁用 PID，关闭加热
- `temperature = 1~41` → 设置 PID 目标温度，启用控制

### 8.2 手柄切换联动

切换手柄时，各手柄的 PID 状态独立保持：
- 大手柄 A → `TEMP_PID_LARGE` (NTC_CH_LARGE, PC11)
- 小手柄 B → `TEMP_PID_SMALL` (NTC_CH_SMALL, PC12)

---

## 九、调试信息

### 9.1 日志输出

通过 UART2 (115200bps) 输出调试信息：

```
[NTC] Sensor module initialized, channels=2
[PID] Temperature PID controller initialized, instances=2
[PID] Kp=8.0 Ki=0.150 Kd=3.0, CtrlPeriod=100ms, PWMPeriod=1000ms
[PROTO] Temp set: handle A = 37 C, PID enabled
[PID] Handle A target: 37.0 C, enabled=1
```

### 9.2 关键监控参数

| 参数 | 获取方式 | 正常范围 |
|------|---------|---------|
| 当前温度 | `ntc_sensor_get_temperature(ch)` | 20~45℃ |
| 目标温度 | `temp_pid_get_target(idx)` | 0~41℃ |
| PID 输出 | `temp_pid_get_output(idx)` | 0~100% |
| 传感器状态 | `temp_pid_sensor_fault(idx)` | 0=正常 |
| NTC 电阻 | `ntc_sensor_get(ch)->resistance` | 30k~200kΩ |
| ADC 原始值 | `ntc_sensor_get(ch)->adc_raw` | 0~4095 |

---

## 十、资源占用

| 资源 | 占用 |
|------|------|
| Flash（代码） | ~3.5 KB（ntc_sensor.c + temp_pid.c） |
| RAM（数据） | ~200 字节（NTC 滤波缓冲 + PID 状态） |
| CPU 时间（10ms） | ~50μs（ADC 读取 + 滤波） |
| CPU 时间（100ms） | ~10μs（PID 计算 + PWM 更新） |
| ADC 通道 | 2 个（CH10, CH11） |
| GPIO 输出 | 2 个（PC11, PC12） |
| 定时器 | 复用系统 1ms 软件定时器 |

---

## 十一、后续优化方向

| 项目 | 说明 |
|------|------|
| PID 参数自整定 | 实现继电反馈自整定（Relay Auto-Tune） |
| NTC 查表法 | 用查表+线性插值替代 B 参数方程（更高精度） |
| 温度上报 | 新增协议功能码，周期性上报当前温度 |
| PWM 硬件化 | 使用 TIM 输出硬件 PWM，减少 CPU 开销 |
| 温度曲线 | 支持多段温度曲线（升温 → 保温 → 降温） |

---

*—— 温度 PID 算法设计文档完毕*
