/**
  ******************************************************************************
  * @file    nnc6521_spi.c
  * @brief   NNC6521 software SPI (GPIO bit-bang) implementation
  *          CPOL=0, CPHA=0, MSB first. Dual-chip support with independent pins.
  *          All comments in English.
  ******************************************************************************
  */

#include "nnc6521.h"

/* ============================================================================
 *  Pin mapping table for both chips
 *  Chip 1: MOSI=PC9, CSN=PC8, SCLK=PC7, MISO=PA8, CHIP_EN=PA12, INTB=PA11
 *  Chip 2: MOSI=PB13, CSN=PB12, SCLK=PB11, MISO=PB14, CHIP_EN=PC6, INTB=PB15
 * ===========================================================================*/
static const nnc6521_pin_map_t nnc6521_pins[NNC6521_NUM_CHIPS] = {
    /* Chip 1 */
    {
        .mosi_port   = GPIOC, .mosi_pin   = GPIO_PIN_9,
        .csn_port    = GPIOC, .csn_pin    = GPIO_PIN_8,
        .sclk_port   = GPIOC, .sclk_pin   = GPIO_PIN_7,
        .miso_port   = GPIOA, .miso_pin   = GPIO_PIN_8,
        .chip_en_port= GPIOA, .chip_en_pin= GPIO_PIN_12,
        .intb_port   = GPIOA, .intb_pin   = GPIO_PIN_11,
    },
    /* Chip 2 */
    {
        .mosi_port   = GPIOB, .mosi_pin   = GPIO_PIN_13,
        .csn_port    = GPIOB, .csn_pin    = GPIO_PIN_12,
        .sclk_port   = GPIOB, .sclk_pin   = GPIO_PIN_11,
        .miso_port   = GPIOB, .miso_pin   = GPIO_PIN_14,
        .chip_en_port= GPIOC, .chip_en_pin= GPIO_PIN_6,
        .intb_port   = GPIOB, .intb_pin   = GPIO_PIN_15,
    },
};

/* ============================================================================
 *  GPIO Initialization
 * ===========================================================================*/

static void nnc6521_enable_gpio_clock(GPIO_TypeDef *port)
{
    if (port == GPIOA)      __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
}

void nnc6521_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    for (uint8_t i = 0; i < NNC6521_NUM_CHIPS; i++)
    {
        const nnc6521_pin_map_t *p = &nnc6521_pins[i];

        /* Enable clocks for all ports used by this chip */
        nnc6521_enable_gpio_clock(p->mosi_port);
        nnc6521_enable_gpio_clock(p->csn_port);
        nnc6521_enable_gpio_clock(p->sclk_port);
        nnc6521_enable_gpio_clock(p->miso_port);
        nnc6521_enable_gpio_clock(p->chip_en_port);
        nnc6521_enable_gpio_clock(p->intb_port);

        /* MOSI: push-pull output, high speed */
        gpio.Pin   = p->mosi_pin;
        gpio.Mode  = GPIO_MODE_OUTPUT_PP;
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(p->mosi_port, &gpio);

        /* CSN: push-pull output (idle high) */
        gpio.Pin = p->csn_pin;
        HAL_GPIO_Init(p->csn_port, &gpio);
        HAL_GPIO_WritePin(p->csn_port, p->csn_pin, GPIO_PIN_SET);

        /* SCLK: push-pull output (idle low, CPOL=0) */
        gpio.Pin = p->sclk_pin;
        HAL_GPIO_Init(p->sclk_port, &gpio);
        HAL_GPIO_WritePin(p->sclk_port, p->sclk_pin, GPIO_PIN_RESET);

        /* CHIP_EN: push-pull output (idle low) */
        gpio.Pin = p->chip_en_pin;
        HAL_GPIO_Init(p->chip_en_port, &gpio);
        HAL_GPIO_WritePin(p->chip_en_port, p->chip_en_pin, GPIO_PIN_RESET);

        /* MISO: floating input */
        gpio.Pin  = p->miso_pin;
        gpio.Mode = GPIO_MODE_INPUT;
        gpio.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(p->miso_port, &gpio);

        /* INTB: floating input */
        gpio.Pin = p->intb_pin;
        HAL_GPIO_Init(p->intb_port, &gpio);
    }
}

/* ============================================================================
 *  Software SPI core routines (CPOL=0, CPHA=0, MSB first)
 * ===========================================================================*/

