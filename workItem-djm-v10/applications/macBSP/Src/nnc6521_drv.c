/**
  ******************************************************************************
  * @file    nnc6521_drv.c
  * @brief   NNC6521 driver adapted from ens1p4.c for dual-chip software SPI.
  *          All SPI calls routed through nnc6521_spi_xxx with chip_id.
  *          Current calibration functions preserved as-is.
  *          All comments in English.
  ******************************************************************************
  */

#include "nnc6521.h"

/* ============================================================================
 *  Private function prototypes (current calibration)
 * ===========================================================================*/
static void generate_scaled_wave(uint16_t *output, uint8_t u8_PointNum,
                                 float *base_wave, float amplitude);
static void convert_16bit_to_8bit(uint8_t *dst, uint8_t *bits_shift,
                                  uint16_t *src, int length);
static uint8_t bits_used(uint16_t value);
static uint8_t CurIsInSide(uint16_t current, uint16_t Imin, uint16_t Imax);
static uint16_t GetDif(uint16_t current, uint16_t Imin, uint16_t Imax);
static uint16_t GetDrefIselIseg(uint8_t chip_id, uint8_t aChan,
                                uint16_t current, uint8_t *aSeg);
static uint32_t Current_Output(uint8_t chip_id, uint32_t current, uint8_t channel);

/* ============================================================================
 *  Chip Initialization with CHIP_EN Power-Up Sequence
 * ===========================================================================*/
void nnc6521_init(uint8_t chip_id)
{
    if (chip_id >= NNC6521_NUM_CHIPS) return;

    /* Get the pin map for this chip */
    /* We need access to chip_en port/pin. Since the pin map is static in
     * nnc6521_spi.c, we replicate the essential init here using the same
     * GPIO constants. For a cleaner implementation, the pin map could be
     * exposed via nnc6521.h, but keeping encapsulation. */

    /* CHIP_EN power-up sequence: low -> delay -> high -> delay -> ready */
    /* The actual GPIO is handled via nnc6521_spi.c's pin table.
     * We use direct HAL calls here for clarity. */
    switch (chip_id) {
        case NNC6521_CHIP_1:
            __HAL_RCC_GPIOA_CLK_ENABLE();
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
            /* ~10ms delay (use rt_thread_mdelay in RT-Thread context) */
            {
                volatile uint32_t delay = 80000;
                while (delay--) __NOP();
            }
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
            {
                volatile uint32_t delay = 80000;
                while (delay--) __NOP();
            }
            break;

        case NNC6521_CHIP_2:
            __HAL_RCC_GPIOC_CLK_ENABLE();
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
            {
                volatile uint32_t delay = 80000;
                while (delay--) __NOP();
            }
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
            {
                volatile uint32_t delay = 80000;
                while (delay--) __NOP();
            }
            break;
    }

    /* Reset waveform generator */
    nnc6521_write_reg(chip_id, PMU_REG_ADDR, 0x00);

    /* Basic clock and analog setup */
    nnc6521_write_reg(chip_id, CLK_CTRL_REG_ADDR, 0x00);     /* PCLK div = 1 (2MHz) */
    nnc6521_write_reg(chip_id, WAVEGEN_GLOBAL_REG_0, 0x01);  /* Global drive enable */
}

/* ============================================================================
 *  AWG Enable/Disable
 * ===========================================================================*/
void nnc6521_awg_enable_disable(uint8_t chip_id, uint8_t AWG_ChannelNum,
                                uint8_t Enable_Disable)
{
    waveform_TypeDef wf = {0};
    wf.WG_DRV_CTRL_REG0.value = nnc6521_read_wave_reg(chip_id,
        WG_REG_ADDR(AWG_ChannelNum, WG_DRV_CTRL_REG0_OFFSET));
    wf.WG_DRV_CTRL_REG0.bits.enable_wavegen = Enable_Disable;
    nnc6521_write_wave_reg(chip_id,
        WG_REG_ADDR(AWG_ChannelNum, WG_DRV_CTRL_REG0_OFFSET),
        wf.WG_DRV_CTRL_REG0.value);
}

/* ============================================================================
 *  Preloaded Waveform Output
 * ===========================================================================*/
void nnc6521_preloaded_waveform(uint8_t chip_id,
                                uint8_t u8_Channel,
                                uint8_t u8_Waveform,
                                uint8_t u8_PointNum,
                                uint8_t u8_CI,
                                uint16_t u32_Positive_Interval,
                                uint16_t u32_Negative_Interval,
                                uint32_t u32_Silent_Time,
                                uint16_t u16_Rest_Time)
{
    waveform_TypeDef wf = {0};
    wf.CHANNEL = u8_Channel;
    wf.WG_DRV_POINT_CONFIG.value = u8_PointNum;
    wf.WG_DRV_CTRL_REG0.bits.symmetric_wave = 0;

    wf.WG_DRV_CONFIG_REG0.bits.rest_enable = 1;
    wf.WG_DRV_CONFIG_REG0.bits.negative_enable = 1;
    wf.WG_DRV_CONFIG_REG0.bits.silent_enable = 1;
    wf.WG_DRV_CONFIG_REG0.bits.sourceB_enable = 1;
    wf.WG_DRV_CONFIG_REG0.bits.multi_electrode = 1;
    wf.WG_DRV_CONFIG_REG0.bits.continue_repeat = 1;
    wf.WG_DRIVE_REG_CTRL2.bits.out_pos = u8_CI;
    wf.WG_DRV_SILENT_CLK.value = u32_Silent_Time;
    wf.WG_DRV_HALF_WAVE_CLK_PNT.value = u32_Positive_Interval;
    wf.WG_DRV_NEG_HALF_WAVE_CLK_PNT.value = u32_Negative_Interval;
    wf.WG_DRV_REST_CLK.value = u16_Rest_Time;
    wf.WG_DRV_CTRL_REG0.bits.enable_wavegen = 1;
    wf.WG_DRV_CTRL_REG0.bits.waveform_select = u8_Waveform;
    wf.WG_DRV_CTRL_REG0.bits.preload_mode = 0;

    nnc6521_wavegen_config(chip_id, &wf, NULL, 0);
}

