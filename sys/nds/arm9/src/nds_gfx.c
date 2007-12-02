#include <nds.h>
#include "nds_gfx.h"
#include "font-bdf.h"
#include "ppm-lite.h"

void nds_draw_hline(int x, int y, int width, u16 colour, u16 *dest)
{
  int i;

  dest += (y * 128) + (x / 2);

  if (x & 0x01) {
    *dest = (*dest & 0x00FF) | (colour << 8);
    dest++;
  }

  for (i = 0; i < (width - ((x & 0x01) ? 1 : 0)) / 2; i++, dest++) {
    *dest = (colour << 8) | colour;
  }

  if ((x + width) & 0x01) {
    *dest = (*dest & 0xFF00) | colour;
  }
}

void nds_draw_vline(int x, int y, int height, u16 colour, u16 *dest)
{
  int i;

  dest += (y * 128) + (x / 2);

  for (i = 0; i < height; i++, dest += 128) {
    if (x & 0x01) {
      *dest = (*dest & 0x00FF) | (colour << 8);
    } else {
      *dest = (*dest & 0xFF00) | colour;
    }
  }
}

void nds_draw_rect(int x, int y, int width, int height, u16 colour, u16 *dest)
{
  int i;
  u16 tmp = (colour << 8) | colour;

  for (i = 0; i < height; i++, dest += 128) {
    nds_draw_hline(x, y, width, tmp, dest);
  }
}

void nds_draw_rect_outline(int x, int y, int width, int height, u8 fill_colour, u8 line_colour, u16 *dest)
{
  nds_draw_rect(x, y, width, height, fill_colour, dest);

  nds_draw_hline(x, y, width, line_colour, dest);
  nds_draw_hline(x, y + height, width, line_colour, dest);

  nds_draw_vline(x, y, height, line_colour, dest);
  nds_draw_vline(x + width, y, height, line_colour, dest);
}

void nds_draw_text(struct font *fnt, 
                   char *str,
                   int x, int y,
                   u16 black, u16 white,
                   u16 *dest)
{
  int w, h;
  struct ppm *img;

  text_dims(fnt, str, &w, &h);
  
  img = alloc_ppm(w, h);

  draw_string(fnt, str, img, 0, 0, 1, 255, 0, 255);

  draw_ppm_bw(img, dest, x, y, 256, black, white);

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
