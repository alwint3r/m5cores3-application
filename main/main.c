#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <driver/gpio.h>

#include <aw9523b/aw9523b.h>
#include <axp2101/axp2101.h>
#include <axp2101/axp2101_register.h>
#include <ii2c/ii2c.h>

static const gpio_num_t SYS_I2C_SDA = GPIO_NUM_12;
static const gpio_num_t SYS_I2C_SCL = GPIO_NUM_11;

static const uint16_t CORES3_AW9523B_I2C_ADDRESS = 0x58;
static const uint16_t CORES3_AXP2101_I2C_ADDRESS = 0x34;
static const uint8_t CORES3_AW9523B_BOOST_EN_PORT = 1;
static const uint8_t CORES3_AW9523B_BOOST_EN_PIN = 7;
static const uint16_t CORES3_AXP2101_DCDC1_LCD_PWR_MV = 3300;
static const uint8_t CORES3_AW9523B_LCD_RST_PORT = 1;
static const uint8_t CORES3_AW9523B_LCD_RST_PIN = 1;

static ii2c_master_bus_handle_t sys_i2c = NULL;
static ii2c_device_handle_t aw9523b = NULL;
static ii2c_device_handle_t axp2101 = NULL;

static const char *bool_to_yes_no(bool value) {
  return value ? "yes" : "no";
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

static void release_handles(void) {
  if (aw9523b != NULL) {
    (void)ii2c_del_device(aw9523b);
    aw9523b = NULL;
  }

  if (axp2101 != NULL) {
    (void)ii2c_del_device(axp2101);
    axp2101 = NULL;
  }

  if (sys_i2c != NULL) {
    (void)ii2c_del_master_bus(sys_i2c);
    sys_i2c = NULL;
  }
}

static int32_t create_i2c_bus(void) {
  ii2c_master_bus_config_t bus_cfg;
  ii2c_get_default_master_bus_config(&bus_cfg);

  bus_cfg.sda_io_num = SYS_I2C_SDA;
  bus_cfg.scl_io_num = SYS_I2C_SCL;
  bus_cfg.enable_internal_pullup = true;

  return ii2c_new_master_bus(&bus_cfg, &sys_i2c);
}

static int32_t probe_i2c_device(const char *label, uint16_t address) {
  int32_t err = ii2c_master_probe(sys_i2c, address, 3000);
  if (err == II2C_ERR_NONE) {
    printf("%s responded at 0x%02X\n", label, address);
  }
  return err;
}

static int32_t attach_i2c_device(uint16_t address, ii2c_device_handle_t *out_device) {
  ii2c_device_config_t dev_cfg;
  ii2c_get_default_device_config(&dev_cfg);

  dev_cfg.device_address = address;
  dev_cfg.timeout_ms = 3000;

  return ii2c_new_device(sys_i2c, &dev_cfg, out_device);
}

static int32_t configure_aw9523b_boost_enable(ii2c_device_handle_t dev) {
  int32_t err = aw9523b_port_dir_set(dev,
                                     CORES3_AW9523B_BOOST_EN_PORT,
                                     CORES3_AW9523B_BOOST_EN_PIN,
                                     AW9523B_PORT_DIRECTION_OUTPUT);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = aw9523b_level_set(dev, CORES3_AW9523B_BOOST_EN_PORT, CORES3_AW9523B_BOOST_EN_PIN, 1);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint8_t boost_en_level = 0;
  err = aw9523b_level_get(
      dev, CORES3_AW9523B_BOOST_EN_PORT, CORES3_AW9523B_BOOST_EN_PIN, &boost_en_level);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  if (boost_en_level == 0) {
    return II2C_ERR_INVALID_STATE;
  }

  puts("AW9523B BOOST_EN asserted");
  return II2C_ERR_NONE;
}

static int32_t configure_axp2101_ldos(ii2c_device_handle_t dev) {
  uint8_t ldo_mask = AXP2101_LDO_CTRL0_EN_DLDO1 | AXP2101_LDO_CTRL0_EN_BLDO2 |
                     AXP2101_LDO_CTRL0_EN_BLDO1 | AXP2101_LDO_CTRL0_EN_ALDO4 |
                     AXP2101_LDO_CTRL0_EN_ALDO3 | AXP2101_LDO_CTRL0_EN_ALDO2 |
                     AXP2101_LDO_CTRL0_EN_ALDO1;

  int32_t err = axp2101_ldo_ctrl0_enable(dev, ldo_mask);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  axp2101_ldo_ctrl0_t ldo_state = {0};
  err = axp2101_ldo_ctrl0_get(dev, &ldo_state);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  if (!ldo_state.dldo1_en || ldo_state.cpusldo_en || !ldo_state.bldo2_en || !ldo_state.bldo1_en ||
      !ldo_state.aldo4_en || !ldo_state.aldo3_en || !ldo_state.aldo2_en || !ldo_state.aldo1_en) {
    return II2C_ERR_INVALID_STATE;
  }

  err = axp2101_aldo1_voltage_set(dev, 1800);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo2_voltage_set(dev, 3300);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo3_voltage_set(dev, 3300);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo4_voltage_set(dev, 3300);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint16_t aldo1_mv = 0;
  uint16_t aldo2_mv = 0;
  uint16_t aldo3_mv = 0;
  uint16_t aldo4_mv = 0;

  err = axp2101_aldo1_voltage_get(dev, &aldo1_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo2_voltage_get(dev, &aldo2_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo3_voltage_get(dev, &aldo3_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_aldo4_voltage_get(dev, &aldo4_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  if (aldo1_mv != 1800 || aldo2_mv != 3300 || aldo3_mv != 3300 || aldo4_mv != 3300) {
    return II2C_ERR_INVALID_STATE;
  }

  puts("AXP2101 LDO rails configured");
  return II2C_ERR_NONE;
}

static int32_t configure_axp2101_dcdc1(ii2c_device_handle_t dev) {
  int32_t err = axp2101_dcdc1_voltage_set(dev, CORES3_AXP2101_DCDC1_LCD_PWR_MV);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = axp2101_dcdc_ctrl0_enable(dev, AXP2101_DCDC_CTRL0_EN_DCDC1);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint16_t dcdc1_mv = 0;
  err = axp2101_dcdc1_voltage_get(dev, &dcdc1_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  axp2101_dcdc_ctrl0_t dcdc_state = {0};
  err = axp2101_dcdc_ctrl0_get(dev, &dcdc_state);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  if (dcdc1_mv != CORES3_AXP2101_DCDC1_LCD_PWR_MV || !dcdc_state.dcdc1_en) {
    return II2C_ERR_INVALID_STATE;
  }

  printf("AXP2101 DCDC1 configured to %u mV\n", dcdc1_mv);
  return II2C_ERR_NONE;
}

static int32_t configure_axp2101_power_key(ii2c_device_handle_t dev) {
  axp2101_irq_off_on_level_t config = {
      .irq_time = AXP2101_POWER_KEY_IRQ_TIME_1S,
      .poweroff_time = AXP2101_POWER_KEY_POWEROFF_TIME_4S,
      .poweron_time = AXP2101_POWER_KEY_ON_TIME_128MS,
  };

  int32_t err = axp2101_irq_off_on_level_set(dev, &config);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  axp2101_irq_off_on_level_t readback = {0};
  err = axp2101_irq_off_on_level_get(dev, &readback);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  if (readback.irq_time != config.irq_time || readback.poweroff_time != config.poweroff_time ||
      readback.poweron_time != config.poweron_time) {
    return II2C_ERR_INVALID_STATE;
  }

  puts("AXP2101 power-key timing configured");
  return II2C_ERR_NONE;
}

static int32_t configure_axp2101_chgled(ii2c_device_handle_t dev) {
  axp2101_chgled_ctrl_t config = {
      .enabled = true,
      .function = AXP2101_CHGLED_FUNCTION_TYPE_A,
      .output = AXP2101_CHGLED_OUTPUT_BLINK_1HZ,
  };

  int32_t err = axp2101_chgled_ctrl_set(dev, &config);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  axp2101_chgled_ctrl_t readback = {0};
  err = axp2101_chgled_ctrl_get(dev, &readback);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  if (!readback.enabled || readback.function != config.function ||
      readback.output != config.output) {
    return II2C_ERR_INVALID_STATE;
  }

  puts("AXP2101 CHGLED configured");
  return II2C_ERR_NONE;
}

static int32_t configure_axp2101_pmu_common(ii2c_device_handle_t dev) {
  axp2101_pmu_common_cfg_t config = {
      .raw_bits_7_6 = 0,
      .internal_off_discharge_enabled = true,
      .raw_bit4 = true,
      .pwrok_restart_enabled = false,
      .pwron_16s_shutdown_enabled = false,
      .restart_system = false,
      .soft_pwroff = false,
  };

  int32_t err = axp2101_pmu_common_cfg_set(dev, &config);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  axp2101_pmu_common_cfg_t readback = {0};
  err = axp2101_pmu_common_cfg_get(dev, &readback);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  if (readback.raw_bits_7_6 != config.raw_bits_7_6 ||
      readback.internal_off_discharge_enabled != config.internal_off_discharge_enabled ||
      readback.raw_bit4 != config.raw_bit4 ||
      readback.pwrok_restart_enabled != config.pwrok_restart_enabled ||
      readback.pwron_16s_shutdown_enabled != config.pwron_16s_shutdown_enabled) {
    return II2C_ERR_INVALID_STATE;
  }

  puts("AXP2101 PMU common configuration applied");
  return II2C_ERR_NONE;
}

static int32_t configure_axp2101_adc(ii2c_device_handle_t dev) {
  uint8_t adc_channels =
      AXP2101_ADC_EN_VSYS | AXP2101_ADC_EN_VBUS | AXP2101_ADC_EN_TS | AXP2101_ADC_EN_BATT;
  int32_t err = axp2101_adc_enable_channels(dev, adc_channels);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  puts("AXP2101 ADC channels enabled");
  return II2C_ERR_NONE;
}

static int32_t apply_cores3_axp2101_startup(ii2c_device_handle_t dev) {
  int32_t err = configure_axp2101_dcdc1(dev);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = configure_axp2101_ldos(dev);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = configure_axp2101_power_key(dev);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = configure_axp2101_chgled(dev);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  err = configure_axp2101_pmu_common(dev);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  return configure_axp2101_adc(dev);
}

static int32_t print_axp2101_summary(ii2c_device_handle_t dev) {
  axp2101_status1_t status1 = {0};
  int32_t err = axp2101_status1_get(dev, &status1);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  axp2101_status2_t status2 = {0};
  err = axp2101_status2_get(dev, &status2);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint16_t vbus_mv = 0;
  err = axp2101_adc_vbus_read(dev, &vbus_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint16_t vsys_mv = 0;
  err = axp2101_adc_vsys_read(dev, &vsys_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint16_t vbat_mv = 0;
  err = axp2101_adc_vbat_read(dev, &vbat_mv);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  printf("VBUS good: %s\n", bool_to_yes_no(status1.vbus_good));
  printf("Battery present: %s\n", bool_to_yes_no(status1.battery_present));
  printf("System power on: %s\n", bool_to_yes_no(status2.system_power_on));
  printf("Charging status: %s\n", axp2101_charging_status_to_string(status2.charging_status));
  printf("VBUS: %u mV\n", vbus_mv);
  printf("VSYS: %u mV\n", vsys_mv);
  printf("VBAT: %u mV\n", vbat_mv);

  return II2C_ERR_NONE;
}

void app_main(void) {
  int32_t err = create_i2c_bus();
  if (err != II2C_ERR_NONE) {
    printf("Failed to initialize system I2C bus: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  err = probe_i2c_device("AW9523B", CORES3_AW9523B_I2C_ADDRESS);
  if (err != II2C_ERR_NONE) {
    printf("Failed to probe AW9523B: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  err = probe_i2c_device("AXP2101", CORES3_AXP2101_I2C_ADDRESS);
  if (err != II2C_ERR_NONE) {
    printf("Failed to probe AXP2101: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  err = attach_i2c_device(CORES3_AW9523B_I2C_ADDRESS, &aw9523b);
  if (err != II2C_ERR_NONE) {
    printf("Failed to attach AW9523B: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  err = attach_i2c_device(CORES3_AXP2101_I2C_ADDRESS, &axp2101);
  if (err != II2C_ERR_NONE) {
    printf("Failed to attach AXP2101: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  puts("Applying CoreS3 startup sequence with local components...");

  err = configure_aw9523b_boost_enable(aw9523b);
  if (err != II2C_ERR_NONE) {
    printf("Failed to assert AW9523B BOOST_EN: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  err = apply_cores3_axp2101_startup(axp2101);
  if (err != II2C_ERR_NONE) {
    printf("Failed to apply CoreS3 AXP2101 startup settings: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  err = print_axp2101_summary(axp2101);
  if (err != II2C_ERR_NONE) {
    printf("Failed to read post-startup AXP2101 summary: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  puts("CoreS3 startup sequence complete.");

  err = aw9523b_port_dir_set(aw9523b,
                             CORES3_AW9523B_LCD_RST_PORT,
                             CORES3_AW9523B_LCD_RST_PIN,
                             AW9523B_PORT_DIRECTION_OUTPUT);
  if (err != II2C_ERR_NONE) {
    printf("Failed to set direction of the LCD reset pin: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }

  err = aw9523b_level_set(aw9523b, CORES3_AW9523B_LCD_RST_PORT, CORES3_AW9523B_LCD_RST_PIN, 1);
  if (err != II2C_ERR_NONE) {
    printf("Failed to set LCD RST logic to high: %s\n", ii2c_err_to_name(err));
    release_handles();
    return;
  }
}
