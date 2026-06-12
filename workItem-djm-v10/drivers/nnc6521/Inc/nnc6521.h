/**
  ******************************************************************************
  * @file    nnc6521.h
  * @brief   NNC6521 dual-chip driver header for RT-Thread + STM32F103
  *          Supports software SPI (GPIO bit-bang) with independent pin mapping
  *          per chip. All comments in English.
  ******************************************************************************
  */

#ifndef __NNC6521_H__
#define __NNC6521_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "nnc6521_reg.h"
#include "stm32f1xx_hal.h"

/* Chip ID definitions -------------------------------------------------------*/
#define NNC6521_CHIP_1      0
#define NNC6521_CHIP_2      1
#define NNC6521_NUM_CHIPS   2

/* Chip 1 pin mapping: MOSI=PC9, CSN=PC8, SCLK=PC7, CHIP_EN=PA12, INTB=PA11, MISO=PA8 */
/* Chip 2 pin mapping: MOSI=PB13, CSN=PB12, SCLK=PB11, CHIP_EN=PC6, INTB=PB15, MISO=PB14 */

typedef struct {
    /* SPI pins */
    GPIO_TypeDef *mosi_port;
    uint16_t      mosi_pin;
    GPIO_TypeDef *csn_port;
    uint16_t      csn_pin;
    GPIO_TypeDef *sclk_port;
    uint16_t      sclk_pin;
    GPIO_TypeDef *miso_port;
    uint16_t      miso_pin;
    /* Control pins */
    GPIO_TypeDef *chip_en_port;
    uint16_t      chip_en_pin;
    GPIO_TypeDef *intb_port;
    uint16_t      intb_pin;
} nnc6521_pin_map_t;

/* Waveform data declarations ------------------------------------------------*/
extern float normalized_sine_waveform_16[16];
extern float normalized_sine_waveform_64[64];
extern float normalized_sine_waveform_64_1[32];
extern float normalized_sine_waveform_64_2[32];
extern float normalized_sine_waveform_128[128];
extern float normalized_sine_waveform_128_1[64];
extern float normalized_sine_waveform_128_2[64];
extern float normalized_triangle_waveform_64[64];
extern float normalized_triangle_waveform_64_1[32];
extern float normalized_triangle_waveform_64_2[32];
extern float normalized_triangle_waveform_128[128];
extern float normalized_triangle_waveform_128_1[64];
extern float normalized_triangle_waveform_128_2[64];
extern float normalized_pulse_waveform_128[128];
extern float normalized_user_waveform_128[128];

/* ============================================================================
 *  GPIO Initialization
 * ===========================================================================*/
/**
 * @brief  Initialize all GPIO pins for both NNC6521 chips.
 *         Configures MOSI/CSN/SCLK/CHIP_EN as push-pull output,
 *         MISO/INTB as floating input. Enables GPIO port clocks.
 */
void nnc6521_gpio_init(void);

/* ============================================================================
 *  SPI Layer (software bit-bang)
 * ===========================================================================*/
/**
 * @brief  Write a register (normal or waveform) to NNC6521 via software SPI.
 * @param  chip_id  NNC6521_CHIP_1 or NNC6521_CHIP_2
 * @param  addr     Register address
 * @param  data     Data byte to write
 * @param  is_wave  1 = waveform register (cmd 0xC0), 0 = normal register (cmd 0x80)
 */
void nnc6521_spi_write(uint8_t chip_id, uint8_t addr, uint8_t data, uint8_t is_wave);

/**
 * @brief  Read a register from NNC6521 via software SPI.
 * @param  chip_id  NNC6521_CHIP_1 or NNC6521_CHIP_2
 * @param  addr     Register address
 * @param  is_wave  1 = waveform register (cmd 0x40), 0 = normal register (cmd 0x00)
 * @return Read byte
 */
uint8_t nnc6521_spi_read(uint8_t chip_id, uint8_t addr, uint8_t is_wave);

/**
 * @brief  Read one byte from OTP memory.
 * @param  chip_id  NNC6521_CHIP_1 or NNC6521_CHIP_2
 * @param  addr     OTP address
 * @return OTP data byte
 */
uint8_t nnc6521_spi_otp_read(uint8_t chip_id, uint8_t addr);

