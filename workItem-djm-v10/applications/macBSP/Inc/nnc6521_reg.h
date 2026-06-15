/**
  ******************************************************************************
  * @file    nnc6521_reg.h
  * @brief   NNC6521 register definitions extracted from ens1p4.h
  *          Contains all register addresses, union types, and struct definitions
  *          for AWG, LOD and SCD modules.
  ******************************************************************************
  */

#ifndef __NNC6521_REG_H__
#define __NNC6521_REG_H__

#include <stdint.h>

/* Calibration macros --------------------------------------------------------*/
#define ARRAY_SIZE          16      /* Custom waveform array point count */

#define TEST_POINT_NUM      25     /* Calibration point count, range: 3~25 */
#define CALIB_BASE_ADDRESS_0 0x00  /* CH1 calibration data base address */
#define CALIB_BASE_ADDRESS_1 0x32  /* CH2 calibration data base address */

/* NNC6521 Common Register List ----------------------------------------------*/
#define PMU_REG_ADDR                0x01
#define CLK_CTRL_REG_ADDR           0x02
#define WAVEGEN_GLOBAL_REG_0        0x03
#define GPIO_COMP_OUT_CTRL_ADDR     0x23
#define LEAD_OFF_CTRL_ADDR          0x30
#define LEAD_OFF_THR_H_0_ADDR       0x31
#define LEAD_OFF_THR_H_1_ADDR       0x32
#define LEAD_OFF_THR_L_0_ADDR       0x33
#define LEAD_OFF_THR_L_1_ADDR       0x34
#define LEAD_OFF_DLY_TGT_0_ADDR     0x35
#define LEAD_OFF_DLY_TGT_1_ADDR     0x36
#define LEAD_OFF_DLY_TGT_2_ADDR     0x37
#define LEAD_OFF_DLY_TGT_3_ADDR     0x38
#define LEAD_OFF_TGT_ADDR           0x39
#define LEAD_OFF_INT_ADDR           0x3A
#define LEAD_OFF_ANA_ADDR           0x3B
#define ANA_ENABLE_REG_0_ADDR       0x40
#define ANA_ENABLE_REG_1_ADDR       0x41
#define ANA_ENABLE_REG_2_ADDR       0x42
#define ANA_ENABLE_REG_3_ADDR       0x43
#define ANA_GEN_REG_1_ADDR          0x44
#define ANA_GEN_REG_2_ADDR          0x45
#define ANA_GEN_REG_3_ADDR          0x46
#define ANA_GEN_REG_4_ADDR          0x47
#define ANA_GEN_REG_5_ADDR          0x48
#define ANA_INTR_EN_ADDR            0x52
#define ANA_INTR_STS_REG_ADDR       0x53
#define ANA_INT_COMP_POL_ADDR       0x54
#define ANA_INT_STOP_WAVEGEN_ADDR   0x55
#define ANA_INT_SIM0_A00_ADDR       0x56
#define ANA_INT_SIM0_A01_ADDR       0x57
#define ANA_INT_SIM1_A10_ADDR       0x58
#define ANA_INT_SIM1_A11_ADDR       0x59
#define ANA_INT_CH1_INT_NUMBER_ADDR 0x5A
#define ANA_INT_SIM2_A20_ADDR       0x5B
#define ANA_INT_SIM2_A21_ADDR       0x5C
#define ANA_INT_SIM3_A30_ADDR       0x5D
#define ANA_INT_SIM3_A31_ADDR       0x5E
#define ANA_INT_CH2_INT_NUMBER_ADDR 0x5F
#define ANA_INTR_SIM_CL_ADDR        0x60

/* NNC6521 Waveform Generator Register List ----------------------------------*/
#define WAVEFORM_GEN_CH0                    0x00
#define WAVEFORM_GEN_CH1                    0x01

#define WAVEFORM_GEN_BASE                   0x00
#define WAVEFORM_GEN_CH_OFFSET              0x40

