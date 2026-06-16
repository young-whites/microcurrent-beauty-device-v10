# NNC6521 双芯片驱动 API 使用说明

## 概述

NNC6521 是一款双通道微电流刺激芯片，适用于美容仪等医疗电子设备。本驱动基于 RT-Thread + STM32F103 平台，采用软件 SPI（GPIO bit-bang）方式通信，支持双芯片独立控制。

### 特性

- 双芯片独立控制，每个芯片支持 2 个通道
- 软件 SPI 通信，CPOL=0, CPHA=0, MSB 先行
- 支持预定义波形（正弦波、三角波、脉冲波）和自定义波形
- 支持幅度调制输出
- 集成 Lead-Off Detection（LOD）和 Short-Circuit Detection（SCD）
- 内置电流校准功能

---

## 文件结构

```
applications/macBSP/
├── Inc/
│   ├── nnc6521.h          # 主头文件，包含 API 声明和数据结构
│   └── nnc6521_reg.h      # 寄存器定义和枚举类型
└── Src/
    ├── nnc6521_drv.c      # 驱动核心实现
    ├── nnc6521_spi.c      # 软件 SPI 通信实现
    └── nnc6521_waveform.c # 预定义波形数据
```

---

## 引脚映射

| 信号 | Chip 1 | Chip 2 |
|------|--------|--------|
| MOSI | PC9 | PB13 |
| CSN | PC8 | PB12 |
| SCLK | PC7 | PB11 |
| MISO | PA8 | PB14 |
| CHIP_EN | PA12 | PC6 |
| INTB | PA11 | PB15 |

---

## API 参考

### 1. 初始化函数

#### nnc6521_gpio_init()

```c
void nnc6521_gpio_init(void);
```

**功能**：初始化所有 NNC6521 芯片的 GPIO 引脚。

**说明**：
- 配置 MOSI/CSN/SCLK/CHIP_EN 为推挽输出
- 配置 MISO/INTB 为浮空输入
- 使能所有相关 GPIO 端口时钟

**调用时机**：系统启动时调用一次。

---

#### nnc6521_init()

```c
void nnc6521_init(uint8_t chip_id);
```

**功能**：初始化指定芯片，执行上电序列。

**参数**：
| 参数 | 类型 | 说明 |
|------|------|------|
| chip_id | uint8_t | 芯片 ID：NNC6521_CHIP_1 (0) 或 NNC6521_CHIP_2 (1) |

**说明**：
- 执行 CHIP_EN 上电序列：低 → 延时 → 高 → 延时 → 就绪
- 复位波形发生器
- 配置时钟和模拟模块

**使用示例**：
```c
nnc6521_gpio_init();      // 初始化 GPIO
nnc6521_init(NNC6521_CHIP_1);  // 初始化芯片 1
nnc6521_init(NNC6521_CHIP_2);  // 初始化芯片 2
```

---

### 2. SPI 通信函数

#### nnc6521_spi_write()

```c
void nnc6521_spi_write(uint8_t chip_id, uint8_t addr, uint8_t data, uint8_t is_wave);
```

**功能**：通过软件 SPI 写入一个寄存器。

**参数**：
| 参数 | 类型 | 说明 |
|------|------|------|
| chip_id | uint8_t | 芯片 ID |
| addr | uint8_t | 寄存器地址 |
| data | uint8_t | 写入数据 |
| is_wave | uint8_t | 1 = 波形寄存器 (cmd 0xC0), 0 = 普通寄存器 (cmd 0x80) |

---

#### nnc6521_spi_read()

```c
uint8_t nnc6521_spi_read(uint8_t chip_id, uint8_t addr, uint8_t is_wave);
```

**功能**：通过软件 SPI 读取一个寄存器。

**参数**：
| 参数 | 类型 | 说明 |
|------|------|------|
| chip_id | uint8_t | 芯片 ID |
| addr | uint8_t | 寄存器地址 |
| is_wave | uint8_t | 1 = 波形寄存器 (cmd 0x40), 0 = 普通寄存器 (cmd 0x00) |

**返回值**：读取的数据字节。

---

#### nnc6521_spi_otp_read()

```c
uint8_t nnc6521_spi_otp_read(uint8_t chip_id, uint8_t addr);
```

**功能**：从 OTP 内存读取一个字节。

**参数**：
| 参数 | 类型 | 说明 |
|------|------|------|
| chip_id | uint8_t | 芯片 ID |
| addr | uint8_t | OTP 地址 |

**返回值**：OTP 数据字节。

---

#### 便捷宏

```c
#define nnc6521_write_reg(cid, addr, data)      nnc6521_spi_write((cid), (addr), (data), 0)
#define nnc6521_read_reg(cid, addr)             nnc6521_spi_read((cid), (addr), 0)
#define nnc6521_write_wave_reg(cid, addr, data) nnc6521_spi_write((cid), (addr), (data), 1)
#define nnc6521_read_wave_reg(cid, addr)        nnc6521_spi_read((cid), (addr), 1)
```

