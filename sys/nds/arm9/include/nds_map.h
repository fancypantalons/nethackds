#ifndef _NDS_TILE_H_
#define _NDS_TILE_H_

#include "nds_win.h"

extern short glyph2tile[];

int nds_init_map(int *rows, int *cols);
void nds_draw_tile(int x, int y, int idx);
void nds_clear_map();
void nds_draw_map(nds_map_t *glyphs);
void nds_map_translate_coords(int x, int y, int *tx, int *ty);

#endif
