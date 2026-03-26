#pragma once

#include <stdint.h>

#include <axp2101/axp2101.h>

#define CORES3_APP_TASK_STACK_SIZE_DEFAULT ((uint32_t)8192U)

typedef int32_t (*cores3_app_pmic_init_hook_t)(axp2101_t *pmic, void *user_ctx);
typedef void (*cores3_app_pmic_periodic_hook_t)(axp2101_t *pmic, void *user_ctx);

void cores3_app_set_pmic_hooks(cores3_app_pmic_init_hook_t init_hook,
                               cores3_app_pmic_periodic_hook_t periodic_hook,
                               void *user_ctx);
int32_t cores3_app_set_main_text_content(const char *text);
void cores3_app_main(void);
void cores3_app_task(void *task_context);
