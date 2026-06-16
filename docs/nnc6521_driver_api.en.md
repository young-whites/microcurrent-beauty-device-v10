# NNC6521 Dual-Chip Driver API Reference

## Overview

NNC6521 is a dual-channel micro-current stimulation chip designed for medical electronic devices such as beauty instruments. This driver is built on the RT-Thread + STM32F103 platform, utilizing software SPI (GPIO bit-bang) communication with independent dual-chip control.

### Features

- Independent dual-chip control, each supporting 2 channels
- Software SPI communication, CPOL=0, CPHA=0, MSB first
- Support for preloaded waveforms (sine, triangle, pulse) and custom waveforms
- Amplitude modulation output support
- Integrated Lead-Off Detection (LOD) and Short-Circuit Detection (SCD)
- Built-in current calibration functionality

---

## File Structure

```
applications/macBSP/
├── Inc/
│   ├── nnc6521.h          # Main header with API declarations and data structures
│   └── nnc6521_reg.h      # Register definitions and enumeration types
└── Src/
    ├── nnc6521_drv.c      # Core driver implementation
    ├── nnc6521_spi.c      # Software SPI communication implementation
    └── nnc6521_waveform.c # Preloaded waveform data
```

---

## Pin Mapping

| Signal | Chip 1 | Chip 2 |
|--------|--------|--------|
| MOSI | PC9 | PB13 |
| CSN | PC8 | PB12 |
| SCLK | PC7 | PB11 |
| MISO | PA8 | PB14 |
| CHIP_EN | PA12 | PC6 |
| INTB | PA11 | PB15 |

---

## API Reference

### 1. Initialization Functions

#### nnc6521_gpio_init()

```c
void nnc6521_gpio_init(void);
```

**Function**: Initialize all GPIO pins for both NNC6521 chips.

**Description**:
- Configures MOSI/CSN/SCLK/CHIP_EN as push-pull output
- Configures MISO/INTB as floating input
- Enables GPIO port clocks for all pins

**When to call**: Once during system startup.

---

#### nnc6521_init()

```c
void nnc6521_init(uint8_t chip_id);
```

**Function**: Initialize the specified chip with power-up sequence.

**Parameters**:
| Parameter | Type | Description |
|-----------|------|-------------|
| chip_id | uint8_t | Chip ID: NNC6521_CHIP_1 (0) or NNC6521_CHIP_2 (1) |

**Description**:
- Executes CHIP_EN power-up sequence: LOW → delay → HIGH → delay → ready
- Resets waveform generator
- Configures clock and analog modules

**Usage Example**:
```c
nnc6521_gpio_init();              // Initialize GPIO
nnc6521_init(NNC6521_CHIP_1);     // Initialize chip 1
nnc6521_init(NNC6521_CHIP_2);     // Initialize chip 2
```

---

### 2. SPI Communication Functions

#### nnc6521_spi_write()

```c
void nnc6521_spi_write(uint8_t chip_id, uint8_t addr, uint8_t data, uint8_t is_wave);
```

**Function**: Write a register via software SPI.

**Parameters**:
| Parameter | Type | Description |
|-----------|------|-------------|
| chip_id | uint8_t | Chip ID |
| addr | uint8_t | Register address |
| data | uint8_t | Data to write |
| is_wave | uint8_t | 1 = waveform register (cmd 0xC0), 0 = normal register (cmd 0x80) |

---

#### nnc6521_spi_read()

```c
uint8_t nnc6521_spi_read(uint8_t chip_id, uint8_t addr, uint8_t is_wave);
```

**Function**: Read a register via software SPI.

**Parameters**:
| Parameter | Type | Description |
|-----------|------|-------------|
| chip_id | uint8_t | Chip ID |
| addr | uint8_t | Register address |
| is_wave | uint8_t | 1 = waveform register (cmd 0x40), 0 = normal register (cmd 0x00) |

**Returns**: Data byte read from register.

---

#### nnc6521_spi_otp_read()

```c
uint8_t nnc6521_spi_otp_read(uint8_t chip_id, uint8_t addr);
```

**Function**: Read one byte from OTP memory.

