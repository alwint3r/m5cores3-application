#include "axp2101/axp2101.h"
#include "axp2101/axp2101_register.h"

#include <stdbool.h>

typedef struct axp2101_voltage_spec_data {
  uint8_t reg;
  uint8_t selector_mask;
  uint16_t min_mv;
  uint16_t max_mv;
  uint16_t step_mv;
} axp2101_voltage_spec_t;

static const axp2101_voltage_spec_t AXP2101_VOLTAGE_SPEC_ALDO_BLDO = {
    .reg = 0,
    .selector_mask = 0x1F,
    .min_mv = 500,
    .max_mv = 3500,
    .step_mv = 100,
};

static const axp2101_voltage_spec_t AXP2101_VOLTAGE_SPEC_DLDO1 = {
    /*
     * The AXP2101 datasheets we checked contain inconsistent DLDO1 wording:
     * one line says 0.5-3.4 V, but the register table also says 29 steps,
     * encodes 11100 as 3.3 V, and marks 11101-11111 as reserved. The register
     * encoding therefore supports 0.5-3.3 V in 100 mV steps.
     */
    .reg = AXP2101_REG_DLDO1_V_SET,
    .selector_mask = 0x1F,
    .min_mv = 500,
    .max_mv = 3300,
    .step_mv = 100,
};

static const axp2101_voltage_spec_t AXP2101_VOLTAGE_SPEC_DCDC1 = {
    .reg = AXP2101_REG_DCDC1_V_SET,
    .selector_mask = 0x1F,
    .min_mv = 1500,
    .max_mv = 3400,
    .step_mv = 100,
};

static const uint16_t AXP2101_CONSTANT_CHARGE_CURRENT_TABLE_MA[] = {
    0,   25,  50,  75,  100, 125, 150,  175,  200,  300,
    400, 500, 600, 700, 800, 900, 1000, 1200, 1400, 1500,
};

static bool axp2101_has_write(const axp2101_t *pmic) {
  return pmic != NULL && pmic->transport_write != NULL;
}

static bool axp2101_has_combined_read(const axp2101_t *pmic) {
  return pmic != NULL && pmic->transport_write_read != NULL;
}

static int32_t axp2101_transport_write_exact(axp2101_t *pmic,
                                             const uint8_t *write_buffer,
                                             size_t write_size) {
  if (!axp2101_has_write(pmic)) {
    return pmic == NULL ? AXP2101_ERR_INVALID_ARG : AXP2101_ERR_INVALID_STATE;
  }

  if (write_buffer == NULL || write_size == 0U) {
    return AXP2101_ERR_INVALID_ARG;
  }

  return pmic->transport_write(write_buffer, write_size);
}

static int32_t axp2101_transport_read_exact(axp2101_t *pmic,
                                            const uint8_t *write_buffer,
                                            size_t write_size,
                                            uint8_t *read_buffer,
                                            size_t read_size) {
  if (pmic == NULL || write_buffer == NULL || write_size == 0U || read_buffer == NULL ||
      read_size == 0U) {
    return AXP2101_ERR_INVALID_ARG;
  }

  if (!axp2101_has_combined_read(pmic)) {
    return AXP2101_ERR_INVALID_STATE;
  }

  size_t actual_read_size = read_size;
  int32_t err = pmic->transport_write_read(
      write_buffer, write_size, read_buffer, &actual_read_size, read_size);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  return actual_read_size == read_size ? AXP2101_ERR_NONE : AXP2101_ERR_IO;
}

const char *axp2101_err_to_name(int32_t err) {
  switch (err) {
    case AXP2101_ERR_NONE:
      return "AXP2101_ERR_NONE";
    case AXP2101_ERR_FAIL:
      return "AXP2101_ERR_FAIL";
    case AXP2101_ERR_NO_MEM:
      return "AXP2101_ERR_NO_MEM";
    case AXP2101_ERR_INVALID_ARG:
      return "AXP2101_ERR_INVALID_ARG";
    case AXP2101_ERR_INVALID_STATE:
      return "AXP2101_ERR_INVALID_STATE";
    case AXP2101_ERR_NOT_FOUND:
      return "AXP2101_ERR_NOT_FOUND";
    case AXP2101_ERR_NOT_SUPPORTED:
      return "AXP2101_ERR_NOT_SUPPORTED";
    case AXP2101_ERR_TIMEOUT:
      return "AXP2101_ERR_TIMEOUT";
    case AXP2101_ERR_IO:
      return "AXP2101_ERR_IO";
    default:
      return "AXP2101_ERR_UNKNOWN";
  }
}

