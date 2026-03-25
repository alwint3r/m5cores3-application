#include "ili9342/ili9342.h"

#include <stdbool.h>

enum {
  ILI9342_CMD_SWRESET = 0x01,
  ILI9342_CMD_SLPIN = 0x10,
  ILI9342_CMD_SLPOUT = 0x11,
  ILI9342_CMD_DISPOFF = 0x28,
  ILI9342_CMD_DISPON = 0x29,
  ILI9342_CMD_CASET = 0x2A,
  ILI9342_CMD_PASET = 0x2B,
  ILI9342_CMD_RAMWR = 0x2C,
  ILI9342_CMD_MADCTL = 0x36,
  ILI9342_CMD_IDMOFF = 0x38,
  ILI9342_CMD_IDMON = 0x39,
  ILI9342_CMD_COLMOD = 0x3A,
  ILI9342_CMD_INVOFF = 0x20,
  ILI9342_CMD_INVON = 0x21,
  ILI9342_CMD_SETEXTC = 0xC8,
  ILI9342_CMD_PWCTR1 = 0xC0,
  ILI9342_CMD_PWCTR2 = 0xC1,
  ILI9342_CMD_VMCTR1 = 0xC5,
  ILI9342_CMD_BGRCTRL = 0xB0,
  ILI9342_CMD_IFMODE = 0xF6,
  ILI9342_CMD_GMCTRP1 = 0xE0,
  ILI9342_CMD_GMCTRN1 = 0xE1,
  ILI9342_CMD_DFUNCTR = 0xB6,
};

static bool ili9342_has_command_transport(const ili9342_t *display) {
  return display != NULL && display->transport_command_write != NULL;
}

static bool ili9342_has_data_transport(const ili9342_t *display) {
  return display != NULL && display->transport_data_write != NULL;
}

static bool ili9342_has_delay(const ili9342_t *display) {
  return display != NULL && display->delay_fn != NULL;
}

static int32_t ili9342_write_command_only(ili9342_t *display, uint8_t command) {
  if (!ili9342_has_command_transport(display)) {
    return ILI9342_ERR_INVALID_STATE;
  }

  return display->transport_command_write(display->transport_context, command);
}

const char *ili9342_err_to_name(int32_t err) {
  switch (err) {
    case ILI9342_ERR_NONE:
      return "ILI9342_ERR_NONE";
    case ILI9342_ERR_FAIL:
      return "ILI9342_ERR_FAIL";
    case ILI9342_ERR_NO_MEM:
      return "ILI9342_ERR_NO_MEM";
    case ILI9342_ERR_INVALID_ARG:
      return "ILI9342_ERR_INVALID_ARG";
    case ILI9342_ERR_INVALID_STATE:
      return "ILI9342_ERR_INVALID_STATE";
    case ILI9342_ERR_NOT_FOUND:
      return "ILI9342_ERR_NOT_FOUND";
    case ILI9342_ERR_NOT_SUPPORTED:
      return "ILI9342_ERR_NOT_SUPPORTED";
    case ILI9342_ERR_TIMEOUT:
      return "ILI9342_ERR_TIMEOUT";
    case ILI9342_ERR_IO:
      return "ILI9342_ERR_IO";
    default:
      return "ILI9342_ERR_UNKNOWN";
  }
}

