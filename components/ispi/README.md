# ispi

`ispi` is a small ESP-IDF SPI master wrapper used by project components. It follows the same design intent as `ii2c`: keep the project-facing transport API narrow, stable, and owned by this repository instead of exposing higher-level components directly to ESP-IDF SPI master types.

This first version is intentionally thin. It supports:

- master-bus creation and teardown
- device attach and detach
- one blocking transfer API for write-only, read-only, full-duplex equal-length, and half-duplex mixed-length transactions
- stable `ISPI_ERR_*` error codes plus `ispi_err_to_name()`

It intentionally does not support:

- queued or asynchronous transactions
- bus acquire/release helpers
- command, address, or dummy-bit configuration
- callbacks, custom clock sources, or advanced SPI flags
- a generic probe helper

There is no SPI equivalent to I2C address ACK probing. Whether a device is present is protocol-specific and should be checked by issuing that device's own command sequence after attach.

## Public Files

- Header: `ispi/include/ispi/ispi.h`
- Implementation: `ispi/ispi_espidf.c`
- Build registration: `ispi/CMakeLists.txt`

## Dependencies

- ESP-IDF component dependency: `esp_driver_spi`

Consumers include the public header as:

```c
#include "ispi/ispi.h"
```

## API Overview

The public API centers on two opaque handles:

- `ispi_master_bus_handle_t` for a configured SPI host
- `ispi_device_handle_t` for one attached SPI device

Main entry points:

- `ispi_get_default_master_bus_config()` fills a safe starting bus config
- `ispi_get_default_device_config()` fills a safe starting device config
- `ispi_get_default_transaction()` fills a safe starting transfer descriptor
- `ispi_new_master_bus()` and `ispi_del_master_bus()` manage the SPI bus
- `ispi_new_device()` and `ispi_del_device()` attach and remove devices
- `ispi_device_transfer()` performs one blocking transfer
- `ispi_err_to_name()` converts `ISPI_ERR_*` codes into stable strings

## Basic Usage

```c
#include <stdint.h>
#include "ispi/ispi.h"

int example_spi_write_only(void) {
  ispi_master_bus_config_t bus_cfg;
  ispi_get_default_master_bus_config(&bus_cfg);
  bus_cfg.host = ISPI_HOST_SPI2;
  bus_cfg.mosi_io_num = 37;
  bus_cfg.miso_io_num = 35;
  bus_cfg.sclk_io_num = 36;

  ispi_master_bus_handle_t bus = NULL;
  int32_t err = ispi_new_master_bus(&bus_cfg, &bus);
  if (err != ISPI_ERR_NONE) {
    return err;
  }

  ispi_device_config_t dev_cfg;
  ispi_get_default_device_config(&dev_cfg);
  dev_cfg.cs_io_num = 34;
  dev_cfg.clock_speed_hz = 1000000;
  dev_cfg.mode = 0;

  ispi_device_handle_t dev = NULL;
  err = ispi_new_device(bus, &dev_cfg, &dev);
  if (err != ISPI_ERR_NONE) {
    ispi_del_master_bus(bus);
    return err;
  }

  const uint8_t data[] = {0x9F, 0x00, 0x00, 0x00};
  ispi_transaction_t trans;
  ispi_get_default_transaction(&trans);
  trans.tx_buffer = data;
  trans.tx_size = sizeof(data);

  err = ispi_device_transfer(dev, &trans);

  ispi_del_device(dev);
  ispi_del_master_bus(bus);
  return err;
}
```

## Transfer Rules

- At least one direction must be active. A transfer with both sizes set to zero is invalid.
- `tx_size > 0` requires `tx_buffer != NULL`.
- `rx_size > 0` requires `rx_buffer != NULL`.
- In full-duplex mode, simultaneous TX and RX require `tx_size == rx_size`.
- In half-duplex mode, TX and RX may use different byte counts.

## Notes

- `ISPI_HOST_SPI3` is target-dependent. `ispi_new_master_bus()` returns `ISPI_ERR_NOT_SUPPORTED` if the current target does not expose `SPI3_HOST`.
- `sclk_io_num` must be a real GPIO, and at least one of MOSI or MISO must also be a real GPIO.
- Default bus config leaves all GPIOs at `-1`; callers must set real pins before creating a bus.
- Default device speed is `1000000` Hz, default mode is `0`, and default queue size is `1`.
- This wrapper uses ESP-IDF `spi_device_transmit()` internally for its blocking transfers.

## Development Notes

The source file `ispi/ispi_espidf.c` is a thin translation layer from ESP-IDF `esp_err_t` results and SPI config structs to the repository-owned `ISPI_ERR_*`, `ispi_master_bus_config_t`, `ispi_device_config_t`, and `ispi_transaction_t` types. If the ESP-IDF SPI master API changes, that file is the place to update the mapping and validation logic.
