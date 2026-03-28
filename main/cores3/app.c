#include "app.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <aw9523b/aw9523b.h>
#include <axp2101/axp2101.h>
#include <ft6x36/ft6x36.h>

#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "cores3_board.h"
#include "cores3_io_extender.h"
#include "cores3_power_mgmt.h"
#include "cores3_touch.h"
#include "display/cores3_display.h"
#include "graphics/display_surface.h"
#include "gui_app.h"

static const char *CORES3_APP_LOG_TAG = "CORES3_APP";
static const char *CORES3_APP_DEFAULT_MAIN_TEXT_CONTENT =
    "Waiting for events...";
static const TickType_t CORES3_APP_STATUS_REFRESH_INTERVAL_TICKS =
    pdMS_TO_TICKS(1000);
static const TickType_t CORES3_APP_DISPLAY_DIM_TIMEOUT_TICKS =
    pdMS_TO_TICKS(15000);

static cores3_gui_power_status_t
cores3_app_power_status_to_gui(cores3_app_power_status_t status) {
  switch (status) {
  case CORES3_APP_POWER_STATUS_CHARGING:
    return CORES3_GUI_POWER_STATUS_CHARGING;
  case CORES3_APP_POWER_STATUS_USB_POWER:
    return CORES3_GUI_POWER_STATUS_USB_POWER;
  case CORES3_APP_POWER_STATUS_BATTERY:
    return CORES3_GUI_POWER_STATUS_BATTERY;
  case CORES3_APP_POWER_STATUS_UNKNOWN:
  default:
    return CORES3_GUI_POWER_STATUS_UNKNOWN;
  }
}

static cores3_app_power_status_t cores3_app_power_status_from_power_mgmt(
    cores3_power_mgmt_power_status_t status) {
  switch (status) {
  case CORES3_POWER_MGMT_POWER_STATUS_CHARGING:
    return CORES3_APP_POWER_STATUS_CHARGING;
  case CORES3_POWER_MGMT_POWER_STATUS_USB_POWER:
    return CORES3_APP_POWER_STATUS_USB_POWER;
  case CORES3_POWER_MGMT_POWER_STATUS_BATTERY:
    return CORES3_APP_POWER_STATUS_BATTERY;
  case CORES3_POWER_MGMT_POWER_STATUS_UNKNOWN:
  default:
    return CORES3_APP_POWER_STATUS_UNKNOWN;
  }
}

const char *
cores3_app_power_status_to_string(cores3_app_power_status_t status) {
  switch (status) {
  case CORES3_APP_POWER_STATUS_CHARGING:
    return "charging";
  case CORES3_APP_POWER_STATUS_USB_POWER:
    return "usb-power";
  case CORES3_APP_POWER_STATUS_BATTERY:
    return "battery";
  case CORES3_APP_POWER_STATUS_UNKNOWN:
  default:
    return "unknown";
  }
}

static cores3_app_power_status_t cores3_app_power_status_read(axp2101_t *pmic) {
  if (pmic == NULL) {
    return CORES3_APP_POWER_STATUS_UNKNOWN;
  }

  cores3_power_mgmt_power_status_t power_status =
      CORES3_POWER_MGMT_POWER_STATUS_UNKNOWN;
  int32_t err = cores3_power_mgmt_power_status_get(pmic, &power_status);
  if (err != AXP2101_ERR_NONE) {
    ESP_LOGW(CORES3_APP_LOG_TAG,
             "Failed to read board power status for status bar: %s (%ld)",
             cores3_power_mgmt_err_to_name(err), (long)err);
    return CORES3_APP_POWER_STATUS_UNKNOWN;
  }

  return cores3_app_power_status_from_power_mgmt(power_status);
}

static struct {
  cores3_app_power_mgmt_init_hook_t init_callback;
  cores3_app_power_mgmt_periodic_hook_t periodic_callback;
  cores3_app_power_status_hook_t status_callback;
  void *user_ctx;
} power_hooks = {0};

