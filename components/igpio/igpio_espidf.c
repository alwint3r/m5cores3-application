#include "igpio/igpio.h"

#include <driver/gpio.h>
#include <esp_err.h>
#include <stdint.h>

static int32_t esp_err_to_igpio(esp_err_t err) {
  switch (err) {
    case ESP_OK:
      return IGPIO_ERR_NONE;
    case ESP_ERR_NO_MEM:
      return IGPIO_ERR_NO_MEM;
    case ESP_ERR_INVALID_ARG:
      return IGPIO_ERR_INVALID_ARG;
    case ESP_ERR_INVALID_STATE:
      return IGPIO_ERR_INVALID_STATE;
    case ESP_ERR_NOT_FOUND:
      return IGPIO_ERR_NOT_FOUND;
    case ESP_ERR_NOT_SUPPORTED:
      return IGPIO_ERR_NOT_SUPPORTED;
    case ESP_ERR_TIMEOUT:
      return IGPIO_ERR_TIMEOUT;
    case ESP_FAIL:
      return IGPIO_ERR_FAIL;
    default:
      return IGPIO_ERR_IO;
  }
}

static bool igpio_is_valid_pin(int32_t io_num) {
  if (io_num < 0 || io_num >= GPIO_PIN_COUNT || io_num >= 64) {
    return false;
  }

  return GPIO_IS_VALID_GPIO(io_num);
}

static bool igpio_is_valid_output_pin(int32_t io_num) {
  if (io_num < 0 || io_num >= GPIO_PIN_COUNT || io_num >= 64) {
    return false;
  }

  return GPIO_IS_VALID_OUTPUT_GPIO(io_num);
}

static bool igpio_mode_requires_output(igpio_mode_t mode) {
  switch (mode) {
    case IGPIO_MODE_OUTPUT:
    case IGPIO_MODE_INPUT_OUTPUT:
    case IGPIO_MODE_OUTPUT_OPEN_DRAIN:
    case IGPIO_MODE_INPUT_OUTPUT_OPEN_DRAIN:
      return true;
    case IGPIO_MODE_DISABLED:
    case IGPIO_MODE_INPUT:
      return false;
    default:
      return false;
  }
}

static int32_t igpio_mode_to_esp(igpio_mode_t mode, gpio_mode_t *ret_mode) {
  if (ret_mode == NULL) {
    return IGPIO_ERR_INVALID_ARG;
  }

  switch (mode) {
    case IGPIO_MODE_DISABLED:
      *ret_mode = GPIO_MODE_DISABLE;
      return IGPIO_ERR_NONE;
    case IGPIO_MODE_INPUT:
      *ret_mode = GPIO_MODE_INPUT;
      return IGPIO_ERR_NONE;
    case IGPIO_MODE_OUTPUT:
      *ret_mode = GPIO_MODE_OUTPUT;
      return IGPIO_ERR_NONE;
    case IGPIO_MODE_INPUT_OUTPUT:
      *ret_mode = GPIO_MODE_INPUT_OUTPUT;
      return IGPIO_ERR_NONE;
    case IGPIO_MODE_OUTPUT_OPEN_DRAIN:
      *ret_mode = GPIO_MODE_OUTPUT_OD;
      return IGPIO_ERR_NONE;
    case IGPIO_MODE_INPUT_OUTPUT_OPEN_DRAIN:
      *ret_mode = GPIO_MODE_INPUT_OUTPUT_OD;
      return IGPIO_ERR_NONE;
    default:
      return IGPIO_ERR_INVALID_ARG;
  }
}

