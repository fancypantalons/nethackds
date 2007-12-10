/*
 * Tile management functions.
 */

#include <nds.h>
#include "hack.h"

#include "bmp.h"
#include "ppm-lite.h"
#include "font-bdf.h"

#include "nds_win.h"
#include "nds_util.h"
#include "nds_map.h"
#include "nds_gfx.h"

#define FONT_FILE_NAME          "map.bdf"
#define FONT_PALETTE_NAME       "map.pal"

#define TILE_WIDTH		iflags.wc_tile_width
#define TILE_HEIGHT		iflags.wc_tile_height
#define TILE_FILE		iflags.wc_tile_file

#define MAP_BASE BG_MAP_BASE(8)
#define TILE_BASE BG_TILE_BASE(6)

#define MAX_TILE_SLOTS          512

#define MINIMAP_X               4
#define MINIMAP_Y               16

#define NDS_LOAD_TILE(glyph, x, y) ((TILE_FILE == NULL) ? nds_load_text_tile(glyph, x, y) : nds_load_graphics_tile(glyph, x, y))
#define NDS_INIT_MAP(pal, pallen) ((TILE_FILE == NULL) ? nds_init_text_map(pal, pallen) : nds_init_tiled_map(pal, pallen))

#define ROUND_UP(val) ( (((val) & 0x07) == 0) ? (val) : (((val / 8) + 1) * 8) )

#define c2(a,i)		(RGB15((a[i+2]>>3),(a[i+1]>>3),(a[i]>>3)))

typedef struct {
  u16 tile_ram_idx;   /* Position this tile is in the tile RAM */
  long last_used;     /* This is in game time (moves)          */
} tile_cache_entry_t;

/* Tile-based graphics specific variables */

bmp_t tiles;

int width_in_tiles;
int height_in_tiles;

/* Text-based graphics variables */

struct font *map_font;
struct ppm *text_img;
int font_char_w;
int font_char_h;

/* General rendering variables */

u16 *tile_ram = (u16 *)BG_TILE_RAM(6);
u16 *map_ram = (u16 *)BG_MAP_RAM(8);

tile_cache_entry_t tile_cache[MAX_GLYPH];

int num_free_tiles;

int tile_width;
int tile_height;

int map_width;
int map_height;

int cx, cy;

/* Sprite-specific variables (for tile mode) */

u16 *oam_ram = OAM;
tOAM oam_shadow;

/*
 * Allocate a cache slot for the given glyph.  Note ,this may evict an entry
 * if the cache is full.
 */
int nds_alloc_cache_slot(int glyph)
{
  int tile_idx = -1;
  int req_tiles = tile_width * tile_height;

  /* 
   * We treat the zeroeth group of tiles "special", used for displaying 
   * empty slots on the map.  So we don't allocate past that.
   */
  if ((num_free_tiles - req_tiles) > req_tiles) {
    /*
     * If there's still free tiles, just use the next empty ones.
     */

    num_free_tiles -= req_tiles;
    tile_idx = num_free_tiles;
  } else {
    /*
     * If there are no free slots, we look for the oldest tile and overwrite
     * it.  I'm assuming (fingers crossed) that by the time we run out of tile
     * RAM, there will be low-level enemies, tiles, etc, that'll stop appearing,
     * and so we can safely remove them.
     */

    long last_diff = 0;
    int old_cache_idx;
    int i;

    for (i = 0; i < MAX_GLYPH; i++) {
      if (tile_cache[i].last_used < 0) {
        continue;
      } else if ((moves - tile_cache[i].last_used) > last_diff) {
        tile_idx = tile_cache[i].tile_ram_idx;
        old_cache_idx = i;

        last_diff = moves - tile_cache[i].last_used;
      }
    }

    /* Evict the old entry from the cache */

    tile_cache[old_cache_idx].last_used = -1;
    tile_cache[old_cache_idx].tile_ram_idx = 0;
  }

  /* Now record the new entry */

  tile_cache[glyph].last_used = moves;
  tile_cache[glyph].tile_ram_idx = tile_idx;

  return tile_idx;
}