typedef struct {
  aw9523b_t io_expander;
  axp2101_t pmic;
  ft6x36_t touch_screen;
  cores3_display_t display;
  display_surface_t surface;
  cores3_gui_app_t gui;
  cores3_board_t board;
  SemaphoreHandle_t backlight_mutex;
  TaskHandle_t task_handle;
  cores3_app_power_status_t power_status;
  bool power_status_valid;
  bool board_initialized;
  bool io_expander_initialized;
  bool touch_initialized;
  bool display_initialized;
  bool surface_initialized;
  bool gui_initialized;
  bool display_power_save_enabled;
  cores3_app_display_power_save_override_t display_power_save_override;
  TickType_t last_user_activity_tick;
} app_t;

static app_t app = {0};

void cores3_app_configure_power_hooks(const cores3_app_power_hooks_t *hooks) {
  if (hooks == NULL) {
    return;
  }

  if ((hooks->update_mask & CORES3_APP_POWER_HOOK_UPDATE_INIT_CALLBACK) != 0U) {
    power_hooks.init_callback = hooks->init_callback;
  }

  if ((hooks->update_mask & CORES3_APP_POWER_HOOK_UPDATE_PERIODIC_CALLBACK) !=
      0U) {
    power_hooks.periodic_callback = hooks->periodic_callback;
  }

  if ((hooks->update_mask & CORES3_APP_POWER_HOOK_UPDATE_STATUS_CALLBACK) !=
      0U) {
    power_hooks.status_callback = hooks->status_callback;
  }

  if ((hooks->update_mask & CORES3_APP_POWER_HOOK_UPDATE_USER_CTX) != 0U) {
    power_hooks.user_ctx = hooks->user_ctx;
  }
}

cores3_app_power_status_t cores3_app_power_status_get(void) {
  if (!app.power_status_valid) {
    return CORES3_APP_POWER_STATUS_UNKNOWN;
  }

  return app.power_status;
}

int32_t cores3_app_display_power_save_override_set(
    cores3_app_display_power_save_override_t override_mode) {
  switch (override_mode) {
  case CORES3_APP_DISPLAY_POWER_SAVE_OVERRIDE_AUTO:
  case CORES3_APP_DISPLAY_POWER_SAVE_OVERRIDE_FORCE_DISABLED:
  case CORES3_APP_DISPLAY_POWER_SAVE_OVERRIDE_FORCE_ENABLED:
    break;

  default:
    return AXP2101_ERR_INVALID_ARG;
  }

  app.display_power_save_override = override_mode;
  app.last_user_activity_tick = xTaskGetTickCount();

  if (app.task_handle != NULL) {
    xTaskNotifyGive(app.task_handle);
  }

  return AXP2101_ERR_NONE;
}

cores3_app_display_power_save_override_t
cores3_app_display_power_save_override_get(void) {
  return app.display_power_save_override;
}

bool cores3_app_display_power_save_enabled_get(void) {
  return app.display_power_save_enabled;
}

int32_t cores3_app_set_main_text_content(const char *text) {
  if (!app.gui_initialized) {
    return ILI9342_ERR_INVALID_STATE;
  }

  return cores3_gui_app_set_main_text_content(&app.gui, text);
}

