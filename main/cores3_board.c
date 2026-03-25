#include "cores3_board.h"
#include "cores3_board_constants.h"

#include <string.h>

static const int32_t CORES3_I2C_PROBE_TIME = 3000;

static int32_t cores3_check_i2c_devices_address(cores3_board_t *board) {
  uint16_t addresses[] = {
      CORES3_AXP2101_I2C_ADDRESS,
      CORES3_AW9523B_I2C_ADDRESS,
      CORES3_FT6336_I2C_ADDRESS,
  };
  int32_t err;
  size_t num_addresses = sizeof(addresses) / sizeof(addresses[0]);
  for (size_t i = 0; i < num_addresses; ++i) {
    err = ii2c_master_probe(board->i2c_bus, addresses[i], CORES3_I2C_PROBE_TIME);
    if (err != II2C_ERR_NONE) {
      return err;
    }
  }

  return II2C_ERR_NONE;
}

static int32_t cores3_board_init_i2c_devices(cores3_board_t *board) {
  uint16_t addresses[] = {
      CORES3_AXP2101_I2C_ADDRESS,
      CORES3_AW9523B_I2C_ADDRESS,
      CORES3_FT6336_I2C_ADDRESS,
  };
  ii2c_device_handle_t *handles[] = {
      &board->i2c_axp2101,
      &board->i2c_aw9523b,
      &board->i2c_ft6336,
  };
  int32_t err;
  ii2c_device_config_t device_cfg;
  ii2c_get_default_device_config(&device_cfg);
  size_t num_addresses = sizeof(addresses) / sizeof(addresses[0]);
  for (size_t i = 0; i < num_addresses; ++i) {
    device_cfg.device_address = addresses[i];
    device_cfg.timeout_ms = CORES3_I2C_PROBE_TIME;
    err = ii2c_new_device(board->i2c_bus, &device_cfg, handles[i]);
    if (err != II2C_ERR_NONE) {
      return err;
    }
  }

  return II2C_ERR_NONE;
}

static int32_t cores3_board_init_display_spi(cores3_board_t *board) {
  ispi_master_bus_config_t spi_bus_cfg = {0};
  ispi_get_default_master_bus_config(&spi_bus_cfg);
  spi_bus_cfg.host = ISPI_HOST_SPI2;
  spi_bus_cfg.mosi_io_num = CORES3_BOARD_LCD_SPI_MOSI;
  spi_bus_cfg.miso_io_num = -1;
  spi_bus_cfg.sclk_io_num = CORES3_BOARD_LCD_SPI_SCK;
  spi_bus_cfg.max_transfer_sz = CORES3_BOARD_LCD_SPI_MAX_TRANSFER_BYTES;

  int32_t err = ispi_new_master_bus(&spi_bus_cfg, &board->display_spi_bus);
  if (err != ISPI_ERR_NONE) {
    return err;
  }

  ispi_device_config_t lcd_dev_cfg = {0};
  ispi_get_default_device_config(&lcd_dev_cfg);
  lcd_dev_cfg.cs_io_num = CORES3_BOARD_LCD_SPI_CS;
  lcd_dev_cfg.clock_speed_hz = CORES3_BOARD_LCD_SPI_CLOCK_HZ;
  lcd_dev_cfg.mode = CORES3_BOARD_LCD_SPI_MODE;

  err = ispi_new_device(board->display_spi_bus, &lcd_dev_cfg, &board->display_spi_device);
  if (err != ISPI_ERR_NONE) {
    (void)ispi_del_master_bus(board->display_spi_bus);
    board->display_spi_bus = NULL;
    return err;
  }

  return ISPI_ERR_NONE;
}

static void cores3_board_cleanup(cores3_board_t *board) {
  if (board == NULL) {
    return;
  }

  if (board->display_spi_device != NULL) {
    (void)ispi_del_device(board->display_spi_device);
    board->display_spi_device = NULL;
  }

  if (board->display_spi_bus != NULL) {
    (void)ispi_del_master_bus(board->display_spi_bus);
    board->display_spi_bus = NULL;
  }

  if (board->i2c_ft6336 != NULL) {
    (void)ii2c_del_device(board->i2c_ft6336);
    board->i2c_ft6336 = NULL;
  }

  if (board->i2c_aw9523b != NULL) {
    (void)ii2c_del_device(board->i2c_aw9523b);
    board->i2c_aw9523b = NULL;
  }

  if (board->i2c_axp2101 != NULL) {
    (void)ii2c_del_device(board->i2c_axp2101);
    board->i2c_axp2101 = NULL;
  }

  if (board->i2c_bus != NULL) {
    (void)ii2c_del_master_bus(board->i2c_bus);
    board->i2c_bus = NULL;
  }
}

int32_t cores3_board_init(cores3_board_t *board) {
  if (board == NULL) {
    return II2C_ERR_INVALID_ARG;
  }

  memset(board, 0, sizeof(*board));

  ii2c_master_bus_config_t bus_cfg;
  ii2c_get_default_master_bus_config(&bus_cfg);

  bus_cfg.sda_io_num = CORES3_BOARD_SYS_I2C_SDA;
  bus_cfg.scl_io_num = CORES3_BOARD_SYS_I2C_SCL;
  bus_cfg.enable_internal_pullup = true;

  int32_t err = ii2c_new_master_bus(&bus_cfg, &board->i2c_bus);
  if (err != 0) {
    return err;
  }

  err = cores3_check_i2c_devices_address(board);
  if (err != 0) {
    cores3_board_cleanup(board);
    return err;
  }

  err = cores3_board_init_i2c_devices(board);
  if (err != 0) {
    cores3_board_cleanup(board);
    return err;
  }

  err = cores3_board_init_display_spi(board);
  if (err != 0) {
    cores3_board_cleanup(board);
    return err;
  }

  return 0;
}
