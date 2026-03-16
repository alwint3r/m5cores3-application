/**
 * @file axp2101.h
 * @brief Public AXP2101 register-access helpers built on top of `ii2c`.
 * @ingroup axp2101
 */
/**
 * @defgroup axp2101 AXP2101
 * @brief Public AXP2101 register-access helpers built on top of `ii2c`.
 *
 * This component does not define a dedicated `axp2101` handle type. Callers
 * first attach the PMIC as an `ii2c_device_handle_t`, then pass that handle to
 * these helper functions.
 * @{
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <ii2c/ii2c.h>

typedef struct axp2101_status1_data axp2101_status1_t;
/** @brief Decoded bit fields from `AXP2101_REG_PMU_STATUS1`. */
struct axp2101_status1_data {
  bool vbus_good;
  bool batfet_open;
  bool battery_present;
  bool battery_active;
  bool thermal_regulated;
  bool current_limited;
};

typedef struct axp2101_ldo_ctrl0_data axp2101_ldo_ctrl0_t;
/** @brief Decoded enable-state bits from `AXP2101_REG_LDO_CTRL0`. */
struct axp2101_ldo_ctrl0_data {
  /** @brief True when DLDO1 is enabled. */
  bool dldo1_en;
  /** @brief True when CPUSLDO is enabled. */
  bool cpusldo_en;
  /** @brief True when BLDO2 is enabled. */
  bool bldo2_en;
  /** @brief True when BLDO1 is enabled. */
  bool bldo1_en;
  /** @brief True when ALDO4 is enabled. */
  bool aldo4_en;
  /** @brief True when ALDO3 is enabled. */
  bool aldo3_en;
  /** @brief True when ALDO2 is enabled. */
  bool aldo2_en;
  /** @brief True when ALDO1 is enabled. */
  bool aldo1_en;
};

typedef struct axp2101_dcdc_ctrl0_data axp2101_dcdc_ctrl0_t;
/** @brief Decoded enable-state bits from `AXP2101_REG_DCDC_CTRL0`. */
struct axp2101_dcdc_ctrl0_data {
  /** @brief True when DCDC4 is enabled. */
  bool dcdc4_en;
  /** @brief True when DCDC3 is enabled. */
  bool dcdc3_en;
  /** @brief True when DCDC2 is enabled. */
  bool dcdc2_en;
  /** @brief True when DCDC1 is enabled. */
  bool dcdc1_en;
};

/**
 * @brief Read and decode `AXP2101_REG_PMU_STATUS1`.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out Output pointer that receives the decoded status fields.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_status1_get(ii2c_device_handle_t dev, axp2101_status1_t *out);

/** @brief Battery current direction reported by PMU Status 2 bits 6:5. */
typedef enum axp2101_battery_current_direction {
  /** @brief Battery current is effectively idle. */
  AXP2101_BATTERY_CURRENT_DIRECTION_STANDBY = 0,
  /** @brief Battery current is flowing into the battery. */
  AXP2101_BATTERY_CURRENT_DIRECTION_CHARGE = 1,
  /** @brief Battery current is flowing out of the battery. */
  AXP2101_BATTERY_CURRENT_DIRECTION_DISCHARGE = 2,
  /** @brief Reserved encoding returned by the PMIC. */
  AXP2101_BATTERY_CURRENT_DIRECTION_RESERVED = 3,
} axp2101_battery_current_direction_t;

/** @brief Charging state reported by PMU Status 2 bits 2:0. */
typedef enum axp2101_charging_status {
  /** @brief Trickle-charge phase. */
  AXP2101_CHARGING_STATUS_TRI_CHARGE = 0,
  /** @brief Pre-charge phase. */
  AXP2101_CHARGING_STATUS_PRE_CHARGE = 1,
  /** @brief Constant-current charge phase. */
  AXP2101_CHARGING_STATUS_CONSTANT_CHARGE = 2,
  /** @brief Constant-voltage charge phase. */
  AXP2101_CHARGING_STATUS_CONSTANT_VOLTAGE = 3,
  /** @brief Charging has completed. */
  AXP2101_CHARGING_STATUS_CHARGE_DONE = 4,
  /** @brief Charger is idle even though charging support is available. */
  AXP2101_CHARGING_STATUS_NOT_CHARGING = 5,
  /** @brief Any PMIC encoding not currently assigned by this component. */
  AXP2101_CHARGING_STATUS_UNKNOWN = 6,
} axp2101_charging_status_t;