#define WG_DRV_CONFIG_REG0_OFFSET           0x00
#define WG_DRV_CTRL_REG0_OFFSET             0x01
#define WG_DRV_POINT_CONFIG_OFFSET          0x02
#define WG_DRV_IN_WAVE_ADDR_OFFSET          0x03
#define WG_DRV_IN_WAVE_OFFSET               0x04
#define WG_DRV_REST_CLK_OFFSET              0x05
#define WG_DRV_SILENT_CLK_OFFSET            0x07
#define WG_DRV_HALF_WAVE_CLK_PNT_OFFSET     0x0A
#define WG_DRV_NEG_HALF_WAVE_CLK_PNT_OFFSET 0x0C
#define WG_DRV_DELAY_LIM_OFFSET             0x20
#define WG_DRV_NEG_SCALE_OFFSET             0x22
#define WG_DRV_NEG_OFFSET_OFFSET            0x23
#define WG_DRV_POS_SCALE_OFFSET             0x24
#define WG_DRV_POS_OFFSET_OFFSET            0x25
#define WG_DRV_SHORT_OFFSET                 0x26
#define WG_DRV_INT_NUM_WAVE_OFFSET          0x27
#define WG_DRV_INT_REG01_OFFSET             0x28
#define WG_DRV_INT_REG02_OFFSET             0x29
#define WG_DRV_INT_REG03_OFFSET             0x2A
#define WG_DRV_ALT_LIM_OFFSET               0x2B
#define WG_DRV_ALT_SILENT_LIM_OFFSET        0x2D
#define WG_DRIVE_REG_CTRL2_OFFSET           0x31

#define WG_REG_ADDR(ch, reg_offset)  (WAVEFORM_GEN_BASE + ((ch) * WAVEFORM_GEN_CH_OFFSET) + (reg_offset))

/* Enumerations --------------------------------------------------------------*/
typedef enum { PMU_NORMAL = 0, PMU_RESET = 1 } pmu_reset_t;
typedef enum { PMU_ENABLE = 0, PMU_DISABLE = 1 } pmu_en_dis_t;

typedef enum
{
    PCLK_DIV_1 = 0,
    PCLK_DIV_2,
    PCLK_DIV_4,
    PCLK_DIV_8,
    PCLK_DIV_16,
    PCLK_DIV_32,
    PCLK_DIV_64,
    PCLK_DIV_128
} pclk_div_t;

typedef enum {
    STIMU_COMP_SEL_CH1_STIMU0 = 0,
    STIMU_COMP_SEL_CH1_STIMU1 = 1,
} D2A_STIMU_COMP_SEL_CH1_e;

typedef enum {
    STIMU_COMP_SEL_CH2_STIMU2 = 0,
    STIMU_COMP_SEL_CH2_STIMU3 = 1,
} D2A_STIMU_COMP_SEL_CH2_e;

typedef enum {
    LVD_SEL_4_2V = 0x00,
    LVD_SEL_3_9V = 0x01,
    LVD_SEL_3_6V = 0x02,
    LVD_SEL_3_3V = 0x03,
    LVD_SEL_3_0V = 0x04,
} LVD_SEL_e;

typedef enum {
    LO_CH0 = 0x01,
    LO_CH1 = 0x02,
    LO_ALL = 0x03,
    LO_DISABLE = 0,
} dac_sel_e;

