#ifndef _NDS_GFX_H_
#define _NDS_GFX_H_

#include "bmp.h"
#include "font-bdf.h"

typedef u16 nds_palette[16];

/*
 * Various graphics functions.
 */

void nds_draw_hline(int x, int y, int width, u16 colour, u16 *dest);
void nds_draw_vline(int x, int y, int height, u16 colour, u16 *dest);
void nds_draw_rect(int x, int y, int width, int height, u16 colour, u16 *dest);
void nds_draw_rect_outline(int x, int y, int width, int height, u8 fill_colour, u8 line_colour, u16 *dest);

void nds_draw_text(struct font *fnt, 
                   char *str,
                   int x, int y,
                   u16 black, u16 white,
                   u16 *dest);

void nds_fill(u16 *dest, u8 colour);
void nds_draw_bmp(bmp_t *bmp, u16 *vram, u16 *palette);
int nds_load_palette(char *fname, nds_palette palette);

#endif
