#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <aw9523b/aw9523b.h>
#include <axp2101/axp2101.h>
#include <ft6x36/ft6x36.h>

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
#include "graphics/bitmap_icon.h"

#define IMG2BITMAP_DECLARE_GRAPHICS_BITMAP_ICON
#include "graphics/icons/icon_reboot_4bpp.h"
#include "graphics/icons/icon_wifi_connected_4bpp.h"
#include "graphics/icons/icon_wifi_disconnected_4bpp.h"

typedef struct {
  aw9523b_t io_expander;
  axp2101_t pmic;
  ft6x36_t touch_screen;
  cores3_display_t display;
  cores3_board_t board;
} app_t;

static app_t app;

static cores3_display_t board_display = {0};
static display_surface_t app_surface = {0};
static TaskHandle_t main_task_handle = NULL;

void app_main(void) {
  main_task_handle = xTaskGetCurrentTaskHandle();

  int32_t err = cores3_board_init(&app.board);
  if (err != 0) {
    printf("Failed to initialize board: %ld\n", (long)err);
    return;
  }

  err = cores3_io_extender_init(app.board.i2c_aw9523b, &app.io_expander);
  if (err != 0) {
    printf("Failed to initialize AW9523B: %s\n", cores3_io_extender_err_to_name(err));
    return;
  }

  err = cores3_power_mgmt_init(app.board.i2c_axp2101, &app.io_expander, &app.pmic);
  if (err != 0) {
    return;
  }

  err = cores3_io_extender_host_interrupt_init(main_task_handle);
  if (err != 0) {
    printf("Failed to setup I/O extender interrupt: %s (%ld)\n",
           cores3_io_extender_err_to_name(err),
           (long)err);
    return;
  }

  err = cores3_touch_init(app.board.i2c_ft6336, &app.io_expander, &app.touch_screen);
  if (err != FT6X36_ERR_NONE) {
    printf("Failed to configure the touch screen: %s (%ld)\n",
           cores3_touch_err_to_name(err),
           (long)err);
    return;
  }

  err = cores3_display_init(&board_display, app.board.display_spi_device, &app.io_expander);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to initialize CoreS3 display: %ld\n", (long)err);
    return;
  }

  err = display_surface_init(&app_surface,
                             cores3_display_panel(&board_display),
                             cores3_display_width(),
                             cores3_display_height(),
                             cores3_display_max_transfer_bytes());
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to initialize display surface: %ld\n", (long)err);
    return;
  }

  uint16_t display_bg_color = 0xFFFF;
  uint16_t text_color = 0x0000;

  bmf_font_view_t opensans_32;
  bmf_font_view_init(&opensans_32);
  bmf_status_t bmf_ret =
      bmf_font_view_load_bytes(&opensans_32, open_sans_regular_32, open_sans_regular_32_len);
  if (bmf_ret != BMF_STATUS_OK) {
    printf("Failed to load font: %d\n", (int)bmf_ret);
    return;
  }

  bmf_font_view_t opensans_16;
  bmf_font_view_init(&opensans_16);
  bmf_ret = bmf_font_view_load_bytes(&opensans_16, open_sans_regular_16, open_sans_regular_16_len);
  if (bmf_ret != BMF_STATUS_OK) {
    printf("Failed to load font: %d\n", (int)bmf_ret);
    return;
  }

  // set background color for the screen
  err = graphics_fill_screen(&app_surface, display_bg_color);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to fill the LCD background: %ld\n", (long)err);
    return;
  }

  char free_heap_str[64] = {0};
  snprintf(free_heap_str,
           sizeof(free_heap_str),
           "Free Heap %lu B",
           (unsigned long)esp_get_free_heap_size());

  uint16_t status_bar_rect_height = 32;
  graphics_rect_t status_bar_rect = {
      .x0 = 0,
      .y0 = (int16_t)(cores3_display_height() - status_bar_rect_height - 1),
      .x1 = (int16_t)(cores3_display_width() - 1U),
      .y1 = (int16_t)(cores3_display_height() - 1U),
  };

  int16_t status_bar_left_margin = 10;
  uint16_t status_bar_rect_color = graphics_rgb888_to_rgb565(184, 224, 248);

  err = graphics_fill_rect(&app_surface,
                           status_bar_rect.x0,
                           status_bar_rect.y0,
                           status_bar_rect.x1,
                           status_bar_rect.y1,
                           status_bar_rect_color);

  if (err != ILI9342_ERR_NONE) {
    printf("Failed to render status bar: %s (%ld)\n", ili9342_err_to_name(err), (long)err);
    return;
  }

  graphics_rect_t wifi_status_rect = {
      .x0 = status_bar_rect.x1 - icon_wifi_disconnected.width - 5,
      .y0 = status_bar_rect.y0 + 5,
      .x1 = status_bar_rect.x1 - 5,
      .y1 = status_bar_rect.y1 - 5,
  };

  int16_t wifi_icon_x;
  int16_t wifi_icon_y;
  graphics_bitmap_icon_center_position(
      &icon_wifi_disconnected, &wifi_status_rect, &wifi_icon_x, &wifi_icon_y);

  err = graphics_draw_bitmap_icon(&app_surface,
                                  &icon_wifi_disconnected,
                                  wifi_icon_x,
                                  wifi_icon_y,
                                  &wifi_status_rect,
                                  0x0000,
                                  status_bar_rect_color);

  int16_t free_heap_text_x = status_bar_rect.x0 + status_bar_left_margin;
  int16_t free_heap_text_y =
      (int16_t)graphics_text_center_baseline_y(&opensans_16, &status_bar_rect);
  err = graphics_draw_text_bounded(&app_surface,
                                   &opensans_16,
                                   free_heap_str,
                                   free_heap_text_x,
                                   free_heap_text_y,
                                   &status_bar_rect,
                                   text_color,
                                   status_bar_rect_color,
                                   NULL,
                                   NULL);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to render heap info: %s (%ld)\n", ili9342_err_to_name(err), (long)err);
    return;
  }

  const char *msg2 =
      "Hello world! This text may overflow on the x axis.\nAlso, this is actually a new line! Will "
      "this overflow too? Ah great! Then, how about this?\nI'm at the new line and I'm about to "
      "overflow the Y axis too! How's that?";

  graphics_rect_t bounding = {
      .x0 = 10,
      .y0 = 36,
      .x1 = (int16_t)(cores3_display_width() - 10U),
      .y1 = (int16_t)(status_bar_rect.y0 - 10),
  };
  free_heap_text_x = bounding.x0;
  free_heap_text_y = graphics_text_first_baseline_y(&opensans_32, &bounding);
  for (size_t i = 0; i < strlen(msg2); i++) {
    err = graphics_draw_char_bounded(&app_surface,
                                     &opensans_32,
                                     msg2[i],
                                     free_heap_text_x,
                                     free_heap_text_y,
                                     &bounding,
                                     text_color,
                                     display_bg_color,
                                     &free_heap_text_x,
                                     &free_heap_text_y);
    if (err != ILI9342_ERR_NONE) {
      break;
    }
  }
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to render bounded string: %s (%ld)\n", ili9342_err_to_name(err), (long)err);
    return;
  }

  uint16_t buttons_width = 32;
  uint16_t buttons_height = 32;
  uint16_t buttons_x_offset = 5;
  uint16_t buttons_y_offset = 5;
  uint16_t buttons_x_margin = 5;
  uint16_t reboot_button_color = display_bg_color;
  graphics_rect_t button_rect = {
      .x0 = (int16_t)(cores3_display_width() - buttons_width - buttons_x_margin),
      .y0 = (int16_t)buttons_y_offset,
      .x1 = (int16_t)(cores3_display_width() - buttons_x_offset),
      .y1 = (int16_t)buttons_height,
  };

  err = graphics_fill_round_rect_r6(&app_surface, &button_rect, reboot_button_color);
  if (err != ILI9342_ERR_NONE) {
    printf(
        "Failed to draw first button background: %s (%ld)\n", ili9342_err_to_name(err), (long)err);
    return;
  }

  int16_t reboot_icon_x = 0;
  int16_t reboot_icon_y = 0;
  err = graphics_bitmap_icon_center_position(
      &icon_reboot, &button_rect, &reboot_icon_x, &reboot_icon_y);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to compute first button icon position: %s (%ld)\n",
           ili9342_err_to_name(err),
           (long)err);
    return;
  }

  err = graphics_draw_bitmap_icon(&app_surface,
                                  &icon_reboot,
                                  reboot_icon_x,
                                  reboot_icon_y,
                                  &button_rect,
                                  graphics_rgb888_to_rgb565(255, 0, 0),
                                  reboot_button_color);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to draw first button label: %s (%ld)\n", ili9342_err_to_name(err), (long)err);
    return;
  }

  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    uint8_t input_value = 0;

    // clear touch interrupt from the I/O extender
    err = aw9523b_port_input_read(&app.io_expander, 1, &input_value);
    if (err != AW9523B_ERR_NONE) {
      ESP_LOGE("MAIN",
               "Failed to read AW9523B register for PORT1 input data: %s\n",
               cores3_io_extender_err_to_name(err));
      continue;
    }

    (void)input_value;
    ft6x36_touch_data_t out_touch;
    err = ft6x36_touch_data_get(&app.touch_screen, &out_touch);
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
          if (graphics_rect_contains_point(&button_rect, touch_x, touch_y)) {
            esp_restart();
          }

        default:
          break;
      }
    }
  }
}
