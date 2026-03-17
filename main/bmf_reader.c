#include "bmf_reader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t read_u16_le_bytes(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32_le_bytes(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int range_is_valid(size_t offset, size_t length, size_t total_size) {
  return offset <= total_size && length <= total_size - offset;
}

static int row_stride_for_bpp(uint8_t bpp, uint32_t width) {
  switch (bpp) {
    case BMF_BPP_MONO:
      return (int)((width + 7) / 8);
    case BMF_BPP_GRAY8:
      return (int)width;
    default:
      return -1;
  }
}

static uint32_t glyph_bitmap_size_for_bpp(uint8_t bpp, uint32_t width, uint32_t height) {
  int stride = row_stride_for_bpp(bpp, width);

  if (stride < 0) {
    return 0;
  }

  return (uint32_t)stride * height;
}

static void decode_glyph_record_bytes(const uint8_t *record, bmf_glyph_record_t *glyph) {
  glyph->codepoint = read_u32_le_bytes(record);
  glyph->bitmap_offset = read_u32_le_bytes(record + 4);
  glyph->bitmap_size = read_u16_le_bytes(record + 8);
  glyph->width = record[10];
  glyph->height = record[11];
  glyph->x_offset = (int8_t)record[12];
  glyph->y_offset = (int8_t)record[13];
  glyph->x_advance = record[14];
  glyph->reserved = record[15];
}

static const uint8_t *glyph_record_ptr(const bmf_font_view_t *view, size_t glyph_index) {
  return view->source_data + view->glyph_table_offset + glyph_index * GLYPH_RECORD_SIZE;
}

static bmf_status_t validate_glyph_record(const bmf_font_view_t *view,
                                          const bmf_glyph_record_t *glyph) {
  uint32_t expected_bitmap_size;

  if (!view || !glyph) {
    return BMF_STATUS_INVALID_ARGUMENT;
  }

  expected_bitmap_size = glyph_bitmap_size_for_bpp(view->bpp, glyph->width, glyph->height);
  if ((glyph->width == 0 || glyph->height == 0) && glyph->bitmap_size != 0) {
    return BMF_STATUS_INVALID_LAYOUT;
  }
  if (expected_bitmap_size > 0 && glyph->bitmap_size < expected_bitmap_size) {
    return BMF_STATUS_INVALID_LAYOUT;
  }
  if (glyph->bitmap_offset > view->bitmap_data_size) {
    return BMF_STATUS_INVALID_LAYOUT;
  }
  if ((uint32_t)glyph->bitmap_size > view->bitmap_data_size - glyph->bitmap_offset) {
    return BMF_STATUS_INVALID_LAYOUT;
  }

  return BMF_STATUS_OK;
}

static const uint8_t *glyph_bitmap_from_parts(uint8_t bpp,
                                              uint32_t bitmap_data_size,
                                              const uint8_t *bitmap_data,
                                              const bmf_glyph_record_t *glyph,
                                              int *width,
                                              int *height) {
  uint32_t bitmap_size;

  if (!bitmap_data || !glyph) {
    return NULL;
  }

  if (glyph->width == 0 || glyph->height == 0) {
    if (width) {
      *width = 0;
    }
    if (height) {
      *height = 0;
    }
    return NULL;
  }

  bitmap_size = glyph_bitmap_size_for_bpp(bpp, glyph->width, glyph->height);
  if (bitmap_size == 0 || glyph->bitmap_size < bitmap_size) {
    return NULL;
  }

  if (glyph->bitmap_offset > bitmap_data_size ||
      (uint32_t)glyph->bitmap_size > bitmap_data_size - glyph->bitmap_offset) {
    return NULL;
  }

  if (width) {
    *width = glyph->width;
  }
  if (height) {
    *height = glyph->height;
  }

  return bitmap_data + glyph->bitmap_offset;
}