---

### 3. 波形输出函数

#### nnc6521_preloaded_waveform()

```c
void nnc6521_preloaded_waveform(
    uint8_t chip_id,
    uint8_t u8_Channel,
    uint8_t u8_Waveform,
    uint8_t u8_PointNum,
    uint8_t u8_CI,
    uint16_t u32_Positive_Interval,
    uint16_t u32_Negative_Interval,
    uint32_t u32_Silent_Time,
    uint16_t u16_Rest_Time);
```

**功能**：输出预加载的波形（正弦波/脉冲波/三角波）。

**参数**：
| 参数 | 类型 | 说明 |
|------|------|------|
| chip_id | uint8_t | 芯片 ID |
| u8_Channel | uint8_t | 通道号：WAVEFORM_GEN_CH0 (0) 或 WAVEFORM_GEN_CH1 (1) |
| u8_Waveform | uint8_t | 波形类型：WAVEFORM_SINE (0), WAVEFORM_PULSE (1), WAVEFORM_TRIANGLE (2) |
| u8_PointNum | uint8_t | 波形点数（16/64/128） |
| u8_CI | uint8_t | 电流强度档位 |
| u32_Positive_Interval | uint16_t | 正半周间隔（时钟周期） |
| u32_Negative_Interval | uint16_t | 负半周间隔（时钟周期） |
| u32_Silent_Time | uint32_t | 静音时间（时钟周期） |
| u16_Rest_Time | uint16_t | 休息时间（时钟周期） |

**使用示例**：
```c
// 输出正弦波，64点，通道0
nnc6521_preloaded_waveform(
    NNC6521_CHIP_1,
    WAVEFORM_GEN_CH0,
    WAVEFORM_SINE,
    64,
    0x04,      // 电流档位
    100,       // 正半周间隔
    100,       // 负半周间隔
    1000,      // 静音时间
    50         // 休息时间
);
```

---

#### nnc6521_customized_waveform()

```c
void nnc6521_customized_waveform(
    uint8_t chip_id,
    uint8_t u8_Channel,
    uint8_t u8_PointNum,
    float *f_Normalized_array,
    uint32_t u32_Max_current,
    uint16_t u32_Positive_Interval,
    uint16_t u32_Negative_Interval,
    uint32_t u32_Silent_Time,
    uint16_t u16_Rest_Time,
    uint8_t u8_Asymmetric_Symmetric);
```

**功能**：输出自定义波形，使用归一化数组进行校准。

**参数**：
| 参数 | 类型 | 说明 |
|------|------|------|
| chip_id | uint8_t | 芯片 ID |
| u8_Channel | uint8_t | 通道号 |
| u8_PointNum | uint8_t | 波形点数 |
| f_Normalized_array | float* | 归一化波形数组（0.0~1.0） |
| u32_Max_current | uint32_t | 最大电流值（μA） |
| u32_Positive_Interval | uint16_t | 正半周间隔 |
| u32_Negative_Interval | uint16_t | 负半周间隔 |
| u32_Silent_Time | uint32_t | 静音时间 |
| u16_Rest_Time | uint16_t | 休息时间 |
| u8_Asymmetric_Symmetric | uint8_t | 0 = 非对称波形, 1 = 对称波形 |

**使用示例**：
```c
// 自定义正弦波，128点，最大电流 500μA
extern float normalized_sine_waveform_128[128];

nnc6521_customized_waveform(
    NNC6521_CHIP_1,
    WAVEFORM_GEN_CH0,
    128,
    normalized_sine_waveform_128,
    500,       // 最大电流 500μA
    100,
    100,
    1000,
    50,
    1          // 对称模式
);
```

---

#### nnc6521_amplitude_modulation()

```c
void nnc6521_amplitude_modulation(
    uint8_t chip_id,
    uint8_t u8_Channel,
    uint8_t u8_PointNum,
    float *f_Normalized_Envelope_array,
    uint32_t u32_Max_current,
    uint16_t u16_Carrier,
    uint32_t u32_Silent_Time,
    uint16_t u16_Interval);
```

**功能**：输出幅度调制波形。

**参数**：
| 参数 | 类型 | 说明 |
|------|------|------|
| chip_id | uint8_t | 芯片 ID |
| u8_Channel | uint8_t | 通道号 |
| u8_PointNum | uint8_t | 包络波形点数 |
| f_Normalized_Envelope_array | float* | 归一化包络数组 |
| u32_Max_current | uint32_t | 最大电流值（μA） |
| u16_Carrier | uint16_t | 载波频率 |
| u32_Silent_Time | uint32_t | 静音时间 |
| u16_Interval | uint16_t | 间隔时间 |

