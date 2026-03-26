#include "app.h"

#include <stdbool.h>
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
#include "graphics/display_surface.h"
#include "gui_app.h"

static const char *CORES3_APP_LOG_TAG = "CORES3_APP";
static const char *CORES3_APP_DEFAULT_MAIN_TEXT_CONTENT = "Waiting for events...";
static const TickType_t CORES3_APP_PMIC_REFRESH_INTERVAL_TICKS = pdMS_TO_TICKS(1000);

static bool cores3_app_power_status_is_charging(axp2101_charging_status_t status) {
  switch (status) {
    case AXP2101_CHARGING_STATUS_TRI_CHARGE:
    case AXP2101_CHARGING_STATUS_PRE_CHARGE:
    case AXP2101_CHARGING_STATUS_CONSTANT_CHARGE:
    case AXP2101_CHARGING_STATUS_CONSTANT_VOLTAGE:
      return true;

    case AXP2101_CHARGING_STATUS_CHARGE_DONE:
    case AXP2101_CHARGING_STATUS_NOT_CHARGING:
    case AXP2101_CHARGING_STATUS_UNKNOWN:
    default:
      return false;
  }
}

static cores3_gui_power_status_t cores3_app_power_status_read(axp2101_t *pmic) {
  if (pmic == NULL) {
    return CORES3_GUI_POWER_STATUS_UNKNOWN;
  }

  axp2101_status1_t status1 = {0};
  int32_t err = axp2101_status1_get(pmic, &status1);
  if (err != AXP2101_ERR_NONE) {
    ESP_LOGW(CORES3_APP_LOG_TAG,
             "Failed to read AXP2101 status1 for status bar: %s (%ld)",
             axp2101_err_to_name(err),
             (long)err);
    return CORES3_GUI_POWER_STATUS_UNKNOWN;
  }

  if (!status1.vbus_good) {
    return CORES3_GUI_POWER_STATUS_BATTERY;
  }

  axp2101_status2_t status2 = {0};
  err = axp2101_status2_get(pmic, &status2);
  if (err != AXP2101_ERR_NONE) {
    ESP_LOGW(CORES3_APP_LOG_TAG,
             "Failed to read AXP2101 status2 for status bar: %s (%ld)",
             axp2101_err_to_name(err),
             (long)err);
    return CORES3_GUI_POWER_STATUS_USB_POWER;
  }

  if (cores3_app_power_status_is_charging(status2.charging_status)) {
    return CORES3_GUI_POWER_STATUS_CHARGING;
  }

  return CORES3_GUI_POWER_STATUS_USB_POWER;
}

static struct {
  cores3_app_pmic_init_hook_t init_hook;
  cores3_app_pmic_periodic_hook_t periodic_hook;
  void *user_ctx;
} app_hooks = {0};

typedef struct {
  aw9523b_t io_expander;
  axp2101_t pmic;
  ft6x36_t touch_screen;
  cores3_display_t display;
  display_surface_t surface;
  cores3_gui_app_t gui;
  cores3_board_t board;
  TaskHandle_t task_handle;
  bool board_initialized;
  bool io_expander_initialized;
  bool touch_initialized;
  bool display_initialized;
  bool surface_initialized;
  bool gui_initialized;
} app_t;

static app_t app = {0};

void cores3_app_set_pmic_hooks(cores3_app_pmic_init_hook_t init_hook,
                               cores3_app_pmic_periodic_hook_t periodic_hook,
                               void *user_ctx) {
  app_hooks.init_hook = init_hook;
  app_hooks.periodic_hook = periodic_hook;
  app_hooks.user_ctx = user_ctx;
}

int32_t cores3_app_set_main_text_content(const char *text) {
  if (!app.gui_initialized) {
    return ILI9342_ERR_INVALID_STATE;
  }

  return cores3_gui_app_set_main_text_content(&app.gui, text);
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

  memset(&app.pmic, 0, sizeof(app.pmic));

  if (app.board_initialized) {
    cores3_board_deinit(&app.board);
    app.board_initialized = false;
  }
}

static bool cores3_app_touch_coordinates_map(uint16_t raw_x,
                                             uint16_t raw_y,
                                             uint16_t *mapped_x,
                                             uint16_t *mapped_y) {
  if (mapped_x == NULL || mapped_y == NULL) {
    return false;
  }

  const uint16_t display_width = cores3_display_width();
  const uint16_t display_height = cores3_display_height();
  if (display_width == 0U || display_height == 0U || raw_x >= display_width ||
      raw_y >= display_height) {
    return false;
  }

  *mapped_x = (uint16_t)((display_width - 1U) - raw_x);
  *mapped_y = (uint16_t)((display_height - 1U) - raw_y);
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
    printf("Failed to initialize AW9523B: %s\n", cores3_io_extender_err_to_name(err));
    return err;
  }
  app.io_expander_initialized = true;

  err = cores3_power_mgmt_init(app.board.i2c_axp2101, &app.io_expander, &app.pmic);
  if (err != 0) {
    return err;
  }

  err = cores3_io_extender_host_interrupt_init(app.task_handle);
  if (err != 0) {
    printf("Failed to setup I/O extender interrupt: %s (%ld)\n",
           cores3_io_extender_err_to_name(err),
           (long)err);
    return err;
  }

  err = cores3_touch_init(app.board.i2c_ft6336, &app.io_expander, &app.touch_screen);
  if (err != FT6X36_ERR_NONE) {
    printf("Failed to configure the touch screen: %s (%ld)\n",
           cores3_touch_err_to_name(err),
           (long)err);
    return err;
  }
  app.touch_initialized = true;

  err = cores3_display_init(&app.display, app.board.display_spi_device, &app.io_expander);
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to initialize CoreS3 display: %ld\n", (long)err);
    return err;
  }
  app.display_initialized = true;

  err = display_surface_init(&app.surface,
                             cores3_display_panel(&app.display),
                             cores3_display_width(),
                             cores3_display_height(),
                             cores3_display_max_transfer_bytes());
  if (err != ILI9342_ERR_NONE) {
    printf("Failed to initialize display surface: %ld\n", (long)err);
    return err;
  }
  app.surface_initialized = true;

  return ILI9342_ERR_NONE;
}

