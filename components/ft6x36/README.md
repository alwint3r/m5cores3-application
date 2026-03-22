# ft6x36

`ft6x36` is a small helper component for the FT6x36 touch controller used in this repository. It stays transport-agnostic: callers provide a write callback plus a combined write-then-read callback, and the component exposes raw register helpers, decoded touch-frame reads for up to two touch points, mode control, gesture-tuning registers, and identification helpers for the working-mode register map documented by the FT6x36 application-note PNGs in `docs/hardwares`.

## Public Files

- Header: `ft6x36/include/ft6x36/ft6x36.h`
- Register constants: `ft6x36/include/ft6x36/ft6x36_register.h`
- Implementation: `ft6x36/ft6x36.c`
- Build registration: `ft6x36/CMakeLists.txt`

## Dependencies

- Required project component dependency: none
- Common transport backend in this repository: `ii2c`

Consumers include the public headers as:

```c
#include "ft6x36/ft6x36.h"
#include "ft6x36/ft6x36_register.h"
```

## API Overview

The public API centers on `ft6x36_t`, which is a small callback set for register writes and combined register write-then-read transactions. The component does not allocate buses, own GPIOs, or wrap an `ii2c_device_handle_t` in a second handle type.

Main entry points:

- `ft6x36_reg_read()`, `ft6x36_reg8_read()`, `ft6x36_reg8_write()`, `ft6x36_reg8_set_bits()`, and `ft6x36_reg8_update_bits()` provide low-level direct register access.
- `ft6x36_touch_data_get()` performs one burst read of the current gesture, touch count, and both touch-point slots.
- `ft6x36_touch_count_get()`, `ft6x36_touch_point_get()`, and `ft6x36_gesture_id_get()` expose the most common touch-reporting queries.
- `ft6x36_dev_mode_get()/set()`, `ft6x36_ctrl_mode_get()/set()`, and `ft6x36_interrupt_mode_get()/set()` expose the documented mode fields.
- `ft6x36_touch_threshold_get()/set()`, `ft6x36_filter_coefficient_get()/set()`, `ft6x36_active_to_monitor_time_get()/set()`, `ft6x36_active_report_rate_get()/set()`, `ft6x36_monitor_report_rate_get()/set()`, and the gesture-tuning helpers expose the working-mode configuration registers from the datasheet PNGs.
- `ft6x36_library_version_get()`, `ft6x36_cipher_get()`, `ft6x36_firmware_version_get()`, `ft6x36_panel_id_get()`, `ft6x36_release_code_version_get()`, `ft6x36_power_mode_get()/set()`, and `ft6x36_operating_state_get()/set()` expose the identification and status registers.
- `ft6x36_err_to_name()` converts `FT6X36_ERR_*` values into stable strings.

All functions return `FT6X36_ERR_NONE` on success. Component-side validation and decoding failures return `FT6X36_ERR_*`. Transport callback errors are passed through unchanged, so an `ii2c`-backed adapter may also return `II2C_ERR_*` values.

Relevant public types and constants include:

- `ft6x36_device_mode_t`, `ft6x36_ctrl_mode_t`, `ft6x36_interrupt_mode_t`, `ft6x36_gesture_id_t`, and `ft6x36_touch_event_t` for the fields explicitly defined by the FT6x36 PNG register descriptions.
- `ft6x36_touch_point_t` and `ft6x36_touch_data_t` for decoded touch reports.
- `FT6X36_TOUCH_POINTS_MAX` and `FT6X36_TOUCH_POINT_ID_INVALID` for touch-frame validation.
- `FT6X36_REG_GEST_ID`, `FT6X36_REG_TD_STATUS`, `FT6X36_REG_TH_GROUP`, `FT6X36_REG_CTRL`, `FT6X36_REG_G_MODE`, `FT6X36_REG_FIRMID`, and the rest of the exported register constants in `ft6x36_register.h`.

## Basic Usage

