# axp2101

`axp2101` is a small helper component for the AXP2101 PMIC used in this repository. It builds on top of `ii2c` and currently exposes decoded PMU status helpers, fuel-gauge support for battery percentage, PMU common-configuration access for `AXP2101_REG_PMU_COMMON_CFG`, charger LED control for `AXP2101_REG_CHGLED_CTRL`, ADC channel control, DCDC enable control for `AXP2101_REG_DCDC_CTRL0`, a dedicated DCDC1 voltage helper, LDO enable control for `AXP2101_REG_LDO_CTRL0`, dedicated voltage configuration helpers for ALDO1 through ALDO4 plus BLDO1, BLDO2, and DLDO1, direct voltage-read helpers for VBUS/VSYS/VBAT, and low-level register access helpers for cases that do not yet have a dedicated wrapper.

## Public Files

- Header: `axp2101/include/axp2101/axp2101.h`
- Register constants: `axp2101/include/axp2101/axp2101_register.h`
- Implementation: `axp2101/axp2101_espidf.c`
- Build registration: `axp2101/CMakeLists.txt`

## Dependencies

- Project component dependency: `ii2c`

Consumers include the public headers as:

```c
#include "axp2101/axp2101.h"
#include "axp2101/axp2101_register.h"
```

## API Overview

The public API takes an existing `ii2c_device_handle_t` for the AXP2101. This component does not wrap that handle in a second device type.

Main entry points:

- `axp2101_status1_get()` reads and decodes `AXP2101_REG_PMU_STATUS1` into `axp2101_status1_t`.
- `axp2101_status2_get()` reads and decodes `AXP2101_REG_PMU_STATUS2` into `axp2101_status2_t`.
- `axp2101_fuel_gauge_enable()` enables the PMIC fuel-gauge and battery-detection bits used by this project.
- `axp2101_fuel_gauge_get()` reads the decoded fuel-gauge state, including whether a battery is present and whether a valid battery percentage is available.
- `axp2101_pmu_common_cfg_get()` reads and decodes `AXP2101_REG_PMU_COMMON_CFG` into `axp2101_pmu_common_cfg_t`.
- `axp2101_pmu_common_cfg_set()` writes `AXP2101_REG_PMU_COMMON_CFG` from a decoded struct, including the undocumented writable fields.
- `axp2101_irq_off_on_level_get()` reads and decodes `AXP2101_REG_IRQ_OFF_ON_LEVEL` into `axp2101_irq_off_on_level_t`.
- `axp2101_irq_off_on_level_set()` programs the IRQ, long-press power-off, and power-on timing fields in `AXP2101_REG_IRQ_OFF_ON_LEVEL`.
- `axp2101_charger_current_get()` reads the charger current-limit configuration from `AXP2101_REG_PRECHG_CURRENT_LIMIT`, `AXP2101_REG_CHG_CURRENT_LIMIT`, and `AXP2101_REG_TERM_CHG_CURRENT_CTRL`.
- `axp2101_chgled_ctrl_get()` reads and decodes `AXP2101_REG_CHGLED_CTRL` into `axp2101_chgled_ctrl_t`.
- `axp2101_chgled_ctrl_set()` programs the CHGLED enable bit, function selector, and register-controlled output field while preserving reserved bits.
- `axp2101_adc_enable_channels()` sets one or more `AXP2101_ADC_EN_*` bits in the ADC enable register.
- `axp2101_adc_disable_channels()` clears one or more `AXP2101_ADC_EN_*` bits in the ADC enable register.
- `axp2101_adc_vbus_read()` reads the decoded VBUS ADC result from `AXP2101_REG_VBUS_H` and returns it in millivolts.
- `axp2101_adc_vsys_read()` reads the decoded VSYS ADC result from `AXP2101_REG_VSYS_H` and returns it in millivolts.
- `axp2101_adc_vbat_read()` reads the decoded VBAT ADC result from `AXP2101_REG_VBAT_H` and returns it in millivolts.
- `axp2101_dcdc_ctrl0_enable()` sets one or more `AXP2101_DCDC_CTRL0_EN_*` bits in `AXP2101_REG_DCDC_CTRL0`.
- `axp2101_dcdc_ctrl0_disable()` performs a masked update on `AXP2101_REG_DCDC_CTRL0` so callers can clear or rewrite selected DCDC enable bits without disturbing the rest.
- `axp2101_dcdc_ctrl0_get()` reads and decodes `AXP2101_REG_DCDC_CTRL0` into `axp2101_dcdc_ctrl0_t`.
- `axp2101_dcdc1_voltage_set()` writes the DCDC1 voltage selection in `AXP2101_REG_DCDC1_V_SET`. Supported values are 1500 mV through 3400 mV in 100 mV steps.
- `axp2101_dcdc1_voltage_get()` reads back the configured DCDC1 voltage selection from `AXP2101_REG_DCDC1_V_SET`.
- `axp2101_ldo_ctrl0_enable()` sets one or more `AXP2101_LDO_CTRL0_EN_*` bits in `AXP2101_REG_LDO_CTRL0`.
- `axp2101_ldo_ctrl0_disable()` performs a masked update on `AXP2101_REG_LDO_CTRL0` so callers can clear or rewrite selected LDO enable bits without disturbing the rest.
- `axp2101_ldo_ctrl0_get()` reads and decodes `AXP2101_REG_LDO_CTRL0` into `axp2101_ldo_ctrl0_t`.
- `axp2101_aldo1_voltage_set()` through `axp2101_aldo4_voltage_set()` write the ALDO voltage selections in `AXP2101_REG_ALDO1_V_SET` through `AXP2101_REG_ALDO4_V_SET`. Supported values are 500 mV through 3500 mV in 100 mV steps.
- `axp2101_aldo1_voltage_get()` through `axp2101_aldo4_voltage_get()` read back the configured ALDO voltage selections.
- `axp2101_bldo1_voltage_set()` and `axp2101_bldo2_voltage_set()` write the BLDO voltage selections in `AXP2101_REG_BLDO1_V_SET` and `AXP2101_REG_BLDO2_V_SET`. Supported values are 500 mV through 3500 mV in 100 mV steps.
- `axp2101_bldo1_voltage_get()` and `axp2101_bldo2_voltage_get()` read back the configured BLDO voltage selections.
- `axp2101_dldo1_voltage_set()` writes the DLDO1 voltage selection in `AXP2101_REG_DLDO1_V_SET`. Supported values are 500 mV through 3300 mV in 100 mV steps.
- `axp2101_dldo1_voltage_get()` reads back the configured DLDO1 voltage selection from `AXP2101_REG_DLDO1_V_SET`.
- `axp2101_reg8_read()` reads one byte from an AXP2101 register.
- `axp2101_reg8_write()` writes one byte to an AXP2101 register.
- `axp2101_reg8_set_bits()` performs a read-modify-write that sets selected bits.
- `axp2101_reg8_update_bits()` performs a masked read-modify-write for direct register access when no higher-level helper exists yet.
- `axp2101_reg14_read()` reads a two-byte AXP2101 register pair encoded as a 14-bit value. Most callers should prefer the dedicated voltage helpers above.

