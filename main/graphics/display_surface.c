#include "graphics/display_surface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graphics/render_support.h"

static uint8_t n_to_m_bits(uint8_t data, uint8_t n, uint8_t m) {
  return (uint8_t)((data * ((1U << m) - 1U)) / ((1U << n) - 1U));
}

int32_t display_surface_init(display_surface_t *surface,
                             ili9342_t *panel,
                             uint16_t width,
                             uint16_t height,
                             size_t max_transfer_bytes) {
  if (surface == NULL || panel == NULL || width == 0U || height == 0U) {
    return ILI9342_ERR_INVALID_ARG;
  }

  memset(surface, 0, sizeof(*surface));
  surface->panel = panel;
  surface->width = width;
  surface->height = height;
  surface->max_transfer_bytes = max_transfer_bytes;
  surface->row_buffer_bytes = (size_t)width * 2U;
  surface->row_buffer = malloc(surface->row_buffer_bytes);
  if (surface->row_buffer == NULL) {
    memset(surface, 0, sizeof(*surface));
    return ILI9342_ERR_NO_MEM;
  }

  return ILI9342_ERR_NONE;
}

void display_surface_deinit(display_surface_t *surface) {
  if (surface == NULL) {
    return;
  }

  free(surface->row_buffer);
  memset(surface, 0, sizeof(*surface));
}

bool graphics_rect_is_valid(const display_surface_t *surface, const graphics_rect_t *rect) {
  if (surface == NULL || rect == NULL) {
    return false;
  }

  return rect->x0 >= 0 && rect->y0 >= 0 && rect->x1 >= rect->x0 && rect->y1 >= rect->y0 &&
         rect->x1 < (int16_t)surface->width && rect->y1 < (int16_t)surface->height;
}

bool graphics_rect_clip_to_bounds(const graphics_rect_t *bounds,
                                  int32_t x0,
                                  int32_t y0,
                                  int32_t x1,
                                  int32_t y1,
                                  graphics_rect_t *clipped) {
  if (bounds == NULL || clipped == NULL || x1 < x0 || y1 < y0) {
    return false;
  }

  if (x0 > bounds->x1 || x1 < bounds->x0 || y0 > bounds->y1 || y1 < bounds->y0) {
    return false;
  }

  clipped->x0 = (int16_t)(x0 < bounds->x0 ? bounds->x0 : x0);
  clipped->y0 = (int16_t)(y0 < bounds->y0 ? bounds->y0 : y0);
  clipped->x1 = (int16_t)(x1 > bounds->x1 ? bounds->x1 : x1);
  clipped->y1 = (int16_t)(y1 > bounds->y1 ? bounds->y1 : y1);
  return true;
}

void graphics_rect_include(graphics_rect_t *dst, const graphics_rect_t *src) {
  if (dst == NULL || src == NULL) {
    return;
  }

  if (src->x0 < dst->x0) {
    dst->x0 = src->x0;
  }
  if (src->y0 < dst->y0) {
    dst->y0 = src->y0;
  }
  if (src->x1 > dst->x1) {
    dst->x1 = src->x1;
  }
  if (src->y1 > dst->y1) {
    dst->y1 = src->y1;
  }
}

bool graphics_rect_contains_point(const graphics_rect_t *rect, uint16_t x, uint16_t y) {
  if (rect == NULL) {
    return false;
  }

  return x >= (uint16_t)rect->x0 && x <= (uint16_t)rect->x1 && y >= (uint16_t)rect->y0 &&
         y <= (uint16_t)rect->y1;
}

uint16_t graphics_rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
  uint8_t r5 = n_to_m_bits(r, 8, 5);
  uint8_t g6 = n_to_m_bits(g, 8, 6);
  uint8_t b5 = n_to_m_bits(b, 8, 5);

  return (uint16_t)((uint16_t)(r5 << 11) | (uint16_t)(g6 << 5) | (uint16_t)b5);
}

