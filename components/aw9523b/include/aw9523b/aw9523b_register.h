/**
 * @file aw9523b_register.h
 * @brief Public AW9523B register addresses and bit masks exposed by this component.
 * @ingroup aw9523b
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Base input-state register for port 0. Use `AW9523B_REG_INPUT0 + port`. */
#define AW9523B_REG_INPUT0 0x00
/** @brief Base output-latch register for port 0. Use `AW9523B_REG_OUTPUT0 + port`. */
#define AW9523B_REG_OUTPUT0 0x02
/**
 * @brief Base direction register for port 0.
 *
 * Use `AW9523B_REG_CONFIG0 + port`. In this register family, a `1` bit means
 * input mode and a `0` bit means output mode.
 */
#define AW9523B_REG_CONFIG0 0x04
/**
 * @brief Base interrupt-enable register for port 0.
 *
 * Use `AW9523B_REG_INTENABLE0 + port`. The raw hardware register stores the
 * inverse of the public API's enabled-bit view used by
 * `aw9523b_port_interrupt_*()`.
 */
#define AW9523B_REG_INTENABLE0 0x06
/** @brief Identification register read by `aw9523b_id_get()`. */
#define AW9523B_REG_ID 0x10
/** @brief Global control register used by `aw9523b_port0_drive_mode_*()`. */
#define AW9523B_REG_GCR 0x11

/** @brief Mask for the port-0 drive-mode bit in `AW9523B_REG_GCR`. */
#define AW9523B_GCR_PORT0_DRIVE_MODE_MASK 0x10
/** @brief Raw `AW9523B_REG_GCR` value for port-0 open-drain mode. */
#define AW9523B_GCR_PORT0_DRIVE_MODE_OPEN_DRAIN 0x00
/** @brief Raw `AW9523B_REG_GCR` value for port-0 push-pull mode. */
#define AW9523B_GCR_PORT0_DRIVE_MODE_PUSH_PULL 0x10

#ifdef __cplusplus
}
#endif