static int32_t igpio_pull_mode_to_esp(igpio_pull_mode_t pull_mode,
                                      gpio_pull_mode_t *ret_pull_mode,
                                      gpio_pullup_t *ret_pullup,
                                      gpio_pulldown_t *ret_pulldown) {
  if (ret_pull_mode == NULL || ret_pullup == NULL || ret_pulldown == NULL) {
    return IGPIO_ERR_INVALID_ARG;
  }

  switch (pull_mode) {
    case IGPIO_PULL_FLOATING:
      *ret_pull_mode = GPIO_FLOATING;
      *ret_pullup = GPIO_PULLUP_DISABLE;
      *ret_pulldown = GPIO_PULLDOWN_DISABLE;
      return IGPIO_ERR_NONE;
    case IGPIO_PULL_UP:
      *ret_pull_mode = GPIO_PULLUP_ONLY;
      *ret_pullup = GPIO_PULLUP_ENABLE;
      *ret_pulldown = GPIO_PULLDOWN_DISABLE;
      return IGPIO_ERR_NONE;
    case IGPIO_PULL_DOWN:
      *ret_pull_mode = GPIO_PULLDOWN_ONLY;
      *ret_pullup = GPIO_PULLUP_DISABLE;
      *ret_pulldown = GPIO_PULLDOWN_ENABLE;
      return IGPIO_ERR_NONE;
    case IGPIO_PULL_UP_DOWN:
      *ret_pull_mode = GPIO_PULLUP_PULLDOWN;
      *ret_pullup = GPIO_PULLUP_ENABLE;
      *ret_pulldown = GPIO_PULLDOWN_ENABLE;
      return IGPIO_ERR_NONE;
    default:
      return IGPIO_ERR_INVALID_ARG;
  }
}

static int32_t igpio_intr_type_to_esp(igpio_intr_type_t intr_type, gpio_int_type_t *ret_intr_type) {
  if (ret_intr_type == NULL) {
    return IGPIO_ERR_INVALID_ARG;
  }

  switch (intr_type) {
    case IGPIO_INTR_DISABLED:
      *ret_intr_type = GPIO_INTR_DISABLE;
      return IGPIO_ERR_NONE;
    case IGPIO_INTR_POSEDGE:
      *ret_intr_type = GPIO_INTR_POSEDGE;
      return IGPIO_ERR_NONE;
    case IGPIO_INTR_NEGEDGE:
      *ret_intr_type = GPIO_INTR_NEGEDGE;
      return IGPIO_ERR_NONE;
    case IGPIO_INTR_ANYEDGE:
      *ret_intr_type = GPIO_INTR_ANYEDGE;
      return IGPIO_ERR_NONE;
    case IGPIO_INTR_LOW_LEVEL:
      *ret_intr_type = GPIO_INTR_LOW_LEVEL;
      return IGPIO_ERR_NONE;
    case IGPIO_INTR_HIGH_LEVEL:
      *ret_intr_type = GPIO_INTR_HIGH_LEVEL;
      return IGPIO_ERR_NONE;
    default:
      return IGPIO_ERR_INVALID_ARG;
  }
}

const char *igpio_err_to_name(int32_t err) {
  switch (err) {
    case IGPIO_ERR_NONE:
      return "IGPIO_ERR_NONE";
    case IGPIO_ERR_FAIL:
      return "IGPIO_ERR_FAIL";
    case IGPIO_ERR_NO_MEM:
      return "IGPIO_ERR_NO_MEM";
    case IGPIO_ERR_INVALID_ARG:
      return "IGPIO_ERR_INVALID_ARG";
    case IGPIO_ERR_INVALID_STATE:
      return "IGPIO_ERR_INVALID_STATE";
    case IGPIO_ERR_NOT_FOUND:
      return "IGPIO_ERR_NOT_FOUND";
    case IGPIO_ERR_NOT_SUPPORTED:
      return "IGPIO_ERR_NOT_SUPPORTED";
    case IGPIO_ERR_TIMEOUT:
      return "IGPIO_ERR_TIMEOUT";
    case IGPIO_ERR_IO:
      return "IGPIO_ERR_IO";
    default:
      return "IGPIO_ERR_UNKNOWN";
  }
}

void igpio_get_default_config(igpio_config_t *config) {
  if (config == NULL) {
    return;
  }

  config->io_num = -1;
  config->mode = IGPIO_MODE_DISABLED;
  config->pull_mode = IGPIO_PULL_FLOATING;
  config->intr_type = IGPIO_INTR_DISABLED;
}

