#pragma once

#include <stdint.h>

#include "bmf_reader.h"
#include "display_surface.h"

int16_t graphics_text_first_baseline_y(const bmf_font_view_t *font, const graphics_rect_t *bounding);

int32_t graphics_draw_text_bounded(display_surface_t *surface,
                                   bmf_font_view_t *font,
                                   const char *text,
                                   int16_t x,
                                   int16_t y,
                                   const graphics_rect_t *bounding,
                                   uint16_t foreground_color,
                                   uint16_t background_color,
                                   int16_t *next_x,
                                   int16_t *next_y);

int32_t graphics_draw_char_bounded(display_surface_t *surface,
                                   bmf_font_view_t *font,
                                   char c,
                                   int16_t x,
                                   int16_t y,
                                   const graphics_rect_t *bounding,
                                   uint16_t foreground_color,
                                   uint16_t background_color,
                                   int16_t *next_x,
                                   int16_t *next_y);