/**
 * @brief  Software SPI byte transfer (full-duplex).
 *         SCLK idle low, data sampled on rising edge, shifted on falling edge.
 */
static uint8_t spi_sw_transfer_byte(uint8_t chip_id, uint8_t tx_byte)
{
    const nnc6521_pin_map_t *p = &nnc6521_pins[chip_id];
    uint8_t rx_byte = 0;

    for (int8_t bit = 7; bit >= 0; bit--)
    {
        /* MOSI: set data on falling edge */
        if (tx_byte & (1 << bit))
            HAL_GPIO_WritePin(p->mosi_port, p->mosi_pin, GPIO_PIN_SET);
        else
            HAL_GPIO_WritePin(p->mosi_port, p->mosi_pin, GPIO_PIN_RESET);

        /* SCLK rising edge -> slave latches MOSI, master latches MISO */
        HAL_GPIO_WritePin(p->sclk_port, p->sclk_pin, GPIO_PIN_SET);

        /* Read MISO */
        if (HAL_GPIO_ReadPin(p->miso_port, p->miso_pin) == GPIO_PIN_SET)
            rx_byte |= (1 << bit);

        /* SCLK falling edge */
        HAL_GPIO_WritePin(p->sclk_port, p->sclk_pin, GPIO_PIN_RESET);
    }

    return rx_byte;
}

/* ============================================================================
 *  NNC6521 SPI Protocol
 * ===========================================================================*/

/**
 * @brief  Write one byte to NNC6521 register.
 *         Write protocol: 4 bytes [addr, cmd, 0x11, data]
 *         Normal register cmd = 0x80, waveform register cmd = 0xC0
 */
void nnc6521_spi_write(uint8_t chip_id, uint8_t addr, uint8_t data, uint8_t is_wave)
{
    const nnc6521_pin_map_t *p = &nnc6521_pins[chip_id];
    uint8_t cmd = is_wave ? 0xC0 : 0x80;

    /* CSN low */
    HAL_GPIO_WritePin(p->csn_port, p->csn_pin, GPIO_PIN_RESET);

    /* Byte 0: address */
    spi_sw_transfer_byte(chip_id, addr);
    /* Byte 1: command */
    spi_sw_transfer_byte(chip_id, cmd);
    /* Byte 2: data marker */
    spi_sw_transfer_byte(chip_id, 0x11);
    /* Byte 3: data */
    spi_sw_transfer_byte(chip_id, data);

    /* CSN high */
    HAL_GPIO_WritePin(p->csn_port, p->csn_pin, GPIO_PIN_SET);
}

/**
 * @brief  Read one byte from NNC6521 register.
 *         Read protocol: 3 bytes [addr, cmd, dummy] -> data in 3rd byte
 *         Normal register cmd = 0x00, waveform register cmd = 0x40
 */
uint8_t nnc6521_spi_read(uint8_t chip_id, uint8_t addr, uint8_t is_wave)
{
    const nnc6521_pin_map_t *p = &nnc6521_pins[chip_id];
    uint8_t cmd = is_wave ? 0x40 : 0x00;
    uint8_t rx[3];

    /* CSN low */
    HAL_GPIO_WritePin(p->csn_port, p->csn_pin, GPIO_PIN_RESET);

    /* Byte 0: address */
    rx[0] = spi_sw_transfer_byte(chip_id, addr);
    /* Byte 1: command */
    rx[1] = spi_sw_transfer_byte(chip_id, cmd);
    /* Byte 2: dummy read, actual data comes back */
    rx[2] = spi_sw_transfer_byte(chip_id, 0x00);

    /* CSN high */
    HAL_GPIO_WritePin(p->csn_port, p->csn_pin, GPIO_PIN_SET);

    return rx[2];
}

/**
 * @brief  Read one byte from OTP memory.
 *         Sequence: write 0x1D with addr, write 0x1B with 0x54 (read cmd),
 *         read 0x1E, write 0x1B with 0x50 (end cmd).
 */
uint8_t nnc6521_spi_otp_read(uint8_t chip_id, uint8_t addr)
{
    uint8_t output;

    nnc6521_write_reg(chip_id, 0x1D, addr);     /* Set OTP address */
    nnc6521_write_reg(chip_id, 0x1B, 0x54);     /* OTP read command */
    output = nnc6521_read_reg(chip_id, 0x1E);   /* Read OTP data */
    nnc6521_write_reg(chip_id, 0x1B, 0x50);     /* End OTP read */

    return output;
}
