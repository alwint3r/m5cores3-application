#include <stdio.h>
#include <stdbool.h>

#include <axp2101/axp2101.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "cores3/app.h"
#include "cores3/cores3_power_mgmt.h"

typedef struct {
  bool initialized;
  bool last_usb_vbus_good;
  bool power_status_valid;
  cores3_app_power_status_t last_power_status;
} custom_cores3_app_context_t;

static custom_cores3_app_context_t custom_cores3_app = {0};

static const char *custom_cores3_app_vbus_state_to_string(bool vbus_good) {
  return vbus_good ? "present" : "absent";
}

static int32_t custom_cores3_app_update_vbus_main_text(bool vbus_good) {
#if 0
  char main_text[64] = {0};
  int written = snprintf(main_text,
                         sizeof(main_text),
                         "USB VBUS %s",
                         custom_cores3_app_vbus_state_to_string(vbus_good));
  if (written < 0 || (size_t)written >= sizeof(main_text)) {
    return AXP2101_ERR_INVALID_STATE;
  }

  return cores3_app_set_main_text_content(main_text);
#else
  (void)vbus_good;
  return 0;
#endif
}

static void custom_cores3_app_apply_vbus_state(custom_cores3_app_context_t *app_ctx,
                                               bool vbus_good,
                                               const char *reason) {
  if (app_ctx == NULL) {
    return;
  }

  if (reason != NULL) {
    printf("Custom CoreS3 app: %s, VBUS %s\n",
           reason,
           custom_cores3_app_vbus_state_to_string(vbus_good));
  }

  app_ctx->last_usb_vbus_good = vbus_good;

  int32_t err = custom_cores3_app_update_vbus_main_text(vbus_good);
  if (err != AXP2101_ERR_NONE) {
    printf("Failed to update main text for VBUS state: %ld\n", (long)err);
  }
}

static void custom_cores3_app_power_status_changed(cores3_app_power_status_t power_status,
                                                   void *user_ctx) {
  custom_cores3_app_context_t *app_ctx = (custom_cores3_app_context_t *)user_ctx;
  if (app_ctx == NULL) {
    return;
  }

  app_ctx->power_status_valid = true;
  app_ctx->last_power_status = power_status;
  printf("Custom power status hook: %s\n", cores3_app_power_status_to_string(power_status));
}

static int32_t custom_cores3_app_init(axp2101_t *pmic, void *user_ctx) {
  custom_cores3_app_context_t *app_ctx = (custom_cores3_app_context_t *)user_ctx;
  if (pmic == NULL || app_ctx == NULL) {
    return AXP2101_ERR_INVALID_ARG;
  }

  int32_t err = cores3_power_mgmt_charge_policy_init(pmic);
  if (err != AXP2101_ERR_NONE) {
    printf("Failed to initialize charge threshold policy: %s\n",
           cores3_power_mgmt_err_to_name(err));
    return err;
  }

  axp2101_status1_t status1 = {0};
  err = axp2101_status1_get(pmic, &status1);
  if (err != AXP2101_ERR_NONE) {
    printf("Failed to read initial AXP2101 VBUS state: %s\n", axp2101_err_to_name(err));
    return err;
  }

  app_ctx->initialized = true;
  custom_cores3_app_apply_vbus_state(app_ctx, status1.vbus_good, "initialized");
  return AXP2101_ERR_NONE;
}

static void custom_cores3_app_refresh(axp2101_t *pmic, void *user_ctx) {
  custom_cores3_app_context_t *app_ctx = (custom_cores3_app_context_t *)user_ctx;
  if (pmic == NULL || app_ctx == NULL || !app_ctx->initialized) {
    return;
  }

  cores3_power_mgmt_charge_policy_refresh(pmic);

  axp2101_status1_t status1 = {0};
  int32_t err = axp2101_status1_get(pmic, &status1);
  if (err != AXP2101_ERR_NONE) {
    printf("Failed to refresh AXP2101 VBUS state: %s\n", axp2101_err_to_name(err));
    return;
  }

  if (status1.vbus_good == app_ctx->last_usb_vbus_good) {
    return;
  }

  custom_cores3_app_apply_vbus_state(app_ctx, status1.vbus_good, "periodic refresh");
}

void app_main(void) {
  // Register custom PMIC behavior before the shared CoreS3 app task starts.
  cores3_app_configure_power_hooks(&(cores3_app_power_hooks_t){
      .update_mask = CORES3_APP_POWER_HOOK_UPDATE_INIT_CALLBACK |
                     CORES3_APP_POWER_HOOK_UPDATE_PERIODIC_CALLBACK |
                     CORES3_APP_POWER_HOOK_UPDATE_STATUS_CALLBACK |
                     CORES3_APP_POWER_HOOK_UPDATE_USER_CTX,
      .init_callback = custom_cores3_app_init,
      .periodic_callback = custom_cores3_app_refresh,
      .status_callback = custom_cores3_app_power_status_changed,
      .user_ctx = &custom_cores3_app,
  });

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