/* PMU Control Register ------------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t pmuenable       : 1;
        uint8_t sleepdeep       : 1;
        uint8_t hresetreq       : 1;
        uint8_t otp_dpstb_en    : 1;
        uint8_t wave_gen_disable: 1;
        uint8_t wave_gen_rst    : 1;
        uint8_t lead_off_dis    : 1;
        uint8_t lead_off_rst    : 1;
    } bits;
} PMU_REG_t;

typedef union {
    uint8_t all;
    struct {
        uint8_t pclk_div     : 3;
        uint8_t int_clk_out  : 1;
        uint8_t reserved_4_7 : 4;
    } bits;
} CLK_CTRL_REG_t;

typedef union {
    uint8_t all;
    struct {
        uint8_t global_drive_en : 1;
        uint8_t reserved_1_7    : 7;
    } bits;
} WAVEGEN_GLOBAL_REG_0_t;

/* LOD Register Types --------------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t dly_dis          : 1;
        uint8_t compare_reverse  : 1;
        uint8_t check_mode       : 2;
        uint8_t dac_sel          : 2;
        uint8_t reserved_6_7     : 2;
    } bits;
} LEAD_OFF_CTRL_t;

typedef union {
    uint8_t all;
    struct {
        uint8_t lead_off_th_h : 8;
    } bits;
} LEAD_OFF_THR_H_0_t;

typedef union {
    uint8_t all;
    struct {
        uint8_t lead_off_th_h : 4;
        uint8_t reserved      : 4;
    } bits;
} LEAD_OFF_THR_H_1_t;

typedef union {
    uint8_t all;
    struct {
        uint8_t lead_off_th_l : 8;
    } bits;
} LEAD_OFF_THR_L_0_t, LEAD_OFF_THR_L_1_t;

typedef union {
    uint8_t all;
    struct {
        uint8_t lead_off_dly_tgt : 8;
    } bits;
} LEAD_OFF_DLY_TGT_t;

typedef union {
    uint8_t all;
    struct {
        uint8_t lead_off_tgt : 8;
    } bits;
} LEAD_OFF_TGT_t;

typedef union {
    uint8_t all;
    struct {
        uint8_t lead_off_result       : 1;
        uint8_t lead_off_status_clear : 1;
        uint8_t reserved              : 2;
        uint8_t lead_off_int_en       : 1;
        uint8_t reserved_5_7          : 3;
    } bits;
} LEAD_OFF_INT_t;

typedef union {
    uint8_t all;
    struct {
        uint8_t A2D_COMP0 : 1;
        uint8_t A2D_COMP1 : 1;
        uint8_t reserved_2_7  : 6;
    } bits;
} LEAD_OFF_ANA_t;

/* ANA_ENABLE_REG_0 (0x40) ---------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t LVD_EN            : 1;
        uint8_t OSC2MHZ_EN        : 1;
        uint8_t reserved_2_7      : 6;
    } bits;
} ANA_ENABLE_REG_0_t;

/* ANA_ENABLE_REG_1 (0x41) ---------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t DRIVERA_AMP_EN_CH1       : 1;
        uint8_t DRIVERA_CSAMP_EN_CH1     : 1;
        uint8_t COMP_EN_CH1              : 1;
        uint8_t IDAC_EN_CH1              : 1;
        uint8_t VDAC_EN_CH1              : 1;
        uint8_t D2A_STIMU_COMP_EN_CH1    : 1;
        uint8_t D2A_STIMU_COMP_SEL_CH1   : 1;
        uint8_t reserved_7               : 1;
    } bits;
} ANA_ENABLE_REG_1_t;

/* ANA_ENABLE_REG_2 (0x42) ---------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t DRIVERA_AMP_EN_CH2       : 1;
        uint8_t DRIVERA_CSAMP_EN_CH2     : 1;
        uint8_t COMP_EN_CH2              : 1;
        uint8_t IDAC_EN_CH2              : 1;
        uint8_t VDAC_EN_CH2              : 1;
        uint8_t D2A_STIMU_COMP_EN_CH2    : 1;
        uint8_t D2A_STIMU_COMP_SEL_CH2   : 1;
        uint8_t reserved_7               : 1;
    } bits;
} ANA_ENABLE_REG_2_t;

/* ANA_ENABLE_REG_3 (0x43) ---------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t BIST_EN      : 1;
        uint8_t BIST_SEL     : 4;
        uint8_t reserved_5_7 : 3;
    } bits;
} ANA_ENABLE_REG_3_t;

/* ANA_GEN_REG_1 (0x44) ------------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t LVD_SEL      : 3;
        uint8_t reserved_3_7 : 5;
    } bits;
} ANA_GEN_REG_1_t;

/* ANA_GEN_REG_2 (0x45) ------------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t VDAC_DIN_CH1_LSB : 8;
    } bits;
} ANA_GEN_REG_2_t;

/* ANA_GEN_REG_3 (0x46) ------------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t VDAC_DIN_CH1_MSB     : 2;
        uint8_t DRIVERA_CSAMP_CH_CH1 : 1;
        uint8_t COMP_ISEL_CH1        : 1;
        uint8_t D2A_VDAC_AMPCHOP_CH1 : 1;
        uint8_t reserved_5_7         : 3;
    } bits;
} ANA_GEN_REG_3_t;

/* ANA_GEN_REG_4 (0x47) ------------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t VDAC_DIN_CH2_LSB : 8;
    } bits;
} ANA_GEN_REG_4_t;

/* ANA_GEN_REG_5 (0x48) ------------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t VDAC_DIN_CH2_MSB     : 2;
        uint8_t DRIVERA_CSAMP_CH_CH2 : 1;
        uint8_t COMP_ISEL_CH2        : 1;
        uint8_t D2A_VDAC_AMPCHOP_CH2 : 1;
        uint8_t reserved_5_7         : 3;
    } bits;
} ANA_GEN_REG_5_t;

/* ANA_INTR_EN (0x52) --------------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t ANA_LVD_INTR_EN                : 1;
        uint8_t ANA_COMP_CH1_INTR_EN           : 1;
        uint8_t ANA_COMP_CH2_INTR_EN           : 1;
        uint8_t ANA_COMP_CH1_INTR_TRANS_SEL    : 1;
        uint8_t ANA_COMP_CH12_INTR_TRANS_SEL   : 1;
        uint8_t reserved_5_7                   : 3;
    } bits;
} ANA_INTR_EN_t;

/* ANA_INTR_STS_REG (0x53) ---------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t ANA_LVD_INTR_STS        : 1;
        uint8_t ANA_COMP_CH1_INTR_STS   : 1;
        uint8_t ANA_COMP_CH2_INTR_STS   : 1;
        uint8_t reserved_3_7            : 5;
    } bits;
} ANA_INTR_STS_REG_t;

/* ANA_INT_COMP_POL (0x54) ---------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t ana_int_comp_pol_reg_0  : 1;
        uint8_t ANA_STIMU_CH1_INTR_EN   : 1;
        uint8_t ANA_STIMU_CH2_INTR_EN   : 1;
        uint8_t reserved_3_7            : 5;
    } bits;
} ANA_INT_COMP_POL_t;

/* ANA_INT_STOP_WAVEGEN (0x55) -----------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t Ana_int_stop_wavegen_0 : 1;
        uint8_t Ana_int_stop_wavegen_1 : 1;
        uint8_t reserved_2_7           : 6;
    } bits;
} ANA_INT_STOP_WAVEGEN_t;

/* ANA_INT_SIM0_A00 (0x56) ---------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t ANA_INT_SIM0_A00 : 8;
    } bits;
} ANA_INT_SIM0_A00_t;

/* ANA_INT_SIM0_A01 (0x57) ---------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t ANA_INT_SIM0_A01 : 8;
    } bits;
} ANA_INT_SIM0_A01_t;

/* ANA_INT_SIM1_A10 (0x58) ---------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t ANA_INT_SIM1_A10 : 8;
    } bits;
} ANA_INT_SIM1_A10_t;

/* ANA_INT_SIM1_A11 (0x59) ---------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t ANA_INT_SIM1_A11 : 8;
    } bits;
} ANA_INT_SIM1_A11_t;

/* ANA_INT_CH1_INT_NUMBER (0x5A) ---------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t ANA_INT_CH1_INT_NUMBER : 8;
    } bits;
} ANA_INT_CH1_INT_NUMBER_t;

/* ANA_INT_SIM2_A20 (0x5B) ---------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t ANA_INT_SIM2_A20 : 8;
    } bits;
} ANA_INT_SIM2_A20_t;

/* ANA_INT_SIM2_A21 (0x5C) ---------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t ANA_INT_SIM2_A21 : 8;
    } bits;
} ANA_INT_SIM2_A21_t;

/* ANA_INT_SIM3_A30 (0x5D) ---------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t ANA_INT_SIM3_A30 : 8;
    } bits;
} ANA_INT_SIM3_A30_t;

/* ANA_INT_SIM3_A31 (0x5E) ---------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t ANA_INT_SIM3_A31 : 8;
    } bits;
} ANA_INT_SIM3_A31_t;

/* ANA_INT_CH2_INT_NUMBER (0x5F) ---------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t ANA_INT_CH2_INT_NUMBER : 8;
    } bits;
} ANA_INT_CH2_INT_NUMBER_t;

/* ANA_INTR_SIM_CL (0x60) ----------------------------------------------------*/
typedef union {
    uint8_t all;
    struct {
        uint8_t ANA_STIMU_CH1_INTR_STS : 1;
        uint8_t ANA_STIMU_CH2_INTR_STS : 1;
        uint8_t reserved_2_7           : 6;
    } bits;
} ANA_INTR_SIM_CL_t;