All functions return `II2C_ERR_NONE` on success or an `II2C_ERR_*` value reported by the underlying `ii2c` transport.

Relevant register and bit-mask exports for the newer ADC helpers include:

- `AXP2101_REG_CHARGE_GAUGE_WDT_CTRL`, `AXP2101_CHARGE_GAUGE_WDT_CTRL_GAUGE_EN`, `AXP2101_REG_BAT_DET_CTRL`, `AXP2101_BAT_DET_CTRL_BAT_TYPE_DET_EN`, and `AXP2101_REG_BAT_PERCENT_DATA` for fuel-gauge support.
- `AXP2101_REG_PMU_COMMON_CFG` for PMU common configuration.
- `AXP2101_REG_IRQ_OFF_ON_LEVEL` plus `AXP2101_IRQ_OFF_ON_LEVEL_MASK_IRQ`, `AXP2101_IRQ_OFF_ON_LEVEL_MASK_OFF`, and `AXP2101_IRQ_OFF_ON_LEVEL_MASK_ON` for power-key timing control.
- `AXP2101_REG_ADC_EN` for ADC channel control.
- `AXP2101_REG_VBUS_H`, `AXP2101_REG_VSYS_H`, and `AXP2101_REG_VBAT_H` for the raw voltage register pairs.
- `AXP2101_ADC_EN_VBUS`, `AXP2101_ADC_EN_VSYS`, and `AXP2101_ADC_EN_BATT` for per-channel control.
- `AXP2101_ADC_EN_ALL` when the application wants to enable every ADC channel exported by this component.
- `AXP2101_REG_DCDC_CTRL0` for the DCDC enable register.
- `AXP2101_REG_DCDC1_V_SET` for the DCDC1 voltage-selection register.
- `AXP2101_DCDC_CTRL0_EN_DCDC4`, `AXP2101_DCDC_CTRL0_EN_DCDC3`, `AXP2101_DCDC_CTRL0_EN_DCDC2`, and `AXP2101_DCDC_CTRL0_EN_DCDC1` for per-output control.
- `AXP2101_DCDC_CTRL0_EN_ALL` when the application wants to operate on every exported DCDC enable bit at once.
- `AXP2101_REG_PRECHG_CURRENT_LIMIT`, `AXP2101_REG_CHG_CURRENT_LIMIT`, and `AXP2101_REG_TERM_CHG_CURRENT_CTRL` for charger-current configuration.
- `AXP2101_PRECHG_CURRENT_LIMIT_MASK`, `AXP2101_CHG_CURRENT_LIMIT_MASK`, `AXP2101_TERM_CHG_CURRENT_CTRL_TERM_EN`, and `AXP2101_TERM_CHG_CURRENT_CTRL_TERM_CURRENT_MASK` for the low-level charger-current fields.
- `AXP2101_REG_CHGLED_CTRL`, `AXP2101_CHGLED_CTRL_ENABLE`, `AXP2101_CHGLED_CTRL_FUNCTION_MASK`, and `AXP2101_CHGLED_CTRL_OUTPUT_MASK` for charging LED control.

