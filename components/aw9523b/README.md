# aw9523b

`aw9523b` is a small helper component for the AW9523B I2C GPIO expander used in this repository. It owns AW9523B register access and register decoding only. Applications provide transport callbacks for raw writes and combined write-then-read transfers, so the component is no longer tied directly to `ii2c`.

## Public Files

- Header: `aw9523b/include/aw9523b/aw9523b.h`
- Register constants: `aw9523b/include/aw9523b/aw9523b_register.h`
- Implementation: `aw9523b/aw9523b.c`
- Build registration: `aw9523b/CMakeLists.txt`

## Dependencies

- No component-level transport dependency. Applications choose the transport and provide callbacks.

Consumers include the public headers as:

```c
#include "aw9523b/aw9523b.h"
#include "aw9523b/aw9523b_register.h"
```

## API Overview

The public API centers on `aw9523b_t`, a small callback holder:

- `transport_write` sends raw bytes for direct register writes.
- `transport_write_read` sends register-address bytes, reads back data bytes, and reports the actual read length.

Main entry points:

- `aw9523b_id_get()` reads the device identification register.
- `aw9523b_port0_drive_mode_get()` and `aw9523b_port0_drive_mode_set()` read and write the port-0 output drive mode through `AW9523B_REG_GCR` bit 4.
- `aw9523b_port_dir_bits_get()`, `aw9523b_port_dir_bits_set()`, and `aw9523b_port_dir_bits_update()` read or write an entire 8-bit port direction mask, where `1` means input and `0` means output.
- `aw9523b_port_dir_set()` changes the direction of a single pin with an explicit direction enum instead of a boolean flag.
- `aw9523b_port_interrupt_bits_get()`, `aw9523b_port_interrupt_bits_set()`, and `aw9523b_port_interrupt_bits_update()` read or write an entire 8-bit interrupt-enable mask using positive enable semantics.
- `aw9523b_interrupt_get()` and `aw9523b_interrupt_set()` read or write interrupt enable state for one pin.
- `aw9523b_port_input_read()` reads the full 8-bit input-state register for one port.
- `aw9523b_port_output_read()` reads the full 8-bit output-latch register for one port.
- `aw9523b_level_get()` reads the current logic level of one pin from the input register.
- `aw9523b_level_set()` writes one pin in the output latch register.
- `aw9523b_reg8_read()`, `aw9523b_reg8_write()`, `aw9523b_reg8_set_bits()`, and `aw9523b_reg8_update_bits()` provide low-level direct register access when no higher-level helper exists yet.

The component defines `AW9523B_ERR_*` for its own validation and impossible-state failures. Transport callback failures are passed through unchanged. `aw9523b_err_to_name()` only decodes `AW9523B_ERR_*` values.

## Basic Usage with `ii2c`

The component itself does not depend on `ii2c`, but the application can still adapt an `ii2c_device_handle_t` into `aw9523b_t` callbacks:

```c
#include <stdint.h>
#include "ii2c/ii2c.h"
#include "aw9523b/aw9523b.h"

static ii2c_device_handle_t s_aw9523b_i2c = NULL;

static int32_t aw9523b_i2c_write(const uint8_t *write_buffer, size_t write_size) {
  return ii2c_master_transmit(s_aw9523b_i2c, write_buffer, write_size);
}

static int32_t aw9523b_i2c_write_read(const uint8_t *write_buffer,
                                      size_t write_size,
                                      uint8_t *read_buffer,
                                      size_t *read_size,
                                      size_t read_capacity) {
  if (!read_buffer || !read_size) {
    return II2C_ERR_INVALID_ARG;
  }

  int32_t err = ii2c_master_transmit_receive(
      s_aw9523b_i2c, write_buffer, write_size, read_buffer, read_capacity);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  *read_size = read_capacity;
  return II2C_ERR_NONE;
}

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

  err = ii2c_new_device(bus, &dev_cfg, &s_aw9523b_i2c);
  if (err != II2C_ERR_NONE) {
    ii2c_del_master_bus(bus);
    return err;
  }

  aw9523b_t expander = {
      .transport_write = aw9523b_i2c_write,
      .transport_write_read = aw9523b_i2c_write_read,
  };

  uint8_t chip_id = 0;
  err = aw9523b_id_get(&expander, &chip_id);
  if (err == AW9523B_ERR_NONE) {
    err = aw9523b_port0_drive_mode_set(&expander, AW9523B_PORT0_DRIVE_MODE_PUSH_PULL);
  }
  if (err == AW9523B_ERR_NONE) {
    err = aw9523b_port_dir_set(&expander, 0, 3, AW9523B_PORT_DIRECTION_OUTPUT);
  }
  if (err == AW9523B_ERR_NONE) {
    err = aw9523b_level_set(&expander, 0, 3, 1);
  }

  ii2c_del_device(s_aw9523b_i2c);
  s_aw9523b_i2c = NULL;
  ii2c_del_master_bus(bus);
  return err;
}
```

## Higher-Level Example

Configure an input pin with interrupts:

```c
int example_aw9523b_enable_interrupt(aw9523b_t *expander) {
  int32_t err = aw9523b_port_dir_set(expander, 1, 2, AW9523B_PORT_DIRECTION_INPUT);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  err = aw9523b_interrupt_set(expander, 1, 2, true);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  uint8_t enabled = 0;
  err = aw9523b_interrupt_get(expander, 1, 2, &enabled);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  return enabled ? AW9523B_ERR_NONE : AW9523B_ERR_INVALID_STATE;
}
```

## Notes

- The component checks callback presence lazily. Missing write or combined-read callbacks return `AW9523B_ERR_INVALID_STATE`.
- `aw9523b_reg8_read()` returns `AW9523B_ERR_IO` if the transport callback reports success but returns fewer bytes than requested.
- Valid `port` values are `0` and `1`. Valid `pin` values are `0` through `7`.
- `aw9523b_port_dir_bits_*()` use the AW9523B configuration-register convention where `1` means input and `0` means output.
- `aw9523b_port_interrupt_bits_*()` and `aw9523b_interrupt_*()` expose positive enable semantics even though the raw interrupt-enable register stores the inverse bit value.
- `aw9523b_port0_drive_mode_*()` affects only port 0. The current public component does not expose a configurable drive-mode API for port 1.
- `aw9523b_level_set()` writes the output latch register, while `aw9523b_level_get()` reads the input-state register.
- `aw9523b_reg8_set_bits()` and `aw9523b_reg8_update_bits()` both perform read-modify-write cycles, so they preserve unrelated bits in the target register.
