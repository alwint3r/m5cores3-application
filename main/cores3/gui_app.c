#include "gui_app.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <ili9342/ili9342.h>
#include <esp_log.h>

#include "display/cores3_display.h"
#include "graphics/bitmap_icon.h"
#include "graphics/fonts/open_sans_regular_16_4bpp.h"
#include "graphics/fonts/open_sans_regular_32_4bpp.h"
#include "graphics/text_renderer.h"

#define IMG2BITMAP_DECLARE_GRAPHICS_BITMAP_ICON
#include "graphics/icons/icon_battery_full.h"
#include "graphics/icons/icon_battery_half.h"
#include "graphics/icons/icon_battery_low.h"
#include "graphics/icons/icon_battery_charging.h"
#include "graphics/icons/icon_reboot_4bpp.h"
#include "graphics/icons/icon_usb_4bpp.h"
#include "graphics/icons/icon_wifi_connected_4bpp.h"
#include "graphics/icons/icon_wifi_disconnected_4bpp.h"

static const uint16_t CORES3_GUI_BG_COLOR = 0xFFFF;
static const uint16_t CORES3_GUI_TEXT_COLOR = 0x0000;
static const uint16_t CORES3_GUI_STATUS_BG_COLOR = 0xB71F;
static const uint16_t CORES3_GUI_REBOOT_ICON_COLOR = 0xF800;
static const char *CORES3_GUI_LOG_TAG = "CORES3_GUI";
static const int16_t CORES3_GUI_STATUS_ICON_SIZE = 24;
static const int16_t CORES3_GUI_STATUS_ICON_PADDING_RIGHT = 5;
static const int16_t CORES3_GUI_STATUS_ICON_GAP = 4;

static int32_t cores3_gui_require_owner_task(const cores3_gui_app_t *gui) {
  if (gui == NULL || !gui->initialized) {
    return ILI9342_ERR_INVALID_ARG;
  }

  TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  if (gui->owner_task == NULL || current_task == NULL || current_task != gui->owner_task) {
    ESP_LOGE(CORES3_GUI_LOG_TAG, "GUI access attempted from a non-owner task");
    return ILI9342_ERR_INVALID_STATE;
  }

  return ILI9342_ERR_NONE;
}

static int16_t cores3_gui_text_center_baseline_y(const bmf_font_view_t *font,
                                                 const graphics_rect_t *bounding) {
  if (font == NULL || bounding == NULL) {
    return 0;
  }

  int32_t box_height = (int32_t)bounding->y1 - (int32_t)bounding->y0 + 1;
  int32_t line_height = font->line_height > 0U ? (int32_t)font->line_height : 1;
  int32_t line_y0 = bounding->y0;
  if (box_height > line_height) {
    line_y0 += (box_height - line_height) / 2;
  }

  int32_t baseline_y = line_y0;
  if (font->ascent > 0) {
    baseline_y += font->ascent;
  } else {
    baseline_y += line_height - 1;
  }

  if (baseline_y > bounding->y1) {
    baseline_y = bounding->y1;
  }

  return (int16_t)baseline_y;
}

static int32_t cores3_gui_load_font(bmf_font_view_t *font, const uint8_t *font_data, size_t len) {
  bmf_font_view_init(font);

  bmf_status_t bmf_ret = bmf_font_view_load_bytes(font, font_data, len);
  if (bmf_ret != BMF_STATUS_OK) {
    return (int32_t)bmf_ret;
  }

  return BMF_STATUS_OK;
}

static void cores3_gui_copy_string(char *dst, size_t dst_len, const char *src) {
  if (dst == NULL || dst_len == 0U) {
    return;
  }

  if (src == NULL) {
    dst[0] = '\0';
    return;
  }

  (void)snprintf(dst, dst_len, "%s", src);
}

