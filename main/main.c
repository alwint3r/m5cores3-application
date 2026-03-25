#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <aw9523b/aw9523b.h>
#include <axp2101/axp2101.h>
#include <ft6x36/ft6x36.h>
#include <ii2c/ii2c.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_system.h>

#include "cores3_board.h"
#include "cores3_io_extender.h"
#include "cores3_power_mgmt.h"
#include "cores3_touch.h"
#include "display/cores3_display.h"
#include "graphics/bmf_reader.h"
#include "graphics/display_surface.h"
#include "graphics/fonts/open_sans_regular_16_4bpp.h"
#include "graphics/fonts/open_sans_regular_32_4bpp.h"
#include "graphics/text_renderer.h"

static aw9523b_t aw9523b_expander = {0};
static axp2101_t axp2101_pmic = {0};
static ft6x36_t ft6336 = {0};
static cores3_display_t board_display = {0};
static display_surface_t app_surface = {0};
static TaskHandle_t main_task_handle = NULL;

static cores3_board_t cores3;

static void release_handles(void) {
  display_surface_deinit(&app_surface);
  cores3_display_deinit(&board_display);
  cores3_touch_deinit(&ft6336);
  cores3_io_extender_deinit(&aw9523b_expander);
}

static int32_t axp2101_i2c_write(void *context,
                                 const uint8_t *write_buffer,
                                 size_t write_size) {
  ii2c_device_handle_t device = (ii2c_device_handle_t)context;
  if (device == NULL) {
    return II2C_ERR_INVALID_ARG;
  }

  return ii2c_master_transmit(device, write_buffer, write_size);
}

static int32_t axp2101_i2c_write_read(void *context,
                                      const uint8_t *write_buffer,
                                      size_t write_size,
                                      uint8_t *read_buffer,
                                      size_t *read_size,
                                      size_t read_capacity) {
  ii2c_device_handle_t device = (ii2c_device_handle_t)context;
  if (device == NULL || !read_buffer || !read_size) {
    return II2C_ERR_INVALID_ARG;
  }

  int32_t err = ii2c_master_transmit_receive(
      device, write_buffer, write_size, read_buffer, read_capacity);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  *read_size = read_capacity;
  return II2C_ERR_NONE;
}

static int32_t draw_rounded_button(display_surface_t *surface,
                                   bmf_font_view_t *font,
                                   const graphics_rect_t *box,
                                   const char *label,
                                   uint16_t fill_color,
                                   uint16_t text_color) {
  int32_t err = graphics_fill_round_rect_r6(surface, box, fill_color);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  int16_t label_y = graphics_text_first_baseline_y(font, box);

  return graphics_draw_text_bounded(surface,
                                    font,
                                    label,
                                    (int16_t)(box->x0 + 12),
                                    label_y,
                                    box,
                                    text_color,
                                    fill_color,
                                    NULL,
                                    NULL);
}

