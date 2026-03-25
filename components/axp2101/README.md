# axp2101

`axp2101` is a small helper component for the AXP2101 PMIC used in this repository. It owns AXP2101 register access and register decoding only. Applications provide transport callbacks for raw writes and combined write-then-read transfers plus an opaque callback context, so the component is no longer tied directly to `ii2c`.

## Public Files

- Header: `axp2101/include/axp2101/axp2101.h`
- Register constants: `axp2101/include/axp2101/axp2101_register.h`
- Implementation: `axp2101/axp2101.c`
- Build registration: `axp2101/CMakeLists.txt`

## Dependencies

- No component-level transport dependency. Applications choose the transport and provide callbacks.

Consumers include the public headers as:

```c
#include "axp2101/axp2101.h"
#include "axp2101/axp2101_register.h"
```

## API Overview

The public API centers on `axp2101_t`, a small callback holder:

- `transport_context` stores caller-owned state that is passed back to each transport callback.
- `transport_write` sends raw bytes for direct register writes.
- `transport_write_read` sends register-address bytes, reads back data bytes, and reports the actual read length.

Main entry points:

- `axp2101_status1_get()` and `axp2101_status2_get()` decode PMU status registers.
- `axp2101_fuel_gauge_enable()` and `axp2101_fuel_gauge_get()` manage fuel-gauge support.
- `axp2101_pmu_common_cfg_get()` and `axp2101_pmu_common_cfg_set()` access `AXP2101_REG_PMU_COMMON_CFG`.
- `axp2101_irq_off_on_level_get()` and `axp2101_irq_off_on_level_set()` access the power-key timing register.
- `axp2101_charger_current_get()` decodes charger current configuration.
- `axp2101_chgled_ctrl_get()` and `axp2101_chgled_ctrl_set()` access `AXP2101_REG_CHGLED_CTRL`.
- `axp2101_adc_enable_channels()`, `axp2101_adc_disable_channels()`, `axp2101_adc_vbus_read()`, `axp2101_adc_vsys_read()`, and `axp2101_adc_vbat_read()` manage and read ADC channels.
- `axp2101_dcdc_ctrl0_enable()`, `axp2101_dcdc_ctrl0_disable()`, `axp2101_dcdc_ctrl0_get()`, `axp2101_dcdc1_voltage_set()`, and `axp2101_dcdc1_voltage_get()` manage DCDC state.
- `axp2101_ldo_ctrl0_enable()`, `axp2101_ldo_ctrl0_disable()`, `axp2101_ldo_ctrl0_get()`, and the ALDO/BLDO/DLDO voltage helpers manage LDO state.
- `axp2101_reg8_read()`, `axp2101_reg8_write()`, `axp2101_reg8_set_bits()`, `axp2101_reg8_update_bits()`, and `axp2101_reg14_read()` are low-level register helpers for workflows that do not yet have dedicated wrappers.

The component defines `AXP2101_ERR_*` for its own validation and impossible-state failures. Transport callback failures are passed through unchanged. `axp2101_err_to_name()` only decodes `AXP2101_ERR_*` values.

## Basic Usage with `ii2c`

The component itself does not depend on `ii2c`, but the application can still adapt an `ii2c_device_handle_t` into `axp2101_t` callbacks:

