#include "cores3_board.h"
#include "cores3_board_constants.h"

static const int32_t CORES3_I2C_PROBE_TIME = 3000;

static int32_t cores3_check_i2c_devices_address(cores3_board_t *board) {
  uint8_t addresses[] = {
      CORES3_AXP2101_I2C_ADDRESS,
      CORES3_AW9523B_I2C_ADDRESS,
      CORES3_FT6336_I2C_ADDRESS,
  };
  int32_t err;
  for (size_t i = 0; i < sizeof(addresses); ++i) {
    err = ii2c_master_probe(board->i2c_bus, addresses[i], CORES3_I2C_PROBE_TIME);
    if (err != II2C_ERR_NONE) {
      return err;
    }
  }

  return II2C_ERR_NONE;
}

static int32_t cores3_board_init_i2c_devices(cores3_board_t *board) {
  uint8_t addresses[] = {
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
  for (size_t i = 0; i < sizeof(addresses); ++i) {
    device_cfg.device_address = addresses[i];
    device_cfg.timeout_ms = CORES3_I2C_PROBE_TIME;
    err = ii2c_new_device(board->i2c_bus, &device_cfg, handles[i]);
    if (err != II2C_ERR_NONE) {
      return err;
    }
  }

  return II2C_ERR_NONE;
}

int32_t cores3_board_init(cores3_board_t *board) {
  if (board == NULL) {
    return II2C_ERR_INVALID_ARG;
  }

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
    ii2c_del_master_bus(board->i2c_bus);
    return err;
  }

  err = cores3_board_init_i2c_devices(board);
  if (err != 0) {
    ii2c_del_master_bus(board->i2c_bus);
    return err;
  }

  return 0;
}
