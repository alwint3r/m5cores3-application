/**
 * @file ft6x36.h
 * @brief Public FT6X36 touch-controller helpers built on abstract transport callbacks.
 * @ingroup ft6x36
 */
/**
 * @defgroup ft6x36 FT6X36
 * @brief Public FT6X36 touch-controller helpers built on abstract transport callbacks.
 *
 * This component owns FT6X36 register communication and register decoding
 * only. Applications provide callbacks plus an opaque transport context that
 * write register bytes and perform a combined write-then-read transaction.
 * Board-level reset, wake, and interrupt GPIO control remain
 * application-owned.
 * @{
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ft6x36/ft6x36_register.h"

/** @brief Success return code. */
#define FT6X36_ERR_NONE 0
/** @brief Base value for FT6X36-specific error codes. */
#define FT6X36_ERR_BASE 0x3300
#define FT6X36_ERR_FAIL (FT6X36_ERR_BASE + 1)          /*!< Generic failure. */
#define FT6X36_ERR_NO_MEM (FT6X36_ERR_BASE + 2)        /*!< Out-of-memory failure. */
#define FT6X36_ERR_INVALID_ARG (FT6X36_ERR_BASE + 3)   /*!< Invalid function argument. */
#define FT6X36_ERR_INVALID_STATE (FT6X36_ERR_BASE + 4) /*!< Invalid state. */
#define FT6X36_ERR_NOT_FOUND (FT6X36_ERR_BASE + 5)     /*!< Requested item was not found. */
#define FT6X36_ERR_NOT_SUPPORTED (FT6X36_ERR_BASE + 6) /*!< Operation is not supported. */
#define FT6X36_ERR_TIMEOUT (FT6X36_ERR_BASE + 7)       /*!< Operation timed out. */
#define FT6X36_ERR_IO (FT6X36_ERR_BASE + 8)            /*!< I/O or short-transfer failure. */

/** @brief Maximum number of simultaneous touch points decoded by this driver. */
#define FT6X36_TOUCH_POINTS_MAX 2
/** @brief Reserved touch ID reported for an unused touch-point slot. */
#define FT6X36_TOUCH_POINT_ID_INVALID 0x0F

/**
 * @brief Callback that writes raw bytes to the controller.
 *
 * The callback is used for direct register writes. The return value is passed
 * through unchanged.
 */
typedef int32_t (*ft6x36_transport_write)(void *context,
                                          const uint8_t *write_buffer,
                                          size_t write_size);

/**
 * @brief Callback that performs a combined write-then-read transaction.
 *
 * The callback should send `write_buffer`, then read up to `read_capacity`
 * bytes into `read_buffer`, and finally store the actual byte count in
 * `*read_size`.
 */
typedef int32_t (*ft6x36_transport_write_read)(void *context,
                                               const uint8_t *write_buffer,
                                               size_t write_size,
                                               uint8_t *read_buffer,
                                               size_t *read_size,
                                               size_t read_capacity);

typedef struct ft6x36 ft6x36_t;
/** @brief Transport callback set used by the FT6X36 helper functions. */
struct ft6x36 {
  /** @brief Opaque caller-owned transport context passed back to callbacks. */
  void *transport_context;
  /** @brief Raw write callback used by register writes. */
  ft6x36_transport_write transport_write;
  /** @brief Required combined write-then-read callback for register reads. */
  ft6x36_transport_write_read transport_write_read;
};

/** @name Error Helpers */
/** @{ */

/**
 * @brief Return a stable string for an FT6X36 component error code.
 *
 * This helper only decodes `FT6X36_ERR_*` values. Transport-specific error
 * codes returned by application callbacks are reported as
 * `"FT6X36_ERR_UNKNOWN"`.
 */
const char *ft6x36_err_to_name(int32_t err);

/** @} */

/** @brief FT6X36 device-mode values decoded from `FT6X36_REG_DEV_MODE`. */
typedef enum ft6x36_device_mode {
  /** @brief Normal touch-sensing working mode. */
  FT6X36_DEVICE_MODE_WORKING = 0x00,
  /** @brief Factory or test mode. */
  FT6X36_DEVICE_MODE_FACTORY = 0x04,
} ft6x36_device_mode_t;

