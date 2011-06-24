#ifndef _NDS_TILE_H_
#define _NDS_TILE_H_

#include "nds_win.h"

typedef struct {
  u16 *gfx;
  int index;
  coord_t coords;
  bool hidden;
} sprite_t;

typedef struct {
  int glyph;
  int tile;
  int ch;
  int colour;
  int special;
  long last_used;     /* This is in game time (moves)          */
} tile_cache_entry_t;

typedef struct {
  int initialized;

  int glyphs[ROWNO][COLNO];
  coord_t center;
  coord_t cursor;
  rectangle_t viewport;
  int dirty;

  tile_cache_entry_t *tile_cache;
  int num_cache_entries;

  sprite_t sprites[64];
  int sprite_count;
} nds_map_t;

extern short glyph2tile[];

int nds_init_map();
nds_map_t *nds_get_map();
void nds_clear_map();
void nds_draw_map(coord_t *center);
coord_t nds_map_translate_coords(coord_t coords);
int nds_map_tile_width();
int nds_map_tile_height();
void nds_map_set_cursor(coord_t pos);
coord_t nds_map_get_center();
void nds_map_set_center(coord_t center);
coord_t nds_map_relativize(coord_t coords);
rectangle_t nds_minimap_dims();

#endif
