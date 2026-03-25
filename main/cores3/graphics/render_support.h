#pragma once

#include <stddef.h>
#include <stdint.h>

#include "bitmap_icon.h"

int32_t graphics_mask_coverage_get(graphics_bitmap_mask_format_t format,
                                   const uint8_t *data,
                                   uint16_t stride_bytes,
                                   uint16_t width,
                                   uint16_t height,
                                   int src_x,
                                   int src_y,
                                   uint8_t *coverage);

uint16_t graphics_blend_rgb565(uint16_t background_color,
                               uint16_t foreground_color,
                               uint8_t coverage);

void graphics_store_rgb565(uint8_t *dst, uint16_t color);
void graphics_fill_color_span(uint8_t *buffer, size_t pixel_count, uint16_t color);

int32_t graphics_write_buffer_chunked(display_surface_t *surface,
                                      const uint8_t *buffer,
                                      size_t len);
