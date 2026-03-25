#pragma once

#include <stdint.h>

#include <aw9523b/aw9523b.h>
#include <axp2101/axp2101.h>
#include <ii2c/ii2c.h>

int32_t cores3_power_mgmt_init(ii2c_device_handle_t device, aw9523b_t *expander, axp2101_t *pmic);
const char *cores3_power_mgmt_err_to_name(int32_t err);