int32_t cores3_app_lcd_backlight_voltage_set(uint16_t voltage_mv) {
  if (!app.board_initialized) {
    return AXP2101_ERR_INVALID_STATE;
  }

  if (app.backlight_mutex == NULL) {
    return AXP2101_ERR_INVALID_STATE;
  }

  if (xSemaphoreTake(app.backlight_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGW(CORES3_APP_LOG_TAG, "Failed to acquire backlight mutex");
    return AXP2101_ERR_TIMEOUT;
  }

  int32_t err = axp2101_dldo1_voltage_set(&app.pmic, voltage_mv);

  xSemaphoreGive(app.backlight_mutex);
  return err;
}

int32_t cores3_app_lcd_backlight_voltage_get(uint16_t *voltage_mv) {
  if (voltage_mv == NULL) {
    return AXP2101_ERR_INVALID_ARG;
  }

  if (!app.board_initialized) {
    return AXP2101_ERR_INVALID_STATE;
  }

  if (app.backlight_mutex == NULL) {
    return AXP2101_ERR_INVALID_STATE;
  }

  if (xSemaphoreTake(app.backlight_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGW(CORES3_APP_LOG_TAG, "Failed to acquire backlight mutex");
    return AXP2101_ERR_TIMEOUT;
  }

  int32_t err = axp2101_dldo1_voltage_get(&app.pmic, voltage_mv);

  xSemaphoreGive(app.backlight_mutex);
  return err;
}

static const uint16_t CORES3_APP_BACKLIGHT_MIN_MV = 500;
static const uint16_t CORES3_APP_BACKLIGHT_MAX_MV = 3300;

static uint8_t cores3_app_brightness_gamma_correct(uint8_t brightness_percent) {
  static const uint8_t gamma_table[101] = {
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  1,
      1,  1,  1,  1,  1,  1,  1,  1,  2,  2,
      2,  2,  2,  2,  3,  3,  3,  3,  3,  4,
      4,  4,  4,  5,  5,  5,  5,  6,  6,  6,
      7,  7,  7,  8,  8,  8,  9,  9,  10, 10,
      10, 11, 11, 12, 12, 13, 13, 14, 14, 15,
      15, 16, 16, 17, 17, 18, 18, 19, 20, 21,
      21, 22, 23, 24, 24, 25, 26, 27, 28, 28,
      28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
      28
  };
  return gamma_table[brightness_percent];
}

int32_t cores3_app_display_brightness_set(uint8_t brightness_percent) {
  if (brightness_percent > 100) {
    return AXP2101_ERR_INVALID_ARG;
  }

  if (!app.board_initialized) {
    return AXP2101_ERR_INVALID_STATE;
  }

  uint8_t step = cores3_app_brightness_gamma_correct(brightness_percent);
  uint16_t voltage_mv = CORES3_APP_BACKLIGHT_MIN_MV + (step * 100);

  int32_t err = cores3_app_lcd_backlight_voltage_set(voltage_mv);
  if (err == AXP2101_ERR_NONE) {
    ESP_LOGI(CORES3_APP_LOG_TAG, "Display brightness set to %u%% (%u mV)",
             brightness_percent, voltage_mv);
  }
  return err;
}

int32_t cores3_app_display_brightness_get(uint8_t *brightness_percent) {
  if (brightness_percent == NULL) {
    return AXP2101_ERR_INVALID_ARG;
  }

  uint16_t voltage_mv = 0;
  int32_t err = cores3_app_lcd_backlight_voltage_get(&voltage_mv);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  if (voltage_mv <= CORES3_APP_BACKLIGHT_MIN_MV) {
    *brightness_percent = 0;
  } else if (voltage_mv >= CORES3_APP_BACKLIGHT_MAX_MV) {
    *brightness_percent = 100;
  } else {
    uint32_t range_mv =
        CORES3_APP_BACKLIGHT_MAX_MV - CORES3_APP_BACKLIGHT_MIN_MV;
    *brightness_percent =
        (uint8_t)(((voltage_mv - CORES3_APP_BACKLIGHT_MIN_MV) * 100) /
                  range_mv);
  }

  return AXP2101_ERR_NONE;
}

static void cores3_app_cleanup(void) {
  if (app.gui_initialized) {
    cores3_gui_app_deinit(&app.gui);
    app.gui_initialized = false;
  }

  if (app.surface_initialized) {
    display_surface_deinit(&app.surface);
    app.surface_initialized = false;
  }

  if (app.display_initialized) {
    cores3_display_deinit(&app.display);
    app.display_initialized = false;
  }

  if (app.touch_initialized) {
    cores3_touch_deinit(&app.touch_screen);
    app.touch_initialized = false;
  }

  if (app.io_expander_initialized) {
    cores3_io_extender_deinit(&app.io_expander);
    app.io_expander_initialized = false;
  }

  if (app.backlight_mutex != NULL) {
    vSemaphoreDelete(app.backlight_mutex);
    app.backlight_mutex = NULL;
  }

  memset(&app.pmic, 0, sizeof(app.pmic));

  if (app.board_initialized) {
    cores3_board_deinit(&app.board);
    app.board_initialized = false;
  }
}

static bool cores3_app_touch_coordinates_map(uint16_t raw_x, uint16_t raw_y,
                                             int16_t *mapped_x,
                                             int16_t *mapped_y) {
  if (mapped_x == NULL || mapped_y == NULL) {
    return false;
  }

  const uint16_t display_width = cores3_display_width();
  const uint16_t display_height = cores3_display_height();
  if (display_width == 0U || display_height == 0U || raw_x >= display_width ||
      raw_y >= display_height) {
    return false;
  }

  *mapped_x = (int16_t)((display_width - 1U) - raw_x);
  *mapped_y = (int16_t)((display_height - 1U) - raw_y);
  return true;
}

static int32_t cores3_app_init_board_devices(void) {
  int32_t err = cores3_board_init(&app.board);
  if (err != 0) {
    printf("Failed to initialize board: %ld\n", (long)err);
    return err;
  }
  app.board_initialized = true;

  err = cores3_io_extender_init(app.board.i2c_aw9523b, &app.io_expander);
  if (err != 0) {
    printf("Failed to initialize AW9523B: %s\n",
           cores3_io_extender_err_to_name(err));
    return err;
  }
  app.io_expander_initialized = true;

  err = cores3_power_mgmt_init(app.board.i2c_axp2101, &app.io_expander,
                               &app.pmic);
  if (err != 0) {
    return err;
  }

  app.backlight_mutex = xSemaphoreCreateMutex();
  if (app.backlight_mutex == NULL) {
    printf("Failed to create backlight mutex\n");
    return AXP2101_ERR_NO_MEM;
  }

  err = cores3_io_extender_host_interrupt_init(app.task_handle);
  if (err != 0) {
    printf("Failed to setup I/O extender interrupt: %s (%ld)\n",
           cores3_io_extender_err_to_name(err), (long)err);
    return err;
  }

  err = cores3_touch_init(app.board.i2c_ft6336, &app.io_expander,
                          &app.touch_screen);
  if (err != FT6X36_ERR_NONE) {
    printf("Failed to configure the touch screen: %s (%ld)\n",
           cores3_touch_err_to_name(err), (long)err);
    return err;
  }
  app.touch_initialized = true;

  err = cores3_display_init(&app.display, app.board.display_spi_device,
                            &app.io_expander);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to initialize CoreS3 display: %ld\n", (long)err);
    return err;
  }
  app.display_initialized = true;

  err = display_surface_init(&app.surface, cores3_display_panel(&app.display),
                             cores3_display_width(), cores3_display_height(),
                             cores3_display_max_transfer_bytes());
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to initialize display surface: %ld\n", (long)err);
    return err;
  }
  app.surface_initialized = true;

  return ILI9342_ERR_NONE;
}