static bool axp2101_power_key_irq_time_valid(axp2101_power_key_irq_time_t value) {
  return value >= AXP2101_POWER_KEY_IRQ_TIME_1S && value <= AXP2101_POWER_KEY_IRQ_TIME_2_5S;
}

static bool axp2101_power_key_poweroff_time_valid(axp2101_power_key_poweroff_time_t value) {
  return value >= AXP2101_POWER_KEY_POWEROFF_TIME_4S &&
         value <= AXP2101_POWER_KEY_POWEROFF_TIME_10S;
}

static bool axp2101_power_key_on_time_valid(axp2101_power_key_on_time_t value) {
  return value >= AXP2101_POWER_KEY_ON_TIME_128MS && value <= AXP2101_POWER_KEY_ON_TIME_2S;
}

static int32_t axp2101_precharge_current_decode(uint8_t selector, uint16_t *out_ma) {
  if (!out_ma) {
    return AXP2101_ERR_INVALID_ARG;
  }
  if (selector > 8) {
    return AXP2101_ERR_INVALID_STATE;
  }

  *out_ma = (uint16_t)selector * 25;
  return AXP2101_ERR_NONE;
}

static int32_t axp2101_constant_charge_current_decode(uint8_t selector, uint16_t *out_ma) {
  if (!out_ma) {
    return AXP2101_ERR_INVALID_ARG;
  }

  if (selector < 9) {
    *out_ma = (uint16_t)selector * 25;
    return AXP2101_ERR_NONE;
  }

  if (selector < sizeof(AXP2101_CONSTANT_CHARGE_CURRENT_TABLE_MA) /
                     sizeof(AXP2101_CONSTANT_CHARGE_CURRENT_TABLE_MA[0])) {
    *out_ma = AXP2101_CONSTANT_CHARGE_CURRENT_TABLE_MA[selector];
    return AXP2101_ERR_NONE;
  }

  /*
   * The datasheet groups the remaining encodings at the top of the selector
   * range into the 1500 mA setting rather than assigning larger currents.
   */
  if (selector <= AXP2101_CHG_CURRENT_LIMIT_MASK) {
    *out_ma = 1500;
    return AXP2101_ERR_NONE;
  }

  return AXP2101_ERR_INVALID_STATE;
}

static int32_t axp2101_termination_current_decode(uint8_t selector, uint16_t *out_ma) {
  if (!out_ma) {
    return AXP2101_ERR_INVALID_ARG;
  }
  if (selector > 8) {
    return AXP2101_ERR_INVALID_STATE;
  }

  *out_ma = (uint16_t)selector * 25;
  return AXP2101_ERR_NONE;
}

static bool axp2101_chgled_function_valid(axp2101_chgled_function_t value) {
  return value >= AXP2101_CHGLED_FUNCTION_TYPE_A &&
         value <= AXP2101_CHGLED_FUNCTION_REGISTER_CONTROL;
}

static bool axp2101_chgled_output_valid(axp2101_chgled_output_t value) {
  return value >= AXP2101_CHGLED_OUTPUT_HIZ && value <= AXP2101_CHGLED_OUTPUT_DRIVE_LOW;
}

static bool axp2101_pmu_common_cfg_raw_bits_7_6_valid(uint8_t value) {
  return value <= 0x03;
}

static axp2101_charging_status_t axp2101_charging_status_from_bits(uint8_t bits) {
  switch (bits & 0x07) {
    case 0:
      return AXP2101_CHARGING_STATUS_TRI_CHARGE;
    case 1:
      return AXP2101_CHARGING_STATUS_PRE_CHARGE;
    case 2:
      return AXP2101_CHARGING_STATUS_CONSTANT_CHARGE;
    case 3:
      return AXP2101_CHARGING_STATUS_CONSTANT_VOLTAGE;
    case 4:
      return AXP2101_CHARGING_STATUS_CHARGE_DONE;
    case 5:
      return AXP2101_CHARGING_STATUS_NOT_CHARGING;
    default:
      return AXP2101_CHARGING_STATUS_UNKNOWN;
  }
}

