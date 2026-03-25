#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "cores3/app.h"

void app_main(void) {
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
