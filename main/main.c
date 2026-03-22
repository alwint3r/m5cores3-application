#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aw9523b/aw9523b.h>
#include <aw9523b/aw9523b_register.h>
#include <axp2101/axp2101.h>
#include <axp2101/axp2101_register.h>
#include <igpio/igpio.h>
#include <ii2c/ii2c.h>
#include <ili9342/ili9342.h>
#include <ispi/ispi.h>
#include <driver/gpio.h>
#include "open_sans_regular_32_4bpp.h"
#include "open_sans_regular_16_4bpp.h"
#include "bmf_reader.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <esp_log.h>

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

static const uint8_t CORES3_AW9523B_TOUCH_INT_PORT = 1;
static const uint8_t CORES3_AW9523B_TOUCH_INT_PIN = 2;
static const uint8_t CORES3_AW9523B_TOUCH_RST_PORT = 0;
static const uint8_t CORES3_AW9523B_TOUCH_RST_PIN = 0;
static const int CORES3_I2C_INT_PIN = 21;

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

typedef struct rect_area rect_area_t;
struct rect_area {
  int16_t x0;
  int16_t y0;
  int16_t x1;
  int16_t y1;
};

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

static inline void lcd_color_to_u8_array(uint8_t *dst, uint16_t color) {
  dst[0] = (uint8_t)(color >> 8);
  dst[1] = (uint8_t)(color & 0xFF);
}

