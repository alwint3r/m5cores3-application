#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aw9523b/aw9523b.h>
#include <axp2101/axp2101.h>
#include <axp2101/axp2101_register.h>
#include <igpio/igpio.h>
#include <ii2c/ii2c.h>
#include <ili9342/ili9342.h>
#include <ispi/ispi.h>
#include "open_sans_regular_32_4bpp.h"
#include "bmf_reader.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

static const int32_t SYS_I2C_SDA = 12;
static const int32_t SYS_I2C_SCL = 11;
static const int32_t LCD_SPI_MOSI = 37;
static const int32_t LCD_SPI_SCK = 36;
static const int32_t LCD_SPI_CS = 3;
static const int32_t LCD_SPI_DC = 35;

static const uint16_t LCD_WIDTH = 320;
static const uint16_t LCD_HEIGHT = 240;
enum {
  LCD_ROW_BUFFER_BYTES = 320 * 2,
  LCD_SPI_MAX_TRANSFER_BYTES = 320 * 2,
};
static uint8_t LCD_ROW_BUFFER[320 * 2] = {0};

static const uint16_t CORES3_AW9523B_I2C_ADDRESS = 0x58;
static const uint16_t CORES3_AXP2101_I2C_ADDRESS = 0x34;
static const uint8_t CORES3_AW9523B_BOOST_EN_PORT = 1;
static const uint8_t CORES3_AW9523B_BOOST_EN_PIN = 7;
static const uint16_t CORES3_AXP2101_DCDC1_LCD_PWR_MV = 3300;
static const uint8_t CORES3_AW9523B_LCD_RST_PORT = 1;
static const uint8_t CORES3_AW9523B_LCD_RST_PIN = 1;

static ii2c_master_bus_handle_t sys_i2c = NULL;
static ii2c_device_handle_t aw9523b = NULL;
static ii2c_device_handle_t axp2101 = NULL;
static ispi_master_bus_handle_t lcd_spi = NULL;
static ispi_device_handle_t lcd = NULL;
static bool lcd_dc_gpio_configured = false;

typedef struct {
  const uint8_t *bitmap;
  int draw_x;
  int draw_y;
  int draw_y1;
  int width;
  int height;
  int stride;
} lcd_prepared_glyph_t;

typedef struct {
  int64_t prepare_us;
  int64_t stream_us;
  int64_t total_us;
  size_t visible_glyph_count;
  size_t row_bytes;
  size_t row_count;
} lcd_render_stats_t;

static int32_t lcd_write_buffer_chunked(ili9342_t *display, const uint8_t *buffer, size_t len);

static const char *bool_to_yes_no(bool value) {
  return value ? "yes" : "no";
}

static const char *axp2101_charging_status_to_string(axp2101_charging_status_t status) {
  switch (status) {
    case AXP2101_CHARGING_STATUS_TRI_CHARGE:
      return "trickle";
    case AXP2101_CHARGING_STATUS_PRE_CHARGE:
      return "pre-charge";
    case AXP2101_CHARGING_STATUS_CONSTANT_CHARGE:
      return "constant-current";
    case AXP2101_CHARGING_STATUS_CONSTANT_VOLTAGE:
      return "constant-voltage";
    case AXP2101_CHARGING_STATUS_CHARGE_DONE:
      return "charge-done";
    case AXP2101_CHARGING_STATUS_NOT_CHARGING:
      return "idle";
    case AXP2101_CHARGING_STATUS_UNKNOWN:
      return "unknown";
  }

  return "unknown";
}

static void release_handles(void) {
  if (lcd_dc_gpio_configured) {
    (void)igpio_reset_pin(LCD_SPI_DC);
    lcd_dc_gpio_configured = false;
  }

  if (aw9523b != NULL) {
    (void)ii2c_del_device(aw9523b);
    aw9523b = NULL;
  }

  if (axp2101 != NULL) {
    (void)ii2c_del_device(axp2101);
    axp2101 = NULL;
  }

  if (sys_i2c != NULL) {
    (void)ii2c_del_master_bus(sys_i2c);
    sys_i2c = NULL;
  }
}

