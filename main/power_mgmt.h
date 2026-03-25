#pragma once

#include <stdint.h>

#include <aw9523b/aw9523b.h>
#include <axp2101/axp2101.h>

int32_t power_mgmt_init(aw9523b_t *expander, axp2101_t *pmic);
const char *power_mgmt_err_to_name(int32_t err);
