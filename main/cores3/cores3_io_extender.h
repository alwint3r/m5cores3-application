#pragma once

#include <stdint.h>

#include <aw9523b/aw9523b.h>
#include <ii2c/ii2c.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

int32_t cores3_io_extender_init(ii2c_device_handle_t device, aw9523b_t *expander);
int32_t cores3_io_extender_host_interrupt_init(TaskHandle_t notify_task);
void cores3_io_extender_deinit(aw9523b_t *expander);
const char *cores3_io_extender_err_to_name(int32_t err);