/* ============================================================================
 *  Customized Waveform Output
 * ===========================================================================*/
void nnc6521_customized_waveform(uint8_t chip_id,
                                 uint8_t u8_Channel,
                                 uint8_t u8_PointNum,
                                 float *f_Normalized_array,
                                 uint32_t u32_Max_current,
                                 uint16_t u32_Positive_Interval,
                                 uint16_t u32_Negative_Interval,
                                 uint32_t u32_Silent_Time,
                                 uint16_t u16_Rest_Time,
                                 uint8_t u8_Asymmetric_Symmetric)
{
    waveform_TypeDef wf = {0};
    wf.CHANNEL = u8_Channel;

    if (u8_Asymmetric_Symmetric == 0) {
        wf.WG_DRV_POINT_CONFIG.value = u8_PointNum;
        wf.WG_DRV_CTRL_REG0.bits.symmetric_wave = 0;
    } else {
        wf.WG_DRV_POINT_CONFIG.value = u8_PointNum / 2;
        wf.WG_DRV_CTRL_REG0.bits.symmetric_wave = 1;
    }

    wf.WG_DRV_CONFIG_REG0.bits.rest_enable = 1;
    wf.WG_DRV_CONFIG_REG0.bits.negative_enable = 1;
    wf.WG_DRV_CONFIG_REG0.bits.silent_enable = 1;
    wf.WG_DRV_CONFIG_REG0.bits.sourceB_enable = 1;
    wf.WG_DRV_CONFIG_REG0.bits.multi_electrode = 1;
    wf.WG_DRV_CONFIG_REG0.bits.continue_repeat = 1;
    wf.WG_DRV_SILENT_CLK.value = u32_Silent_Time;
    wf.WG_DRV_HALF_WAVE_CLK_PNT.value = u32_Positive_Interval;
    wf.WG_DRV_NEG_HALF_WAVE_CLK_PNT.value = u32_Negative_Interval;
    wf.WG_DRV_REST_CLK.value = u16_Rest_Time;
    wf.WG_DRV_CTRL_REG0.bits.enable_wavegen = 1;
    wf.WG_DRV_CTRL_REG0.bits.waveform_select = WAVEFORM_SPI;
    wf.WG_DRV_CTRL_REG0.bits.preload_mode = 0;

    nnc6521_wavegen_config(chip_id, &wf, f_Normalized_array, u32_Max_current);
}

/* ============================================================================
 *  Amplitude Modulation
 * ===========================================================================*/
void nnc6521_amplitude_modulation(uint8_t chip_id,
                                  uint8_t u8_Channel,
                                  uint8_t u8_PointNum,
                                  float *f_Normalized_Envelope_array,
                                  uint32_t u32_Max_current,
                                  uint16_t u16_Carrier,
                                  uint32_t u32_Silent_Time,
                                  uint16_t u16_Interval)
{
    waveform_TypeDef wf = {0};
    wf.CHANNEL = u8_Channel;
    wf.WG_DRV_POINT_CONFIG.value = u8_PointNum;
    wf.WG_DRV_CONFIG_REG0.bits.rest_enable = 1;
    wf.WG_DRV_CONFIG_REG0.bits.negative_enable = 0;
    wf.WG_DRV_CONFIG_REG0.bits.silent_enable = 1;
    wf.WG_DRV_CONFIG_REG0.bits.sourceB_enable = 1;
    wf.WG_DRV_CONFIG_REG0.bits.multi_electrode = 1;
    wf.WG_DRV_CONFIG_REG0.bits.alternating_pos = 1;
    wf.WG_DRV_SILENT_CLK.value = u32_Silent_Time;
    wf.WG_DRV_HALF_WAVE_CLK_PNT.value = u16_Interval;
    wf.WG_DRV_REST_CLK.value = 0x00;
    wf.WG_DRV_CTRL_REG0.bits.enable_wavegen = 1;
    wf.WG_DRV_CTRL_REG0.bits.waveform_select = WAVEFORM_SPI;
    wf.WG_DRV_CTRL_REG0.bits.symmetric_wave = 1;
    wf.WG_DRV_ALT_LIM.value = u16_Carrier;
    wf.WG_DRV_ALT_SILENT_LIM.value = 0x0000;

    nnc6521_wavegen_config(chip_id, &wf, f_Normalized_Envelope_array, u32_Max_current);
}

/* ============================================================================
 *  Lead-Off Detection (LOD) Init
 * ===========================================================================*/