/** @brief FT6X36 control-mode values stored in `FT6X36_REG_CTRL`. */
typedef enum ft6x36_ctrl_mode {
  /** @brief Active scanning mode. */
  FT6X36_CTRL_MODE_ACTIVE = 0x00,
  /** @brief Lower-power monitor mode. */
  FT6X36_CTRL_MODE_MONITOR = 0x01,
} ft6x36_ctrl_mode_t;

/** @brief Compatibility alias for `FT6X36_CTRL_MODE_ACTIVE`. */
#define FT6x36_CTRL_MODE_ACTIVE FT6X36_CTRL_MODE_ACTIVE
/** @brief Compatibility alias for `FT6X36_CTRL_MODE_MONITOR`. */
#define FT6x36_CTRL_MODE_MONITOR FT6X36_CTRL_MODE_MONITOR

/** @brief FT6X36 interrupt-reporting mode values stored in `FT6X36_REG_G_MODE`. */
typedef enum ft6x36_interrupt_mode {
  /** @brief Poll the controller for touch data. */
  FT6X36_INTERRUPT_MODE_POLLING = 0x00,
  /** @brief Trigger the interrupt output when new data is ready. */
  FT6X36_INTERRUPT_MODE_TRIGGER = 0x01,
} ft6x36_interrupt_mode_t;

/** @brief Gesture IDs reported by `FT6X36_REG_GEST_ID`. */
typedef enum ft6x36_gesture_id {
  /** @brief No gesture is currently reported. */
  FT6X36_GESTURE_ID_NONE = 0x00,
  /** @brief Upward swipe gesture. */
  FT6X36_GESTURE_ID_MOVE_UP = 0x10,
  /** @brief Rightward swipe gesture. */
  FT6X36_GESTURE_ID_MOVE_RIGHT = 0x14,
  /** @brief Downward swipe gesture. */
  FT6X36_GESTURE_ID_MOVE_DOWN = 0x18,
  /** @brief Leftward swipe gesture. */
  FT6X36_GESTURE_ID_MOVE_LEFT = 0x1C,
  /** @brief Zoom-in gesture. */
  FT6X36_GESTURE_ID_ZOOM_IN = 0x48,
  /** @brief Zoom-out gesture. */
  FT6X36_GESTURE_ID_ZOOM_OUT = 0x49,
} ft6x36_gesture_id_t;

/** @brief Touch-event values decoded from `Pn_XH` bits 7:6. */
typedef enum ft6x36_touch_event {
  /** @brief New touch press detected. */
  FT6X36_TOUCH_EVENT_PRESS_DOWN = 0x00,
  /** @brief Existing touch lifted. */
  FT6X36_TOUCH_EVENT_LIFT_UP = 0x01,
  /** @brief Existing touch remains in contact. */
  FT6X36_TOUCH_EVENT_CONTACT = 0x02,
  /** @brief No valid event is present in this slot. */
  FT6X36_TOUCH_EVENT_NO_EVENT = 0x03,
} ft6x36_touch_event_t;

typedef struct ft6x36_touch_point_data ft6x36_touch_point_t;
/** @brief Decoded data for one FT6X36 touch-point slot. */
struct ft6x36_touch_point_data {
  /** @brief True when this slot contains an active, decodable touch. */
  bool valid;
  /** @brief Touch event decoded from the slot header bits. */
  ft6x36_touch_event_t event;
  /** @brief 12-bit X coordinate decoded from `Pn_XH` and `Pn_XL`. */
  uint16_t x;
  /** @brief 12-bit Y coordinate decoded from `Pn_YH` and `Pn_YL`. */
  uint16_t y;
  /** @brief Touch ID nibble used to correlate a moving contact. */
  uint8_t touch_id;
  /** @brief Raw weight byte reported by `Pn_WEIGHT`. */
  uint8_t weight;
  /** @brief Touch area nibble decoded from `Pn_MISC` bits 7:4. */
  uint8_t area;
};

typedef struct ft6x36_touch_data ft6x36_touch_data_t;
/** @brief Decoded FT6X36 touch frame covering both public touch slots. */
struct ft6x36_touch_data {
  /** @brief Gesture ID byte decoded from `FT6X36_REG_GEST_ID`. */
  ft6x36_gesture_id_t gesture_id;
  /** @brief Active touch count decoded from `FT6X36_REG_TD_STATUS`. */
  uint8_t touch_count;
  /** @brief Decoded touch-point slots 0 and 1. */
  ft6x36_touch_point_t points[FT6X36_TOUCH_POINTS_MAX];
};

