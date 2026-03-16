#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <ii2c/ii2c.h>
#include <axp2101/axp2101.h>
#include <axp2101/axp2101_register.h>

const gpio_num_t sys_i2c_sda = GPIO_NUM_12;
const gpio_num_t sys_i2c_scl = GPIO_NUM_11;

static ii2c_master_bus_handle_t sys_i2c = NULL;
static ii2c_device_handle_t axp2101 = NULL;

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

static const char *axp2101_charging_status_to_string(axp2101_charging_status_t status) {
  switch (status) {
    case AXP2101_CHARGING_STATUS_TRI_CHARGE:
      return "tri_charge";
    case AXP2101_CHARGING_STATUS_PRE_CHARGE:
      return "pre_charge";
    case AXP2101_CHARGING_STATUS_CONSTANT_CHARGE:
      return "constant charge";
    case AXP2101_CHARGING_STATUS_CONSTANT_VOLTAGE:
      return "constant voltage";
    case AXP2101_CHARGING_STATUS_CHARGE_DONE:
      return "charge done";
    case AXP2101_CHARGING_STATUS_NOT_CHARGING:
      return "not charging";
    case AXP2101_CHARGING_STATUS_UNKNOWN:
      return "unknown";
  }

  return "unknown";
}

static const char *axp2101_power_key_irq_time_to_string(axp2101_power_key_irq_time_t value) {
  switch (value) {
    case AXP2101_POWER_KEY_IRQ_TIME_1S:
      return "1 s";
    case AXP2101_POWER_KEY_IRQ_TIME_1_5S:
      return "1.5 s";
    case AXP2101_POWER_KEY_IRQ_TIME_2S:
      return "2 s";
    case AXP2101_POWER_KEY_IRQ_TIME_2_5S:
      return "2.5 s";
  }

  return "unknown";
}

static const char *axp2101_power_key_poweroff_time_to_string(
    axp2101_power_key_poweroff_time_t value) {
  switch (value) {
    case AXP2101_POWER_KEY_POWEROFF_TIME_4S:
      return "4 s";
    case AXP2101_POWER_KEY_POWEROFF_TIME_6S:
      return "6 s";
    case AXP2101_POWER_KEY_POWEROFF_TIME_8S:
      return "8 s";
    case AXP2101_POWER_KEY_POWEROFF_TIME_10S:
      return "10 s";
  }

  return "unknown";
}

static const char *axp2101_power_key_on_time_to_string(axp2101_power_key_on_time_t value) {
  switch (value) {
    case AXP2101_POWER_KEY_ON_TIME_128MS:
      return "128 ms";
    case AXP2101_POWER_KEY_ON_TIME_512MS:
      return "512 ms";
    case AXP2101_POWER_KEY_ON_TIME_1S:
      return "1 s";
    case AXP2101_POWER_KEY_ON_TIME_2S:
      return "2 s";
  }

  return "unknown";
}

static const char *axp2101_chgled_function_to_string(axp2101_chgled_function_t value) {
  switch (value) {
    case AXP2101_CHGLED_FUNCTION_TYPE_A:
      return "type A";
    case AXP2101_CHGLED_FUNCTION_TYPE_B:
      return "type B";
    case AXP2101_CHGLED_FUNCTION_REGISTER_CONTROL:
      return "register control";
    case AXP2101_CHGLED_FUNCTION_RESERVED:
      return "reserved";
  }

  return "unknown";
}

static const char *axp2101_chgled_output_to_string(axp2101_chgled_output_t value) {
  switch (value) {
    case AXP2101_CHGLED_OUTPUT_HIZ:
      return "Hi-Z";
    case AXP2101_CHGLED_OUTPUT_BLINK_1HZ:
      return "blink 1 Hz";
    case AXP2101_CHGLED_OUTPUT_BLINK_4HZ:
      return "blink 4 Hz";
    case AXP2101_CHGLED_OUTPUT_DRIVE_LOW:
      return "drive low";
  }

  return "unknown";
}

static const char *bool_to_enabled_string(bool value) {
  return value ? "enabled" : "disabled";
}