static void nnc6521_leadoff_init(uint8_t chip_id)
{
    nnc6521_write_reg(chip_id, LEAD_OFF_CTRL_ADDR, 0x30);
    nnc6521_write_reg(chip_id, LEAD_OFF_THR_H_0_ADDR, 0xF0);
    nnc6521_write_reg(chip_id, LEAD_OFF_THR_H_1_ADDR, 0x00);
    nnc6521_write_reg(chip_id, LEAD_OFF_THR_L_0_ADDR, 0x10);
    nnc6521_write_reg(chip_id, LEAD_OFF_THR_L_1_ADDR, 0x10);
    nnc6521_write_reg(chip_id, LEAD_OFF_DLY_TGT_0_ADDR, 0x00);
    nnc6521_write_reg(chip_id, LEAD_OFF_DLY_TGT_1_ADDR, 0x00);
    nnc6521_write_reg(chip_id, LEAD_OFF_DLY_TGT_2_ADDR, 0x00);
    nnc6521_write_reg(chip_id, LEAD_OFF_DLY_TGT_3_ADDR, 0x00);
    nnc6521_write_reg(chip_id, LEAD_OFF_TGT_ADDR, 0x10);
    nnc6521_write_reg(chip_id, LEAD_OFF_INT_ADDR, 0x10);
    nnc6521_write_reg(chip_id, LEAD_OFF_ANA_ADDR, 0x00);
}

static void nnc6521_leadoff_config(uint8_t chip_id, Leadoff_TypeDef *LO)
{
    uint8_t data;

    nnc6521_write_reg(chip_id, LEAD_OFF_CTRL_ADDR, LO->lead_off_ctrl.all);
    nnc6521_write_reg(chip_id, LEAD_OFF_THR_H_0_ADDR, LO->lead_off_thr_h_0.all);
    nnc6521_write_reg(chip_id, LEAD_OFF_THR_H_1_ADDR, LO->lead_off_thr_h_1.all);
    nnc6521_write_reg(chip_id, LEAD_OFF_THR_L_0_ADDR, LO->lead_off_thr_l_0.all);
    nnc6521_write_reg(chip_id, LEAD_OFF_THR_L_1_ADDR, LO->lead_off_thr_l_1.all);
    nnc6521_write_reg(chip_id, LEAD_OFF_DLY_TGT_0_ADDR, LO->lead_off_dly_tgt[0].all);
    nnc6521_write_reg(chip_id, LEAD_OFF_DLY_TGT_1_ADDR, LO->lead_off_dly_tgt[1].all);
    nnc6521_write_reg(chip_id, LEAD_OFF_DLY_TGT_2_ADDR, LO->lead_off_dly_tgt[2].all);
    nnc6521_write_reg(chip_id, LEAD_OFF_DLY_TGT_3_ADDR, LO->lead_off_dly_tgt[3].all);
    nnc6521_write_reg(chip_id, LEAD_OFF_TGT_ADDR, LO->lead_off_tgt.all);
    nnc6521_write_reg(chip_id, LEAD_OFF_INT_ADDR, LO->lead_off_int.all);
    nnc6521_write_reg(chip_id, LEAD_OFF_ANA_ADDR, LO->lead_off_ana.all);

    if ((LO->lead_off_ctrl.bits.dac_sel & 0x01) == 1) {
        data = nnc6521_read_reg(chip_id, ANA_INTR_EN_ADDR) & ~(1 << 1);
        nnc6521_write_reg(chip_id, ANA_INTR_EN_ADDR, data);

        data = nnc6521_read_reg(chip_id, ANA_GEN_REG_2_ADDR) | LO->ANA_GEN_REG_2.all;
        nnc6521_write_reg(chip_id, ANA_GEN_REG_2_ADDR, data);

        data = nnc6521_read_reg(chip_id, ANA_GEN_REG_3_ADDR) | LO->ANA_GEN_REG_3.all;
        nnc6521_write_reg(chip_id, ANA_GEN_REG_3_ADDR, data);

        data = nnc6521_read_reg(chip_id, ANA_ENABLE_REG_1_ADDR) | LO->ANA_ENABLE_REG_1.all;
        nnc6521_write_reg(chip_id, ANA_ENABLE_REG_1_ADDR, data);
    }

    if ((LO->lead_off_ctrl.bits.dac_sel & 0x02) == 2) {
        data = nnc6521_read_reg(chip_id, ANA_INTR_EN_ADDR) & ~(1 << 2);
        nnc6521_write_reg(chip_id, ANA_INTR_EN_ADDR, data);

        data = nnc6521_read_reg(chip_id, ANA_GEN_REG_4_ADDR) | LO->ANA_GEN_REG_4.all;
        nnc6521_write_reg(chip_id, ANA_GEN_REG_4_ADDR, data);

        data = nnc6521_read_reg(chip_id, ANA_GEN_REG_5_ADDR) | LO->ANA_GEN_REG_5.all;
        nnc6521_write_reg(chip_id, ANA_GEN_REG_5_ADDR, data);

        data = nnc6521_read_reg(chip_id, ANA_ENABLE_REG_2_ADDR) | LO->ANA_ENABLE_REG_2.all;
        nnc6521_write_reg(chip_id, ANA_ENABLE_REG_2_ADDR, data);
    }
}

