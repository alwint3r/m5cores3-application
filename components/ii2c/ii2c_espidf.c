#include "ii2c/ii2c.h"

#include <driver/i2c_master.h>
#include <esp_err.h>
#include <soc/soc_caps.h>
#include <stdlib.h>

struct ii2c_master_bus_t {
  i2c_master_bus_handle_t esp_bus;
};

struct ii2c_device_t {
  i2c_master_dev_handle_t esp_dev;
  int32_t timeout_ms;
};

static int32_t esp_err_to_ii2c(esp_err_t err) {
  switch (err) {
    case ESP_OK:
      return II2C_ERR_NONE;
    case ESP_ERR_NO_MEM:
      return II2C_ERR_NO_MEM;
    case ESP_ERR_INVALID_ARG:
      return II2C_ERR_INVALID_ARG;
    case ESP_ERR_INVALID_STATE:
      return II2C_ERR_INVALID_STATE;
    case ESP_ERR_NOT_FOUND:
      return II2C_ERR_NOT_FOUND;
    case ESP_ERR_NOT_SUPPORTED:
      return II2C_ERR_NOT_SUPPORTED;
    case ESP_ERR_TIMEOUT:
      return II2C_ERR_TIMEOUT;
    default:
      return II2C_ERR_IO;
  }
}

static int32_t ii2c_clk_src_to_esp(ii2c_clock_source_t clk_src, i2c_clock_source_t *ret_clk_src) {
  if (ret_clk_src == NULL) {
    return II2C_ERR_INVALID_ARG;
  }

  switch (clk_src) {
#if SOC_I2C_SUPPORT_APB
    case II2C_CLK_SRC_APB:
      *ret_clk_src = I2C_CLK_SRC_APB;
      return II2C_ERR_NONE;
#endif
#if SOC_I2C_SUPPORT_RTC
    case II2C_CLK_SRC_XTAL:
      *ret_clk_src = I2C_CLK_SRC_XTAL;
      return II2C_ERR_NONE;
    case II2C_CLK_SRC_RC_FAST:
      *ret_clk_src = I2C_CLK_SRC_RC_FAST;
      return II2C_ERR_NONE;
#endif
    case II2C_CLK_SRC_DEFAULT:
      *ret_clk_src = I2C_CLK_SRC_DEFAULT;
      return II2C_ERR_NONE;
#if !SOC_I2C_SUPPORT_APB
    case II2C_CLK_SRC_APB:
#endif
#if !SOC_I2C_SUPPORT_RTC
    case II2C_CLK_SRC_XTAL:
    case II2C_CLK_SRC_RC_FAST:
#endif
      return II2C_ERR_NOT_SUPPORTED;
    default:
      return II2C_ERR_INVALID_ARG;
  }
}

const char *ii2c_err_to_name(int32_t err) {
  switch (err) {
    case II2C_ERR_NONE:
      return "II2C_ERR_NONE";
    case II2C_ERR_FAIL:
      return "II2C_ERR_FAIL";
    case II2C_ERR_NO_MEM:
      return "II2C_ERR_NO_MEM";
    case II2C_ERR_INVALID_ARG:
      return "II2C_ERR_INVALID_ARG";
    case II2C_ERR_INVALID_STATE:
      return "II2C_ERR_INVALID_STATE";
    case II2C_ERR_NOT_FOUND:
      return "II2C_ERR_NOT_FOUND";
    case II2C_ERR_NOT_SUPPORTED:
      return "II2C_ERR_NOT_SUPPORTED";
    case II2C_ERR_TIMEOUT:
      return "II2C_ERR_TIMEOUT";
    case II2C_ERR_IO:
      return "II2C_ERR_IO";
    default:
      return "II2C_ERR_UNKNOWN";
  }
}

void ii2c_get_default_master_bus_config(ii2c_master_bus_config_t *config) {
  if (config == NULL) {
    return;
  }
  config->port_num = II2C_PORT_AUTO;
  config->sda_io_num = -1;
  config->scl_io_num = -1;
  config->clk_src = II2C_CLK_SRC_DEFAULT;
  config->glitch_ignore_cnt = 7;
  config->enable_internal_pullup = false;
}

void ii2c_get_default_device_config(ii2c_device_config_t *config) {
  if (config == NULL) {
    return;
  }
  config->device_address = 0;
  config->scl_speed_hz = 100000U;
  config->timeout_ms = II2C_DEFAULT_TIMEOUT_MS;
}

