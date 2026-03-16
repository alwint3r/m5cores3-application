/**
 * @file aw9523b.h
 * @brief Public AW9523B GPIO-expander helpers built on top of `ii2c`.
 * @ingroup aw9523b
 */
/**
 * @defgroup aw9523b AW9523B
 * @brief Public AW9523B GPIO-expander helpers built on top of `ii2c`.
 *
 * This component does not define a dedicated `aw9523b` handle type. Callers
 * first attach the AW9523B as an `ii2c_device_handle_t`, then pass that handle
 * to these helper functions.
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
#include <stdint.h>
#include <ii2c/ii2c.h>

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

/**
 * @brief Write one byte to an AW9523B register.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param reg Register address to write.
 * @param value Byte value to store in `reg`.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_reg8_write(ii2c_device_handle_t dev, uint8_t reg, uint8_t value);

/**
 * @brief Read one byte from an AW9523B register.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param reg Register address to read.
 * @param out_value Output pointer that receives the register byte.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_reg8_read(ii2c_device_handle_t dev, uint8_t reg, uint8_t *out_value);

/**
 * @brief Set selected bits in an 8-bit register.
 *
 * This helper performs a read-modify-write cycle so bits outside `bits` are
 * preserved.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param reg Register address to update.
 * @param bits Bit mask of the bits to force high.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_reg8_set_bits(ii2c_device_handle_t dev, uint8_t reg, uint8_t bits);

/**
 * @brief Replace selected bits in an 8-bit register.
 *
 * Bits covered by `mask` are taken from `new_value`. Bits outside `mask` are
 * preserved from the current register value.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param reg Register address to update.
 * @param mask Bit mask that selects which register bits to rewrite.
 * @param new_value Replacement value for the masked bits.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_reg8_update_bits(ii2c_device_handle_t dev,
                                 uint8_t reg,
                                 uint8_t mask,
                                 uint8_t new_value);

/** @} */

/** @name Identification */
/** @{ */

/**
 * @brief Read the AW9523B identification register.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param out Output pointer that receives the ID byte from `AW9523B_REG_ID`.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `out` is
 * `NULL`, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_id_get(ii2c_device_handle_t dev, uint8_t *out);

/** @} */

/** @name Drive Mode Control */
/** @{ */

/**
 * @brief Read the configured port-0 output drive mode.
 *
 * This helper decodes `AW9523B_REG_GCR` bit 4 into
 * `AW9523B_PORT0_DRIVE_MODE_OPEN_DRAIN` or
 * `AW9523B_PORT0_DRIVE_MODE_PUSH_PULL`.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param out_mode Output pointer that receives the decoded drive mode.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `out_mode`
 * is `NULL`, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_port0_drive_mode_get(ii2c_device_handle_t dev,
                                     aw9523b_port0_drive_mode_t *out_mode);

/**
 * @brief Set the port-0 output drive mode.
 *
 * Only port 0 is configurable through this helper because the public driver
 * currently exposes drive-mode control only for `AW9523B_REG_GCR` bit 4.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param mode Requested port-0 drive mode.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `mode` is
 * not one of the exported enum values, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_port0_drive_mode_set(ii2c_device_handle_t dev, aw9523b_port0_drive_mode_t mode);

/** @} */

/** @name Direction Control */
/** @{ */

/**
 * @brief Read all eight direction bits for one port.
 *
 * The returned byte mirrors `AW9523B_REG_CONFIG0 + port`, where `1` means the
 * pin is configured as input and `0` means the pin is configured as output.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param port Port index to read: `0` or `1`.
 * @param out_bits Output pointer that receives the 8-bit direction mask.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `port` is
 * out of range or `out_bits` is `NULL`, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_port_dir_bits_get(ii2c_device_handle_t dev, uint8_t port, uint8_t *out_bits);

/**
 * @brief Write all eight direction bits for one port.
 *
 * Each `1` bit configures the matching pin as input. Each `0` bit configures
 * the matching pin as output.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param port Port index to update: `0` or `1`.
 * @param bits New 8-bit direction mask to store.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `port` is
 * out of range, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_port_dir_bits_set(ii2c_device_handle_t dev, uint8_t port, uint8_t bits);

/**
 * @brief Update selected direction bits for one port.
 *
 * Bits covered by `mask` are taken from `bits`. A masked bit value of `1`
 * means input, and `0` means output.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param port Port index to update: `0` or `1`.
 * @param mask Bit mask that selects which pins to rewrite.
 * @param bits Replacement direction bits for the masked pins.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `port` is
 * out of range, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_port_dir_bits_update(ii2c_device_handle_t dev,
                                     uint8_t port,
                                     uint8_t mask,
                                     uint8_t bits);

/**
 * @brief Set the direction of one pin.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param port Port index to update: `0` or `1`.
 * @param pin Pin index inside the port: `0` through `7`.
 * @param direction Requested pin direction. Use
 * `AW9523B_PORT_DIRECTION_OUTPUT` or `AW9523B_PORT_DIRECTION_INPUT`.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `port` or
 * `pin` is out of range, when `direction` is not one of the exported enum
 * values, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_port_dir_set(ii2c_device_handle_t dev,
                             uint8_t port,
                             uint8_t pin,
                             aw9523b_port_direction_t direction);

/** @} */