/** @name Raw Register Access */
/** @{ */

/**
 * @brief Read one or more bytes starting at a register address.
 *
 * The helper requires `transport_write_read` for register reads.
 *
 * @param touch FT6X36 transport callback set.
 * @param reg First register address to read.
 * @param out_data Output buffer that receives the register bytes.
 * @param len Number of bytes to read.
 * @return `FT6X36_ERR_NONE` on success, `FT6X36_ERR_INVALID_ARG` when `touch`,
 * `out_data`, or `len` is invalid, `FT6X36_ERR_INVALID_STATE` when
 * `transport_write_read` is not configured, `FT6X36_ERR_IO` when a callback
 * reports a short read, or a transport-specific error code from the callbacks.
 */
int32_t ft6x36_reg_read(ft6x36_t *touch, uint8_t reg, uint8_t *out_data, size_t len);

/**
 * @brief Read one 8-bit FT6X36 register.
 *
 * @param touch FT6X36 transport callback set.
 * @param reg Register address to read.
 * @param out_value Output pointer that receives the register byte.
 * @return `FT6X36_ERR_NONE` on success, `FT6X36_ERR_INVALID_ARG` when `touch`
 * or `out_value` is invalid, `FT6X36_ERR_INVALID_STATE` when
 * `transport_write_read` is not configured, `FT6X36_ERR_IO` when the transport
 * reports a short read, or a transport-specific error code from the callbacks.
 */
int32_t ft6x36_reg8_read(ft6x36_t *touch, uint8_t reg, uint8_t *out_value);

/**
 * @brief Write one 8-bit FT6X36 register.
 *
 * @param touch FT6X36 transport callback set.
 * @param reg Register address to write.
 * @param value Byte value to store in `reg`.
 * @return `FT6X36_ERR_NONE` on success, `FT6X36_ERR_INVALID_ARG` when `touch`
 * is `NULL`, `FT6X36_ERR_INVALID_STATE` when `transport_write` is not
 * configured, or a transport-specific error code from the callback.
 */
int32_t ft6x36_reg8_write(ft6x36_t *touch, uint8_t reg, uint8_t value);

/**
 * @brief Set selected bits in an 8-bit FT6X36 register.
 *
 * This helper performs a read-modify-write cycle so bits outside `bits` are
 * preserved. It therefore requires a usable read path in addition to
 * `transport_write`.
 *
 * @param touch FT6X36 transport callback set.
 * @param reg Register address to update.
 * @param bits Bit mask of the bits to force high.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()` or `ft6x36_reg8_write()`.
 */
int32_t ft6x36_reg8_set_bits(ft6x36_t *touch, uint8_t reg, uint8_t bits);

/**
 * @brief Replace selected bits in an 8-bit FT6X36 register.
 *
 * Bits covered by `mask` are taken from `new_value`. Bits outside `mask` are
 * preserved from the current register value. It therefore requires a usable
 * read path in addition to `transport_write`.
 *
 * @param touch FT6X36 transport callback set.
 * @param reg Register address to update.
 * @param mask Bit mask that selects which register bits to rewrite.
 * @param new_value Replacement value for the masked bits.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()` or `ft6x36_reg8_write()`.
 */
int32_t ft6x36_reg8_update_bits(ft6x36_t *touch, uint8_t reg, uint8_t mask, uint8_t new_value);

/** @} */

/** @name Device Mode Control */
/** @{ */

/**
 * @brief Read and decode `FT6X36_REG_DEV_MODE` bits 6:4.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_mode Output pointer that receives the decoded device mode.
 * @return `FT6X36_ERR_NONE` on success, `FT6X36_ERR_INVALID_ARG` when `touch`
 * or `out_mode` is invalid, `FT6X36_ERR_INVALID_STATE` when the register holds
 * an unsupported encoding or `transport_write_read` is not configured,
 * `FT6X36_ERR_IO` on a short read, or a transport-specific error code from the
 * callbacks.
 */
int32_t ft6x36_dev_mode_get(ft6x36_t *touch, ft6x36_device_mode_t *out_mode);