typedef struct axp2101_status2_data axp2101_status2_t;
/** @brief Decoded bit fields from `AXP2101_REG_PMU_STATUS2`. */
struct axp2101_status2_data {
  /** @brief Battery current direction decoded from bits 6:5. */
  axp2101_battery_current_direction_t battery_current_direction;
  /** @brief True when the PMIC reports the system is powered on. */
  bool system_power_on;
  /** @brief True when the PMIC reports the input is limited by VINDPM. */
  bool vindpm_active;
  /** @brief Charging phase decoded from bits 2:0. */
  axp2101_charging_status_t charging_status;
};

/**
 * @brief Read and decode `AXP2101_REG_PMU_STATUS2`.
 *
 * This helper converts the multi-bit state fields into enums so callers do
 * not need to interpret raw register bits in application code.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out Output pointer that receives the decoded status fields.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_status2_get(ii2c_device_handle_t dev, axp2101_status2_t *out);

typedef struct axp2101_fuel_gauge_data axp2101_fuel_gauge_t;
/** @brief Decoded AXP2101 fuel-gauge state. */
struct axp2101_fuel_gauge_data {
  /** @brief True when battery detection is enabled in `AXP2101_REG_BAT_DET_CTRL`. */
  bool battery_detection_enabled;
  /** @brief True when the fuel gauge is enabled in `AXP2101_REG_CHARGE_GAUGE_WDT_CTRL`. */
  bool fuel_gauge_enabled;
  /** @brief True when the PMIC currently reports a battery is present. */
  bool battery_present;
  /** @brief True when `battery_percent` contains a valid gauge reading. */
  bool battery_percent_valid;
  /** @brief Battery state-of-charge percentage from `AXP2101_REG_BAT_PERCENT_DATA`. */
  uint8_t battery_percent;
};

/**
 * @brief Enable the AXP2101 fuel-gauge path used by this project.
 *
 * This helper enables the PMIC fuel-gauge block and the battery-detection
 * path used by external reference implementations on this hardware.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_fuel_gauge_enable(ii2c_device_handle_t dev);

/**
 * @brief Read the current decoded fuel-gauge state.
 *
 * The helper reports whether battery detection and the gauge are enabled,
 * whether a battery is present, and, when available, the decoded battery
 * percentage. If the battery is absent or the gauge path is disabled,
 * `battery_percent_valid` is returned as `false`.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out Output pointer that receives the decoded fuel-gauge fields.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `out` is
 * `NULL`, `II2C_ERR_INVALID_STATE` when the PMIC reports an impossible battery
 * percentage above 100, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_fuel_gauge_get(ii2c_device_handle_t dev, axp2101_fuel_gauge_t *out);

typedef struct axp2101_pmu_common_cfg_data axp2101_pmu_common_cfg_t;
/** @brief Decoded fields from `AXP2101_REG_PMU_COMMON_CFG`. */
struct axp2101_pmu_common_cfg_data {
  /** @brief Raw value of writable but undocumented bits 7:6. */
  uint8_t raw_bits_7_6;
  /** @brief True when internal off-discharge is enabled for DCDC, LDO, and SWITCH. */
  bool internal_off_discharge_enabled;
  /** @brief Raw value of writable but undocumented bit 4. */
  bool raw_bit4;
  /** @brief True when PWROK pull-low is allowed to restart the system. */
  bool pwrok_restart_enabled;
  /** @brief True when holding PWRON for 16 seconds can shut down the PMIC. */
  bool pwron_16s_shutdown_enabled;
  /** @brief True requests the write-one restart action on the next setter call. */
  bool restart_system;
  /** @brief True requests the write-one soft-poweroff action on the next setter call. */
  bool soft_pwroff;
};

/**
 * @brief Read and decode `AXP2101_REG_PMU_COMMON_CFG`.
 *
 * This helper exposes the documented control bits and also returns the
 * writable-but-undocumented fields so callers can preserve them accurately.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out Output pointer that receives the decoded register fields.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `out` is
 * `NULL`, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_pmu_common_cfg_get(ii2c_device_handle_t dev, axp2101_pmu_common_cfg_t *out);

/**
 * @brief Write `AXP2101_REG_PMU_COMMON_CFG` from a decoded struct.
 *
 * The `restart_system` and `soft_pwroff` fields map to write-one action bits.
 * Callers should leave them `false` unless they intentionally want the PMIC
 * to perform those actions.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param config Input pointer that provides the register fields to write.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `config` is
 * `NULL` or `raw_bits_7_6` exceeds the two-bit range, or an `II2C_ERR_*` code
 * from `ii2c`.
 */