static bool cores3_app_wifi_connected(void) {
  wifi_ap_record_t ap_info = {0};
  esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
  return err == ESP_OK;
}

static int32_t cores3_app_refresh_status_bar(void) {
  cores3_app_power_status_t power_status =
      cores3_app_power_status_read(&app.pmic);
  bool power_status_changed =
      !app.power_status_valid || app.power_status != power_status;
  app.power_status = power_status;
  app.power_status_valid = true;

  axp2101_fuel_gauge_t fuel_gauge = {0};
  int32_t fuel_err = axp2101_fuel_gauge_get(&app.pmic, &fuel_gauge);
  uint8_t battery_percent = 0;
  bool battery_percent_valid = false;
  if (fuel_err == AXP2101_ERR_NONE && fuel_gauge.battery_percent_valid) {
    battery_percent = fuel_gauge.battery_percent;
    battery_percent_valid = true;
  }

  if (power_status_changed && power_hooks.status_callback != NULL) {
    power_hooks.status_callback(power_status, power_hooks.user_ctx);
  }

  char free_heap_str[64] = {0};
  snprintf(free_heap_str, sizeof(free_heap_str), "Free Heap %lu B",
           (unsigned long)esp_get_free_heap_size());

  return cores3_gui_app_set_status_bar(
      &app.gui, free_heap_str, cores3_app_wifi_connected(),
      cores3_app_power_status_to_gui(power_status), battery_percent,
      battery_percent_valid);
}

