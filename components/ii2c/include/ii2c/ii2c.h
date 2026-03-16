/**
 * @file ii2c.h
 * @brief Minimal I2C master wrapper API used by project components.
 * @ingroup ii2c
 */
/**
 * @defgroup ii2c II2C
 * @brief Minimal I2C master wrapper API used by project components.
 * @{
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Success return code. */
#define II2C_ERR_NONE 0
/** @brief Base value for II2C-specific error codes. */
#define II2C_ERR_BASE 0x3000
#define II2C_ERR_FAIL (II2C_ERR_BASE + 1)          /*!< Generic failure */
#define II2C_ERR_NO_MEM (II2C_ERR_BASE + 2)        /*!< Out of memory */
#define II2C_ERR_INVALID_ARG (II2C_ERR_BASE + 3)   /*!< Invalid argument */
#define II2C_ERR_INVALID_STATE (II2C_ERR_BASE + 4) /*!< Invalid state */
#define II2C_ERR_NOT_FOUND (II2C_ERR_BASE + 5)     /*!< Resource not found */
#define II2C_ERR_NOT_SUPPORTED (II2C_ERR_BASE + 6) /*!< Operation not supported */
#define II2C_ERR_TIMEOUT (II2C_ERR_BASE + 7)       /*!< Operation timed out */
#define II2C_ERR_IO (II2C_ERR_BASE + 8)            /*!< I/O failure */

/** @brief Use backend auto-selection for the bus port number. */
#define II2C_PORT_AUTO (-1)
/** @brief Default synchronous transfer timeout in milliseconds. */
#define II2C_DEFAULT_TIMEOUT_MS 1000

/**
 * @brief I2C clock source selection.
 *
 * The available selections depend on the active ESP target. If a caller
 * requests a source that the current target does not expose, bus creation
 * fails with `II2C_ERR_NOT_SUPPORTED`.
 */
typedef enum {
  II2C_CLK_SRC_DEFAULT = 0, /**< Use backend default clock source. */
  II2C_CLK_SRC_APB = 1,     /**< Use the APB clock when supported by the backend. */
  II2C_CLK_SRC_XTAL = 2,    /**< Use the XTAL clock when supported by the backend. */
  II2C_CLK_SRC_RC_FAST = 3, /**< Use the RC_FAST clock when supported by the backend. */
} ii2c_clock_source_t;

/**
 * @brief Master-bus configuration.
 */
typedef struct {
  int32_t port_num;            /**< I2C controller/port number, or II2C_PORT_AUTO. */
  int32_t sda_io_num;          /**< SDA GPIO number. */
  int32_t scl_io_num;          /**< SCL GPIO number. */
  ii2c_clock_source_t clk_src; /**< Clock source selection, subject to target support. */
  uint8_t glitch_ignore_cnt;   /**< Glitch filter threshold in backend clock cycles. */
  bool enable_internal_pullup; /**< Enable weak internal pull-ups on SDA and SCL. */
} ii2c_master_bus_config_t;

/**
 * @brief Attached device configuration.
 */
typedef struct {
  uint16_t device_address; /**< Raw 7-bit I2C device address. */
  uint32_t scl_speed_hz;   /**< SCL bus speed for this device. */
  int32_t timeout_ms;      /**< Transfer timeout for synchronous API calls. */
} ii2c_device_config_t;

/** @brief Opaque master-bus handle. */
typedef struct ii2c_master_bus_t *ii2c_master_bus_handle_t;
/** @brief Opaque attached-device handle. */
typedef struct ii2c_device_t *ii2c_device_handle_t;

/**
 * @brief Return a stable string for an II2C error code.
 */
const char *ii2c_err_to_name(int32_t err);

/**
 * @brief Fill a master-bus config with safe defaults.
 */
void ii2c_get_default_master_bus_config(ii2c_master_bus_config_t *config);

/**
 * @brief Fill a device config with safe defaults.
 */
void ii2c_get_default_device_config(ii2c_device_config_t *config);

/**
 * @brief Create a new I2C master bus.
 *
 * If `config->clk_src` is not supported by the active ESP target, this
 * function returns `II2C_ERR_NOT_SUPPORTED`.
 *
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code on failure.
 */
int32_t ii2c_new_master_bus(const ii2c_master_bus_config_t *config,
                            ii2c_master_bus_handle_t *ret_bus);
/**
 * @brief Delete a master-bus handle.
 *
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code on failure.
 */
int32_t ii2c_del_master_bus(ii2c_master_bus_handle_t bus);

/**
 * @brief Attach a new device to an existing I2C master bus.
 *
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code on failure.
 */
int32_t ii2c_new_device(ii2c_master_bus_handle_t bus,
                        const ii2c_device_config_t *config,
                        ii2c_device_handle_t *ret_device);
/**
 * @brief Delete an attached device handle.
 *
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code on failure.
 */
int32_t ii2c_del_device(ii2c_device_handle_t device);

/**
 * @brief Probe a 7-bit device address on an existing I2C master bus.
 *
 * If the address responds with ACK, this function returns `II2C_ERR_NONE`.
 * If no device responds, this function returns `II2C_ERR_NOT_FOUND`.
 * Passing `timeout_ms == 0` uses `II2C_DEFAULT_TIMEOUT_MS`.
 *
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code on failure.
 */
int32_t ii2c_master_probe(ii2c_master_bus_handle_t bus,
                          uint16_t device_address,
                          int32_t timeout_ms);

/**
 * @brief Perform a synchronous write transaction to a device.
 *
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code on failure.
 */
int32_t ii2c_master_transmit(ii2c_device_handle_t device,
                             const uint8_t *write_buffer,
                             size_t write_size);

/**
 * @brief Perform a synchronous combined write-then-read transaction.
 *
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code on failure.
 */
int32_t ii2c_master_transmit_receive(ii2c_device_handle_t device,
                                     const uint8_t *write_buffer,
                                     size_t write_size,
                                     uint8_t *read_buffer,
                                     size_t read_size);

#ifdef __cplusplus
}
#endif

/** @} */  // end of ii2c