```c
#include <stdint.h>
#include "ii2c/ii2c.h"
#include "axp2101/axp2101.h"

static int32_t axp2101_i2c_write(void *context,
                                 const uint8_t *write_buffer,
                                 size_t write_size) {
  ii2c_device_handle_t device = (ii2c_device_handle_t)context;
  if (!device) {
    return II2C_ERR_INVALID_ARG;
  }

  return ii2c_master_transmit(device, write_buffer, write_size);
}

static int32_t axp2101_i2c_write_read(void *context,
                                      const uint8_t *write_buffer,
                                      size_t write_size,
                                      uint8_t *read_buffer,
                                      size_t *read_size,
                                      size_t read_capacity) {
  ii2c_device_handle_t device = (ii2c_device_handle_t)context;
  if (!device || !read_buffer || !read_size) {
    return II2C_ERR_INVALID_ARG;
  }

  int32_t err = ii2c_master_transmit_receive(
      device, write_buffer, write_size, read_buffer, read_capacity);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  *read_size = read_capacity;
  return II2C_ERR_NONE;
}

int example_axp2101_read_voltages(uint16_t *vbus_mv,
                                  uint16_t *vsys_mv,
                                  uint16_t *vbat_mv) {
  if (!vbus_mv || !vsys_mv || !vbat_mv) {
    return AXP2101_ERR_INVALID_ARG;
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
  dev_cfg.device_address = 0x34;
  dev_cfg.scl_speed_hz = 100000;
  dev_cfg.timeout_ms = 3000;

  ii2c_device_handle_t axp2101_device = NULL;
  err = ii2c_new_device(bus, &dev_cfg, &axp2101_device);
  if (err != II2C_ERR_NONE) {
    ii2c_del_master_bus(bus);
    return err;
  }

  axp2101_t pmic = {
      .transport_context = axp2101_device,
      .transport_write = axp2101_i2c_write,
      .transport_write_read = axp2101_i2c_write_read,
  };

  err = axp2101_adc_enable_channels(
      &pmic, AXP2101_ADC_EN_VBUS | AXP2101_ADC_EN_VSYS | AXP2101_ADC_EN_BATT);
  if (err == AXP2101_ERR_NONE) {
    err = axp2101_adc_vbus_read(&pmic, vbus_mv);
  }
  if (err == AXP2101_ERR_NONE) {
    err = axp2101_adc_vsys_read(&pmic, vsys_mv);
  }
  if (err == AXP2101_ERR_NONE) {
    err = axp2101_adc_vbat_read(&pmic, vbat_mv);
  }

  ii2c_del_device(axp2101_device);
  ii2c_del_master_bus(bus);
  return err;
}
```

## Higher-Level Examples

Read PMU status:

```c
int example_axp2101_status_read(axp2101_t *pmic) {
  axp2101_status2_t status2 = {0};
  int32_t err = axp2101_status2_get(pmic, &status2);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  return AXP2101_ERR_NONE;
}
```

Configure ALDO1 and BLDO1:

```c
int example_axp2101_configure_ldos(axp2101_t *pmic) {
  int32_t err = axp2101_aldo1_voltage_set(pmic, 1800);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = axp2101_bldo1_voltage_set(pmic, 3300);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = axp2101_ldo_ctrl0_enable(
      pmic, AXP2101_LDO_CTRL0_EN_ALDO1 | AXP2101_LDO_CTRL0_EN_BLDO1);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  uint16_t aldo1_mv = 0;
  uint16_t bldo1_mv = 0;
  err = axp2101_aldo1_voltage_get(pmic, &aldo1_mv);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = axp2101_bldo1_voltage_get(pmic, &bldo1_mv);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  return (aldo1_mv == 1800 && bldo1_mv == 3300) ? AXP2101_ERR_NONE : AXP2101_ERR_INVALID_STATE;
}
```

Configure DCDC1:

```c
int example_axp2101_configure_dcdc1(axp2101_t *pmic) {
  int32_t err = axp2101_dcdc1_voltage_set(pmic, 3300);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = axp2101_dcdc_ctrl0_enable(pmic, AXP2101_DCDC_CTRL0_EN_DCDC1);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  uint16_t dcdc1_mv = 0;
  axp2101_dcdc_ctrl0_t dcdc0 = {0};
  err = axp2101_dcdc1_voltage_get(pmic, &dcdc1_mv);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = axp2101_dcdc_ctrl0_get(pmic, &dcdc0);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  return (dcdc0.dcdc1_en && dcdc1_mv == 3300) ? AXP2101_ERR_NONE : AXP2101_ERR_INVALID_STATE;
}
```

## Notes

- The component checks callback presence lazily. Missing write or combined-read callbacks return `AXP2101_ERR_INVALID_STATE`.
- `axp2101_reg8_read()` and `axp2101_reg14_read()` return `AXP2101_ERR_IO` if the transport callback reports success but returns fewer bytes than requested.
- The matching `*_voltage_get()` helpers return `AXP2101_ERR_INVALID_STATE` if the PMIC register contains a reserved selector value.
- The component exports `AXP2101_REG_*` register constants and related bit masks from `axp2101_register.h` for direct register workflows.
