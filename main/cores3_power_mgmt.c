#include "cores3_power_mgmt.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <axp2101/axp2101_register.h>
#include <ii2c/ii2c.h>

static const uint8_t CORES3_AW9523B_BOOST_EN_PORT = 1;
static const uint8_t CORES3_AW9523B_BOOST_EN_PIN = 7;
static const uint16_t CORES3_AXP2101_DCDC1_LCD_PWR_MV = 3300;

static const char *bool_to_yes_no(bool value) {
  return value ? "yes" : "no";
}

const char *cores3_power_mgmt_err_to_name(int32_t err) {
  if (err >= AW9523B_ERR_BASE && err < (AW9523B_ERR_BASE + 0x100)) {
    return aw9523b_err_to_name(err);
  }
  if (err >= AXP2101_ERR_BASE && err < (AXP2101_ERR_BASE + 0x100)) {
    return axp2101_err_to_name(err);
  }

  return ii2c_err_to_name(err);
}

static const char *axp2101_charging_status_to_string(axp2101_charging_status_t status) {
  switch (status) {
    case AXP2101_CHARGING_STATUS_TRI_CHARGE:
      return "trickle";
    case AXP2101_CHARGING_STATUS_PRE_CHARGE:
      return "pre-charge";
    case AXP2101_CHARGING_STATUS_CONSTANT_CHARGE:
      return "constant-current";
    case AXP2101_CHARGING_STATUS_CONSTANT_VOLTAGE:
      return "constant-voltage";
    case AXP2101_CHARGING_STATUS_CHARGE_DONE:
      return "charge-done";
    case AXP2101_CHARGING_STATUS_NOT_CHARGING:
      return "idle";
    case AXP2101_CHARGING_STATUS_UNKNOWN:
      return "unknown";
  }

  return "unknown";
}

static int32_t configure_aw9523b_boost_enable(aw9523b_t *expander) {
  int32_t err = aw9523b_port_dir_set(expander,
                                     CORES3_AW9523B_BOOST_EN_PORT,
                                     CORES3_AW9523B_BOOST_EN_PIN,
                                     AW9523B_PORT_DIRECTION_OUTPUT);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  err = aw9523b_level_set(expander, CORES3_AW9523B_BOOST_EN_PORT, CORES3_AW9523B_BOOST_EN_PIN, 1);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  uint8_t boost_en_level = 0;
  err = aw9523b_level_get(
      expander, CORES3_AW9523B_BOOST_EN_PORT, CORES3_AW9523B_BOOST_EN_PIN, &boost_en_level);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  if (boost_en_level == 0) {
    return AW9523B_ERR_INVALID_STATE;
  }

  puts("AW9523B BOOST_EN asserted");
  return AW9523B_ERR_NONE;
}

static int32_t configure_axp2101_ldos(axp2101_t *pmic) {
  uint8_t ldo_mask = AXP2101_LDO_CTRL0_EN_DLDO1 | AXP2101_LDO_CTRL0_EN_BLDO2 |
                     AXP2101_LDO_CTRL0_EN_BLDO1 | AXP2101_LDO_CTRL0_EN_ALDO4 |
                     AXP2101_LDO_CTRL0_EN_ALDO3 | AXP2101_LDO_CTRL0_EN_ALDO2 |
                     AXP2101_LDO_CTRL0_EN_ALDO1;

  int32_t err = axp2101_ldo_ctrl0_enable(pmic, ldo_mask);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  axp2101_ldo_ctrl0_t ldo_state = {0};
  err = axp2101_ldo_ctrl0_get(pmic, &ldo_state);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  if (!ldo_state.dldo1_en || ldo_state.cpusldo_en || !ldo_state.bldo2_en || !ldo_state.bldo1_en ||
      !ldo_state.aldo4_en || !ldo_state.aldo3_en || !ldo_state.aldo2_en || !ldo_state.aldo1_en) {
    return AXP2101_ERR_INVALID_STATE;
  }

  err = axp2101_aldo1_voltage_set(pmic, 1800);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo2_voltage_set(pmic, 3300);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo3_voltage_set(pmic, 3300);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo4_voltage_set(pmic, 3300);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  uint16_t aldo1_mv = 0;
  uint16_t aldo2_mv = 0;
  uint16_t aldo3_mv = 0;
  uint16_t aldo4_mv = 0;

  err = axp2101_aldo1_voltage_get(pmic, &aldo1_mv);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo2_voltage_get(pmic, &aldo2_mv);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo3_voltage_get(pmic, &aldo3_mv);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo4_voltage_get(pmic, &aldo4_mv);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  if (aldo1_mv != 1800 || aldo2_mv != 3300 || aldo3_mv != 3300 || aldo4_mv != 3300) {
    return AXP2101_ERR_INVALID_STATE;
  }

  puts("AXP2101 LDO rails configured");
  return AXP2101_ERR_NONE;
}