/** @name Interrupt Control */
/** @{ */

/**
 * @brief Read the interrupt-enable state for one port.
 *
 * The returned byte uses positive enable semantics: a `1` bit means interrupts
 * are enabled for that pin, even though the raw `AW9523B_REG_INTENABLE0 +
 * port` register stores the inverse value.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param port Port index to read: `0` or `1`.
 * @param out_bits Output pointer that receives the enabled-bit mask.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `port` is
 * out of range or `out_bits` is `NULL`, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_port_interrupt_bits_get(ii2c_device_handle_t dev, uint8_t port, uint8_t *out_bits);

/**
 * @brief Write the interrupt-enable state for one port.
 *
 * The `bits` argument uses positive enable semantics: a `1` bit enables the
 * interrupt for that pin and a `0` bit disables it.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param port Port index to update: `0` or `1`.
 * @param bits New enabled-bit mask for the selected port.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `port` is
 * out of range, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_port_interrupt_bits_set(ii2c_device_handle_t dev, uint8_t port, uint8_t bits);

/**
 * @brief Update selected interrupt-enable bits for one port.
 *
 * The `bits` argument uses positive enable semantics for the pins selected by
 * `mask`.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param port Port index to update: `0` or `1`.
 * @param mask Bit mask that selects which pins to rewrite.
 * @param bits Replacement enabled bits for the masked pins.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `port` is
 * out of range, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_port_interrupt_bits_update(ii2c_device_handle_t dev,
                                           uint8_t port,
                                           uint8_t mask,
                                           uint8_t bits);

/**
 * @brief Enable or disable interrupt reporting for one pin.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param port Port index to update: `0` or `1`.
 * @param pin Pin index inside the port: `0` through `7`.
 * @param enabled Set `true` to enable interrupts or `false` to disable them.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `port` or
 * `pin` is out of range, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_interrupt_set(ii2c_device_handle_t dev, uint8_t port, uint8_t pin, bool enabled);

/**
 * @brief Read the interrupt-enable state for one pin.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param port Port index to read: `0` or `1`.
 * @param pin Pin index inside the port: `0` through `7`.
 * @param out_enabled Output pointer that receives `1` when interrupts are
 * enabled for the selected pin or `0` when they are disabled.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `port`,
 * `pin`, or `out_enabled` is invalid, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_interrupt_get(ii2c_device_handle_t dev,
                              uint8_t port,
                              uint8_t pin,
                              uint8_t *out_enabled);

/** @} */

/** @name Level I/O */
/** @{ */

/**
 * @brief Write the output latch for one pin.
 *
 * This helper updates `AW9523B_REG_OUTPUT0 + port`. Any non-zero `level` is
 * treated as logic high, and zero is treated as logic low.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param port Port index to update: `0` or `1`.
 * @param pin Pin index inside the port: `0` through `7`.
 * @param level Requested output level; zero means low and non-zero means high.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `port` or
 * `pin` is out of range, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_level_set(ii2c_device_handle_t dev, uint8_t port, uint8_t pin, uint8_t level);

/**
 * @brief Read the current input level for one pin.
 *
 * This helper reads `AW9523B_REG_INPUT0 + port` and normalizes the selected
 * bit to `1` for logic high or `0` for logic low.
 *
 * @param dev Attached `ii2c` device handle for the AW9523B.
 * @param port Port index to read: `0` or `1`.
 * @param pin Pin index inside the port: `0` through `7`.
 * @param out_level Output pointer that receives `1` for logic high or `0` for
 * logic low.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `port`,
 * `pin`, or `out_level` is invalid, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t aw9523b_level_get(ii2c_device_handle_t dev, uint8_t port, uint8_t pin, uint8_t *out_level);

/** @} */

#ifdef __cplusplus
}
#endif

/** @} */