int32_t axp2101_pmu_common_cfg_set(ii2c_device_handle_t dev,
                                   const axp2101_pmu_common_cfg_t *config);

/** @brief Power-key IRQ timing choices used by `AXP2101_REG_IRQ_OFF_ON_LEVEL` bits 5:4. */
typedef enum axp2101_power_key_irq_time {
  /** @brief 1 second IRQ timing. */
  AXP2101_POWER_KEY_IRQ_TIME_1S = 0,
  /** @brief 1.5 second IRQ timing. */
  AXP2101_POWER_KEY_IRQ_TIME_1_5S = 1,
  /** @brief 2 second IRQ timing. */
  AXP2101_POWER_KEY_IRQ_TIME_2S = 2,
  /** @brief 2.5 second IRQ timing. */
  AXP2101_POWER_KEY_IRQ_TIME_2_5S = 3,
} axp2101_power_key_irq_time_t;

/** @brief Long-press power-off timing choices used by `AXP2101_REG_IRQ_OFF_ON_LEVEL`. */
typedef enum axp2101_power_key_poweroff_time {
  /** @brief Power off after 4 seconds. */
  AXP2101_POWER_KEY_POWEROFF_TIME_4S = 0,
  /** @brief Power off after 6 seconds. */
  AXP2101_POWER_KEY_POWEROFF_TIME_6S = 1,
  /** @brief Power off after 8 seconds. */
  AXP2101_POWER_KEY_POWEROFF_TIME_8S = 2,
  /** @brief Power off after 10 seconds. */
  AXP2101_POWER_KEY_POWEROFF_TIME_10S = 3,
} axp2101_power_key_poweroff_time_t;

/** @brief Power-key power-on timing choices used by `AXP2101_REG_IRQ_OFF_ON_LEVEL` bits 1:0. */
typedef enum axp2101_power_key_on_time {
  /** @brief 128 millisecond power-on timing. */
  AXP2101_POWER_KEY_ON_TIME_128MS = 0,
  /** @brief 512 millisecond power-on timing. */
  AXP2101_POWER_KEY_ON_TIME_512MS = 1,
  /** @brief 1 second power-on timing. */
  AXP2101_POWER_KEY_ON_TIME_1S = 2,
  /** @brief 2 second power-on timing. */
  AXP2101_POWER_KEY_ON_TIME_2S = 3,
} axp2101_power_key_on_time_t;

typedef struct axp2101_irq_off_on_level_data axp2101_irq_off_on_level_t;
/** @brief Decoded fields from `AXP2101_REG_IRQ_OFF_ON_LEVEL`. */
struct axp2101_irq_off_on_level_data {
  /** @brief Press time required to trigger the PMIC power-key IRQ path. */
  axp2101_power_key_irq_time_t irq_time;
  /** @brief Press time required for PMIC power-off. */
  axp2101_power_key_poweroff_time_t poweroff_time;
  /** @brief Press time required for PMIC power-on. */
  axp2101_power_key_on_time_t poweron_time;
};

/**
 * @brief Read and decode `AXP2101_REG_IRQ_OFF_ON_LEVEL`.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out Output pointer that receives the decoded timing fields.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `out` is
 * `NULL`, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_irq_off_on_level_get(ii2c_device_handle_t dev, axp2101_irq_off_on_level_t *out);

/**
 * @brief Program selected fields in `AXP2101_REG_IRQ_OFF_ON_LEVEL`.
 *
 * This helper updates the IRQ timing, long-press power-off timing, and
 * power-on timing fields while preserving any other bits in the register.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param config Input pointer that provides the decoded timing fields.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `config` is
 * `NULL` or contains an out-of-range enum value, or an `II2C_ERR_*` code from
 * `ii2c`.
 */
int32_t axp2101_irq_off_on_level_set(ii2c_device_handle_t dev,
                                     const axp2101_irq_off_on_level_t *config);

