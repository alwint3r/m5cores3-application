#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <aw9523b/aw9523b.h>
#include <ili9342/ili9342.h>
#include <ispi/ispi.h>

typedef struct {
  ispi_master_bus_handle_t spi_bus;
  ispi_device_handle_t spi_device;
  ili9342_t panel;
  aw9523b_t *io_expander;
  bool dc_gpio_configured;
} cores3_display_t;

int32_t cores3_display_init(cores3_display_t *display, aw9523b_t *io_expander);
void cores3_display_deinit(cores3_display_t *display);
ili9342_t *cores3_display_panel(cores3_display_t *display);
uint16_t cores3_display_width(void);
uint16_t cores3_display_height(void);
size_t cores3_display_max_transfer_bytes(void);