/* Convenience wrappers for normal/waveform registers */
#define nnc6521_write_reg(cid, addr, data)      nnc6521_spi_write((cid), (addr), (data), 0)
#define nnc6521_read_reg(cid, addr)             nnc6521_spi_read((cid), (addr), 0)
#define nnc6521_write_wave_reg(cid, addr, data) nnc6521_spi_write((cid), (addr), (data), 1)
#define nnc6521_read_wave_reg(cid, addr)        nnc6521_spi_read((cid), (addr), 1)

/* ============================================================================
 *  NNC6521 High-Level API
 * ===========================================================================*/

/**
 * @brief  Initialize NNC6521 chip with power-up sequence.
 * @param  chip_id  NNC6521_CHIP_1 or NNC6521_CHIP_2
 */
void nnc6521_init(uint8_t chip_id);

/**
 * @brief  Output preloaded waveform (sine/pulse/triangle).
 */
void nnc6521_preloaded_waveform(uint8_t chip_id,
                                uint8_t u8_Channel,
                                uint8_t u8_Waveform,
                                uint8_t u8_PointNum,
                                uint8_t u8_CI,
                                uint16_t u32_Positive_Interval,
                                uint16_t u32_Negative_Interval,
                                uint32_t u32_Silent_Time,
                                uint16_t u16_Rest_Time);

/**
 * @brief  Output customized waveform from normalized array with calibration.
 */
void nnc6521_customized_waveform(uint8_t chip_id,
                                 uint8_t u8_Channel,
                                 uint8_t u8_PointNum,
                                 float *f_Normalized_array,
                                 uint32_t u32_Max_current,
                                 uint16_t u32_Positive_Interval,
                                 uint16_t u32_Negative_Interval,
                                 uint32_t u32_Silent_Time,
                                 uint16_t u16_Rest_Time,
                                 uint8_t u8_Asymmetric_Symmetric);

/**
 * @brief  Amplitude modulation waveform output.
 */
void nnc6521_amplitude_modulation(uint8_t chip_id,
                                  uint8_t u8_Channel,
                                  uint8_t u8_PointNum,
                                  float *f_Normalized_Envelope_array,
                                  uint32_t u32_Max_current,
                                  uint16_t u16_Carrier,
                                  uint32_t u32_Silent_Time,
                                  uint16_t u16_Interval);

/**
 * @brief  Initialize lead-off detection (LOD).
 */
void nnc6521_lod_init(uint8_t chip_id,
                      uint16_t u16_LOD_Threshold,
                      uint32_t u32_LOD_DLY,
                      uint8_t u8_LOD_TGT);

/**
 * @brief  Initialize short-circuit detection (SCD).
 */
void nnc6521_scd_init(uint8_t chip_id,
                      uint8_t u8_CH0_DAC_Threshold,
                      uint8_t u8_CH0_Addr_0,
                      uint8_t u8_CH0_Addr_1,
                      uint8_t u8_CH0_Int_Num,
                      uint8_t u8_CH1_DAC_Threshold,
                      uint8_t u8_CH1_Addr_0,
                      uint8_t u8_CH1_Addr_1,
                      uint8_t u8_CH1_Int_Num);

/**
 * @brief  Enable or disable waveform generator on a channel.
 */
void nnc6521_awg_enable_disable(uint8_t chip_id,
                                uint8_t AWG_ChannelNum,
                                uint8_t Enable_Disable);

/**
 * @brief  Clear lead-off detection interrupt flag.
 */
void nnc6521_clear_lod_int(uint8_t chip_id);

/**
 * @brief  Clear short-circuit detection interrupt flag.
 */
void nnc6521_clear_scd_int(uint8_t chip_id);

/**
 * @brief  Configure waveform generator registers.
 */
void nnc6521_wavegen_config(uint8_t chip_id,
                            waveform_TypeDef *wf,
                            float *normalized_waveform_array,
                            uint32_t max_current);

/**
 * @brief  Update waveform amplitude (used in interrupt context).
 */
void nnc6521_customized_amplitude(uint8_t chip_id,
                                  uint8_t Channel,
                                  uint8_t u8_PointNum,
                                  float *normalized_waveform_array,
                                  uint32_t max_current);

/**
 * @brief  Update waveform amplitude at specific address range.
 */
__attribute__((section(".nnc6521_amfunc")))
void nnc6521_customized_amplitude_addr(uint8_t chip_id,
                                       uint8_t Channel,
                                       uint8_t u8_Start_Address,
                                       uint8_t u8_End_Address,
                                       float *normalized_waveform_array,
                                       uint32_t max_current);

#ifdef __cplusplus
}
#endif

#endif /* __NNC6521_H__ */