int32_t ii2c_new_master_bus(const ii2c_master_bus_config_t *config,
                            ii2c_master_bus_handle_t *ret_bus) {
  if (config == NULL || ret_bus == NULL) {
    return II2C_ERR_INVALID_ARG;
  }

  i2c_clock_source_t esp_clk_src = I2C_CLK_SRC_DEFAULT;
  int32_t rc = ii2c_clk_src_to_esp(config->clk_src, &esp_clk_src);
  if (rc != II2C_ERR_NONE) {
    return rc;
  }

  i2c_master_bus_config_t esp_config = {
      .i2c_port = (i2c_port_num_t)config->port_num,
      .sda_io_num = (gpio_num_t)config->sda_io_num,
      .scl_io_num = (gpio_num_t)config->scl_io_num,
      .clk_source = esp_clk_src,
      .glitch_ignore_cnt = config->glitch_ignore_cnt,
      .intr_priority = 0,
      .trans_queue_depth = 0,
      .flags =
          {
              .enable_internal_pullup = config->enable_internal_pullup ? 1U : 0U,
              .allow_pd = 0U,
          },
  };

  i2c_master_bus_handle_t esp_bus = NULL;
  rc = esp_err_to_ii2c(i2c_new_master_bus(&esp_config, &esp_bus));
  if (rc != II2C_ERR_NONE) {
    return rc;
  }

  struct ii2c_master_bus_t *bus = calloc(1, sizeof(*bus));
  if (bus == NULL) {
    (void)i2c_del_master_bus(esp_bus);
    return II2C_ERR_NO_MEM;
  }

  bus->esp_bus = esp_bus;
  *ret_bus = bus;
  return II2C_ERR_NONE;
}

int32_t ii2c_del_master_bus(ii2c_master_bus_handle_t bus) {
  if (bus == NULL) {
    return II2C_ERR_INVALID_ARG;
  }

  int32_t rc = esp_err_to_ii2c(i2c_del_master_bus(bus->esp_bus));
  free(bus);
  return rc;
}

int32_t ii2c_new_device(ii2c_master_bus_handle_t bus,
                        const ii2c_device_config_t *config,
                        ii2c_device_handle_t *ret_device) {
  if (bus == NULL || config == NULL || ret_device == NULL) {
    return II2C_ERR_INVALID_ARG;
  }

  i2c_device_config_t esp_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = config->device_address,
      .scl_speed_hz = config->scl_speed_hz,
      .scl_wait_us = 0,
      .flags =
          {
              .disable_ack_check = 0U,
          },
  };

  i2c_master_dev_handle_t esp_dev = NULL;
  int32_t rc = esp_err_to_ii2c(i2c_master_bus_add_device(bus->esp_bus, &esp_config, &esp_dev));
  if (rc != II2C_ERR_NONE) {
    return rc;
  }

  struct ii2c_device_t *device = calloc(1, sizeof(*device));
  if (device == NULL) {
    (void)i2c_master_bus_rm_device(esp_dev);
    return II2C_ERR_NO_MEM;
  }

  device->esp_dev = esp_dev;
  device->timeout_ms = (config->timeout_ms == 0) ? II2C_DEFAULT_TIMEOUT_MS : config->timeout_ms;
  *ret_device = device;
  return II2C_ERR_NONE;
}

int32_t ii2c_del_device(ii2c_device_handle_t device) {
  if (device == NULL) {
    return II2C_ERR_INVALID_ARG;
  }

  int32_t rc = esp_err_to_ii2c(i2c_master_bus_rm_device(device->esp_dev));
  free(device);
  return rc;
}

int32_t ii2c_master_probe(ii2c_master_bus_handle_t bus,
                          uint16_t device_address,
                          int32_t timeout_ms) {
  if (bus == NULL || device_address > 0x7FU) {
    return II2C_ERR_INVALID_ARG;
  }

  int32_t probe_timeout_ms = (timeout_ms == 0) ? II2C_DEFAULT_TIMEOUT_MS : timeout_ms;
  return esp_err_to_ii2c(i2c_master_probe(bus->esp_bus, device_address, probe_timeout_ms));
}

int32_t ii2c_master_transmit(ii2c_device_handle_t device,
                             const uint8_t *write_buffer,
                             size_t write_size) {
  if (device == NULL || write_buffer == NULL || write_size == 0) {
    return II2C_ERR_INVALID_ARG;
  }

  return esp_err_to_ii2c(
      i2c_master_transmit(device->esp_dev, write_buffer, write_size, device->timeout_ms));
}

int32_t ii2c_master_transmit_receive(ii2c_device_handle_t device,
                                     const uint8_t *write_buffer,
                                     size_t write_size,
                                     uint8_t *read_buffer,
                                     size_t read_size) {
  if (device == NULL || write_buffer == NULL || write_size == 0 || read_buffer == NULL ||
      read_size == 0) {
    return II2C_ERR_INVALID_ARG;
  }

  return esp_err_to_ii2c(i2c_master_transmit_receive(
      device->esp_dev, write_buffer, write_size, read_buffer, read_size, device->timeout_ms));
}
