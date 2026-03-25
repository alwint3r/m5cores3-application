#include "aw9523b/aw9523b.h"
#include "aw9523b/aw9523b_register.h"

static bool aw9523b_has_write(const aw9523b_t *expander) {
  return expander != NULL && expander->transport_write != NULL;
}

static bool aw9523b_has_combined_read(const aw9523b_t *expander) {
  return expander != NULL && expander->transport_write_read != NULL;
}

static int32_t aw9523b_transport_write_exact(aw9523b_t *expander,
                                             const uint8_t *write_buffer,
                                             size_t write_size) {
  if (!aw9523b_has_write(expander)) {
    return expander == NULL ? AW9523B_ERR_INVALID_ARG : AW9523B_ERR_INVALID_STATE;
  }

  if (write_buffer == NULL || write_size == 0U) {
    return AW9523B_ERR_INVALID_ARG;
  }

  return expander->transport_write(write_buffer, write_size);
}

static int32_t aw9523b_transport_read_exact(aw9523b_t *expander,
                                            const uint8_t *write_buffer,
                                            size_t write_size,
                                            uint8_t *read_buffer,
                                            size_t read_size) {
  if (expander == NULL || write_buffer == NULL || write_size == 0U || read_buffer == NULL ||
      read_size == 0U) {
    return AW9523B_ERR_INVALID_ARG;
  }

  if (!aw9523b_has_combined_read(expander)) {
    return AW9523B_ERR_INVALID_STATE;
  }

  size_t actual_read_size = read_size;
  int32_t err = expander->transport_write_read(
      write_buffer, write_size, read_buffer, &actual_read_size, read_size);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  return actual_read_size == read_size ? AW9523B_ERR_NONE : AW9523B_ERR_IO;
}

const char *aw9523b_err_to_name(int32_t err) {
  switch (err) {
    case AW9523B_ERR_NONE:
      return "AW9523B_ERR_NONE";
    case AW9523B_ERR_FAIL:
      return "AW9523B_ERR_FAIL";
    case AW9523B_ERR_NO_MEM:
      return "AW9523B_ERR_NO_MEM";
    case AW9523B_ERR_INVALID_ARG:
      return "AW9523B_ERR_INVALID_ARG";
    case AW9523B_ERR_INVALID_STATE:
      return "AW9523B_ERR_INVALID_STATE";
    case AW9523B_ERR_NOT_FOUND:
      return "AW9523B_ERR_NOT_FOUND";
    case AW9523B_ERR_NOT_SUPPORTED:
      return "AW9523B_ERR_NOT_SUPPORTED";
    case AW9523B_ERR_TIMEOUT:
      return "AW9523B_ERR_TIMEOUT";
    case AW9523B_ERR_IO:
      return "AW9523B_ERR_IO";
    default:
      return "AW9523B_ERR_UNKNOWN";
  }
}

static int32_t aw9523b_port_to_reg(uint8_t base_reg, uint8_t port, uint8_t *out_reg) {
  if (!out_reg) {
    return AW9523B_ERR_INVALID_ARG;
  }

  if (port > 1) {
    return AW9523B_ERR_INVALID_ARG;
  }

  *out_reg = (uint8_t)(base_reg + port);
  return AW9523B_ERR_NONE;
}

static int32_t aw9523b_port_pin_to_reg_and_mask(uint8_t base_reg,
                                                uint8_t port,
                                                uint8_t pin,
                                                uint8_t *out_reg,
                                                uint8_t *out_mask) {
  if (!out_reg || !out_mask) {
    return AW9523B_ERR_INVALID_ARG;
  }

  if (port > 1 || pin > 7) {
    return AW9523B_ERR_INVALID_ARG;
  }

  *out_reg = (uint8_t)(base_reg + port);
  *out_mask = (uint8_t)(1u << pin);
  return AW9523B_ERR_NONE;
}

int32_t aw9523b_reg8_read(aw9523b_t *expander, uint8_t reg, uint8_t *out_value) {
  if (out_value == NULL) {
    return AW9523B_ERR_INVALID_ARG;
  }

  return aw9523b_transport_read_exact(expander, &reg, 1, out_value, 1);
}

int32_t aw9523b_reg8_write(aw9523b_t *expander, uint8_t reg, uint8_t value) {
  return aw9523b_transport_write_exact(expander, (uint8_t[2]){reg, value}, 2);
}