static int32_t configure_lcd_dc_gpio(void) {
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

  lcd_dc_gpio_configured = true;

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

static void delay_ms(uint32_t ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

static int32_t lcd_write_bytes(bool is_data, const uint8_t *bytes, size_t len) {
  if (bytes == NULL || len == 0) {
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

  return ispi_device_transfer(lcd, &trans);
}

static int32_t lcd_write_command(uint8_t cmd) {
  return lcd_write_bytes(false, &cmd, 1);
}

static int32_t lcd_write_data(const uint8_t *bytes, size_t len) {
  return lcd_write_bytes(true, bytes, len);
}

static int32_t lcd_hard_reset(void) {
  int32_t err =
      aw9523b_level_set(aw9523b, CORES3_AW9523B_LCD_RST_PORT, CORES3_AW9523B_LCD_RST_PIN, 0);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  delay_ms(20);

  err = aw9523b_level_set(aw9523b, CORES3_AW9523B_LCD_RST_PORT, CORES3_AW9523B_LCD_RST_PIN, 1);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  delay_ms(120);
  puts("LCD hard reset complete");
  return II2C_ERR_NONE;
}

static int32_t lcd_fill_rect(ili9342_t *display,
                             uint16_t x0,
                             uint16_t y0,
                             uint16_t x1,
                             uint16_t y1,
                             uint16_t color) {
  if (display == NULL || x1 < x0 || y1 < y0 || x1 >= LCD_WIDTH || y1 >= LCD_HEIGHT) {
    return ILI9342_ERR_INVALID_ARG;
  }

  size_t row_pixels = (size_t)(x1 - x0 + 1U);
  size_t row_bytes = row_pixels * 2U;
  if (row_bytes == 0U || row_bytes > LCD_ROW_BUFFER_BYTES) {
    return ILI9342_ERR_INVALID_ARG;
  }

  for (size_t offset = 0; offset < row_bytes; offset += 2U) {
    LCD_ROW_BUFFER[offset + 0U] = (uint8_t)(color >> 8);
    LCD_ROW_BUFFER[offset + 1U] = (uint8_t)(color & 0xFF);
  }

  int32_t err = ili9342_address_window_set(display, x0, y0, x1, y1);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  size_t row_count = (size_t)(y1 - y0 + 1U);
  for (size_t row = 0; row < row_count; row++) {
    err = lcd_write_buffer_chunked(display, LCD_ROW_BUFFER, row_bytes);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }
  }

  return ILI9342_ERR_NONE;
}

static int32_t lcd_fill_screen(ili9342_t *display, uint16_t color) {
  int32_t err = lcd_fill_rect(display, 0U, 0U, LCD_WIDTH - 1U, LCD_HEIGHT - 1U, color);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  puts("LCD display memory fill complete");
  return ILI9342_ERR_NONE;
}

inline uint8_t n_to_m_bits(uint8_t data, uint8_t n, uint8_t m) {
  return (data * ((1 << m) - 1)) / ((1 << n) - 1);
}

static uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
  uint8_t r5 = n_to_m_bits(r, 8, 5);
  uint8_t g6 = n_to_m_bits(g, 8, 6);
  uint8_t b5 = n_to_m_bits(b, 8, 5);

  return (uint16_t)(r5 << 11) | (uint16_t)(g6 << 5) | (uint16_t)b5;
}

static __attribute__((unused)) int32_t
lcd_fill_buffer_rgb888(uint8_t *buffer, size_t buffer_len, uint8_t r, uint8_t g, uint8_t b) {
  if (buffer == NULL || (buffer_len % 2U) != 0U) {
    return ISPI_ERR_INVALID_ARG;
  }

  uint16_t color = rgb888_to_rgb565(r, g, b);

  for (size_t i = 0; i < buffer_len; i += 2U) {
    buffer[i] = (uint8_t)(color >> 8);
    buffer[i + 1U] = (uint8_t)(color & 0xFF);
  }

  return ISPI_ERR_NONE;
}

static __attribute__((unused)) int32_t lcd_buffer_set_pixel_rgb565(uint8_t *buffer,
                                                                   uint16_t buffer_width,
                                                                   uint16_t buffer_height,
                                                                   uint16_t x,
                                                                   uint16_t y,
                                                                   uint16_t color) {
  if (buffer == NULL) {
    return ISPI_ERR_INVALID_ARG;
  }

  if (x >= buffer_width || y >= buffer_height) {
    return ISPI_ERR_INVALID_ARG;
  }

  size_t pixel_index = ((size_t)y * (size_t)buffer_width) + (size_t)x;
  size_t byte_index = pixel_index * 2U;
  buffer[byte_index + 0U] = (uint8_t)(color >> 8);
  buffer[byte_index + 1U] = (uint8_t)(color & 0xFF);
  return ISPI_ERR_NONE;
}

static int32_t lcd_write_buffer_chunked(ili9342_t *display, const uint8_t *buffer, size_t len) {
  if (display == NULL || buffer == NULL || len == 0U) {
    return ILI9342_ERR_INVALID_ARG;
  }

  size_t offset = 0U;
  while (offset < len) {
    size_t bytes_this_round = len - offset;
    if (bytes_this_round > LCD_SPI_MAX_TRANSFER_BYTES) {
      bytes_this_round = LCD_SPI_MAX_TRANSFER_BYTES;
    }
    int32_t err = ili9342_write_data(display, buffer + offset, bytes_this_round);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }

    offset += bytes_this_round;
  }

  return ISPI_ERR_NONE;
}