int32_t igpio_configure(const igpio_config_t *config) {
  if (config == NULL || !igpio_is_valid_pin(config->io_num)) {
    return IGPIO_ERR_INVALID_ARG;
  }
  if (igpio_mode_requires_output(config->mode) && !igpio_is_valid_output_pin(config->io_num)) {
    return IGPIO_ERR_INVALID_ARG;
  }

  gpio_mode_t esp_mode = GPIO_MODE_DISABLE;
  int32_t rc = igpio_mode_to_esp(config->mode, &esp_mode);
  if (rc != IGPIO_ERR_NONE) {
    return rc;
  }

  gpio_pull_mode_t esp_pull_mode = GPIO_FLOATING;
  gpio_pullup_t esp_pullup = GPIO_PULLUP_DISABLE;
  gpio_pulldown_t esp_pulldown = GPIO_PULLDOWN_DISABLE;
  rc = igpio_pull_mode_to_esp(config->pull_mode, &esp_pull_mode, &esp_pullup, &esp_pulldown);
  if (rc != IGPIO_ERR_NONE) {
    return rc;
  }

  gpio_int_type_t esp_intr_type = GPIO_INTR_DISABLE;
  rc = igpio_intr_type_to_esp(config->intr_type, &esp_intr_type);
  if (rc != IGPIO_ERR_NONE) {
    return rc;
  }

  gpio_config_t esp_config = {
      .pin_bit_mask = 1ULL << config->io_num,
      .mode = esp_mode,
      .pull_up_en = esp_pullup,
      .pull_down_en = esp_pulldown,
      .intr_type = esp_intr_type,
  };

  return esp_err_to_igpio(gpio_config(&esp_config));
}

int32_t igpio_reset_pin(int32_t io_num) {
  if (!igpio_is_valid_pin(io_num)) {
    return IGPIO_ERR_INVALID_ARG;
  }

  return esp_err_to_igpio(gpio_reset_pin((gpio_num_t)io_num));
}

int32_t igpio_set_mode(int32_t io_num, igpio_mode_t mode) {
  if (!igpio_is_valid_pin(io_num)) {
    return IGPIO_ERR_INVALID_ARG;
  }
  if (igpio_mode_requires_output(mode) && !igpio_is_valid_output_pin(io_num)) {
    return IGPIO_ERR_INVALID_ARG;
  }

  gpio_mode_t esp_mode = GPIO_MODE_DISABLE;
  int32_t rc = igpio_mode_to_esp(mode, &esp_mode);
  if (rc != IGPIO_ERR_NONE) {
    return rc;
  }

  return esp_err_to_igpio(gpio_set_direction((gpio_num_t)io_num, esp_mode));
}

int32_t igpio_set_pull_mode(int32_t io_num, igpio_pull_mode_t pull_mode) {
  if (!igpio_is_valid_pin(io_num)) {
    return IGPIO_ERR_INVALID_ARG;
  }

  gpio_pull_mode_t esp_pull_mode = GPIO_FLOATING;
  gpio_pullup_t esp_pullup = GPIO_PULLUP_DISABLE;
  gpio_pulldown_t esp_pulldown = GPIO_PULLDOWN_DISABLE;
  int32_t rc = igpio_pull_mode_to_esp(pull_mode, &esp_pull_mode, &esp_pullup, &esp_pulldown);
  if (rc != IGPIO_ERR_NONE) {
    return rc;
  }

  return esp_err_to_igpio(gpio_set_pull_mode((gpio_num_t)io_num, esp_pull_mode));
}

int32_t igpio_set_intr_type(int32_t io_num, igpio_intr_type_t intr_type) {
  if (!igpio_is_valid_pin(io_num)) {
    return IGPIO_ERR_INVALID_ARG;
  }

  gpio_int_type_t esp_intr_type = GPIO_INTR_DISABLE;
  int32_t rc = igpio_intr_type_to_esp(intr_type, &esp_intr_type);
  if (rc != IGPIO_ERR_NONE) {
    return rc;
  }

  return esp_err_to_igpio(gpio_set_intr_type((gpio_num_t)io_num, esp_intr_type));
}

int32_t igpio_set_level(int32_t io_num, bool level) {
  if (!igpio_is_valid_output_pin(io_num)) {
    return IGPIO_ERR_INVALID_ARG;
  }

  return esp_err_to_igpio(gpio_set_level((gpio_num_t)io_num, level ? 1U : 0U));
}

int32_t igpio_get_level(int32_t io_num, bool *out_level) {
  if (!igpio_is_valid_pin(io_num) || out_level == NULL) {
    return IGPIO_ERR_INVALID_ARG;
  }

  *out_level = gpio_get_level((gpio_num_t)io_num) != 0;
  return IGPIO_ERR_NONE;
}
