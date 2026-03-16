# aw9523b

`aw9523b` is a small helper component for the AW9523B I2C GPIO expander used in this repository. It builds on top of `ii2c` and currently exposes chip identification, port-0 drive-mode control, whole-port direction and interrupt masks, single-pin direction and interrupt helpers, single-pin level I/O helpers, and low-level 8-bit register access for cases that do not yet have a dedicated wrapper.

## Public Files

- Header: `aw9523b/include/aw9523b/aw9523b.h`
- Register constants: `aw9523b/include/aw9523b/aw9523b_register.h`
- Implementation: `aw9523b/aw9523b_espidf.c`
- Build registration: `aw9523b/CMakeLists.txt`

## Dependencies

- Project component dependency: `ii2c`

Consumers include the public headers as:

```c
#include "aw9523b/aw9523b.h"
#include "aw9523b/aw9523b_register.h"
```

## API Overview

The public API takes an existing `ii2c_device_handle_t` for the AW9523B. This component does not wrap that handle in a second device type.

Main entry points:

- `aw9523b_id_get()` reads the device identification register.
- `aw9523b_port0_drive_mode_get()` and `aw9523b_port0_drive_mode_set()` read and write the port-0 output drive mode through `AW9523B_REG_GCR` bit 4.
- `aw9523b_port_dir_bits_get()`, `aw9523b_port_dir_bits_set()`, and `aw9523b_port_dir_bits_update()` read or write an entire 8-bit port direction mask, where `1` means input and `0` means output.
- `aw9523b_port_dir_set()` changes the direction of a single pin without forcing the rest of the port.
- `aw9523b_port_interrupt_bits_get()`, `aw9523b_port_interrupt_bits_set()`, and `aw9523b_port_interrupt_bits_update()` read or write an entire 8-bit interrupt-enable mask using positive enable semantics.
- `aw9523b_interrupt_get()` and `aw9523b_interrupt_set()` read or write interrupt enable state for one pin.
- `aw9523b_level_get()` reads the current logic level of one pin from the input register.
- `aw9523b_level_set()` writes one pin in the output latch register.
- `aw9523b_reg8_read()`, `aw9523b_reg8_write()`, `aw9523b_reg8_set_bits()`, and `aw9523b_reg8_update_bits()` provide low-level direct register access when no higher-level helper exists yet.

All functions return `II2C_ERR_NONE` on success or an `II2C_ERR_*` value reported by the underlying `ii2c` transport. Helpers that take output pointers or validated `port`/`pin` arguments also return `II2C_ERR_INVALID_ARG` when those inputs are invalid.

Relevant register and bit-mask exports include:

- `AW9523B_REG_INPUT0` for the base input-state register. Use `+ port` to reach port 1.
- `AW9523B_REG_OUTPUT0` for the base output-latch register. Use `+ port` to reach port 1.
- `AW9523B_REG_CONFIG0` for the base direction register. Use `+ port` to reach port 1.
- `AW9523B_REG_INTENABLE0` for the base interrupt-enable register. Use `+ port` to reach port 1.
- `AW9523B_REG_ID` for the identification register.
- `AW9523B_REG_GCR` for the global control register.
- `AW9523B_GCR_PORT0_DRIVE_MODE_MASK`, `AW9523B_GCR_PORT0_DRIVE_MODE_OPEN_DRAIN`, and `AW9523B_GCR_PORT0_DRIVE_MODE_PUSH_PULL` for the port-0 drive-mode field in `AW9523B_REG_GCR`.

## Basic Usage

```c
#include <stdint.h>
#include "ii2c/ii2c.h"
#include "aw9523b/aw9523b.h"

int example_aw9523b_set_output(uint16_t aw9523b_address) {
  ii2c_master_bus_config_t bus_cfg;
  ii2c_get_default_master_bus_config(&bus_cfg);
  bus_cfg.sda_io_num = 12;
  bus_cfg.scl_io_num = 11;
  bus_cfg.enable_internal_pullup = true;

  ii2c_master_bus_handle_t bus = NULL;
  int32_t err = ii2c_new_master_bus(&bus_cfg, &bus);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  ii2c_device_config_t dev_cfg;
  ii2c_get_default_device_config(&dev_cfg);
  dev_cfg.device_address = aw9523b_address;
  dev_cfg.scl_speed_hz = 100000;
  dev_cfg.timeout_ms = 3000;

  ii2c_device_handle_t aw9523b = NULL;
  err = ii2c_new_device(bus, &dev_cfg, &aw9523b);
  if (err != II2C_ERR_NONE) {
    ii2c_del_master_bus(bus);
    return err;
  }

  uint8_t chip_id = 0;
  err = aw9523b_id_get(aw9523b, &chip_id);
  if (err == II2C_ERR_NONE) {
    err = aw9523b_port0_drive_mode_set(aw9523b, AW9523B_PORT0_DRIVE_MODE_PUSH_PULL);
  }
  if (err == II2C_ERR_NONE) {
    err = aw9523b_port_dir_set(aw9523b, 0, 3, false);
  }
  if (err == II2C_ERR_NONE) {
    err = aw9523b_level_set(aw9523b, 0, 3, 1);
  }

  ii2c_del_device(aw9523b);
  ii2c_del_master_bus(bus);
  return err;
}
```

Example interrupt configuration:

```c
int example_aw9523b_enable_interrupt(ii2c_device_handle_t aw9523b) {
  int32_t err = aw9523b_port_dir_set(aw9523b, 1, 2, true);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = aw9523b_interrupt_set(aw9523b, 1, 2, true);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint8_t enabled = 0;
  err = aw9523b_interrupt_get(aw9523b, 1, 2, &enabled);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  return enabled ? II2C_ERR_NONE : II2C_ERR_INVALID_STATE;
}
```

## Notes

- Valid `port` values are `0` and `1`. Valid `pin` values are `0` through `7`.
- `aw9523b_port_dir_bits_*()` and `aw9523b_port_dir_set()` use the AW9523B configuration-register convention where `1` means input and `0` means output.
- `aw9523b_port_interrupt_bits_*()` and `aw9523b_interrupt_*()` expose positive enable semantics even though the raw interrupt-enable register stores the inverse bit value.
- `aw9523b_port0_drive_mode_*()` affects only port 0. The current public component does not expose a configurable drive-mode API for port 1.
- `aw9523b_level_set()` writes the output latch register, while `aw9523b_level_get()` reads the input-state register.
- `aw9523b_reg8_set_bits()` and `aw9523b_reg8_update_bits()` both perform read-modify-write cycles, so they preserve unrelated bits in the target register.