static bmf_status_t parse_font_view(bmf_font_view_t *view, const uint8_t *data, size_t size) {
  bmf_font_view_t parsed;
  size_t glyph_table_size;
  size_t glyph_table_end;

  if (!view || !data) {
    return BMF_STATUS_INVALID_ARGUMENT;
  }

  memset(&parsed, 0, sizeof(parsed));
  parsed.source_data = data;
  parsed.source_size = size;

  if (size < HEADER_SIZE) {
    return BMF_STATUS_TRUNCATED_DATA;
  }

  if (memcmp(data, BMF_MAGIC, 4) != 0) {
    return BMF_STATUS_INVALID_MAGIC;
  }

  parsed.version = read_u16_le_bytes(data + 4);
  if (parsed.version != BMF_VERSION) {
    return BMF_STATUS_UNSUPPORTED_VERSION;
  }

  parsed.flags = read_u16_le_bytes(data + 6);
  parsed.bpp = data[8];
  if (parsed.bpp != BMF_BPP_MONO && parsed.bpp != BMF_BPP_GRAY8) {
    return BMF_STATUS_UNSUPPORTED_BPP;
  }

  parsed.glyph_count = read_u16_le_bytes(data + 10);
  parsed.line_height = read_u16_le_bytes(data + 12);
  parsed.ascent = (int16_t)read_u16_le_bytes(data + 14);
  parsed.descent = (int16_t)read_u16_le_bytes(data + 16);
  parsed.glyph_table_offset = read_u32_le_bytes(data + 18);
  parsed.bitmap_data_offset = read_u32_le_bytes(data + 22);
  parsed.bitmap_data_size = read_u32_le_bytes(data + 26);

  if (parsed.glyph_count == 0) {
    return BMF_STATUS_EMPTY_FONT;
  }

  glyph_table_size = (size_t)parsed.glyph_count * GLYPH_RECORD_SIZE;
  if (glyph_table_size / GLYPH_RECORD_SIZE != (size_t)parsed.glyph_count) {
    return BMF_STATUS_INVALID_LAYOUT;
  }

  if (!range_is_valid((size_t)parsed.glyph_table_offset, glyph_table_size, size)) {
    return BMF_STATUS_TRUNCATED_DATA;
  }
  if (!range_is_valid((size_t)parsed.bitmap_data_offset, (size_t)parsed.bitmap_data_size, size)) {
    return BMF_STATUS_TRUNCATED_DATA;
  }

  glyph_table_end = (size_t)parsed.glyph_table_offset + glyph_table_size;
  if ((size_t)parsed.bitmap_data_offset < glyph_table_end) {
    return BMF_STATUS_INVALID_LAYOUT;
  }

  for (uint16_t i = 0; i < parsed.glyph_count; i++) {
    bmf_glyph_record_t glyph;
    decode_glyph_record_bytes(glyph_record_ptr(&parsed, i), &glyph);
    if (validate_glyph_record(&parsed, &glyph) != BMF_STATUS_OK) {
      return BMF_STATUS_INVALID_LAYOUT;
    }
  }

  *view = parsed;
  return BMF_STATUS_OK;
}

static void font_assign_view_metadata(bmf_font_t *font, const bmf_font_view_t *view) {
  font->version = view->version;
  font->flags = view->flags;
  font->bpp = view->bpp;
  font->glyph_count = view->glyph_count;
  font->line_height = view->line_height;
  font->ascent = view->ascent;
  font->descent = view->descent;
  font->glyph_table_offset = view->glyph_table_offset;
  font->bitmap_data_offset = view->bitmap_data_offset;
  font->bitmap_data_size = view->bitmap_data_size;
  font->bitmap_data = view->source_data + view->bitmap_data_offset;
  font->source_data = view->source_data;
  font->source_size = view->source_size;
}

static bmf_status_t decode_all_glyphs(const bmf_font_view_t *view,
                                      bmf_glyph_record_t *glyphs,
                                      size_t glyph_capacity) {
  if (!view || !glyphs) {
    return BMF_STATUS_INVALID_ARGUMENT;
  }

  if (glyph_capacity < view->glyph_count) {
    return BMF_STATUS_INSUFFICIENT_STORAGE;
  }

  for (uint16_t i = 0; i < view->glyph_count; i++) {
    bmf_status_t status = bmf_font_view_get_glyph(view, i, &glyphs[i]);
    if (status != BMF_STATUS_OK) {
      return status;
    }
  }

  return BMF_STATUS_OK;
}

