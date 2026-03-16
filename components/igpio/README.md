# igpio

`igpio` is a small ESP-IDF GPIO wrapper used by project code. It follows the same design intent as `ii2c` and `ispi`: keep the project-facing API narrow, stable, and owned by this repository instead of exposing higher-level code directly to ESP-IDF GPIO driver types.

This first version is intentionally thin. It supports:

- one-pin configuration with repo-owned mode, pull, and interrupt enums
- pin reset to backend defaults
- synchronous mode, pull, and interrupt updates
- synchronous logic level set and read helpers
- stable `IGPIO_ERR_*` error codes plus `igpio_err_to_name()`

It intentionally does not support:

- ISR service install or removal
- per-pin interrupt handler registration
- wake-up helpers
- drive-strength configuration
- matrix routing or other advanced GPIO features

## Public Files

- Header: `igpio/include/igpio/igpio.h`
- Implementation: `igpio/igpio_espidf.c`
- Build registration: `igpio/CMakeLists.txt`

## Dependencies

- ESP-IDF component dependency: `esp_driver_gpio`

Consumers include the public header as:

```c
#include "igpio/igpio.h"
```

## API Overview

The public API is pin-centric. It does not use opaque handles because GPIO configuration in this repository is global to a pin number rather than tied to a bus or attached device.

Main entry points:

- `igpio_get_default_config()` fills a safe starting config
- `igpio_configure()` applies one complete pin configuration
- `igpio_reset_pin()` restores one pin to backend defaults
- `igpio_set_mode()` updates only the pin mode
- `igpio_set_pull_mode()` updates only the pull resistor mode
- `igpio_set_intr_type()` updates only the interrupt trigger type
- `igpio_set_level()` drives one output level
- `igpio_get_level()` reads one logic level
- `igpio_err_to_name()` converts `IGPIO_ERR_*` codes into stable strings

## Basic Usage

```c
#include <stdbool.h>
#include "igpio/igpio.h"

int example_lcd_dc_gpio(void) {
  igpio_config_t cfg;
  igpio_get_default_config(&cfg);
  cfg.io_num = 35;
  cfg.mode = IGPIO_MODE_OUTPUT;
  cfg.pull_mode = IGPIO_PULL_FLOATING;
  cfg.intr_type = IGPIO_INTR_DISABLED;

  int32_t err = igpio_configure(&cfg);
  if (err != IGPIO_ERR_NONE) {
    return err;
  }

  err = igpio_set_level(cfg.io_num, false);
  if (err != IGPIO_ERR_NONE) {
    return err;
  }

  bool level = false;
  err = igpio_get_level(cfg.io_num, &level);
  if (err != IGPIO_ERR_NONE) {
    return err;
  }

  return level ? IGPIO_ERR_INVALID_STATE : IGPIO_ERR_NONE;
}
```

## Notes

- Default config leaves `io_num` at `-1`; callers must set a real GPIO before calling `igpio_configure()`.
- Output-related operations validate that the selected pin is output-capable.
- `igpio_get_level()` is synchronous and returns the current logic level as a `bool`.
- This wrapper exposes only simple synchronous GPIO helpers. ISR service setup and per-pin interrupt callbacks are intentionally out of scope for v1.

## Development Notes

The source file `igpio/igpio_espidf.c` is a thin translation layer from ESP-IDF GPIO driver types and `esp_err_t` results to the repository-owned `IGPIO_ERR_*`, `igpio_mode_t`, `igpio_pull_mode_t`, `igpio_intr_type_t`, and `igpio_config_t` types. If the ESP-IDF GPIO API changes, that file is the place to update the mapping and validation logic.