typedef struct axp2101_charger_current_data axp2101_charger_current_t;
/** @brief Decoded charger-current configuration values. */
struct axp2101_charger_current_data {
  /** @brief Precharge current limit decoded from `AXP2101_REG_PRECHG_CURRENT_LIMIT`. */
  uint16_t precharge_current_ma;
  /** @brief Constant-charge current limit decoded from `AXP2101_REG_CHG_CURRENT_LIMIT`. */
  uint16_t constant_charge_current_ma;
  /** @brief Termination current limit decoded from `AXP2101_REG_TERM_CHG_CURRENT_CTRL`. */
  uint16_t termination_current_ma;
  /** @brief True when charger current termination is enabled. */
  bool termination_enabled;
};

/**
 * @brief Read and decode the charger-current configuration registers.
 *
 * This helper reads `AXP2101_REG_PRECHG_CURRENT_LIMIT`,
 * `AXP2101_REG_CHG_CURRENT_LIMIT`, and `AXP2101_REG_TERM_CHG_CURRENT_CTRL`,
 * then decodes the stored current limits into milliamps.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out Output pointer that receives the decoded charger-current fields.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `out` is
 * `NULL`, `II2C_ERR_INVALID_STATE` when any register contains an unsupported
 * selector value, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_charger_current_get(ii2c_device_handle_t dev, axp2101_charger_current_t *out);

/** @brief CHGLED function choices used by `AXP2101_REG_CHGLED_CTRL` bits 2:1. */
typedef enum axp2101_chgled_function {
  /** @brief Hardware-controlled charging indicator mode A. */
  AXP2101_CHGLED_FUNCTION_TYPE_A = 0,
  /** @brief Hardware-controlled charging indicator mode B. */
  AXP2101_CHGLED_FUNCTION_TYPE_B = 1,
  /** @brief Use bits 5:4 for direct CHGLED pin output control. */
  AXP2101_CHGLED_FUNCTION_REGISTER_CONTROL = 2,
  /** @brief Reserved encoding reported by the PMIC. */
  AXP2101_CHGLED_FUNCTION_RESERVED = 3,
} axp2101_chgled_function_t;

/** @brief CHGLED output choices used by `AXP2101_REG_CHGLED_CTRL` bits 5:4. */
typedef enum axp2101_chgled_output {
  /** @brief High-impedance output. */
  AXP2101_CHGLED_OUTPUT_HIZ = 0,
  /** @brief 1 Hz low/Hi-Z blink with 25%/75% duty cycle. */
  AXP2101_CHGLED_OUTPUT_BLINK_1HZ = 1,
  /** @brief 4 Hz low/Hi-Z blink with 25%/75% duty cycle. */
  AXP2101_CHGLED_OUTPUT_BLINK_4HZ = 2,
  /** @brief Drive the CHGLED output low. */
  AXP2101_CHGLED_OUTPUT_DRIVE_LOW = 3,
} axp2101_chgled_output_t;

typedef struct axp2101_chgled_ctrl_data axp2101_chgled_ctrl_t;
/** @brief Decoded CHGLED control fields from `AXP2101_REG_CHGLED_CTRL`. */
struct axp2101_chgled_ctrl_data {
  /** @brief True when the CHGLED pin function is enabled. */
  bool enabled;
  /** @brief CHGLED function-select field decoded from bits 2:1. */
  axp2101_chgled_function_t function;
  /** @brief Register-controlled CHGLED output field decoded from bits 5:4. */
  axp2101_chgled_output_t output;
};

/**
 * @brief Read and decode `AXP2101_REG_CHGLED_CTRL`.
 *
 * The decoded `output` field is always returned, even when the active
 * `function` does not currently use bits 5:4.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out Output pointer that receives the decoded CHGLED fields.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `out` is
 * `NULL`, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_chgled_ctrl_get(ii2c_device_handle_t dev, axp2101_chgled_ctrl_t *out);

/**
 * @brief Program selected fields in `AXP2101_REG_CHGLED_CTRL`.
 *
 * This helper updates the CHGLED enable bit, function selector, and
 * register-controlled output field while preserving reserved bits in the
 * register. The reserved function encoding is rejected.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param config Input pointer that provides the decoded CHGLED fields.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `config` is
 * `NULL` or contains an unsupported enum value, or an `II2C_ERR_*` code from
 * `ii2c`.
 */
int32_t axp2101_chgled_ctrl_set(ii2c_device_handle_t dev, const axp2101_chgled_ctrl_t *config);