void nnc6521_lod_init(uint8_t chip_id,
                      uint16_t u16_LOD_Threshold,
                      uint32_t u32_LOD_DLY,
                      uint8_t u8_LOD_TGT)
{
    nnc6521_leadoff_init(chip_id);

    Leadoff_TypeDef lo = {0};
    lo.lead_off_ctrl.bits.dac_sel = 0x03;  /* LO_ALL: both CH1 and CH2 VDAC on */
    lo.lead_off_ctrl.bits.check_mode = 0x01; /* High-level detection */
    lo.lead_off_int.bits.lead_off_int_en = 0;

    lo.lead_off_thr_h_0.all = u16_LOD_Threshold & 0x00FF;
    lo.lead_off_thr_h_1.all = u16_LOD_Threshold >> 8;

    lo.lead_off_dly_tgt[0].all = (uint8_t)(u32_LOD_DLY & 0xFF);
    lo.lead_off_dly_tgt[1].all = (uint8_t)((u32_LOD_DLY >> 8) & 0xFF);
    lo.lead_off_dly_tgt[2].all = (uint8_t)((u32_LOD_DLY >> 16) & 0xFF);
    lo.lead_off_dly_tgt[3].all = (uint8_t)((u32_LOD_DLY >> 24) & 0xFF);
    lo.lead_off_tgt.all = u8_LOD_TGT;

    lo.ANA_GEN_REG_2.all = 0x04;
    lo.ANA_GEN_REG_3.bits.D2A_VDAC_AMPCHOP_CH1 = 1;
    lo.ANA_GEN_REG_3.bits.COMP_ISEL_CH1 = 0;
    lo.ANA_GEN_REG_3.bits.DRIVERA_CSAMP_CH_CH1 = 1;
    lo.ANA_GEN_REG_3.bits.VDAC_DIN_CH1_MSB = 0;
    lo.ANA_ENABLE_REG_1.bits.VDAC_EN_CH1 = 1;
    lo.ANA_ENABLE_REG_1.bits.COMP_EN_CH1 = 1;
    lo.ANA_ENABLE_REG_1.bits.DRIVERA_CSAMP_EN_CH1 = 1;

    lo.ANA_GEN_REG_4.all = 0x04;
    lo.ANA_GEN_REG_5.bits.D2A_VDAC_AMPCHOP_CH2 = 1;
    lo.ANA_GEN_REG_5.bits.COMP_ISEL_CH2 = 0;
    lo.ANA_GEN_REG_5.bits.DRIVERA_CSAMP_CH_CH2 = 1;
    lo.ANA_GEN_REG_5.bits.VDAC_DIN_CH2_MSB = 0;
    lo.ANA_ENABLE_REG_2.bits.VDAC_EN_CH2 = 1;
    lo.ANA_ENABLE_REG_2.bits.COMP_EN_CH2 = 1;
    lo.ANA_ENABLE_REG_2.bits.DRIVERA_CSAMP_EN_CH2 = 1;

    nnc6521_leadoff_config(chip_id, &lo);
    nnc6521_clear_lod_int(chip_id);
}

/* ============================================================================
 *  Short-Circuit Detection (SCD) Init
 * ===========================================================================*/
static void nnc6521_short_detect_config(uint8_t chip_id, Short_detect_TypeDef *SO)
{
    uint8_t data;

    nnc6521_write_reg(chip_id, LEAD_OFF_CTRL_ADDR, SO->lead_off_ctrl.all);

    data = nnc6521_read_reg(chip_id, ANA_ENABLE_REG_1_ADDR) | SO->ANA_ENABLE_REG_1.all;
    nnc6521_write_reg(chip_id, ANA_ENABLE_REG_1_ADDR, data);

    data = nnc6521_read_reg(chip_id, ANA_ENABLE_REG_2_ADDR) | SO->ANA_ENABLE_REG_2.all;
    nnc6521_write_reg(chip_id, ANA_ENABLE_REG_2_ADDR, data);

    nnc6521_write_reg(chip_id, ANA_ENABLE_REG_3_ADDR, SO->ANA_ENABLE_REG_3.all);

    data = nnc6521_read_reg(chip_id, ANA_GEN_REG_2_ADDR) | SO->ANA_GEN_REG_2.all;
    nnc6521_write_reg(chip_id, ANA_GEN_REG_2_ADDR, data);

    data = nnc6521_read_reg(chip_id, ANA_GEN_REG_3_ADDR) | SO->ANA_GEN_REG_3.all;
    nnc6521_write_reg(chip_id, ANA_GEN_REG_3_ADDR, data);

    data = nnc6521_read_reg(chip_id, ANA_GEN_REG_4_ADDR) | SO->ANA_GEN_REG_4.all;
    nnc6521_write_reg(chip_id, ANA_GEN_REG_4_ADDR, data);

    data = nnc6521_read_reg(chip_id, ANA_GEN_REG_5_ADDR) | SO->ANA_GEN_REG_5.all;
    nnc6521_write_reg(chip_id, ANA_GEN_REG_5_ADDR, data);

    nnc6521_write_reg(chip_id, ANA_INT_COMP_POL_ADDR, SO->ANA_INT_COMP_POL.all);
    nnc6521_write_reg(chip_id, ANA_INT_STOP_WAVEGEN_ADDR, SO->ANA_INT_STOP_WAVEGEN.all);

    nnc6521_write_reg(chip_id, ANA_INT_SIM0_A00_ADDR, SO->ANA_INT_SIM0_A00.all);
    nnc6521_write_reg(chip_id, ANA_INT_SIM0_A01_ADDR, SO->ANA_INT_SIM0_A01.all);
    nnc6521_write_reg(chip_id, ANA_INT_SIM1_A10_ADDR, SO->ANA_INT_SIM1_A10.all);
    nnc6521_write_reg(chip_id, ANA_INT_SIM1_A11_ADDR, SO->ANA_INT_SIM1_A11.all);

    nnc6521_write_reg(chip_id, ANA_INT_CH1_INT_NUMBER_ADDR, SO->ANA_INT_CH1_INT_NUMBER.all);

    nnc6521_write_reg(chip_id, ANA_INT_SIM2_A20_ADDR, SO->ANA_INT_SIM2_A20.all);
    nnc6521_write_reg(chip_id, ANA_INT_SIM2_A21_ADDR, SO->ANA_INT_SIM2_A21.all);
    nnc6521_write_reg(chip_id, ANA_INT_SIM3_A30_ADDR, SO->ANA_INT_SIM3_A30.all);
    nnc6521_write_reg(chip_id, ANA_INT_SIM3_A31_ADDR, SO->ANA_INT_SIM3_A31.all);

    nnc6521_write_reg(chip_id, ANA_INT_CH2_INT_NUMBER_ADDR, SO->ANA_INT_CH2_INT_NUMBER.all);
    nnc6521_write_reg(chip_id, ANA_INTR_SIM_CL_ADDR, SO->ANA_INTR_SIM_CL.all);
}