For the LDO control helpers, the component also exports:

- `AXP2101_REG_LDO_CTRL0` for the LDO enable register.
- `AXP2101_REG_ALDO1_V_SET`, `AXP2101_REG_ALDO2_V_SET`, `AXP2101_REG_ALDO3_V_SET`, and `AXP2101_REG_ALDO4_V_SET` for the ALDO voltage-selection registers.
- `AXP2101_REG_BLDO1_V_SET` and `AXP2101_REG_BLDO2_V_SET` for the BLDO voltage-selection registers.
- `AXP2101_REG_DLDO1_V_SET` for the DLDO1 voltage-selection register.
- `AXP2101_LDO_CTRL0_EN_DLDO1`, `AXP2101_LDO_CTRL0_EN_CPUSLDO`, `AXP2101_LDO_CTRL0_EN_BLDO2`, `AXP2101_LDO_CTRL0_EN_BLDO1`, `AXP2101_LDO_CTRL0_EN_ALDO4`, `AXP2101_LDO_CTRL0_EN_ALDO3`, `AXP2101_LDO_CTRL0_EN_ALDO2`, and `AXP2101_LDO_CTRL0_EN_ALDO1` for per-output control.
- `AXP2101_LDO_CTRL0_EN_ALL` when the application wants to operate on every exported LDO enable bit at once.

The PMU status helpers expose:

- `axp2101_status1_t`, which reports VBUS, BATFET, battery-presence, battery-active, thermal-regulation, and current-limit flags.
- `axp2101_status2_t`, which reports `battery_current_direction`, `system_power_on`, `vindpm_active`, and `charging_status`.
- `axp2101_fuel_gauge_t`, which reports whether battery detection and the gauge are enabled, whether a battery is present, and whether a valid battery percentage is available.
- `axp2101_pmu_common_cfg_t`, which reports raw bits `7:6`, the documented PMU common configuration bits, and the action bits for restart and soft power-off.
- `axp2101_battery_current_direction_t`, with the values `STANDBY`, `CHARGE`, `DISCHARGE`, and `RESERVED`.
- `axp2101_charging_status_t`, with the values `TRI_CHARGE`, `PRE_CHARGE`, `CONSTANT_CHARGE`, `CONSTANT_VOLTAGE`, `CHARGE_DONE`, `NOT_CHARGING`, and `UNKNOWN`.
- `axp2101_irq_off_on_level_t`, which reports the power-key IRQ, power-off, and power-on timing fields using typed enums.
- `axp2101_charger_current_t`, which reports the precharge, constant-charge, and termination current values in milliamps plus the termination-enable flag.
- `axp2101_chgled_function_t`, with the values `TYPE_A`, `TYPE_B`, `REGISTER_CONTROL`, and `RESERVED`.
- `axp2101_chgled_output_t`, with the values `HIZ`, `BLINK_1HZ`, `BLINK_4HZ`, and `DRIVE_LOW`.
- `axp2101_chgled_ctrl_t`, which reports the CHGLED enable bit, function selector, and register-controlled output mode.
- `axp2101_dcdc_ctrl0_t`, which reports the enable state of DCDC4, DCDC3, DCDC2, and DCDC1.

