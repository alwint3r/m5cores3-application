#include "render_support.h"

void graphics_store_rgb565(uint8_t *dst, uint16_t color) {
  if (dst == NULL) {
    return;
  }

  dst[0] = (uint8_t)(color >> 8);
  dst[1] = (uint8_t)(color & 0xFF);
}

void graphics_fill_color_span(uint8_t *buffer, size_t pixel_count, uint16_t color) {
  if (buffer == NULL) {
    return;
  }

  for (size_t i = 0; i < pixel_count; i++) {
    graphics_store_rgb565(buffer + (i * 2U), color);
  }
}

int32_t graphics_write_buffer_chunked(display_surface_t *surface,
                                      const uint8_t *buffer,
                                      size_t len) {
  if (surface == NULL || surface->panel == NULL || buffer == NULL || len == 0U) {
    return ILI9342_ERR_INVALID_ARG;
  }

  size_t chunk_bytes = surface->max_transfer_bytes;
  if (chunk_bytes == 0U) {
    chunk_bytes = len;
  }

  size_t offset = 0U;
  while (offset < len) {
    size_t bytes_this_round = len - offset;
    if (bytes_this_round > chunk_bytes) {
      bytes_this_round = chunk_bytes;
    }

    int32_t err = ili9342_write_data(surface->panel, buffer + offset, bytes_this_round);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }

    offset += bytes_this_round;
  }

  return ILI9342_ERR_NONE;
}

uint16_t graphics_blend_rgb565(uint16_t background_color,
                               uint16_t foreground_color,
                               uint8_t coverage) {
  uint32_t bg_r = (background_color >> 11) & 0x1F;
  uint32_t bg_g = (background_color >> 5) & 0x3F;
  uint32_t bg_b = background_color & 0x1F;

  uint32_t fg_r = (foreground_color >> 11) & 0x1F;
  uint32_t fg_g = (foreground_color >> 5) & 0x3F;
  uint32_t fg_b = foreground_color & 0x1F;

  uint32_t inv = 255U - coverage;
  uint32_t out_r = (fg_r * coverage + bg_r * inv + 127U) / 255U;
  uint32_t out_g = (fg_g * coverage + bg_g * inv + 127U) / 255U;
  uint32_t out_b = (fg_b * coverage + bg_b * inv + 127U) / 255U;

  return (uint16_t)((out_r << 11) | (out_g << 5) | out_b);
}

int32_t graphics_mask_coverage_get(graphics_bitmap_mask_format_t format,
                                   const uint8_t *data,
                                   uint16_t stride_bytes,
                                   uint16_t width,
                                   uint16_t height,
                                   int src_x,
                                   int src_y,
                                   uint8_t *coverage) {
  if (data == NULL || coverage == NULL || width == 0U || height == 0U || stride_bytes == 0U ||
      src_x < 0 || src_y < 0 || src_x >= width || src_y >= height) {
    return ILI9342_ERR_INVALID_ARG;
  }

  if (format == GRAPHICS_BITMAP_MASK_1BPP) {
    int byte_idx = (src_y * (int)stride_bytes) + (src_x / 8);
    int bit_idx = 7 - (src_x % 8);
    uint8_t byte = data[byte_idx];
    *coverage = ((byte >> bit_idx) & 1U) != 0U ? 255U : 0U;
    return ILI9342_ERR_NONE;
  }

  if (format == GRAPHICS_BITMAP_MASK_4BPP) {
    int byte_idx = (src_y * (int)stride_bytes) + (src_x / 2);
    uint8_t byte = data[byte_idx];
    uint8_t gray4 = (src_x % 2 == 0) ? (byte >> 4) : (byte & 0x0F);
    *coverage = (uint8_t)((gray4 << 4) | gray4);
    return ILI9342_ERR_NONE;
  }

  if (format == GRAPHICS_BITMAP_MASK_8BPP) {
    *coverage = data[(src_y * (int)stride_bytes) + src_x];
    return ILI9342_ERR_NONE;
  }

  return ILI9342_ERR_INVALID_ARG;
}
