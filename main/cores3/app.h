#pragma once

#include <stdint.h>

#include <axp2101/axp2101.h>

#define CORES3_APP_TASK_STACK_SIZE_DEFAULT ((uint32_t)8192U)

typedef enum {
  CORES3_APP_POWER_STATUS_UNKNOWN = 0,
  CORES3_APP_POWER_STATUS_CHARGING,
  CORES3_APP_POWER_STATUS_USB_POWER,
  CORES3_APP_POWER_STATUS_BATTERY,
} cores3_app_power_status_t;

typedef int32_t (*cores3_app_power_mgmt_init_hook_t)(axp2101_t *pmic, void *user_ctx);
typedef void (*cores3_app_power_mgmt_periodic_hook_t)(axp2101_t *pmic, void *user_ctx);
typedef void (*cores3_app_power_status_hook_t)(cores3_app_power_status_t power_status,
                                               void *user_ctx);

typedef enum {
  CORES3_APP_POWER_HOOK_UPDATE_NONE = 0,
  CORES3_APP_POWER_HOOK_UPDATE_INIT_CALLBACK = 1u << 0,
  CORES3_APP_POWER_HOOK_UPDATE_PERIODIC_CALLBACK = 1u << 1,
  CORES3_APP_POWER_HOOK_UPDATE_STATUS_CALLBACK = 1u << 2,
  CORES3_APP_POWER_HOOK_UPDATE_USER_CTX = 1u << 3,
} cores3_app_power_hook_update_t;

typedef struct {
  uint32_t update_mask;
  cores3_app_power_mgmt_init_hook_t init_callback;
  cores3_app_power_mgmt_periodic_hook_t periodic_callback;
  cores3_app_power_status_hook_t status_callback;
  void *user_ctx;
} cores3_app_power_hooks_t;

void cores3_app_configure_power_hooks(const cores3_app_power_hooks_t *hooks);
cores3_app_power_status_t cores3_app_power_status_get(void);
const char *cores3_app_power_status_to_string(cores3_app_power_status_t status);
int32_t cores3_app_set_main_text_content(const char *text);
void cores3_app_main(void);
void cores3_app_task(void *task_context);