void bmf_font_view_init(bmf_font_view_t *view) {
  if (!view) {
    return;
  }

  memset(view, 0, sizeof(*view));
}

bmf_status_t bmf_font_view_load_bytes(bmf_font_view_t *view, const uint8_t *data, size_t size) {
  bmf_font_view_t parsed;
  bmf_status_t status;

  if (!view || !data) {
    return BMF_STATUS_INVALID_ARGUMENT;
  }

  status = parse_font_view(&parsed, data, size);
  if (status != BMF_STATUS_OK) {
    bmf_font_view_init(view);
    return status;
  }

  *view = parsed;
  return BMF_STATUS_OK;
}

bmf_status_t bmf_font_view_get_glyph(const bmf_font_view_t *view,
                                     size_t glyph_index,
                                     bmf_glyph_record_t *glyph) {
  bmf_status_t status;

  if (!view || !glyph) {
    return BMF_STATUS_INVALID_ARGUMENT;
  }

  if (glyph_index >= view->glyph_count) {
    return BMF_STATUS_NOT_FOUND;
  }

  decode_glyph_record_bytes(glyph_record_ptr(view, glyph_index), glyph);
  status = validate_glyph_record(view, glyph);
  if (status != BMF_STATUS_OK) {
    memset(glyph, 0, sizeof(*glyph));
    return status;
  }

  return BMF_STATUS_OK;
}

bmf_status_t bmf_font_view_find_glyph(const bmf_font_view_t *view,
                                      uint32_t codepoint,
                                      bmf_glyph_record_t *glyph,
                                      size_t *glyph_index) {
  bmf_glyph_record_t decoded;

  if (!view) {
    return BMF_STATUS_INVALID_ARGUMENT;
  }

  for (uint16_t i = 0; i < view->glyph_count; i++) {
    bmf_status_t status = bmf_font_view_get_glyph(view, i, &decoded);
    if (status != BMF_STATUS_OK) {
      return status;
    }
    if (decoded.codepoint == codepoint) {
      if (glyph) {
        *glyph = decoded;
      }
      if (glyph_index) {
        *glyph_index = i;
      }
      return BMF_STATUS_OK;
    }
  }

  return BMF_STATUS_NOT_FOUND;
}

const uint8_t *bmf_font_view_get_glyph_bitmap(const bmf_font_view_t *view,
                                              const bmf_glyph_record_t *glyph,
                                              int *width,
                                              int *height) {
  if (!view || !glyph || !view->source_data) {
    return NULL;
  }

  return glyph_bitmap_from_parts(view->bpp,
                                 view->bitmap_data_size,
                                 view->source_data + view->bitmap_data_offset,
                                 glyph,
                                 width,
                                 height);
}

bmf_status_t bmf_font_view_decode_glyphs(const bmf_font_view_t *view,
                                         bmf_glyph_record_t *glyphs,
                                         size_t glyph_capacity) {
  return decode_all_glyphs(view, glyphs, glyph_capacity);
}

void bmf_font_init(bmf_font_t *font) {
  if (!font) {
    return;
  }

  memset(font, 0, sizeof(*font));
}

void bmf_font_destroy(bmf_font_t *font) {
  if (!font) {
    return;
  }

  if (font->owns_glyphs) {
    free(font->glyphs);
  }
  if (font->owns_source_data) {
    free((void *)font->source_data);
  }
  memset(font, 0, sizeof(*font));
}

