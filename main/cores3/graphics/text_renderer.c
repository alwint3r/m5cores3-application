#include "text_renderer.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const uint8_t *bitmap;
  int draw_x;
  int draw_y;
  int draw_y1;
  int width;
  int height;
  int stride;
} prepared_glyph_t;

static inline void graphics_color_to_u8_array(uint8_t *dst, uint16_t color) {
  dst[0] = (uint8_t)(color >> 8);
  dst[1] = (uint8_t)(color & 0xFF);
}

static void graphics_fill_color_span(uint8_t *buffer, size_t pixel_count, uint16_t color) {
  for (size_t i = 0; i < pixel_count; i++) {
    graphics_color_to_u8_array(buffer + (i * 2U), color);
  }
}

static int32_t graphics_write_buffer_chunked(display_surface_t *surface,
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

static uint16_t blend_rgb565(uint16_t bg, uint16_t fg, uint8_t coverage) {
  uint32_t bg_r = (bg >> 11) & 0x1F;
  uint32_t bg_g = (bg >> 5) & 0x3F;
  uint32_t bg_b = bg & 0x1F;

  uint32_t fg_r = (fg >> 11) & 0x1F;
  uint32_t fg_g = (fg >> 5) & 0x3F;
  uint32_t fg_b = fg & 0x1F;

  uint32_t inv = 255U - coverage;
  uint32_t out_r = (fg_r * coverage + bg_r * inv + 127U) / 255U;
  uint32_t out_g = (fg_g * coverage + bg_g * inv + 127U) / 255U;
  uint32_t out_b = (fg_b * coverage + bg_b * inv + 127U) / 255U;

  return (uint16_t)((out_r << 11) | (out_g << 5) | out_b);
}

static int32_t graphics_glyph_stride_get(const bmf_font_view_t *font_view, int width, int *stride) {
  if (font_view == NULL || stride == NULL || width <= 0) {
    return ILI9342_ERR_INVALID_ARG;
  }

  if (font_view->bpp == BMF_BPP_MONO) {
    *stride = (width + 7) / 8;
    return ILI9342_ERR_NONE;
  }

  if (font_view->bpp == BMF_BPP_GRAY4) {
    *stride = (width + 1) / 2;
    return ILI9342_ERR_NONE;
  }

  if (font_view->bpp == BMF_BPP_GRAY8) {
    *stride = width;
    return ILI9342_ERR_NONE;
  }

  return ILI9342_ERR_INVALID_ARG;
}

static int32_t prepared_glyph_load(bmf_font_view_t *font_view,
                                   const bmf_glyph_record_t *glyph,
                                   prepared_glyph_t *prepared) {
  if (font_view == NULL || glyph == NULL || prepared == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  memset(prepared, 0, sizeof(*prepared));

  int width = 0;
  int height = 0;
  prepared->bitmap = bmf_font_view_get_glyph_bitmap(font_view, glyph, &width, &height);
  prepared->width = width;
  prepared->height = height;
  if (prepared->bitmap == NULL || width <= 0 || height <= 0) {
    return ILI9342_ERR_NONE;
  }

  return graphics_glyph_stride_get(font_view, width, &prepared->stride);
}

static void prepared_glyph_position_set(prepared_glyph_t *prepared, int draw_x, int draw_y) {
  prepared->draw_x = draw_x;
  prepared->draw_y = draw_y;
  prepared->draw_y1 = draw_y + prepared->height - 1;
}

static bool prepared_glyph_has_bitmap(const prepared_glyph_t *prepared) {
  return prepared != NULL && prepared->bitmap != NULL && prepared->width > 0 &&
         prepared->height > 0;
}

static int32_t prepared_glyph_coverage_get(const bmf_font_view_t *font_view,
                                           const prepared_glyph_t *prepared,
                                           int src_x,
                                           int src_y,
                                           uint8_t *coverage) {
  if (font_view == NULL || prepared == NULL || coverage == NULL ||
      !prepared_glyph_has_bitmap(prepared) || src_x < 0 || src_y < 0 || src_x >= prepared->width ||
      src_y >= prepared->height) {
    return ILI9342_ERR_INVALID_ARG;
  }

  if (font_view->bpp == BMF_BPP_MONO) {
    int byte_idx = src_y * prepared->stride + src_x / 8;
    int bit_idx = 7 - (src_x % 8);
    uint8_t byte = prepared->bitmap[byte_idx];
    *coverage = ((byte >> bit_idx) & 1U) != 0U ? 255U : 0U;
    return ILI9342_ERR_NONE;
  }

  if (font_view->bpp == BMF_BPP_GRAY4) {
    int byte_idx = src_y * prepared->stride + src_x / 2;
    uint8_t byte = prepared->bitmap[byte_idx];
    uint8_t gray4 = (src_x % 2 == 0) ? (byte >> 4) : (byte & 0x0F);
    *coverage = (uint8_t)((gray4 << 4) | gray4);
    return ILI9342_ERR_NONE;
  }

  if (font_view->bpp == BMF_BPP_GRAY8) {
    *coverage = prepared->bitmap[src_y * prepared->stride + src_x];
    return ILI9342_ERR_NONE;
  }

  return ILI9342_ERR_INVALID_ARG;
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

  return blend_rgb565(background_color, foreground_color, coverage);
}

static int32_t render_prepared_glyph_row(uint8_t *row_buffer,
                                         int row_origin_x,
                                         int dst_y,
                                         const bmf_font_view_t *font_view,
                                         const prepared_glyph_t *prepared,
                                         int clip_x0,
                                         int clip_x1,
                                         uint16_t background_color,
                                         uint16_t foreground_color) {
  if (row_buffer == NULL || font_view == NULL || prepared == NULL || clip_x1 < clip_x0) {
    return ILI9342_ERR_INVALID_ARG;
  }

  if (dst_y < prepared->draw_y || dst_y > prepared->draw_y1) {
    return ILI9342_ERR_NONE;
  }

  int src_y = dst_y - prepared->draw_y;
  for (int dst_x = clip_x0; dst_x <= clip_x1; dst_x++) {
    int src_x = dst_x - prepared->draw_x;
    uint8_t coverage = 0U;
    int32_t err = prepared_glyph_coverage_get(font_view, prepared, src_x, src_y, &coverage);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }

    if (coverage == 0U) {
      continue;
    }

    size_t row_offset = (size_t)(dst_x - row_origin_x) * 2U;
    graphics_color_to_u8_array(row_buffer + row_offset,
                               color_from_coverage(background_color, foreground_color, coverage));
  }

  return ILI9342_ERR_NONE;
}

static int32_t render_prepared_glyphs(display_surface_t *surface,
                                      const bmf_font_view_t *font_view,
                                      const prepared_glyph_t *prepared_glyphs,
                                      size_t prepared_glyph_count,
                                      const graphics_rect_t *clip,
                                      uint16_t background_color,
                                      uint16_t foreground_color) {
  if (surface == NULL || font_view == NULL || prepared_glyphs == NULL || clip == NULL ||
      surface->row_buffer == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  if (!graphics_rect_is_valid(surface, clip)) {
    return ILI9342_ERR_INVALID_ARG;
  }

  size_t row_pixels = (size_t)(clip->x1 - clip->x0 + 1);
  size_t row_bytes = row_pixels * 2U;
  if (row_bytes == 0U || row_bytes > surface->row_buffer_bytes) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t err = ili9342_address_window_set(surface->panel,
                                           (uint16_t)clip->x0,
                                           (uint16_t)clip->y0,
                                           (uint16_t)clip->x1,
                                           (uint16_t)clip->y1);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  for (int dst_y = clip->y0; dst_y <= clip->y1; dst_y++) {
    graphics_fill_color_span(surface->row_buffer, row_pixels, background_color);

    for (size_t i = 0; i < prepared_glyph_count; i++) {
      const prepared_glyph_t *prepared = &prepared_glyphs[i];
      if (!prepared_glyph_has_bitmap(prepared) || dst_y < prepared->draw_y ||
          dst_y > prepared->draw_y1) {
        continue;
      }

      int clip_x0 = prepared->draw_x < clip->x0 ? clip->x0 : prepared->draw_x;
      int clip_x1 = prepared->draw_x + prepared->width - 1;
      if (clip_x1 > clip->x1) {
        clip_x1 = clip->x1;
      }

      if (clip_x0 > clip_x1) {
        continue;
      }

      err = render_prepared_glyph_row(surface->row_buffer,
                                      clip->x0,
                                      dst_y,
                                      font_view,
                                      prepared,
                                      clip_x0,
                                      clip_x1,
                                      background_color,
                                      foreground_color);
      if (err != ILI9342_ERR_NONE) {
        return err;
      }
    }

    err = graphics_write_buffer_chunked(surface, surface->row_buffer, row_bytes);
    if (err != ILI9342_ERR_NONE) {
      return err;
    }
  }

  return ILI9342_ERR_NONE;
}

static int32_t flush_prepared_glyph_run(display_surface_t *surface,
                                        bmf_font_view_t *font_view,
                                        const prepared_glyph_t *prepared_glyphs,
                                        size_t prepared_glyph_count,
                                        bool has_visible_pixels,
                                        const graphics_rect_t *clip,
                                        uint16_t background_color,
                                        uint16_t foreground_color) {
  if (prepared_glyph_count == 0U || !has_visible_pixels) {
    return ILI9342_ERR_NONE;
  }

  return render_prepared_glyphs(surface,
                                font_view,
                                prepared_glyphs,
                                prepared_glyph_count,
                                clip,
                                background_color,
                                foreground_color);
}

int16_t graphics_text_first_baseline_y(const bmf_font_view_t *font,
                                       const graphics_rect_t *bounding) {
  int16_t first_line_y = bounding->y0;
  if (font->ascent > 0) {
    first_line_y = (int16_t)(bounding->y0 + font->ascent);
  } else if (font->line_height > 0U) {
    first_line_y = (int16_t)(bounding->y0 + (int16_t)font->line_height - 1);
  }

  if (first_line_y > bounding->y1) {
    first_line_y = bounding->y1;
  }

  return first_line_y;
}

static int32_t clear_text_line(display_surface_t *surface,
                               const bmf_font_view_t *font,
                               const graphics_rect_t *bounding,
                               int16_t baseline_y,
                               uint16_t background_color) {
  if (surface == NULL || font == NULL || bounding == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  if (!graphics_rect_is_valid(surface, bounding)) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int32_t line_y0 = 0;
  if (font->ascent > 0) {
    line_y0 = (int32_t)baseline_y - font->ascent;
  } else if (font->line_height > 0U) {
    line_y0 = (int32_t)baseline_y - (int32_t)font->line_height + 1;
  } else {
    line_y0 = baseline_y;
  }

  int32_t line_y1 = line_y0;
  if (font->line_height > 0U) {
    line_y1 = line_y0 + (int32_t)font->line_height - 1;
  }

  if (line_y1 < bounding->y0 || line_y0 > bounding->y1) {
    return ILI9342_ERR_NONE;
  }

  if (line_y0 < bounding->y0) {
    line_y0 = bounding->y0;
  }
  if (line_y1 > bounding->y1) {
    line_y1 = bounding->y1;
  }
  if (line_y0 > line_y1) {
    return ILI9342_ERR_NONE;
  }

  return graphics_fill_rect(surface,
                            (uint16_t)bounding->x0,
                            (uint16_t)line_y0,
                            (uint16_t)bounding->x1,
                            (uint16_t)line_y1,
                            background_color);
}

static int32_t wrap_pen_to_next_line(display_surface_t *surface,
                                     const bmf_font_view_t *font,
                                     const graphics_rect_t *bounding,
                                     int16_t first_line_y,
                                     uint16_t background_color,
                                     int16_t *pen_x,
                                     int16_t *pen_y) {
  if (surface == NULL || font == NULL || bounding == NULL || pen_x == NULL || pen_y == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  *pen_x = bounding->x0;
  *pen_y = (int16_t)(*pen_y + (int16_t)font->line_height);
  if (*pen_y > bounding->y1) {
    *pen_y = first_line_y;
  }

  return clear_text_line(surface, font, bounding, *pen_y, background_color);
}

static int32_t reset_pen_to_first_line(display_surface_t *surface,
                                       const bmf_font_view_t *font,
                                       const graphics_rect_t *bounding,
                                       int16_t first_line_y,
                                       uint16_t background_color,
                                       int16_t *pen_x,
                                       int16_t *pen_y) {
  if (surface == NULL || font == NULL || bounding == NULL || pen_x == NULL || pen_y == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  *pen_x = bounding->x0;
  *pen_y = first_line_y;

  return clear_text_line(surface, font, bounding, *pen_y, background_color);
}

static int32_t draw_char_array_bounded(display_surface_t *surface,
                                       bmf_font_view_t *font,
                                       const char *text,
                                       size_t text_length,
                                       int16_t x,
                                       int16_t y,
                                       const graphics_rect_t *bounding,
                                       uint16_t foreground_color,
                                       uint16_t background_color,
                                       int16_t *next_x,
                                       int16_t *next_y) {
  if (surface == NULL || font == NULL || text == NULL || bounding == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  if (!graphics_rect_is_valid(surface, bounding)) {
    return ILI9342_ERR_INVALID_ARG;
  }

  int16_t first_line_y = graphics_text_first_baseline_y(font, bounding);

  int16_t pen_x = x;
  int16_t pen_y = y;
  if (pen_x < bounding->x0 || pen_x > bounding->x1) {
    pen_x = bounding->x0;
  }
  if (pen_y < first_line_y || pen_y > bounding->y1) {
    pen_y = first_line_y;
  }

  if (text_length == 0U) {
    if (next_x != NULL) {
      *next_x = pen_x;
    }
    if (next_y != NULL) {
      *next_y = pen_y;
    }

    return ILI9342_ERR_NONE;
  }

  prepared_glyph_t *prepared_glyphs = calloc(text_length, sizeof(*prepared_glyphs));
  if (prepared_glyphs == NULL) {
    return ILI9342_ERR_NO_MEM;
  }

  size_t prepared_count = 0U;
  bool has_visible_pixels = false;
  graphics_rect_t line_clip = {0};

  for (size_t i = 0; i < text_length; i++) {
    char c = text[i];

    if (c == '\n') {
      int32_t err = flush_prepared_glyph_run(surface,
                                             font,
                                             prepared_glyphs,
                                             prepared_count,
                                             has_visible_pixels,
                                             &line_clip,
                                             background_color,
                                             foreground_color);
      if (err != ILI9342_ERR_NONE) {
        free(prepared_glyphs);
        return err;
      }

      prepared_count = 0U;
      has_visible_pixels = false;

      err = wrap_pen_to_next_line(
          surface, font, bounding, first_line_y, background_color, &pen_x, &pen_y);
      if (err != ILI9342_ERR_NONE) {
        free(prepared_glyphs);
        return err;
      }

      continue;
    }

    uint32_t codepoint = (uint8_t)c;
    bmf_glyph_record_t glyph;
    bmf_status_t bmf_ret = bmf_font_view_find_glyph_binary(font, codepoint, &glyph, NULL);
    if (bmf_ret != BMF_STATUS_OK) {
      free(prepared_glyphs);
      return ILI9342_ERR_INVALID_ARG;
    }

    prepared_glyph_t prepared;
    int32_t err = prepared_glyph_load(font, &glyph, &prepared);
    if (err != ILI9342_ERR_NONE) {
      free(prepared_glyphs);
      return err;
    }

    if (prepared_glyph_has_bitmap(&prepared)) {
      prepared_glyph_position_set(
          &prepared, (int32_t)pen_x + glyph.x_offset, (int32_t)pen_y + glyph.y_offset);

      if (prepared.draw_x + prepared.width - 1 > bounding->x1 && pen_x > bounding->x0) {
        err = flush_prepared_glyph_run(surface,
                                       font,
                                       prepared_glyphs,
                                       prepared_count,
                                       has_visible_pixels,
                                       &line_clip,
                                       background_color,
                                       foreground_color);
        if (err != ILI9342_ERR_NONE) {
          free(prepared_glyphs);
          return err;
        }

        prepared_count = 0U;
        has_visible_pixels = false;

        err = wrap_pen_to_next_line(
            surface, font, bounding, first_line_y, background_color, &pen_x, &pen_y);
        if (err != ILI9342_ERR_NONE) {
          free(prepared_glyphs);
          return err;
        }

        prepared_glyph_position_set(
            &prepared, (int32_t)pen_x + glyph.x_offset, (int32_t)pen_y + glyph.y_offset);
      }

      if (prepared.draw_y1 > bounding->y1) {
        err = flush_prepared_glyph_run(surface,
                                       font,
                                       prepared_glyphs,
                                       prepared_count,
                                       has_visible_pixels,
                                       &line_clip,
                                       background_color,
                                       foreground_color);
        if (err != ILI9342_ERR_NONE) {
          free(prepared_glyphs);
          return err;
        }

        prepared_count = 0U;
        has_visible_pixels = false;

        err = reset_pen_to_first_line(
            surface, font, bounding, first_line_y, background_color, &pen_x, &pen_y);
        if (err != ILI9342_ERR_NONE) {
          free(prepared_glyphs);
          return err;
        }

        prepared_glyph_position_set(
            &prepared, (int32_t)pen_x + glyph.x_offset, (int32_t)pen_y + glyph.y_offset);
      }

      graphics_rect_t glyph_clip;
      if (graphics_rect_clip_to_bounds(bounding,
                                       prepared.draw_x,
                                       prepared.draw_y,
                                       prepared.draw_x + prepared.width - 1,
                                       prepared.draw_y1,
                                       &glyph_clip)) {
        prepared_glyphs[prepared_count++] = prepared;

        if (!has_visible_pixels) {
          line_clip = glyph_clip;
          has_visible_pixels = true;
        } else {
          graphics_rect_include(&line_clip, &glyph_clip);
        }
      }
    }

    pen_x = (int16_t)((int32_t)pen_x + glyph.x_advance);
    if (pen_x > bounding->x1) {
      err = flush_prepared_glyph_run(surface,
                                     font,
                                     prepared_glyphs,
                                     prepared_count,
                                     has_visible_pixels,
                                     &line_clip,
                                     background_color,
                                     foreground_color);
      if (err != ILI9342_ERR_NONE) {
        free(prepared_glyphs);
        return err;
      }

      prepared_count = 0U;
      has_visible_pixels = false;

      err = wrap_pen_to_next_line(
          surface, font, bounding, first_line_y, background_color, &pen_x, &pen_y);
      if (err != ILI9342_ERR_NONE) {
        free(prepared_glyphs);
        return err;
      }
    }
  }

  int32_t err = flush_prepared_glyph_run(surface,
                                         font,
                                         prepared_glyphs,
                                         prepared_count,
                                         has_visible_pixels,
                                         &line_clip,
                                         background_color,
                                         foreground_color);
  free(prepared_glyphs);
  if (err != ILI9342_ERR_NONE) {
    return err;
  }

  if (next_x != NULL) {
    *next_x = pen_x;
  }
  if (next_y != NULL) {
    *next_y = pen_y;
  }
  return ILI9342_ERR_NONE;
}

int32_t graphics_draw_text_bounded(display_surface_t *surface,
                                   bmf_font_view_t *font,
                                   const char *text,
                                   int16_t x,
                                   int16_t y,
                                   const graphics_rect_t *bounding,
                                   uint16_t foreground_color,
                                   uint16_t background_color,
                                   int16_t *next_x,
                                   int16_t *next_y) {
  if (text == NULL) {
    return ILI9342_ERR_INVALID_ARG;
  }

  return draw_char_array_bounded(surface,
                                 font,
                                 text,
                                 strlen(text),
                                 x,
                                 y,
                                 bounding,
                                 foreground_color,
                                 background_color,
                                 next_x,
                                 next_y);
}

int32_t graphics_draw_char_bounded(display_surface_t *surface,
                                   bmf_font_view_t *font,
                                   char c,
                                   int16_t x,
                                   int16_t y,
                                   const graphics_rect_t *bounding,
                                   uint16_t foreground_color,
                                   uint16_t background_color,
                                   int16_t *next_x,
                                   int16_t *next_y) {
  return draw_char_array_bounded(
      surface, font, &c, 1U, x, y, bounding, foreground_color, background_color, next_x, next_y);
}