static uint16_t blend_rgb565(uint16_t bg, uint16_t fg, uint8_t coverage) {
  uint32_t bg_r = (bg >> 11) & 0x1F;
  uint32_t bg_g = (bg >> 5) & 0x3F;
  uint32_t bg_b = bg & 0x1F;

  uint32_t fg_r = (fg >> 11) & 0x1F;
  uint32_t fg_g = (fg >> 5) & 0x3F;
  uint32_t fg_b = fg & 0x1F;

  uint32_t inv = 255U - coverage;
  uint32_t out_r = (fg_r * coverage + bg_r * inv + 127U) / 255U;
  uint32_t out_g = (fg_g * coverage + bg_g * inv + 127U) / 255U;
  uint32_t out_b = (fg_b * coverage + bg_b * inv + 127U) / 255U;

  return (uint16_t)((out_r << 11) | (out_g << 5) | out_b);
}

static int32_t lcd_prepare_glyph_run(uint16_t screen_width,
                                     uint16_t screen_height,
                                     const char *text,
                                     uint16_t x,
                                     uint16_t y,
                                     bmf_font_view_t *font_view,
                                     lcd_prepared_glyph_t *prepared_glyphs,
                                     size_t prepared_capacity,
                                     size_t *visible_glyph_count,
                                     int *text_clip_x0,
                                     int *text_clip_y0,
                                     int *text_clip_x1,
                                     int *text_clip_y1) {
  if (text == NULL || font_view == NULL || prepared_glyphs == NULL || visible_glyph_count == NULL ||
      text_clip_x0 == NULL || text_clip_y0 == NULL || text_clip_x1 == NULL ||
      text_clip_y1 == NULL || screen_width == 0U || screen_height == 0U ||
      screen_width > LCD_WIDTH) {
    return ILI9342_ERR_INVALID_ARG;
  }

  size_t text_length = strlen(text);
  if (text_length > prepared_capacity) {
    return ILI9342_ERR_INVALID_ARG;
  }

  bool has_visible_pixels = false;
  size_t prepared_count = 0U;
  int baseline_y = y;
  int pen_x = x;

  for (size_t i = 0; i < text_length; i++) {
    bmf_glyph_record_t glyph;

    uint32_t codepoint = (uint8_t)text[i];
    bmf_status_t glyph_status = bmf_font_view_find_glyph(font_view, codepoint, &glyph, NULL);
    if (glyph_status != BMF_STATUS_OK) {
      return ISPI_ERR_FAIL;
    }

    int width = 0;
    int height = 0;
    const uint8_t *bitmap = bmf_font_view_get_glyph_bitmap(font_view, &glyph, &width, &height);
    if (bitmap == NULL || width == 0 || height == 0) {
      pen_x += glyph.x_advance;
      continue;
    }

    int draw_x = pen_x + glyph.x_offset;
    int draw_y = baseline_y + glyph.y_offset;
    int clip_x0 = draw_x < 0 ? 0 : draw_x;
    int clip_y0 = draw_y < 0 ? 0 : draw_y;
    int clip_x1 = draw_x + width - 1;
    int clip_y1 = draw_y + height - 1;

    if (clip_x0 >= screen_width || clip_y0 >= screen_height || clip_x1 < 0 || clip_y1 < 0) {
      pen_x += glyph.x_advance;
      continue;
    }

    if (clip_x1 >= screen_width) {
      clip_x1 = (int)screen_width - 1;
    }

    if (clip_y1 >= screen_height) {
      clip_y1 = (int)screen_height - 1;
    }

    int stride = 0;
    if (font_view->bpp == BMF_BPP_MONO) {
      stride = (int)((width + 7) / 8);
    } else if (font_view->bpp == BMF_BPP_GRAY4) {
      stride = (int)((width + 1) / 2);
    } else if (font_view->bpp == BMF_BPP_GRAY8) {
      stride = width;
    } else {
      return ILI9342_ERR_INVALID_ARG;
    }

    lcd_prepared_glyph_t *prepared = &prepared_glyphs[prepared_count++];
    prepared->bitmap = bitmap;
    prepared->draw_x = draw_x;
    prepared->draw_y = draw_y;
    prepared->draw_y1 = draw_y + height - 1;
    prepared->width = width;
    prepared->height = height;
    prepared->stride = stride;

    if (!has_visible_pixels) {
      *text_clip_x0 = clip_x0;
      *text_clip_y0 = clip_y0;
      *text_clip_x1 = clip_x1;
      *text_clip_y1 = clip_y1;
      has_visible_pixels = true;
    } else {
      if (clip_x0 < *text_clip_x0) {
        *text_clip_x0 = clip_x0;
      }
      if (clip_y0 < *text_clip_y0) {
        *text_clip_y0 = clip_y0;
      }
      if (clip_x1 > *text_clip_x1) {
        *text_clip_x1 = clip_x1;
      }
      if (clip_y1 > *text_clip_y1) {
        *text_clip_y1 = clip_y1;
      }
    }

    pen_x += glyph.x_advance;
  }

  *visible_glyph_count = prepared_count;
  return ILI9342_ERR_NONE;
}

