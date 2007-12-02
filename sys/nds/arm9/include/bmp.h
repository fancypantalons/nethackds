#ifndef _BMP_H_
#define _BMP_H_

#include <nds.h>

#define BMP_WIN_V3_HEADER_LEN 40
#define BMP_OS2_V1_HEADER_LEN 12

#define BMP_WIN_V3_COMPRESSION_NONE 0
#define BMP_WIN_V3_COMPRESSION_RLE8 1
#define BMP_WIN_V3_COMPRESSION_RLE4 2
#define BMP_WIN_V3_COMPRESSION_BITFIELDS 3
#define BMP_WIN_V3_COMPRESSION_JPEG 4
#define BMP_WIN_V3_COMPRESSION_PNG 5

typedef struct {
  u16 magic;
  u32 file_length;
  u16 reserved[2];
  u32 bitmap_offset;
} bmp_header_t;

typedef struct {
  s32 width;
  s32 height;
  u16 planes;
  u16 bpp;
  u32 compression_method;
  u32 bitmap_length;
  s32 h_res;
  s32 v_res;
  s32 palette_size;
  s32 important_colours;
} bmp_dib_win_v3_header_t;

typedef struct {
  u16 width;
  u16 height;
  u16 planes;
  u16 bpp;
} bmp_dib_os2_v1_header_t;

typedef struct {
  u32 length;

  union {
    bmp_dib_win_v3_header_t win_v3;
    bmp_dib_os2_v1_header_t os2_v1;
  } fields;
} bmp_dib_header_t;

typedef struct {
  u8 r;
  u8 g;
  u8 b;
} bmp_palette_entry_t;

typedef struct {
  bmp_header_t header;
  bmp_dib_header_t dib_header;

  bmp_palette_entry_t *palette;
  u32 palette_length;

  u8 *bitmap;
  u32 bitmap_length;
} bmp_t;

int bmp_read(const char *name, bmp_t *bmp);
void bmp_free(bmp_t *bmp);

int bmp_bpp(bmp_t *bmp);
int bmp_width(bmp_t *bmp);
int bmp_height(bmp_t *bmp);

#endif