int32_t axp2101_status1_get(axp2101_t *pmic, axp2101_status1_t *out) {
  if (!out) {
    return AXP2101_ERR_INVALID_ARG;
  }

  uint8_t status1_reg = 0;
  int32_t err = axp2101_reg8_read(pmic, AXP2101_REG_PMU_STATUS1, &status1_reg);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  out->vbus_good = (status1_reg & 32) == 32;
  out->batfet_open = (status1_reg & 16) == 16;
  out->battery_present = (status1_reg & 8) == 8;
  out->battery_active = (status1_reg & 4) == 4;
  out->thermal_regulated = (status1_reg & 2) == 2;
  out->current_limited = (status1_reg & 1) == 1;

  return AXP2101_ERR_NONE;
}

int32_t axp2101_status2_get(axp2101_t *pmic, axp2101_status2_t *out) {
  if (!out) {
    return AXP2101_ERR_INVALID_ARG;
  }

  uint8_t status2_reg = 0;
  int32_t err = axp2101_reg8_read(pmic, AXP2101_REG_PMU_STATUS2, &status2_reg);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  out->battery_current_direction = (axp2101_battery_current_direction_t)((status2_reg >> 5) & 0x03);
  out->system_power_on = (status2_reg & 16) == 16;
  out->vindpm_active = (status2_reg & 8) == 8;
  out->charging_status = axp2101_charging_status_from_bits(status2_reg);

  return AXP2101_ERR_NONE;
}

int32_t axp2101_fuel_gauge_enable(axp2101_t *pmic) {
  int32_t err = axp2101_reg8_update_bits(pmic,
                                         AXP2101_REG_CHARGE_GAUGE_WDT_CTRL,
                                         AXP2101_CHARGE_GAUGE_WDT_CTRL_GAUGE_EN,
                                         AXP2101_CHARGE_GAUGE_WDT_CTRL_GAUGE_EN);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  return axp2101_reg8_update_bits(pmic,
                                  AXP2101_REG_BAT_DET_CTRL,
                                  AXP2101_BAT_DET_CTRL_BAT_TYPE_DET_EN,
                                  AXP2101_BAT_DET_CTRL_BAT_TYPE_DET_EN);
}

int32_t axp2101_fuel_gauge_get(axp2101_t *pmic, axp2101_fuel_gauge_t *out) {
  if (!out) {
    return AXP2101_ERR_INVALID_ARG;
  }

  uint8_t gauge_ctrl = 0;
  int32_t err = axp2101_reg8_read(pmic, AXP2101_REG_CHARGE_GAUGE_WDT_CTRL, &gauge_ctrl);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  uint8_t bat_det_ctrl = 0;
  err = axp2101_reg8_read(pmic, AXP2101_REG_BAT_DET_CTRL, &bat_det_ctrl);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  uint8_t status1_reg = 0;
  err = axp2101_reg8_read(pmic, AXP2101_REG_PMU_STATUS1, &status1_reg);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  out->fuel_gauge_enabled = (gauge_ctrl & AXP2101_CHARGE_GAUGE_WDT_CTRL_GAUGE_EN) ==
                            AXP2101_CHARGE_GAUGE_WDT_CTRL_GAUGE_EN;
  out->battery_detection_enabled =
      (bat_det_ctrl & AXP2101_BAT_DET_CTRL_BAT_TYPE_DET_EN) == AXP2101_BAT_DET_CTRL_BAT_TYPE_DET_EN;
  out->battery_present = (status1_reg & 8) == 8;
  out->battery_percent_valid = false;
  out->battery_percent = 0;

  if (!out->fuel_gauge_enabled || !out->battery_present) {
    return AXP2101_ERR_NONE;
  }

  uint8_t battery_percent = 0;
  err = axp2101_reg8_read(pmic, AXP2101_REG_BAT_PERCENT_DATA, &battery_percent);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }
  if (battery_percent > 100) {
    return AXP2101_ERR_INVALID_STATE;
  }

  out->battery_percent = battery_percent;
  out->battery_percent_valid = true;
  return AXP2101_ERR_NONE;
}