static void lcd_fill_color_span(uint8_t *buffer, size_t pixel_count, uint16_t color) {
  for (size_t i = 0; i < pixel_count; i++) {
    lcd_color_to_u8_array(buffer + (i * 2U), color);
  }
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

  lcd_fill_color_span(LCD_ROW_BUFFER, row_pixels, color);

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

static bool rect_area_is_valid_for_lcd(const rect_area_t *area) {
  if (area == NULL) {
    return false;
  }

  return area->x0 >= 0 && area->y0 >= 0 && area->x1 >= area->x0 && area->y1 >= area->y0 &&
         area->x1 < LCD_WIDTH && area->y1 < LCD_HEIGHT;
}

static bool rect_area_clip_to_bounds(const rect_area_t *bounds,
                                     int32_t x0,
                                     int32_t y0,
                                     int32_t x1,
                                     int32_t y1,
                                     rect_area_t *clipped) {
  if (bounds == NULL || clipped == NULL || x1 < x0 || y1 < y0) {
    return false;
  }

  if (x0 > bounds->x1 || x1 < bounds->x0 || y0 > bounds->y1 || y1 < bounds->y0) {
    return false;
  }

  clipped->x0 = (int16_t)(x0 < bounds->x0 ? bounds->x0 : x0);
  clipped->y0 = (int16_t)(y0 < bounds->y0 ? bounds->y0 : y0);
  clipped->x1 = (int16_t)(x1 > bounds->x1 ? bounds->x1 : x1);
  clipped->y1 = (int16_t)(y1 > bounds->y1 ? bounds->y1 : y1);
  return true;
}

static void rect_area_include(rect_area_t *dst, const rect_area_t *src) {
  if (dst == NULL || src == NULL) {
    return;
  }

  if (src->x0 < dst->x0) {
    dst->x0 = src->x0;
  }
  if (src->y0 < dst->y0) {
    dst->y0 = src->y0;
  }
  if (src->x1 > dst->x1) {
    dst->x1 = src->x1;
  }
  if (src->y1 > dst->y1) {
    dst->y1 = src->y1;
  }
}

static int16_t lcd_text_first_baseline_y(const bmf_font_view_t *font, const rect_area_t *bounding) {
  int16_t first_line_y = bounding->y0;
  if (font->ascent > 0) {
    first_line_y = (int16_t)(bounding->y0 + font->ascent);
  } else if (font->line_height > 0U) {
    first_line_y = (int16_t)(bounding->y0 + (int16_t)font->line_height - 1);
  }

  if (first_line_y > bounding->y1) {
    first_line_y = bounding->y1;
  }

  return first_line_y;
}

static int32_t lcd_clear_text_line(ili9342_t *display,
                                   const bmf_font_view_t *font,
                                   const rect_area_t *bounding,
                                   int16_t baseline_y,
                                   uint16_t background_color) {
  if (display == NULL || font == NULL || bounding == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  if (!rect_area_is_valid_for_lcd(bounding)) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t line_y0 = 0;
  if (font->ascent > 0) {
    line_y0 = (int32_t)baseline_y - font->ascent;
  } else if (font->line_height > 0U) {
    line_y0 = (int32_t)baseline_y - (int32_t)font->line_height + 1;
  } else {
    line_y0 = baseline_y;
  }

  int32_t line_y1 = line_y0;
  if (font->line_height > 0U) {
    line_y1 = line_y0 + (int32_t)font->line_height - 1;
  }

  if (line_y1 < bounding->y0 || line_y0 > bounding->y1) {
    return ILI9342_ERR_NONE;
  }

  if (line_y0 < bounding->y0) {
    line_y0 = bounding->y0;
  }
  if (line_y1 > bounding->y1) {
    line_y1 = bounding->y1;
  }
  if (line_y0 > line_y1) {
    return ILI9342_ERR_NONE;
  }

  return lcd_fill_rect(display,
                       (uint16_t)bounding->x0,
                       (uint16_t)line_y0,
                       (uint16_t)bounding->x1,
                       (uint16_t)line_y1,
                       background_color);
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

  lcd_fill_color_span(buffer, buffer_len / 2U, color);

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
  lcd_color_to_u8_array(buffer + byte_index, color);
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

static int32_t lcd_glyph_stride_get(const bmf_font_view_t *font_view, int width, int *stride) {
  if (font_view == NULL || stride == NULL || width <= 0) {
    return ILI9342_ERR_INVALID_ARG;
  }

  if (font_view->bpp == BMF_BPP_MONO) {
    *stride = (width + 7) / 8;
    return ILI9342_ERR_NONE;
  }

  if (font_view->bpp == BMF_BPP_GRAY4) {
    *stride = (width + 1) / 2;
    return ILI9342_ERR_NONE;
  }

  if (font_view->bpp == BMF_BPP_GRAY8) {
    *stride = width;
    return ILI9342_ERR_NONE;
  }

  return ILI9342_ERR_INVALID_ARG;
}

static int32_t lcd_prepared_glyph_load(bmf_font_view_t *font_view,
                                       const bmf_glyph_record_t *glyph,
                                       lcd_prepared_glyph_t *prepared) {
  if (font_view == NULL || glyph == NULL || prepared == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  memset(prepared, 0, sizeof(*prepared));

  int width = 0;
  int height = 0;
  prepared->bitmap = bmf_font_view_get_glyph_bitmap(font_view, glyph, &width, &height);
  prepared->width = width;
  prepared->height = height;
  if (prepared->bitmap == NULL || width <= 0 || height <= 0) {
    return ILI9342_ERR_NONE;
  }

  return lcd_glyph_stride_get(font_view, width, &prepared->stride);
}

static void lcd_prepared_glyph_position_set(lcd_prepared_glyph_t *prepared,
                                            int draw_x,
                                            int draw_y) {
  prepared->draw_x = draw_x;
  prepared->draw_y = draw_y;
  prepared->draw_y1 = draw_y + prepared->height - 1;
}

static bool lcd_prepared_glyph_has_bitmap(const lcd_prepared_glyph_t *prepared) {
  return prepared != NULL && prepared->bitmap != NULL && prepared->width > 0 &&
         prepared->height > 0;
}

static int32_t lcd_prepared_glyph_coverage_get(const bmf_font_view_t *font_view,
                                               const lcd_prepared_glyph_t *prepared,
                                               int src_x,
                                               int src_y,
                                               uint8_t *coverage) {
  if (font_view == NULL || prepared == NULL || coverage == NULL ||
      !lcd_prepared_glyph_has_bitmap(prepared) || src_x < 0 || src_y < 0 ||
      src_x >= prepared->width || src_y >= prepared->height) {
    return ILI9342_ERR_INVALID_ARG;
  }

  if (font_view->bpp == BMF_BPP_MONO) {
    int byte_idx = src_y * prepared->stride + src_x / 8;
    int bit_idx = 7 - (src_x % 8);
    uint8_t byte = prepared->bitmap[byte_idx];
    *coverage = ((byte >> bit_idx) & 1U) != 0U ? 255U : 0U;
    return ILI9342_ERR_NONE;
  }

  if (font_view->bpp == BMF_BPP_GRAY4) {
    int byte_idx = src_y * prepared->stride + src_x / 2;
    uint8_t byte = prepared->bitmap[byte_idx];
    uint8_t gray4 = (src_x % 2 == 0) ? (byte >> 4) : (byte & 0x0F);
    *coverage = (uint8_t)((gray4 << 4) | gray4);
    return ILI9342_ERR_NONE;
  }

  if (font_view->bpp == BMF_BPP_GRAY8) {
    *coverage = prepared->bitmap[src_y * prepared->stride + src_x];
    return ILI9342_ERR_NONE;
  }

  return ILI9342_ERR_INVALID_ARG;
}

static uint16_t lcd_color_from_coverage(uint16_t background_color,
                                        uint16_t foreground_color,
                                        uint8_t coverage) {
  if (coverage == 0U) {
    return background_color;
  }

  if (coverage == 255U) {
    return foreground_color;
  }

  return blend_rgb565(background_color, foreground_color, coverage);
}

static int32_t lcd_render_prepared_glyph_row(uint8_t *row_buffer,
                                             int row_origin_x,
                                             int dst_y,
                                             const bmf_font_view_t *font_view,
                                             const lcd_prepared_glyph_t *prepared,
                                             int clip_x0,
                                             int clip_x1,
                                             uint16_t background_color,
                                             uint16_t foreground_color) {
  if (row_buffer == NULL || font_view == NULL || prepared == NULL || clip_x1 < clip_x0) {
    return ILI9342_ERR_INVALID_ARG;
  }

  if (dst_y < prepared->draw_y || dst_y > prepared->draw_y1) {
    return ILI9342_ERR_NONE;
  }

  int src_y = dst_y - prepared->draw_y;
  for (int dst_x = clip_x0; dst_x <= clip_x1; dst_x++) {
    int src_x = dst_x - prepared->draw_x;
    uint8_t coverage = 0U;
    int32_t err = lcd_prepared_glyph_coverage_get(font_view, prepared, src_x, src_y, &coverage);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }

    if (coverage == 0U) {
      continue;
    }

    size_t row_offset = (size_t)(dst_x - row_origin_x) * 2U;
    lcd_color_to_u8_array(row_buffer + row_offset,
                          lcd_color_from_coverage(background_color, foreground_color, coverage));
  }

  return ILI9342_ERR_NONE;
}

static int32_t lcd_render_prepared_glyphs(ili9342_t *display,
                                          const bmf_font_view_t *font_view,
                                          const lcd_prepared_glyph_t *prepared_glyphs,
                                          size_t prepared_glyph_count,
                                          const rect_area_t *clip,
                                          uint16_t background_color,
                                          uint16_t foreground_color) {
  if (display == NULL || font_view == NULL || prepared_glyphs == NULL || clip == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  if (!rect_area_is_valid_for_lcd(clip)) {
    return ILI9342_ERR_INVALID_ARG;
  }

  size_t row_pixels = (size_t)(clip->x1 - clip->x0 + 1);
  size_t row_bytes = row_pixels * 2U;
  if (row_bytes == 0U || row_bytes > LCD_ROW_BUFFER_BYTES) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t err = ili9342_address_window_set(
      display, (uint16_t)clip->x0, (uint16_t)clip->y0, (uint16_t)clip->x1, (uint16_t)clip->y1);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  for (int dst_y = clip->y0; dst_y <= clip->y1; dst_y++) {
    lcd_fill_color_span(LCD_ROW_BUFFER, row_pixels, background_color);

    for (size_t i = 0; i < prepared_glyph_count; i++) {
      const lcd_prepared_glyph_t *prepared = &prepared_glyphs[i];
      if (!lcd_prepared_glyph_has_bitmap(prepared) || dst_y < prepared->draw_y ||
          dst_y > prepared->draw_y1) {
        continue;
      }

      int clip_x0 = prepared->draw_x < clip->x0 ? clip->x0 : prepared->draw_x;
      int clip_x1 = prepared->draw_x + prepared->width - 1;
      if (clip_x1 > clip->x1) {
        clip_x1 = clip->x1;
      }

      if (clip_x0 > clip_x1) {
        continue;
      }

      err = lcd_render_prepared_glyph_row(LCD_ROW_BUFFER,
                                          clip->x0,
                                          dst_y,
                                          font_view,
                                          prepared,
                                          clip_x0,
                                          clip_x1,
                                          background_color,
                                          foreground_color);
      if (err != ILI9342_ERR_NONE) {
        return err;
      }
    }

    err = lcd_write_buffer_chunked(display, LCD_ROW_BUFFER, row_bytes);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }
  }

  return ILI9342_ERR_NONE;
}

static int32_t lcd_flush_prepared_glyph_run(ili9342_t *display,
                                            bmf_font_view_t *font_view,
                                            const lcd_prepared_glyph_t *prepared_glyphs,
                                            size_t prepared_glyph_count,
                                            bool has_visible_pixels,
                                            const rect_area_t *clip,
                                            uint16_t background_color,
                                            uint16_t foreground_color) {
  if (prepared_glyph_count == 0U || !has_visible_pixels) {
    return ILI9342_ERR_NONE;
  }

  return lcd_render_prepared_glyphs(display,
                                    font_view,
                                    prepared_glyphs,
                                    prepared_glyph_count,
                                    clip,
                                    background_color,
                                    foreground_color);
}

static int32_t lcd_wrap_pen_to_next_line(ili9342_t *display,
                                         const bmf_font_view_t *font,
                                         const rect_area_t *bounding,
                                         int16_t first_line_y,
                                         uint16_t background_color,
                                         int16_t *pen_x,
                                         int16_t *pen_y) {
  if (display == NULL || font == NULL || bounding == NULL || pen_x == NULL || pen_y == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  *pen_x = bounding->x0;
  *pen_y = (int16_t)(*pen_y + (int16_t)font->line_height);
  if (*pen_y > bounding->y1) {
    *pen_y = first_line_y;
  }

  return lcd_clear_text_line(display, font, bounding, *pen_y, background_color);
}

static int32_t lcd_reset_pen_to_first_line(ili9342_t *display,
                                           const bmf_font_view_t *font,
                                           const rect_area_t *bounding,
                                           int16_t first_line_y,
                                           uint16_t background_color,
                                           int16_t *pen_x,
                                           int16_t *pen_y) {
  if (display == NULL || font == NULL || bounding == NULL || pen_x == NULL || pen_y == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  *pen_x = bounding->x0;
  *pen_y = first_line_y;

  return lcd_clear_text_line(display, font, bounding, *pen_y, background_color);
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
  rect_area_t screen_bounds = {
      .x0 = 0,
      .y0 = 0,
      .x1 = (int16_t)(screen_width - 1U),
      .y1 = (int16_t)(screen_height - 1U),
  };

  for (size_t i = 0; i < text_length; i++) {
    bmf_glyph_record_t glyph;

    uint32_t codepoint = (uint8_t)text[i];
    bmf_status_t glyph_status = bmf_font_view_find_glyph_binary(font_view, codepoint, &glyph, NULL);
    if (glyph_status != BMF_STATUS_OK) {
      return ISPI_ERR_FAIL;
    }

    lcd_prepared_glyph_t glyph_state;
    int32_t err = lcd_prepared_glyph_load(font_view, &glyph, &glyph_state);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }

    if (!lcd_prepared_glyph_has_bitmap(&glyph_state)) {
      pen_x += glyph.x_advance;
      continue;
    }

    int draw_x = pen_x + glyph.x_offset;
    int draw_y = baseline_y + glyph.y_offset;
    lcd_prepared_glyph_position_set(&glyph_state, draw_x, draw_y);

    rect_area_t clipped;
    if (!rect_area_clip_to_bounds(&screen_bounds,
                                  glyph_state.draw_x,
                                  glyph_state.draw_y,
                                  glyph_state.draw_x + glyph_state.width - 1,
                                  glyph_state.draw_y1,
                                  &clipped)) {
      pen_x += glyph.x_advance;
      continue;
    }

    lcd_prepared_glyph_t *prepared = &prepared_glyphs[prepared_count++];
    *prepared = glyph_state;

    if (!has_visible_pixels) {
      *text_clip_x0 = clipped.x0;
      *text_clip_y0 = clipped.y0;
      *text_clip_x1 = clipped.x1;
      *text_clip_y1 = clipped.y1;
      has_visible_pixels = true;
    } else {
      if (clipped.x0 < *text_clip_x0) {
        *text_clip_x0 = clipped.x0;
      }
      if (clipped.y0 < *text_clip_y0) {
        *text_clip_y0 = clipped.y0;
      }
      if (clipped.x1 > *text_clip_x1) {
        *text_clip_x1 = clipped.x1;
      }
      if (clipped.y1 > *text_clip_y1) {
        *text_clip_y1 = clipped.y1;
      }
    }

    pen_x += glyph.x_advance;
  }

  *visible_glyph_count = prepared_count;
  return ILI9342_ERR_NONE;
}

static __attribute__((unused)) int32_t lcd_render_c_str_direct(uint16_t screen_width,
                                                               uint16_t screen_height,
                                                               const char *text,
                                                               uint16_t x,
                                                               uint16_t y,
                                                               uint16_t background_color,
                                                               uint16_t foreground_color,
                                                               bmf_font_view_t *font_view,
                                                               ili9342_t *display) {
  if (display == NULL || text == NULL || font_view == NULL || screen_width == 0U ||
      screen_height == 0U || screen_width > LCD_WIDTH) {
    return ILI9342_ERR_INVALID_ARG;
  }

  size_t text_length = strlen(text);
  if (text_length == 0U) {
    return ISPI_ERR_NONE;
  }

  lcd_prepared_glyph_t *prepared_glyphs = calloc(text_length, sizeof(*prepared_glyphs));
  if (prepared_glyphs == NULL) {
    return ILI9342_ERR_NO_MEM;
  }

  int text_clip_x0 = 0;
  int text_clip_y0 = 0;
  int text_clip_x1 = 0;
  int text_clip_y1 = 0;
  size_t visible_glyph_count = 0U;
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
  if (err != ILI9342_ERR_NONE) {
    free(prepared_glyphs);
    return err;
  }

  if (visible_glyph_count == 0U) {
    free(prepared_glyphs);
    return ISPI_ERR_NONE;
  }

  rect_area_t text_clip = {
      .x0 = (int16_t)text_clip_x0,
      .y0 = (int16_t)text_clip_y0,
      .x1 = (int16_t)text_clip_x1,
      .y1 = (int16_t)text_clip_y1,
  };
  err = lcd_render_prepared_glyphs(display,
                                   font_view,
                                   prepared_glyphs,
                                   visible_glyph_count,
                                   &text_clip,
                                   background_color,
                                   foreground_color);
  if (err != ILI9342_ERR_NONE) {
    free(prepared_glyphs);
    return err;
  }

  free(prepared_glyphs);
  return ISPI_ERR_NONE;
}

static int32_t lcd_render_char_array_bounded(ili9342_t *display,
                                             bmf_font_view_t *font,
                                             const char *text,
                                             size_t text_length,
                                             int16_t x,
                                             int16_t y,
                                             const rect_area_t *bounding,
                                             uint16_t foreground_color,
                                             uint16_t background_color,
                                             int16_t *next_x,
                                             int16_t *next_y) {
  if (display == NULL || font == NULL || text == NULL || bounding == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  if (!rect_area_is_valid_for_lcd(bounding)) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int16_t first_line_y = lcd_text_first_baseline_y(font, bounding);

  int16_t pen_x = x;
  int16_t pen_y = y;
  if (pen_x < bounding->x0 || pen_x > bounding->x1) {
    pen_x = bounding->x0;
  }
  if (pen_y < first_line_y || pen_y > bounding->y1) {
    pen_y = first_line_y;
  }

  if (text_length == 0U) {
    if (next_x != NULL) {
      *next_x = pen_x;
    }
    if (next_y != NULL) {
      *next_y = pen_y;
    }

    return ILI9342_ERR_NONE;
  }

  lcd_prepared_glyph_t *prepared_glyphs = calloc(text_length, sizeof(*prepared_glyphs));
  if (prepared_glyphs == NULL) {
    return ILI9342_ERR_NO_MEM;
  }

  size_t prepared_count = 0U;
  bool has_visible_pixels = false;
  rect_area_t line_clip = {0};

  for (size_t i = 0; i < text_length; i++) {
    char c = text[i];

    if (c == '\n') {
      int32_t err = lcd_flush_prepared_glyph_run(display,
                                                 font,
                                                 prepared_glyphs,
                                                 prepared_count,
                                                 has_visible_pixels,
                                                 &line_clip,
                                                 background_color,
                                                 foreground_color);
      if (err != ILI9342_ERR_NONE) {
        free(prepared_glyphs);
        return err;
      }

      prepared_count = 0U;
      has_visible_pixels = false;

      err = lcd_wrap_pen_to_next_line(
          display, font, bounding, first_line_y, background_color, &pen_x, &pen_y);
      if (err != ILI9342_ERR_NONE) {
        free(prepared_glyphs);
        return err;
      }

      continue;
    }

    uint32_t codepoint = (uint8_t)c;
    bmf_glyph_record_t glyph;
    bmf_status_t bmf_ret = bmf_font_view_find_glyph_binary(font, codepoint, &glyph, NULL);
    if (bmf_ret != BMF_STATUS_OK) {
      free(prepared_glyphs);
      return ILI9342_ERR_INVALID_ARG;
    }

    lcd_prepared_glyph_t prepared;
    int32_t err = lcd_prepared_glyph_load(font, &glyph, &prepared);
    if (err != ILI9342_ERR_NONE) {
      free(prepared_glyphs);
      return err;
    }

    if (lcd_prepared_glyph_has_bitmap(&prepared)) {
      lcd_prepared_glyph_position_set(
          &prepared, (int32_t)pen_x + glyph.x_offset, (int32_t)pen_y + glyph.y_offset);

      if (prepared.draw_x + prepared.width - 1 > bounding->x1 && pen_x > bounding->x0) {
        err = lcd_flush_prepared_glyph_run(display,
                                           font,
                                           prepared_glyphs,
                                           prepared_count,
                                           has_visible_pixels,
                                           &line_clip,
                                           background_color,
                                           foreground_color);
        if (err != ILI9342_ERR_NONE) {
          free(prepared_glyphs);
          return err;
        }

        prepared_count = 0U;
        has_visible_pixels = false;

        err = lcd_wrap_pen_to_next_line(
            display, font, bounding, first_line_y, background_color, &pen_x, &pen_y);
        if (err != ILI9342_ERR_NONE) {
          free(prepared_glyphs);
          return err;
        }

        lcd_prepared_glyph_position_set(
            &prepared, (int32_t)pen_x + glyph.x_offset, (int32_t)pen_y + glyph.y_offset);
      }

      if (prepared.draw_y1 > bounding->y1) {
        err = lcd_flush_prepared_glyph_run(display,
                                           font,
                                           prepared_glyphs,
                                           prepared_count,
                                           has_visible_pixels,
                                           &line_clip,
                                           background_color,
                                           foreground_color);
        if (err != ILI9342_ERR_NONE) {
          free(prepared_glyphs);
          return err;
        }

        prepared_count = 0U;
        has_visible_pixels = false;

        err = lcd_reset_pen_to_first_line(
            display, font, bounding, first_line_y, background_color, &pen_x, &pen_y);
        if (err != ILI9342_ERR_NONE) {
          free(prepared_glyphs);
          return err;
        }

        lcd_prepared_glyph_position_set(
            &prepared, (int32_t)pen_x + glyph.x_offset, (int32_t)pen_y + glyph.y_offset);
      }

      rect_area_t glyph_clip;
      if (rect_area_clip_to_bounds(bounding,
                                   prepared.draw_x,
                                   prepared.draw_y,
                                   prepared.draw_x + prepared.width - 1,
                                   prepared.draw_y1,
                                   &glyph_clip)) {
        prepared_glyphs[prepared_count++] = prepared;

        if (!has_visible_pixels) {
          line_clip = glyph_clip;
          has_visible_pixels = true;
        } else {
          rect_area_include(&line_clip, &glyph_clip);
        }
      }
    }

    pen_x = (int16_t)((int32_t)pen_x + glyph.x_advance);
    if (pen_x > bounding->x1) {
      err = lcd_flush_prepared_glyph_run(display,
                                         font,
                                         prepared_glyphs,
                                         prepared_count,
                                         has_visible_pixels,
                                         &line_clip,
                                         background_color,
                                         foreground_color);
      if (err != ILI9342_ERR_NONE) {
        free(prepared_glyphs);
        return err;
      }

      prepared_count = 0U;
      has_visible_pixels = false;

      err = lcd_wrap_pen_to_next_line(
          display, font, bounding, first_line_y, background_color, &pen_x, &pen_y);
      if (err != ILI9342_ERR_NONE) {
        free(prepared_glyphs);
        return err;
      }
    }
  }

  int32_t err = lcd_flush_prepared_glyph_run(display,
                                             font,
                                             prepared_glyphs,
                                             prepared_count,
                                             has_visible_pixels,
                                             &line_clip,
                                             background_color,
                                             foreground_color);
  free(prepared_glyphs);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  if (next_x != NULL) {
    *next_x = pen_x;
  }
  if (next_y != NULL) {
    *next_y = pen_y;
  }
  return ILI9342_ERR_NONE;
}

static int32_t lcd_render_c_str_bounded(ili9342_t *display,
                                        bmf_font_view_t *font,
                                        const char *text,
                                        int16_t x,
                                        int16_t y,
                                        const rect_area_t *bounding,
                                        uint16_t foreground_color,
                                        uint16_t background_color,
                                        int16_t *next_x,
                                        int16_t *next_y) {
  if (text == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  return lcd_render_char_array_bounded(display,
                                       font,
                                       text,
                                       strlen(text),
                                       x,
                                       y,
                                       bounding,
                                       foreground_color,
                                       background_color,
                                       next_x,
                                       next_y);
}

static __attribute__((unused)) int32_t lcd_putc_wrap(ili9342_t *display,
                                                     bmf_font_view_t *font,
                                                     char c,
                                                     int16_t x,
                                                     int16_t y,
                                                     const rect_area_t *bounding,
                                                     uint16_t foreground_color,
                                                     uint16_t background_color,
                                                     int16_t *next_x,
                                                     int16_t *next_y) {
  return lcd_render_char_array_bounded(
      display, font, &c, 1U, x, y, bounding, foreground_color, background_color, next_x, next_y);
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

static TaskHandle_t main_task_handle;

static void IRAM_ATTR host_gpio_intr_handler(void *arg) {
  if ((int32_t)arg == CORES3_I2C_INT_PIN) {
    BaseType_t higher_priority_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(main_task_handle, &higher_priority_task_woken);
    if (higher_priority_task_woken == pdTRUE) {
      portYIELD_FROM_ISR();
    }
  }
}

static int32_t configure_touch_screen(void) {
  int32_t err = aw9523b_port_dir_set(aw9523b,
                                     CORES3_AW9523B_TOUCH_INT_PORT,
                                     CORES3_AW9523B_TOUCH_INT_PIN,
                                     AW9523B_PORT_DIRECTION_INPUT);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = aw9523b_port_dir_set(aw9523b,
                             CORES3_AW9523B_TOUCH_RST_PORT,
                             CORES3_AW9523B_TOUCH_RST_PIN,
                             AW9523B_PORT_DIRECTION_OUTPUT);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = aw9523b_level_set(aw9523b, CORES3_AW9523B_TOUCH_RST_PORT, CORES3_AW9523B_TOUCH_RST_PIN, 1);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint8_t pin_level;
  err = aw9523b_level_get(
      aw9523b, CORES3_AW9523B_TOUCH_INT_PORT, CORES3_AW9523B_TOUCH_INT_PIN, &pin_level);
  if (err != II2C_ERR_NONE) {
    return err;
  }
  (void)pin_level;

  err = aw9523b_interrupt_set(
      aw9523b, CORES3_AW9523B_TOUCH_INT_PORT, CORES3_AW9523B_TOUCH_INT_PIN, true);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  return II2C_ERR_NONE;
}

static int32_t configure_io_extender_interrupt(void) {
  igpio_config_t config;
  igpio_get_default_config(&config);
  config.intr_type = IGPIO_INTR_NEGEDGE;
  config.io_num = CORES3_I2C_INT_PIN;
  config.mode = IGPIO_MODE_INPUT;
  config.pull_mode = IGPIO_PULL_UP;

  int32_t err = igpio_configure(&config);
  if (err != IGPIO_ERR_NONE) {
    return err;
  }

  err = gpio_install_isr_service(0);
  if (err != ESP_OK) {
    return err;
  }

  err = gpio_isr_handler_add(
      (gpio_num_t)CORES3_I2C_INT_PIN, host_gpio_intr_handler, (void *)CORES3_I2C_INT_PIN);
  if (err != ESP_OK) {
    return err;
  }

  return IGPIO_ERR_NONE;
}

void app_main(void) {
  main_task_handle = xTaskGetCurrentTaskHandle();

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

  err = configure_io_extender_interrupt();
  if (err != 0) {
    printf("Failed to setup Setup I/O Extender: %ld\n", (long)err);
    release_handles();
    return;
  }

  err = configure_touch_screen();
  if (err != 0) {
    printf("Failed to configure the touch screen: %s (%ld)\n", ii2c_err_to_name(err), (long)err);
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
  err = lcd_fill_screen(&display, background_color);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to fill the LCD background: %ld\n", (long)err);
    release_handles();
    return;
  }

  bmf_font_view_t opensans_16;
  bmf_ret = bmf_font_view_load_bytes(&opensans_16, open_sans_regular_16, open_sans_regular_16_len);
  if (bmf_ret != BMF_STATUS_OK) {
    printf("Failed to load font: %d\n", (int)bmf_ret);
    release_handles();
    return;
  }

  uint16_t foreground_color = 0xFFFF;
  int16_t x, y;

  char free_heap_str[64] = {0};
  snprintf(
      free_heap_str, sizeof(free_heap_str), "Heap: %lu B", (unsigned long)esp_get_free_heap_size());
  rect_area_t status_bounding = {
      .x0 = 10,
      .y0 = LCD_HEIGHT - 30,
      .x1 = LCD_WIDTH - 10,
      .y1 = LCD_HEIGHT - 1,
  };

  err = lcd_fill_rect(&display, 0, status_bounding.y0, LCD_WIDTH - 1, LCD_HEIGHT - 1, 0xFFFF);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to render status bar: %s (%ld)\n", ili9342_err_to_name(err), (long)err);
    release_handles();
  }
  x = status_bounding.x0;
  y = lcd_text_first_baseline_y(&opensans_16, &status_bounding) + 5;
  err = lcd_render_c_str_bounded(
      &display, &opensans_16, free_heap_str, x, y, &status_bounding, 0x0000, 0xFFFF, NULL, NULL);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to render heap info: %s (%ld)\n", ili9342_err_to_name(err), (long)err);
    release_handles();
  }

  const char *msg2 =
      "Hello world! This text may overflow on the x axis.\nAlso, this is actually a new line! Will "
      "this overflow too? Ah great! Then, how about this?\nI'm at the new line and I'm about to "
      "overflow the Y axis too! How's that?";

  rect_area_t bounding = {
      .x0 = 10,
      .y0 = 64,
      .x1 = LCD_WIDTH - 10,
      .y1 = LCD_HEIGHT - 30,
  };
  x = bounding.x0;
  y = lcd_text_first_baseline_y(&font_view, &bounding);
#if 0
  err = lcd_render_c_str_bounded(
      &display, &font_view, msg2, x, y, &bounding, foreground_color, background_color, &x, &y);
#else
  for (size_t i = 0; i < strlen(msg2); i++) {
    err = lcd_putc_wrap(
        &display, &font_view, msg2[i], x, y, &bounding, foreground_color, background_color, &x, &y);
    if (err != ILI9342_ERR_NONE) {
      break;
    }
  }
#endif
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to render bounded string: %s (%ld)\n", ili9342_err_to_name(err), (long)err);
    release_handles();
    return;
  }

  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    uint8_t input_value = 0;

    err = aw9523b_port_input_read(aw9523b, CORES3_AW9523B_TOUCH_INT_PORT, &input_value);
    if (err != II2C_ERR_NONE) {
      printf("Failed to read AW9523B register for PORT1 input data: %s\n", ii2c_err_to_name(err));
      continue;
    }

    (void)input_value;
    ESP_LOGI("TOUCH", "Touch INT is propagated!");
  }
}