**Parameters**:
| Parameter | Type | Description |
|-----------|------|-------------|
| chip_id | uint8_t | Chip ID |
| addr | uint8_t | OTP address |

**Returns**: OTP data byte.

---

#### Convenience Macros

```c
#define nnc6521_write_reg(cid, addr, data)      nnc6521_spi_write((cid), (addr), (data), 0)
#define nnc6521_read_reg(cid, addr)             nnc6521_spi_read((cid), (addr), 0)
#define nnc6521_write_wave_reg(cid, addr, data) nnc6521_spi_write((cid), (addr), (data), 1)
#define nnc6521_read_wave_reg(cid, addr)        nnc6521_spi_read((cid), (addr), 1)
```

---

### 3. Waveform Output Functions

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

**Function**: Output preloaded waveform (sine/pulse/triangle).

**Parameters**:
| Parameter | Type | Description |
|-----------|------|-------------|
| chip_id | uint8_t | Chip ID |
| u8_Channel | uint8_t | Channel number: WAVEFORM_GEN_CH0 (0) or WAVEFORM_GEN_CH1 (1) |
| u8_Waveform | uint8_t | Waveform type: WAVEFORM_SINE (0), WAVEFORM_PULSE (1), WAVEFORM_TRIANGLE (2) |
| u8_PointNum | uint8_t | Number of waveform points (16/64/128) |
| u8_CI | uint8_t | Current intensity level |
| u32_Positive_Interval | uint16_t | Positive half-cycle interval (clock cycles) |
| u32_Negative_Interval | uint16_t | Negative half-cycle interval (clock cycles) |
| u32_Silent_Time | uint32_t | Silent time (clock cycles) |
| u16_Rest_Time | uint16_t | Rest time (clock cycles) |

**Usage Example**:
```c
// Output 64-point sine wave on channel 0
nnc6521_preloaded_waveform(
    NNC6521_CHIP_1,
    WAVEFORM_GEN_CH0,
    WAVEFORM_SINE,
    64,
    0x04,      // Current level
    100,       // Positive half-cycle interval
    100,       // Negative half-cycle interval
    1000,      // Silent time
    50         // Rest time
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

**Function**: Output custom waveform with calibration using normalized array.

**Parameters**:
| Parameter | Type | Description |
|-----------|------|-------------|
| chip_id | uint8_t | Chip ID |
| u8_Channel | uint8_t | Channel number |
| u8_PointNum | uint8_t | Number of waveform points |
| f_Normalized_array | float* | Normalized waveform array (0.0~1.0) |
| u32_Max_current | uint32_t | Maximum current value (μA) |
| u32_Positive_Interval | uint16_t | Positive half-cycle interval |
| u32_Negative_Interval | uint16_t | Negative half-cycle interval |
| u32_Silent_Time | uint32_t | Silent time |
| u16_Rest_Time | uint16_t | Rest time |
| u8_Asymmetric_Symmetric | uint8_t | 0 = asymmetric waveform, 1 = symmetric waveform |

**Usage Example**:
```c
// Custom 128-point sine wave, max current 500μA
extern float normalized_sine_waveform_128[128];