static bool cores3_app_tick_deadline_reached(TickType_t now,
                                             TickType_t deadline) {
  return (int32_t)(now - deadline) >= 0;
}

static bool cores3_app_display_power_save_allowed(void) {
  return app.power_status_valid &&
         app.power_status == CORES3_APP_POWER_STATUS_BATTERY;
}

static bool cores3_app_display_power_save_forced_enabled(void) {
  return app.display_power_save_override ==
         CORES3_APP_DISPLAY_POWER_SAVE_OVERRIDE_FORCE_ENABLED;
}

static bool cores3_app_display_power_save_forced_disabled(void) {
  return app.display_power_save_override ==
         CORES3_APP_DISPLAY_POWER_SAVE_OVERRIDE_FORCE_DISABLED;
}

static int32_t cores3_app_display_power_save_set(bool enabled) {
  if (!app.display_initialized) {
    return ILI9342_ERR_INVALID_STATE;
  }

  if (app.display_power_save_enabled == enabled) {
    return ILI9342_ERR_NONE;
  }

  int32_t err = ILI9342_ERR_NONE;
  if (enabled) {
    err = cores3_power_mgmt_lcd_backlight_dim_set(&app.pmic, true);
    if (err != AXP2101_ERR_NONE) {
      return err;
    }

    err = cores3_display_power_save_set(&app.display, true);
    if (err != ILI9342_ERR_NONE) {
      (void)cores3_power_mgmt_lcd_backlight_dim_set(&app.pmic, false);
      return err;
    }
  } else {
    err = cores3_display_power_save_set(&app.display, false);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }

    err = cores3_power_mgmt_lcd_backlight_dim_set(&app.pmic, false);
    if (err != AXP2101_ERR_NONE) {
      (void)cores3_display_power_save_set(&app.display, true);
      return err;
    }
  }

  app.display_power_save_enabled = enabled;
  ESP_LOGI(CORES3_APP_LOG_TAG, "Display power-save %s",
           enabled ? "enabled" : "disabled");
  return ILI9342_ERR_NONE;
}

static void cores3_app_note_user_activity(void) {
  app.last_user_activity_tick = xTaskGetTickCount();

  if (!app.display_power_save_enabled ||
      cores3_app_display_power_save_forced_enabled()) {
    return;
  }

  int32_t err = cores3_app_display_power_save_set(false);
  if (err != ILI9342_ERR_NONE) {
    ESP_LOGW(CORES3_APP_LOG_TAG, "Failed to restore display after touch: %ld",
             (long)err);
  }
}

