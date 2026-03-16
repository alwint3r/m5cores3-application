#include "aw9523b/aw9523b.h"
#include "aw9523b/aw9523b_register.h"

int32_t aw9523b_reg8_read(ii2c_device_handle_t dev, uint8_t reg, uint8_t *out_value) {
  return ii2c_master_transmit_receive(dev, &reg, 1, out_value, 1);
}

int32_t aw9523b_reg8_write(ii2c_device_handle_t dev, uint8_t reg, uint8_t value) {
  return ii2c_master_transmit(dev, (uint8_t[2]){reg, value}, 2);
}

int32_t aw9523b_reg8_set_bits(ii2c_device_handle_t dev, uint8_t reg, uint8_t bits) {
  uint8_t current_value = 0;
  int32_t err = aw9523b_reg8_read(dev, reg, &current_value);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  return aw9523b_reg8_write(dev, reg, current_value | bits);
}

int32_t aw9523b_reg8_update_bits(ii2c_device_handle_t dev,
                                 uint8_t reg,
                                 uint8_t mask,
                                 uint8_t new_value) {
  uint8_t current_value = 0;
  int32_t err = aw9523b_reg8_read(dev, reg, &current_value);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  current_value = (new_value & mask) | (current_value & ~mask);

  return aw9523b_reg8_write(dev, reg, current_value);
}

int32_t aw9523b_id_get(ii2c_device_handle_t dev, uint8_t *out) {
  if (!out) {
    return II2C_ERR_INVALID_ARG;
  }

  return aw9523b_reg8_read(dev, AW9523B_REG_ID, out);
}

int32_t aw9523b_port0_drive_mode_get(ii2c_device_handle_t dev,
                                     aw9523b_port0_drive_mode_t *out_mode) {
  if (!out_mode) {
    return II2C_ERR_INVALID_ARG;
  }

  uint8_t gcr = 0;
  int32_t err = aw9523b_reg8_read(dev, AW9523B_REG_GCR, &gcr);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  *out_mode = (gcr & AW9523B_GCR_PORT0_DRIVE_MODE_MASK) != 0 ? AW9523B_PORT0_DRIVE_MODE_PUSH_PULL
                                                             : AW9523B_PORT0_DRIVE_MODE_OPEN_DRAIN;
  return II2C_ERR_NONE;
}

int32_t aw9523b_port0_drive_mode_set(ii2c_device_handle_t dev, aw9523b_port0_drive_mode_t mode) {
  uint8_t new_value = 0;

  switch (mode) {
    case AW9523B_PORT0_DRIVE_MODE_OPEN_DRAIN:
      new_value = AW9523B_GCR_PORT0_DRIVE_MODE_OPEN_DRAIN;
      break;
    case AW9523B_PORT0_DRIVE_MODE_PUSH_PULL:
      new_value = AW9523B_GCR_PORT0_DRIVE_MODE_PUSH_PULL;
      break;
    default:
      return II2C_ERR_INVALID_ARG;
  }

  return aw9523b_reg8_update_bits(
      dev, AW9523B_REG_GCR, AW9523B_GCR_PORT0_DRIVE_MODE_MASK, new_value);
}

static int32_t aw9523b_port_to_reg(uint8_t base_reg, uint8_t port, uint8_t *out_reg) {
  if (!out_reg) {
    return II2C_ERR_INVALID_ARG;
  }

  if (port > 1) {
    return II2C_ERR_INVALID_ARG;
  }

  *out_reg = (uint8_t)(base_reg + port);
  return II2C_ERR_NONE;
}

static int32_t aw9523b_port_pin_to_reg_and_mask(uint8_t base_reg,
                                                uint8_t port,
                                                uint8_t pin,
                                                uint8_t *out_reg,
                                                uint8_t *out_mask) {
  if (!out_reg || !out_mask) {
    return II2C_ERR_INVALID_ARG;
  }

  if (port > 1) {
    return II2C_ERR_INVALID_ARG;
  }

  if (pin > 7) {
    return II2C_ERR_INVALID_ARG;
  }

  *out_reg = (uint8_t)(base_reg + port);
  *out_mask = (uint8_t)(1u << pin);
  return II2C_ERR_NONE;
}

int32_t aw9523b_port_dir_bits_get(ii2c_device_handle_t dev, uint8_t port, uint8_t *out_bits) {
  if (!out_bits) {
    return II2C_ERR_INVALID_ARG;
  }

  uint8_t reg = 0;
  int32_t err = aw9523b_port_to_reg(AW9523B_REG_CONFIG0, port, &reg);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  return aw9523b_reg8_read(dev, reg, out_bits);
}

int32_t aw9523b_port_dir_bits_set(ii2c_device_handle_t dev, uint8_t port, uint8_t bits) {
  uint8_t reg = 0;
  int32_t err = aw9523b_port_to_reg(AW9523B_REG_CONFIG0, port, &reg);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  return aw9523b_reg8_write(dev, reg, bits);
}