int32_t axp2101_pmu_common_cfg_get(axp2101_t *pmic, axp2101_pmu_common_cfg_t *out) {
  if (!out) {
    return AXP2101_ERR_INVALID_ARG;
  }

  uint8_t reg_value = 0;
  int32_t err = axp2101_reg8_read(pmic, AXP2101_REG_PMU_COMMON_CFG, &reg_value);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  out->raw_bits_7_6 = (uint8_t)((reg_value & AXP2101_PMU_COMMON_CFG_RAW_BITS_7_6_MASK) >> 6);
  out->internal_off_discharge_enabled =
      (reg_value & AXP2101_PMU_COMMON_CFG_INTERNAL_OFF_DISCHARGE_EN) ==
      AXP2101_PMU_COMMON_CFG_INTERNAL_OFF_DISCHARGE_EN;
  out->raw_bit4 = (reg_value & AXP2101_PMU_COMMON_CFG_RAW_BIT4) == AXP2101_PMU_COMMON_CFG_RAW_BIT4;
  out->pwrok_restart_enabled = (reg_value & AXP2101_PMU_COMMON_CFG_PWROK_RESTART_EN) ==
                               AXP2101_PMU_COMMON_CFG_PWROK_RESTART_EN;
  out->pwron_16s_shutdown_enabled = (reg_value & AXP2101_PMU_COMMON_CFG_PWRON_16S_SHUTDOWN_EN) ==
                                    AXP2101_PMU_COMMON_CFG_PWRON_16S_SHUTDOWN_EN;
  out->restart_system =
      (reg_value & AXP2101_PMU_COMMON_CFG_RESTART_SYSTEM) == AXP2101_PMU_COMMON_CFG_RESTART_SYSTEM;
  out->soft_pwroff =
      (reg_value & AXP2101_PMU_COMMON_CFG_SOFT_PWROFF) == AXP2101_PMU_COMMON_CFG_SOFT_PWROFF;

  return AXP2101_ERR_NONE;
}

int32_t axp2101_pmu_common_cfg_set(axp2101_t *pmic,
                                   const axp2101_pmu_common_cfg_t *config) {
  if (!config || !axp2101_pmu_common_cfg_raw_bits_7_6_valid(config->raw_bits_7_6)) {
    return AXP2101_ERR_INVALID_ARG;
  }

  uint8_t reg_value = (uint8_t)(config->raw_bits_7_6 << 6);
  if (config->internal_off_discharge_enabled) {
    reg_value |= AXP2101_PMU_COMMON_CFG_INTERNAL_OFF_DISCHARGE_EN;
  }
  if (config->raw_bit4) {
    reg_value |= AXP2101_PMU_COMMON_CFG_RAW_BIT4;
  }
  if (config->pwrok_restart_enabled) {
    reg_value |= AXP2101_PMU_COMMON_CFG_PWROK_RESTART_EN;
  }
  if (config->pwron_16s_shutdown_enabled) {
    reg_value |= AXP2101_PMU_COMMON_CFG_PWRON_16S_SHUTDOWN_EN;
  }
  if (config->restart_system) {
    reg_value |= AXP2101_PMU_COMMON_CFG_RESTART_SYSTEM;
  }
  if (config->soft_pwroff) {
    reg_value |= AXP2101_PMU_COMMON_CFG_SOFT_PWROFF;
  }

  return axp2101_reg8_write(pmic, AXP2101_REG_PMU_COMMON_CFG, reg_value);
}

int32_t axp2101_irq_off_on_level_get(axp2101_t *pmic, axp2101_irq_off_on_level_t *out) {
  if (!out) {
    return AXP2101_ERR_INVALID_ARG;
  }

  uint8_t reg_value = 0;
  int32_t err = axp2101_reg8_read(pmic, AXP2101_REG_IRQ_OFF_ON_LEVEL, &reg_value);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  out->irq_time =
      (axp2101_power_key_irq_time_t)((reg_value & AXP2101_IRQ_OFF_ON_LEVEL_MASK_IRQ) >> 4);
  out->poweroff_time =
      (axp2101_power_key_poweroff_time_t)((reg_value & AXP2101_IRQ_OFF_ON_LEVEL_MASK_OFF) >> 2);
  out->poweron_time = (axp2101_power_key_on_time_t)(reg_value & AXP2101_IRQ_OFF_ON_LEVEL_MASK_ON);

  return AXP2101_ERR_NONE;
}