/**
 * @brief Enable one or more ADC measurement channels.
 *
 * This helper sets the selected bits in `AXP2101_REG_ADC_EN` while preserving
 * the other bits already present in that register.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param channels_bits Bitwise OR of `AXP2101_ADC_EN_*` values to enable.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_adc_enable_channels(ii2c_device_handle_t dev, uint8_t channels_bits);

/**
 * @brief Disable one or more ADC measurement channels.
 *
 * This helper clears the selected bits in `AXP2101_REG_ADC_EN` while
 * preserving the other bits already present in that register.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param channels_bits Bitwise OR of `AXP2101_ADC_EN_*` values to disable.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_adc_disable_channels(ii2c_device_handle_t dev, uint8_t channels_bits);

/**
 * @brief Read the decoded VBUS ADC value in millivolts.
 *
 * Callers should enable `AXP2101_ADC_EN_VBUS` first with
 * `axp2101_adc_enable_channels()`.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out_mv Output pointer that receives the decoded VBUS value in mV.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_adc_vbus_read(ii2c_device_handle_t dev, uint16_t *out_mv);

/**
 * @brief Read the decoded VSYS ADC value in millivolts.
 *
 * Callers should enable `AXP2101_ADC_EN_VSYS` first with
 * `axp2101_adc_enable_channels()`.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out_mv Output pointer that receives the decoded VSYS value in mV.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_adc_vsys_read(ii2c_device_handle_t dev, uint16_t *out_mv);

/**
 * @brief Read the decoded VBAT ADC value in millivolts.
 *
 * Callers should enable `AXP2101_ADC_EN_BATT` first with
 * `axp2101_adc_enable_channels()`.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out_mv Output pointer that receives the decoded VBAT value in mV.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_adc_vbat_read(ii2c_device_handle_t dev, uint16_t *out_mv);

/**
 * @brief Enable one or more DCDC outputs controlled by `AXP2101_REG_DCDC_CTRL0`.
 *
 * This helper sets the selected enable bits while preserving the other bits
 * already present in the register.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param dcdc_bits Bitwise OR of `AXP2101_DCDC_CTRL0_EN_*` values to enable.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_dcdc_ctrl0_enable(ii2c_device_handle_t dev, uint8_t dcdc_bits);

/**
 * @brief Update one or more DCDC enable bits in `AXP2101_REG_DCDC_CTRL0`.
 *
 * This helper performs a masked read-modify-write. Bits selected by `mask`
 * are replaced with the corresponding values from `dcdc_bits`; all other bits
 * remain unchanged. To clear specific outputs, pass those bits in `mask` and
 * leave them clear in `dcdc_bits`.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param mask Bitwise OR of `AXP2101_DCDC_CTRL0_EN_*` values to update.
 * @param dcdc_bits Replacement bit values applied under `mask`.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_dcdc_ctrl0_disable(ii2c_device_handle_t dev, uint8_t mask, uint8_t dcdc_bits);

/**
 * @brief Read and decode `AXP2101_REG_DCDC_CTRL0`.
 *
 * This helper converts each exported DCDC enable bit into a boolean field in
 * `axp2101_dcdc_ctrl0_t`.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out Output pointer that receives the decoded enable states.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `out` is
 * `NULL`, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_dcdc_ctrl0_get(ii2c_device_handle_t dev, axp2101_dcdc_ctrl0_t *out);

/**
 * @brief Program the DCDC1 output voltage in millivolts.
 *
 * This helper writes `AXP2101_REG_DCDC1_V_SET` using the AXP2101 DCDC1
 * encoding. Valid values are 1500 mV through 3400 mV in 100 mV steps.
 * Programming the voltage does not enable the DCDC1 rail; use
 * `axp2101_dcdc_ctrl0_enable()` separately when the output should be turned
 * on.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param mv Requested DCDC1 output voltage in mV.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `mv` is
 * outside the supported range or step size, or an `II2C_ERR_*` code from
 * `ii2c`.
 */
int32_t axp2101_dcdc1_voltage_set(ii2c_device_handle_t dev, uint16_t mv);

