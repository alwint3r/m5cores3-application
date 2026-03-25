#include "ft6x36/ft6x36.h"

#include <string.h>

enum {
  FT6X36_TOUCH_FRAME_SIZE = FT6X36_REG_P2_MISC - FT6X36_REG_GEST_ID + 1,
};

static bool ft6x36_has_write(const ft6x36_t *touch) {
  return touch != NULL && touch->transport_write != NULL;
}

static bool ft6x36_has_combined_read(const ft6x36_t *touch) {
  return touch != NULL && touch->transport_write_read != NULL;
}

static void ft6x36_touch_point_clear(ft6x36_touch_point_t *point) {
  if (point == NULL) {
    return;
  }

  memset(point, 0, sizeof(*point));
  point->event = FT6X36_TOUCH_EVENT_NO_EVENT;
  point->touch_id = FT6X36_TOUCH_POINT_ID_INVALID;
}

static void ft6x36_touch_point_decode(ft6x36_touch_point_t *out_point, const uint8_t *raw_point) {
  out_point->event =
      (ft6x36_touch_event_t)((raw_point[0] & FT6X36_TOUCH_EVENT_MASK) >> FT6X36_TOUCH_EVENT_SHIFT);
  out_point->x =
      (uint16_t)(((uint16_t)(raw_point[0] & FT6X36_TOUCH_X_POS_H_MASK) << 8) | raw_point[1]);
  out_point->touch_id = (uint8_t)((raw_point[2] & FT6X36_TOUCH_ID_MASK) >> FT6X36_TOUCH_ID_SHIFT);
  out_point->y =
      (uint16_t)(((uint16_t)(raw_point[2] & FT6X36_TOUCH_Y_POS_H_MASK) << 8) | raw_point[3]);
  out_point->weight = raw_point[4];
  out_point->area = (uint8_t)((raw_point[5] & FT6X36_TOUCH_AREA_MASK) >> FT6X36_TOUCH_AREA_SHIFT);
  out_point->valid = out_point->event != FT6X36_TOUCH_EVENT_NO_EVENT &&
                     out_point->touch_id != FT6X36_TOUCH_POINT_ID_INVALID;
}