static void cores3_app_run_display_idle_policy(void) {
  if (!app.display_initialized) {
    return;
  }

  TickType_t now = xTaskGetTickCount();
  if (cores3_app_display_power_save_forced_disabled()) {
    app.last_user_activity_tick = now;
    if (app.display_power_save_enabled) {
      int32_t err = cores3_app_display_power_save_set(false);
      if (err != ILI9342_ERR_NONE) {
        ESP_LOGW(CORES3_APP_LOG_TAG,
                 "Failed to disable forced-off display power-save mode: %ld",
                 (long)err);
      }
    }
    return;
  }

  if (cores3_app_display_power_save_forced_enabled()) {
    if (!app.display_power_save_enabled) {
      int32_t err = cores3_app_display_power_save_set(true);
      if (err != ILI9342_ERR_NONE) {
        ESP_LOGW(CORES3_APP_LOG_TAG,
                 "Failed to enable forced display power-save mode: %ld",
                 (long)err);
      }
    }
    return;
  }

  if (!cores3_app_display_power_save_allowed()) {
    app.last_user_activity_tick = now;
    if (app.display_power_save_enabled) {
      int32_t err = cores3_app_display_power_save_set(false);
      if (err != ILI9342_ERR_NONE) {
        ESP_LOGW(
            CORES3_APP_LOG_TAG,
            "Failed to disable display power-save outside battery mode: %ld",
            (long)err);
      }
    }
    return;
  }

  if (app.display_power_save_enabled) {
    return;
  }

  TickType_t dim_deadline =
      app.last_user_activity_tick + CORES3_APP_DISPLAY_DIM_TIMEOUT_TICKS;
  if (!cores3_app_tick_deadline_reached(now, dim_deadline)) {
    return;
  }

  int32_t err = cores3_app_display_power_save_set(true);
  if (err != ILI9342_ERR_NONE) {
    ESP_LOGW(CORES3_APP_LOG_TAG, "Failed to enter display power-save mode: %ld",
             (long)err);
  }
}

static bool
cores3_app_run_periodic_power_mgmt_hook_if_due(TickType_t *next_refresh_tick) {
  if (power_hooks.periodic_callback == NULL || next_refresh_tick == NULL) {
    return false;
  }

  TickType_t now = xTaskGetTickCount();
  if (!cores3_app_tick_deadline_reached(now, *next_refresh_tick)) {
    return false;
  }

  power_hooks.periodic_callback(&app.pmic, power_hooks.user_ctx);

  do {
    *next_refresh_tick += CORES3_APP_STATUS_REFRESH_INTERVAL_TICKS;
  } while (cores3_app_tick_deadline_reached(now, *next_refresh_tick));

  return true;
}

static void cores3_app_handle_gui_event(cores3_gui_app_event_t event,
                                        void *user_ctx) {
  (void)user_ctx;

  switch (event) {
  case CORES3_GUI_APP_EVENT_REBOOT_BUTTON_PRESSED:
    esp_restart();

  default:
    break;
  }
}

static void cores3_app_process_touch(void) {
  uint8_t input_value = 0;

  int32_t err = aw9523b_port_input_read(&app.io_expander, 1, &input_value);
  if (err != AW9523B_ERR_NONE) {
    ESP_LOGE(CORES3_APP_LOG_TAG,
             "Failed to read AW9523B register for PORT1 input data: %s",
             cores3_io_extender_err_to_name(err));
    return;
  }

  (void)input_value;
  ft6x36_touch_data_t out_touch;
  err = ft6x36_touch_data_get(&app.touch_screen, &out_touch);
  if (err != FT6X36_ERR_NONE) {
    ESP_LOGE(CORES3_APP_LOG_TAG, "Failed to read the touch data: %s (%ld)",
             ft6x36_err_to_name(err), (long)err);
    return;
  }

  for (uint8_t count = 0; count < out_touch.touch_count; count++) {
    const ft6x36_touch_point_t *point = &out_touch.points[count];
    if (!point->valid) {
      continue;
    }

    ESP_LOGI(CORES3_APP_LOG_TAG,
             "Touch %u coord (%u, %u), area: %u, weight: %u, event: %u", count,
             (unsigned)point->x, (unsigned)point->y, point->area, point->weight,
             (uint8_t)point->event);

    int16_t touch_x = 0;
    int16_t touch_y = 0;
    if (!cores3_app_touch_coordinates_map(point->x, point->y, &touch_x,
                                          &touch_y)) {
      ESP_LOGW(CORES3_APP_LOG_TAG,
               "Discarding out-of-bounds touch sample (%u, %u)",
               (unsigned)point->x, (unsigned)point->y);
      continue;
    }

    switch (point->event) {
    case FT6X36_TOUCH_EVENT_PRESS_DOWN:
      cores3_app_note_user_activity();
      (void)cores3_gui_app_handle_touch(&app.gui, touch_x, touch_y,
                                        point->event);
      break;

    default:
      break;
    }
  }
}