---

#### nnc6521_awg_enable_disable()

```c
void nnc6521_awg_enable_disable(uint8_t chip_id, uint8_t AWG_ChannelNum, uint8_t Enable_Disable);
```

**功能**：使能或禁用指定通道的波形发生器。

**参数**：
| 参数 | 类型 | 说明 |
|------|------|------|
| chip_id | uint8_t | 芯片 ID |
| AWG_ChannelNum | uint8_t | 通道号 |
| Enable_Disable | uint8_t | 1 = 使能, 0 = 禁用 |

---

### 4. 检测功能函数

#### nnc6521_lod_init()

```c
void nnc6521_lod_init(
    uint8_t chip_id,
    uint16_t u16_LOD_Threshold,
    uint32_t u32_LOD_DLY,
    uint8_t u8_LOD_TGT);
```

**功能**：初始化 Lead-Off Detection（LOD）功能。

**参数**：
| 参数 | 类型 | 说明 |
|------|------|------|
| chip_id | uint8_t | 芯片 ID |
| u16_LOD_Threshold | uint16_t | LOD 阈值 |
| u32_LOD_DLY | uint32_t | LOD 延迟时间 |
| u8_LOD_TGT | uint8_t | LOD 目标值 |

**说明**：
- 同时配置 CH1 和 CH2 的 VDAC
- 使能高电平检测模式

---

#### nnc6521_scd_init()

```c
void nnc6521_scd_init(
    uint8_t chip_id,
    uint8_t u8_CH0_DAC_Threshold,
    uint8_t u8_CH0_Addr_0,
    uint8_t u8_CH0_Addr_1,
    uint8_t u8_CH0_Int_Num,
    uint8_t u8_CH1_DAC_Threshold,
    uint8_t u8_CH1_Addr_0,
    uint8_t u8_CH1_Addr_1,
    uint8_t u8_CH1_Int_Num);
```

**功能**：初始化 Short-Circuit Detection（SCD）功能。

**参数**：
| 参数 | 类型 | 说明 |
|------|------|------|
| chip_id | uint8_t | 芯片 ID |
| u8_CH0_DAC_Threshold | uint8_t | CH1 DAC 阈值 |
| u8_CH0_Addr_0 | uint8_t | CH1 地址 0 |
| u8_CH0_Addr_1 | uint8_t | CH1 地址 1 |
| u8_CH0_Int_Num | uint8_t | CH1 中断编号 |
| u8_CH1_DAC_Threshold | uint8_t | CH2 DAC 阈值 |
| u8_CH1_Addr_0 | uint8_t | CH2 地址 0 |
| u8_CH1_Addr_1 | uint8_t | CH2 地址 1 |
| u8_CH1_Int_Num | uint8_t | CH2 中断编号 |

---

#### nnc6521_clear_lod_int()

```c
void nnc6521_clear_lod_int(uint8_t chip_id);
```

**功能**：清除 LOD 中断标志。

---

#### nnc6521_clear_scd_int()

```c
void nnc6521_clear_scd_int(uint8_t chip_id);
```

**功能**：清除 SCD 中断标志。

---

### 5. 幅度更新函数（中断上下文）

#### nnc6521_customized_amplitude()

```c
void nnc6521_customized_amplitude(
    uint8_t chip_id,
    uint8_t Channel,
    uint8_t u8_PointNum,
    float *normalized_waveform_array,
    uint32_t max_current);
```

**功能**：更新波形幅度（可在中断上下文中调用）。

**参数**：
| 参数 | 类型 | 说明 |
|------|------|------|
| chip_id | uint8_t | 芯片 ID |
| Channel | uint8_t | 通道号 |
| u8_PointNum | uint8_t | 波形点数 |
| normalized_waveform_array | float* | 归一化波形数组 |
| max_current | uint32_t | 最大电流值（μA） |

---

#### nnc6521_customized_amplitude_addr()

```c
__attribute__((section(".nnc6521_amfunc")))
void nnc6521_customized_amplitude_addr(
    uint8_t chip_id,
    uint8_t Channel,
    uint8_t u8_Start_Address,
    uint8_t u8_End_Address,
    float *normalized_waveform_array,
    uint32_t max_current);
```

**功能**：在指定地址范围内更新波形幅度。

**参数**：
| 参数 | 类型 | 说明 |
|------|------|------|
| chip_id | uint8_t | 芯片 ID |
| Channel | uint8_t | 通道号 |
| u8_Start_Address | uint8_t | 起始地址 |
| u8_End_Address | uint8_t | 结束地址 |
| normalized_waveform_array | float* | 归一化波形数组 |
| max_current | uint32_t | 最大电流值（μA） |