/**
 * @brief Read back the configured DCDC1 output voltage in millivolts.
 *
 * This helper reads `AXP2101_REG_DCDC1_V_SET` and decodes the stored voltage
 * selection. It reports the programmed DCDC1 voltage setting only; use
 * `axp2101_dcdc_ctrl0_get()` if the caller also needs to know whether DCDC1
 * is currently enabled.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out_mv Output pointer that receives the decoded DCDC1 voltage in mV.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `out_mv` is
 * `NULL`, `II2C_ERR_INVALID_STATE` when the register stores a reserved
 * selector, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_dcdc1_voltage_get(ii2c_device_handle_t dev, uint16_t *out_mv);

/**
 * @brief Enable one or more LDO outputs controlled by `AXP2101_REG_LDO_CTRL0`.
 *
 * This helper sets the selected enable bits while preserving the other bits
 * already present in the register.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param ldo_bits Bitwise OR of `AXP2101_LDO_CTRL0_EN_*` values to enable.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_ldo_ctrl0_enable(ii2c_device_handle_t dev, uint8_t ldo_bits);

/**
 * @brief Update one or more LDO enable bits in `AXP2101_REG_LDO_CTRL0`.
 *
 * This helper performs a masked read-modify-write. Bits selected by `mask`
 * are replaced with the corresponding values from `ldo_bits`; all other bits
 * remain unchanged. To clear specific outputs, pass those bits in `mask` and
 * leave them clear in `ldo_bits`.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param mask Bitwise OR of `AXP2101_LDO_CTRL0_EN_*` values to update.
 * @param ldo_bits Replacement bit values applied under `mask`.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_ldo_ctrl0_disable(ii2c_device_handle_t dev, uint8_t mask, uint8_t ldo_bits);

/**
 * @brief Read and decode `AXP2101_REG_LDO_CTRL0`.
 *
 * This helper converts each exported LDO enable bit into a boolean field in
 * `axp2101_ldo_ctrl0_t`.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out Output pointer that receives the decoded enable states.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_ldo_ctrl0_get(ii2c_device_handle_t dev, axp2101_ldo_ctrl0_t *out);

/**
 * @brief Program the ALDO1 output voltage in millivolts.
 *
 * This helper writes `AXP2101_REG_ALDO1_V_SET` using the encoding currently
 * exported by this component. Valid values are 500 mV through 3500 mV in
 * 100 mV steps. Programming the voltage does not enable the ALDO1 rail; use
 * `axp2101_ldo_ctrl0_enable()` separately when the output should be turned on.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param mv Requested ALDO1 output voltage in mV.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `mv` is
 * outside the supported range or step size, or an `II2C_ERR_*` code from
 * `ii2c`.
 */
int32_t axp2101_aldo1_voltage_set(ii2c_device_handle_t dev, uint16_t mv);

/**
 * @brief Read back the configured ALDO1 output voltage in millivolts.
 *
 * This helper reads `AXP2101_REG_ALDO1_V_SET` and decodes the stored voltage
 * selection. It reports the programmed ALDO1 voltage setting only; use
 * `axp2101_ldo_ctrl0_get()` if the caller also needs to know whether ALDO1 is
 * currently enabled.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out_mv Output pointer that receives the decoded ALDO1 voltage in mV.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `out_mv` is
 * `NULL`, `II2C_ERR_INVALID_STATE` when the register stores a reserved
 * selector, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_aldo1_voltage_get(ii2c_device_handle_t dev, uint16_t *out_mv);

/**
 * @brief Program the ALDO2 output voltage in millivolts.
 *
 * Valid values are 500 mV through 3500 mV in 100 mV steps. Programming the
 * voltage does not enable the ALDO2 rail; use `axp2101_ldo_ctrl0_enable()`
 * separately when the output should be turned on.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param mv Requested ALDO2 output voltage in mV.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `mv` is
 * outside the supported range or step size, or an `II2C_ERR_*` code from
 * `ii2c`.
 */
int32_t axp2101_aldo2_voltage_set(ii2c_device_handle_t dev, uint16_t mv);

