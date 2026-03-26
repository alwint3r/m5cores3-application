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

  return cores3_gui_app_set_status_bar(&app.gui, free_heap_str, false);
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

  err = cores3_app_refresh_status_bar();
  if (err != 0) {
    cores3_app_cleanup();
    return;
  }

  while (1) {
    uint32_t pending_notifications = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    while (pending_notifications > 0U) {
      (void)cores3_app_refresh_status_bar();
      cores3_app_process_touch();
      pending_notifications--;
      if (pending_notifications == 0U) {
        pending_notifications = ulTaskNotifyTake(pdTRUE, 0U);
      }
    }
  }
}

void cores3_app_task(void *task_context) {
  (void)task_context;

  cores3_app_main();
  vTaskDelete(NULL);
}