int32_t graphics_fill_rect(display_surface_t *surface,
                           uint16_t x0,
                           uint16_t y0,
                           uint16_t x1,
                           uint16_t y1,
                           uint16_t color) {
  if (surface == NULL || surface->panel == NULL || surface->row_buffer == NULL || x1 < x0 ||
      y1 < y0 || x1 >= surface->width || y1 >= surface->height) {
    return ILI9342_ERR_INVALID_ARG;
  }

  size_t row_pixels = (size_t)(x1 - x0 + 1U);
  size_t row_bytes = row_pixels * 2U;
  if (row_bytes == 0U || row_bytes > surface->row_buffer_bytes) {
    return ILI9342_ERR_INVALID_ARG;
  }

  graphics_fill_color_span(surface->row_buffer, row_pixels, color);

  int32_t err = ili9342_address_window_set(surface->panel, x0, y0, x1, y1);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  size_t row_count = (size_t)(y1 - y0 + 1U);
  for (size_t row = 0; row < row_count; row++) {
    err = graphics_write_buffer_chunked(surface, surface->row_buffer, row_bytes);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }
  }

  return ILI9342_ERR_NONE;
}

int32_t graphics_fill_screen(display_surface_t *surface, uint16_t color) {
  if (surface == NULL || surface->width == 0U || surface->height == 0U) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t err =
      graphics_fill_rect(surface, 0U, 0U, surface->width - 1U, surface->height - 1U, color);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  puts("LCD display memory fill complete");
  return ILI9342_ERR_NONE;
}

int32_t graphics_fill_round_rect_r6_top(display_surface_t *surface,
                                        const graphics_rect_t *bounds,
                                        uint16_t color) {
  static const uint8_t inset[6] = {4, 3, 2, 1, 1, 0};

  if (!graphics_rect_is_valid(surface, bounds)) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t err = graphics_fill_rect(
      surface, (uint16_t)bounds->x0, (uint16_t)(bounds->y0 + 6), (uint16_t)bounds->x1,
      (uint16_t)bounds->y1, color);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  for (uint16_t dy = 0; dy < 6U; ++dy) {
    uint16_t dx = inset[dy];
    err = graphics_fill_rect(surface,
                             (uint16_t)(bounds->x0 + dx),
                             (uint16_t)(bounds->y0 + (int16_t)dy),
                             (uint16_t)(bounds->x1 - (int16_t)dx),
                             (uint16_t)(bounds->y0 + (int16_t)dy),
                             color);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }
  }

  return ILI9342_ERR_NONE;
}

int32_t graphics_fill_round_rect_r6(display_surface_t *surface,
                                    const graphics_rect_t *bounds,
                                    uint16_t color) {
  static const uint8_t inset[6] = {3, 2, 1, 1, 0, 0};

  if (!graphics_rect_is_valid(surface, bounds)) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t err = graphics_fill_rect(surface,
                                   (uint16_t)bounds->x0,
                                   (uint16_t)(bounds->y0 + 6),
                                   (uint16_t)bounds->x1,
                                   (uint16_t)(bounds->y1 - 6),
                                   color);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  for (uint16_t dy = 0; dy < 6U; ++dy) {
    uint16_t dx = inset[dy];
    err = graphics_fill_rect(surface,
                             (uint16_t)(bounds->x0 + dx),
                             (uint16_t)(bounds->y0 + (int16_t)dy),
                             (uint16_t)(bounds->x1 - (int16_t)dx),
                             (uint16_t)(bounds->y0 + (int16_t)dy),
                             color);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }

    err = graphics_fill_rect(surface,
                             (uint16_t)(bounds->x0 + dx),
                             (uint16_t)(bounds->y1 - (int16_t)dy),
                             (uint16_t)(bounds->x1 - (int16_t)dx),
                             (uint16_t)(bounds->y1 - (int16_t)dy),
                             color);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }
  }

  return ILI9342_ERR_NONE;
}