/**
 * @brief Program `FT6X36_REG_DEV_MODE` bits 6:4.
 *
 * This helper performs a direct register write so a configured
 * `transport_write` callback is sufficient.
 *
 * @param touch FT6X36 transport callback set.
 * @param mode Requested device mode.
 * @return `FT6X36_ERR_NONE` on success, `FT6X36_ERR_INVALID_ARG` when `touch`
 * is `NULL` or `mode` is not one of the exported enum values,
 * `FT6X36_ERR_INVALID_STATE` when `transport_write` is not configured, or a
 * transport-specific error code from the callback.
 */
int32_t ft6x36_dev_mode_set(ft6x36_t *touch, ft6x36_device_mode_t mode);

/**
 * @brief Compatibility wrapper for `ft6x36_dev_mode_get()`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_mode Output pointer that receives the decoded device mode.
 * @return Same result as `ft6x36_dev_mode_get()`.
 */
static inline int32_t ft6x36_device_mode_get(ft6x36_t *touch, ft6x36_device_mode_t *out_mode) {
  return ft6x36_dev_mode_get(touch, out_mode);
}

/**
 * @brief Compatibility wrapper for `ft6x36_dev_mode_set()`.
 *
 * @param touch FT6X36 transport callback set.
 * @param mode Requested device mode.
 * @return Same result as `ft6x36_dev_mode_set()`.
 */
static inline int32_t ft6x36_device_mode_set(ft6x36_t *touch, ft6x36_device_mode_t mode) {
  return ft6x36_dev_mode_set(touch, mode);
}

/** @} */

/** @name Touch Reporting */
/** @{ */

/**
 * @brief Read the current gesture ID byte.
 *
 * The helper returns the raw `FT6X36_REG_GEST_ID` byte cast to
 * `ft6x36_gesture_id_t`. Callers should treat values outside the exported enum
 * constants as controller-defined encodings.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_gesture Output pointer that receives the gesture ID.
 * @return `FT6X36_ERR_NONE` on success, `FT6X36_ERR_INVALID_ARG` when `touch`
 * or `out_gesture` is invalid, `FT6X36_ERR_INVALID_STATE` when
 * `transport_write_read` is not configured, `FT6X36_ERR_IO` on a short read,
 * or a transport-specific error code from the callbacks.
 */
int32_t ft6x36_gesture_id_get(ft6x36_t *touch, ft6x36_gesture_id_t *out_gesture);

/**
 * @brief Read the current number of active touch points.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_count Output pointer that receives the decoded touch count.
 * @return `FT6X36_ERR_NONE` on success, `FT6X36_ERR_INVALID_ARG` when `touch`
 * or `out_count` is invalid, `FT6X36_ERR_INVALID_STATE` when the controller
 * reports more than `FT6X36_TOUCH_POINTS_MAX` touches or
 * `transport_write_read` is not configured, `FT6X36_ERR_IO` on a short read,
 * or a transport-specific error code from the callbacks.
 */
int32_t ft6x36_touch_count_get(ft6x36_t *touch, uint8_t *out_count);

/**
 * @brief Read and decode one touch-point slot.
 *
 * This helper performs a fresh burst read through `ft6x36_touch_data_get()`.
 *
 * @param touch FT6X36 transport callback set.
 * @param point_index Touch slot index to read: `0` or `1`.
 * @param out_point Output pointer that receives the decoded point data.
 * @return `FT6X36_ERR_NONE` on success, `FT6X36_ERR_INVALID_ARG` when `touch`,
 * `point_index`, or `out_point` is invalid, `FT6X36_ERR_NOT_FOUND` when the
 * requested slot is not currently active, `FT6X36_ERR_INVALID_STATE` when the
 * controller reports an impossible touch count, the decoded slot is invalid, or
 * `transport_write_read` is not configured, `FT6X36_ERR_IO` on a short read,
 * or a transport-specific error code from the callbacks.
 */
int32_t ft6x36_touch_point_get(ft6x36_t *touch,
                               uint8_t point_index,
                               ft6x36_touch_point_t *out_point);