/* Waveform Generator Register Types -----------------------------------------*/
typedef union {
    uint8_t value;
    struct {
        uint8_t rest_enable        : 1;
        uint8_t negative_enable    : 1;
        uint8_t silent_enable      : 1;
        uint8_t sourceB_enable     : 1;
        uint8_t alternating_pos    : 1;
        uint8_t continue_repeat    : 1;
        uint8_t multi_electrode    : 1;
        uint8_t disable_positive   : 1;
    } bits;
} WG_DrvConfigReg0_t;

typedef enum {
    WAVEFORM_SINE        = 0x00,
    WAVEFORM_PULSE       = 0x01,
    WAVEFORM_TRIANGLE    = 0x02,
    WAVEFORM_SPI         = 0x03
} WaveformSelect_t;

typedef union {
    uint8_t value;
    struct {
        uint8_t enable_wavegen     : 1;
        uint8_t waveform_select    : 2;
        uint8_t num_waveforms      : 3;
        uint8_t preload_mode       : 1;
        uint8_t symmetric_wave     : 1;
    } bits;
} WG_DrvCtrlReg0_t;

typedef union {
    uint8_t value;
} WG_DrvPointConfig_t;

typedef union {
    uint8_t value;
} WG_DrvInWaveAddrReg_t;

typedef union {
    uint8_t value;
} WG_DrvInWaveReg_t;