const char *bmf_status_string(bmf_status_t status) {
  switch (status) {
    case BMF_STATUS_OK:
      return "ok";
    case BMF_STATUS_INVALID_ARGUMENT:
      return "invalid argument";
    case BMF_STATUS_OUT_OF_MEMORY:
      return "out of memory";
    case BMF_STATUS_FILE_IO:
      return "file I/O error";
    case BMF_STATUS_TRUNCATED_DATA:
      return "truncated BMF data";
    case BMF_STATUS_INVALID_MAGIC:
      return "invalid BMF magic number";
    case BMF_STATUS_UNSUPPORTED_VERSION:
      return "unsupported BMF version";
    case BMF_STATUS_UNSUPPORTED_BPP:
      return "unsupported BMF bpp";
    case BMF_STATUS_EMPTY_FONT:
      return "BMF file contains no glyphs";
    case BMF_STATUS_INVALID_LAYOUT:
      return "invalid BMF layout";
    case BMF_STATUS_INSUFFICIENT_STORAGE:
      return "insufficient glyph storage";
    case BMF_STATUS_NOT_FOUND:
      return "not found";
    default:
      return "unknown BMF status";
  }
}

int bmf_row_stride(const bmf_font_t *font, uint32_t width) {
  if (!font) {
    return -1;
  }

  return row_stride_for_bpp(font->bpp, width);
}

uint32_t bmf_glyph_bitmap_size(const bmf_font_t *font, uint32_t width, uint32_t height) {
  if (!font) {
    return 0;
  }

  return glyph_bitmap_size_for_bpp(font->bpp, width, height);
}

bmf_status_t bmf_font_load_bytes(bmf_font_t *font, const uint8_t *data, size_t size) {
  bmf_font_view_t view;
  bmf_font_view_t copied_view;
  bmf_glyph_record_t *glyphs;
  uint8_t *buffer;
  bmf_status_t status;

  if (!font || !data) {
    return BMF_STATUS_INVALID_ARGUMENT;
  }

  bmf_font_view_init(&view);
  status = bmf_font_view_load_bytes(&view, data, size);
  if (status != BMF_STATUS_OK) {
    return status;
  }

  buffer = malloc(size > 0 ? size : 1);
  if (!buffer) {
    return BMF_STATUS_OUT_OF_MEMORY;
  }
  if (size > 0) {
    memcpy(buffer, data, size);
  }

  bmf_font_view_init(&copied_view);
  status = bmf_font_view_load_bytes(&copied_view, buffer, size);
  if (status != BMF_STATUS_OK) {
    free(buffer);
    return status;
  }

  glyphs = calloc(copied_view.glyph_count, sizeof(*glyphs));
  if (!glyphs) {
    free(buffer);
    return BMF_STATUS_OUT_OF_MEMORY;
  }

  status = decode_all_glyphs(&copied_view, glyphs, copied_view.glyph_count);
  if (status != BMF_STATUS_OK) {
    free(glyphs);
    free(buffer);
    return status;
  }

  bmf_font_destroy(font);
  font_assign_view_metadata(font, &copied_view);
  font->glyphs = glyphs;
  font->owns_glyphs = 1;
  font->owns_source_data = 1;

  return BMF_STATUS_OK;
}

bmf_status_t bmf_font_load_bytes_with_glyph_storage(bmf_font_t *font,
                                                    const uint8_t *data,
                                                    size_t size,
                                                    bmf_glyph_record_t *glyph_storage,
                                                    size_t glyph_capacity) {
  bmf_font_view_t view;
  bmf_glyph_record_t *old_glyphs;
  const uint8_t *old_source_data;
  uint8_t preserve_owned_glyphs;
  uint8_t preserve_owned_source_data;
  bmf_status_t status;

  if (!font || !data || !glyph_storage) {
    return BMF_STATUS_INVALID_ARGUMENT;
  }

  bmf_font_view_init(&view);
  status = bmf_font_view_load_bytes(&view, data, size);
  if (status != BMF_STATUS_OK) {
    return status;
  }

  status = decode_all_glyphs(&view, glyph_storage, glyph_capacity);
  if (status != BMF_STATUS_OK) {
    return status;
  }

  old_glyphs = font->glyphs;
  old_source_data = font->source_data;
  preserve_owned_glyphs = (uint8_t)(font->owns_glyphs && old_glyphs == glyph_storage);
  preserve_owned_source_data = (uint8_t)(font->owns_source_data && old_source_data == data);

  if (font->owns_glyphs && !preserve_owned_glyphs) {
    free(old_glyphs);
  }
  if (font->owns_source_data && !preserve_owned_source_data) {
    free((void *)old_source_data);
  }

  memset(font, 0, sizeof(*font));
  font_assign_view_metadata(font, &view);
  font->glyphs = glyph_storage;
  font->owns_glyphs = preserve_owned_glyphs;
  font->owns_source_data = preserve_owned_source_data;

  return BMF_STATUS_OK;
}