/**
 * @brief Read one coherent FT6X36 touch frame.
 *
 * The helper performs a single burst read that covers the current gesture ID,
 * touch count, and both public touch-point slots.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_data Output pointer that receives the decoded touch frame.
 * @return `FT6X36_ERR_NONE` on success, `FT6X36_ERR_INVALID_ARG` when `touch`
 * or `out_data` is invalid, `FT6X36_ERR_INVALID_STATE` when the controller
 * reports more than `FT6X36_TOUCH_POINTS_MAX` touches or
 * `transport_write_read` is not configured, `FT6X36_ERR_IO` on a short read,
 * or a transport-specific error code from the callbacks.
 */
int32_t ft6x36_touch_data_get(ft6x36_t *touch, ft6x36_touch_data_t *out_data);

/** @} */

/** @name Control, Threshold, and Timing Configuration */
/** @{ */

/**
 * @brief Read the raw touch-threshold byte from `FT6X36_REG_TH_GROUP`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_value Output pointer that receives the threshold byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_touch_threshold_get(ft6x36_t *touch, uint8_t *out_value);

/**
 * @brief Write the raw touch-threshold byte to `FT6X36_REG_TH_GROUP`.
 *
 * @param touch FT6X36 transport callback set.
 * @param value Threshold byte to write.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_write()`.
 */
int32_t ft6x36_touch_threshold_set(ft6x36_t *touch, uint8_t value);

/**
 * @brief Read the raw filter-coefficient byte from `FT6X36_REG_TH_DIFF`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_value Output pointer that receives the coefficient byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_filter_coefficient_get(ft6x36_t *touch, uint8_t *out_value);

/**
 * @brief Write the raw filter-coefficient byte to `FT6X36_REG_TH_DIFF`.
 *
 * @param touch FT6X36 transport callback set.
 * @param value Coefficient byte to write.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_write()`.
 */
int32_t ft6x36_filter_coefficient_set(ft6x36_t *touch, uint8_t value);

/**
 * @brief Read and decode `FT6X36_REG_CTRL`.
 *
 * @param touch FT6X36 transport callback set.
 * @param ctrl_out Output pointer that receives the decoded control mode.
 * @return `FT6X36_ERR_NONE` on success, `FT6X36_ERR_INVALID_ARG` when `touch`
 * or `ctrl_out` is invalid, `FT6X36_ERR_INVALID_STATE` when the register holds
 * an unsupported encoding or `transport_write_read` is not configured,
 * `FT6X36_ERR_IO` on a short read, or a transport-specific error code from the
 * callbacks.
 */
int32_t ft6x36_ctrl_mode_get(ft6x36_t *touch, ft6x36_ctrl_mode_t *ctrl_out);

/**
 * @brief Write `FT6X36_REG_CTRL`.
 *
 * @param touch FT6X36 transport callback set.
 * @param ctrl Requested control mode.
 * @return `FT6X36_ERR_NONE` on success, `FT6X36_ERR_INVALID_ARG` when `touch`
 * is `NULL` or `ctrl` is not one of the exported enum values,
 * `FT6X36_ERR_INVALID_STATE` when `transport_write` is not configured, or a
 * transport-specific error code from the callback.
 */
int32_t ft6x36_ctrl_mode_set(ft6x36_t *touch, ft6x36_ctrl_mode_t ctrl);

/**
 * @brief Read the raw active-to-monitor delay byte from `FT6X36_REG_TIMEENTERMONITOR`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_value Output pointer that receives the delay byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_active_to_monitor_time_get(ft6x36_t *touch, uint8_t *out_value);

/**
 * @brief Write the raw active-to-monitor delay byte to `FT6X36_REG_TIMEENTERMONITOR`.
 *
 * @param touch FT6X36 transport callback set.
 * @param value Delay byte to write.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_write()`.
 */
int32_t ft6x36_active_to_monitor_time_set(ft6x36_t *touch, uint8_t value);

/**
 * @brief Read the raw active report-rate byte from `FT6X36_REG_PERIODACTIVE`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_value Output pointer that receives the report-rate byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_active_report_rate_get(ft6x36_t *touch, uint8_t *out_value);

/**
 * @brief Write the raw active report-rate byte to `FT6X36_REG_PERIODACTIVE`.
 *
 * @param touch FT6X36 transport callback set.
 * @param value Report-rate byte to write.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_write()`.
 */
int32_t ft6x36_active_report_rate_set(ft6x36_t *touch, uint8_t value);