typedef union {
    uint16_t value;
} WG_DrvRestClkReg_t;

typedef union {
    uint32_t value;
} WG_DrvSilentClkReg_t;

typedef union {
    uint16_t value;
} WG_DrvHalfWaveClkPntReg_t;

typedef union {
    uint16_t value;
} WG_DrvNegHalfWaveClkPntReg_t;

typedef union {
    uint16_t value;
} WG_DrvDelayLimReg_t;

typedef union {
    uint8_t value;
    struct {
        uint8_t scale_value : 7;
        uint8_t scale_dir   : 1;
    } bits;
} WG_DrvNegScaleReg_t;

typedef union {
    uint8_t value;
} WG_DrvNegOffsetReg_t;

typedef union {
    uint8_t value;
    struct {
        uint8_t scale_value : 7;
        uint8_t scale_dir   : 1;
    } bits;
} WG_DrvPosScaleReg_t;

typedef union {
    uint8_t value;
} WG_DrvPosOffsetReg_t;

typedef union {
    uint8_t value;
} WG_DrvIntNumWaveReg_t;

typedef union {
    uint8_t value;
    struct {
        uint8_t driver_num   : 3;
        uint8_t int_en       : 1;
        uint8_t addr0_hit    : 1;
        uint8_t addr1_hit    : 1;
        uint8_t reserved_6_7 : 2;
    } bits;
} WG_DrvIntReg01_t;