The LDO control helper exposes:

- `axp2101_ldo_ctrl0_t`, which reports the enable state of DLDO1, CPUSLDO, BLDO2, BLDO1, ALDO4, ALDO3, ALDO2, and ALDO1.

## Basic Usage

```c
#include <stdint.h>
#include "ii2c/ii2c.h"
#include "axp2101/axp2101.h"
#include "axp2101/axp2101_register.h"

int example_axp2101_read_voltages(uint16_t *vbus_mv,
                                  uint16_t *vsys_mv,
                                  uint16_t *vbat_mv) {
  if (!vbus_mv || !vsys_mv || !vbat_mv) {
    return II2C_ERR_INVALID_ARG;
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

  ii2c_device_handle_t axp2101 = NULL;
  err = ii2c_new_device(bus, &dev_cfg, &axp2101);
  if (err != II2C_ERR_NONE) {
    ii2c_del_master_bus(bus);
    return err;
  }

  err = axp2101_adc_enable_channels(
      axp2101, AXP2101_ADC_EN_VBUS | AXP2101_ADC_EN_VSYS | AXP2101_ADC_EN_BATT);
  if (err == II2C_ERR_NONE) {
    err = axp2101_adc_vbus_read(axp2101, vbus_mv);
  }
  if (err == II2C_ERR_NONE) {
    err = axp2101_adc_vsys_read(axp2101, vsys_mv);
  }
  if (err == II2C_ERR_NONE) {
    err = axp2101_adc_vbat_read(axp2101, vbat_mv);
  }

  ii2c_del_device(axp2101);
  ii2c_del_master_bus(bus);
  return err;
}
```

Example status read:

```c
int example_axp2101_status_read(ii2c_device_handle_t axp2101) {
  axp2101_status2_t status2 = {0};
  int32_t err = axp2101_status2_get(axp2101, &status2);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  if (status2.vindpm_active) {
    /* Handle input voltage limiting if needed. */
  }

  return II2C_ERR_NONE;
}
```

Example ALDO and BLDO control:

```c
int example_axp2101_configure_ldos(ii2c_device_handle_t axp2101) {
  int32_t err = axp2101_aldo1_voltage_set(axp2101, 1800);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_bldo1_voltage_set(axp2101, 3300);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_ldo_ctrl0_enable(
      axp2101, AXP2101_LDO_CTRL0_EN_ALDO1 | AXP2101_LDO_CTRL0_EN_BLDO1);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint16_t aldo1_mv = 0;
  err = axp2101_aldo1_voltage_get(axp2101, &aldo1_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint16_t bldo1_mv = 0;
  err = axp2101_bldo1_voltage_get(axp2101, &bldo1_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  axp2101_ldo_ctrl0_t ldo0 = {0};
  err = axp2101_ldo_ctrl0_get(axp2101, &ldo0);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  if (!ldo0.aldo1_en || !ldo0.bldo1_en) {
    return II2C_ERR_INVALID_STATE;
  }

  return (aldo1_mv == 1800 && bldo1_mv == 3300) ? II2C_ERR_NONE : II2C_ERR_INVALID_STATE;
}
```

Example DCDC1 control:

```c
int example_axp2101_configure_dcdc1(ii2c_device_handle_t axp2101) {
  int32_t err = axp2101_dcdc1_voltage_set(axp2101, 3300);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_dcdc_ctrl0_enable(axp2101, AXP2101_DCDC_CTRL0_EN_DCDC1);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint16_t dcdc1_mv = 0;
  err = axp2101_dcdc1_voltage_get(axp2101, &dcdc1_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  axp2101_dcdc_ctrl0_t dcdc0 = {0};
  err = axp2101_dcdc_ctrl0_get(axp2101, &dcdc0);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  return (dcdc0.dcdc1_en && dcdc1_mv == 3300) ? II2C_ERR_NONE : II2C_ERR_INVALID_STATE;
}
```

## Notes

