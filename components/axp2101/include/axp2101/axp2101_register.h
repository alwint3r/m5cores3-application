/**
 * @file axp2101_register.h
 * @brief Public AXP2101 register addresses and bit masks exposed by this component.
 * @ingroup axp2101
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** @brief PMU Status 1 register read by `axp2101_status1_get()`. */
#define AXP2101_REG_PMU_STATUS1 0x0

/** @brief PMU Status 2 register read by `axp2101_status2_get()`. */
#define AXP2101_REG_PMU_STATUS2 0x1

/** @brief PMU common-configuration register. */
#define AXP2101_REG_PMU_COMMON_CFG 0x10
/** @brief Charge, fuel-gauge, and watchdog control register. */
#define AXP2101_REG_CHARGE_GAUGE_WDT_CTRL 0x18

/** @brief Power-key IRQ, power-off, and power-on timing register. */
#define AXP2101_REG_IRQ_OFF_ON_LEVEL 0x27

/** @brief ADC channel enable register. */
#define AXP2101_REG_ADC_EN 0x30

/** @brief High byte of the VBAT ADC result register pair. */
#define AXP2101_REG_VBAT_H 0x34

/** @brief High byte of the VBUS ADC result register pair. */
#define AXP2101_REG_VBUS_H 0x38

/** @brief High byte of the VSYS ADC result register pair. */
#define AXP2101_REG_VSYS_H 0x3A

/** @brief Charger precharge-current configuration register. */
#define AXP2101_REG_PRECHG_CURRENT_LIMIT 0x61
/** @brief Charger constant-current configuration register. */
#define AXP2101_REG_CHG_CURRENT_LIMIT 0x62
/** @brief Charger termination-current and termination-enable register. */
#define AXP2101_REG_TERM_CHG_CURRENT_CTRL 0x63
/** @brief Battery-detection control register. */
#define AXP2101_REG_BAT_DET_CTRL 0x68
/** @brief Charging-indicator LED control register. */
#define AXP2101_REG_CHGLED_CTRL 0x69

/** @brief LDO enable-control register used by the `axp2101_ldo_ctrl0_*()` helpers. */
#define AXP2101_REG_LDO_CTRL0 0x90

/** @brief ALDO1 output-voltage register used by `axp2101_aldo1_voltage_{set,get}()`. */
#define AXP2101_REG_ALDO1_V_SET 0x92
/** @brief ALDO2 output-voltage register used by `axp2101_aldo2_voltage_{set,get}()`. */
#define AXP2101_REG_ALDO2_V_SET 0x93
/** @brief ALDO3 output-voltage register used by `axp2101_aldo3_voltage_{set,get}()`. */
#define AXP2101_REG_ALDO3_V_SET 0x94
/** @brief ALDO4 output-voltage register used by `axp2101_aldo4_voltage_{set,get}()`. */
#define AXP2101_REG_ALDO4_V_SET 0x95
/** @brief BLDO1 output-voltage register used by `axp2101_bldo1_voltage_{set,get}()`. */
#define AXP2101_REG_BLDO1_V_SET 0x96
/** @brief BLDO2 output-voltage register used by `axp2101_bldo2_voltage_{set,get}()`. */
#define AXP2101_REG_BLDO2_V_SET 0x97
/** @brief DLDO1 output-voltage register used by `axp2101_dldo1_voltage_{set,get}()`. */
#define AXP2101_REG_DLDO1_V_SET 0x99

/** @brief Mask for the power-on timing field in `AXP2101_REG_IRQ_OFF_ON_LEVEL`. */
#define AXP2101_IRQ_OFF_ON_LEVEL_MASK_ON 0x03

/** @brief Mask for the power-off timing field in `AXP2101_REG_IRQ_OFF_ON_LEVEL`. */
#define AXP2101_IRQ_OFF_ON_LEVEL_MASK_OFF 0x0C

/** @brief Mask for the IRQ timing field in `AXP2101_REG_IRQ_OFF_ON_LEVEL`. */
#define AXP2101_IRQ_OFF_ON_LEVEL_MASK_IRQ 0x30

/** @brief Mask for the precharge-current selector field in `AXP2101_REG_PRECHG_CURRENT_LIMIT`. */
#define AXP2101_PRECHG_CURRENT_LIMIT_MASK 0x0F

/** @brief Mask for the constant-charge-current selector field in `AXP2101_REG_CHG_CURRENT_LIMIT`. */
#define AXP2101_CHG_CURRENT_LIMIT_MASK 0x1F

/** @brief Enable bit for charger-current termination in `AXP2101_REG_TERM_CHG_CURRENT_CTRL`. */
#define AXP2101_TERM_CHG_CURRENT_CTRL_TERM_EN (1 << 4)

/** @brief Mask for the termination-current selector field in `AXP2101_REG_TERM_CHG_CURRENT_CTRL`. */
#define AXP2101_TERM_CHG_CURRENT_CTRL_TERM_CURRENT_MASK 0x0F

/** @brief Enable bit for the CHGLED pin function in `AXP2101_REG_CHGLED_CTRL`. */
#define AXP2101_CHGLED_CTRL_ENABLE (1)

/** @brief Mask for the CHGLED function-select field in `AXP2101_REG_CHGLED_CTRL`. */
#define AXP2101_CHGLED_CTRL_FUNCTION_MASK 0x06

/** @brief Mask for the CHGLED register-output field in `AXP2101_REG_CHGLED_CTRL`. */
#define AXP2101_CHGLED_CTRL_OUTPUT_MASK 0x30

/** @brief Mask for undocumented writable bits 7:6 in `AXP2101_REG_PMU_COMMON_CFG`. */
#define AXP2101_PMU_COMMON_CFG_RAW_BITS_7_6_MASK 0xC0