typedef union {
    uint8_t value;
} WG_DrvIntReg02_t;

typedef union {
    uint8_t value;
} WG_DrvIntReg03_t;

typedef union {
    uint16_t value;
} WG_DrvAltLimReg_t;

typedef union {
    uint16_t value;
} WG_DrvAltSilentLimReg_t;

typedef union {
    uint8_t value;
    struct {
        uint8_t idac_msb    : 4;
        uint8_t out_pos     : 3;
        uint8_t reserved_7  : 1;
    } bits;
} WG_DriveRegCtrl2_t;

/* Composite structs ---------------------------------------------------------*/
typedef struct
{
    int CHANNEL;
    LEAD_OFF_CTRL_t        lead_off_ctrl;           /* 0x30 */
    LEAD_OFF_THR_H_0_t     lead_off_thr_h_0;        /* 0x31 */
    LEAD_OFF_THR_H_1_t     lead_off_thr_h_1;        /* 0x32 */
    LEAD_OFF_THR_L_0_t     lead_off_thr_l_0;        /* 0x33 */
    LEAD_OFF_THR_L_1_t     lead_off_thr_l_1;        /* 0x34 */
    LEAD_OFF_DLY_TGT_t     lead_off_dly_tgt[4];     /* 0x35 */
    LEAD_OFF_TGT_t         lead_off_tgt;            /* 0x39 */
    LEAD_OFF_INT_t         lead_off_int;            /* 0x3A */
    LEAD_OFF_ANA_t         lead_off_ana;            /* 0x3B */
    ANA_ENABLE_REG_1_t     ANA_ENABLE_REG_1;        /* 0x41 */
    ANA_ENABLE_REG_2_t     ANA_ENABLE_REG_2;        /* 0x42 */
    ANA_GEN_REG_2_t        ANA_GEN_REG_2;           /* 0x45 */
    ANA_GEN_REG_3_t        ANA_GEN_REG_3;           /* 0x46 */
    ANA_GEN_REG_4_t        ANA_GEN_REG_4;           /* 0x47 */
    ANA_GEN_REG_5_t        ANA_GEN_REG_5;           /* 0x48 */
} Leadoff_TypeDef;

typedef struct
{
    int CHANNEL;
    LEAD_OFF_CTRL_t         lead_off_ctrl;           /* 0x30 */
    ANA_ENABLE_REG_0_t      ANA_ENABLE_REG_0;        /* 0x40 */
    ANA_ENABLE_REG_1_t      ANA_ENABLE_REG_1;        /* 0x41 */
    ANA_ENABLE_REG_2_t      ANA_ENABLE_REG_2;        /* 0x42 */
    ANA_ENABLE_REG_3_t      ANA_ENABLE_REG_3;        /* 0x43 */
    ANA_GEN_REG_1_t         ANA_GEN_REG_1;           /* 0x44 */
    ANA_GEN_REG_2_t         ANA_GEN_REG_2;           /* 0x45 */
    ANA_GEN_REG_3_t         ANA_GEN_REG_3;           /* 0x46 */
    ANA_GEN_REG_4_t         ANA_GEN_REG_4;           /* 0x47 */
    ANA_GEN_REG_5_t         ANA_GEN_REG_5;           /* 0x48 */
    ANA_INTR_EN_t           ANA_INTR_EN;             /* 0x52 */
    ANA_INTR_STS_REG_t      ANA_INTR_STS_REG;        /* 0x53 */
    ANA_INT_COMP_POL_t      ANA_INT_COMP_POL;        /* 0x54 */
    ANA_INT_STOP_WAVEGEN_t  ANA_INT_STOP_WAVEGEN;    /* 0x55 */
    ANA_INT_SIM0_A00_t      ANA_INT_SIM0_A00;        /* 0x56 */
    ANA_INT_SIM0_A01_t      ANA_INT_SIM0_A01;        /* 0x57 */
    ANA_INT_SIM1_A10_t      ANA_INT_SIM1_A10;        /* 0x58 */
    ANA_INT_SIM1_A11_t      ANA_INT_SIM1_A11;        /* 0x59 */
    ANA_INT_CH1_INT_NUMBER_t ANA_INT_CH1_INT_NUMBER; /* 0x5A */
    ANA_INT_SIM2_A20_t      ANA_INT_SIM2_A20;        /* 0x5B */
    ANA_INT_SIM2_A21_t      ANA_INT_SIM2_A21;        /* 0x5C */
    ANA_INT_SIM3_A30_t      ANA_INT_SIM3_A30;        /* 0x5D */
    ANA_INT_SIM3_A31_t      ANA_INT_SIM3_A31;        /* 0x5E */
    ANA_INT_CH2_INT_NUMBER_t ANA_INT_CH2_INT_NUMBER; /* 0x5F */
    ANA_INTR_SIM_CL_t       ANA_INTR_SIM_CL;         /* 0x60 */
} Short_detect_TypeDef;