static int32_t cores3_app_refresh_status_bar(void) {
  char free_heap_str[64] = {0};
  snprintf(free_heap_str,
           sizeof(free_heap_str),
           "Free Heap %lu B",
           (unsigned long)esp_get_free_heap_size());

  return cores3_gui_app_set_status_bar(
      &app.gui, free_heap_str, false, cores3_app_power_status_read(&app.pmic));
}

static bool cores3_app_tick_deadline_reached(TickType_t now, TickType_t deadline) {
  return (int32_t)(now - deadline) >= 0;
}

static bool cores3_app_run_periodic_pmic_hook_if_due(TickType_t *next_refresh_tick) {
  if (app_hooks.periodic_hook == NULL || next_refresh_tick == NULL) {
    return false;
  }

  TickType_t now = xTaskGetTickCount();
  if (!cores3_app_tick_deadline_reached(now, *next_refresh_tick)) {
    return false;
  }

  app_hooks.periodic_hook(&app.pmic, app_hooks.user_ctx);

  do {
    *next_refresh_tick += CORES3_APP_PMIC_REFRESH_INTERVAL_TICKS;
  } while (cores3_app_tick_deadline_reached(now, *next_refresh_tick));

  return true;
}

static void cores3_app_handle_gui_event(cores3_gui_app_event_t event, void *user_ctx) {
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
    ESP_LOGE(CORES3_APP_LOG_TAG,
             "Failed to read the touch data: %s (%ld)",
             ft6x36_err_to_name(err),
             (long)err);
    return;
  }

  for (uint8_t count = 0; count < out_touch.touch_count; count++) {
    const ft6x36_touch_point_t *point = &out_touch.points[count];
    if (!point->valid) {
      continue;
    }

    ESP_LOGI(CORES3_APP_LOG_TAG,
             "Touch %u coord (%d, %d), area: %u, weight: %u, event: %u",
             count,
             point->x,
             point->y,
             point->area,
             point->weight,
             (uint8_t)point->event);

    uint16_t touch_x = 0;
    uint16_t touch_y = 0;
    if (!cores3_app_touch_coordinates_map(point->x, point->y, &touch_x, &touch_y)) {
      ESP_LOGW(CORES3_APP_LOG_TAG,
               "Discarding out-of-bounds touch sample (%u, %u)",
               (unsigned)point->x,
               (unsigned)point->y);
      continue;
    }

    switch (point->event) {
      case FT6X36_TOUCH_EVENT_PRESS_DOWN:
        (void)cores3_gui_app_handle_touch(&app.gui, touch_x, touch_y, point->event);
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
    return;
  }

  int32_t err = cores3_app_init_board_devices();
  if (err != 0) {
    cores3_app_cleanup();
    return;
  }

  err = cores3_gui_app_init(&app.gui, &app.surface);
  if (err != 0) {
    cores3_app_cleanup();
    return;
  }
  app.gui_initialized = true;

  cores3_gui_app_set_event_callback(&app.gui, cores3_app_handle_gui_event, NULL);

  err = cores3_gui_app_set_main_text_content(&app.gui, CORES3_APP_DEFAULT_MAIN_TEXT_CONTENT);
  if (err != 0) {
    cores3_app_cleanup();
    return;
  }

  if (app_hooks.init_hook != NULL) {
    err = app_hooks.init_hook(&app.pmic, app_hooks.user_ctx);
    if (err != 0) {
      printf("Failed to run custom PMIC init hook: %ld\n", (long)err);
      cores3_app_cleanup();
      return;
    }
  }

  err = cores3_app_refresh_status_bar();
  if (err != 0) {
    cores3_app_cleanup();
    return;
  }

  TickType_t next_pmic_refresh_tick = 0U;
  if (app_hooks.periodic_hook != NULL) {
    next_pmic_refresh_tick = xTaskGetTickCount() + CORES3_APP_PMIC_REFRESH_INTERVAL_TICKS;
  }

  while (1) {
    TickType_t wait_ticks = portMAX_DELAY;
    if (app_hooks.periodic_hook != NULL) {
      TickType_t now = xTaskGetTickCount();
      wait_ticks = cores3_app_tick_deadline_reached(now, next_pmic_refresh_tick)
                       ? 0U
                       : (next_pmic_refresh_tick - now);
    }

    uint32_t pending_notifications = ulTaskNotifyTake(pdTRUE, wait_ticks);
    while (pending_notifications > 0U) {
      (void)cores3_app_refresh_status_bar();
      cores3_app_process_touch();
      if (cores3_app_run_periodic_pmic_hook_if_due(&next_pmic_refresh_tick)) {
        (void)cores3_app_refresh_status_bar();
      }
      pending_notifications--;
      if (pending_notifications == 0U) {
        pending_notifications = ulTaskNotifyTake(pdTRUE, 0U);
      }
    }

    if (cores3_app_run_periodic_pmic_hook_if_due(&next_pmic_refresh_tick)) {
      (void)cores3_app_refresh_status_bar();
    }
  }
}

void cores3_app_task(void *task_context) {
  (void)task_context;

  cores3_app_main();
  vTaskDelete(NULL);
}