int32_t aw9523b_reg8_set_bits(aw9523b_t *expander, uint8_t reg, uint8_t bits) {
  uint8_t current_value = 0;
  int32_t err = aw9523b_reg8_read(expander, reg, &current_value);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  return aw9523b_reg8_write(expander, reg, (uint8_t)(current_value | bits));
}

int32_t aw9523b_reg8_update_bits(aw9523b_t *expander,
                                 uint8_t reg,
                                 uint8_t mask,
                                 uint8_t new_value) {
  uint8_t current_value = 0;
  int32_t err = aw9523b_reg8_read(expander, reg, &current_value);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  current_value = (uint8_t)((new_value & mask) | (current_value & (uint8_t)~mask));
  return aw9523b_reg8_write(expander, reg, current_value);
}

int32_t aw9523b_id_get(aw9523b_t *expander, uint8_t *out) {
  if (!out) {
    return AW9523B_ERR_INVALID_ARG;
  }

  return aw9523b_reg8_read(expander, AW9523B_REG_ID, out);
}

int32_t aw9523b_port0_drive_mode_get(aw9523b_t *expander, aw9523b_port0_drive_mode_t *out_mode) {
  if (!out_mode) {
    return AW9523B_ERR_INVALID_ARG;
  }

  uint8_t gcr = 0;
  int32_t err = aw9523b_reg8_read(expander, AW9523B_REG_GCR, &gcr);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  *out_mode = (gcr & AW9523B_GCR_PORT0_DRIVE_MODE_MASK) != 0 ? AW9523B_PORT0_DRIVE_MODE_PUSH_PULL
                                                             : AW9523B_PORT0_DRIVE_MODE_OPEN_DRAIN;
  return AW9523B_ERR_NONE;
}

int32_t aw9523b_port0_drive_mode_set(aw9523b_t *expander, aw9523b_port0_drive_mode_t mode) {
  uint8_t new_value = 0;

  switch (mode) {
    case AW9523B_PORT0_DRIVE_MODE_OPEN_DRAIN:
      new_value = AW9523B_GCR_PORT0_DRIVE_MODE_OPEN_DRAIN;
      break;
    case AW9523B_PORT0_DRIVE_MODE_PUSH_PULL:
      new_value = AW9523B_GCR_PORT0_DRIVE_MODE_PUSH_PULL;
      break;
    default:
      return AW9523B_ERR_INVALID_ARG;
  }

  return aw9523b_reg8_update_bits(
      expander, AW9523B_REG_GCR, AW9523B_GCR_PORT0_DRIVE_MODE_MASK, new_value);
}

int32_t aw9523b_port_dir_bits_get(aw9523b_t *expander, uint8_t port, uint8_t *out_bits) {
  if (!out_bits) {
    return AW9523B_ERR_INVALID_ARG;
  }

  uint8_t reg = 0;
  int32_t err = aw9523b_port_to_reg(AW9523B_REG_CONFIG0, port, &reg);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  return aw9523b_reg8_read(expander, reg, out_bits);
}

int32_t aw9523b_port_dir_bits_set(aw9523b_t *expander, uint8_t port, uint8_t bits) {
  uint8_t reg = 0;
  int32_t err = aw9523b_port_to_reg(AW9523B_REG_CONFIG0, port, &reg);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  return aw9523b_reg8_write(expander, reg, bits);
}

int32_t aw9523b_port_dir_bits_update(aw9523b_t *expander,
                                     uint8_t port,
                                     uint8_t mask,
                                     uint8_t bits) {
  uint8_t reg = 0;
  int32_t err = aw9523b_port_to_reg(AW9523B_REG_CONFIG0, port, &reg);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  return aw9523b_reg8_update_bits(expander, reg, mask, bits);
}

int32_t aw9523b_port_dir_set(aw9523b_t *expander,
                             uint8_t port,
                             uint8_t pin,
                             aw9523b_port_direction_t direction) {
  uint8_t reg = 0;
  uint8_t mask = 0;
  int32_t err = aw9523b_port_pin_to_reg_and_mask(AW9523B_REG_CONFIG0, port, pin, &reg, &mask);
  if (err != AW9523B_ERR_NONE) {
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
      return AW9523B_ERR_INVALID_ARG;
  }

  return aw9523b_reg8_update_bits(expander, reg, mask, new_value);
}