static int32_t configure_axp2101_dcdc1(axp2101_t *pmic) {
  int32_t err = axp2101_dcdc1_voltage_set(pmic, CORES3_AXP2101_DCDC1_LCD_PWR_MV);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = axp2101_dcdc_ctrl0_enable(pmic, AXP2101_DCDC_CTRL0_EN_DCDC1);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  uint16_t dcdc1_mv = 0;
  err = axp2101_dcdc1_voltage_get(pmic, &dcdc1_mv);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  axp2101_dcdc_ctrl0_t dcdc_state = {0};
  err = axp2101_dcdc_ctrl0_get(pmic, &dcdc_state);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  if (dcdc1_mv != CORES3_AXP2101_DCDC1_LCD_PWR_MV || !dcdc_state.dcdc1_en) {
    return AXP2101_ERR_INVALID_STATE;
  }

  printf("AXP2101 DCDC1 configured to %u mV\n", dcdc1_mv);
  return AXP2101_ERR_NONE;
}

static int32_t configure_axp2101_power_key(axp2101_t *pmic) {
  axp2101_irq_off_on_level_t config = {
      .irq_time = AXP2101_POWER_KEY_IRQ_TIME_1S,
      .poweroff_time = AXP2101_POWER_KEY_POWEROFF_TIME_4S,
      .poweron_time = AXP2101_POWER_KEY_ON_TIME_128MS,
  };

  int32_t err = axp2101_irq_off_on_level_set(pmic, &config);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  axp2101_irq_off_on_level_t readback = {0};
  err = axp2101_irq_off_on_level_get(pmic, &readback);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  if (readback.irq_time != config.irq_time || readback.poweroff_time != config.poweroff_time ||
      readback.poweron_time != config.poweron_time) {
    return AXP2101_ERR_INVALID_STATE;
  }

  puts("AXP2101 power-key timing configured");
  return AXP2101_ERR_NONE;
}

static int32_t configure_axp2101_chgled(axp2101_t *pmic) {
  axp2101_chgled_ctrl_t config = {
      .enabled = true,
      .function = AXP2101_CHGLED_FUNCTION_TYPE_A,
      .output = AXP2101_CHGLED_OUTPUT_BLINK_1HZ,
  };

  int32_t err = axp2101_chgled_ctrl_set(pmic, &config);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  axp2101_chgled_ctrl_t readback = {0};
  err = axp2101_chgled_ctrl_get(pmic, &readback);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  if (!readback.enabled || readback.function != config.function ||
      readback.output != config.output) {
    return AXP2101_ERR_INVALID_STATE;
  }

  puts("AXP2101 CHGLED configured");
  return AXP2101_ERR_NONE;
}