typedef struct {
    int CHANNEL;
    WG_DrvConfigReg0_t              WG_DRV_CONFIG_REG0;           /* 0x00 */
    WG_DrvCtrlReg0_t                WG_DRV_CTRL_REG0;             /* 0x01 */
    WG_DrvPointConfig_t             WG_DRV_POINT_CONFIG;          /* 0x02 */
    WG_DrvInWaveAddrReg_t           WG_DRV_IN_WAVE_ADDR;          /* 0x03 */
    WG_DrvInWaveReg_t               WG_DRV_IN_WAVE;               /* 0x04 */
    WG_DrvRestClkReg_t              WG_DRV_REST_CLK;              /* 0x05~0x06 */
    WG_DrvSilentClkReg_t            WG_DRV_SILENT_CLK;            /* 0x07~0x09 */
    WG_DrvHalfWaveClkPntReg_t       WG_DRV_HALF_WAVE_CLK_PNT;     /* 0x0A~0x0B */
    WG_DrvNegHalfWaveClkPntReg_t    WG_DRV_NEG_HALF_WAVE_CLK_PNT; /* 0x0C~0x0D */
    WG_DrvDelayLimReg_t             WG_DRV_DELAY_LIM;             /* 0x20~0x21 */
    WG_DrvNegScaleReg_t             WG_DRV_NEG_SCALE;             /* 0x22 */
    WG_DrvNegOffsetReg_t            WG_DRV_NEG_OFFSET;            /* 0x23 */
    WG_DrvPosScaleReg_t             WG_DRV_POS_SCALE;             /* 0x24 */
    WG_DrvPosOffsetReg_t            WG_DRV_POS_OFFSET;            /* 0x25 */
    WG_DrvIntNumWaveReg_t           WG_DRV_INT_NUM_WAVE;          /* 0x27 */
    WG_DrvIntReg01_t                WG_DRV_INT_REG01;             /* 0x28 */
    WG_DrvIntReg02_t                WG_DRV_INT_REG02;             /* 0x29 */
    WG_DrvIntReg03_t                WG_DRV_INT_REG03;             /* 0x2A */
    WG_DrvAltLimReg_t               WG_DRV_ALT_LIM;               /* 0x2B~0x2C */
    WG_DrvAltSilentLimReg_t         WG_DRV_ALT_SILENT_LIM;        /* 0x2D~0x2E */
    WG_DriveRegCtrl2_t              WG_DRIVE_REG_CTRL2;           /* 0x31 */
} waveform_TypeDef;

#endif /* __NNC6521_REG_H__ */