int32_t axp2101_irq_off_on_level_set(axp2101_t *pmic,
                                     const axp2101_irq_off_on_level_t *config) {
  if (!config) {
    return AXP2101_ERR_INVALID_ARG;
  }
  if (!axp2101_power_key_irq_time_valid(config->irq_time) ||
      !axp2101_power_key_poweroff_time_valid(config->poweroff_time) ||
      !axp2101_power_key_on_time_valid(config->poweron_time)) {
    return AXP2101_ERR_INVALID_ARG;
  }

  uint8_t new_value =
      (uint8_t)((((uint8_t)config->irq_time) << 4) | (((uint8_t)config->poweroff_time) << 2) |
                ((uint8_t)config->poweron_time));
  uint8_t mask = AXP2101_IRQ_OFF_ON_LEVEL_MASK_IRQ | AXP2101_IRQ_OFF_ON_LEVEL_MASK_OFF |
                 AXP2101_IRQ_OFF_ON_LEVEL_MASK_ON;

  return axp2101_reg8_update_bits(pmic, AXP2101_REG_IRQ_OFF_ON_LEVEL, mask, new_value);
}

int32_t axp2101_charger_current_get(axp2101_t *pmic, axp2101_charger_current_t *out) {
  if (!out) {
    return AXP2101_ERR_INVALID_ARG;
  }

  uint8_t reg61 = 0;
  int32_t err = axp2101_reg8_read(pmic, AXP2101_REG_PRECHG_CURRENT_LIMIT, &reg61);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  uint8_t reg62 = 0;
  err = axp2101_reg8_read(pmic, AXP2101_REG_CHG_CURRENT_LIMIT, &reg62);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  uint8_t reg63 = 0;
  err = axp2101_reg8_read(pmic, AXP2101_REG_TERM_CHG_CURRENT_CTRL, &reg63);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = axp2101_precharge_current_decode(reg61 & AXP2101_PRECHG_CURRENT_LIMIT_MASK,
                                         &out->precharge_current_ma);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = axp2101_constant_charge_current_decode(reg62 & AXP2101_CHG_CURRENT_LIMIT_MASK,
                                               &out->constant_charge_current_ma);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = axp2101_termination_current_decode(reg63 & AXP2101_TERM_CHG_CURRENT_CTRL_TERM_CURRENT_MASK,
                                           &out->termination_current_ma);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  out->termination_enabled =
      (reg63 & AXP2101_TERM_CHG_CURRENT_CTRL_TERM_EN) == AXP2101_TERM_CHG_CURRENT_CTRL_TERM_EN;

  return AXP2101_ERR_NONE;
}

int32_t axp2101_chgled_ctrl_get(axp2101_t *pmic, axp2101_chgled_ctrl_t *out) {
  if (!out) {
    return AXP2101_ERR_INVALID_ARG;
  }

  uint8_t reg_value = 0;
  int32_t err = axp2101_reg8_read(pmic, AXP2101_REG_CHGLED_CTRL, &reg_value);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  out->enabled = (reg_value & AXP2101_CHGLED_CTRL_ENABLE) == AXP2101_CHGLED_CTRL_ENABLE;
  out->function = (axp2101_chgled_function_t)((reg_value & AXP2101_CHGLED_CTRL_FUNCTION_MASK) >> 1);
  out->output = (axp2101_chgled_output_t)((reg_value & AXP2101_CHGLED_CTRL_OUTPUT_MASK) >> 4);

  return AXP2101_ERR_NONE;
}

int32_t axp2101_chgled_ctrl_set(axp2101_t *pmic, const axp2101_chgled_ctrl_t *config) {
  if (!config) {
    return AXP2101_ERR_INVALID_ARG;
  }
  if (!axp2101_chgled_function_valid(config->function) ||
      !axp2101_chgled_output_valid(config->output)) {
    return AXP2101_ERR_INVALID_ARG;
  }

  uint8_t new_value = 0;
  if (config->enabled) {
    new_value |= AXP2101_CHGLED_CTRL_ENABLE;
  }
  new_value |= (uint8_t)(((uint8_t)config->function) << 1);
  new_value |= (uint8_t)(((uint8_t)config->output) << 4);

  uint8_t mask = AXP2101_CHGLED_CTRL_ENABLE | AXP2101_CHGLED_CTRL_FUNCTION_MASK |
                 AXP2101_CHGLED_CTRL_OUTPUT_MASK;
  return axp2101_reg8_update_bits(pmic, AXP2101_REG_CHGLED_CTRL, mask, new_value);
}

int32_t axp2101_adc_enable_channels(axp2101_t *pmic, uint8_t channels_bits) {
  return axp2101_reg8_update_bits(pmic, AXP2101_REG_ADC_EN, channels_bits, channels_bits);
}

int32_t axp2101_adc_disable_channels(axp2101_t *pmic, uint8_t channel_bits) {
  return axp2101_reg8_update_bits(pmic, AXP2101_REG_ADC_EN, channel_bits, 0);
}