static int32_t lcd_render_c_str_direct(uint16_t screen_width,
                                       uint16_t screen_height,
                                       const char *text,
                                       uint16_t x,
                                       uint16_t y,
                                       uint16_t background_color,
                                       uint16_t foreground_color,
                                       bmf_font_view_t *font_view,
                                       ili9342_t *display,
                                       lcd_render_stats_t *stats) {
  if (display == NULL || text == NULL || font_view == NULL || screen_width == 0U ||
      screen_height == 0U || screen_width > LCD_WIDTH) {
    return ILI9342_ERR_INVALID_ARG;
  }

  if (stats != NULL) {
    memset(stats, 0, sizeof(*stats));
  }

  size_t text_length = strlen(text);
  if (text_length == 0U) {
    return ISPI_ERR_NONE;
  }

  lcd_prepared_glyph_t *prepared_glyphs = calloc(text_length, sizeof(*prepared_glyphs));
  if (prepared_glyphs == NULL) {
    return ILI9342_ERR_NO_MEM;
  }

  int64_t total_start = esp_timer_get_time();
  int text_clip_x0 = 0;
  int text_clip_y0 = 0;
  int text_clip_x1 = 0;
  int text_clip_y1 = 0;
  size_t visible_glyph_count = 0U;
  int64_t prepare_start = esp_timer_get_time();
  int32_t err = lcd_prepare_glyph_run(screen_width,
                                      screen_height,
                                      text,
                                      x,
                                      y,
                                      font_view,
                                      prepared_glyphs,
                                      text_length,
                                      &visible_glyph_count,
                                      &text_clip_x0,
                                      &text_clip_y0,
                                      &text_clip_x1,
                                      &text_clip_y1);
  int64_t prepare_us = esp_timer_get_time() - prepare_start;
  if (err != ILI9342_ERR_NONE) {
    free(prepared_glyphs);
    return err;
  }

  if (stats != NULL) {
    stats->prepare_us = prepare_us;
    stats->visible_glyph_count = visible_glyph_count;
  }

  if (visible_glyph_count == 0U) {
    if (stats != NULL) {
      stats->total_us = esp_timer_get_time() - total_start;
    }
    free(prepared_glyphs);
    return ISPI_ERR_NONE;
  }

  size_t row_pixels = (size_t)(text_clip_x1 - text_clip_x0 + 1);
  size_t row_bytes = row_pixels * 2U;
  size_t row_count = (size_t)(text_clip_y1 - text_clip_y0 + 1);
  uint8_t background_hi = (uint8_t)(background_color >> 8);
  uint8_t background_lo = (uint8_t)(background_color & 0xFF);
  if (stats != NULL) {
    stats->row_bytes = row_bytes;
    stats->row_count = row_count;
  }

  err = ili9342_address_window_set(display,
                                   (uint16_t)text_clip_x0,
                                   (uint16_t)text_clip_y0,
                                   (uint16_t)text_clip_x1,
                                   (uint16_t)text_clip_y1);
  if (err != ILI9342_ERR_NONE) {
    free(prepared_glyphs);
    return err;
  }

  int64_t stream_start = esp_timer_get_time();
  for (int dst_y = text_clip_y0; dst_y <= text_clip_y1; dst_y++) {
    for (size_t row_offset = 0; row_offset < row_bytes; row_offset += 2U) {
      LCD_ROW_BUFFER[row_offset + 0U] = background_hi;
      LCD_ROW_BUFFER[row_offset + 1U] = background_lo;
    }

    for (size_t i = 0; i < visible_glyph_count; i++) {
      const lcd_prepared_glyph_t *prepared = &prepared_glyphs[i];
      if (dst_y < prepared->draw_y || dst_y > prepared->draw_y1) {
        continue;
      }

      int clip_x0 = prepared->draw_x < text_clip_x0 ? text_clip_x0 : prepared->draw_x;
      int clip_x1 = prepared->draw_x + prepared->width - 1;
      if (clip_x1 > text_clip_x1) {
        clip_x1 = text_clip_x1;
      }

      if (clip_x0 <= clip_x1) {
        int src_y = dst_y - prepared->draw_y;
        for (int dst_x = clip_x0; dst_x <= clip_x1; dst_x++) {
          int src_x = dst_x - prepared->draw_x;
          size_t row_offset = (size_t)(dst_x - text_clip_x0) * 2U;

          if (font_view->bpp == BMF_BPP_MONO) {
            int byte_idx = src_y * prepared->stride + src_x / 8;
            int bit_idx = 7 - (src_x % 8);
            uint8_t byte = prepared->bitmap[byte_idx];
            if (((byte >> bit_idx) & 1U) != 0U) {
              LCD_ROW_BUFFER[row_offset + 0U] = (uint8_t)(foreground_color >> 8);
              LCD_ROW_BUFFER[row_offset + 1U] = (uint8_t)(foreground_color & 0xFF);
            }
          } else if (font_view->bpp == BMF_BPP_GRAY4) {
            int byte_idx = src_y * prepared->stride + src_x / 2;
            uint8_t byte = prepared->bitmap[byte_idx];
            uint8_t coverage = (src_x % 2 == 0) ? (byte >> 4) : (byte & 0x0F);
            coverage = (coverage << 4) | coverage;
            if (coverage != 0U) {
              uint16_t color = blend_rgb565(background_color, foreground_color, coverage);
              LCD_ROW_BUFFER[row_offset + 0U] = (uint8_t)(color >> 8);
              LCD_ROW_BUFFER[row_offset + 1U] = (uint8_t)(color & 0xFF);
            }
          } else {
            uint8_t coverage = prepared->bitmap[src_y * prepared->stride + src_x];
            if (coverage != 0U) {
              uint16_t color = blend_rgb565(background_color, foreground_color, coverage);
              LCD_ROW_BUFFER[row_offset + 0U] = (uint8_t)(color >> 8);
              LCD_ROW_BUFFER[row_offset + 1U] = (uint8_t)(color & 0xFF);
            }
          }
        }
      }
    }

    err = lcd_write_buffer_chunked(display, LCD_ROW_BUFFER, row_bytes);
    if (err != ILI9342_ERR_NONE) {
      free(prepared_glyphs);
      return err;
    }
  }

  if (stats != NULL) {
    stats->stream_us = esp_timer_get_time() - stream_start;
    stats->total_us = esp_timer_get_time() - total_start;
  }

  free(prepared_glyphs);
  return ISPI_ERR_NONE;
}