/*
 * Copy a tile from our tile image, loaded previously, into tile RAM.  In the
 * case of tiles with a width or height larger than 8 pixels, this will result
 * in multiple tile positions being occupied in video RAM.
 */
void nds_load_graphics_tile(int glyph, int gx, int gy) 
{
  int i, j, x;
  int tile_idx = -1;
  int width, height, bpp, row_bytes;
  int bmp_tile_x, bmp_tile_y;
  u8 *bmp_row_start;
  u16 *tile_row_start;

  /*
   * Allocate a tile RAM block for the given glyph.
   */

  tile_idx = nds_alloc_cache_slot(glyph);

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

  bmp_tile_y = glyph2tile[glyph] / width_in_tiles;
  bmp_tile_x = width_in_tiles - glyph2tile[glyph] % width_in_tiles;

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
 * Render the glyph into a tile.
 */
void nds_load_text_tile(int glyph, int gx, int gy) 
{
  int tile_idx = -1;
  int ch, color;
  unsigned int special;
  unsigned char tmp[2] = { 0, 0 };
  int tile_x, y, i;
  u16 *tile_ptr;
  long *img_data;

  /*
   * Allocate a tile RAM block for the given glyph.
   */

  tile_idx = nds_alloc_cache_slot(glyph);

  /* Alright, now convert the glyph to a character */

  mapglyph(glyph, &ch, &color, &special, gx, gy);

  /* Mmm... hacky... */

  if (color == CLR_BLACK) {
    color = CLR_BLUE;
  }

  tmp[0] = ch; // Build a string.

  /* Now draw the character to a PPM image... yes, this is inefficient :) */

  clear_ppm(text_img);

  draw_string(map_font, tmp, text_img, 
              text_img->width / 2 - font_char_w / 2, 
              text_img->height / 2 - font_char_h / 2, 
              1,
              255, 0, 255);

  img_data = (long *)text_img->rgba;
  tile_ptr = tile_ram + tile_idx * 64 / 2;

  /* Now copy the contents of the PPM to tile RAM */

  for (y = 0; y < text_img->height; y++) {
    for (tile_x = 0; tile_x < tile_width; tile_x++) {
      int tile_y = y & 0xF0;
      int tile_row = y & 0x0F;
      u16 *row_ptr = tile_ptr + 
                     ((tile_y * tile_width + tile_x) * 64 +
                      tile_row * 8) / 2;

      for (i = 0; i < 4; i++, img_data += 2) {
        u8 c0 = (img_data[0] ? color : 0) + 1;
        u8 c1 = (img_data[1] ? color : 0) + 1;

        row_ptr[i] = (c1 << 8) |
                     c0;
      }
    }
  }
}

/*
 * Plot the specified tile to the map.  Note, this represents the tile index
 * as present in the BMP file, *not* the tile RAM index.
 */
void nds_draw_tile(nds_map_t *map, int glyph, int x, int y, int gx, int gy)
{
  int midx, tidx;
  int i, j;
  int palette;

  if (glyph < 0) {
    tidx = 0x0000;
  } else if (tile_cache[glyph].tile_ram_idx == 0) {
    NDS_LOAD_TILE(glyph, gx, gy);

    tile_cache[glyph].last_used = moves;
    tidx = tile_cache[glyph].tile_ram_idx; 
  } else {
    tile_cache[glyph].last_used = moves;
    tidx = tile_cache[glyph].tile_ram_idx; 
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

  if (TILE_FILE == NULL) {
    palette = (iflags.cursor && (gx == map->cx) && (gy == map->cy)) ? 3 : 2;
  } else {
    palette = 2;
  }

  midx = (y * tile_height) * 32 + x * tile_width;

  for (j = 0; j < tile_height; j++, midx += 32) {
    for (i = 0; i < tile_width; i++) {
      map_ram[midx + i] = tidx | (palette << 12); 

      if (tidx > 0) {
        tidx++;
      }
    }
  }
}

/*
 * Initialize the graphical tile map.
 */
int nds_init_tiled_map(u16 *palette, int *pallen)
{
  char *fname = TILE_FILE;
  int i;

  tile_width = TILE_WIDTH / 8;
  tile_height = TILE_HEIGHT / 8;

  /* Now load the tiles into memory */

  if (bmp_read(fname, &tiles) < 0) {
    return -1;
  }

  /* Compute the width and height of our image, in tiles */

  width_in_tiles = bmp_width(&tiles) / TILE_WIDTH;
  height_in_tiles = bmp_height(&tiles) / TILE_HEIGHT;

  /* Alright, file loaded, let's copy over the palette */

  *pallen = tiles.palette_length;

  for (i = 0; i < tiles.palette_length; i++) {
    u16 val = RGB15((tiles.palette[i].r >> 3),
                    (tiles.palette[i].g >> 3),
                    (tiles.palette[i].b >> 3));

    palette[i] = val;
  }

  return bmp_bpp(&tiles);
}

/*
 * Initialize the textual map.
 */
int nds_init_text_map(u16 *palette, int *pallen)
{
  FILE *pfile;
  int ret;
  int img_w, img_h;
  u8 rgb[64];
  int i, j;

  if ((map_font = read_bdf(FONT_FILE_NAME)) == NULL) {
    iprintf("Unable to open '%s'\n", FONT_FILE_NAME);

    return -1;
  }

  if ((pfile = fopen(FONT_PALETTE_NAME, "r")) == NULL) {
    iprintf("Unable to open '%s'\n", FONT_PALETTE_NAME);

    return -1;
  }

  ret = fread(rgb, 1, sizeof(rgb), pfile);
  fclose(pfile);

  if (ret < sizeof(rgb)) {
    iprintf("Short read loading text palette (got %d, wanted %d)\n",
            ret, sizeof(rgb));
    return -1;
  }

  /* Translate to 16-color NDS palette */

  for (i = 0, j = 1; i < sizeof(rgb); i += 4, j++) {
    palette[j] = RGB15((rgb[i + 2] >> 3),
                       (rgb[i + 1] >> 3),
                       (rgb[i + 0] >> 3));

    palette[j + 256] = palette[j] ^ 0x7FFF;
  }

  *pallen = 512;

  /* Now figure out our dimensions */

  text_dims(map_font, "#", &font_char_w, &font_char_h);

  img_w = ROUND_UP(font_char_w);
  img_h = ROUND_UP(font_char_h);

  text_img = alloc_ppm(img_w, img_h);

  tile_width = img_w / 8;
  tile_height = img_h / 8;

  /* Lastly, create out "blank" tile */

  for (i = 0; i < tile_width * tile_height * 32; i++) {
    tile_ram[i] = 0x0101;
  }

  return 8;
}

/*
 * Get the user sprite set up and drawn.
 */
void nds_draw_graphics_cursor(int x, int y)
{
  if ((x < 0) || (y < 0) || (x > map_width) || (y > map_height) ||
      (! iflags.cursor)) {
    oam_shadow.spriteBuffer[0].isHidden = 1;

    return;
  } 

  oam_shadow.spriteBuffer[0].posX = x * TILE_WIDTH;
  oam_shadow.spriteBuffer[0].posY = y * TILE_HEIGHT;
  oam_shadow.spriteBuffer[0].isHidden = 0;
}

void nds_init_sprite(int bpp)
{
  int dim = (TILE_WIDTH > TILE_HEIGHT) ? TILE_WIDTH : TILE_HEIGHT;
  u16 *spr_palette = SPRITE_PALETTE;
  u16 *spr_gfx_ram = SPRITE_GFX;
  int i, x, y;
  int spr_size;

  /* First thing's f'ing last, let's disable all sprites */

  for (i = 0; i < 128; i++) {
    oam_shadow.spriteBuffer[i].isHidden = 1;
  }

  switch (dim) {
    case 8:
      oam_shadow.spriteBuffer[0].objSize = OBJSIZE_8;
      spr_size = 1;
      break;

    case 16:
      oam_shadow.spriteBuffer[0].objSize = OBJSIZE_16;
      spr_size = 2;
      break;

    case 24:
    case 32:
      oam_shadow.spriteBuffer[0].objSize = OBJSIZE_32;
      spr_size = 4;
      break;

    case 48:
    case 64:
      oam_shadow.spriteBuffer[0].objSize = OBJSIZE_64;
      spr_size = 8;
      break;

    default:
      break;
  }
  
  switch (bpp) {
    case 4:
      oam_shadow.spriteBuffer[0].colMode = OBJCOLOR_16;
      break;

    case 8:
      oam_shadow.spriteBuffer[0].colMode = OBJCOLOR_256;
      break;

    default:
      break;
  }

  oam_shadow.spriteBuffer[0].isRotoscale = 0;
  oam_shadow.spriteBuffer[0].rsDouble = 0;
  oam_shadow.spriteBuffer[0].objMode = OBJMODE_BLENDED;
  oam_shadow.spriteBuffer[0].isMosaic = 0;
  oam_shadow.spriteBuffer[0].objShape = OBJSHAPE_SQUARE;

  oam_shadow.spriteBuffer[0].posX = 0;
  oam_shadow.spriteBuffer[0].posY = 0;
  oam_shadow.spriteBuffer[0].tileIdx = spr_size * spr_size * (bpp / 4);
  oam_shadow.spriteBuffer[0].objPriority = OBJPRIORITY_3;
  oam_shadow.spriteBuffer[0].objPal = 0;
  
  /* Let's draw our highlight thinger */

  spr_palette[1] = RGB15(31, 31, 31);

  spr_gfx_ram += spr_size * spr_size * bpp * 8 / 2;

  for (y = 0; y < TILE_HEIGHT; y++) {
    for (x = 0; x < TILE_WIDTH; x++) {
      int tidx;
      u16 *tptr;

      int tx = x & 0x07;
      int ty = y & 0x07;

      if ((x != 0) && (x != (TILE_WIDTH - 1)) && 
          (y != 0) && (y != (TILE_HEIGHT - 1))) {
        continue;
      }

      tidx = (y / 8) * spr_size + x / 8;
      tptr = spr_gfx_ram + (tidx * bpp * 8) / 2 + ((ty * 8 + tx) / (8 / bpp)) / 2;

      switch (bpp) {
        case 4:
          *tptr |= 0x01 << ((x & 3) << 2);

          break;

        case 8:
          *tptr |= 0x01 << ((x & 1) << 3);

          break;

        default:
          break;
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
  int i;
  u16 blend_dst;

  int bpp;
  u16 palette_data[512];
  int palette_length;

  /* 
   * Alright, load the tile file or font data, depending on the rendering
   * mode.
   */

  if ((bpp = NDS_INIT_MAP(palette_data, &palette_length)) < 0) {
    return -1;
  }

  /* Initialize the data we need to manage the map and tiles */

  memset(tile_cache, 0, sizeof(tile_cache));

  map_width = 32 / tile_width;
  map_height = 24 / tile_height;

  num_free_tiles = MAX_TILE_SLOTS - 1;

  for (i = 0; i < MAX_GLYPH; i++) {
    tile_cache[i].last_used = -1;
  }

  /* Now initialize our graphics layer */

  /*
   * Set up the tile RAM starting at 16k.  At 65k, the bitmap data starts
   * for the menu layer, so we stick the map RAM 2k before that, 
   * which maximizes the amount of tile RAM we have.
   */

  switch (bpp) {
    case 4:
      BG1_CR = BG_32x32 | MAP_BASE | TILE_BASE | BG_16_COLOR | BG_PRIORITY_3; 
      DISPLAY_CR |= DISPLAY_BG1_ACTIVE;

      blend_dst = BLEND_DST_BG1;

      palette = (u16 *)BG_PALETTE + 32;
      break;

    case 8:
      BG3_CR = BG_RS_32x32 | MAP_BASE | TILE_BASE | BG_PRIORITY_3; 
      DISPLAY_CR |= DISPLAY_BG3_ACTIVE;

      BG3_XDX = 1 << 8;
      BG3_XDY = 0;
      BG3_YDX = 0;
      BG3_YDY = 1 << 8;

      blend_dst = BLEND_DST_BG3;

      vramSetBankE(VRAM_E_LCD);
      palette = VRAM_E_EXT_PALETTE[3][2];

      break;

    default:
      iprintf("Sorry, %d bpp tile files aren't supported.\n", bmp_bpp(&tiles));

      return -1;
  }

  BLEND_CR = BLEND_ALPHA | BLEND_SRC_SPRITE | blend_dst;
  BLEND_AB = 0x0010;

  /* Alright, time to copy over the palette data. */

  for (i = 0; i < palette_length; i++) {
    palette[i] = palette_data[i];
  }

  /* If we're using extended palettes, get the VRAM set up. */

  if (bpp == 8) {
    vramSetBankE(VRAM_E_BG_EXT_PALETTE);
  }

  if (TILE_FILE != NULL) {
    nds_init_sprite(bpp);
  }

  return 0;
}

void nds_clear_map()
{
  memset(map_ram, 0, 32 * 24 * 2);
}

void nds_draw_minimap(nds_map_t *map)
{
  u16 *sub_vram = (u16 *)BG_BMP_RAM_SUB(4);
  int x, y;
  int rx1, rx2, ry1, ry2;

  /* Clear the edges of the map */

  rx1 = MINIMAP_X - 1;
  ry1 = MINIMAP_Y - 1;
  rx2 = MINIMAP_X + COLNO * 2;
  ry2 = MINIMAP_Y + ROWNO * 2;

  nds_draw_hline(rx1, ry1, COLNO * 2 + 1, 0, sub_vram);
  nds_draw_hline(rx1, ry2, COLNO * 2 + 1, 0, sub_vram);

  nds_draw_vline(rx1, ry1, ROWNO * 2 + 1, 0, sub_vram);
  nds_draw_vline(rx2, ry1, ROWNO * 2 + 1, 0, sub_vram);

  for (y = 0; y < ROWNO; y++) {
    for (x = 0; x < COLNO; x++) {
      if (map->glyphs[y][x] < 0) {
        sub_vram[(y + MINIMAP_Y / 2) * 256 + x + MINIMAP_X / 2] = 0x0000;
        sub_vram[(y + MINIMAP_Y / 2) * 256 + x + MINIMAP_X / 2 + 128] = 0x0000;
      } else {
        sub_vram[(y + MINIMAP_Y / 2) * 256 + x + MINIMAP_X / 2] = 0xFFFF;
        sub_vram[(y + MINIMAP_Y / 2) * 256 + x + MINIMAP_X / 2 + 128] = 0xFFFF;
      }
    }
  }

  sub_vram[(u.uy + MINIMAP_Y / 2) * 256 + u.ux + MINIMAP_X / 2] = 0xFCFC;
  sub_vram[(u.uy + MINIMAP_Y / 2) * 256 + u.ux + MINIMAP_X / 2 + 128] = 0xFCFC;

  rx1 = cx * 2 - map_width + MINIMAP_X - 1;
  rx2 = cx * 2 + map_width + MINIMAP_X;

  ry1 = cy * 2 - map_height + MINIMAP_Y - 1;
  ry2 = cy * 2 + map_height + MINIMAP_Y;

  nds_draw_hline(rx1, ry1, map_width * 2 + 1, 253, sub_vram);
  nds_draw_hline(rx1, ry2, map_width * 2 + 1, 253, sub_vram);
  nds_draw_vline(rx1, ry1, map_height * 2 + 1, 253, sub_vram);
  nds_draw_vline(rx2, ry1, map_height * 2 + 1, 253, sub_vram);
}

void nds_clear_minimap()
{
  u16 *sub_vram = (u16 *)BG_BMP_RAM_SUB(4);
  int x, y;
  int rx1, rx2, ry1, ry2;

  for (y = 0; y < ROWNO; y++) {
    for (x = 0; x < COLNO; x++) {
      sub_vram[(y + MINIMAP_Y) * 256 + x + MINIMAP_X] = 0x0000;
      sub_vram[(y + MINIMAP_Y) * 256 + x + MINIMAP_X + 128] = 0x0000;
    }
  }

  rx1 = MINIMAP_X - 2;
  ry1 = MINIMAP_Y - 2;
  rx2 = MINIMAP_X + COLNO * 2 + 1;
  ry2 = MINIMAP_Y + ROWNO * 2 + 1;

  nds_draw_hline(rx1, ry1, COLNO * 2 + 3, 255, sub_vram);
  nds_draw_hline(rx1, ry2, COLNO * 2 + 3, 255, sub_vram);

  nds_draw_vline(rx1, ry1, ROWNO * 2 + 3, 255, sub_vram);
  nds_draw_vline(rx2, ry1, ROWNO * 2 + 3, 255, sub_vram);
}

void nds_draw_map(nds_map_t *map, int *xp, int *yp)
{
  int sx, sy;

  swiWaitForVBlank();

  if (xp == NULL) {
    cx = u.ux;
  } else {
    cx = *xp;
  }
  
  if (yp == NULL) {
    cy = u.uy;
  } else {
    cy = *yp;
  }

  if ((cx + map_width / 2) > COLNO) {
    cx = COLNO - map_width / 2;
  } else if ((cx - map_width / 2) < 0) {
    cx = map_width / 2;
  }

  if ((cy + map_height / 2) > ROWNO) {
    cy = ROWNO - map_height / 2;
  } else if ((cy - map_height / 2) < 0) {
    cy = map_height / 2;
  }

  sx = cx - map_width / 2;
  sy = cy - map_height / 2;

  if (map != NULL) {
    int x, y;

    for (y = 0; y < map_height; y++) {
      for (x = 0; x < map_width; x++) {
        if (((sx + x) < 0) || ((sx + x) >= COLNO) ||
            ((sy + y) < 0) || ((sy + y) >= ROWNO)) {
          nds_draw_tile(map, -1, x, y, sx + x, sy + y);
        } else {
          nds_draw_tile(map, map->glyphs[sy + y][sx + x], x, y, sx + x, sy + y);
        }
      }
    }

    if (TILE_FILE != NULL) {
      nds_draw_graphics_cursor(map->cx - sx, map->cy - sy);
    }

    nds_draw_minimap(map);
  } else {
    nds_draw_graphics_cursor(-1, -1);

    nds_clear_map();
    nds_clear_minimap();
  }

  DC_FlushAll();
  dmaCopy(&oam_shadow, oam_ram, sizeof(oam_shadow));
}

void nds_map_translate_coords(int x, int y, int *tx, int *ty)
{
  int sx = cx - map_width / 2;
  int sy = cy - map_height / 2;

  *tx = sx + x / (tile_width * 8);
  *ty = sy + y / (tile_height * 8);
}

void nds_map_get_center(int *xp, int *yp)
{
  *xp = cx;
  *yp = cy;
}

void nds_map_relativize(int *px, int *py)
{
  int upx, upy;
  int sx = cx - map_width / 2;
  int sy = cy - map_height / 2;
  int t_width = tile_width * 8;
  int t_height = tile_height * 8;

  upx = (u.ux - sx) * t_width + t_width / 2;
  upy = (u.uy - sy) * t_height + t_height / 2;

  iprintf("%d %d\n", upx, upy);

  *px -= upx;
  *py -= upy;
}
