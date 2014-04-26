#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "nds_debug.h"
#include "bmp.h"

#define READ_LONG(f, var) { \
  unsigned char buf[4]; \
  \
  if (fread(buf, 4, 1, file) < 1) { return -1; }; \
  var = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0]; \
}

#define READ_SHORT(f, var) { \
  unsigned char buf[2]; \
  \
  if (fread(buf, 2, 1, file) < 1) { return -1; }; \
  var = (buf[1] << 8) | buf[0]; \
}

#define READ_BYTE(f, var) { \
  if (fread(&(var), 1, 1, file) < 1) { return -1; }; \
}

static int bmp_read_header(FILE *file, bmp_header_t *header)
{
  READ_SHORT(file, header->magic);

  if (header->magic != 0x4D42) {
    return -1;
  }

  READ_LONG(file, header->file_length);
  READ_SHORT(file, header->reserved[0]);
  READ_SHORT(file, header->reserved[1]);
  READ_LONG(file, header->bitmap_offset);

  return 0;
}

static int bmp_read_dib_header(FILE *file, bmp_dib_header_t *dib_header)
{
  READ_LONG(file, dib_header->length);

  switch (dib_header->length) {
    /* WIN V3 Header */
    case 40:
      {
        bmp_dib_win_v3_header_t *header = &(dib_header->fields.win_v3);

        READ_LONG(file, header->width);
        READ_LONG(file, header->height);
        READ_SHORT(file, header->planes);
        READ_SHORT(file, header->bpp);
        READ_LONG(file, header->compression_method);
        READ_LONG(file, header->bitmap_length);
        READ_LONG(file, header->h_res);
        READ_LONG(file, header->v_res);
        READ_LONG(file, header->palette_size);
        READ_LONG(file, header->important_colours);
      }

      break;

    /* OS/2 V1 Header */
    case 12:
      {
        bmp_dib_os2_v1_header_t *header = &(dib_header->fields.os2_v1);

        READ_SHORT(file, header->width);
        READ_SHORT(file, header->height);
        READ_SHORT(file, header->planes);
        READ_SHORT(file, header->bpp);
      }

      break;

    default:
      return -1;
  }

  return 0;
}

static int bmp_read_palette(FILE *file, bmp_t *bmp)
{
  int i;

  /* Alright, let's find out how big our palette is */

  switch (bmp->dib_header.length) {
    case BMP_WIN_V3_HEADER_LEN:
      if (bmp->dib_header.fields.win_v3.bpp > 8) {
        bmp->palette_length = 0;
      } else if (bmp->dib_header.fields.win_v3.palette_size == 0) {
        bmp->palette_length = 1 << bmp->dib_header.fields.win_v3.bpp;
      } else {
        bmp->palette_length = bmp->dib_header.fields.win_v3.palette_size;
      }

      break;

    case BMP_OS2_V1_HEADER_LEN:
      if (bmp->dib_header.fields.os2_v1.bpp <= 8) {
        bmp->palette_length = 1 << bmp->dib_header.fields.os2_v1.bpp;
      } else {
        bmp->palette_length = 0;
      }

      break;
  }

  /* Now load it */

  bmp->palette = (bmp_palette_entry_t *)malloc(sizeof(bmp_palette_entry_t) * bmp->palette_length);

  for (i = 0; i < bmp->palette_length; i++) {
    READ_BYTE(file, bmp->palette[i].b);
    READ_BYTE(file, bmp->palette[i].g);
    READ_BYTE(file, bmp->palette[i].r);

    if (bmp->dib_header.length == BMP_WIN_V3_HEADER_LEN) {
      u8 tmp;

      READ_BYTE(file, tmp);
    }
  }
 
  return 0;
}

int bmp_read_bitmap(FILE *file, bmp_t *bmp) {
  int len;

  switch (bmp->dib_header.length) {
    case BMP_WIN_V3_HEADER_LEN:
      bmp->bitmap_length = bmp->dib_header.fields.win_v3.bitmap_length;

      break;

    case BMP_OS2_V1_HEADER_LEN:
      bmp->bitmap_length = bmp->dib_header.fields.os2_v1.width *
                           bmp->dib_header.fields.os2_v1.height;

      bmp->bitmap_length /= 8 / bmp->dib_header.fields.os2_v1.bpp;

      break;

    default:
      return -1;
  }

  bmp->bitmap = (u8 *)malloc(bmp->bitmap_length);

  if ((len = fread(bmp->bitmap, 1, bmp->bitmap_length, file)) < bmp->bitmap_length) {
    DEBUG_PRINT("Short read on BMP, got %d bytes, expected %d\n", 
            len, bmp->bitmap_length);

    return -1;
  }

  return 0;
}

int bmp_read(const char *name, bmp_t *bmp) {
  FILE *file = fopen(name, "rb");
  int ret;

  if (! file) {
    return -1;
  }

  if ((ret = bmp_read_header(file, &(bmp->header))) < 0) {
    goto DONE;
  }

  if ((ret = bmp_read_dib_header(file, &(bmp->dib_header))) < 0) {
    goto DONE;
  }

  if ((ret = bmp_read_palette(file, bmp)) < 0) {
    goto DONE;
  }

  if ((ret = bmp_read_bitmap(file, bmp)) < 0) {
    free(bmp->palette);
    goto DONE;
  }

DONE:

  fclose(file);

  return ret;
}

void bmp_free(bmp_t *bmp)
{
  if (bmp->bitmap) {
    free(bmp->bitmap);
  }

  if (bmp->palette) {
    free(bmp->palette);
  }
}

int bmp_bpp(bmp_t *bmp)
{
  switch (bmp->dib_header.length) {
    case BMP_WIN_V3_HEADER_LEN:
      return bmp->dib_header.fields.win_v3.bpp;

    case BMP_OS2_V1_HEADER_LEN:
      return bmp->dib_header.fields.os2_v1.bpp;

    default:
      return -1;
  }
}

int bmp_width(bmp_t *bmp)
{
  switch (bmp->dib_header.length) {
    case BMP_WIN_V3_HEADER_LEN:
      return bmp->dib_header.fields.win_v3.width;

    case BMP_OS2_V1_HEADER_LEN:
      return bmp->dib_header.fields.os2_v1.width;

    default:
      return -1;
  }
}

int bmp_height(bmp_t *bmp)
{
  switch (bmp->dib_header.length) {
    case BMP_WIN_V3_HEADER_LEN:
      return bmp->dib_header.fields.win_v3.height;

    case BMP_OS2_V1_HEADER_LEN:
      return bmp->dib_header.fields.os2_v1.height;

    default:
      return -1;
  }
}