static int32_t configure_axp2101_pmu_common(axp2101_t *pmic) {
  axp2101_pmu_common_cfg_t config = {
      .raw_bits_7_6 = 0,
      .internal_off_discharge_enabled = true,
      .raw_bit4 = true,
      .pwrok_restart_enabled = false,
      .pwron_16s_shutdown_enabled = false,
      .restart_system = false,
      .soft_pwroff = false,
  };

  int32_t err = axp2101_pmu_common_cfg_set(pmic, &config);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  axp2101_pmu_common_cfg_t readback = {0};
  err = axp2101_pmu_common_cfg_get(pmic, &readback);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  if (readback.raw_bits_7_6 != config.raw_bits_7_6 ||
      readback.internal_off_discharge_enabled != config.internal_off_discharge_enabled ||
      readback.raw_bit4 != config.raw_bit4 ||
      readback.pwrok_restart_enabled != config.pwrok_restart_enabled ||
      readback.pwron_16s_shutdown_enabled != config.pwron_16s_shutdown_enabled) {
    return AXP2101_ERR_INVALID_STATE;
  }

  puts("AXP2101 PMU common configuration applied");
  return AXP2101_ERR_NONE;
}

static int32_t configure_axp2101_adc(axp2101_t *pmic) {
  uint8_t adc_channels =
      AXP2101_ADC_EN_VSYS | AXP2101_ADC_EN_VBUS | AXP2101_ADC_EN_TS | AXP2101_ADC_EN_BATT;
  int32_t err = axp2101_adc_enable_channels(pmic, adc_channels);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  puts("AXP2101 ADC channels enabled");
  return AXP2101_ERR_NONE;
}

static int32_t apply_cores3_axp2101_startup(axp2101_t *pmic) {
  int32_t err = configure_axp2101_dcdc1(pmic);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = configure_axp2101_ldos(pmic);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = configure_axp2101_power_key(pmic);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = configure_axp2101_chgled(pmic);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  err = configure_axp2101_pmu_common(pmic);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  return configure_axp2101_adc(pmic);
}

static int32_t print_axp2101_summary(axp2101_t *pmic) {
  axp2101_status1_t status1 = {0};
  int32_t err = axp2101_status1_get(pmic, &status1);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  axp2101_status2_t status2 = {0};
  err = axp2101_status2_get(pmic, &status2);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  uint16_t vbus_mv = 0;
  err = axp2101_adc_vbus_read(pmic, &vbus_mv);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  uint16_t vsys_mv = 0;
  err = axp2101_adc_vsys_read(pmic, &vsys_mv);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  uint16_t vbat_mv = 0;
  err = axp2101_adc_vbat_read(pmic, &vbat_mv);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  printf("VBUS good: %s\n", bool_to_yes_no(status1.vbus_good));
  printf("Battery present: %s\n", bool_to_yes_no(status1.battery_present));
  printf("System power on: %s\n", bool_to_yes_no(status2.system_power_on));
  printf("Charging status: %s\n", axp2101_charging_status_to_string(status2.charging_status));
  printf("VBUS: %u mV\n", vbus_mv);
  printf("VSYS: %u mV\n", vsys_mv);
  printf("VBAT: %u mV\n", vbat_mv);

  return AXP2101_ERR_NONE;
}

int32_t cores3_power_mgmt_init(aw9523b_t *expander, axp2101_t *pmic) {
  if (expander == NULL || pmic == NULL) {
    return II2C_ERR_INVALID_ARG;
  }

  puts("Applying CoreS3 startup sequence with local components...");

  int32_t err = configure_aw9523b_boost_enable(expander);
  if (err != AW9523B_ERR_NONE) {
    printf("Failed to assert AW9523B BOOST_EN: %s\n", cores3_power_mgmt_err_to_name(err));
    return err;
  }

  err = apply_cores3_axp2101_startup(pmic);
  if (err != AXP2101_ERR_NONE) {
    printf("Failed to apply CoreS3 AXP2101 startup settings: %s\n", cores3_power_mgmt_err_to_name(err));
    return err;
  }

  err = print_axp2101_summary(pmic);
  if (err != AXP2101_ERR_NONE) {
    printf("Failed to read post-startup AXP2101 summary: %s\n", cores3_power_mgmt_err_to_name(err));
    return err;
  }

  puts("CoreS3 startup sequence complete.");
  return AXP2101_ERR_NONE;
}
