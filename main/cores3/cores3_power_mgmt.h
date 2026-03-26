#pragma once

#include <stdint.h>

#include <aw9523b/aw9523b.h>
#include <axp2101/axp2101.h>
#include <ii2c/ii2c.h>

typedef enum {
  CORES3_POWER_MGMT_POWER_STATUS_UNKNOWN = 0,
  CORES3_POWER_MGMT_POWER_STATUS_CHARGING,
  CORES3_POWER_MGMT_POWER_STATUS_USB_POWER,
  CORES3_POWER_MGMT_POWER_STATUS_BATTERY,
} cores3_power_mgmt_power_status_t;

int32_t cores3_power_mgmt_init(ii2c_device_handle_t device, aw9523b_t *expander, axp2101_t *pmic);
int32_t cores3_power_mgmt_charge_policy_init(axp2101_t *pmic);
void cores3_power_mgmt_charge_policy_refresh(axp2101_t *pmic);
int32_t cores3_power_mgmt_power_status_get(axp2101_t *pmic,
                                           cores3_power_mgmt_power_status_t *out_status);
const char *cores3_power_mgmt_err_to_name(int32_t err);
const char *cores3_power_mgmt_power_status_to_string(cores3_power_mgmt_power_status_t status);
const char *axp2101_charging_status_to_string(axp2101_charging_status_t status);
