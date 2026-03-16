/**
 * @file igpio.h
 * @brief Minimal GPIO wrapper API used by project components.
 * @ingroup igpio
 */
/**
 * @defgroup igpio IGPIO
 * @brief Minimal GPIO wrapper API used by project components.
 * @{
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Success return code. */
#define IGPIO_ERR_NONE 0
/** @brief Base value for IGPIO-specific error codes. */
#define IGPIO_ERR_BASE 0x3200
#define IGPIO_ERR_FAIL (IGPIO_ERR_BASE + 1)          /*!< Generic failure */
#define IGPIO_ERR_NO_MEM (IGPIO_ERR_BASE + 2)        /*!< Out of memory */
#define IGPIO_ERR_INVALID_ARG (IGPIO_ERR_BASE + 3)   /*!< Invalid argument */
#define IGPIO_ERR_INVALID_STATE (IGPIO_ERR_BASE + 4) /*!< Invalid state */
#define IGPIO_ERR_NOT_FOUND (IGPIO_ERR_BASE + 5)     /*!< Resource not found */
#define IGPIO_ERR_NOT_SUPPORTED (IGPIO_ERR_BASE + 6) /*!< Operation not supported */
#define IGPIO_ERR_TIMEOUT (IGPIO_ERR_BASE + 7)       /*!< Operation timed out */
#define IGPIO_ERR_IO (IGPIO_ERR_BASE + 8)            /*!< I/O failure */

/**
 * @brief Supported GPIO pin modes.
 */
typedef enum {
  IGPIO_MODE_DISABLED = 0,         /**< Disable both input and output. */
  IGPIO_MODE_INPUT = 1,            /**< Enable input only. */
  IGPIO_MODE_OUTPUT = 2,           /**< Enable output only. */
  IGPIO_MODE_INPUT_OUTPUT = 3,     /**< Enable both input and output. */
  IGPIO_MODE_OUTPUT_OPEN_DRAIN = 4, /**< Enable open-drain output only. */
  IGPIO_MODE_INPUT_OUTPUT_OPEN_DRAIN = 5, /**< Enable input and open-drain output. */
} igpio_mode_t;

/**
 * @brief Supported GPIO internal pull resistor modes.
 */
typedef enum {
  IGPIO_PULL_FLOATING = 0,      /**< Disable pull-up and pull-down. */
  IGPIO_PULL_UP = 1,            /**< Enable pull-up only. */
  IGPIO_PULL_DOWN = 2,          /**< Enable pull-down only. */
  IGPIO_PULL_UP_DOWN = 3,       /**< Enable both pull-up and pull-down. */
} igpio_pull_mode_t;

/**
 * @brief Supported GPIO interrupt trigger types.
 */
typedef enum {
  IGPIO_INTR_DISABLED = 0,    /**< Disable interrupts for the pin. */
  IGPIO_INTR_POSEDGE = 1,     /**< Interrupt on rising edge. */
  IGPIO_INTR_NEGEDGE = 2,     /**< Interrupt on falling edge. */
  IGPIO_INTR_ANYEDGE = 3,     /**< Interrupt on either edge. */
  IGPIO_INTR_LOW_LEVEL = 4,   /**< Interrupt while input is low. */
  IGPIO_INTR_HIGH_LEVEL = 5,  /**< Interrupt while input is high. */
} igpio_intr_type_t;

/**
 * @brief GPIO configuration.
 */
typedef struct {
  int32_t io_num;              /**< GPIO number, or -1 until set by the caller. */
  igpio_mode_t mode;           /**< Direction and open-drain mode. */
  igpio_pull_mode_t pull_mode; /**< Internal pull resistor mode. */
  igpio_intr_type_t intr_type; /**< Interrupt trigger mode. */
} igpio_config_t;

/**
 * @brief Return a stable string for an IGPIO error code.
 */
const char *igpio_err_to_name(int32_t err);

/**
 * @brief Fill a GPIO config with safe defaults.
 */
void igpio_get_default_config(igpio_config_t *config);

/**
 * @brief Configure one GPIO pin.
 *
 * @return `IGPIO_ERR_NONE` on success, or an `IGPIO_ERR_*` code on failure.
 */
int32_t igpio_configure(const igpio_config_t *config);

/**
 * @brief Reset one GPIO pin to backend defaults.
 *
 * @return `IGPIO_ERR_NONE` on success, or an `IGPIO_ERR_*` code on failure.
 */
int32_t igpio_reset_pin(int32_t io_num);

/**
 * @brief Change the mode of one GPIO pin.
 *
 * @return `IGPIO_ERR_NONE` on success, or an `IGPIO_ERR_*` code on failure.
 */
int32_t igpio_set_mode(int32_t io_num, igpio_mode_t mode);

/**
 * @brief Change the pull mode of one GPIO pin.
 *
 * @return `IGPIO_ERR_NONE` on success, or an `IGPIO_ERR_*` code on failure.
 */
int32_t igpio_set_pull_mode(int32_t io_num, igpio_pull_mode_t pull_mode);

/**
 * @brief Change the interrupt trigger type of one GPIO pin.
 *
 * @return `IGPIO_ERR_NONE` on success, or an `IGPIO_ERR_*` code on failure.
 */
int32_t igpio_set_intr_type(int32_t io_num, igpio_intr_type_t intr_type);

/**
 * @brief Drive one GPIO output to a logic level.
 *
 * @return `IGPIO_ERR_NONE` on success, or an `IGPIO_ERR_*` code on failure.
 */
int32_t igpio_set_level(int32_t io_num, bool level);

/**
 * @brief Read one GPIO input level.
 *
 * @return `IGPIO_ERR_NONE` on success, or an `IGPIO_ERR_*` code on failure.
 */
int32_t igpio_get_level(int32_t io_num, bool *out_level);

#ifdef __cplusplus
}
#endif

/** @} */  // end of igpio