static void cores3_gui_layout_init(cores3_gui_app_t *gui) {
  const uint16_t display_width = cores3_display_width();
  const uint16_t display_height = cores3_display_height();
  const uint16_t status_bar_height = 32;
  const int16_t status_icon_y0 = (int16_t)(display_height - status_bar_height +
                                           ((status_bar_height - CORES3_GUI_STATUS_ICON_SIZE) / 2));
  const int16_t status_icon_y1 = (int16_t)(status_icon_y0 + CORES3_GUI_STATUS_ICON_SIZE - 1);
  const int16_t wifi_x1 = (int16_t)(display_width - CORES3_GUI_STATUS_ICON_PADDING_RIGHT - 1);
  const int16_t wifi_x0 = (int16_t)(wifi_x1 - CORES3_GUI_STATUS_ICON_SIZE + 1);
  const int16_t power_x1 = (int16_t)(wifi_x0 - CORES3_GUI_STATUS_ICON_GAP - 1);
  const int16_t power_x0 = (int16_t)(power_x1 - CORES3_GUI_STATUS_ICON_SIZE + 1);

  gui->status_bar_rect = (graphics_rect_t){
      .x0 = 0,
      .y0 = (int16_t)(display_height - status_bar_height - 1),
      .x1 = (int16_t)(display_width - 1U),
      .y1 = (int16_t)(display_height - 1U),
  };

  gui->power_status_rect = (graphics_rect_t){
      .x0 = power_x0,
      .y0 = status_icon_y0,
      .x1 = power_x1,
      .y1 = status_icon_y1,
  };

  gui->wifi_status_rect = (graphics_rect_t){
      .x0 = wifi_x0,
      .y0 = status_icon_y0,
      .x1 = wifi_x1,
      .y1 = status_icon_y1,
  };

  gui->status_text_rect = gui->status_bar_rect;
  gui->status_text_rect.x0 += 10;
  gui->status_text_rect.x1 = (int16_t)(gui->power_status_rect.x0 - 6);

  gui->main_text_content_rect = (graphics_rect_t){
      .x0 = 10,
      .y0 = 36,
      .x1 = (int16_t)(display_width - 10U),
      .y1 = (int16_t)(gui->status_bar_rect.y0 - 10),
  };

  gui->reboot_button_rect = (graphics_rect_t){
      .x0 = (int16_t)(display_width - 32 - 5),
      .y0 = 5,
      .x1 = (int16_t)(display_width - 5),
      .y1 = 32,
  };
}

static int32_t cores3_gui_fill_rect(display_surface_t *surface,
                                    const graphics_rect_t *rect,
                                    uint16_t color) {
  return graphics_fill_rect_from_bounds(surface, rect, color);
}