int32_t aw9523b_port_interrupt_bits_get(aw9523b_t *expander, uint8_t port, uint8_t *out_bits) {
  if (!out_bits) {
    return AW9523B_ERR_INVALID_ARG;
  }

  uint8_t reg = 0;
  int32_t err = aw9523b_port_to_reg(AW9523B_REG_INTENABLE0, port, &reg);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  uint8_t current_value = 0;
  err = aw9523b_reg8_read(expander, reg, &current_value);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  *out_bits = (uint8_t)~current_value;
  return AW9523B_ERR_NONE;
}

int32_t aw9523b_port_interrupt_bits_set(aw9523b_t *expander, uint8_t port, uint8_t bits) {
  uint8_t reg = 0;
  int32_t err = aw9523b_port_to_reg(AW9523B_REG_INTENABLE0, port, &reg);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  return aw9523b_reg8_write(expander, reg, (uint8_t)~bits);
}

int32_t aw9523b_port_interrupt_bits_update(aw9523b_t *expander,
                                           uint8_t port,
                                           uint8_t mask,
                                           uint8_t bits) {
  uint8_t reg = 0;
  int32_t err = aw9523b_port_to_reg(AW9523B_REG_INTENABLE0, port, &reg);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  return aw9523b_reg8_update_bits(expander, reg, mask, (uint8_t)~bits);
}

int32_t aw9523b_interrupt_set(aw9523b_t *expander, uint8_t port, uint8_t pin, bool enabled) {
  uint8_t reg = 0;
  uint8_t mask = 0;
  int32_t err = aw9523b_port_pin_to_reg_and_mask(AW9523B_REG_INTENABLE0, port, pin, &reg, &mask);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  return aw9523b_reg8_update_bits(expander, reg, mask, enabled ? 0 : mask);
}

int32_t aw9523b_interrupt_get(aw9523b_t *expander,
                              uint8_t port,
                              uint8_t pin,
                              uint8_t *out_enabled) {
  if (!out_enabled) {
    return AW9523B_ERR_INVALID_ARG;
  }

  uint8_t reg = 0;
  uint8_t mask = 0;
  int32_t err = aw9523b_port_pin_to_reg_and_mask(AW9523B_REG_INTENABLE0, port, pin, &reg, &mask);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  uint8_t current_value = 0;
  err = aw9523b_reg8_read(expander, reg, &current_value);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  *out_enabled = (current_value & mask) == 0 ? 1 : 0;
  return AW9523B_ERR_NONE;
}

int32_t aw9523b_port_input_read(aw9523b_t *expander, uint8_t port, uint8_t *input_value) {
  if (!input_value) {
    return AW9523B_ERR_INVALID_ARG;
  }

  uint8_t reg = 0;
  int32_t err = aw9523b_port_to_reg(AW9523B_REG_INPUT0, port, &reg);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  return aw9523b_reg8_read(expander, reg, input_value);
}

int32_t aw9523b_port_output_read(aw9523b_t *expander, uint8_t port, uint8_t *output_value) {
  if (!output_value) {
    return AW9523B_ERR_INVALID_ARG;
  }

  uint8_t reg = 0;
  int32_t err = aw9523b_port_to_reg(AW9523B_REG_OUTPUT0, port, &reg);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  return aw9523b_reg8_read(expander, reg, output_value);
}

int32_t aw9523b_level_set(aw9523b_t *expander, uint8_t port, uint8_t pin, uint8_t level) {
  uint8_t reg = 0;
  uint8_t mask = 0;
  int32_t err = aw9523b_port_pin_to_reg_and_mask(AW9523B_REG_OUTPUT0, port, pin, &reg, &mask);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  return aw9523b_reg8_update_bits(expander, reg, mask, (level != 0U) ? mask : 0U);
}

int32_t aw9523b_level_get(aw9523b_t *expander, uint8_t port, uint8_t pin, uint8_t *out_level) {
  if (!out_level) {
    return AW9523B_ERR_INVALID_ARG;
  }

  uint8_t reg = 0;
  uint8_t mask = 0;
  int32_t err = aw9523b_port_pin_to_reg_and_mask(AW9523B_REG_INPUT0, port, pin, &reg, &mask);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  uint8_t current_value = 0;
  err = aw9523b_reg8_read(expander, reg, &current_value);
  if (err != AW9523B_ERR_NONE) {
    return err;
  }

  *out_level = (current_value & mask) != 0U ? 1U : 0U;
  return AW9523B_ERR_NONE;
}