/**
 * @brief Read back the configured ALDO2 output voltage in millivolts.
 *
 * This helper reads `AXP2101_REG_ALDO2_V_SET` and decodes the stored voltage
 * selection. It reports the programmed ALDO2 voltage setting only; use
 * `axp2101_ldo_ctrl0_get()` if the caller also needs to know whether ALDO2 is
 * currently enabled.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out_mv Output pointer that receives the decoded ALDO2 voltage in mV.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `out_mv` is
 * `NULL`, `II2C_ERR_INVALID_STATE` when the register stores a reserved
 * selector, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_aldo2_voltage_get(ii2c_device_handle_t dev, uint16_t *out_mv);

/**
 * @brief Program the ALDO3 output voltage in millivolts.
 *
 * Valid values are 500 mV through 3500 mV in 100 mV steps. Programming the
 * voltage does not enable the ALDO3 rail; use `axp2101_ldo_ctrl0_enable()`
 * separately when the output should be turned on.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param mv Requested ALDO3 output voltage in mV.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `mv` is
 * outside the supported range or step size, or an `II2C_ERR_*` code from
 * `ii2c`.
 */
int32_t axp2101_aldo3_voltage_set(ii2c_device_handle_t dev, uint16_t mv);

/**
 * @brief Read back the configured ALDO3 output voltage in millivolts.
 *
 * This helper reads `AXP2101_REG_ALDO3_V_SET` and decodes the stored voltage
 * selection. It reports the programmed ALDO3 voltage setting only; use
 * `axp2101_ldo_ctrl0_get()` if the caller also needs to know whether ALDO3 is
 * currently enabled.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out_mv Output pointer that receives the decoded ALDO3 voltage in mV.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `out_mv` is
 * `NULL`, `II2C_ERR_INVALID_STATE` when the register stores a reserved
 * selector, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_aldo3_voltage_get(ii2c_device_handle_t dev, uint16_t *out_mv);

/**
 * @brief Program the ALDO4 output voltage in millivolts.
 *
 * Valid values are 500 mV through 3500 mV in 100 mV steps. Programming the
 * voltage does not enable the ALDO4 rail; use `axp2101_ldo_ctrl0_enable()`
 * separately when the output should be turned on.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param mv Requested ALDO4 output voltage in mV.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `mv` is
 * outside the supported range or step size, or an `II2C_ERR_*` code from
 * `ii2c`.
 */
int32_t axp2101_aldo4_voltage_set(ii2c_device_handle_t dev, uint16_t mv);

/**
 * @brief Read back the configured ALDO4 output voltage in millivolts.
 *
 * This helper reads `AXP2101_REG_ALDO4_V_SET` and decodes the stored voltage
 * selection. It reports the programmed ALDO4 voltage setting only; use
 * `axp2101_ldo_ctrl0_get()` if the caller also needs to know whether ALDO4 is
 * currently enabled.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out_mv Output pointer that receives the decoded ALDO4 voltage in mV.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `out_mv` is
 * `NULL`, `II2C_ERR_INVALID_STATE` when the register stores a reserved
 * selector, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_aldo4_voltage_get(ii2c_device_handle_t dev, uint16_t *out_mv);

/**
 * @brief Program the BLDO1 output voltage in millivolts.
 *
 * Valid values are 500 mV through 3500 mV in 100 mV steps. Programming the
 * voltage does not enable the BLDO1 rail; use `axp2101_ldo_ctrl0_enable()`
 * separately when the output should be turned on.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param mv Requested BLDO1 output voltage in mV.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `mv` is
 * outside the supported range or step size, or an `II2C_ERR_*` code from
 * `ii2c`.
 */
int32_t axp2101_bldo1_voltage_set(ii2c_device_handle_t dev, uint16_t mv);

/**
 * @brief Read back the configured BLDO1 output voltage in millivolts.
 *
 * This helper reads `AXP2101_REG_BLDO1_V_SET` and decodes the stored voltage
 * selection. It reports the programmed BLDO1 voltage setting only; use
 * `axp2101_ldo_ctrl0_get()` if the caller also needs to know whether BLDO1 is
 * currently enabled.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out_mv Output pointer that receives the decoded BLDO1 voltage in mV.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `out_mv` is
 * `NULL`, `II2C_ERR_INVALID_STATE` when the register stores a reserved
 * selector, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_bldo1_voltage_get(ii2c_device_handle_t dev, uint16_t *out_mv);

/**
 * @brief Program the BLDO2 output voltage in millivolts.
 *
 * Valid values are 500 mV through 3500 mV in 100 mV steps. Programming the
 * voltage does not enable the BLDO2 rail; use `axp2101_ldo_ctrl0_enable()`
 * separately when the output should be turned on.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param mv Requested BLDO2 output voltage in mV.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `mv` is
 * outside the supported range or step size, or an `II2C_ERR_*` code from
 * `ii2c`.
 */
