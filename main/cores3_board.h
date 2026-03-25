#pragma once

#include <ii2c/ii2c.h>

typedef struct cores3_board cores3_board_t;
struct cores3_board {
  ii2c_master_bus_handle_t i2c_bus;
  ii2c_device_handle_t i2c_aw9523b;
  ii2c_device_handle_t i2c_axp2101;
  ii2c_device_handle_t i2c_ft6336;
};

int32_t cores3_board_init(cores3_board_t *board);
