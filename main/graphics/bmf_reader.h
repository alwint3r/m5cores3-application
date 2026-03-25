#ifndef BMF_READER_H
#define BMF_READER_H

#include <stddef.h>
#include <stdint.h>

#define BMF_MAGIC "BMF1"
#define BMF_VERSION 1

/* Supported bits per pixel values:
 * - BMF_BPP_MONO (1): 1-bit monochrome, 8 pixels per byte, MSB-first
 * - BMF_BPP_GRAY4 (4): 4-bit grayscale, 2 pixels per byte, high nibble first
 * - BMF_BPP_GRAY8 (8): 8-bit grayscale, 1 byte per pixel
 */
#define BMF_BPP_MONO 1
#define BMF_BPP_GRAY4 4
#define BMF_BPP_GRAY8 8

#define GLYPH_RECORD_SIZE 16
#define HEADER_SIZE 30

typedef enum {
  BMF_STATUS_OK = 0,
  BMF_STATUS_INVALID_ARGUMENT = -1,
  BMF_STATUS_OUT_OF_MEMORY = -2,
  BMF_STATUS_FILE_IO = -3,
  BMF_STATUS_TRUNCATED_DATA = -4,
  BMF_STATUS_INVALID_MAGIC = -5,
  BMF_STATUS_UNSUPPORTED_VERSION = -6,
  BMF_STATUS_UNSUPPORTED_BPP = -7,
  BMF_STATUS_EMPTY_FONT = -8,
  BMF_STATUS_INVALID_LAYOUT = -9,
  BMF_STATUS_INSUFFICIENT_STORAGE = -10,
  BMF_STATUS_NOT_FOUND = -11,
} bmf_status_t;

typedef struct {
  uint32_t codepoint;
  uint32_t bitmap_offset;
  uint16_t bitmap_size;
  uint8_t width;
  uint8_t height;
  int8_t x_offset;
  int8_t y_offset;
  uint8_t x_advance;
  uint8_t reserved;
} bmf_glyph_record_t;

typedef struct {
  uint16_t version;
  uint16_t flags;
  uint8_t bpp;
  uint16_t glyph_count;
  uint16_t line_height;
  int16_t ascent;
  int16_t descent;
  uint32_t glyph_table_offset;
  uint32_t bitmap_data_offset;
  uint32_t bitmap_data_size;
  const uint8_t *source_data;
  size_t source_size;
  uint8_t is_sorted;
} bmf_font_view_t;

typedef struct {
  uint16_t version;
  uint16_t flags;
  uint8_t bpp;
  uint16_t glyph_count;
  uint16_t line_height;
  int16_t ascent;
  int16_t descent;
  uint32_t glyph_table_offset;
  uint32_t bitmap_data_offset;
  uint32_t bitmap_data_size;
  bmf_glyph_record_t *glyphs;
  const uint8_t *bitmap_data;
  const uint8_t *source_data;
  size_t source_size;
  uint8_t owns_source_data;
  uint8_t owns_glyphs;
  uint8_t is_sorted;
} bmf_font_t;

void bmf_font_view_init(bmf_font_view_t *view);
bmf_status_t bmf_font_view_load_bytes(bmf_font_view_t *view, const uint8_t *data, size_t size);
bmf_status_t bmf_font_view_get_glyph(const bmf_font_view_t *view,
                                     size_t glyph_index,
                                     bmf_glyph_record_t *glyph);
bmf_status_t bmf_font_view_find_glyph(const bmf_font_view_t *view,
                                      uint32_t codepoint,
                                      bmf_glyph_record_t *glyph,
                                      size_t *glyph_index);
bmf_status_t bmf_font_view_find_glyph_binary(const bmf_font_view_t *view,
                                             uint32_t codepoint,
                                             bmf_glyph_record_t *glyph,
                                             size_t *glyph_index);
const uint8_t *bmf_font_view_get_glyph_bitmap(const bmf_font_view_t *view,
                                              const bmf_glyph_record_t *glyph,
                                              int *width,
                                              int *height);
bmf_status_t bmf_font_view_decode_glyphs(const bmf_font_view_t *view,
                                         bmf_glyph_record_t *glyphs,
                                         size_t glyph_capacity);

void bmf_font_init(bmf_font_t *font);
void bmf_font_destroy(bmf_font_t *font);
bmf_status_t bmf_font_load_bytes(bmf_font_t *font, const uint8_t *data, size_t size);
bmf_status_t bmf_font_load_bytes_with_glyph_storage(bmf_font_t *font,
                                                    const uint8_t *data,
                                                    size_t size,
                                                    bmf_glyph_record_t *glyph_storage,
                                                    size_t glyph_capacity);
bmf_status_t bmf_font_load_file(bmf_font_t *font, const char *path);
const char *bmf_status_string(bmf_status_t status);

bmf_font_t *bmf_load(const char *path);
bmf_font_t *bmf_load_from_memory(const uint8_t *data, size_t size);
void bmf_free(bmf_font_t *font);
const bmf_glyph_record_t *bmf_find_glyph(const bmf_font_t *font, uint32_t codepoint);
const bmf_glyph_record_t *bmf_find_glyph_binary(const bmf_font_t *font, uint32_t codepoint);
const uint8_t *bmf_get_glyph_bitmap(const bmf_font_t *font,
                                    size_t glyph_index,
                                    int *width,
                                    int *height);
int bmf_row_stride(const bmf_font_t *font, uint32_t width);
uint32_t bmf_glyph_bitmap_size(const bmf_font_t *font, uint32_t width, uint32_t height);

#endif