static int32_t ft6x36_reg16_be_read(ft6x36_t *touch, uint8_t reg, uint16_t *out_value) {
  if (out_value == NULL) {
    return FT6X36_ERR_INVALID_ARG;
  }

  uint8_t bytes[2] = {0};
  int32_t err = ft6x36_reg_read(touch, reg, bytes, sizeof(bytes));
  if (err != FT6X36_ERR_NONE) {
    return err;
  }

  *out_value = (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
  return FT6X36_ERR_NONE;
}

const char *ft6x36_err_to_name(int32_t err) {
  switch (err) {
    case FT6X36_ERR_NONE:
      return "FT6X36_ERR_NONE";
    case FT6X36_ERR_FAIL:
      return "FT6X36_ERR_FAIL";
    case FT6X36_ERR_NO_MEM:
      return "FT6X36_ERR_NO_MEM";
    case FT6X36_ERR_INVALID_ARG:
      return "FT6X36_ERR_INVALID_ARG";
    case FT6X36_ERR_INVALID_STATE:
      return "FT6X36_ERR_INVALID_STATE";
    case FT6X36_ERR_NOT_FOUND:
      return "FT6X36_ERR_NOT_FOUND";
    case FT6X36_ERR_NOT_SUPPORTED:
      return "FT6X36_ERR_NOT_SUPPORTED";
    case FT6X36_ERR_TIMEOUT:
      return "FT6X36_ERR_TIMEOUT";
    case FT6X36_ERR_IO:
      return "FT6X36_ERR_IO";
    default:
      return "FT6X36_ERR_UNKNOWN";
  }
}

int32_t ft6x36_reg_read(ft6x36_t *touch, uint8_t reg, uint8_t *out_data, size_t len) {
  if (touch == NULL || out_data == NULL || len == 0U) {
    return FT6X36_ERR_INVALID_ARG;
  }

  if (!ft6x36_has_combined_read(touch)) {
    return FT6X36_ERR_INVALID_STATE;
  }

  size_t read_size = len;
  int32_t err =
      touch->transport_write_read(touch->transport_context, &reg, 1, out_data, &read_size, len);
  if (err != FT6X36_ERR_NONE) {
    return err;
  }

  return read_size == len ? FT6X36_ERR_NONE : FT6X36_ERR_IO;
}

int32_t ft6x36_reg8_read(ft6x36_t *touch, uint8_t reg, uint8_t *out_value) {
  if (out_value == NULL) {
    return FT6X36_ERR_INVALID_ARG;
  }

  return ft6x36_reg_read(touch, reg, out_value, 1);
}

int32_t ft6x36_reg8_write(ft6x36_t *touch, uint8_t reg, uint8_t value) {
  if (!ft6x36_has_write(touch)) {
    return touch == NULL ? FT6X36_ERR_INVALID_ARG : FT6X36_ERR_INVALID_STATE;
  }

  return touch->transport_write(touch->transport_context, (uint8_t[2]){reg, value}, 2);
}

int32_t ft6x36_reg8_set_bits(ft6x36_t *touch, uint8_t reg, uint8_t bits) {
  uint8_t current_value = 0;
  int32_t err = ft6x36_reg8_read(touch, reg, &current_value);
  if (err != FT6X36_ERR_NONE) {
    return err;
  }

  return ft6x36_reg8_write(touch, reg, (uint8_t)(current_value | bits));
}

int32_t ft6x36_reg8_update_bits(ft6x36_t *touch, uint8_t reg, uint8_t mask, uint8_t new_value) {
  uint8_t current_value = 0;
  int32_t err = ft6x36_reg8_read(touch, reg, &current_value);
  if (err != FT6X36_ERR_NONE) {
    return err;
  }

  current_value = (uint8_t)((new_value & mask) | (current_value & (uint8_t)~mask));
  return ft6x36_reg8_write(touch, reg, current_value);
}

int32_t ft6x36_dev_mode_get(ft6x36_t *touch, ft6x36_device_mode_t *out_mode) {
  if (out_mode == NULL) {
    return FT6X36_ERR_INVALID_ARG;
  }

  uint8_t reg_value = 0;
  int32_t err = ft6x36_reg8_read(touch, FT6X36_REG_DEV_MODE, &reg_value);
  if (err != FT6X36_ERR_NONE) {
    return err;
  }

  switch ((reg_value & FT6X36_DEV_MODE_MASK) >> FT6X36_DEV_MODE_SHIFT) {
    case FT6X36_DEVICE_MODE_WORKING:
      *out_mode = FT6X36_DEVICE_MODE_WORKING;
      return FT6X36_ERR_NONE;
    case FT6X36_DEVICE_MODE_FACTORY:
      *out_mode = FT6X36_DEVICE_MODE_FACTORY;
      return FT6X36_ERR_NONE;
    default:
      return FT6X36_ERR_INVALID_STATE;
  }
}

int32_t ft6x36_dev_mode_set(ft6x36_t *touch, ft6x36_device_mode_t mode) {
  switch (mode) {
    case FT6X36_DEVICE_MODE_WORKING:
    case FT6X36_DEVICE_MODE_FACTORY:
      return ft6x36_reg8_write(
          touch, FT6X36_REG_DEV_MODE, (uint8_t)(((uint8_t)mode & 0x07U) << FT6X36_DEV_MODE_SHIFT));
    default:
      return FT6X36_ERR_INVALID_ARG;
  }
}

int32_t ft6x36_gesture_id_get(ft6x36_t *touch, ft6x36_gesture_id_t *out_gesture) {
  if (out_gesture == NULL) {
    return FT6X36_ERR_INVALID_ARG;
  }

  uint8_t reg_value = 0;
  int32_t err = ft6x36_reg8_read(touch, FT6X36_REG_GEST_ID, &reg_value);
  if (err != FT6X36_ERR_NONE) {
    return err;
  }

  *out_gesture = (ft6x36_gesture_id_t)reg_value;
  return FT6X36_ERR_NONE;
}

int32_t ft6x36_touch_count_get(ft6x36_t *touch, uint8_t *out_count) {
  if (out_count == NULL) {
    return FT6X36_ERR_INVALID_ARG;
  }

  uint8_t reg_value = 0;
  int32_t err = ft6x36_reg8_read(touch, FT6X36_REG_TD_STATUS, &reg_value);
  if (err != FT6X36_ERR_NONE) {
    return err;
  }

  *out_count = (uint8_t)(reg_value & FT6X36_TD_STATUS_TOUCH_COUNT_MASK);
  if (*out_count > FT6X36_TOUCH_POINTS_MAX) {
    return FT6X36_ERR_INVALID_STATE;
  }

  return FT6X36_ERR_NONE;
}

int32_t ft6x36_touch_data_get(ft6x36_t *touch, ft6x36_touch_data_t *out_data) {
  if (out_data == NULL) {
    return FT6X36_ERR_INVALID_ARG;
  }

  memset(out_data, 0, sizeof(*out_data));
  for (size_t i = 0; i < FT6X36_TOUCH_POINTS_MAX; ++i) {
    ft6x36_touch_point_clear(&out_data->points[i]);
  }

  uint8_t frame[FT6X36_TOUCH_FRAME_SIZE] = {0};
  int32_t err = ft6x36_reg_read(touch, FT6X36_REG_GEST_ID, frame, sizeof(frame));
  if (err != FT6X36_ERR_NONE) {
    return err;
  }

  out_data->gesture_id = (ft6x36_gesture_id_t)frame[0];
  out_data->touch_count = (uint8_t)(frame[1] & FT6X36_TD_STATUS_TOUCH_COUNT_MASK);
  if (out_data->touch_count > FT6X36_TOUCH_POINTS_MAX) {
    return FT6X36_ERR_INVALID_STATE;
  }

  ft6x36_touch_point_decode(&out_data->points[0], &frame[2]);
  ft6x36_touch_point_decode(&out_data->points[1], &frame[2 + FT6X36_TOUCH_POINT_STRIDE]);

  for (size_t i = out_data->touch_count; i < FT6X36_TOUCH_POINTS_MAX; ++i) {
    ft6x36_touch_point_clear(&out_data->points[i]);
  }

  return FT6X36_ERR_NONE;
}

int32_t ft6x36_touch_point_get(ft6x36_t *touch,
                               uint8_t point_index,
                               ft6x36_touch_point_t *out_point) {
  if (out_point == NULL) {
    return FT6X36_ERR_INVALID_ARG;
  }

  if (point_index >= FT6X36_TOUCH_POINTS_MAX) {
    return FT6X36_ERR_INVALID_ARG;
  }

  ft6x36_touch_data_t touch_data;
  int32_t err = ft6x36_touch_data_get(touch, &touch_data);
  if (err != FT6X36_ERR_NONE) {
    return err;
  }

  if (point_index >= touch_data.touch_count) {
    return FT6X36_ERR_NOT_FOUND;
  }

  if (!touch_data.points[point_index].valid) {
    return FT6X36_ERR_INVALID_STATE;
  }

  *out_point = touch_data.points[point_index];
  return FT6X36_ERR_NONE;
}

#define FT6X36_DEFINE_REG8_GETTER(function_name, reg_name)     \
  int32_t function_name(ft6x36_t *touch, uint8_t *out_value) { \
    if (out_value == NULL) {                                   \
      return FT6X36_ERR_INVALID_ARG;                           \
    }                                                          \
    return ft6x36_reg8_read(touch, reg_name, out_value);       \
  }

#define FT6X36_DEFINE_REG8_SETTER(function_name, reg_name) \
  int32_t function_name(ft6x36_t *touch, uint8_t value) {  \
    return ft6x36_reg8_write(touch, reg_name, value);      \
  }

FT6X36_DEFINE_REG8_GETTER(ft6x36_touch_threshold_get, FT6X36_REG_TH_GROUP)
FT6X36_DEFINE_REG8_SETTER(ft6x36_touch_threshold_set, FT6X36_REG_TH_GROUP)
FT6X36_DEFINE_REG8_GETTER(ft6x36_filter_coefficient_get, FT6X36_REG_TH_DIFF)
FT6X36_DEFINE_REG8_SETTER(ft6x36_filter_coefficient_set, FT6X36_REG_TH_DIFF)
FT6X36_DEFINE_REG8_GETTER(ft6x36_active_to_monitor_time_get, FT6X36_REG_TIMEENTERMONITOR)
FT6X36_DEFINE_REG8_SETTER(ft6x36_active_to_monitor_time_set, FT6X36_REG_TIMEENTERMONITOR)
FT6X36_DEFINE_REG8_GETTER(ft6x36_active_report_rate_get, FT6X36_REG_PERIODACTIVE)
FT6X36_DEFINE_REG8_SETTER(ft6x36_active_report_rate_set, FT6X36_REG_PERIODACTIVE)
FT6X36_DEFINE_REG8_GETTER(ft6x36_monitor_report_rate_get, FT6X36_REG_PERIODMONITOR)
FT6X36_DEFINE_REG8_SETTER(ft6x36_monitor_report_rate_set, FT6X36_REG_PERIODMONITOR)
FT6X36_DEFINE_REG8_GETTER(ft6x36_minimum_rotation_angle_get, FT6X36_REG_RADIAN_VALUE)
FT6X36_DEFINE_REG8_SETTER(ft6x36_minimum_rotation_angle_set, FT6X36_REG_RADIAN_VALUE)
FT6X36_DEFINE_REG8_GETTER(ft6x36_left_right_offset_get, FT6X36_REG_OFFSET_LEFT_RIGHT)
FT6X36_DEFINE_REG8_SETTER(ft6x36_left_right_offset_set, FT6X36_REG_OFFSET_LEFT_RIGHT)
FT6X36_DEFINE_REG8_GETTER(ft6x36_up_down_offset_get, FT6X36_REG_OFFSET_UP_DOWN)
FT6X36_DEFINE_REG8_SETTER(ft6x36_up_down_offset_set, FT6X36_REG_OFFSET_UP_DOWN)
FT6X36_DEFINE_REG8_GETTER(ft6x36_left_right_distance_get, FT6X36_REG_DISTANCE_LEFT_RIGHT)
FT6X36_DEFINE_REG8_SETTER(ft6x36_left_right_distance_set, FT6X36_REG_DISTANCE_LEFT_RIGHT)
FT6X36_DEFINE_REG8_GETTER(ft6x36_up_down_distance_get, FT6X36_REG_DISTANCE_UP_DOWN)
FT6X36_DEFINE_REG8_SETTER(ft6x36_up_down_distance_set, FT6X36_REG_DISTANCE_UP_DOWN)
FT6X36_DEFINE_REG8_GETTER(ft6x36_zoom_distance_get, FT6X36_REG_DISTANCE_ZOOM)
FT6X36_DEFINE_REG8_SETTER(ft6x36_zoom_distance_set, FT6X36_REG_DISTANCE_ZOOM)
FT6X36_DEFINE_REG8_GETTER(ft6x36_cipher_get, FT6X36_REG_CIPHER)
FT6X36_DEFINE_REG8_GETTER(ft6x36_power_mode_get, FT6X36_REG_PWR_MODE)
FT6X36_DEFINE_REG8_SETTER(ft6x36_power_mode_set, FT6X36_REG_PWR_MODE)
FT6X36_DEFINE_REG8_GETTER(ft6x36_firmware_version_get, FT6X36_REG_FIRMID)
FT6X36_DEFINE_REG8_GETTER(ft6x36_panel_id_get, FT6X36_REG_FOCALTECH_ID)
FT6X36_DEFINE_REG8_GETTER(ft6x36_release_code_version_get, FT6X36_REG_RELEASE_CODE_ID)
FT6X36_DEFINE_REG8_GETTER(ft6x36_operating_state_get, FT6X36_REG_STATE)
FT6X36_DEFINE_REG8_SETTER(ft6x36_operating_state_set, FT6X36_REG_STATE)

int32_t ft6x36_ctrl_mode_get(ft6x36_t *touch, ft6x36_ctrl_mode_t *ctrl_out) {
  if (ctrl_out == NULL) {
    return FT6X36_ERR_INVALID_ARG;
  }

  uint8_t reg_value = 0;
  int32_t err = ft6x36_reg8_read(touch, FT6X36_REG_CTRL, &reg_value);
  if (err != FT6X36_ERR_NONE) {
    return err;
  }

  switch (reg_value) {
    case FT6X36_CTRL_MODE_ACTIVE:
      *ctrl_out = FT6X36_CTRL_MODE_ACTIVE;
      return FT6X36_ERR_NONE;
    case FT6X36_CTRL_MODE_MONITOR:
      *ctrl_out = FT6X36_CTRL_MODE_MONITOR;
      return FT6X36_ERR_NONE;
    default:
      return FT6X36_ERR_INVALID_STATE;
  }
}

int32_t ft6x36_ctrl_mode_set(ft6x36_t *touch, ft6x36_ctrl_mode_t ctrl) {
  switch (ctrl) {
    case FT6X36_CTRL_MODE_ACTIVE:
    case FT6X36_CTRL_MODE_MONITOR:
      return ft6x36_reg8_write(touch, FT6X36_REG_CTRL, (uint8_t)ctrl);
    default:
      return FT6X36_ERR_INVALID_ARG;
  }
}

int32_t ft6x36_library_version_get(ft6x36_t *touch, uint16_t *out_version) {
  return ft6x36_reg16_be_read(touch, FT6X36_REG_LIB_VER_H, out_version);
}

int32_t ft6x36_interrupt_mode_get(ft6x36_t *touch, ft6x36_interrupt_mode_t *out_mode) {
  if (out_mode == NULL) {
    return FT6X36_ERR_INVALID_ARG;
  }

  uint8_t reg_value = 0;
  int32_t err = ft6x36_reg8_read(touch, FT6X36_REG_G_MODE, &reg_value);
  if (err != FT6X36_ERR_NONE) {
    return err;
  }

  switch (reg_value) {
    case FT6X36_INTERRUPT_MODE_POLLING:
      *out_mode = FT6X36_INTERRUPT_MODE_POLLING;
      return FT6X36_ERR_NONE;
    case FT6X36_INTERRUPT_MODE_TRIGGER:
      *out_mode = FT6X36_INTERRUPT_MODE_TRIGGER;
      return FT6X36_ERR_NONE;
    default:
      return FT6X36_ERR_INVALID_STATE;
  }
}

int32_t ft6x36_interrupt_mode_set(ft6x36_t *touch, ft6x36_interrupt_mode_t mode) {
  switch (mode) {
    case FT6X36_INTERRUPT_MODE_POLLING:
    case FT6X36_INTERRUPT_MODE_TRIGGER:
      return ft6x36_reg8_write(touch, FT6X36_REG_G_MODE, (uint8_t)mode);
    default:
      return FT6X36_ERR_INVALID_ARG;
  }
}
