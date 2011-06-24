#ifndef _NDS_GFX_H_
#define _NDS_GFX_H_

#include "bmp.h"
#include "font-bdf.h"
#include "nds_util.h"

#define SET_PIXEL(buffer, x, y, colour) \
  ( \
    ((x & 1) == 0) \
    ? (dest[(y * 128) + (x / 2)] = ((dest[(y * 128) + (x / 2)]) & 0xFF00) | (colour & 0xFF)) \
    : (dest[(y * 128) + (x / 2)] = ((dest[(y * 128) + (x / 2)]) & 0x00FF) | ((colour & 0xFF) << 8)) \
  )

typedef u16 nds_palette[16];

/*
 * Various graphics functions.
 */

void nds_draw_hline(int x, int y, int width, u16 colour, u16 *dest);
void nds_draw_vline(int x, int y, int height, u16 colour, u16 *dest);
void nds_draw_rect(rectangle_t rect, u16 colour, u16 *dest);
void nds_draw_rect_outline(rectangle_t rect, u16 colour, u16 *dest);

void nds_draw_text(struct font *fnt, 
                   char *str,
                   int x, int y,
                   u16 *dest);

void nds_fill(u16 *dest, u8 colour);
void nds_draw_bmp(bmp_t *bmp, u16 *vram, u16 *palette);
int nds_load_palette(char *fname, nds_palette palette);

#endif