void app_main(void) {
  main_task_handle = xTaskGetCurrentTaskHandle();

  int32_t err = cores3_board_init(&cores3);
  if (err != 0) {
    printf("Failed to initialize system I2C bus: %ld\n", (long)err);
    return;
  }

  err = cores3_io_extender_init(cores3.i2c_aw9523b, &aw9523b_expander);
  if (err != 0) {
    printf("Failed to initialize AW9523B: %s\n", cores3_io_extender_err_to_name(err));
    release_handles();
    return;
  }

  axp2101_pmic.transport_context = cores3.i2c_axp2101;
  axp2101_pmic.transport_write = axp2101_i2c_write;
  axp2101_pmic.transport_write_read = axp2101_i2c_write_read;

  err = cores3_power_mgmt_init(&aw9523b_expander, &axp2101_pmic);
  if (err != 0) {
    release_handles();
    return;
  }

  err = cores3_io_extender_host_interrupt_init(main_task_handle);
  if (err != 0) {
    printf("Failed to setup I/O extender interrupt: %s (%ld)\n",
           cores3_io_extender_err_to_name(err),
           (long)err);
    release_handles();
    return;
  }

  err = cores3_touch_init(cores3.i2c_ft6336, &aw9523b_expander, &ft6336);
  if (err != FT6X36_ERR_NONE) {
    printf("Failed to configure the touch screen: %s (%ld)\n",
           cores3_touch_err_to_name(err),
           (long)err);
    release_handles();
    return;
  }

  err = cores3_display_init(&board_display, &aw9523b_expander);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to initialize CoreS3 display: %ld\n", (long)err);
    release_handles();
    return;
  }

  err = display_surface_init(&app_surface,
                             cores3_display_panel(&board_display),
                             cores3_display_width(),
                             cores3_display_height(),
                             cores3_display_max_transfer_bytes());
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to initialize display surface: %ld\n", (long)err);
    release_handles();
    return;
  }

  uint16_t background_color = graphics_rgb888_to_rgb565(0, 0, 0);
  uint16_t foreground_color = 0xFFFF;

  bmf_font_view_t font_view;
  bmf_font_view_init(&font_view);
  bmf_status_t bmf_ret =
      bmf_font_view_load_bytes(&font_view, open_sans_regular_32, open_sans_regular_32_len);
  if (bmf_ret != BMF_STATUS_OK) {
    printf("Failed to load font: %d\n", (int)bmf_ret);
    release_handles();
    return;
  }

  err = graphics_fill_screen(&app_surface, background_color);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to fill the LCD background: %ld\n", (long)err);
    release_handles();
    return;
  }

  bmf_font_view_t opensans_16;
  bmf_font_view_init(&opensans_16);
  bmf_ret = bmf_font_view_load_bytes(&opensans_16, open_sans_regular_16, open_sans_regular_16_len);
  if (bmf_ret != BMF_STATUS_OK) {
    printf("Failed to load font: %d\n", (int)bmf_ret);
    release_handles();
    return;
  }

  char free_heap_str[64] = {0};
  snprintf(
      free_heap_str, sizeof(free_heap_str), "Heap: %lu B", (unsigned long)esp_get_free_heap_size());
  graphics_rect_t status_bounding = {
      .x0 = 10,
      .y0 = (int16_t)(cores3_display_height() - 30U),
      .x1 = (int16_t)(cores3_display_width() - 10U),
      .y1 = (int16_t)(cores3_display_height() - 1U),
  };

  err = graphics_fill_rect(&app_surface,
                           0U,
                           (uint16_t)status_bounding.y0,
                           (uint16_t)(cores3_display_width() - 1U),
                           (uint16_t)(cores3_display_height() - 1U),
                           0xFFFF);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to render status bar: %s (%ld)\n", ili9342_err_to_name(err), (long)err);
    release_handles();
    return;
  }

  int16_t x = status_bounding.x0;
  int16_t y = (int16_t)(graphics_text_first_baseline_y(&opensans_16, &status_bounding) + 5);
  err = graphics_draw_text_bounded(&app_surface,
                                   &opensans_16,
                                   free_heap_str,
                                   x,
                                   y,
                                   &status_bounding,
                                   0x0000,
                                   0xFFFF,
                                   NULL,
                                   NULL);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to render heap info: %s (%ld)\n", ili9342_err_to_name(err), (long)err);
    release_handles();
    return;
  }

  const char *msg2 =
      "Hello world! This text may overflow on the x axis.\nAlso, this is actually a new line! Will "
      "this overflow too? Ah great! Then, how about this?\nI'm at the new line and I'm about to "
      "overflow the Y axis too! How's that?";

  graphics_rect_t bounding = {
      .x0 = 10,
      .y0 = 64,
      .x1 = (int16_t)(cores3_display_width() - 10U),
      .y1 = (int16_t)(cores3_display_height() - 30U),
  };
  x = bounding.x0;
  y = graphics_text_first_baseline_y(&font_view, &bounding);
  for (size_t i = 0; i < strlen(msg2); i++) {
    err = graphics_draw_char_bounded(&app_surface,
                                     &font_view,
                                     msg2[i],
                                     x,
                                     y,
                                     &bounding,
                                     foreground_color,
                                     background_color,
                                     &x,
                                     &y);
    if (err != ILI9342_ERR_NONE) {
      break;
    }
  }
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to render bounded string: %s (%ld)\n", ili9342_err_to_name(err), (long)err);
    release_handles();
    return;
  }

  uint16_t buttons_width = 80;
  uint16_t buttons_height = 50;
  uint16_t buttons_x_offset = 5;
  uint16_t buttons_y_offset = 10;
  uint16_t buttons_x_margin = 10;
  graphics_rect_t button = {
      .x0 = (int16_t)buttons_x_offset,
      .y0 = (int16_t)buttons_y_offset,
      .x1 = (int16_t)(buttons_x_offset + buttons_width),
      .y1 = (int16_t)buttons_height,
  };

  err = graphics_fill_round_rect_r6(&app_surface, &button, 0x001F);
  if (err != ILI9342_ERR_NONE) {
    printf(
        "Failed to draw first button background: %s (%ld)\n", ili9342_err_to_name(err), (long)err);
    release_handles();
    return;
  }

  int16_t label_y = graphics_text_first_baseline_y(&opensans_16, &button);
  err = graphics_draw_text_bounded(&app_surface,
                                   &opensans_16,
                                   "Tap me",
                                   (int16_t)(button.x0 + 15),
                                   (int16_t)(label_y + 12),
                                   &button,
                                   0xFFFF,
                                   0x001F,
                                   NULL,
                                   NULL);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to draw first button label: %s (%ld)\n", ili9342_err_to_name(err), (long)err);
    release_handles();
    return;
  }

  buttons_x_offset += buttons_width;
  graphics_rect_t another = {
      .x0 = (int16_t)(buttons_x_offset + buttons_x_margin),
      .y0 = (int16_t)buttons_y_offset,
      .x1 = (int16_t)(buttons_x_offset + buttons_width),
      .y1 = (int16_t)buttons_height,
  };
  err = draw_rounded_button(&app_surface, &opensans_16, &another, "Also me", 0xFFFF, 0x0000);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to draw second button: %s (%ld)\n", ili9342_err_to_name(err), (long)err);
    release_handles();
    return;
  }

  buttons_x_offset += buttons_width;
  err = graphics_fill_round_rect_r6_top(&app_surface,
                                        &(const graphics_rect_t){
                                            .x0 = (int16_t)(buttons_x_offset + buttons_x_margin),
                                            .y0 = (int16_t)buttons_y_offset,
                                            .x1 = (int16_t)(buttons_x_offset + buttons_width),
                                            .y1 = (int16_t)buttons_height,
                                        },
                                        0xFF08);

  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    uint8_t input_value = 0;

    // clear touch interrupt from the I/O extender
    err = aw9523b_port_input_read(&aw9523b_expander, 1, &input_value);
    if (err != AW9523B_ERR_NONE) {
      ESP_LOGE("MAIN",
               "Failed to read AW9523B register for PORT1 input data: %s\n",
               cores3_io_extender_err_to_name(err));
      continue;
    }

    (void)input_value;
    ft6x36_touch_data_t out_touch;
    err = ft6x36_touch_data_get(&ft6336, &out_touch);
    if (err != FT6X36_ERR_NONE) {
      ESP_LOGE(
          "MAIN", "Failed to read the touch data: %s (%ld)", ft6x36_err_to_name(err), (long)err);
      continue;
    }

    for (uint8_t count = 0; count < out_touch.touch_count; count++) {
      ESP_LOGI("MAIN",
               "Touch %u coord (%d, %d), area: %u, weight: %u, event: %u",
               count,
               out_touch.points[count].x,
               out_touch.points[count].y,
               out_touch.points[count].area,
               out_touch.points[count].weight,
               (uint8_t)out_touch.points[count].event);

      uint16_t touch_x = (uint16_t)(cores3_display_width() - out_touch.points[count].x);
      uint16_t touch_y = (uint16_t)(cores3_display_height() - out_touch.points[count].y);

      switch (out_touch.points[count].event) {
        case FT6X36_TOUCH_EVENT_PRESS_DOWN:
          if (graphics_rect_contains_point(&button, touch_x, touch_y)) {
            ESP_LOGI("MAIN", "Button is touched!");
          }

          if (graphics_rect_contains_point(&another, touch_x, touch_y)) {
            ESP_LOGI("MAIN", "Another button is touched!");
          }

        default:
          break;
      }
    }
  }
}
