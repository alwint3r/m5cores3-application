#include "cores3_touch.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const uint8_t CORES3_AW9523B_TOUCH_INT_PORT = 1;
static const uint8_t CORES3_AW9523B_TOUCH_INT_PIN = 2;
static const uint8_t CORES3_AW9523B_TOUCH_RST_PORT = 0;
static const uint8_t CORES3_AW9523B_TOUCH_RST_PIN = 0;
static const uint32_t CORES3_TOUCH_POWER_SETTLE_DELAY_MS = 10;
static const uint32_t CORES3_TOUCH_RESET_LOW_DELAY_MS = 10;
static const uint32_t CORES3_TOUCH_RESET_READY_DELAY_MS = 120;

static void delay_ms(uint32_t ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

static ii2c_device_handle_t ft6336_device_from_context(void *context) {
  return (ii2c_device_handle_t)context;
}

const char *cores3_touch_err_to_name(int32_t err) {
  if (err >= FT6X36_ERR_BASE && err < (FT6X36_ERR_BASE + 0x100)) {
    return ft6x36_err_to_name(err);
  }
  if (err >= AW9523B_ERR_BASE && err < (AW9523B_ERR_BASE + 0x100)) {
    return aw9523b_err_to_name(err);
  }

  return ii2c_err_to_name(err);
}

static int32_t ft6336_i2c_write(void *context, const uint8_t *write_buffer, size_t write_size) {
  ii2c_device_handle_t device = ft6336_device_from_context(context);
  if (device == NULL) {
    return II2C_ERR_INVALID_ARG;
  }

  return ii2c_master_transmit(device, write_buffer, write_size);
}

static int32_t ft6336_i2c_write_read(void *context,
                                     const uint8_t *write_buffer,
                                     size_t write_size,
                                     uint8_t *read_buffer,
                                     size_t *read_size,
                                     size_t read_capacity) {
  ii2c_device_handle_t device = ft6336_device_from_context(context);
  if (device == NULL || read_buffer == NULL || read_size == NULL) {
    return II2C_ERR_INVALID_ARG;
  }

  int32_t err =
      ii2c_master_transmit_receive(device, write_buffer, write_size, read_buffer, read_capacity);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  *read_size = read_capacity;
  return II2C_ERR_NONE;
}

static int32_t configure_touch_aw9523b_pins(aw9523b_t *expander) {
  int32_t err = aw9523b_port_dir_set(expander,
                                     CORES3_AW9523B_TOUCH_INT_PORT,
                                     CORES3_AW9523B_TOUCH_INT_PIN,
                                     AW9523B_PORT_DIRECTION_INPUT);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  err = aw9523b_port_dir_set(expander,
                             CORES3_AW9523B_TOUCH_RST_PORT,
                             CORES3_AW9523B_TOUCH_RST_PIN,
                             AW9523B_PORT_DIRECTION_OUTPUT);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  err = aw9523b_level_set(expander, CORES3_AW9523B_TOUCH_RST_PORT, CORES3_AW9523B_TOUCH_RST_PIN, 0);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  return AW9523B_ERR_NONE;
}

static int32_t prepare_touch_power_and_reset(aw9523b_t *expander) {
  if (expander == NULL) {
    return AW9523B_ERR_INVALID_ARG;
  }

  puts("FT6x36 transport attached; waiting for LCD-linked power to settle");
  delay_ms(CORES3_TOUCH_POWER_SETTLE_DELAY_MS);

  int32_t err =
      aw9523b_level_set(expander, CORES3_AW9523B_TOUCH_RST_PORT, CORES3_AW9523B_TOUCH_RST_PIN, 0);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  delay_ms(CORES3_TOUCH_RESET_LOW_DELAY_MS);

  err = aw9523b_level_set(expander, CORES3_AW9523B_TOUCH_RST_PORT, CORES3_AW9523B_TOUCH_RST_PIN, 1);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  delay_ms(CORES3_TOUCH_RESET_READY_DELAY_MS);

  uint8_t pin_level = 0;
  err = aw9523b_level_get(
      expander, CORES3_AW9523B_TOUCH_INT_PORT, CORES3_AW9523B_TOUCH_INT_PIN, &pin_level);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }
  (void)pin_level;

  err = aw9523b_interrupt_set(
      expander, CORES3_AW9523B_TOUCH_INT_PORT, CORES3_AW9523B_TOUCH_INT_PIN, true);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  puts("FT6x36 reset released.");
  return AW9523B_ERR_NONE;
}

int32_t cores3_touch_init(ii2c_device_handle_t device, aw9523b_t *expander, ft6x36_t *touch) {
  if (device == NULL || expander == NULL || touch == NULL) {
    return II2C_ERR_INVALID_ARG;
  }

  int32_t err = configure_touch_aw9523b_pins(expander);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  touch->transport_context = device;
  touch->transport_write = ft6336_i2c_write;
  touch->transport_write_read = ft6336_i2c_write_read;

  err = prepare_touch_power_and_reset(expander);
  if (err != AW9523B_ERR_NONE) {
    cores3_touch_deinit(touch);
    return err;
  }

  uint8_t touch_fwver = 0;
  err = ft6x36_firmware_version_get(touch, &touch_fwver);
  if (err != FT6X36_ERR_NONE) {
    cores3_touch_deinit(touch);
    return err;
  }

  printf("FT6x36 firmware version: %u\n", touch_fwver);

  err = ft6x36_interrupt_mode_set(touch, FT6X36_INTERRUPT_MODE_TRIGGER);
  if (err != FT6X36_ERR_NONE) {
    cores3_touch_deinit(touch);
    return err;
  }

  return FT6X36_ERR_NONE;
}

void cores3_touch_deinit(ft6x36_t *touch) {
  if (touch != NULL) {
    touch->transport_context = NULL;
    touch->transport_write = NULL;
    touch->transport_write_read = NULL;
  }
}