```c
#include <stddef.h>
#include <stdint.h>
#include "ii2c/ii2c.h"
#include "ft6x36/ft6x36.h"

static ii2c_device_handle_t s_ft6x36_dev = NULL;

static int32_t ft6x36_i2c_write(const uint8_t *write_buffer, size_t write_size) {
  return ii2c_master_transmit(s_ft6x36_dev, write_buffer, write_size);
}

static int32_t ft6x36_i2c_write_read(const uint8_t *write_buffer,
                                     size_t write_size,
                                     uint8_t *read_buffer,
                                     size_t *read_size,
                                     size_t read_capacity) {
  if (!read_buffer || !read_size) {
    return II2C_ERR_INVALID_ARG;
  }

  int32_t err = ii2c_master_transmit_receive(
      s_ft6x36_dev, write_buffer, write_size, read_buffer, read_capacity);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  *read_size = read_capacity;
  return II2C_ERR_NONE;
}

int example_ft6x36_read_touch(uint16_t ft6x36_address, ft6x36_touch_data_t *out_touch) {
  if (!out_touch) {
    return FT6X36_ERR_INVALID_ARG;
  }

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
  dev_cfg.device_address = ft6x36_address;
  dev_cfg.scl_speed_hz = 100000;
  dev_cfg.timeout_ms = 3000;

  err = ii2c_new_device(bus, &dev_cfg, &s_ft6x36_dev);
  if (err != II2C_ERR_NONE) {
    ii2c_del_master_bus(bus);
    return err;
  }

  ft6x36_t touch = {
      .transport_write = ft6x36_i2c_write,
      .transport_write_read = ft6x36_i2c_write_read,
  };

  err = ft6x36_interrupt_mode_set(&touch, FT6X36_INTERRUPT_MODE_TRIGGER);
  if (err == FT6X36_ERR_NONE) {
    err = ft6x36_touch_data_get(&touch, out_touch);
  }

  ii2c_del_device(s_ft6x36_dev);
  s_ft6x36_dev = NULL;
  ii2c_del_master_bus(bus);
  return err;
}
```

Example point decode check:

```c
int example_ft6x36_first_point(ft6x36_t *touch, uint16_t *out_x, uint16_t *out_y) {
  if (!touch || !out_x || !out_y) {
    return FT6X36_ERR_INVALID_ARG;
  }

  ft6x36_touch_data_t touch_data = {0};
  int32_t err = ft6x36_touch_data_get(touch, &touch_data);
  if (err != FT6X36_ERR_NONE) {
    return err;
  }

  if (touch_data.touch_count == 0 || !touch_data.points[0].valid) {
    return FT6X36_ERR_NOT_FOUND;
  }

  *out_x = touch_data.points[0].x;
  *out_y = touch_data.points[0].y;
  return FT6X36_ERR_NONE;
}
```

## Notes

- This component expects `transport_write` for setters and `transport_write_read` for all register reads.
- `transport_write` alone is enough for the direct-write setters such as `ft6x36_dev_mode_set()`, `ft6x36_ctrl_mode_set()`, and `ft6x36_interrupt_mode_set()`. Getter APIs and read-modify-write helpers such as `ft6x36_reg8_set_bits()` and `ft6x36_reg8_update_bits()` require `transport_write_read`.
- `ft6x36_touch_data_get()` is the best choice when the application wants a coherent snapshot of gesture ID, touch count, and both touch-point slots in one read.
- `ft6x36_touch_point_get()` performs a fresh read internally and returns `FT6X36_ERR_NOT_FOUND` when the requested point index is not currently active.
- `ft6x36_power_mode_get()/set()` and `ft6x36_operating_state_get()/set()` expose raw bytes because the PNG application note names those registers but does not provide a full symbolic decode table for them.
- `ft6x36_err_to_name()` only decodes `FT6X36_ERR_*` values. If your adapter uses `ii2c`, call `ii2c_err_to_name()` for non-zero return codes outside the FT6X36 range.
- Board-level `INT` interrupt wiring, wake pulses, and `RSTN` reset timing remain application-owned. This component only covers the controller register interface.
- The example above uses the same `ii2c` lifecycle pattern as the other components in this repository, but board GPIO numbers, bus speed, and device address remain application-specific.

## Development Notes

The source file `ft6x36/ft6x36.c` is a transport-agnostic translation layer from the exported callback interface to the FT6x36 working-mode register map. If the project later needs board-owned `INT` or `RSTN` control inside the component, that work should be added deliberately as a wider abstraction change instead of being hidden inside the current register transport callbacks.