void nnc6521_scd_init(uint8_t chip_id,
                      uint8_t u8_CH0_DAC_Threshold,
                      uint8_t u8_CH0_Addr_0,
                      uint8_t u8_CH0_Addr_1,
                      uint8_t u8_CH0_Int_Num,
                      uint8_t u8_CH1_DAC_Threshold,
                      uint8_t u8_CH1_Addr_0,
                      uint8_t u8_CH1_Addr_1,
                      uint8_t u8_CH1_Int_Num)
{
    Short_detect_TypeDef so = {0};
    so.lead_off_ctrl.bits.dac_sel = 0x03;  /* LO_ALL */

    /* CH1 short-circuit detection */
    so.ANA_GEN_REG_2.all = u8_CH0_DAC_Threshold;
    so.ANA_GEN_REG_3.bits.D2A_VDAC_AMPCHOP_CH1 = 1;
    so.ANA_GEN_REG_3.bits.COMP_ISEL_CH1 = 0;
    so.ANA_ENABLE_REG_1.bits.VDAC_EN_CH1 = 1;
    so.ANA_ENABLE_REG_1.bits.D2A_STIMU_COMP_EN_CH1 = 1;
    so.ANA_ENABLE_REG_1.bits.D2A_STIMU_COMP_SEL_CH1 = 0;
    so.ANA_INT_SIM0_A00.all = u8_CH0_Addr_0;
    so.ANA_INT_SIM0_A01.all = u8_CH0_Addr_1;
    so.ANA_INT_CH1_INT_NUMBER.all = u8_CH0_Int_Num;
    so.ANA_INT_COMP_POL.bits.ANA_STIMU_CH1_INTR_EN = 0;

    /* CH2 short-circuit detection */
    so.ANA_GEN_REG_4.all = u8_CH1_DAC_Threshold;
    so.ANA_GEN_REG_5.bits.D2A_VDAC_AMPCHOP_CH2 = 1;
    so.ANA_GEN_REG_5.bits.COMP_ISEL_CH2 = 0;
    so.ANA_ENABLE_REG_2.bits.VDAC_EN_CH2 = 1;
    so.ANA_ENABLE_REG_2.bits.D2A_STIMU_COMP_EN_CH2 = 1;
    so.ANA_ENABLE_REG_2.bits.D2A_STIMU_COMP_SEL_CH2 = 0;
    so.ANA_INT_SIM2_A20.all = u8_CH1_Addr_0;
    so.ANA_INT_SIM2_A21.all = u8_CH1_Addr_1;
    so.ANA_INT_CH2_INT_NUMBER.all = u8_CH1_Int_Num;
    so.ANA_INT_COMP_POL.bits.ANA_STIMU_CH2_INTR_EN = 0;

    nnc6521_short_detect_config(chip_id, &so);
    nnc6521_clear_scd_int(chip_id);
}

/* ============================================================================
 *  Interrupt Clear Functions
 * ===========================================================================*/
void nnc6521_clear_scd_int(uint8_t chip_id)
{
    nnc6521_write_reg(chip_id, ANA_INTR_SIM_CL_ADDR, 0x03);
}

void nnc6521_clear_lod_int(uint8_t chip_id)
{
    uint8_t data;
    data = nnc6521_read_reg(chip_id, LEAD_OFF_INT_ADDR);
    nnc6521_write_reg(chip_id, LEAD_OFF_INT_ADDR, data & ~(0x02));
    nnc6521_write_reg(chip_id, LEAD_OFF_INT_ADDR, data | 0x02);
}

/* ============================================================================
 *  Waveform Generator Configuration
 * ===========================================================================*/