static int32_t create_i2c_bus(void) {
  ii2c_master_bus_config_t bus_cfg;
  ii2c_get_default_master_bus_config(&bus_cfg);

  bus_cfg.sda_io_num = SYS_I2C_SDA;
  bus_cfg.scl_io_num = SYS_I2C_SCL;
  bus_cfg.enable_internal_pullup = true;

  return ii2c_new_master_bus(&bus_cfg, &sys_i2c);
}

static int32_t probe_i2c_device(const char *label, uint16_t address) {
  int32_t err = ii2c_master_probe(sys_i2c, address, 3000);
  if (err == II2C_ERR_NONE) {
    printf("%s responded at 0x%02X\n", label, address);
  }
  return err;
}

static int32_t attach_i2c_device(uint16_t address, ii2c_device_handle_t *out_device) {
  ii2c_device_config_t dev_cfg;
  ii2c_get_default_device_config(&dev_cfg);

  dev_cfg.device_address = address;
  dev_cfg.timeout_ms = 3000;

  return ii2c_new_device(sys_i2c, &dev_cfg, out_device);
}

static int32_t configure_aw9523b_boost_enable(ii2c_device_handle_t dev) {
  int32_t err = aw9523b_port_dir_set(dev,
                                     CORES3_AW9523B_BOOST_EN_PORT,
                                     CORES3_AW9523B_BOOST_EN_PIN,
                                     AW9523B_PORT_DIRECTION_OUTPUT);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = aw9523b_level_set(dev, CORES3_AW9523B_BOOST_EN_PORT, CORES3_AW9523B_BOOST_EN_PIN, 1);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint8_t boost_en_level = 0;
  err = aw9523b_level_get(
      dev, CORES3_AW9523B_BOOST_EN_PORT, CORES3_AW9523B_BOOST_EN_PIN, &boost_en_level);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  if (boost_en_level == 0) {
    return II2C_ERR_INVALID_STATE;
  }

  puts("AW9523B BOOST_EN asserted");
  return II2C_ERR_NONE;
}

