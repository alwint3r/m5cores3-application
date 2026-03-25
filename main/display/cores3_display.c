#include "display/cores3_display.h"

#include <stdio.h>
#include <string.h>

#include <aw9523b/aw9523b.h>
#include <igpio/igpio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const int32_t LCD_SPI_MOSI = 37;
static const int32_t LCD_SPI_SCK = 36;
static const int32_t LCD_SPI_CS = 3;
static const int32_t LCD_SPI_DC = 35;

static const uint16_t LCD_WIDTH = 320;
static const uint16_t LCD_HEIGHT = 240;
enum {
  LCD_SPI_MAX_TRANSFER_BYTES = 320 * 2,
};

static const uint8_t CORES3_AW9523B_LCD_RST_PORT = 1;
static const uint8_t CORES3_AW9523B_LCD_RST_PIN = 1;

static void delay_ms(uint32_t ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

static int32_t configure_lcd_dc_gpio(cores3_display_t *display) {
  igpio_config_t config;
  igpio_get_default_config(&config);

  config.io_num = LCD_SPI_DC;
  config.mode = IGPIO_MODE_OUTPUT;
  config.pull_mode = IGPIO_PULL_FLOATING;
  config.intr_type = IGPIO_INTR_DISABLED;

  int32_t err = igpio_configure(&config);
  if (err != IGPIO_ERR_NONE) {
    return err;
  }

  display->dc_gpio_configured = true;

  err = igpio_set_level(LCD_SPI_DC, false);
  if (err != IGPIO_ERR_NONE) {
    return err;
  }

  bool level = true;
  err = igpio_get_level(LCD_SPI_DC, &level);
  if (err != IGPIO_ERR_NONE) {
    return err;
  }

  if (level) {
    return IGPIO_ERR_INVALID_STATE;
  }

  puts("LCD DC GPIO configured on GPIO35 and verified low");
  return IGPIO_ERR_NONE;
}

static int32_t display_write_bytes(cores3_display_t *display,
                                   bool is_data,
                                   const uint8_t *bytes,
                                   size_t len) {
  if (display == NULL || display->spi_device == NULL || bytes == NULL || len == 0U) {
    return ISPI_ERR_INVALID_ARG;
  }

  int32_t err = igpio_set_level(LCD_SPI_DC, is_data);
  if (err != IGPIO_ERR_NONE) {
    return err;
  }

  ispi_transaction_t trans;
  ispi_get_default_transaction(&trans);
  trans.tx_buffer = bytes;
  trans.tx_size = len;

  return ispi_device_transfer(display->spi_device, &trans);
}

static int32_t transport_write_command(void *context, uint8_t command) {
  return display_write_bytes((cores3_display_t *)context, false, &command, 1U);
}

static int32_t transport_write_data(void *context, const uint8_t *data, size_t len) {
  return display_write_bytes((cores3_display_t *)context, true, data, len);
}

static void transport_delay_ms(void *context, uint32_t ms) {
  (void)context;
  delay_ms(ms);
}

static int32_t lcd_hard_reset(cores3_display_t *display) {
  if (display == NULL || display->io_expander == NULL) {
    return AW9523B_ERR_INVALID_ARG;
  }

  int32_t err = aw9523b_level_set(
      display->io_expander, CORES3_AW9523B_LCD_RST_PORT, CORES3_AW9523B_LCD_RST_PIN, 0);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  delay_ms(20);

  err = aw9523b_level_set(
      display->io_expander, CORES3_AW9523B_LCD_RST_PORT, CORES3_AW9523B_LCD_RST_PIN, 1);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  delay_ms(120);
  puts("LCD hard reset complete");
  return AW9523B_ERR_NONE;
}

void cores3_display_deinit(cores3_display_t *display) {
  if (display == NULL) {
    return;
  }

  if (display->spi_device != NULL) {
    (void)ispi_del_device(display->spi_device);
    display->spi_device = NULL;
  }

  if (display->spi_bus != NULL) {
    (void)ispi_del_master_bus(display->spi_bus);
    display->spi_bus = NULL;
  }

  if (display->dc_gpio_configured) {
    (void)igpio_reset_pin(LCD_SPI_DC);
    display->dc_gpio_configured = false;
  }

  memset(&display->panel, 0, sizeof(display->panel));
  display->io_expander = NULL;
}

int32_t cores3_display_init(cores3_display_t *display, aw9523b_t *io_expander) {
  if (display == NULL || io_expander == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  memset(display, 0, sizeof(*display));
  display->io_expander = io_expander;

  int32_t err = aw9523b_port_dir_set(io_expander,
                                     CORES3_AW9523B_LCD_RST_PORT,
                                     CORES3_AW9523B_LCD_RST_PIN,
                                     AW9523B_PORT_DIRECTION_OUTPUT);
  if (err != AW9523B_ERR_NONE) {
    cores3_display_deinit(display);
    return err;
  }

  err = aw9523b_level_set(io_expander, CORES3_AW9523B_LCD_RST_PORT, CORES3_AW9523B_LCD_RST_PIN, 1);
  if (err != AW9523B_ERR_NONE) {
    cores3_display_deinit(display);
    return err;
  }

  err = configure_lcd_dc_gpio(display);
  if (err != IGPIO_ERR_NONE) {
    cores3_display_deinit(display);
    return err;
  }

  ispi_master_bus_config_t spi_bus_cfg = {0};
  ispi_get_default_master_bus_config(&spi_bus_cfg);
  spi_bus_cfg.host = ISPI_HOST_SPI2;
  spi_bus_cfg.mosi_io_num = LCD_SPI_MOSI;
  spi_bus_cfg.miso_io_num = -1;
  spi_bus_cfg.sclk_io_num = LCD_SPI_SCK;
  spi_bus_cfg.max_transfer_sz = LCD_SPI_MAX_TRANSFER_BYTES;

  err = ispi_new_master_bus(&spi_bus_cfg, &display->spi_bus);
  if (err != ISPI_ERR_NONE) {
    cores3_display_deinit(display);
    return err;
  }

  ispi_device_config_t lcd_dev_cfg = {0};
  ispi_get_default_device_config(&lcd_dev_cfg);
  lcd_dev_cfg.cs_io_num = LCD_SPI_CS;
  lcd_dev_cfg.clock_speed_hz = 40000000;
  lcd_dev_cfg.mode = 0;

  err = ispi_new_device(display->spi_bus, &lcd_dev_cfg, &display->spi_device);
  if (err != ISPI_ERR_NONE) {
    cores3_display_deinit(display);
    return err;
  }

  err = lcd_hard_reset(display);
  if (err != AW9523B_ERR_NONE) {
    cores3_display_deinit(display);
    return err;
  }

  display->panel.transport_context = display;
  display->panel.transport_command_write = transport_write_command;
  display->panel.transport_data_write = transport_write_data;
  display->panel.delay_fn = transport_delay_ms;

  err = ili9342_init_default(&display->panel);
  if (err != ILI9342_ERR_NONE) {
    cores3_display_deinit(display);
    return err;
  }

  err = ili9342_memory_access_control_set(
      &display->panel, ILI9342_MADCTL_BGR | ILI9342_MADCTL_MX | ILI9342_MADCTL_MY);
  if (err != ILI9342_ERR_NONE) {
    cores3_display_deinit(display);
    return err;
  }

  puts("LCD panel init commands complete");
  return ILI9342_ERR_NONE;
}

ili9342_t *cores3_display_panel(cores3_display_t *display) {
  if (display == NULL) {
    return NULL;
  }

  return &display->panel;
}

uint16_t cores3_display_width(void) {
  return LCD_WIDTH;
}

uint16_t cores3_display_height(void) {
  return LCD_HEIGHT;
}

size_t cores3_display_max_transfer_bytes(void) {
  return LCD_SPI_MAX_TRANSFER_BYTES;
}