bmf_status_t bmf_font_load_file(bmf_font_t *font, const char *path) {
  FILE *f;
  long file_size_long;
  size_t file_size;
  uint8_t *buffer;
  bmf_font_view_t view;
  bmf_glyph_record_t *glyphs;
  bmf_status_t status;

  if (!font || !path) {
    return BMF_STATUS_INVALID_ARGUMENT;
  }

  f = fopen(path, "rb");
  if (!f) {
    return BMF_STATUS_FILE_IO;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return BMF_STATUS_FILE_IO;
  }
  file_size_long = ftell(f);
  if (file_size_long < 0) {
    fclose(f);
    return BMF_STATUS_FILE_IO;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return BMF_STATUS_FILE_IO;
  }

  file_size = (size_t)file_size_long;
  buffer = malloc(file_size > 0 ? file_size : 1);
  if (!buffer) {
    fclose(f);
    return BMF_STATUS_OUT_OF_MEMORY;
  }

  if (file_size > 0 && fread(buffer, 1, file_size, f) != file_size) {
    free(buffer);
    fclose(f);
    return BMF_STATUS_FILE_IO;
  }

  fclose(f);

  bmf_font_view_init(&view);
  status = bmf_font_view_load_bytes(&view, buffer, file_size);
  if (status != BMF_STATUS_OK) {
    free(buffer);
    return status;
  }

  glyphs = calloc(view.glyph_count, sizeof(*glyphs));
  if (!glyphs) {
    free(buffer);
    return BMF_STATUS_OUT_OF_MEMORY;
  }

  status = decode_all_glyphs(&view, glyphs, view.glyph_count);
  if (status != BMF_STATUS_OK) {
    free(glyphs);
    free(buffer);
    return status;
  }

  bmf_font_destroy(font);
  font_assign_view_metadata(font, &view);
  font->glyphs = glyphs;
  font->source_data = buffer;
  font->source_size = file_size;
  font->bitmap_data = buffer + view.bitmap_data_offset;
  font->owns_glyphs = 1;
  font->owns_source_data = 1;

  return BMF_STATUS_OK;
}

bmf_font_t *bmf_load(const char *path) {
  bmf_font_t *font = malloc(sizeof(*font));

  if (!font) {
    return NULL;
  }

  bmf_font_init(font);
  if (bmf_font_load_file(font, path) != BMF_STATUS_OK) {
    free(font);
    return NULL;
  }

  return font;
}

bmf_font_t *bmf_load_from_memory(const uint8_t *data, size_t size) {
  bmf_font_t *font = malloc(sizeof(*font));

  if (!font) {
    return NULL;
  }

  bmf_font_init(font);
  if (bmf_font_load_bytes(font, data, size) != BMF_STATUS_OK) {
    free(font);
    return NULL;
  }

  return font;
}

void bmf_free(bmf_font_t *font) {
  if (!font) {
    return;
  }

  bmf_font_destroy(font);
  free(font);
}

const bmf_glyph_record_t *bmf_find_glyph(const bmf_font_t *font, uint32_t codepoint) {
  if (!font || !font->glyphs) {
    return NULL;
  }

  for (uint16_t i = 0; i < font->glyph_count; i++) {
    if (font->glyphs[i].codepoint == codepoint) {
      return &font->glyphs[i];
    }
  }

  return NULL;
}

const uint8_t *bmf_get_glyph_bitmap(const bmf_font_t *font,
                                    size_t glyph_index,
                                    int *width,
                                    int *height) {
  if (!font || !font->glyphs || glyph_index >= font->glyph_count) {
    return NULL;
  }

  return glyph_bitmap_from_parts(font->bpp,
                                 font->bitmap_data_size,
                                 font->bitmap_data,
                                 &font->glyphs[glyph_index],
                                 width,
                                 height);
}