int32_t axp2101_bldo2_voltage_set(ii2c_device_handle_t dev, uint16_t mv);

/**
 * @brief Read back the configured BLDO2 output voltage in millivolts.
 *
 * This helper reads `AXP2101_REG_BLDO2_V_SET` and decodes the stored voltage
 * selection. It reports the programmed BLDO2 voltage setting only; use
 * `axp2101_ldo_ctrl0_get()` if the caller also needs to know whether BLDO2 is
 * currently enabled.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out_mv Output pointer that receives the decoded BLDO2 voltage in mV.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `out_mv` is
 * `NULL`, `II2C_ERR_INVALID_STATE` when the register stores a reserved
 * selector, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_bldo2_voltage_get(ii2c_device_handle_t dev, uint16_t *out_mv);

/**
 * @brief Program the DLDO1 output voltage in millivolts.
 *
 * Valid values are 500 mV through 3300 mV in 100 mV steps. Programming the
 * voltage does not enable the DLDO1 rail; use `axp2101_ldo_ctrl0_enable()`
 * separately when the output should be turned on.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param mv Requested DLDO1 output voltage in mV.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `mv` is
 * outside the supported range or step size, or an `II2C_ERR_*` code from
 * `ii2c`.
 */
int32_t axp2101_dldo1_voltage_set(ii2c_device_handle_t dev, uint16_t mv);

/**
 * @brief Read back the configured DLDO1 output voltage in millivolts.
 *
 * This helper reads `AXP2101_REG_DLDO1_V_SET` and decodes the stored voltage
 * selection. It reports the programmed DLDO1 voltage setting only; use
 * `axp2101_ldo_ctrl0_get()` if the caller also needs to know whether DLDO1 is
 * currently enabled.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param out_mv Output pointer that receives the decoded DLDO1 voltage in mV.
 * @return `II2C_ERR_NONE` on success, `II2C_ERR_INVALID_ARG` when `out_mv` is
 * `NULL`, `II2C_ERR_INVALID_STATE` when the register stores a reserved
 * selector, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_dldo1_voltage_get(ii2c_device_handle_t dev, uint16_t *out_mv);

/**
 * @brief Write one byte to an AXP2101 register.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param reg Register address to write.
 * @param value New byte value to store in the register.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_reg8_write(ii2c_device_handle_t dev, uint8_t reg, uint8_t value);

/**
 * @brief Read one byte from an AXP2101 register.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param reg Register address to read.
 * @param out_value Output pointer that receives the register byte.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_reg8_read(ii2c_device_handle_t dev, uint8_t reg, uint8_t *out_value);

/**
 * @brief Set one or more bits in an 8-bit AXP2101 register.
 *
 * This helper performs a read-modify-write cycle: it reads the current
 * register byte, ORs in `bits`, then writes the updated byte back.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param reg Register address to update.
 * @param bits Bit mask to force high.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_reg8_set_bits(ii2c_device_handle_t dev, uint8_t reg, uint8_t bits);

/**
 * @brief Update selected bits in an 8-bit AXP2101 register.
 *
 * This helper performs a read-modify-write cycle. Only bits selected by
 * `mask` are replaced with the corresponding bits from `new_value`; all other
 * bits keep their previous state.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param reg Register address to update.
 * @param mask Bit mask selecting which register bits to replace.
 * @param new_value Replacement value applied under `mask`.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_reg8_update_bits(ii2c_device_handle_t dev,
                                 uint8_t reg,
                                 uint8_t mask,
                                 uint8_t new_value);

/**
 * @brief Read a 14-bit value from a two-byte AXP2101 register pair.
 *
 * This helper reads the byte at `reg` and the following byte, then combines
 * them as `(high & 0x3F) << 8 | low`. Most callers should prefer the
 * dedicated ADC voltage helpers when reading VBUS, VSYS, or VBAT.
 *
 * @param dev Attached `ii2c` device handle for the AXP2101.
 * @param reg Address of the high byte in the two-byte register pair.
 * @param out_value Output pointer that receives the decoded 14-bit value.
 * @return `II2C_ERR_NONE` on success, or an `II2C_ERR_*` code from `ii2c`.
 */
int32_t axp2101_reg14_read(ii2c_device_handle_t dev, uint8_t reg, uint16_t *out_value);

#ifdef __cplusplus
}
#endif

/** @} */  // end of axp2101