void nnc6521_wavegen_config(uint8_t chip_id,
                            waveform_TypeDef *wf,
                            float *normalized_waveform_array,
                            uint32_t max_current)
{
    int addr;
    uint8_t i = 0;
    uint8_t driver_point = 0;

    /* Disable AWG before modifying parameters */
    nnc6521_awg_enable_disable(chip_id, wf->CHANNEL, 0);

    /* Enable analog channel */
    if (wf->CHANNEL == WAVEFORM_GEN_CH0) {
        nnc6521_write_reg(chip_id, ANA_ENABLE_REG_1_ADDR, 0x09);
    } else {
        nnc6521_write_reg(chip_id, ANA_ENABLE_REG_2_ADDR, 0x09);
    }

    /* Point count register */
    addr = WG_REG_ADDR(wf->CHANNEL, WG_DRV_POINT_CONFIG_OFFSET);
    nnc6521_write_wave_reg(chip_id, addr, wf->WG_DRV_POINT_CONFIG.value);

    /* Configuration register (rest, negative, silent, etc.) */
    addr = WG_REG_ADDR(wf->CHANNEL, WG_DRV_CONFIG_REG0_OFFSET);
    nnc6521_write_wave_reg(chip_id, addr, wf->WG_DRV_CONFIG_REG0.value);

    /* Silent time register (24-bit: 3 bytes) */
    addr = WG_REG_ADDR(wf->CHANNEL, WG_DRV_SILENT_CLK_OFFSET);
    nnc6521_write_wave_reg(chip_id, addr, wf->WG_DRV_SILENT_CLK.value & 0xFF);
    nnc6521_write_wave_reg(chip_id, addr + 0x01, (wf->WG_DRV_SILENT_CLK.value >> 8) & 0xFF);
    nnc6521_write_wave_reg(chip_id, addr + 0x02, (wf->WG_DRV_SILENT_CLK.value >> 16) & 0xFF);

    /* Positive half-wave interval register */
    addr = WG_REG_ADDR(wf->CHANNEL, WG_DRV_HALF_WAVE_CLK_PNT_OFFSET);
    nnc6521_write_wave_reg(chip_id, addr, wf->WG_DRV_HALF_WAVE_CLK_PNT.value & 0xFF);
    nnc6521_write_wave_reg(chip_id, addr + 1, (wf->WG_DRV_HALF_WAVE_CLK_PNT.value >> 8) & 0xFF);

    /* Negative half-wave interval register */
    addr = WG_REG_ADDR(wf->CHANNEL, WG_DRV_NEG_HALF_WAVE_CLK_PNT_OFFSET);
    nnc6521_write_wave_reg(chip_id, addr, wf->WG_DRV_NEG_HALF_WAVE_CLK_PNT.value & 0xFF);
    nnc6521_write_wave_reg(chip_id, addr + 1, (wf->WG_DRV_NEG_HALF_WAVE_CLK_PNT.value >> 8) & 0xFF);

    /* Drive current register 2 */
    addr = WG_REG_ADDR(wf->CHANNEL, WG_DRIVE_REG_CTRL2_OFFSET);
    nnc6521_write_wave_reg(chip_id, addr, wf->WG_DRIVE_REG_CTRL2.value);

    /* Rest time (dead zone) register */
    addr = WG_REG_ADDR(wf->CHANNEL, WG_DRV_REST_CLK_OFFSET);
    nnc6521_write_wave_reg(chip_id, addr, wf->WG_DRV_REST_CLK.value & 0xFF);
    nnc6521_write_wave_reg(chip_id, addr + 1, (wf->WG_DRV_REST_CLK.value >> 8) & 0xFF);

    /* Interrupt count register */
    addr = WG_REG_ADDR(wf->CHANNEL, WG_DRV_INT_NUM_WAVE_OFFSET);
    nnc6521_write_wave_reg(chip_id, addr, wf->WG_DRV_INT_NUM_WAVE.value);

    /* Interrupt control register */
    addr = WG_REG_ADDR(wf->CHANNEL, WG_DRV_INT_REG01_OFFSET);
    nnc6521_write_wave_reg(chip_id, addr, wf->WG_DRV_INT_REG01.value);

    /* Interrupt address registers */
    addr = WG_REG_ADDR(wf->CHANNEL, WG_DRV_INT_REG02_OFFSET);
    nnc6521_write_wave_reg(chip_id, addr, wf->WG_DRV_INT_REG02.value);

    addr = WG_REG_ADDR(wf->CHANNEL, WG_DRV_INT_REG03_OFFSET);
    nnc6521_write_wave_reg(chip_id, addr, wf->WG_DRV_INT_REG03.value);

    if (wf->WG_DRV_CTRL_REG0.bits.waveform_select == WAVEFORM_SPI) {
        /* Read back point count */
        addr = WG_REG_ADDR(wf->CHANNEL, WG_DRV_POINT_CONFIG_OFFSET);
        wf->WG_DRV_POINT_CONFIG.value = nnc6521_read_wave_reg(chip_id, addr);

        /* Calibrate waveform current array */
        uint16_t calibrated_CurrentArray_16bits[wf->WG_DRV_POINT_CONFIG.value];
        uint8_t calibrated_CurrentArray_8bits[wf->WG_DRV_POINT_CONFIG.value];
        uint8_t scale_up = 0;
        uint16_t max_amplitude = 0;

        max_amplitude = Current_Output(chip_id, max_current, wf->CHANNEL);
        generate_scaled_wave(calibrated_CurrentArray_16bits,
                             wf->WG_DRV_POINT_CONFIG.value,
                             normalized_waveform_array, max_amplitude);
        convert_16bit_to_8bit(calibrated_CurrentArray_8bits, &scale_up,
                             calibrated_CurrentArray_16bits,
                             wf->WG_DRV_POINT_CONFIG.value);

        /* Update drive current register 2 with scale factor */
        addr = WG_REG_ADDR(wf->CHANNEL, WG_DRIVE_REG_CTRL2_OFFSET);
        wf->WG_DRIVE_REG_CTRL2.bits.out_pos = 0x04 - scale_up;
        nnc6521_write_wave_reg(chip_id, addr, wf->WG_DRIVE_REG_CTRL2.value);

        if (wf->WG_DRV_POINT_CONFIG.value > 0) {
            if (wf->WG_DRV_CTRL_REG0.bits.preload_mode == 0) {
                driver_point = wf->WG_DRV_POINT_CONFIG.value;
            } else {
                driver_point = wf->WG_DRV_POINT_CONFIG.value * 2;
            }

            if (normalized_waveform_array != NULL) {
                for (i = 0; i < driver_point; i++) {
                    nnc6521_write_wave_reg(chip_id,
                        WG_REG_ADDR(wf->CHANNEL, WG_DRV_IN_WAVE_ADDR_OFFSET), i);
                    nnc6521_write_wave_reg(chip_id,
                        WG_REG_ADDR(wf->CHANNEL, WG_DRV_IN_WAVE_OFFSET),
                        *(calibrated_CurrentArray_8bits + i));
                }
            }
        }
    } else {
        /* Preloaded waveform: just update drive current */
        addr = WG_REG_ADDR(wf->CHANNEL, WG_DRIVE_REG_CTRL2_OFFSET);
        nnc6521_write_wave_reg(chip_id, addr, wf->WG_DRIVE_REG_CTRL2.value);
    }

    /* Alternating mode period register */
    addr = WG_REG_ADDR(wf->CHANNEL, WG_DRV_ALT_LIM_OFFSET);
    nnc6521_write_wave_reg(chip_id, addr, wf->WG_DRV_ALT_LIM.value & 0xFF);
    nnc6521_write_wave_reg(chip_id, addr + 1, (wf->WG_DRV_ALT_LIM.value >> 8) & 0xFF);

    /* Alternating mode silent interval register */
    addr = WG_REG_ADDR(wf->CHANNEL, WG_DRV_ALT_SILENT_LIM_OFFSET);
    nnc6521_write_wave_reg(chip_id, addr, wf->WG_DRV_ALT_SILENT_LIM.value & 0xFF);
    nnc6521_write_wave_reg(chip_id, addr + 1,
        (wf->WG_DRV_ALT_SILENT_LIM.value >> 8) & 0xFF);

    /* Finally enable waveform generator */
    addr = WG_REG_ADDR(wf->CHANNEL, WG_DRV_CTRL_REG0_OFFSET);
    nnc6521_write_wave_reg(chip_id, addr, wf->WG_DRV_CTRL_REG0.value);
}

