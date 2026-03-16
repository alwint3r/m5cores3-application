# ii2c

`ii2c` is a small ESP-IDF I2C master wrapper used by the other components in this repository. It narrows the ESP-IDF I2C API to a compact set of bus/device lifecycle calls plus synchronous probe, write, and write-then-read transfers.

## Public Files

- Header: `ii2c/include/ii2c/ii2c.h`
- Implementation: `ii2c/ii2c_espidf.c`
- Build registration: `ii2c/CMakeLists.txt`

## Dependencies

- ESP-IDF component dependency: `esp_driver_i2c`

Consumers include the public header as:

```c
#include "ii2c/ii2c.h"
```

## API Overview

The public API centers on two opaque handles:

- `ii2c_master_bus_handle_t` for a configured master controller
- `ii2c_device_handle_t` for one attached 7-bit I2C target device

Main entry points:

- `ii2c_get_default_master_bus_config()` fills a safe starting bus config
- `ii2c_get_default_device_config()` fills a safe starting device config
- `ii2c_new_master_bus()` and `ii2c_del_master_bus()` manage the bus
- `ii2c_new_device()` and `ii2c_del_device()` attach and remove devices
- `ii2c_master_probe()` checks whether a 7-bit address responds on the bus
- `ii2c_master_transmit()` performs a blocking write
- `ii2c_master_transmit_receive()` performs a blocking write-then-read transaction
- `ii2c_err_to_name()` converts `II2C_ERR_*` codes into stable strings

## Basic Usage

```c
#include <stdint.h>
#include "ii2c/ii2c.h"

int example_i2c_probe(void) {
  ii2c_master_bus_config_t bus_cfg;
  ii2c_get_default_master_bus_config(&bus_cfg);
  bus_cfg.sda_io_num = 21;
  bus_cfg.scl_io_num = 22;
  bus_cfg.enable_internal_pullup = true;

  ii2c_master_bus_handle_t bus = NULL;
  int32_t err = ii2c_new_master_bus(&bus_cfg, &bus);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  ii2c_device_config_t dev_cfg;
  ii2c_get_default_device_config(&dev_cfg);
  dev_cfg.device_address = 0x68;
  dev_cfg.scl_speed_hz = 400000;

  ii2c_device_handle_t dev = NULL;
  err = ii2c_new_device(bus, &dev_cfg, &dev);
  if (err != II2C_ERR_NONE) {
    ii2c_del_master_bus(bus);
    return err;
  }

  uint8_t reg = 0x75;
  uint8_t who_am_i = 0;
  err = ii2c_master_transmit_receive(dev, &reg, 1, &who_am_i, 1);

  ii2c_del_device(dev);
  ii2c_del_master_bus(bus);
  return err;
}
```

## Bus Scan Example

```c
#include <stdio.h>
#include "ii2c/ii2c.h"

int example_i2c_scan(void) {
  ii2c_master_bus_config_t bus_cfg;
  ii2c_get_default_master_bus_config(&bus_cfg);
  bus_cfg.sda_io_num = 21;
  bus_cfg.scl_io_num = 22;
  bus_cfg.enable_internal_pullup = true;

  ii2c_master_bus_handle_t bus = NULL;
  int32_t err = ii2c_new_master_bus(&bus_cfg, &bus);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  int found = 0;
  for (uint16_t addr = 0x08; addr <= 0x77; ++addr) {
    err = ii2c_master_probe(bus, addr, 20);
    if (err == II2C_ERR_NONE) {
      printf("Found device at 0x%02X\n", addr);
      found++;
    } else if (err != II2C_ERR_NOT_FOUND) {
      printf("Probe error at 0x%02X: %s\n", addr, ii2c_err_to_name(err));
    }
  }

  printf("Scan complete, found %d device(s)\n", found);
  ii2c_del_master_bus(bus);
  return II2C_ERR_NONE;
}
```

## Notes

- `II2C_PORT_AUTO` asks the backend to auto-select the controller port.
- Default device speed is `100000` Hz and default synchronous timeout is `1000` ms.
- Default bus config leaves `sda_io_num` and `scl_io_num` at `-1`; callers must set real GPIOs before creating a bus.
- Clock-source support is target-dependent. `ii2c_new_master_bus()` returns `II2C_ERR_NOT_SUPPORTED` if `clk_src` requests a source that the active ESP target does not expose.
- Internal pull-ups are disabled by default.
- `ii2c_master_probe()` treats `timeout_ms == 0` as `II2C_DEFAULT_TIMEOUT_MS` and returns `II2C_ERR_NOT_FOUND` when no device acknowledges the address.
- This wrapper exposes only master-mode, synchronous transfer helpers. It does not provide slave support or asynchronous transactions.

## Development Notes

The source file `ii2c/ii2c_espidf.c` is a thin translation layer from ESP-IDF `esp_err_t` results to `II2C_ERR_*` values. If the ESP-IDF I2C driver API changes, this file is the place to update the mapping and bus/device configuration translation.