static int32_t cores3_gui_render_status_wifi(cores3_gui_app_t *gui) {
  if (gui == NULL || gui->surface == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t err =
      cores3_gui_fill_rect(gui->surface, &gui->wifi_status_rect, CORES3_GUI_STATUS_BG_COLOR);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  const graphics_bitmap_icon_t *wifi_icon =
      gui->wifi_connected ? &icon_wifi_connected : &icon_wifi_disconnected;
  int16_t wifi_icon_x = 0;
  int16_t wifi_icon_y = 0;
  err = graphics_bitmap_icon_center_position(
      wifi_icon, &gui->wifi_status_rect, &wifi_icon_x, &wifi_icon_y);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = graphics_draw_bitmap_icon(gui->surface,
                                  wifi_icon,
                                  wifi_icon_x,
                                  wifi_icon_y,
                                  &gui->wifi_status_rect,
                                  CORES3_GUI_TEXT_COLOR,
                                  CORES3_GUI_STATUS_BG_COLOR);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  return ILI9342_ERR_NONE;
}

static int32_t cores3_gui_render_status_power(cores3_gui_app_t *gui) {
  if (gui == NULL || gui->surface == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t err =
      cores3_gui_fill_rect(gui->surface, &gui->power_status_rect, CORES3_GUI_STATUS_BG_COLOR);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  const graphics_bitmap_icon_t *power_icon = &icon_battery_low;
  switch (gui->power_status) {
    case CORES3_GUI_POWER_STATUS_CHARGING:
      power_icon = &icon_battery_charging;
      break;
    case CORES3_GUI_POWER_STATUS_USB_POWER:
      power_icon = &icon_usb;
      break;
    case CORES3_GUI_POWER_STATUS_BATTERY:
      if (gui->battery_percent_valid) {
        if (gui->battery_percent >= 67) {
          power_icon = &icon_battery_full;
        } else if (gui->battery_percent >= 33) {
          power_icon = &icon_battery_half;
        } else {
          power_icon = &icon_battery_low;
        }
      } else {
        power_icon = &icon_battery_half;
      }
      break;
    case CORES3_GUI_POWER_STATUS_UNKNOWN:
    default:
      power_icon = &icon_battery_low;
      break;
  }

  int16_t power_icon_x = 0;
  int16_t power_icon_y = 0;
  err = graphics_bitmap_icon_center_position(
      power_icon, &gui->power_status_rect, &power_icon_x, &power_icon_y);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  return graphics_draw_bitmap_icon(gui->surface,
                                   power_icon,
                                   power_icon_x,
                                   power_icon_y,
                                   &gui->power_status_rect,
                                   CORES3_GUI_TEXT_COLOR,
                                   CORES3_GUI_STATUS_BG_COLOR);
}

static int32_t cores3_gui_render_status_text(cores3_gui_app_t *gui) {
  if (gui == NULL || gui->surface == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t err =
      cores3_gui_fill_rect(gui->surface, &gui->status_text_rect, CORES3_GUI_STATUS_BG_COLOR);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  if (gui->status_text[0] == '\0') {
    return ILI9342_ERR_NONE;
  }

  int16_t text_y = cores3_gui_text_center_baseline_y(&gui->status_font, &gui->status_text_rect);
  return graphics_draw_text_bounded(gui->surface,
                                    &gui->status_font,
                                    gui->status_text,
                                    gui->status_text_rect.x0,
                                    text_y,
                                    &gui->status_text_rect,
                                    CORES3_GUI_TEXT_COLOR,
                                    CORES3_GUI_STATUS_BG_COLOR,
                                    NULL,
                                    NULL);
}

static int32_t cores3_gui_render_status_bar(cores3_gui_app_t *gui) {
  if (gui == NULL || gui->surface == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t err =
      cores3_gui_fill_rect(gui->surface, &gui->status_bar_rect, CORES3_GUI_STATUS_BG_COLOR);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = cores3_gui_render_status_wifi(gui);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = cores3_gui_render_status_power(gui);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  return cores3_gui_render_status_text(gui);
}

static int32_t cores3_gui_render_main_text_content(cores3_gui_app_t *gui) {
  if (gui == NULL || gui->surface == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t err = graphics_fill_rect_from_bounds(
      gui->surface, &gui->main_text_content_rect, CORES3_GUI_BG_COLOR);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  if (gui->main_text_content[0] == '\0') {
    return ILI9342_ERR_NONE;
  }

  return graphics_draw_text_bounded(
      gui->surface,
      &gui->center_font,
      gui->main_text_content,
      gui->main_text_content_rect.x0,
      graphics_text_first_baseline_y(&gui->center_font, &gui->main_text_content_rect),
      &gui->main_text_content_rect,
      CORES3_GUI_TEXT_COLOR,
      CORES3_GUI_BG_COLOR,
      NULL,
      NULL);
}

static int32_t cores3_gui_render_top_button(cores3_gui_app_t *gui) {
  if (gui == NULL || gui->surface == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t err =
      graphics_fill_round_rect_r6(gui->surface, &gui->reboot_button_rect, CORES3_GUI_BG_COLOR);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  int16_t reboot_icon_x = 0;
  int16_t reboot_icon_y = 0;
  err = graphics_bitmap_icon_center_position(
      &icon_reboot, &gui->reboot_button_rect, &reboot_icon_x, &reboot_icon_y);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  return graphics_draw_bitmap_icon(gui->surface,
                                   &icon_reboot,
                                   reboot_icon_x,
                                   reboot_icon_y,
                                   &gui->reboot_button_rect,
                                   CORES3_GUI_REBOOT_ICON_COLOR,
                                   CORES3_GUI_BG_COLOR);
}

static int32_t cores3_gui_render_full(cores3_gui_app_t *gui) {
  if (gui == NULL || gui->surface == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t err = graphics_fill_screen(gui->surface, CORES3_GUI_BG_COLOR);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = cores3_gui_render_top_button(gui);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = cores3_gui_render_main_text_content(gui);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  return cores3_gui_render_status_bar(gui);
}

int32_t cores3_gui_app_init(cores3_gui_app_t *gui, display_surface_t *surface) {
  if (gui == NULL || surface == NULL || surface->panel == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t err = display_surface_require_owner_task(surface);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  memset(gui, 0, sizeof(*gui));
  gui->surface = surface;
  gui->owner_task = xTaskGetCurrentTaskHandle();
  if (gui->owner_task == NULL || gui->owner_task != display_surface_owner_task_get(surface)) {
    memset(gui, 0, sizeof(*gui));
    return ILI9342_ERR_INVALID_STATE;
  }

  err = cores3_gui_load_font(&gui->center_font, open_sans_regular_32, open_sans_regular_32_len);
  if (err != BMF_STATUS_OK) {
    cores3_gui_app_deinit(gui);
    return err;
  }

  err = cores3_gui_load_font(&gui->status_font, open_sans_regular_16, open_sans_regular_16_len);
  if (err != BMF_STATUS_OK) {
    cores3_gui_app_deinit(gui);
    return err;
  }

  cores3_gui_layout_init(gui);
  err = cores3_gui_render_full(gui);
  if (err != ILI9342_ERR_NONE) {
    cores3_gui_app_deinit(gui);
    return err;
  }

  gui->initialized = true;
  return ILI9342_ERR_NONE;
}

void cores3_gui_app_deinit(cores3_gui_app_t *gui) {
  if (gui == NULL) {
    return;
  }

  memset(gui, 0, sizeof(*gui));
}

int32_t cores3_gui_app_set_main_text_content(cores3_gui_app_t *gui, const char *text) {
  int32_t err = cores3_gui_require_owner_task(gui);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  cores3_gui_copy_string(gui->main_text_content, sizeof(gui->main_text_content), text);
  return cores3_gui_render_main_text_content(gui);
}

int32_t cores3_gui_app_set_status_bar(cores3_gui_app_t *gui,
                                      const char *text,
                                      bool wifi_connected,
                                      cores3_gui_power_status_t power_status,
                                      uint8_t battery_percent,
                                      bool battery_percent_valid) {
  int32_t err = cores3_gui_require_owner_task(gui);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  char next_status_text[CORES3_GUI_APP_STATUS_TEXT_MAX_LEN] = {0};
  cores3_gui_copy_string(next_status_text, sizeof(next_status_text), text);

  bool text_changed = strcmp(gui->status_text, next_status_text) != 0;
  bool wifi_changed = gui->wifi_connected != wifi_connected;
  bool power_changed = gui->power_status != power_status;
  bool battery_changed = gui->battery_percent_valid != battery_percent_valid ||
                         (battery_percent_valid && gui->battery_percent != battery_percent);
  if (!text_changed && !wifi_changed && !power_changed && !battery_changed) {
    return ILI9342_ERR_NONE;
  }

  cores3_gui_copy_string(gui->status_text, sizeof(gui->status_text), next_status_text);
  gui->wifi_connected = wifi_connected;
  gui->power_status = power_status;
  gui->battery_percent = battery_percent;
  gui->battery_percent_valid = battery_percent_valid;

  err = ILI9342_ERR_NONE;
  if (wifi_changed) {
    err = cores3_gui_render_status_wifi(gui);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }
  }

  if (power_changed || battery_changed) {
    err = cores3_gui_render_status_power(gui);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }
  }

  if (text_changed) {
    err = cores3_gui_render_status_text(gui);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }
  }

  return ILI9342_ERR_NONE;
}

void cores3_gui_app_set_event_callback(cores3_gui_app_t *gui,
                                       cores3_gui_app_event_callback_t callback,
                                       void *user_ctx) {
  int32_t err = cores3_gui_require_owner_task(gui);
  if (err != ILI9342_ERR_NONE) {
    return;
  }

  gui->event_callback = callback;
  gui->event_callback_user_ctx = user_ctx;
}

int32_t cores3_gui_app_handle_touch(cores3_gui_app_t *gui,
                                    int16_t x,
                                    int16_t y,
                                    ft6x36_touch_event_t touch_event) {
  int32_t err = cores3_gui_require_owner_task(gui);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  if (touch_event != FT6X36_TOUCH_EVENT_PRESS_DOWN) {
    return ILI9342_ERR_NONE;
  }

  if (!graphics_rect_contains_point(&gui->reboot_button_rect, x, y)) {
    return ILI9342_ERR_NONE;
  }

  if (gui->event_callback != NULL) {
    gui->event_callback(CORES3_GUI_APP_EVENT_REBOOT_BUTTON_PRESSED, gui->event_callback_user_ctx);
  }

  return ILI9342_ERR_NONE;
}
