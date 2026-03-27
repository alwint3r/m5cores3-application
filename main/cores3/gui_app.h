#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <ft6x36/ft6x36.h>

#include "graphics/bmf_reader.h"
#include "graphics/display_surface.h"

#define CORES3_GUI_APP_MAIN_TEXT_CONTENT_MAX_LEN ((size_t)512U)
#define CORES3_GUI_APP_STATUS_TEXT_MAX_LEN ((size_t)64U)

typedef enum {
  CORES3_GUI_APP_EVENT_REBOOT_BUTTON_PRESSED = 0,
} cores3_gui_app_event_t;

typedef enum {
  CORES3_GUI_POWER_STATUS_UNKNOWN = 0,
  CORES3_GUI_POWER_STATUS_CHARGING,
  CORES3_GUI_POWER_STATUS_USB_POWER,
  CORES3_GUI_POWER_STATUS_BATTERY,
} cores3_gui_power_status_t;

typedef void (*cores3_gui_app_event_callback_t)(cores3_gui_app_event_t event, void *user_ctx);

typedef struct {
  display_surface_t *surface;
  TaskHandle_t owner_task;
  bmf_font_view_t center_font;
  bmf_font_view_t status_font;
  graphics_rect_t main_text_content_rect;
  graphics_rect_t status_bar_rect;
  graphics_rect_t status_text_rect;
  graphics_rect_t power_status_rect;
  graphics_rect_t wifi_status_rect;
  graphics_rect_t reboot_button_rect;
  cores3_gui_power_status_t power_status;
  bool wifi_connected;
  char main_text_content[CORES3_GUI_APP_MAIN_TEXT_CONTENT_MAX_LEN];
  char status_text[CORES3_GUI_APP_STATUS_TEXT_MAX_LEN];
  cores3_gui_app_event_callback_t event_callback;
  void *event_callback_user_ctx;
  bool initialized;
} cores3_gui_app_t;

int32_t cores3_gui_app_init(cores3_gui_app_t *gui, display_surface_t *surface);
void cores3_gui_app_deinit(cores3_gui_app_t *gui);
int32_t cores3_gui_app_set_main_text_content(cores3_gui_app_t *gui, const char *text);
int32_t cores3_gui_app_set_status_bar(cores3_gui_app_t *gui,
                                      const char *text,
                                      bool wifi_connected,
                                      cores3_gui_power_status_t power_status);
void cores3_gui_app_set_event_callback(cores3_gui_app_t *gui,
                                       cores3_gui_app_event_callback_t callback,
                                       void *user_ctx);
int32_t cores3_gui_app_handle_touch(cores3_gui_app_t *gui,
                                    int16_t x,
                                    int16_t y,
                                    ft6x36_touch_event_t touch_event);