**说明**：
- 此函数放置在 `.nnc6521_amfunc` 段，适用于需要快速执行的中断上下文

---

## 波形数据

驱动内置以下归一化波形数组（值范围 0.0 ~ 1.0）：

### 正弦波

| 数组名 | 点数 | 说明 |
|--------|------|------|
| `normalized_sine_waveform_16` | 16 | 16点正弦波 |
| `normalized_sine_waveform_64` | 64 | 64点完整正弦波 |
| `normalized_sine_waveform_64_1` | 32 | 64点正弦波前半段 |
| `normalized_sine_waveform_64_2` | 32 | 64点正弦波后半段 |
| `normalized_sine_waveform_128` | 128 | 128点完整正弦波 |
| `normalized_sine_waveform_128_1` | 64 | 128点正弦波前半段 |
| `normalized_sine_waveform_128_2` | 64 | 128点正弦波后半段 |

### 三角波

| 数组名 | 点数 | 说明 |
|--------|------|------|
| `normalized_triangle_waveform_64` | 64 | 64点完整三角波 |
| `normalized_triangle_waveform_64_1` | 32 | 64点三角波前半段 |
| `normalized_triangle_waveform_64_2` | 32 | 64点三角波后半段 |
| `normalized_triangle_waveform_128` | 128 | 128点完整三角波 |
| `normalized_triangle_waveform_128_1` | 64 | 128点三角波前半段 |
| `normalized_triangle_waveform_128_2` | 64 | 128点三角波后半段 |

### 脉冲波

| 数组名 | 点数 | 说明 |
|--------|------|------|
| `normalized_pulse_waveform_128` | 128 | 128点脉冲波 |

### 用户自定义

| 数组名 | 点数 | 说明 |
|--------|------|------|
| `normalized_user_waveform_128` | 128 | 带指数衰减包络的用户波形 |

---

## 使用流程

### 基本初始化

```c
#include "nnc6521.h"

// 1. 初始化 GPIO
nnc6521_gpio_init();

// 2. 初始化两个芯片
nnc6521_init(NNC6521_CHIP_1);
nnc6521_init(NNC6521_CHIP_2);
```

### 输出正弦波

```c
// 输出 64 点正弦波到芯片 1 通道 0
nnc6521_preloaded_waveform(
    NNC6521_CHIP_1,
    WAVEFORM_GEN_CH0,
    WAVEFORM_SINE,
    64,
    0x04,
    100, 100, 1000, 50
);
```

### 输出自定义波形

```c
// 使用 128 点正弦波数组，最大电流 500μA
nnc6521_customized_waveform(
    NNC6521_CHIP_1,
    WAVEFORM_GEN_CH0,
    128,
    normalized_sine_waveform_128,
    500,
    100, 100, 1000, 50,
    1  // 对称模式
);
```

### 停止波形输出

```c
nnc6521_awg_enable_disable(NNC6521_CHIP_1, WAVEFORM_GEN_CH0, 0);  // 禁用
nnc6521_awg_enable_disable(NNC6521_CHIP_1, WAVEFORM_GEN_CH0, 1);  // 重新使能
```

### 初始化 LOD 检测

```c
nnc6521_lod_init(
    NNC6521_CHIP_1,
    0x00F0,   // 阈值
    0x000000, // 延迟
    0x10      // 目标值
);
```

---

## 注意事项

1. **初始化顺序**：必须先调用 `nnc6521_gpio_init()`，再调用 `nnc6521_init()` 初始化各芯片。

2. **波形点数**：自定义波形的点数必须与传入的数组大小匹配。

3. **电流校准**：驱动内置 OTP 校准数据读取，确保电流输出精度。

4. **中断安全**：`nnc6521_customized_amplitude()` 和 `nnc6521_customized_amplitude_addr()` 可在中断上下文中安全调用。

5. **双芯片操作**：两个芯片可独立操作，使用 `chip_id` 参数指定目标芯片。

---

## 寄存器映射

详见 `nnc6521_reg.h` 文件，包含所有寄存器地址、联合体类型和结构体定义。

### 关键寄存器

| 寄存器 | 地址 | 说明 |
|--------|------|------|
| PMU_REG_ADDR | 0x01 | 电源管理单元 |
| CLK_CTRL_REG_ADDR | 0x02 | 时钟控制 |
| WAVEGEN_GLOBAL_REG_0 | 0x03 | 波形发生器全局控制 |
| LEAD_OFF_CTRL_ADDR | 0x30 | LOD 控制 |
| ANA_ENABLE_REG_1_ADDR | 0x41 | 模拟使能寄存器 1 (CH1) |
| ANA_ENABLE_REG_2_ADDR | 0x42 | 模拟使能寄存器 2 (CH2) |

---

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0 | 2026-06-16 | 初始版本 |