/* ============================================================================
 *  Waveform Amplitude Update (for interrupt context)
 * ===========================================================================*/
void nnc6521_customized_amplitude(uint8_t chip_id,
                                  uint8_t Channel,
                                  uint8_t u8_PointNum,
                                  float *normalized_waveform_array,
                                  uint32_t max_current)
{
    int addr;
    uint8_t i = 0;

    uint16_t calibrated_CurrentArray_16bits[u8_PointNum];
    uint8_t calibrated_CurrentArray_8bits[u8_PointNum];
    uint8_t scale_up = 0;
    uint16_t max_amplitude = 0;

    max_amplitude = Current_Output(chip_id, max_current, Channel);
    generate_scaled_wave(calibrated_CurrentArray_16bits, u8_PointNum,
                         normalized_waveform_array, max_amplitude);
    convert_16bit_to_8bit(calibrated_CurrentArray_8bits, &scale_up,
                         calibrated_CurrentArray_16bits, u8_PointNum);

    addr = WG_REG_ADDR(Channel, WG_DRIVE_REG_CTRL2_OFFSET);
    nnc6521_write_wave_reg(chip_id, addr, 0x04 - scale_up);

    if (normalized_waveform_array != NULL) {
        for (i = 0; i < u8_PointNum; i++) {
            nnc6521_write_wave_reg(chip_id,
                WG_REG_ADDR(Channel, WG_DRV_IN_WAVE_ADDR_OFFSET), i);
            nnc6521_write_wave_reg(chip_id,
                WG_REG_ADDR(Channel, WG_DRV_IN_WAVE_OFFSET),
                *(calibrated_CurrentArray_8bits + i));
        }
    }
}