int32_t axp2101_adc_vbat_read(axp2101_t *pmic, uint16_t *out_mv) {
  if (!out_mv) {
    return AXP2101_ERR_INVALID_ARG;
  }
  return axp2101_reg14_read(pmic, AXP2101_REG_VBAT_H, out_mv);
}

int32_t axp2101_adc_vbus_read(axp2101_t *pmic, uint16_t *out_mv) {
  if (!out_mv) {
    return AXP2101_ERR_INVALID_ARG;
  }

  return axp2101_reg14_read(pmic, AXP2101_REG_VBUS_H, out_mv);
}

int32_t axp2101_adc_vsys_read(axp2101_t *pmic, uint16_t *out_mv) {
  if (!out_mv) {
    return AXP2101_ERR_INVALID_ARG;
  }

  return axp2101_reg14_read(pmic, AXP2101_REG_VSYS_H, out_mv);
}

int32_t axp2101_dcdc_ctrl0_enable(axp2101_t *pmic, uint8_t dcdc_bits) {
  return axp2101_reg8_update_bits(pmic, AXP2101_REG_DCDC_CTRL0, dcdc_bits, dcdc_bits);
}

int32_t axp2101_dcdc_ctrl0_disable(axp2101_t *pmic, uint8_t mask, uint8_t dcdc_bits) {
  return axp2101_reg8_update_bits(pmic, AXP2101_REG_DCDC_CTRL0, mask, dcdc_bits);
}

int32_t axp2101_dcdc_ctrl0_get(axp2101_t *pmic, axp2101_dcdc_ctrl0_t *out) {
  if (!out) {
    return AXP2101_ERR_INVALID_ARG;
  }

  uint8_t dcdc_ctrl0 = 0;
  int32_t err = axp2101_reg8_read(pmic, AXP2101_REG_DCDC_CTRL0, &dcdc_ctrl0);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  out->dcdc4_en = (dcdc_ctrl0 & AXP2101_DCDC_CTRL0_EN_DCDC4) == AXP2101_DCDC_CTRL0_EN_DCDC4;
  out->dcdc3_en = (dcdc_ctrl0 & AXP2101_DCDC_CTRL0_EN_DCDC3) == AXP2101_DCDC_CTRL0_EN_DCDC3;
  out->dcdc2_en = (dcdc_ctrl0 & AXP2101_DCDC_CTRL0_EN_DCDC2) == AXP2101_DCDC_CTRL0_EN_DCDC2;
  out->dcdc1_en = (dcdc_ctrl0 & AXP2101_DCDC_CTRL0_EN_DCDC1) == AXP2101_DCDC_CTRL0_EN_DCDC1;

  return AXP2101_ERR_NONE;
}

int32_t axp2101_ldo_ctrl0_enable(axp2101_t *pmic, uint8_t ldo_bits) {
  return axp2101_reg8_update_bits(pmic, AXP2101_REG_LDO_CTRL0, ldo_bits, ldo_bits);
}

int32_t axp2101_ldo_ctrl0_disable(axp2101_t *pmic, uint8_t mask, uint8_t ldo_bits) {
  return axp2101_reg8_update_bits(pmic, AXP2101_REG_LDO_CTRL0, mask, ldo_bits);
}

int32_t axp2101_ldo_ctrl0_get(axp2101_t *pmic, axp2101_ldo_ctrl0_t *out) {
  if (!out) {
    return AXP2101_ERR_INVALID_ARG;
  }

  uint8_t ldo_ctrl0 = 0;
  int32_t err = axp2101_reg8_read(pmic, AXP2101_REG_LDO_CTRL0, &ldo_ctrl0);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  out->dldo1_en = (ldo_ctrl0 & AXP2101_LDO_CTRL0_EN_DLDO1) == AXP2101_LDO_CTRL0_EN_DLDO1;
  out->cpusldo_en = (ldo_ctrl0 & AXP2101_LDO_CTRL0_EN_CPUSLDO) == AXP2101_LDO_CTRL0_EN_CPUSLDO;
  out->bldo2_en = (ldo_ctrl0 & AXP2101_LDO_CTRL0_EN_BLDO2) == AXP2101_LDO_CTRL0_EN_BLDO2;
  out->bldo1_en = (ldo_ctrl0 & AXP2101_LDO_CTRL0_EN_BLDO1) == AXP2101_LDO_CTRL0_EN_BLDO1;
  out->aldo4_en = (ldo_ctrl0 & AXP2101_LDO_CTRL0_EN_ALDO4) == AXP2101_LDO_CTRL0_EN_ALDO4;
  out->aldo3_en = (ldo_ctrl0 & AXP2101_LDO_CTRL0_EN_ALDO3) == AXP2101_LDO_CTRL0_EN_ALDO3;
  out->aldo2_en = (ldo_ctrl0 & AXP2101_LDO_CTRL0_EN_ALDO2) == AXP2101_LDO_CTRL0_EN_ALDO2;
  out->aldo1_en = (ldo_ctrl0 & AXP2101_LDO_CTRL0_EN_ALDO1) == AXP2101_LDO_CTRL0_EN_ALDO1;

  return AXP2101_ERR_NONE;
}

