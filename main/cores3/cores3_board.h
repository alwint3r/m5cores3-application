#pragma once

#include <ii2c/ii2c.h>
#include <ispi/ispi.h>

typedef struct cores3_board cores3_board_t;
struct cores3_board {
  ii2c_master_bus_handle_t i2c_bus;
  ii2c_device_handle_t i2c_aw9523b;
  ii2c_device_handle_t i2c_axp2101;
  ii2c_device_handle_t i2c_ft6336;
  ispi_master_bus_handle_t display_spi_bus;
  ispi_device_handle_t display_spi_device;
};

int32_t cores3_board_init(cores3_board_t *board);
