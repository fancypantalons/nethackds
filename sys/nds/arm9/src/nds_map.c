/*
 * Tile management functions.
 */

#include <nds.h>
#include "hack.h"

#include "bmp.h"

#include "nds_win.h"
#include "nds_util.h"
#include "nds_map.h"

#define DEF_TILE_FILE "tiles.bmp"

#define DEF_TILE_WIDTH		16
#define DEF_TILE_HEIGHT		16

#define NUM_TILES               1057

#define MAX_TILE_SLOTS          512

#define TILE_WIDTH		(iflags.wc_tile_width ? iflags.wc_tile_width : DEF_TILE_WIDTH)
#define TILE_HEIGHT		(iflags.wc_tile_height ? iflags.wc_tile_height : DEF_TILE_HEIGHT)
#define TILE_FILE		iflags.wc_tile_file

#define c2(a,i)		(RGB15((a[i+2]>>3),(a[i+1]>>3),(a[i]>>3)))

typedef struct {
  u16 tile_ram_idx;   /* Position this tile is in the tile RAM */
  long load_time;     /* This is in game time (moves)          */
} tile_cache_entry_t;

bmp_t tiles;
int width_in_tiles;
int height_in_tiles;

u16 *tile_ram = (u16 *)BG_TILE_RAM(6);
u16 *map_ram = (u16 *)BG_MAP_RAM(8);

tile_cache_entry_t tile_cache[NUM_TILES];

int num_free_tiles;

int tile_width;
int tile_height;

int map_width;
int map_height;

/*
 * Copy a tile from our tile image, loaded previously, into tile RAM.  In the
 * case of tiles with a width or height larger than 8 pixels, this will result
 * in multiple tile positions being occupied in video RAM.
 */
void nds_load_tile(int idx) 
{
  int i, j, x;
  int tile_idx = -1;
  int width, height, bpp, row_bytes;
  int bmp_tile_x, bmp_tile_y;
  u8 *bmp_row_start;
  u16 *tile_row_start;

  if (num_free_tiles > 0) {
    /*
     * If there's still free tiles, just use the next empty ones.
     */

    tile_idx = MAX_TILE_SLOTS - num_free_tiles;
    num_free_tiles -= tile_width * tile_height;
  } else {
    /*
     * If there are no free slots, we look for the oldest tile and overwrite
     * it.  I'm assuming (fingers crossed) that by the time we run out of tile
     * RAM, there will be low-level enemies, tiles, etc, that'll stop appearing,
     * and so we can safely remove them.
     */

    long last_diff = 0;
    int old_cache_idx;

    for (i = 0; i < NUM_TILES; i++) {
      if (tile_cache[i].load_time < 0) {
        continue;
      } else if ((moves - tile_cache[i].load_time) > last_diff) {
        tile_idx = tile_cache[i].tile_ram_idx;
        old_cache_idx = i;

        last_diff = moves - tile_cache[i].load_time;
      }
    }

    /* Evict the old entry from the cache */

    tile_cache[old_cache_idx].load_time = -1;
    tile_cache[old_cache_idx].tile_ram_idx = 0;
  }

  /* Now record the new entry */

  tile_cache[idx].load_time = moves;
  tile_cache[idx].tile_ram_idx = tile_idx;

  /*
   * Alright, now load the tile into memory.
   */

  /* Next, get a few fields from the BMP headers that we'll need. */

  width = bmp_width(&tiles);
  height = bmp_height(&tiles);
  bpp = bmp_bpp(&tiles);

  /*
   * Compute the number of bytes in each row of 8-pixel tiles.
   *
   * This works explicitely because each tile component is 8 pixels wide.
   * Thus, the number of bytes in a row is (8 * (bpp / 8)), which just
   * ends up being bpp.
   */

  row_bytes = tile_width * bpp;

  /* Now calculate the pointer which points to the start of the BMP row */

  bmp_tile_y = idx / width_in_tiles;
  bmp_tile_x = width_in_tiles - idx % width_in_tiles;

  bmp_row_start = tiles.bitmap + tiles.bitmap_length - 
                  bmp_tile_y * TILE_HEIGHT * width_in_tiles * row_bytes - 
                  bmp_tile_x * row_bytes ;

  for (j = 0; j < tile_height; j++) {
    int y;

    for (y = 0; y < 8; y++) {
      for (i = 0; i < tile_width; i++) {
        tile_row_start = tile_ram + (tile_idx + j * tile_width + i) * bpp * 8 / 2 + y * bpp / 2;

        /* Again, this works because 8 * (bpp / 8) == bpp */

        for (x = 0; x < bpp; x += 2, bmp_row_start += 2) {
          if (bpp == 4) {
            u16 a, b, c, d;

            a = (bmp_row_start[0] & 0xF0) >> 4;
            b = (bmp_row_start[0] & 0x0F);
            c = (bmp_row_start[1] & 0xF0) >> 4;
            d = (bmp_row_start[1] & 0x0F);

            //tile_row_start[x / 2] = (bmp_row_start[1]) |
            //                        (bmp_row_start[0] << 8);

            tile_row_start[x / 2] = (d << 12) | (c << 8) | (b << 4) | (a << 0);
          } else {
            tile_row_start[x / 2] = (bmp_row_start[1] << 8) |
                                     bmp_row_start[0];
          }
        }
      }

      bmp_row_start -= row_bytes * width_in_tiles + row_bytes;
    }
  }
}