static int32_t axp2101_voltage_value_validate(uint16_t value,
                                              uint16_t step,
                                              uint16_t min,
                                              uint16_t max) {
  if (value < min || value > max) {
    return AXP2101_ERR_INVALID_ARG;
  }
  if (((uint16_t)(value - min) % step) != 0) {
    return AXP2101_ERR_INVALID_ARG;
  }

  return AXP2101_ERR_NONE;
}

static uint8_t axp2101_voltage_selector_max(const axp2101_voltage_spec_t *spec) {
  return (uint8_t)((spec->max_mv - spec->min_mv) / spec->step_mv);
}

static int32_t axp2101_voltage_set(axp2101_t *pmic,
                                   const axp2101_voltage_spec_t *spec,
                                   uint16_t mv) {
  int32_t err = axp2101_voltage_value_validate(mv, spec->step_mv, spec->min_mv, spec->max_mv);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  uint8_t selector = (uint8_t)((mv - spec->min_mv) / spec->step_mv);
  return axp2101_reg8_update_bits(pmic, spec->reg, spec->selector_mask, selector);
}

static int32_t axp2101_voltage_get(axp2101_t *pmic,
                                   const axp2101_voltage_spec_t *spec,
                                   uint16_t *out_mv) {
  if (!out_mv) {
    return AXP2101_ERR_INVALID_ARG;
  }

  uint8_t reg_value = 0;
  int32_t err = axp2101_reg8_read(pmic, spec->reg, &reg_value);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  uint8_t selector = reg_value & spec->selector_mask;
  if (selector > axp2101_voltage_selector_max(spec)) {
    return AXP2101_ERR_INVALID_STATE;
  }

  *out_mv = spec->min_mv + ((uint16_t)selector * spec->step_mv);
  return AXP2101_ERR_NONE;
}

static int32_t axp2101_aldo_bldo_voltage_set(axp2101_t *pmic, uint8_t reg, uint16_t mv) {
  axp2101_voltage_spec_t spec = AXP2101_VOLTAGE_SPEC_ALDO_BLDO;
  spec.reg = reg;
  return axp2101_voltage_set(pmic, &spec, mv);
}

static int32_t axp2101_aldo_bldo_voltage_get(axp2101_t *pmic,
                                             uint8_t reg,
                                             uint16_t *out_mv) {
  axp2101_voltage_spec_t spec = AXP2101_VOLTAGE_SPEC_ALDO_BLDO;
  spec.reg = reg;
  return axp2101_voltage_get(pmic, &spec, out_mv);
}

int32_t axp2101_dcdc1_voltage_set(axp2101_t *pmic, uint16_t mv) {
  return axp2101_voltage_set(pmic, &AXP2101_VOLTAGE_SPEC_DCDC1, mv);
}

int32_t axp2101_dcdc1_voltage_get(axp2101_t *pmic, uint16_t *out_mv) {
  return axp2101_voltage_get(pmic, &AXP2101_VOLTAGE_SPEC_DCDC1, out_mv);
}

int32_t axp2101_aldo1_voltage_set(axp2101_t *pmic, uint16_t mv) {
  return axp2101_aldo_bldo_voltage_set(pmic, AXP2101_REG_ALDO1_V_SET, mv);
}

int32_t axp2101_aldo1_voltage_get(axp2101_t *pmic, uint16_t *out_mv) {
  return axp2101_aldo_bldo_voltage_get(pmic, AXP2101_REG_ALDO1_V_SET, out_mv);
}

int32_t axp2101_aldo2_voltage_set(axp2101_t *pmic, uint16_t mv) {
  return axp2101_aldo_bldo_voltage_set(pmic, AXP2101_REG_ALDO2_V_SET, mv);
}

