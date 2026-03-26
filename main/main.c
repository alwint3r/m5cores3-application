#include <stdio.h>
#include <stdbool.h>

#include <axp2101/axp2101.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "cores3/app.h"

typedef struct {
  bool initialized;
  bool last_vbus_good;
} custom_vbus_monitor_t;

static custom_vbus_monitor_t custom_vbus_monitor = {0};

static const char *custom_vbus_state_to_string(bool vbus_good) {
  return vbus_good ? "present" : "absent";
}

static int32_t custom_vbus_monitor_update_main_text(bool vbus_good) {
  char main_text[64] = {0};
  int written =
      snprintf(main_text, sizeof(main_text), "USB VBUS %s", custom_vbus_state_to_string(vbus_good));
  if (written < 0 || (size_t)written >= sizeof(main_text)) {
    return AXP2101_ERR_INVALID_STATE;
  }

  return cores3_app_set_main_text_content(main_text);
}

static void custom_vbus_monitor_apply_state(custom_vbus_monitor_t *monitor,
                                            bool vbus_good,
                                            const char *reason) {
  if (monitor == NULL) {
    return;
  }

  if (reason != NULL) {
    printf("Custom VBUS monitor: %s, VBUS %s\n", reason, custom_vbus_state_to_string(vbus_good));
  }

  monitor->last_vbus_good = vbus_good;

  int32_t err = custom_vbus_monitor_update_main_text(vbus_good);
  if (err != AXP2101_ERR_NONE) {
    printf("Failed to update main text for VBUS state: %ld\n", (long)err);
  }
}

static int32_t custom_vbus_monitor_init(axp2101_t *pmic, void *user_ctx) {
  custom_vbus_monitor_t *monitor = (custom_vbus_monitor_t *)user_ctx;
  if (pmic == NULL || monitor == NULL) {
    return AXP2101_ERR_INVALID_ARG;
  }

  int32_t err = AXP2101_ERR_NONE;
  axp2101_status1_t status1 = {0};
  err = axp2101_status1_get(pmic, &status1);
  if (err != AXP2101_ERR_NONE) {
    printf("Failed to read initial AXP2101 VBUS state: %s\n", axp2101_err_to_name(err));
    return err;
  }

  monitor->initialized = true;
  custom_vbus_monitor_apply_state(monitor, status1.vbus_good, "initialized");
  return AXP2101_ERR_NONE;
}

static void custom_vbus_monitor_refresh(axp2101_t *pmic, void *user_ctx) {
  custom_vbus_monitor_t *monitor = (custom_vbus_monitor_t *)user_ctx;
  if (pmic == NULL || monitor == NULL || !monitor->initialized) {
    return;
  }

  axp2101_status1_t status1 = {0};
  int32_t err = axp2101_status1_get(pmic, &status1);
  if (err != AXP2101_ERR_NONE) {
    printf("Failed to refresh AXP2101 VBUS state: %s\n", axp2101_err_to_name(err));
    return;
  }

  if (status1.vbus_good == monitor->last_vbus_good) {
    return;
  }

  custom_vbus_monitor_apply_state(monitor, status1.vbus_good, "periodic refresh");
}

void app_main(void) {
  // Register custom PMIC behavior before the shared CoreS3 app task starts.
  cores3_app_set_pmic_hooks(
      custom_vbus_monitor_init, custom_vbus_monitor_refresh, &custom_vbus_monitor);

  BaseType_t ret = xTaskCreate(cores3_app_task,
                               "cores3_app",
                               CORES3_APP_TASK_STACK_SIZE_DEFAULT,
                               NULL,
                               tskIDLE_PRIORITY + 1,
                               NULL);
  if (ret != pdPASS) {
    printf("Failed to create CoreS3 app task\n");
  }
}
