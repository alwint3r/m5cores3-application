#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ili9342/ili9342.h>

typedef struct {
  int16_t x0;
  int16_t y0;
  int16_t x1;
  int16_t y1;
} graphics_rect_t;

typedef struct {
  ili9342_t *panel;
  uint16_t width;
  uint16_t height;
  size_t max_transfer_bytes;
  uint8_t *row_buffer;
  size_t row_buffer_bytes;
  TaskHandle_t owner_task;
} display_surface_t;

int32_t display_surface_init(display_surface_t *surface,
                             ili9342_t *panel,
                             uint16_t width,
                             uint16_t height,
                             size_t max_transfer_bytes);
void display_surface_deinit(display_surface_t *surface);
TaskHandle_t display_surface_owner_task_get(const display_surface_t *surface);
int32_t display_surface_require_owner_task(const display_surface_t *surface);

bool graphics_rect_is_valid(const display_surface_t *surface, const graphics_rect_t *rect);
bool graphics_rect_clip_to_bounds(const graphics_rect_t *bounds,
                                  int32_t x0,
                                  int32_t y0,
                                  int32_t x1,
                                  int32_t y1,
                                  graphics_rect_t *clipped);
void graphics_rect_include(graphics_rect_t *dst, const graphics_rect_t *src);
bool graphics_rect_contains_point(const graphics_rect_t *rect, int16_t x, int16_t y);

uint16_t graphics_rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b);

int32_t graphics_fill_rect(display_surface_t *surface,
                           uint16_t x0,
                           uint16_t y0,
                           uint16_t x1,
                           uint16_t y1,
                           uint16_t color);
int32_t graphics_fill_rect_clipped(display_surface_t *surface,
                                   int32_t x0,
                                   int32_t y0,
                                   int32_t x1,
                                   int32_t y1,
                                   uint16_t color);
int32_t graphics_fill_rect_from_bounds(display_surface_t *surface,
                                       const graphics_rect_t *bounds,
                                       uint16_t color);
int32_t graphics_fill_screen(display_surface_t *surface, uint16_t color);
int32_t graphics_fill_round_rect_r6(display_surface_t *surface,
                                    const graphics_rect_t *bounds,
                                    uint16_t color);
int32_t graphics_fill_round_rect_r6_top(display_surface_t *surface,
                                        const graphics_rect_t *bounds,
                                        uint16_t color);