/** @brief Internal off-discharge enable bit in `AXP2101_REG_PMU_COMMON_CFG`. */
#define AXP2101_PMU_COMMON_CFG_INTERNAL_OFF_DISCHARGE_EN (1 << 5)

/** @brief Undocumented writable bit 4 in `AXP2101_REG_PMU_COMMON_CFG`. */
#define AXP2101_PMU_COMMON_CFG_RAW_BIT4 (1 << 4)

/** @brief PWROK-triggered restart enable bit in `AXP2101_REG_PMU_COMMON_CFG`. */
#define AXP2101_PMU_COMMON_CFG_PWROK_RESTART_EN (1 << 3)

/** @brief PWRON 16-second PMIC shutdown enable bit in `AXP2101_REG_PMU_COMMON_CFG`. */
#define AXP2101_PMU_COMMON_CFG_PWRON_16S_SHUTDOWN_EN (1 << 2)

/** @brief Write-one restart action bit in `AXP2101_REG_PMU_COMMON_CFG`. */
#define AXP2101_PMU_COMMON_CFG_RESTART_SYSTEM (1 << 1)

/** @brief Write-one soft-poweroff action bit in `AXP2101_REG_PMU_COMMON_CFG`. */
#define AXP2101_PMU_COMMON_CFG_SOFT_PWROFF (1)

/** @brief Fuel-gauge enable bit in `AXP2101_REG_CHARGE_GAUGE_WDT_CTRL`. */
#define AXP2101_CHARGE_GAUGE_WDT_CTRL_GAUGE_EN (1 << 3)

/** @brief Battery-detection enable bit in `AXP2101_REG_BAT_DET_CTRL`. */
#define AXP2101_BAT_DET_CTRL_BAT_TYPE_DET_EN (1)

/** @brief Battery percentage data register exported by the AXP2101 fuel gauge. */
#define AXP2101_REG_BAT_PERCENT_DATA 0xA4

/**
 * @brief Enable the GPADC general-purpose input channel.
 *
 * Use this mask with `axp2101_adc_enable_channels()` or
 * `axp2101_adc_disable_channels()`.
 */
#define AXP2101_ADC_EN_GP (1 << 5)
/** @brief Enable the internal die-temperature ADC channel. */
#define AXP2101_ADC_EN_DIE_TEMP (1 << 4)
/** @brief Enable the VSYS voltage ADC channel. */
#define AXP2101_ADC_EN_VSYS (1 << 3)
/** @brief Enable the VBUS voltage ADC channel. */
#define AXP2101_ADC_EN_VBUS (1 << 2)
/** @brief Enable the TS pin ADC channel. */
#define AXP2101_ADC_EN_TS (1 << 1)
/** @brief Enable the battery voltage/current ADC channel. */
#define AXP2101_ADC_EN_BATT (1)
/** @brief Bitwise OR of all ADC channel-enable bits exported by this component. */
#define AXP2101_ADC_EN_ALL                                                                   \
  (AXP2101_ADC_EN_GP | AXP2101_ADC_EN_DIE_TEMP | AXP2101_ADC_EN_VSYS | AXP2101_ADC_EN_VBUS | \
   AXP2101_ADC_EN_TS | AXP2101_ADC_EN_BATT)

/** @brief Enable bit for the DLDO1 output in `AXP2101_REG_LDO_CTRL0`. */
#define AXP2101_LDO_CTRL0_EN_DLDO1 (1 << 7)
/** @brief Enable bit for the CPUSLDO output in `AXP2101_REG_LDO_CTRL0`. */
#define AXP2101_LDO_CTRL0_EN_CPUSLDO (1 << 6)
/** @brief Enable bit for the BLDO2 output in `AXP2101_REG_LDO_CTRL0`. */
#define AXP2101_LDO_CTRL0_EN_BLDO2 (1 << 5)
/** @brief Enable bit for the BLDO1 output in `AXP2101_REG_LDO_CTRL0`. */
#define AXP2101_LDO_CTRL0_EN_BLDO1 (1 << 4)
/** @brief Enable bit for the ALDO4 output in `AXP2101_REG_LDO_CTRL0`. */
#define AXP2101_LDO_CTRL0_EN_ALDO4 (1 << 3)
/** @brief Enable bit for the ALDO3 output in `AXP2101_REG_LDO_CTRL0`. */
#define AXP2101_LDO_CTRL0_EN_ALDO3 (1 << 2)
/** @brief Enable bit for the ALDO2 output in `AXP2101_REG_LDO_CTRL0`. */
#define AXP2101_LDO_CTRL0_EN_ALDO2 (1 << 1)
/** @brief Enable bit for the ALDO1 output in `AXP2101_REG_LDO_CTRL0`. */
#define AXP2101_LDO_CTRL0_EN_ALDO1 (1)

/** @brief Bitwise OR of all exported `AXP2101_REG_LDO_CTRL0` enable bits. */
#define AXP2101_LDO_CTRL0_EN_ALL                                                            \
  (AXP2101_LDO_CTRL0_EN_DLDO1 | AXP2101_LDO_CTRL0_EN_CPUSLDO | AXP2101_LDO_CTRL0_EN_BLDO2 | \
   AXP2101_LDO_CTRL0_EN_BLDO1 | AXP2101_LDO_CTRL0_EN_ALDO4 | AXP2101_LDO_CTRL0_EN_ALDO3 |   \
   AXP2101_LDO_CTRL0_EN_ALDO2 | AXP2101_LDO_CTRL0_EN_ALDO1)

#ifdef __cplusplus
}
#endif
