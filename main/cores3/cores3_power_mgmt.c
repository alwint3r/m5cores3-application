#include "cores3_power_mgmt.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <axp2101/axp2101_register.h>
#include <ii2c/ii2c.h>

static const uint8_t CORES3_AW9523B_BOOST_EN_PORT = 1;
static const uint8_t CORES3_AW9523B_BOOST_EN_PIN = 7;
static const uint16_t CORES3_AXP2101_DCDC1_LCD_PWR_MV = 3300;

static ii2c_device_handle_t axp2101_device_from_context(void *context) {
  return (ii2c_device_handle_t)context;
}

static int32_t axp2101_i2c_write(void *context, const uint8_t *write_buffer, size_t write_size) {
  ii2c_device_handle_t device = axp2101_device_from_context(context);
  if (device == NULL) {
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
  ii2c_device_handle_t device = axp2101_device_from_context(context);
  if (device == NULL || read_buffer == NULL || read_size == NULL) {
    return II2C_ERR_INVALID_ARG;
  }

  int32_t err =
      ii2c_master_transmit_receive(device, write_buffer, write_size, read_buffer, read_capacity);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  *read_size = read_capacity;
  return II2C_ERR_NONE;
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

const char *axp2101_charging_status_to_string(axp2101_charging_status_t status) {
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

static const char *axp2101_battery_current_direction_to_string(
    axp2101_battery_current_direction_t direction) {
  switch (direction) {
    case AXP2101_BATTERY_CURRENT_DIRECTION_STANDBY:
      return "standby";
    case AXP2101_BATTERY_CURRENT_DIRECTION_CHARGE:
      return "charge";
    case AXP2101_BATTERY_CURRENT_DIRECTION_DISCHARGE:
      return "discharge";
    case AXP2101_BATTERY_CURRENT_DIRECTION_RESERVED:
      return "reserved";
  }

  return "unknown";
}

static void log_axp2101_current_telemetry(axp2101_t *pmic) {
  axp2101_current_telemetry_t telemetry = {0};
  int32_t err = axp2101_current_telemetry_get(pmic, &telemetry);
  if (err != AXP2101_ERR_NONE) {
    printf("AXP2101 current telemetry unavailable: %s\n", cores3_power_mgmt_err_to_name(err));
    return;
  }

  printf(
      "AXP2101 current telemetry: battery=%s, charging=%s, input-limit=%u mA, "
      "precharge=%u mA, charge=%u mA, termination=%u mA (%s)\n",
      axp2101_battery_current_direction_to_string(telemetry.status2.battery_current_direction),
      axp2101_charging_status_to_string(telemetry.status2.charging_status),
      telemetry.input_current_limit_ma,
      telemetry.charger_current.precharge_current_ma,
      telemetry.charger_current.constant_charge_current_ma,
      telemetry.charger_current.termination_current_ma,
      telemetry.charger_current.termination_enabled ? "enabled" : "disabled");
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

  err = configure_axp2101_adc(pmic);
  if (err != AXP2101_ERR_NONE) {
    return err;
  }

  log_axp2101_current_telemetry(pmic);
  return AXP2101_ERR_NONE;
}

int32_t cores3_power_mgmt_init(ii2c_device_handle_t device, aw9523b_t *expander, axp2101_t *pmic) {
  if (device == NULL || expander == NULL || pmic == NULL) {
    return II2C_ERR_INVALID_ARG;
  }

  pmic->transport_context = device;
  pmic->transport_write = axp2101_i2c_write;
  pmic->transport_write_read = axp2101_i2c_write_read;

  puts("Applying CoreS3 startup sequence with local components...");

  int32_t err = configure_aw9523b_boost_enable(expander);
  if (err != AW9523B_ERR_NONE) {
    printf("Failed to assert AW9523B BOOST_EN: %s\n", cores3_power_mgmt_err_to_name(err));
    return err;
  }

  err = apply_cores3_axp2101_startup(pmic);
  if (err != AXP2101_ERR_NONE) {
    printf("Failed to apply CoreS3 AXP2101 startup settings: %s\n",
           cores3_power_mgmt_err_to_name(err));
    return err;
  }

  puts("CoreS3 startup sequence complete.");
  return AXP2101_ERR_NONE;
}