/**
 * @brief Read the raw monitor report-rate byte from `FT6X36_REG_PERIODMONITOR`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_value Output pointer that receives the report-rate byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_monitor_report_rate_get(ft6x36_t *touch, uint8_t *out_value);

/**
 * @brief Write the raw monitor report-rate byte to `FT6X36_REG_PERIODMONITOR`.
 *
 * @param touch FT6X36 transport callback set.
 * @param value Report-rate byte to write.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_write()`.
 */
int32_t ft6x36_monitor_report_rate_set(ft6x36_t *touch, uint8_t value);

/** @} */

/** @name Gesture Tuning Configuration */
/** @{ */

/**
 * @brief Read the raw minimum-rotation-angle byte from `FT6X36_REG_RADIAN_VALUE`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_value Output pointer that receives the raw angle byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_minimum_rotation_angle_get(ft6x36_t *touch, uint8_t *out_value);

/**
 * @brief Write the raw minimum-rotation-angle byte to `FT6X36_REG_RADIAN_VALUE`.
 *
 * @param touch FT6X36 transport callback set.
 * @param value Raw angle byte to write.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_write()`.
 */
int32_t ft6x36_minimum_rotation_angle_set(ft6x36_t *touch, uint8_t value);

/**
 * @brief Read the raw left-right offset byte from `FT6X36_REG_OFFSET_LEFT_RIGHT`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_value Output pointer that receives the offset byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_left_right_offset_get(ft6x36_t *touch, uint8_t *out_value);

/**
 * @brief Write the raw left-right offset byte to `FT6X36_REG_OFFSET_LEFT_RIGHT`.
 *
 * @param touch FT6X36 transport callback set.
 * @param value Offset byte to write.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_write()`.
 */
int32_t ft6x36_left_right_offset_set(ft6x36_t *touch, uint8_t value);

/**
 * @brief Read the raw up-down offset byte from `FT6X36_REG_OFFSET_UP_DOWN`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_value Output pointer that receives the offset byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_up_down_offset_get(ft6x36_t *touch, uint8_t *out_value);

/**
 * @brief Write the raw up-down offset byte to `FT6X36_REG_OFFSET_UP_DOWN`.
 *
 * @param touch FT6X36 transport callback set.
 * @param value Offset byte to write.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_write()`.
 */
int32_t ft6x36_up_down_offset_set(ft6x36_t *touch, uint8_t value);

/**
 * @brief Read the raw left-right distance byte from `FT6X36_REG_DISTANCE_LEFT_RIGHT`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_value Output pointer that receives the distance byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_left_right_distance_get(ft6x36_t *touch, uint8_t *out_value);

/**
 * @brief Write the raw left-right distance byte to `FT6X36_REG_DISTANCE_LEFT_RIGHT`.
 *
 * @param touch FT6X36 transport callback set.
 * @param value Distance byte to write.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_write()`.
 */
int32_t ft6x36_left_right_distance_set(ft6x36_t *touch, uint8_t value);

/**
 * @brief Read the raw up-down distance byte from `FT6X36_REG_DISTANCE_UP_DOWN`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_value Output pointer that receives the distance byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_up_down_distance_get(ft6x36_t *touch, uint8_t *out_value);

/**
 * @brief Write the raw up-down distance byte to `FT6X36_REG_DISTANCE_UP_DOWN`.
 *
 * @param touch FT6X36 transport callback set.
 * @param value Distance byte to write.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_write()`.
 */
int32_t ft6x36_up_down_distance_set(ft6x36_t *touch, uint8_t value);

/**
 * @brief Read the raw zoom-distance byte from `FT6X36_REG_DISTANCE_ZOOM`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_value Output pointer that receives the distance byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_zoom_distance_get(ft6x36_t *touch, uint8_t *out_value);

/**
 * @brief Write the raw zoom-distance byte to `FT6X36_REG_DISTANCE_ZOOM`.
 *
 * @param touch FT6X36 transport callback set.
 * @param value Distance byte to write.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_write()`.
 */
int32_t ft6x36_zoom_distance_set(ft6x36_t *touch, uint8_t value);

/** @} */

/** @name Identification and Status */
/** @{ */