void cores3_app_main(void) {
  memset(&app, 0, sizeof(app));
  app.task_handle = xTaskGetCurrentTaskHandle();
  if (app.task_handle == NULL) {
    ESP_LOGE(CORES3_APP_LOG_TAG, "Failed to get current task handle");
    return;
  }

  int32_t err = cores3_app_init_board_devices();
  if (err != 0) {
    ESP_LOGE(CORES3_APP_LOG_TAG, "Failed to initialize board devices: %ld",
             (long)err);
    cores3_app_cleanup();
    return;
  }

  err = cores3_gui_app_init(&app.gui, &app.surface);
  if (err != 0) {
    ESP_LOGE(CORES3_APP_LOG_TAG, "Failed to initialize GUI: %ld", (long)err);
    cores3_app_cleanup();
    return;
  }
  app.gui_initialized = true;
  app.last_user_activity_tick = xTaskGetTickCount();

  cores3_gui_app_set_event_callback(&app.gui, cores3_app_handle_gui_event,
                                    NULL);

  err = cores3_gui_app_set_main_text_content(
      &app.gui, CORES3_APP_DEFAULT_MAIN_TEXT_CONTENT);
  if (err != 0) {
    ESP_LOGE(CORES3_APP_LOG_TAG, "Failed to set main text content: %ld",
             (long)err);
    cores3_app_cleanup();
    return;
  }

  if (power_hooks.init_callback != NULL) {
    err = power_hooks.init_callback(&app.pmic, power_hooks.user_ctx);
    if (err != 0) {
      ESP_LOGE(CORES3_APP_LOG_TAG,
               "Failed to run power management init hook: %ld", (long)err);
      cores3_app_cleanup();
      return;
    }
  }

  err = cores3_app_refresh_status_bar();
  if (err != 0) {
    ESP_LOGE(CORES3_APP_LOG_TAG, "Failed to refresh status bar: %ld",
             (long)err);
    cores3_app_cleanup();
    return;
  }

  TickType_t next_power_mgmt_refresh_tick = 0U;
  if (power_hooks.periodic_callback != NULL) {
    next_power_mgmt_refresh_tick =
        xTaskGetTickCount() + CORES3_APP_STATUS_REFRESH_INTERVAL_TICKS;
  }

  while (1) {
    TickType_t wait_ticks = portMAX_DELAY;
    if (power_hooks.periodic_callback != NULL) {
      TickType_t now = xTaskGetTickCount();
      wait_ticks =
          cores3_app_tick_deadline_reached(now, next_power_mgmt_refresh_tick)
              ? 0U
              : (next_power_mgmt_refresh_tick - now);
    }

    uint32_t pending_notifications = ulTaskNotifyTake(pdTRUE, wait_ticks);
    while (pending_notifications > 0U) {
      (void)cores3_app_refresh_status_bar();
      cores3_app_process_touch();
      if (cores3_app_run_periodic_power_mgmt_hook_if_due(
              &next_power_mgmt_refresh_tick)) {
        (void)cores3_app_refresh_status_bar();
      }
      cores3_app_run_display_idle_policy();
      pending_notifications--;
      if (pending_notifications == 0U) {
        pending_notifications = ulTaskNotifyTake(pdTRUE, 0U);
      }
    }

    if (cores3_app_run_periodic_power_mgmt_hook_if_due(
            &next_power_mgmt_refresh_tick)) {
      (void)cores3_app_refresh_status_bar();
    }

    cores3_app_run_display_idle_policy();
  }
}

void cores3_app_task(void *task_context) {
  (void)task_context;

  cores3_app_main();
  vTaskDelete(NULL);
}
