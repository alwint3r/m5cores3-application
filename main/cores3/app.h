#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <axp2101/axp2101.h>

#define CORES3_APP_TASK_STACK_SIZE_DEFAULT ((uint32_t)8192U)

typedef enum {
  CORES3_APP_POWER_STATUS_UNKNOWN = 0,
  CORES3_APP_POWER_STATUS_CHARGING,
  CORES3_APP_POWER_STATUS_USB_POWER,
  CORES3_APP_POWER_STATUS_BATTERY,
} cores3_app_power_status_t;

typedef enum {
  CORES3_APP_DISPLAY_POWER_SAVE_OVERRIDE_AUTO = 0,
  CORES3_APP_DISPLAY_POWER_SAVE_OVERRIDE_FORCE_DISABLED,
  CORES3_APP_DISPLAY_POWER_SAVE_OVERRIDE_FORCE_ENABLED,
} cores3_app_display_power_save_override_t;

typedef int32_t (*cores3_app_init_hook_t)(axp2101_t *pmic, void *user_ctx);
typedef void (*cores3_app_periodic_hook_t)(axp2101_t *pmic, void *user_ctx);
typedef void (*cores3_app_power_status_hook_t)(cores3_app_power_status_t power_status,
                                               void *user_ctx);
typedef void (*cores3_app_reboot_hook_t)(void *user_ctx);

typedef struct {
  cores3_app_init_hook_t init_hook;
  cores3_app_periodic_hook_t tick_1s_hook;
  cores3_app_power_status_hook_t power_status_hook;
  cores3_app_reboot_hook_t reboot_hook;
  void *user_ctx;
} cores3_app_hooks_t;

cores3_app_power_status_t cores3_app_power_status_get(void);
const char *cores3_app_power_status_to_string(cores3_app_power_status_t status);
int32_t cores3_app_display_power_save_override_set(
    cores3_app_display_power_save_override_t override_mode);
cores3_app_display_power_save_override_t cores3_app_display_power_save_override_get(void);
bool cores3_app_display_power_save_enabled_get(void);
int32_t cores3_app_lcd_backlight_voltage_set(uint16_t voltage_mv);
int32_t cores3_app_lcd_backlight_voltage_get(uint16_t *voltage_mv);
int32_t cores3_app_display_brightness_set(uint8_t brightness_percent);
int32_t cores3_app_display_brightness_get(uint8_t *brightness_percent);
int32_t cores3_app_set_main_text_content(const char *text);
int32_t cores3_app_battery_percentage_get(uint8_t *out_percent);
int32_t cores3_app_battery_voltage_get(uint16_t *out_mv);
void cores3_app_main(void);
void cores3_app_task(void *task_context);
void cores3_app_hooks_init(void);
cores3_app_hooks_t *cores3_app_hooks_get(void);
