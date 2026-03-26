#include "cores3_io_extender.h"
#include "cores3_board_constants.h"

#include <stddef.h>
#include <stdint.h>

#include <igpio/igpio.h>

#include <driver/gpio.h>
#include <esp_err.h>

static ii2c_device_handle_t aw9523b_device_from_context(void *context) {
  return (ii2c_device_handle_t)context;
}

const char *cores3_io_extender_err_to_name(int32_t err) {
  if (err >= AW9523B_ERR_BASE && err < (AW9523B_ERR_BASE + 0x100)) {
    return aw9523b_err_to_name(err);
  }
  if (err >= II2C_ERR_BASE && err < (II2C_ERR_BASE + 0x100)) {
    return ii2c_err_to_name(err);
  }
  if (err >= IGPIO_ERR_BASE && err < (IGPIO_ERR_BASE + 0x100)) {
    return igpio_err_to_name(err);
  }

  return esp_err_to_name(err);
}

static int32_t aw9523b_i2c_write(void *context, const uint8_t *write_buffer, size_t write_size) {
  ii2c_device_handle_t device = aw9523b_device_from_context(context);
  if (device == NULL) {
    return II2C_ERR_INVALID_ARG;
  }

  return ii2c_master_transmit(device, write_buffer, write_size);
}

static int32_t aw9523b_i2c_write_read(void *context,
                                      const uint8_t *write_buffer,
                                      size_t write_size,
                                      uint8_t *read_buffer,
                                      size_t *read_size,
                                      size_t read_capacity) {
  ii2c_device_handle_t device = aw9523b_device_from_context(context);
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

static void IRAM_ATTR host_gpio_intr_handler(void *arg) {
  TaskHandle_t notify_task = (TaskHandle_t)arg;
  if (notify_task == NULL) {
    return;
  }

  BaseType_t higher_priority_task_woken = pdFALSE;
  vTaskNotifyGiveFromISR(notify_task, &higher_priority_task_woken);
  if (higher_priority_task_woken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

int32_t cores3_io_extender_init(ii2c_device_handle_t device, aw9523b_t *expander) {
  if (device == NULL || expander == NULL) {
    return II2C_ERR_INVALID_ARG;
  }

  expander->transport_context = device;
  expander->transport_write = aw9523b_i2c_write;
  expander->transport_write_read = aw9523b_i2c_write_read;
  return II2C_ERR_NONE;
}

int32_t cores3_io_extender_host_interrupt_init(TaskHandle_t notify_task) {
  if (notify_task == NULL) {
    return IGPIO_ERR_INVALID_ARG;
  }

  igpio_config_t config;
  igpio_get_default_config(&config);
  config.intr_type = IGPIO_INTR_ANYEDGE;
  config.io_num = CORES3_BOARD_I2C_INT;
  config.mode = IGPIO_MODE_INPUT;
  config.pull_mode = IGPIO_PULL_UP;

  int32_t err = igpio_configure(&config);
  if (err != IGPIO_ERR_NONE) {
    return err;
  }

  err = gpio_install_isr_service(0);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }

  (void)gpio_isr_handler_remove((gpio_num_t)CORES3_BOARD_I2C_INT);

  err = gpio_isr_handler_add(
      (gpio_num_t)CORES3_BOARD_I2C_INT, host_gpio_intr_handler, (void *)notify_task);
  if (err != ESP_OK) {
    return err;
  }

  return IGPIO_ERR_NONE;
}

void cores3_io_extender_deinit(aw9523b_t *expander) {
  (void)gpio_isr_handler_remove((gpio_num_t)CORES3_BOARD_I2C_INT);
  (void)igpio_reset_pin(CORES3_BOARD_I2C_INT);

  if (expander != NULL) {
    expander->transport_context = NULL;
    expander->transport_write = NULL;
    expander->transport_write_read = NULL;
  }
}