int32_t axp2101_aldo2_voltage_get(axp2101_t *pmic, uint16_t *out_mv) {
  return axp2101_aldo_bldo_voltage_get(pmic, AXP2101_REG_ALDO2_V_SET, out_mv);
}

int32_t axp2101_aldo3_voltage_set(axp2101_t *pmic, uint16_t mv) {
  return axp2101_aldo_bldo_voltage_set(pmic, AXP2101_REG_ALDO3_V_SET, mv);
}

int32_t axp2101_aldo3_voltage_get(axp2101_t *pmic, uint16_t *out_mv) {
  return axp2101_aldo_bldo_voltage_get(pmic, AXP2101_REG_ALDO3_V_SET, out_mv);
}

int32_t axp2101_aldo4_voltage_set(axp2101_t *pmic, uint16_t mv) {
  return axp2101_aldo_bldo_voltage_set(pmic, AXP2101_REG_ALDO4_V_SET, mv);
}

int32_t axp2101_aldo4_voltage_get(axp2101_t *pmic, uint16_t *out_mv) {
  return axp2101_aldo_bldo_voltage_get(pmic, AXP2101_REG_ALDO4_V_SET, out_mv);
}

int32_t axp2101_bldo1_voltage_set(axp2101_t *pmic, uint16_t mv) {
  return axp2101_aldo_bldo_voltage_set(pmic, AXP2101_REG_BLDO1_V_SET, mv);
}

int32_t axp2101_bldo1_voltage_get(axp2101_t *pmic, uint16_t *out_mv) {
  return axp2101_aldo_bldo_voltage_get(pmic, AXP2101_REG_BLDO1_V_SET, out_mv);
}

int32_t axp2101_bldo2_voltage_set(axp2101_t *pmic, uint16_t mv) {
  return axp2101_aldo_bldo_voltage_set(pmic, AXP2101_REG_BLDO2_V_SET, mv);
}

int32_t axp2101_bldo2_voltage_get(axp2101_t *pmic, uint16_t *out_mv) {
  return axp2101_aldo_bldo_voltage_get(pmic, AXP2101_REG_BLDO2_V_SET, out_mv);
}

int32_t axp2101_dldo1_voltage_set(axp2101_t *pmic, uint16_t mv) {
  return axp2101_voltage_set(pmic, &AXP2101_VOLTAGE_SPEC_DLDO1, mv);
}

int32_t axp2101_dldo1_voltage_get(axp2101_t *pmic, uint16_t *out_mv) {
  return axp2101_voltage_get(pmic, &AXP2101_VOLTAGE_SPEC_DLDO1, out_mv);
}

int32_t axp2101_reg8_read(axp2101_t *pmic, uint8_t reg, uint8_t *out_value) {
  if (out_value == NULL) {
    return AXP2101_ERR_INVALID_ARG;
  }

  return axp2101_transport_read_exact(pmic, &reg, 1, out_value, 1);
}

int32_t axp2101_reg8_write(axp2101_t *pmic, uint8_t reg, uint8_t value) {
  return axp2101_transport_write_exact(pmic, (uint8_t[2]){reg, value}, 2);
}

int32_t axp2101_reg8_set_bits(axp2101_t *pmic, uint8_t reg, uint8_t bits) {
  uint8_t current_value = 0;
  int32_t err = axp2101_reg8_read(pmic, reg, &current_value);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  return axp2101_reg8_write(pmic, reg, current_value | bits);
}

int32_t axp2101_reg8_update_bits(axp2101_t *pmic,
                                 uint8_t reg,
                                 uint8_t mask,
                                 uint8_t new_value) {
  uint8_t current_value = 0;
  int32_t err = axp2101_reg8_read(pmic, reg, &current_value);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  current_value = (new_value & mask) | (current_value & ~mask);

  return axp2101_reg8_write(pmic, reg, current_value);
}

int32_t axp2101_reg14_read(axp2101_t *pmic, uint8_t reg, uint16_t *out_value) {
  if (!out_value) {
    return AXP2101_ERR_INVALID_ARG;
  }

  uint8_t data[2] = {0};
  int32_t err = axp2101_transport_read_exact(pmic, &reg, 1, data, sizeof(data));
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  *out_value = ((data[0] & 0x3F) << 8) | data[1];
  return AXP2101_ERR_NONE;
}
