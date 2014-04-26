#include <nds.h>
#include <stdio.h>
#include "hack.h"
#include "nds_debug.h"
#include "nds_gfx.h"
#include "nds_util.h"
#include "font-bdf.h"
#include "ppm-lite.h"

void nds_draw_hline(int x, int y, int width, u16 colour, u16 *dest)
{
  int i;

  for (i = x; i <= (x + width); i++)
  {
    SET_PIXEL(dest, i, y, colour);
  }
}

void nds_draw_vline(int x, int y, int height, u16 colour, u16 *dest)
{
  int i;

  for (i = y; i <= (y + height); i++)
  {
    SET_PIXEL(dest, x, i, colour);
  }
}

void nds_draw_rect(rectangle_t rect, u16 colour, u16 *dest)
{
  int i;

  for (i = rect.start.y; i < RECT_END_Y(rect); i++)
  {
    nds_draw_hline(rect.start.x, i, RECT_END_X(rect), colour, dest);
  }
}

void nds_draw_rect_outline(rectangle_t rect, u16 colour, u16 *dest)
{
  nds_draw_hline(rect.start.x, rect.start.y, rect.dims.width, colour, dest);
  nds_draw_hline(rect.start.x, RECT_END_Y(rect), rect.dims.width, colour, dest);

  nds_draw_vline(rect.start.x, rect.start.y, rect.dims.height, colour, dest);
  nds_draw_vline(RECT_END_X(rect), rect.start.y, rect.dims.height, colour, dest);
}

void nds_draw_text(struct font *fnt, 
                   char *str,
                   int x, int y,
                   u16 *dest)
{
  int w, h;
  struct ppm *img;

  text_dims(fnt, str, &w, &h);
  
  img = alloc_ppm(w, h);

  draw_string(fnt, str, img, 0, 0, -1, -1);

  draw_ppm(img, dest, x, y, 256);

  free_ppm(img);
}

void nds_fill(u16 *dest, u8 colour)
{
  int i;
  u16 tmp = (colour << 8) | colour;

  for (i = 0; i < 256 * 192 / 2; i++) {
    dest[i] = tmp;
  }
}

void nds_draw_bmp(bmp_t *bmp, u16 *vram, u16 *palette)
{
  int i, y;
  int height = bmp_height(bmp);
  int bpp = bmp_bpp(bmp);
  u8 *bitmap = bmp->bitmap;

  for (i = 0; i < bmp->palette_length; i++) {
    palette[i] = RGB15(bmp->palette[i].r >> 3,
                       bmp->palette[i].g >> 3,
                       bmp->palette[i].b >> 3);
  }

  for (y = height - 1; y >= 0; y--) {
    u16 *target = vram + (192 / 2 - height / 2 + y) * 128;
    u16 *tend = target + 128;

    while (target < tend) {
      switch (bpp) {
        case 1:
          target[0] = ((*bitmap & 0x80) >> 7) |
                      (((*bitmap & 0x40) >> 6) << 8);
          target[1] = ((*bitmap & 0x20) >> 5) |
                      (((*bitmap & 0x10) >> 4) << 8);
          target[2] = ((*bitmap & 0x08) >> 3) |
                      (((*bitmap & 0x04) >> 2) << 8);
          target[3] = ((*bitmap & 0x02) >> 1) |
                      (((*bitmap & 0x01) >> 0) << 8);

          bitmap++;
          target += 4;
          break;

        case 4:
          *target = ((*bitmap & 0xF0) >> 4) |
                    ((*bitmap & 0x0F) << 8);

          bitmap++;
          target++;
          break;

        case 8:
          *target = (bitmap[1] << 8) | bitmap[0];

          bitmap += 2;
          target++; 
          break;

        default:
          break;
      }
    }
  }
}

int nds_load_palette(char *fname, nds_palette palette)
{
  FILE *pfile;
  int palidx = 0;

  if ((pfile = fopen(fname, "r")) == NULL) {
    DEBUG_PRINT("Unable to open '%s'\n", fname);

    return -1;
  }

  while (1) {
    char buf[BUFSZ];
    char *tmp;
    int r, g, b;

    if (fgets(buf, BUFSZ, pfile) == NULL) {
      break;
    }

    tmp = nds_strip(buf, 1, 1);

    if ((*tmp == '#') || (*tmp == '\0')) {
      continue;
    }

    if (sscanf(tmp, "%2x%2x%2x", &r, &g, &b) < 3) {
      DEBUG_PRINT("Malformed line: '%s'\n", tmp);

      return -1;
    }

    palette[palidx++] = RGB15(r >> 3, g >> 3, b >> 3);
  }

  return palidx;
}