/*
 * Plot the specified tile to the map.  Note, this represents the tile index
 * as present in the BMP file, *not* the tile RAM index.
 */
void nds_draw_tile(int x, int y, int idx)
{
  int midx, tidx;
  int i, j;

  if (idx < 0) {
    tidx = 0x0000;
  } else if (tile_cache[idx].tile_ram_idx == 0) {
    nds_load_tile(idx);

    tidx = tile_cache[idx].tile_ram_idx; 
  } else {
    tidx = tile_cache[idx].tile_ram_idx; 
  }

  /*
   * midx is the starting map index.
   * tidx is the starting tile index.
   *
   * We do a bit of tricky looping here to keep things fast.  midx points to
   * the starting map index.  Each time we complete a row iteration, we move
   * midx ahead by map_width, so it points to the start of the next row.  We
   * then use that to loop through the row copying the tile indices over.
   *
   * Meanwhile, tidx starts off as the initial tile index in RAM.  Because
   * the tiles are ordered left-to-right, top-to-bottom, we can just
   * increment the counter as we populate the map.
   */

  midx = (y * tile_height) * 32 + x * tile_width;

  for (j = 0; j < tile_height; j++, midx += 32) {
    for (i = 0; i < tile_width; i++) {
      map_ram[midx + i] = tidx | 0x2000; 

      if (tidx > 0) {
        tidx++;
      }
    }
  }
}

/*
 * Here we do two things.  First, we load the BMP into memory.  We throw an
 * error if it isn't an indexed file of some kind (bpp <= 8).  After the file 
 * is loaded, we populate the palette provided.  This involves converting the 
 * 24-bit RGB tuplets to 15-bit NDS palette entries.
 */
int nds_init_map(int *rows, int *cols)
{
  u16 *palette;
  char *fname = TILE_FILE ? TILE_FILE : DEF_TILE_FILE;
  int i;

  /* Initialize the data we need to manage the map and tiles */

  memset(tile_cache, 0, sizeof(tile_cache));

  tile_width = TILE_WIDTH / 8;
  tile_height = TILE_HEIGHT / 8;

  map_width = 32 / tile_width;
  map_height = 24 / tile_height;

  num_free_tiles = MAX_TILE_SLOTS / tile_width * tile_height - 1;

  for (i = 0; i < NUM_TILES; i++) {
    tile_cache[i].load_time = -1;
  }

  /* Now load the tiles into memory */

  if (bmp_read(fname, &tiles) < 0) {
    return -1;
  }

  /* Now initialize our graphics layer */

  /*
   * Set up the tile RAM starting at 16k.  At 65k, the bitmap data starts
   * for the menu layer, so we stick the map RAM 2k before that, 
   * which maximizes the amount of tile RAM we have.
   */

  BG1_CR = BG_32x32 | BG_MAP_BASE(8) | BG_TILE_BASE(6) | BG_PRIORITY_3;

  switch (bmp_bpp(&tiles)) {
    case 4:
      BG1_CR |= BG_16_COLOR;

      palette = (u16 *)BG_PALETTE + 32;
      break;

    case 8:
      BG1_CR |= BG_256_COLOR;

      vramSetBankH(VRAM_H_LCD);
      palette = VRAM_H;

      break;

    default:
      iprintf("Sorry, %d bpp tile files aren't supported.\n", bmp_bpp(&tiles));

      return -1;
  }

  width_in_tiles = bmp_width(&tiles) / TILE_WIDTH;
  height_in_tiles = bmp_height(&tiles) / TILE_HEIGHT;

  /* Alright, file loaded, let's copy over the palette */

  for (i = 0; i < tiles.palette_length; i++) {
    palette[i] = RGB15((tiles.palette[i].r >> 3),
                       (tiles.palette[i].g >> 3),
                       (tiles.palette[i].b >> 3));
  }

  if (bmp_bpp(&tiles) == 8) {
    vramSetBankH(VRAM_H_SUB_BG_EXT_PALETTE);
  }

  /* Lastly, compute the number of rows and columns in our map. */

  return 0;
}

void nds_clear_map()
{
  memset(map_ram, 0, 32 * 24 * 2);
}

void nds_draw_map(nds_map_t *map)
{
  if (map != NULL) {
    int sx = u.ux - map_width / 2;
    int sy = u.uy - map_height / 2;

    int x, y;

    for (y = 0; y < map_height; y++) {
      for (x = 0; x < map_width; x++) {
        if (((sx + x) < 0) || ((sx + x) >= COLNO) ||
            ((sy + y) < 0) || ((sy + y) >= ROWNO)) {
          nds_draw_tile(x, y, -1);
        } else {
          nds_draw_tile(x, y, glyph2tile[map->glyphs[sy + y][sx + x]]);
        }
      }
    }
  } else {
    nds_clear_map();
  }
}

void nds_map_translate_coords(int x, int y, int *tx, int *ty)
{
  int sx = u.ux - map_width / 2;
  int sy = u.uy - map_height / 2;

  *tx = sx + x / TILE_WIDTH;
  *ty = sy + y / TILE_HEIGHT;
}