/**
 * @brief Read the 16-bit library version from `FT6X36_REG_LIB_VER_H/L`.
 *
 * The returned value is decoded as a big-endian 16-bit integer.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_version Output pointer that receives the version value.
 * @return `FT6X36_ERR_NONE` on success, `FT6X36_ERR_INVALID_ARG` when `touch`
 * or `out_version` is invalid, `FT6X36_ERR_INVALID_STATE` when
 * `transport_write_read` is not configured, `FT6X36_ERR_IO` on a short read,
 * or a transport-specific error code from the callbacks.
 */
int32_t ft6x36_library_version_get(ft6x36_t *touch, uint16_t *out_version);

/**
 * @brief Read the raw cipher byte from `FT6X36_REG_CIPHER`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_cipher Output pointer that receives the cipher byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_cipher_get(ft6x36_t *touch, uint8_t *out_cipher);

/**
 * @brief Read and decode `FT6X36_REG_G_MODE`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_mode Output pointer that receives the decoded interrupt mode.
 * @return `FT6X36_ERR_NONE` on success, `FT6X36_ERR_INVALID_ARG` when `touch`
 * or `out_mode` is invalid, `FT6X36_ERR_INVALID_STATE` when the register holds
 * an unsupported encoding or `transport_write_read` is not configured,
 * `FT6X36_ERR_IO` on a short read, or a transport-specific error code from the
 * callbacks.
 */
int32_t ft6x36_interrupt_mode_get(ft6x36_t *touch, ft6x36_interrupt_mode_t *out_mode);

/**
 * @brief Write `FT6X36_REG_G_MODE`.
 *
 * @param touch FT6X36 transport callback set.
 * @param mode Requested interrupt mode.
 * @return `FT6X36_ERR_NONE` on success, `FT6X36_ERR_INVALID_ARG` when `touch`
 * is `NULL` or `mode` is not one of the exported enum values,
 * `FT6X36_ERR_INVALID_STATE` when `transport_write` is not configured, or a
 * transport-specific error code from the callback.
 */
int32_t ft6x36_interrupt_mode_set(ft6x36_t *touch, ft6x36_interrupt_mode_t mode);

/**
 * @brief Read the raw power-mode byte from `FT6X36_REG_PWR_MODE`.
 *
 * The PNG application-note material names this register but does not define a
 * complete symbolic decode table, so this API exposes the raw byte.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_mode Output pointer that receives the power-mode byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_power_mode_get(ft6x36_t *touch, uint8_t *out_mode);

/**
 * @brief Write the raw power-mode byte to `FT6X36_REG_PWR_MODE`.
 *
 * @param touch FT6X36 transport callback set.
 * @param mode Power-mode byte to write.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_write()`.
 */
int32_t ft6x36_power_mode_set(ft6x36_t *touch, uint8_t mode);

/**
 * @brief Read the raw firmware version byte from `FT6X36_REG_FIRMID`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_version Output pointer that receives the firmware version byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_firmware_version_get(ft6x36_t *touch, uint8_t *out_version);

/**
 * @brief Read the raw panel ID byte from `FT6X36_REG_FOCALTECH_ID`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_panel_id Output pointer that receives the panel ID byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_panel_id_get(ft6x36_t *touch, uint8_t *out_panel_id);

/**
 * @brief Read the raw release-code byte from `FT6X36_REG_RELEASE_CODE_ID`.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_version Output pointer that receives the release-code byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_release_code_version_get(ft6x36_t *touch, uint8_t *out_version);

/**
 * @brief Read the raw operating-state byte from `FT6X36_REG_STATE`.
 *
 * The PNG application-note material names this register but does not define a
 * complete symbolic decode table, so this API exposes the raw byte.
 *
 * @param touch FT6X36 transport callback set.
 * @param out_state Output pointer that receives the state byte.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_read()`.
 */
int32_t ft6x36_operating_state_get(ft6x36_t *touch, uint8_t *out_state);

/**
 * @brief Write the raw operating-state byte to `FT6X36_REG_STATE`.
 *
 * @param touch FT6X36 transport callback set.
 * @param state State byte to write.
 * @return `FT6X36_ERR_NONE` on success, or any error returned by
 * `ft6x36_reg8_write()`.
 */
int32_t ft6x36_operating_state_set(ft6x36_t *touch, uint8_t state);

/** @} */

#ifdef __cplusplus
}
#endif

/** @} */
