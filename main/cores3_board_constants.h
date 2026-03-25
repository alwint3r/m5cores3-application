#pragma once

#include <stddef.h>
#include <stdint.h>

static const int32_t CORES3_BOARD_SYS_I2C_SDA = 12;
static const int32_t CORES3_BOARD_SYS_I2C_SCL = 11;
static const int32_t CORES3_BOARD_LCD_SPI_MOSI = 37;
static const int32_t CORES3_BOARD_LCD_SPI_SCK = 36;
static const int32_t CORES3_BOARD_LCD_SPI_CS = 3;
static const int32_t CORES3_BOARD_LCD_SPI_DC = 35;

enum {
  CORES3_BOARD_LCD_SPI_MAX_TRANSFER_BYTES = 320 * 2,
};

static const uint32_t CORES3_BOARD_LCD_SPI_CLOCK_HZ = 40000000;
static const uint8_t CORES3_BOARD_LCD_SPI_MODE = 0;

static const uint16_t CORES3_AW9523B_I2C_ADDRESS = 0x58;
static const uint16_t CORES3_AXP2101_I2C_ADDRESS = 0x34;
static const uint16_t CORES3_FT6336_I2C_ADDRESS = 0x38;
