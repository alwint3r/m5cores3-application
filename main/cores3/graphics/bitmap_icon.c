#include "bitmap_icon.h"

#include <stdbool.h>

#include "render_support.h"

static uint16_t minimum_stride_bytes(uint16_t width, graphics_bitmap_mask_format_t format) {
  if (format == GRAPHICS_BITMAP_MASK_1BPP) {
    return (uint16_t)((width + 7U) / 8U);
  }

  if (format == GRAPHICS_BITMAP_MASK_4BPP) {
    return (uint16_t)((width + 1U) / 2U);
  }

  if (format == GRAPHICS_BITMAP_MASK_8BPP) {
    return width;
  }

  return 0U;
}

static bool graphics_bitmap_icon_is_valid(const graphics_bitmap_icon_t *icon) {
  if (icon == NULL || icon->data == NULL || icon->width == 0U || icon->height == 0U) {
    return false;
  }

  uint16_t min_stride = minimum_stride_bytes(icon->width, icon->format);
  return min_stride != 0U && icon->stride_bytes >= min_stride;
}

static uint16_t color_from_coverage(uint16_t background_color,
                                    uint16_t foreground_color,
                                    uint8_t coverage) {
  if (coverage == 0U) {
    return background_color;
  }

  if (coverage == 255U) {
    return foreground_color;
  }

  return graphics_blend_rgb565(background_color, foreground_color, coverage);
}

int32_t graphics_bitmap_icon_center_position(const graphics_bitmap_icon_t *icon,
                                             const graphics_rect_t *bounding,
                                             int16_t *x,
                                             int16_t *y) {
  if (!graphics_bitmap_icon_is_valid(icon) || bounding == NULL || x == NULL || y == NULL ||
      bounding->x1 < bounding->x0 || bounding->y1 < bounding->y0) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t bounding_width = (int32_t)bounding->x1 - (int32_t)bounding->x0 + 1;
  int32_t bounding_height = (int32_t)bounding->y1 - (int32_t)bounding->y0 + 1;

  int32_t offset_x = 0;
  if (bounding_width > icon->width) {
    offset_x = (bounding_width - (int32_t)icon->width) / 2;
  }

  int32_t offset_y = 0;
  if (bounding_height > icon->height) {
    offset_y = (bounding_height - (int32_t)icon->height) / 2;
  }

  *x = (int16_t)(bounding->x0 + offset_x);
  *y = (int16_t)(bounding->y0 + offset_y);
  return ILI9342_ERR_NONE;
}

int32_t graphics_draw_bitmap_icon(display_surface_t *surface,
                                  const graphics_bitmap_icon_t *icon,
                                  int16_t x,
                                  int16_t y,
                                  const graphics_rect_t *clip,
                                  uint16_t foreground_color,
                                  uint16_t background_color) {
  int32_t err = display_surface_require_owner_task(surface);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  if (surface->panel == NULL || surface->row_buffer == NULL ||
      !graphics_bitmap_icon_is_valid(icon) || surface->width == 0U || surface->height == 0U) {
    return ILI9342_ERR_INVALID_ARG;
  }

  graphics_rect_t surface_bounds = {
      .x0 = 0,
      .y0 = 0,
      .x1 = (int16_t)(surface->width - 1U),
      .y1 = (int16_t)(surface->height - 1U),
  };

  const graphics_rect_t *clip_bounds = &surface_bounds;
  if (clip != NULL) {
    if (!graphics_rect_is_valid(surface, clip)) {
      return ILI9342_ERR_INVALID_ARG;
    }
    clip_bounds = clip;
  }

  int32_t icon_x0 = x;
  int32_t icon_y0 = y;
  int32_t icon_x1 = icon_x0 + (int32_t)icon->width - 1;
  int32_t icon_y1 = icon_y0 + (int32_t)icon->height - 1;

  graphics_rect_t visible = {0};
  if (!graphics_rect_clip_to_bounds(clip_bounds, icon_x0, icon_y0, icon_x1, icon_y1, &visible)) {
    return ILI9342_ERR_NONE;
  }

  size_t row_pixels = (size_t)(visible.x1 - visible.x0 + 1);
  size_t row_bytes = row_pixels * 2U;
  if (row_bytes == 0U || row_bytes > surface->row_buffer_bytes) {
    return ILI9342_ERR_INVALID_ARG;
  }

  err = ili9342_address_window_set(surface->panel,
                                   (uint16_t)visible.x0,
                                   (uint16_t)visible.y0,
                                   (uint16_t)visible.x1,
                                   (uint16_t)visible.y1);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  for (int dst_y = visible.y0; dst_y <= visible.y1; dst_y++) {
    graphics_fill_color_span(surface->row_buffer, row_pixels, background_color);

    int src_y = dst_y - y;
    for (int dst_x = visible.x0; dst_x <= visible.x1; dst_x++) {
      int src_x = dst_x - x;
      uint8_t coverage = 0U;
      err = graphics_mask_coverage_get(icon->format,
                                       icon->data,
                                       icon->stride_bytes,
                                       icon->width,
                                       icon->height,
                                       src_x,
                                       src_y,
                                       &coverage);
      if (err != ILI9342_ERR_NONE) {
        return err;
      }

      if (coverage == 0U) {
        continue;
      }

      size_t row_offset = (size_t)(dst_x - visible.x0) * 2U;
      graphics_store_rgb565(surface->row_buffer + row_offset,
                            color_from_coverage(background_color, foreground_color, coverage));
    }

    err = graphics_write_buffer_chunked(surface, surface->row_buffer, row_bytes);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }
  }

  return ILI9342_ERR_NONE;
}
