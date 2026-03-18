/**
 * @file ili9342.h
 * @brief Low-level ILI9342 controller helpers built on abstract transport callbacks.
 * @ingroup ili9342
 */
/**
 * @defgroup ili9342 ILI9342
 * @brief Low-level ILI9342 controller helpers built on abstract transport callbacks.
 *
 * This component only owns register and command communication with the
 * controller. It does not allocate frame buffers, convert colors, render text,
 * or provide drawing primitives. Applications provide transport callbacks that
 * send command bytes, send data bytes, and sleep for a requested number of
 * milliseconds.
 * @{
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/** @brief Success return code. */
#define ILI9342_ERR_NONE 0
/** @brief Base value for ILI9342-specific error codes. */
#define ILI9342_ERR_BASE 0x3200
#define ILI9342_ERR_FAIL (ILI9342_ERR_BASE + 1)          /*!< Generic failure */
#define ILI9342_ERR_NO_MEM (ILI9342_ERR_BASE + 2)        /*!< Out of memory */
#define ILI9342_ERR_INVALID_ARG (ILI9342_ERR_BASE + 3)   /*!< Invalid argument */
#define ILI9342_ERR_INVALID_STATE (ILI9342_ERR_BASE + 4) /*!< Invalid state */
#define ILI9342_ERR_NOT_FOUND (ILI9342_ERR_BASE + 5)     /*!< Resource not found */
#define ILI9342_ERR_NOT_SUPPORTED (ILI9342_ERR_BASE + 6) /*!< Operation not supported */
#define ILI9342_ERR_TIMEOUT (ILI9342_ERR_BASE + 7)       /*!< Operation timed out */
#define ILI9342_ERR_IO (ILI9342_ERR_BASE + 8)            /*!< I/O failure */

/** @brief ILI9342 16-bit RGB565 pixel format. */
#define ILI9342_PIXEL_FORMAT_RGB565 0x55

/** @brief `MADCTL` bit: row-address order. */
#define ILI9342_MADCTL_MY 0x80
/** @brief `MADCTL` bit: column-address order. */
#define ILI9342_MADCTL_MX 0x40
/** @brief `MADCTL` bit: row/column exchange. */
#define ILI9342_MADCTL_MV 0x20
/** @brief `MADCTL` bit: vertical refresh order. */
#define ILI9342_MADCTL_ML 0x10
/** @brief `MADCTL` bit: RGB/BGR order. */
#define ILI9342_MADCTL_BGR 0x08
/** @brief `MADCTL` bit: horizontal refresh order. */
#define ILI9342_MADCTL_MH 0x04

/**
 * @brief Callback that sends one command byte to the controller.
 *
 * The callback must already know how to represent the command phase for the
 * active transport, such as D/C low on SPI or a command prefix on I2C.
 */
typedef int32_t (*ili9342_transport_command_write)(uint8_t command);

/**
 * @brief Callback that sends raw data bytes to the controller.
 *
 * The callback must already know how to represent the data phase for the
 * active transport.
 */
typedef int32_t (*ili9342_transport_data_write)(const uint8_t *data, size_t len);

/** @brief Callback that sleeps for the requested number of milliseconds. */
typedef void (*ili9342_delay_ms)(uint32_t ms);

typedef struct ili9342 ili9342_t;
/** @brief Transport callback set used by the low-level controller helpers. */
struct ili9342 {
  ili9342_transport_data_write transport_data_write;
  ili9342_transport_command_write transport_command_write;
  ili9342_delay_ms delay_fn;
};

/**
 * @brief Return a stable string for an ILI9342 component error code.
 *
 * This helper only decodes `ILI9342_ERR_*` values. Transport-specific errors
 * returned by the callbacks are passed through unchanged.
 */
const char *ili9342_err_to_name(int32_t err);

/**
 * @brief Send one command byte and an optional payload.
 *
 * When `len == 0`, `data` is ignored and only the command byte is sent.
 * Callback return codes are passed through unchanged.
 */
int32_t ili9342_write_command(ili9342_t *display, uint8_t command, const uint8_t *data, size_t len);

/**
 * @brief Send raw data bytes using the configured transport callback.
 *
 * This is a low-level primitive for controller payload phases such as RAM write
 * data. It does not interpret the payload as pixels or drawing commands.
 */
int32_t ili9342_write_data(ili9342_t *display, const uint8_t *data, size_t len);

/** @brief Issue `SWRESET` and wait the required 120 ms recovery delay. */
int32_t ili9342_reset_software(ili9342_t *display);

/** @brief Issue `SLPIN`. */
int32_t ili9342_sleep_enter(ili9342_t *display);

/** @brief Issue `SLPOUT` and wait the required 120 ms recovery delay. */
int32_t ili9342_sleep_exit(ili9342_t *display);

/** @brief Issue `DISPON`. */
int32_t ili9342_display_on(ili9342_t *display);

/** @brief Issue `DISPOFF`. */
int32_t ili9342_display_off(ili9342_t *display);

/** @brief Issue `INVON`. */
int32_t ili9342_inversion_on(ili9342_t *display);

/** @brief Issue `INVOFF`. */
int32_t ili9342_inversion_off(ili9342_t *display);

/** @brief Issue `IDMON`. */
int32_t ili9342_idle_mode_on(ili9342_t *display);

/** @brief Issue `IDMOFF`. */
int32_t ili9342_idle_mode_off(ili9342_t *display);

/** @brief Write `COLMOD`. */
int32_t ili9342_pixel_format_set(ili9342_t *display, uint8_t pixel_format);

/** @brief Write `MADCTL`. */
int32_t ili9342_memory_access_control_set(ili9342_t *display, uint8_t value);

/**
 * @brief Program `CASET`, `PASET`, and `RAMWR`.
 *
 * The component validates that the end coordinates are not smaller than the
 * start coordinates. It does not enforce any panel-specific width or height.
 */
int32_t ili9342_address_window_set(ili9342_t *display,
                                   uint16_t x0,
                                   uint16_t y0,
                                   uint16_t x1,
                                   uint16_t y1);

/** @brief Issue `RAMWR` so the application can stream raw data bytes next. */
int32_t ili9342_memory_write_begin(ili9342_t *display);

/**
 * @brief Apply the known-good default initialization sequence used by this repository.
 *
 * The sequence is intentionally based on the verified commands already used in
 * `main/main.c`, not on a generic best-effort datasheet profile.
 */
int32_t ili9342_init_default(ili9342_t *display);

#ifdef __cplusplus
}
#endif

/** @} */  // end of ili9342