nnc6521_customized_waveform(
    NNC6521_CHIP_1,
    WAVEFORM_GEN_CH0,
    128,
    normalized_sine_waveform_128,
    500,       // Max current 500μA
    100,
    100,
    1000,
    50,
    1          // Symmetric mode
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

**Function**: Output amplitude modulated waveform.

**Parameters**:
| Parameter | Type | Description |
|-----------|------|-------------|
| chip_id | uint8_t | Chip ID |
| u8_Channel | uint8_t | Channel number |
| u8_PointNum | uint8_t | Envelope waveform point count |
| f_Normalized_Envelope_array | float* | Normalized envelope array |
| u32_Max_current | uint32_t | Maximum current value (μA) |
| u16_Carrier | uint16_t | Carrier frequency |
| u32_Silent_Time | uint32_t | Silent time |
| u16_Interval | uint16_t | Interval time |

---

#### nnc6521_awg_enable_disable()

```c
void nnc6521_awg_enable_disable(uint8_t chip_id, uint8_t AWG_ChannelNum, uint8_t Enable_Disable);
```

**Function**: Enable or disable waveform generator on specified channel.

**Parameters**:
| Parameter | Type | Description |
|-----------|------|-------------|
| chip_id | uint8_t | Chip ID |
| AWG_ChannelNum | uint8_t | Channel number |
| Enable_Disable | uint8_t | 1 = enable, 0 = disable |

---

### 4. Detection Functions

#### nnc6521_lod_init()

```c
void nnc6521_lod_init(
    uint8_t chip_id,
    uint16_t u16_LOD_Threshold,
    uint32_t u32_LOD_DLY,
    uint8_t u8_LOD_TGT);
```

**Function**: Initialize Lead-Off Detection (LOD) functionality.

**Parameters**:
| Parameter | Type | Description |
|-----------|------|-------------|
| chip_id | uint8_t | Chip ID |
| u16_LOD_Threshold | uint16_t | LOD threshold value |
| u32_LOD_DLY | uint32_t | LOD delay time |
| u8_LOD_TGT | uint8_t | LOD target value |

**Description**:
- Configures VDAC for both CH1 and CH2
- Enables high-level detection mode

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

**Function**: Initialize Short-Circuit Detection (SCD) functionality.

**Parameters**:
| Parameter | Type | Description |
|-----------|------|-------------|
| chip_id | uint8_t | Chip ID |
| u8_CH0_DAC_Threshold | uint8_t | CH1 DAC threshold |
| u8_CH0_Addr_0 | uint8_t | CH1 address 0 |
| u8_CH0_Addr_1 | uint8_t | CH1 address 1 |
| u8_CH0_Int_Num | uint8_t | CH1 interrupt number |
| u8_CH1_DAC_Threshold | uint8_t | CH2 DAC threshold |
| u8_CH1_Addr_0 | uint8_t | CH2 address 0 |
| u8_CH1_Addr_1 | uint8_t | CH2 address 1 |
| u8_CH1_Int_Num | uint8_t | CH2 interrupt number |

---

#### nnc6521_clear_lod_int()

```c
void nnc6521_clear_lod_int(uint8_t chip_id);
```

**Function**: Clear LOD interrupt flag.

---

#### nnc6521_clear_scd_int()

```c
void nnc6521_clear_scd_int(uint8_t chip_id);
```

**Function**: Clear SCD interrupt flag.

---

### 5. Amplitude Update Functions (Interrupt Context)

#### nnc6521_customized_amplitude()

```c
void nnc6521_customized_amplitude(
    uint8_t chip_id,
    uint8_t Channel,
    uint8_t u8_PointNum,
    float *normalized_waveform_array,
    uint32_t max_current);
```

**Function**: Update waveform amplitude (can be called in interrupt context).

**Parameters**:
| Parameter | Type | Description |
|-----------|------|-------------|
| chip_id | uint8_t | Chip ID |
| Channel | uint8_t | Channel number |
| u8_PointNum | uint8_t | Number of waveform points |
| normalized_waveform_array | float* | Normalized waveform array |
| max_current | uint32_t | Maximum current value (μA) |

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

**Function**: Update waveform amplitude within specified address range.

**Parameters**:
| Parameter | Type | Description |
|-----------|------|-------------|
| chip_id | uint8_t | Chip ID |
| Channel | uint8_t | Channel number |
| u8_Start_Address | uint8_t | Start address |
| u8_End_Address | uint8_t | End address |
| normalized_waveform_array | float* | Normalized waveform array |
| max_current | uint32_t | Maximum current value (μA) |

**Description**:
- This function is placed in `.nnc6521_amfunc` section for fast execution in interrupt context

---

## Waveform Data

The driver includes the following normalized waveform arrays (values range 0.0 ~ 1.0):

### Sine Wave

| Array Name | Points | Description |
|------------|--------|-------------|
| `normalized_sine_waveform_16` | 16 | 16-point sine wave |
| `normalized_sine_waveform_64` | 64 | 64-point complete sine wave |
| `normalized_sine_waveform_64_1` | 32 | 64-point sine wave first half |
| `normalized_sine_waveform_64_2` | 32 | 64-point sine wave second half |
| `normalized_sine_waveform_128` | 128 | 128-point complete sine wave |
| `normalized_sine_waveform_128_1` | 64 | 128-point sine wave first half |
| `normalized_sine_waveform_128_2` | 64 | 128-point sine wave second half |

### Triangle Wave

| Array Name | Points | Description |
|------------|--------|-------------|
| `normalized_triangle_waveform_64` | 64 | 64-point complete triangle wave |
| `normalized_triangle_waveform_64_1` | 32 | 64-point triangle wave first half |
| `normalized_triangle_waveform_64_2` | 32 | 64-point triangle wave second half |
| `normalized_triangle_waveform_128` | 128 | 128-point complete triangle wave |
| `normalized_triangle_waveform_128_1` | 64 | 128-point triangle wave first half |
| `normalized_triangle_waveform_128_2` | 64 | 128-point triangle wave second half |

### Pulse Wave

| Array Name | Points | Description |
|------------|--------|-------------|
| `normalized_pulse_waveform_128` | 128 | 128-point pulse wave |

### User Custom

| Array Name | Points | Description |
|------------|--------|-------------|
| `normalized_user_waveform_128` | 128 | User waveform with exponential decay envelope |

---

## Usage Flow

### Basic Initialization

```c
#include "nnc6521.h"

// 1. Initialize GPIO
nnc6521_gpio_init();

// 2. Initialize both chips
nnc6521_init(NNC6521_CHIP_1);
nnc6521_init(NNC6521_CHIP_2);
```

### Output Sine Wave

```c
// Output 64-point sine wave to chip 1, channel 0
nnc6521_preloaded_waveform(
    NNC6521_CHIP_1,
    WAVEFORM_GEN_CH0,
    WAVEFORM_SINE,
    64,
    0x04,
    100, 100, 1000, 50
);
```

### Output Custom Waveform

```c
// Use 128-point sine wave array, max current 500μA
nnc6521_customized_waveform(
    NNC6521_CHIP_1,
    WAVEFORM_GEN_CH0,
    128,
    normalized_sine_waveform_128,
    500,
    100, 100, 1000, 50,
    1  // Symmetric mode
);
```

### Stop Waveform Output

```c
nnc6521_awg_enable_disable(NNC6521_CHIP_1, WAVEFORM_GEN_CH0, 0);  // Disable
nnc6521_awg_enable_disable(NNC6521_CHIP_1, WAVEFORM_GEN_CH0, 1);  // Re-enable
```

### Initialize LOD Detection

```c
nnc6521_lod_init(
    NNC6521_CHIP_1,
    0x00F0,   // Threshold
    0x000000, // Delay
    0x10      // Target value
);
```

---

## Important Notes

1. **Initialization Order**: `nnc6521_gpio_init()` must be called before `nnc6521_init()` for each chip.

2. **Waveform Point Count**: Custom waveform point count must match the size of the input array.

3. **Current Calibration**: The driver includes built-in OTP calibration data reading for accurate current output.

4. **Interrupt Safety**: `nnc6521_customized_amplitude()` and `nnc6521_customized_amplitude_addr()` are safe to call from interrupt context.

5. **Dual-Chip Operation**: Both chips can be operated independently using the `chip_id` parameter to specify the target chip.

---

## Register Map

Refer to `nnc6521_reg.h` for complete register addresses, union types, and structure definitions.

### Key Registers

| Register | Address | Description |
|----------|---------|-------------|
| PMU_REG_ADDR | 0x01 | Power Management Unit |
| CLK_CTRL_REG_ADDR | 0x02 | Clock Control |
| WAVEGEN_GLOBAL_REG_0 | 0x03 | Waveform Generator Global Control |
| LEAD_OFF_CTRL_ADDR | 0x30 | LOD Control |
| ANA_ENABLE_REG_1_ADDR | 0x41 | Analog Enable Register 1 (CH1) |
| ANA_ENABLE_REG_2_ADDR | 0x42 | Analog Enable Register 2 (CH2) |

---

## Revision History

| Version | Date | Description |
|---------|------|-------------|
| 1.0 | 2026-06-16 | Initial version |