void app_main(void) {
  ii2c_master_bus_config_t bus_cfg;
  ii2c_get_default_master_bus_config(&bus_cfg);

  bus_cfg.sda_io_num = sys_i2c_sda;
  bus_cfg.scl_io_num = sys_i2c_scl;
  bus_cfg.enable_internal_pullup = true;

  int32_t err = ii2c_new_master_bus(&bus_cfg, &sys_i2c);
  if (err != II2C_ERR_NONE) {
    printf("FAILED initializing I2C: %s\n", ii2c_err_to_name(err));
    return;
  }

  ii2c_device_config_t axp2101_cfg = {
      .device_address = 0x34,
      .scl_speed_hz = 100000,
      .timeout_ms = 3000,
  };

  err = ii2c_new_device(sys_i2c, &axp2101_cfg, &axp2101);
  if (err != II2C_ERR_NONE) {
    printf("Failed creating AXP2101 I2C device: %s\n", ii2c_err_to_name(err));
    return;
  }

  err = axp2101_adc_enable_channels(
      axp2101, AXP2101_ADC_EN_VBUS | AXP2101_ADC_EN_VSYS | AXP2101_ADC_EN_BATT);
  if (err != II2C_ERR_NONE) {
    printf("Failed transmit ADC control: %s", ii2c_err_to_name(err));
    return;
  }

  axp2101_status1_t status1 = {0};
  err = axp2101_status1_get(axp2101, &status1);
  if (err != II2C_ERR_NONE) {
    printf("Failed transmit receive: %s\n", ii2c_err_to_name(err));
    return;
  }

  printf("VBUS state: %s\n", status1.vbus_good ? "OK" : "Not OK");
  printf("BATFET state: %s\n", status1.batfet_open ? "open" : "closed");
  printf("Battery present? %s\n", status1.battery_present ? "yes" : "no");
  printf("Battery in active mode? %s\n", status1.battery_active ? "yes" : "no");
  printf("Current limit state: %s\n",
         status1.current_limited ? "in current limit" : "not in current limit");

  puts("=================\n\n");

  axp2101_status2_t status2 = {0};
  err = axp2101_status2_get(axp2101, &status2);
  if (err != II2C_ERR_NONE) {
    printf("Failed transmiting: %s\n", ii2c_err_to_name(err));
    return;
  }

  printf("Battery current direction: %s\n",
         axp2101_battery_current_direction_to_string(status2.battery_current_direction));
  printf("System status indication: %s\n", status2.system_power_on ? "power on" : "power off");
  printf("VINDPM indication: %s\n", status2.vindpm_active ? "VINDPM" : "not in VINDPM");
  printf("Charging status: %s\n", axp2101_charging_status_to_string(status2.charging_status));

  err = axp2101_fuel_gauge_enable(axp2101);
  if (err != II2C_ERR_NONE) {
    printf("Failed to enable fuel gauge support: %s\n", ii2c_err_to_name(err));
    return;
  }

  axp2101_fuel_gauge_t fuel_gauge = {0};
  err = axp2101_fuel_gauge_get(axp2101, &fuel_gauge);
  if (err != II2C_ERR_NONE) {
    printf("Failed to read fuel gauge state: %s\n", ii2c_err_to_name(err));
    return;
  }

  printf("Fuel gauge: %s\n", bool_to_enabled_string(fuel_gauge.fuel_gauge_enabled));
  printf("Battery detect: %s\n", bool_to_enabled_string(fuel_gauge.battery_detection_enabled));
  printf("Fuel gauge battery present: %s\n", fuel_gauge.battery_present ? "yes" : "no");
  if (fuel_gauge.battery_percent_valid) {
    printf("Battery percent: %u%%\n", fuel_gauge.battery_percent);
  } else {
    puts("Battery percent: unavailable");
  }

  axp2101_pmu_common_cfg_t pmu_cfg = {
      .internal_off_discharge_enabled = true,
      .pwrok_restart_enabled = false,
      .pwron_16s_shutdown_enabled = false,
      .restart_system = false,
      .soft_pwroff = false,
  };
  err = axp2101_pmu_common_cfg_set(axp2101, &pmu_cfg);
  if (err != II2C_ERR_NONE) {
    printf("Failed to write PMU common config: %s\n", ii2c_err_to_name(err));
    return;
  }

  axp2101_pmu_common_cfg_t pmu_common_cfg = {0};
  err = axp2101_pmu_common_cfg_get(axp2101, &pmu_common_cfg);
  if (err != II2C_ERR_NONE) {
    printf("Failed to read PMU common config: %s\n", ii2c_err_to_name(err));
    return;
  }

  printf("PMU common raw bits[7:6]: 0x%X\n", pmu_common_cfg.raw_bits_7_6);
  printf("PMU common internal off-discharge: %s\n",
         bool_to_enabled_string(pmu_common_cfg.internal_off_discharge_enabled));
  printf("PMU common raw bit4: %s\n", pmu_common_cfg.raw_bit4 ? "set" : "clear");
  printf("PMU common PWROK restart: %s\n",
         bool_to_enabled_string(pmu_common_cfg.pwrok_restart_enabled));
  printf("PMU common PWRON 16s shutdown: %s\n",
         bool_to_enabled_string(pmu_common_cfg.pwron_16s_shutdown_enabled));
  printf("PMU common restart action bit: %s\n", pmu_common_cfg.restart_system ? "set" : "clear");
  printf("PMU common soft PWROFF bit: %s\n", pmu_common_cfg.soft_pwroff ? "set" : "clear");

  axp2101_pmu_common_cfg_t pmu_common_writeback = pmu_common_cfg;
  pmu_common_writeback.restart_system = false;
  pmu_common_writeback.soft_pwroff = false;
  err = axp2101_pmu_common_cfg_set(axp2101, &pmu_common_writeback);
  if (err != II2C_ERR_NONE) {
    printf("Failed to write PMU common config: %s\n", ii2c_err_to_name(err));
    return;
  }

  puts("PMU common config safe write-back complete");

  axp2101_irq_off_on_level_t pwrkey_config = {
      .irq_time = AXP2101_POWER_KEY_IRQ_TIME_1S,
      .poweroff_time = AXP2101_POWER_KEY_POWEROFF_TIME_4S,
      .poweron_time = AXP2101_POWER_KEY_ON_TIME_128MS,
  };
  err = axp2101_irq_off_on_level_set(axp2101, &pwrkey_config);
  if (err != II2C_ERR_NONE) {
    printf("Failed to configure the IRQ ON/OFF LEVEL: %s\n", ii2c_err_to_name(err));
    return;
  }

  axp2101_irq_off_on_level_t power_key = {0};
  err = axp2101_irq_off_on_level_get(axp2101, &power_key);
  if (err != II2C_ERR_NONE) {
    printf("Failed to read power-key timing config: %s\n", ii2c_err_to_name(err));
    return;
  }

  printf("Power-key IRQ time: %s\n", axp2101_power_key_irq_time_to_string(power_key.irq_time));
  printf("Power-key off time: %s\n",
         axp2101_power_key_poweroff_time_to_string(power_key.poweroff_time));
  printf("Power-key on time: %s\n", axp2101_power_key_on_time_to_string(power_key.poweron_time));

  axp2101_charger_current_t charger_current = {0};
  err = axp2101_charger_current_get(axp2101, &charger_current);
  if (err != II2C_ERR_NONE) {
    printf("Failed to read charger current config: %s\n", ii2c_err_to_name(err));
    return;
  }

  printf("Precharge current: %u mA\n", charger_current.precharge_current_ma);
  printf("Constant charge current: %u mA\n", charger_current.constant_charge_current_ma);
  printf("Termination current: %u mA (%s)\n",
         charger_current.termination_current_ma,
         charger_current.termination_enabled ? "enabled" : "disabled");

  axp2101_chgled_ctrl_t chgled_config = {
      .enabled = true,
      .function = AXP2101_CHGLED_FUNCTION_REGISTER_CONTROL,
      .output = AXP2101_CHGLED_OUTPUT_BLINK_1HZ,
  };
  err = axp2101_chgled_ctrl_set(axp2101, &chgled_config);
  if (err != II2C_ERR_NONE) {
    printf("Failed to configure CHGLED control: %s\n", ii2c_err_to_name(err));
    return;
  }

  axp2101_chgled_ctrl_t chgled_state = {0};
  err = axp2101_chgled_ctrl_get(axp2101, &chgled_state);
  if (err != II2C_ERR_NONE) {
    printf("Failed to read CHGLED control: %s\n", ii2c_err_to_name(err));
    return;
  }

  printf("CHGLED enabled: %s\n", chgled_state.enabled ? "yes" : "no");
  printf("CHGLED function: %s\n", axp2101_chgled_function_to_string(chgled_state.function));
  printf("CHGLED output mode: %s\n", axp2101_chgled_output_to_string(chgled_state.output));

  uint16_t vbus_mv = 0;
  err = axp2101_adc_vbus_read(axp2101, &vbus_mv);
  if (err != II2C_ERR_NONE) {
    printf("Failed to read vbus: %s\n", ii2c_err_to_name(err));
    return;
  }
  printf("vbus: %u\n", vbus_mv);

  uint16_t vsys_mv = 0;
  err = axp2101_adc_vsys_read(axp2101, &vsys_mv);
  if (err != II2C_ERR_NONE) {
    printf("Failed to read vsys data: %s\n", ii2c_err_to_name(err));
    return;
  }

  printf("vsys: %u\n", vsys_mv);

  uint16_t vbat_mv = 0;
  err = axp2101_adc_vbat_read(axp2101, &vbat_mv);

  if (err != II2C_ERR_NONE) {
    printf("Failed to read vbat data: %s\n", ii2c_err_to_name(err));
    return;
  }

  printf("vbat: %u\n", vbat_mv);

  uint8_t ldo_rails =
      (AXP2101_LDO_CTRL0_EN_DLDO1 | AXP2101_LDO_CTRL0_EN_BLDO2 | AXP2101_LDO_CTRL0_EN_BLDO1 |
       AXP2101_LDO_CTRL0_EN_ALDO4 | AXP2101_LDO_CTRL0_EN_ALDO3 | AXP2101_LDO_CTRL0_EN_ALDO2 |
       AXP2101_LDO_CTRL0_EN_ALDO1);
  err = axp2101_ldo_ctrl0_enable(axp2101, ldo_rails);
  if (err != II2C_ERR_NONE) {
    printf("Failed to enable LDO rails: %s\n", ii2c_err_to_name(err));
    return;
  }

  err = axp2101_aldo1_voltage_set(axp2101, 1800);
  if (err != II2C_ERR_NONE) {
    printf("Failed to set the voltage for ALDO1: %s\n", ii2c_err_to_name(err));
    return;
  }

  uint16_t aldo1_volt = 0;
  err = axp2101_aldo1_voltage_get(axp2101, &aldo1_volt);
  if (err != II2C_ERR_NONE) {
    printf("Failed to get the configured voltage for ALDO1: %s\n", ii2c_err_to_name(err));
    return;
  }
  printf("ALDO1 set voltage to: %u mv\n", aldo1_volt);

  err = axp2101_aldo2_voltage_set(axp2101, 3300);
  if (err != II2C_ERR_NONE) {
    printf("Failed to set the voltage for ALDO2: %s\n", ii2c_err_to_name(err));

    return;
  }

  err = axp2101_aldo3_voltage_set(axp2101, 3300);
  if (err != II2C_ERR_NONE) {
    printf("Failed to set the voltage for ALDO3: %s\n", ii2c_err_to_name(err));
    return;
  }

  err = axp2101_aldo4_voltage_set(axp2101, 3300);
  if (err != II2C_ERR_NONE) {
    printf("Failed to set the voltage for ALDO4: %s\n", ii2c_err_to_name(err));
    return;
  }
}