int32_t aw9523b_port_dir_bits_update(ii2c_device_handle_t dev,
                                     uint8_t port,
                                     uint8_t mask,
                                     uint8_t bits) {
  uint8_t reg = 0;
  int32_t err = aw9523b_port_to_reg(AW9523B_REG_CONFIG0, port, &reg);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  return aw9523b_reg8_update_bits(dev, reg, mask, bits);
}

int32_t aw9523b_port_dir_set(ii2c_device_handle_t dev,
                             uint8_t port,
                             uint8_t pin,
                             aw9523b_port_direction_t direction) {
  uint8_t reg = 0;
  uint8_t mask = 0;
  int32_t err = aw9523b_port_pin_to_reg_and_mask(AW9523B_REG_CONFIG0, port, pin, &reg, &mask);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint8_t new_value = 0;

  switch (direction) {
    case AW9523B_PORT_DIRECTION_OUTPUT:
      new_value = 0;
      break;
    case AW9523B_PORT_DIRECTION_INPUT:
      new_value = mask;
      break;
    default:
      return II2C_ERR_INVALID_ARG;
  }

  return aw9523b_reg8_update_bits(dev, reg, mask, new_value);
}

int32_t aw9523b_port_interrupt_bits_get(ii2c_device_handle_t dev, uint8_t port, uint8_t *out_bits) {
  if (!out_bits) {
    return II2C_ERR_INVALID_ARG;
  }

  uint8_t reg = 0;
  int32_t err = aw9523b_port_to_reg(AW9523B_REG_INTENABLE0, port, &reg);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint8_t current_value = 0;
  err = aw9523b_reg8_read(dev, reg, &current_value);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  *out_bits = (uint8_t)~current_value;
  return II2C_ERR_NONE;
}

int32_t aw9523b_port_interrupt_bits_set(ii2c_device_handle_t dev, uint8_t port, uint8_t bits) {
  uint8_t reg = 0;
  int32_t err = aw9523b_port_to_reg(AW9523B_REG_INTENABLE0, port, &reg);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  return aw9523b_reg8_write(dev, reg, (uint8_t)~bits);
}

int32_t aw9523b_port_interrupt_bits_update(ii2c_device_handle_t dev,
                                           uint8_t port,
                                           uint8_t mask,
                                           uint8_t bits) {
  uint8_t reg = 0;
  int32_t err = aw9523b_port_to_reg(AW9523B_REG_INTENABLE0, port, &reg);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  return aw9523b_reg8_update_bits(dev, reg, mask, (uint8_t)~bits);
}

int32_t aw9523b_interrupt_set(ii2c_device_handle_t dev, uint8_t port, uint8_t pin, bool enabled) {
  uint8_t reg = 0;
  uint8_t mask = 0;
  int32_t err = aw9523b_port_pin_to_reg_and_mask(AW9523B_REG_INTENABLE0, port, pin, &reg, &mask);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint8_t new_value = enabled ? 0 : mask;

  return aw9523b_reg8_update_bits(dev, reg, mask, new_value);
}

int32_t aw9523b_interrupt_get(ii2c_device_handle_t dev,
                              uint8_t port,
                              uint8_t pin,
                              uint8_t *out_enabled) {
  if (!out_enabled) {
    return II2C_ERR_INVALID_ARG;
  }

  uint8_t reg = 0;
  uint8_t mask = 0;
  int32_t err = aw9523b_port_pin_to_reg_and_mask(AW9523B_REG_INTENABLE0, port, pin, &reg, &mask);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint8_t current_value = 0;
  err = aw9523b_reg8_read(dev, reg, &current_value);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  *out_enabled = (current_value & mask) == 0 ? 1 : 0;
  return II2C_ERR_NONE;
}

int32_t aw9523b_level_set(ii2c_device_handle_t dev, uint8_t port, uint8_t pin, uint8_t level) {
  uint8_t reg = 0;
  uint8_t mask = 0;
  int32_t err = aw9523b_port_pin_to_reg_and_mask(AW9523B_REG_OUTPUT0, port, pin, &reg, &mask);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint8_t new_value = (level != 0) ? mask : 0;

  return aw9523b_reg8_update_bits(dev, reg, mask, new_value);
}

int32_t aw9523b_level_get(ii2c_device_handle_t dev, uint8_t port, uint8_t pin, uint8_t *out_level) {
  if (!out_level) {
    return II2C_ERR_INVALID_ARG;
  }

  uint8_t reg = 0;
  uint8_t mask = 0;
  int32_t err = aw9523b_port_pin_to_reg_and_mask(AW9523B_REG_INPUT0, port, pin, &reg, &mask);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  uint8_t current_value = 0;
  err = aw9523b_reg8_read(dev, reg, &current_value);
  if (err != II2C_ERR_NONE) {
    return err;
  }

  *out_level = (current_value & mask) != 0 ? 1 : 0;
  return II2C_ERR_NONE;
}
