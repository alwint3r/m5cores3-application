#pragma once

#include <stdint.h>

#include <aw9523b/aw9523b.h>
#include <ft6x36/ft6x36.h>
#include <ii2c/ii2c.h>

int32_t cores3_touch_init(ii2c_device_handle_t device, aw9523b_t *expander, ft6x36_t *touch);
void cores3_touch_deinit(ft6x36_t *touch);
const char *cores3_touch_err_to_name(int32_t err);
