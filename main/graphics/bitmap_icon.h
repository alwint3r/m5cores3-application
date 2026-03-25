#pragma once

#include <stdint.h>

#include "display_surface.h"

typedef enum {
  GRAPHICS_BITMAP_MASK_1BPP = 1,
  GRAPHICS_BITMAP_MASK_4BPP = 4,
  GRAPHICS_BITMAP_MASK_8BPP = 8,
} graphics_bitmap_mask_format_t;

typedef struct {
  const uint8_t *data;
  uint16_t width;
  uint16_t height;
  uint16_t stride_bytes;
  graphics_bitmap_mask_format_t format;
} graphics_bitmap_icon_t;

int32_t graphics_draw_bitmap_icon(display_surface_t *surface,
                                  const graphics_bitmap_icon_t *icon,
                                  int16_t x,
                                  int16_t y,
                                  const graphics_rect_t *clip,
                                  uint16_t foreground_color,
                                  uint16_t background_color);