- Enable the relevant ADC channels before calling `axp2101_adc_vbus_read()`, `axp2101_adc_vsys_read()`, or `axp2101_adc_vbat_read()`. Those helpers return decoded 14-bit PMIC readings in millivolts.
- Call `axp2101_fuel_gauge_enable()` before depending on `axp2101_fuel_gauge_get()` for battery percentage. If the gauge is disabled or the battery is absent, `battery_percent_valid` will be `false`.
- `axp2101_pmu_common_cfg_set()` writes the full register byte. The `restart_system` and `soft_pwroff` fields map to write-one action bits, so leave them clear unless the caller intentionally wants those side effects.
- Use `axp2101_irq_off_on_level_get()` and `axp2101_irq_off_on_level_set()` for `AXP2101_REG_IRQ_OFF_ON_LEVEL` instead of packing or masking the timing fields in application code.
- `axp2101_charger_current_get()` reports charger configuration values, not live battery current measurements. The decoded fields are read from the PMIC charger-control registers and returned in milliamps.
- `axp2101_chgled_ctrl_set()` rejects the reserved CHGLED function encoding and preserves register bits `7:6` and bit `3` in `AXP2101_REG_CHGLED_CTRL`.
- The termination-current selector in `AXP2101_REG_TERM_CHG_CURRENT_CTRL` is zero-based: selector `0` means `0 mA`, and the valid range currently decoded by this component is `0 mA` through `200 mA` in `25 mA` steps.
- Use `axp2101_dcdc_ctrl0_enable()`, `axp2101_dcdc_ctrl0_disable()`, and `axp2101_dcdc_ctrl0_get()` for the exported `AXP2101_REG_DCDC_CTRL0` workflows instead of open-coding DCDC bit decoding in the application.
- `axp2101_dcdc1_voltage_set()` accepts only 1500 mV through 3400 mV in 100 mV steps. This helper programs the voltage selection but does not enable the rail by itself.
- Use `axp2101_ldo_ctrl0_enable()`, `axp2101_ldo_ctrl0_disable()`, and `axp2101_ldo_ctrl0_get()` for the exported `AXP2101_REG_LDO_CTRL0` workflows instead of open-coding LDO bit decoding in the application.
- `axp2101_aldo1_voltage_set()` through `axp2101_aldo4_voltage_set()` and `axp2101_bldo1_voltage_set()` through `axp2101_bldo2_voltage_set()` accept only 500 mV through 3500 mV in 100 mV steps. `axp2101_dldo1_voltage_set()` accepts only 500 mV through 3300 mV in 100 mV steps. These helpers program the voltage selection but do not enable the rail by themselves.
- The AXP2101 DLDO1 datasheet text is inconsistent: some summaries say `0.5-3.4 V`, but the `REG99H` selector table says `29 steps`, encodes `11100` as `3.3 V`, and marks `11101-11111` as reserved. This component follows the register table semantics and therefore caps DLDO1 at `3.3 V`.
- The matching `*_voltage_get()` helpers return `II2C_ERR_INVALID_STATE` if the PMIC register contains a reserved selector value.
- The component exports `AXP2101_REG_PMU_STATUS1`, `AXP2101_REG_PMU_STATUS2`, `AXP2101_REG_IRQ_OFF_ON_LEVEL`, `AXP2101_REG_ADC_EN`, `AXP2101_REG_VBUS_H`, `AXP2101_REG_VSYS_H`, `AXP2101_REG_VBAT_H`, `AXP2101_REG_PRECHG_CURRENT_LIMIT`, `AXP2101_REG_CHG_CURRENT_LIMIT`, `AXP2101_REG_TERM_CHG_CURRENT_CTRL`, `AXP2101_REG_LDO_CTRL0`, `AXP2101_REG_ALDO1_V_SET` through `AXP2101_REG_ALDO4_V_SET`, `AXP2101_REG_BLDO1_V_SET`, `AXP2101_REG_BLDO2_V_SET`, and `AXP2101_REG_DLDO1_V_SET`, plus the `AXP2101_IRQ_OFF_ON_LEVEL_*`, `AXP2101_ADC_EN_*`, and `AXP2101_LDO_CTRL0_EN_*` bit masks. Other AXP2101 register workflows still use the generic `axp2101_reg8_*` and `axp2101_reg14_read()` helpers.
- `axp2101_reg8_set_bits()` and `axp2101_reg8_update_bits()` both perform read-modify-write cycles, so they preserve unrelated bits in the target register.
- `axp2101_reg14_read()` expects the high-byte register address for a 14-bit two-byte PMIC value and masks the top byte down to the lower 6 bits before combining the result.
- The example above uses the same device address and GPIO pattern that appear in `main/main.c`, but board wiring is still application-specific.
