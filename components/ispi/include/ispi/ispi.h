/**
 * @file ispi.h
 * @brief Minimal SPI master wrapper API used by project components.
 * @ingroup ispi
 */
/**
 * @defgroup ispi ISPI
 * @brief Minimal SPI master wrapper API used by project components.
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
#define ISPI_ERR_NONE 0
/** @brief Base value for ISPI-specific error codes. */
#define ISPI_ERR_BASE 0x3100
#define ISPI_ERR_FAIL (ISPI_ERR_BASE + 1)          /*!< Generic failure */
#define ISPI_ERR_NO_MEM (ISPI_ERR_BASE + 2)        /*!< Out of memory */
#define ISPI_ERR_INVALID_ARG (ISPI_ERR_BASE + 3)   /*!< Invalid argument */
#define ISPI_ERR_INVALID_STATE (ISPI_ERR_BASE + 4) /*!< Invalid state */
#define ISPI_ERR_NOT_FOUND (ISPI_ERR_BASE + 5)     /*!< Resource not found */
#define ISPI_ERR_NOT_SUPPORTED (ISPI_ERR_BASE + 6) /*!< Operation not supported */
#define ISPI_ERR_TIMEOUT (ISPI_ERR_BASE + 7)       /*!< Operation timed out */
#define ISPI_ERR_IO (ISPI_ERR_BASE + 8)            /*!< I/O failure */

/**
 * @brief Supported SPI master hosts.
 *
 * Availability is target-dependent. `ISPI_HOST_SPI3` returns
 * `ISPI_ERR_NOT_SUPPORTED` on targets that do not expose `SPI3_HOST`.
 */
typedef enum {
  ISPI_HOST_SPI2 = 0, /**< Use ESP-IDF `SPI2_HOST`. */
  ISPI_HOST_SPI3 = 1, /**< Use ESP-IDF `SPI3_HOST` when supported by the target. */
} ispi_host_t;

/**
 * @brief DMA selection for SPI bus initialization.
 */
typedef enum {
  ISPI_DMA_DISABLED = 0, /**< Disable SPI DMA. */
  ISPI_DMA_AUTO = 1,     /**< Let ESP-IDF allocate a DMA channel automatically. */
} ispi_dma_channel_t;

/**
 * @brief Master-bus configuration.
 */
typedef struct {
  ispi_host_t host;               /**< SPI host to initialize. */
  int32_t mosi_io_num;            /**< MOSI GPIO number, or -1 if unused. */
  int32_t miso_io_num;            /**< MISO GPIO number, or -1 if unused. */
  int32_t sclk_io_num;            /**< Clock GPIO number. */
  int32_t max_transfer_sz;        /**< Maximum transfer size in bytes, or 0 for ESP-IDF default. */
  ispi_dma_channel_t dma_channel; /**< DMA selection. */
} ispi_master_bus_config_t;

/**
 * @brief Attached device configuration.
 */
typedef struct {
  int32_t cs_io_num;       /**< Chip-select GPIO number, or -1 if not used. */
  uint32_t clock_speed_hz; /**< Requested SPI clock speed in Hz. */
  uint8_t mode;            /**< SPI mode 0-3. */
  int32_t input_delay_ns;  /**< Input delay in nanoseconds, or 0 for default timing. */
  uint16_t queue_size;     /**< Internal ESP-IDF queue depth for this device. */
  bool lsb_first;          /**< Shift both TX and RX least-significant bit first. */
  bool half_duplex;        /**< Use half-duplex transactions. */
  bool use_3wire;          /**< Use MOSI for both write and read phases. */
} ispi_device_config_t;

/**
 * @brief One blocking SPI transaction.
 *
 * Buffer sizes are expressed in bytes. This thin wrapper supports write-only,
 * read-only, full-duplex equal-length exchanges, and half-duplex mixed-length
 * transfers.
 */
typedef struct {
  const uint8_t *tx_buffer; /**< Bytes to transmit, or NULL when no TX phase is used. */
  size_t tx_size;           /**< Number of bytes to transmit. */
  uint8_t *rx_buffer; /**< Destination for received bytes, or NULL when no RX phase is used. */
  size_t rx_size;     /**< Number of bytes to receive. */
} ispi_transaction_t;

/** @brief Opaque master-bus handle. */
typedef struct ispi_master_bus_t *ispi_master_bus_handle_t;
/** @brief Opaque attached-device handle. */
typedef struct ispi_device_t *ispi_device_handle_t;

/**
 * @brief Return a stable string for an ISPI error code.
 */
const char *ispi_err_to_name(int32_t err);

/**
 * @brief Fill a master-bus config with safe defaults.
 */
void ispi_get_default_master_bus_config(ispi_master_bus_config_t *config);

/**
 * @brief Fill a device config with safe defaults.
 */
void ispi_get_default_device_config(ispi_device_config_t *config);

/**
 * @brief Fill a transaction descriptor with safe defaults.
 */
void ispi_get_default_transaction(ispi_transaction_t *transaction);

/**
 * @brief Create a new SPI master bus.
 *
 * `config->host` must select a supported SPI host. `config->sclk_io_num` must
 * be set to a real GPIO, and at least one of MOSI or MISO must also be set to
 * a real GPIO.
 *
 * @return `ISPI_ERR_NONE` on success, or an `ISPI_ERR_*` code on failure.
 */
int32_t ispi_new_master_bus(const ispi_master_bus_config_t *config,
                            ispi_master_bus_handle_t *ret_bus);

/**
 * @brief Delete a master-bus handle.
 *
 * All attached devices must already have been removed.
 *
 * @return `ISPI_ERR_NONE` on success, or an `ISPI_ERR_*` code on failure.
 */
int32_t ispi_del_master_bus(ispi_master_bus_handle_t bus);

/**
 * @brief Attach a new device to an existing SPI master bus.
 *
 * @return `ISPI_ERR_NONE` on success, or an `ISPI_ERR_*` code on failure.
 */
int32_t ispi_new_device(ispi_master_bus_handle_t bus,
                        const ispi_device_config_t *config,
                        ispi_device_handle_t *ret_device);

/**
 * @brief Delete an attached device handle.
 *
 * @return `ISPI_ERR_NONE` on success, or an `ISPI_ERR_*` code on failure.
 */
int32_t ispi_del_device(ispi_device_handle_t device);

/**
 * @brief Perform a blocking SPI transfer.
 *
 * The wrapper accepts one transfer descriptor and validates the common transfer
 * patterns supported by this thin API:
 * - write-only
 * - read-only
 * - full-duplex equal-length TX/RX
 * - half-duplex TX and RX with independent byte counts
 *
 * @return `ISPI_ERR_NONE` on success, or an `ISPI_ERR_*` code on failure.
 */
int32_t ispi_device_transfer(ispi_device_handle_t device, const ispi_transaction_t *transaction);

#ifdef __cplusplus
}
#endif

/** @} */  // end of ispi