static int32_t configure_axp2101_ldos(ii2c_device_handle_t dev) {
  uint8_t ldo_mask = AXP2101_LDO_CTRL0_EN_DLDO1 | AXP2101_LDO_CTRL0_EN_BLDO2 |
                     AXP2101_LDO_CTRL0_EN_BLDO1 | AXP2101_LDO_CTRL0_EN_ALDO4 |
                     AXP2101_LDO_CTRL0_EN_ALDO3 | AXP2101_LDO_CTRL0_EN_ALDO2 |
                     AXP2101_LDO_CTRL0_EN_ALDO1;

  int32_t err = axp2101_ldo_ctrl0_enable(dev, ldo_mask);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  axp2101_ldo_ctrl0_t ldo_state = {0};
  err = axp2101_ldo_ctrl0_get(dev, &ldo_state);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  if (!ldo_state.dldo1_en || ldo_state.cpusldo_en || !ldo_state.bldo2_en || !ldo_state.bldo1_en ||
      !ldo_state.aldo4_en || !ldo_state.aldo3_en || !ldo_state.aldo2_en || !ldo_state.aldo1_en) {
    return II2C_ERR_INVALID_STATE;
  }

  err = axp2101_aldo1_voltage_set(dev, 1800);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo2_voltage_set(dev, 3300);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo3_voltage_set(dev, 3300);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo4_voltage_set(dev, 3300);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint16_t aldo1_mv = 0;
  uint16_t aldo2_mv = 0;
  uint16_t aldo3_mv = 0;
  uint16_t aldo4_mv = 0;

  err = axp2101_aldo1_voltage_get(dev, &aldo1_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo2_voltage_get(dev, &aldo2_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo3_voltage_get(dev, &aldo3_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo4_voltage_get(dev, &aldo4_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  if (aldo1_mv != 1800 || aldo2_mv != 3300 || aldo3_mv != 3300 || aldo4_mv != 3300) {
    return II2C_ERR_INVALID_STATE;
  }

  puts("AXP2101 LDO rails configured");
  return II2C_ERR_NONE;
}

static int32_t configure_axp2101_dcdc1(ii2c_device_handle_t dev) {
  int32_t err = axp2101_dcdc1_voltage_set(dev, CORES3_AXP2101_DCDC1_LCD_PWR_MV);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_dcdc_ctrl0_enable(dev, AXP2101_DCDC_CTRL0_EN_DCDC1);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint16_t dcdc1_mv = 0;
  err = axp2101_dcdc1_voltage_get(dev, &dcdc1_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  axp2101_dcdc_ctrl0_t dcdc_state = {0};
  err = axp2101_dcdc_ctrl0_get(dev, &dcdc_state);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  if (dcdc1_mv != CORES3_AXP2101_DCDC1_LCD_PWR_MV || !dcdc_state.dcdc1_en) {
    return II2C_ERR_INVALID_STATE;
  }

  printf("AXP2101 DCDC1 configured to %u mV\n", dcdc1_mv);
  return II2C_ERR_NONE;
}

static int32_t configure_axp2101_power_key(ii2c_device_handle_t dev) {
  axp2101_irq_off_on_level_t config = {
      .irq_time = AXP2101_POWER_KEY_IRQ_TIME_1S,
      .poweroff_time = AXP2101_POWER_KEY_POWEROFF_TIME_4S,
      .poweron_time = AXP2101_POWER_KEY_ON_TIME_128MS,
  };

  int32_t err = axp2101_irq_off_on_level_set(dev, &config);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  axp2101_irq_off_on_level_t readback = {0};
  err = axp2101_irq_off_on_level_get(dev, &readback);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  if (readback.irq_time != config.irq_time || readback.poweroff_time != config.poweroff_time ||
      readback.poweron_time != config.poweron_time) {
    return II2C_ERR_INVALID_STATE;
  }

  puts("AXP2101 power-key timing configured");
  return II2C_ERR_NONE;
}

static int32_t configure_axp2101_chgled(ii2c_device_handle_t dev) {
  axp2101_chgled_ctrl_t config = {
      .enabled = true,
      .function = AXP2101_CHGLED_FUNCTION_TYPE_A,
      .output = AXP2101_CHGLED_OUTPUT_BLINK_1HZ,
  };

  int32_t err = axp2101_chgled_ctrl_set(dev, &config);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  axp2101_chgled_ctrl_t readback = {0};
  err = axp2101_chgled_ctrl_get(dev, &readback);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  if (!readback.enabled || readback.function != config.function ||
      readback.output != config.output) {
    return II2C_ERR_INVALID_STATE;
  }

  puts("AXP2101 CHGLED configured");
  return II2C_ERR_NONE;
}

static int32_t configure_axp2101_pmu_common(ii2c_device_handle_t dev) {
  axp2101_pmu_common_cfg_t config = {
      .raw_bits_7_6 = 0,
      .internal_off_discharge_enabled = true,
      .raw_bit4 = true,
      .pwrok_restart_enabled = false,
      .pwron_16s_shutdown_enabled = false,
      .restart_system = false,
      .soft_pwroff = false,
  };

  int32_t err = axp2101_pmu_common_cfg_set(dev, &config);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  axp2101_pmu_common_cfg_t readback = {0};
  err = axp2101_pmu_common_cfg_get(dev, &readback);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  if (readback.raw_bits_7_6 != config.raw_bits_7_6 ||
      readback.internal_off_discharge_enabled != config.internal_off_discharge_enabled ||
      readback.raw_bit4 != config.raw_bit4 ||
      readback.pwrok_restart_enabled != config.pwrok_restart_enabled ||
      readback.pwron_16s_shutdown_enabled != config.pwron_16s_shutdown_enabled) {
    return II2C_ERR_INVALID_STATE;
  }

  puts("AXP2101 PMU common configuration applied");
  return II2C_ERR_NONE;
}

static int32_t configure_axp2101_adc(ii2c_device_handle_t dev) {
  uint8_t adc_channels =
      AXP2101_ADC_EN_VSYS | AXP2101_ADC_EN_VBUS | AXP2101_ADC_EN_TS | AXP2101_ADC_EN_BATT;
  int32_t err = axp2101_adc_enable_channels(dev, adc_channels);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  puts("AXP2101 ADC channels enabled");
  return II2C_ERR_NONE;
}

static int32_t apply_cores3_axp2101_startup(ii2c_device_handle_t dev) {
  int32_t err = configure_axp2101_dcdc1(dev);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = configure_axp2101_ldos(dev);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = configure_axp2101_power_key(dev);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = configure_axp2101_chgled(dev);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = configure_axp2101_pmu_common(dev);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  return configure_axp2101_adc(dev);
}

static int32_t print_axp2101_summary(ii2c_device_handle_t dev) {
  axp2101_status1_t status1 = {0};
  int32_t err = axp2101_status1_get(dev, &status1);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  axp2101_status2_t status2 = {0};
  err = axp2101_status2_get(dev, &status2);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint16_t vbus_mv = 0;
  err = axp2101_adc_vbus_read(dev, &vbus_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint16_t vsys_mv = 0;
  err = axp2101_adc_vsys_read(dev, &vsys_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint16_t vbat_mv = 0;
  err = axp2101_adc_vbat_read(dev, &vbat_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  printf("VBUS good: %s\n", bool_to_yes_no(status1.vbus_good));
  printf("Battery present: %s\n", bool_to_yes_no(status1.battery_present));
  printf("System power on: %s\n", bool_to_yes_no(status2.system_power_on));
  printf("Charging status: %s\n", axp2101_charging_status_to_string(status2.charging_status));
  printf("VBUS: %u mV\n", vbus_mv);
  printf("VSYS: %u mV\n", vsys_mv);
  printf("VBAT: %u mV\n", vbat_mv);

  return II2C_ERR_NONE;
}

void app_main(void) {
  int32_t err = create_i2c_bus();
  if (err != II2C_ERR_NONE) {
    printf("Failed to initialize system I2C bus: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  err = probe_i2c_device("AW9523B", CORES3_AW9523B_I2C_ADDRESS);
  if (err != II2C_ERR_NONE) {
    printf("Failed to probe AW9523B: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  err = probe_i2c_device("AXP2101", CORES3_AXP2101_I2C_ADDRESS);
  if (err != II2C_ERR_NONE) {
    printf("Failed to probe AXP2101: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  err = attach_i2c_device(CORES3_AW9523B_I2C_ADDRESS, &aw9523b);
  if (err != II2C_ERR_NONE) {
    printf("Failed to attach AW9523B: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  err = attach_i2c_device(CORES3_AXP2101_I2C_ADDRESS, &axp2101);
  if (err != II2C_ERR_NONE) {
    printf("Failed to attach AXP2101: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  puts("Applying CoreS3 startup sequence with local components...");

  err = configure_aw9523b_boost_enable(aw9523b);
  if (err != II2C_ERR_NONE) {
    printf("Failed to assert AW9523B BOOST_EN: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  err = apply_cores3_axp2101_startup(axp2101);
  if (err != II2C_ERR_NONE) {
    printf("Failed to apply CoreS3 AXP2101 startup settings: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  err = print_axp2101_summary(axp2101);
  if (err != II2C_ERR_NONE) {
    printf("Failed to read post-startup AXP2101 summary: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  puts("CoreS3 startup sequence complete.");

  err = aw9523b_port_dir_set(aw9523b,
                             CORES3_AW9523B_LCD_RST_PORT,
                             CORES3_AW9523B_LCD_RST_PIN,
                             AW9523B_PORT_DIRECTION_OUTPUT);
  if (err != II2C_ERR_NONE) {
    printf("Failed to set direction of the LCD reset pin: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  err = aw9523b_level_set(aw9523b, CORES3_AW9523B_LCD_RST_PORT, CORES3_AW9523B_LCD_RST_PIN, 1);
  if (err != II2C_ERR_NONE) {
    printf("Failed to set LCD RST logic to high: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  err = configure_lcd_dc_gpio();
  if (err != IGPIO_ERR_NONE) {
    printf("Failed to configure LCD DC GPIO: %s\n", igpio_err_to_name(err));
    release_handles();
    return;
  }

  ispi_master_bus_config_t spi_bus_cfg = {0};
  ispi_get_default_master_bus_config(&spi_bus_cfg);
  spi_bus_cfg.host = ISPI_HOST_SPI2;
  spi_bus_cfg.mosi_io_num = LCD_SPI_MOSI;
  spi_bus_cfg.miso_io_num = -1;
  spi_bus_cfg.sclk_io_num = LCD_SPI_SCK;
  spi_bus_cfg.max_transfer_sz = LCD_SPI_MAX_TRANSFER_BYTES;

  err = ispi_new_master_bus(&spi_bus_cfg, &lcd_spi);
  if (err != ISPI_ERR_NONE) {
    printf("Failed to create new SPI master bus: %s\n", ispi_err_to_name(err));
    release_handles();
    return;
  }

  ispi_device_config_t lcd_dev_cfg = {0};
  ispi_get_default_device_config(&lcd_dev_cfg);
  lcd_dev_cfg.cs_io_num = LCD_SPI_CS;
  lcd_dev_cfg.clock_speed_hz = 40000000;
  lcd_dev_cfg.mode = 0;

  err = ispi_new_device(lcd_spi, &lcd_dev_cfg, &lcd);
  if (err != ISPI_ERR_NONE) {
    printf("Failed to create SPI device: %s\n", ispi_err_to_name(err));
    release_handles();
    return;
  }

  err = lcd_hard_reset();
  if (err != II2C_ERR_NONE) {
    printf("Failed to hard-reset LCD: %ld\n", (long)err);
    release_handles();
    return;
  }

  ili9342_t display = {
      .transport_data_write = lcd_write_data,
      .transport_command_write = lcd_write_command,
      .delay_fn = delay_ms,
  };

  err = ili9342_init_default(&display);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to initialize LCD controller: %ld\n", (long)err);
    release_handles();
    return;
  }

  err = ili9342_memory_access_control_set(
      &display, ILI9342_MADCTL_BGR | ILI9342_MADCTL_MX | ILI9342_MADCTL_MY);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed setting the memory access control: %ld\n", (long)err);
    release_handles();
    return;
  }

  puts("LCD panel init commands complete");

  err = ili9342_address_window_set(&display, 0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to set LCD address window: %ld\n", (long)err);
    release_handles();
    return;
  }

#if 0
  uint16_t colors[] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
  for (size_t i = 0; i < 4; i++) {
    lcd_fill_screen_rgb565(colors[i]);
    delay_ms(1000);
  }
#endif

  puts("LCD smoke test complete");

  const char *msg = "jet me up face & at";
  uint16_t background_color = rgb888_to_rgb565(0, 0, 0);

  bmf_font_view_t font_view;
  bmf_font_view_init(&font_view);
  bmf_status_t bmf_ret =
      bmf_font_view_load_bytes(&font_view, open_sans_regular_32, open_sans_regular_32_len);
  if (bmf_ret != BMF_STATUS_OK) {
    printf("Failed to load font: %d\n", (int)bmf_ret);
    release_handles();
    return;
  }

  int64_t fill_start = esp_timer_get_time();
  err = lcd_fill_screen(&display, background_color);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to fill the LCD background: %ld\n", (long)err);
    release_handles();
    return;
  }
  int64_t fill_dur = esp_timer_get_time() - fill_start;
  printf("Fill duration: %lld ms\n", fill_dur / 1000);
  int64_t start = esp_timer_get_time();
  uint16_t foreground_color = 0xFFFF;
  lcd_render_stats_t render_stats = {0};
  err = lcd_render_c_str_direct(LCD_WIDTH,
                                LCD_HEIGHT,
                                msg,
                                2,
                                33,
                                background_color,
                                foreground_color,
                                &font_view,
                                &display,
                                &render_stats);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to render string directly to the LCD: %ld\n", (long)err);
    release_handles();
    return;
  }

  int64_t dur = esp_timer_get_time() - start;
  printf("Render duration: %lld ms\n", dur / 1000);
  printf(
      "Render breakdown: prepare=%lld us, stream=%lld us, visible_glyphs=%u, rows=%u, "
      "row_bytes=%u\n",
      render_stats.prepare_us,
      render_stats.stream_us,
      (unsigned)render_stats.visible_glyph_count,
      (unsigned)render_stats.row_count,
      (unsigned)render_stats.row_bytes);

#if 0
  const uint16_t RECT_X = 40;
  const uint16_t RECT_Y = 30;
  const uint16_t RECT_W = 96;
  const uint16_t RECT_H = 64;

  memset(LCD_BUFFER, 0xFF, lcd_buffer_size);

  for (uint16_t y = 0; y < RECT_H; ++y) {
    uint16_t draw_y = RECT_Y + y;
    if (draw_y >= LCD_HEIGHT) {
      continue;
    }

    for (uint16_t x = 0; x < RECT_W; ++x) {
      uint16_t draw_x = RECT_X + x;
      if (draw_x >= LCD_WIDTH) {
        continue;
      }

      int32_t err =
          lcd_buffer_set_pixel_rgb565(LCD_BUFFER, LCD_WIDTH, LCD_HEIGHT, draw_x, draw_y, 0xF800);
      if (err != ISPI_ERR_NONE) {
        printf("Failed setting pixel value: %ld\n", (long)err);
        release_handles();
        return;
      }
    }
  }

  err = lcd_write_buffer_chunked(&display, LCD_BUFFER, lcd_buffer_size);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed writing buffer: %ld\n", (long)err);
    release_handles();
    return;
  }
#endif
}