__attribute__((section(".nnc6521_amfunc")))
void nnc6521_customized_amplitude_addr(uint8_t chip_id,
                                       uint8_t Channel,
                                       uint8_t u8_Start_Address,
                                       uint8_t u8_End_Address,
                                       float *normalized_waveform_array,
                                       uint32_t max_current)
{
    int addr;
    uint8_t i = 0, j = 0;
    uint8_t u8_PointNum = u8_End_Address - u8_Start_Address;

    uint16_t calibrated_CurrentArray_16bits[u8_PointNum];
    uint8_t calibrated_CurrentArray_8bits[u8_PointNum];
    uint8_t scale_up = 0;
    uint16_t max_amplitude = 0;

    max_amplitude = Current_Output(chip_id, max_current, Channel);
    generate_scaled_wave(calibrated_CurrentArray_16bits, u8_PointNum,
                         normalized_waveform_array, max_amplitude);
    convert_16bit_to_8bit(calibrated_CurrentArray_8bits, &scale_up,
                         calibrated_CurrentArray_16bits, u8_PointNum);

    addr = WG_REG_ADDR(Channel, WG_DRIVE_REG_CTRL2_OFFSET);
    nnc6521_write_wave_reg(chip_id, addr, 0x04 - scale_up);

    if (normalized_waveform_array != NULL) {
        for (i = u8_Start_Address, j = 0; i < u8_End_Address; i++, j++) {
            nnc6521_write_wave_reg(chip_id,
                WG_REG_ADDR(Channel, WG_DRV_IN_WAVE_ADDR_OFFSET), i);
            nnc6521_write_wave_reg(chip_id,
                WG_REG_ADDR(Channel, WG_DRV_IN_WAVE_OFFSET),
                *(calibrated_CurrentArray_8bits + j));
        }
    }
}

/* ============================================================================
 *  Current Calibration Functions (preserved from original ens1p4.c)
 * ===========================================================================*/

static void generate_scaled_wave(uint16_t *output, uint8_t u8_PointNum,
                                 float *base_wave, float amplitude)
{
    for (int i = 0; i < u8_PointNum; i++) {
        float value = base_wave[i] * amplitude;
        if (value > 4095.0f) value = 4095.0f;
        output[i] = (uint16_t)(value + 0.5f);
    }
}

static void convert_16bit_to_8bit(uint8_t *dst, uint8_t *bits_shift,
                                  uint16_t *src, int length)
{
    uint16_t max_amp = 0;
    for (int i = 0; i < length; i++) {
        if (src[i] > max_amp) max_amp = src[i];
    }

    uint8_t used = bits_used(max_amp);
    if (used <= 8) {
        *bits_shift = 0;
    } else {
        *bits_shift = used % 8;
    }

    for (int i = 0; i < length; i++) {
        dst[i] = (uint8_t)((src[i] >> (*bits_shift)) & 0xFF);
    }
}

static uint8_t bits_used(uint16_t value)
{
    uint8_t bits = 0;
    while (value > 0) {
        bits++;
        value >>= 1;
    }
    return bits;
}

static uint8_t CurIsInSide(uint16_t current, uint16_t Imin, uint16_t Imax)
{
    if (current < Imin) return 0;
    if (Imax < current) return 0;
    return 1;
}

static uint16_t GetDif(uint16_t current, uint16_t Imin, uint16_t Imax)
{
    if (current < Imin) return Imin - current;
    if (Imax < current) return current - Imax;
    return 0;
}

static uint16_t GetDrefIselIseg(uint8_t chip_id, uint8_t Channel,
                                uint16_t current, uint8_t *aSeg)
{
    uint8_t tSeg;
    uint16_t Dref, Dref1 = 0;
    uint32_t Imax, Imin;
    uint32_t tDif = 0xFFFFFFFF, tmp;
    uint8_t dataADC1, dataADC2;

    int start_address_add;

    if (Channel == WAVEFORM_GEN_CH0) {
        start_address_add = CALIB_BASE_ADDRESS_0;
    } else {
        start_address_add = CALIB_BASE_ADDRESS_1;
    }

    for (tSeg = 0; tSeg < TEST_POINT_NUM * 2; tSeg += 2) {
        /* Read calibration data from OTP */
        dataADC1 = nnc6521_spi_otp_read(chip_id, start_address_add + 2 * tSeg);
        dataADC2 = nnc6521_spi_otp_read(chip_id, start_address_add + 2 * tSeg + 1);
        Dref = ((uint16_t)dataADC2 << 8) | (uint16_t)dataADC1;

        Imin = ((tSeg / 2) * 163.84f + 1.0f) * Dref;
        Imin /= 1000;
        Imax = (((tSeg / 2) + 1) * 163.84f) * Dref;
        Imax /= 1000;

        if (CurIsInSide(current, Imin, Imax)) {
            *aSeg = tSeg / 2;
            return Dref;
        } else {
            tmp = GetDif(current, Imin, Imax);
            if (tmp < tDif) {
                tDif = tmp;
                *aSeg = tSeg / 2;
                Dref1 = Dref;
            } else {
                return Dref1;
            }
        }
    }

    return Dref1;
}

static uint32_t Current_Output(uint8_t chip_id, uint32_t current, uint8_t channel)
{
    uint8_t tSeg;
    uint16_t Dref;

    uint32_t Ireq = current * 1;
    float Ival_f;
    uint32_t Ival;

    Dref = GetDrefIselIseg(chip_id, channel, current, &tSeg);
    Ival_f = (float)(((float)Ireq * 1000) / ((float)Dref));
    Ival = (uint32_t)(Ival_f + 0.5f);
    Ival = (4096 < Ival) ? 4096 : Ival;
    return Ival;
}
