/**
 * @file aw9523b.h
 * @brief Public AW9523B GPIO-expander helpers built on abstract transport callbacks.
 * @ingroup aw9523b
 */
/**
 * @defgroup aw9523b AW9523B
 * @brief Public AW9523B GPIO-expander helpers built on abstract transport callbacks.
 *
 * This component owns AW9523B register communication and register decoding
 * only. Applications provide callbacks that write register bytes and perform a
 * combined write-then-read transaction.
 *
 * The public API uses `port` values `0` and `1` for the two 8-bit GPIO banks,
 * and `pin` values `0` through `7` for one bit inside a port.
 * @{
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Success return code. */
#define AW9523B_ERR_NONE 0
/** @brief Base value for AW9523B-specific error codes. */
#define AW9523B_ERR_BASE 0x3500
#define AW9523B_ERR_FAIL (AW9523B_ERR_BASE + 1)          /*!< Generic failure */
#define AW9523B_ERR_NO_MEM (AW9523B_ERR_BASE + 2)        /*!< Out of memory */
#define AW9523B_ERR_INVALID_ARG (AW9523B_ERR_BASE + 3)   /*!< Invalid argument */
#define AW9523B_ERR_INVALID_STATE (AW9523B_ERR_BASE + 4) /*!< Invalid state */
#define AW9523B_ERR_NOT_FOUND (AW9523B_ERR_BASE + 5)     /*!< Resource not found */
#define AW9523B_ERR_NOT_SUPPORTED (AW9523B_ERR_BASE + 6) /*!< Operation not supported */
#define AW9523B_ERR_TIMEOUT (AW9523B_ERR_BASE + 7)       /*!< Operation timed out */
#define AW9523B_ERR_IO (AW9523B_ERR_BASE + 8)            /*!< I/O failure */

/**
 * @brief Callback that writes raw bytes to the expander.
 *
 * The callback is used for direct register writes. The return value is passed
 * through unchanged.
 */
typedef int32_t (*aw9523b_transport_write)(const uint8_t *write_buffer, size_t write_size);

/**
 * @brief Callback that performs a combined write-then-read transaction.
 *
 * The callback should send `write_buffer`, then read up to `read_capacity`
 * bytes into `read_buffer`, and finally store the actual byte count in
 * `*read_size`.
 */
typedef int32_t (*aw9523b_transport_write_read)(const uint8_t *write_buffer,
                                                size_t write_size,
                                                uint8_t *read_buffer,
                                                size_t *read_size,
                                                size_t read_capacity);

typedef struct aw9523b aw9523b_t;
/** @brief Transport callback set used by the AW9523B helper functions. */
struct aw9523b {
  aw9523b_transport_write transport_write;
  aw9523b_transport_write_read transport_write_read;
};

/**
 * @brief Return a stable string for an AW9523B component error code.
 *
 * This helper only decodes `AW9523B_ERR_*` values. Transport-specific error
 * codes returned by application callbacks are reported as
 * `"AW9523B_ERR_UNKNOWN"`.
 */
const char *aw9523b_err_to_name(int32_t err);

/**
 * @brief Port 0 output drive modes controlled by `AW9523B_REG_GCR`.
 *
 * Only port 0 exposes a configurable drive-mode bit in the current public API.
 */
typedef enum aw9523b_port0_drive_mode {
  /** @brief Port 0 outputs behave as open-drain drivers. */
  AW9523B_PORT0_DRIVE_MODE_OPEN_DRAIN = 0,
  /** @brief Port 0 outputs actively drive both logic high and logic low. */
  AW9523B_PORT0_DRIVE_MODE_PUSH_PULL = 1,
} aw9523b_port0_drive_mode_t;

/** @brief Single-pin direction choices used by `aw9523b_port_dir_set()`. */
typedef enum aw9523b_port_direction {
  /** @brief Configure the selected pin as an output. */
  AW9523B_PORT_DIRECTION_OUTPUT = 0,
  /** @brief Configure the selected pin as an input. */
  AW9523B_PORT_DIRECTION_INPUT = 1,
} aw9523b_port_direction_t;

/** @name Raw Register Access */
/** @{ */

int32_t aw9523b_reg8_write(aw9523b_t *expander, uint8_t reg, uint8_t value);
int32_t aw9523b_reg8_read(aw9523b_t *expander, uint8_t reg, uint8_t *out_value);
int32_t aw9523b_reg8_set_bits(aw9523b_t *expander, uint8_t reg, uint8_t bits);
int32_t aw9523b_reg8_update_bits(aw9523b_t *expander, uint8_t reg, uint8_t mask, uint8_t new_value);

/** @} */

/** @name Identification */
/** @{ */

/**
 * @brief Read the AW9523B identification register.
 *
 * @return `AW9523B_ERR_NONE` on success, `AW9523B_ERR_INVALID_ARG` when
 * `out` is `NULL`, or a pass-through transport error.
 */
int32_t aw9523b_id_get(aw9523b_t *expander, uint8_t *out);

/** @} */

/** @name Drive Mode Control */
/** @{ */

int32_t aw9523b_port0_drive_mode_get(aw9523b_t *expander, aw9523b_port0_drive_mode_t *out_mode);
int32_t aw9523b_port0_drive_mode_set(aw9523b_t *expander, aw9523b_port0_drive_mode_t mode);

/** @} */

/** @name Direction Control */
/** @{ */

int32_t aw9523b_port_dir_bits_get(aw9523b_t *expander, uint8_t port, uint8_t *out_bits);
int32_t aw9523b_port_dir_bits_set(aw9523b_t *expander, uint8_t port, uint8_t bits);
int32_t aw9523b_port_dir_bits_update(aw9523b_t *expander, uint8_t port, uint8_t mask, uint8_t bits);
int32_t aw9523b_port_dir_set(aw9523b_t *expander,
                             uint8_t port,
                             uint8_t pin,
                             aw9523b_port_direction_t direction);

/** @} */

/** @name Interrupt Control */
/** @{ */

int32_t aw9523b_port_interrupt_bits_get(aw9523b_t *expander, uint8_t port, uint8_t *out_bits);
int32_t aw9523b_port_interrupt_bits_set(aw9523b_t *expander, uint8_t port, uint8_t bits);
int32_t aw9523b_port_interrupt_bits_update(aw9523b_t *expander,
                                           uint8_t port,
                                           uint8_t mask,
                                           uint8_t bits);
int32_t aw9523b_interrupt_set(aw9523b_t *expander, uint8_t port, uint8_t pin, bool enabled);
int32_t aw9523b_interrupt_get(aw9523b_t *expander, uint8_t port, uint8_t pin, uint8_t *out_enabled);

/** @} */

/** @name Level I/O */
/** @{ */

int32_t aw9523b_port_input_read(aw9523b_t *expander, uint8_t port, uint8_t *input_value);
int32_t aw9523b_port_output_read(aw9523b_t *expander, uint8_t port, uint8_t *output_value);
int32_t aw9523b_level_set(aw9523b_t *expander, uint8_t port, uint8_t pin, uint8_t level);
int32_t aw9523b_level_get(aw9523b_t *expander, uint8_t port, uint8_t pin, uint8_t *out_level);

/** @} */

#ifdef __cplusplus
}
#endif

/** @} */
