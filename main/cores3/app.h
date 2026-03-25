#pragma once

#include <stdint.h>

#define CORES3_APP_TASK_STACK_SIZE_DEFAULT ((uint32_t)8192U)

void cores3_app_main(void);
void cores3_app_task(void *task_context);