int32_t ili9342_write_command(ili9342_t *display,
                              uint8_t command,
                              const uint8_t *data,
                              size_t len) {
  if (!ili9342_has_command_transport(display)) {
    return ILI9342_ERR_INVALID_STATE;
  }

  if (len > 0U && (!ili9342_has_data_transport(display) || data == NULL)) {
    if (!ili9342_has_data_transport(display)) {
      return ILI9342_ERR_INVALID_STATE;
    }

    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t err = display->transport_command_write(display->transport_context, command);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  if (len == 0U) {
    return ILI9342_ERR_NONE;
  }

  return display->transport_data_write(display->transport_context, data, len);
}

int32_t ili9342_write_data(ili9342_t *display, const uint8_t *data, size_t len) {
  if (!ili9342_has_data_transport(display)) {
    return ILI9342_ERR_INVALID_STATE;
  }

  if (data == NULL || len == 0U) {
    return ILI9342_ERR_INVALID_ARG;
  }

  return display->transport_data_write(display->transport_context, data, len);
}

int32_t ili9342_reset_software(ili9342_t *display) {
  if (!ili9342_has_delay(display)) {
    return ILI9342_ERR_INVALID_STATE;
  }

  int32_t err = ili9342_write_command_only(display, ILI9342_CMD_SWRESET);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  display->delay_fn(display->transport_context, 120);
  return ILI9342_ERR_NONE;
}

int32_t ili9342_sleep_enter(ili9342_t *display) {
  return ili9342_write_command_only(display, ILI9342_CMD_SLPIN);
}

int32_t ili9342_sleep_exit(ili9342_t *display) {
  if (!ili9342_has_delay(display)) {
    return ILI9342_ERR_INVALID_STATE;
  }

  int32_t err = ili9342_write_command_only(display, ILI9342_CMD_SLPOUT);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  display->delay_fn(display->transport_context, 120);
  return ILI9342_ERR_NONE;
}

int32_t ili9342_display_on(ili9342_t *display) {
  return ili9342_write_command_only(display, ILI9342_CMD_DISPON);
}

int32_t ili9342_display_off(ili9342_t *display) {
  return ili9342_write_command_only(display, ILI9342_CMD_DISPOFF);
}

int32_t ili9342_inversion_on(ili9342_t *display) {
  return ili9342_write_command_only(display, ILI9342_CMD_INVON);
}

int32_t ili9342_inversion_off(ili9342_t *display) {
  return ili9342_write_command_only(display, ILI9342_CMD_INVOFF);
}

int32_t ili9342_idle_mode_on(ili9342_t *display) {
  return ili9342_write_command_only(display, ILI9342_CMD_IDMON);
}

int32_t ili9342_idle_mode_off(ili9342_t *display) {
  return ili9342_write_command_only(display, ILI9342_CMD_IDMOFF);
}

int32_t ili9342_pixel_format_set(ili9342_t *display, uint8_t pixel_format) {
  return ili9342_write_command(display, ILI9342_CMD_COLMOD, &pixel_format, 1U);
}

int32_t ili9342_memory_access_control_set(ili9342_t *display, uint8_t value) {
  return ili9342_write_command(display, ILI9342_CMD_MADCTL, &value, 1U);
}

int32_t ili9342_address_window_set(ili9342_t *display,
                                   uint16_t x0,
                                   uint16_t y0,
                                   uint16_t x1,
                                   uint16_t y1) {
  if (x1 < x0 || y1 < y0) {
    return ILI9342_ERR_INVALID_ARG;
  }

  uint8_t column_data[4] = {
      (uint8_t)(x0 >> 8),
      (uint8_t)(x0 & 0xFF),
      (uint8_t)(x1 >> 8),
      (uint8_t)(x1 & 0xFF),
  };
  uint8_t row_data[4] = {
      (uint8_t)(y0 >> 8),
      (uint8_t)(y0 & 0xFF),
      (uint8_t)(y1 >> 8),
      (uint8_t)(y1 & 0xFF),
  };

  int32_t err = ili9342_write_command(display, ILI9342_CMD_CASET, column_data, sizeof(column_data));
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = ili9342_write_command(display, ILI9342_CMD_PASET, row_data, sizeof(row_data));
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  return ili9342_memory_write_begin(display);
}

int32_t ili9342_memory_write_begin(ili9342_t *display) {
  return ili9342_write_command_only(display, ILI9342_CMD_RAMWR);
}

int32_t ili9342_init_default(ili9342_t *display) {
  static const uint8_t setextc[] = {0xFF, 0x93, 0x42};
  static const uint8_t pwctr1[] = {0x12, 0x12};
  static const uint8_t pwctr2[] = {0x03};
  static const uint8_t vmctr1[] = {0xF2};
  static const uint8_t bgrctrl[] = {0xE0};
  static const uint8_t ifmode[] = {0x01, 0x00, 0x00};
  static const uint8_t gamma_positive[] = {
      0x00,
      0x0C,
      0x11,
      0x04,
      0x11,
      0x08,
      0x37,
      0x89,
      0x4C,
      0x06,
      0x0C,
      0x0A,
      0x2E,
      0x34,
      0x0F,
  };
  static const uint8_t gamma_negative[] = {
      0x00,
      0x0B,
      0x11,
      0x05,
      0x13,
      0x09,
      0x33,
      0x67,
      0x48,
      0x07,
      0x0E,
      0x0B,
      0x2E,
      0x33,
      0x0F,
  };
  static const uint8_t display_function_control[] = {0x08, 0x82, 0x1D, 0x04};

  if (!ili9342_has_delay(display)) {
    return ILI9342_ERR_INVALID_STATE;
  }

  int32_t err = ili9342_reset_software(display);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = ili9342_write_command(display, ILI9342_CMD_SETEXTC, setextc, sizeof(setextc));
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = ili9342_write_command(display, ILI9342_CMD_PWCTR1, pwctr1, sizeof(pwctr1));
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = ili9342_write_command(display, ILI9342_CMD_PWCTR2, pwctr2, sizeof(pwctr2));
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = ili9342_write_command(display, ILI9342_CMD_VMCTR1, vmctr1, sizeof(vmctr1));
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = ili9342_write_command(display, ILI9342_CMD_BGRCTRL, bgrctrl, sizeof(bgrctrl));
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = ili9342_write_command(display, ILI9342_CMD_IFMODE, ifmode, sizeof(ifmode));
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = ili9342_write_command(display, ILI9342_CMD_GMCTRP1, gamma_positive, sizeof(gamma_positive));
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = ili9342_write_command(display, ILI9342_CMD_GMCTRN1, gamma_negative, sizeof(gamma_negative));
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = ili9342_write_command(
      display, ILI9342_CMD_DFUNCTR, display_function_control, sizeof(display_function_control));
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = ili9342_sleep_exit(display);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = ili9342_inversion_on(display);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = ili9342_pixel_format_set(display, ILI9342_PIXEL_FORMAT_RGB565);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = ili9342_memory_access_control_set(display, ILI9342_MADCTL_BGR);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = ili9342_idle_mode_off(display);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  err = ili9342_display_on(display);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  display->delay_fn(display->transport_context, 20);
  return ILI9342_ERR_NONE;
}
