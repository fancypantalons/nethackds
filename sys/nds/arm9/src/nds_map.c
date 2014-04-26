/*
 * Tile management functions.
 */

#include <nds.h>
#include "hack.h"

#include "bmp.h"
#include "ppm-lite.h"
#include "font-bdf.h"

#include "nds_debug.h"
#include "nds_win.h"
#include "nds_util.h"
#include "nds_map.h"
#include "nds_gfx.h"

#define FONT_FILE_NAME          "map.bdf"
#define FONT_PALETTE_NAME       "map.pal"

#define MAP_PALETTE_BASE        32

#define TILE_WIDTH		iflags.wc_tile_width
#define TILE_HEIGHT		iflags.wc_tile_height
#define TILE_FILE		iflags.wc_tile_file

#define MAP_BASE BG_MAP_BASE(8)
#define TILE_BASE BG_TILE_BASE(6)

#define MAX_TILE_SLOTS          512

#define MINIMAP_X               4

#define NDS_LOAD_TILE(tile_idx, coords) ((TILE_FILE == NULL) ? nds_load_text_tile(tile_idx, coords) : nds_load_graphics_tile(tile_idx, coords))
#define NDS_INIT_MAP(pal, pallen) ((TILE_FILE == NULL) ? nds_init_text_map(pal, pallen) : nds_init_tiled_map(pal, pallen))

#define ROUND_UP(val) ( (((val) & 0x07) == 0) ? (val) : (((val / 8) + 1) * 8) )

#define c2(a,i)		(RGB15((a[i+2]>>3),(a[i+1]>>3),(a[i]>>3)))

#define CACHE_SLOT_TO_TILE_INDEX(c) ((c + 1) * tile_width_in_tiles * tile_height_in_tiles)

/* The map */

nds_map_t *map;

/* Tile-based graphics specific variables */

bmp_t tiles;

int width_in_tiles;
int height_in_tiles;
int tile_width_in_tiles, tile_height_in_tiles;
int tile_width_in_px, tile_height_in_px;

/* Text-based graphics variables */

struct font *map_font;
struct ppm *text_img;
int font_char_w;
int font_char_h;

/* General rendering variables */

u16 *tile_ram = (u16 *)BG_TILE_RAM(6);
u16 *map_ram = (u16 *)BG_MAP_RAM(8);

rectangle_t map_dimensions = {
  .start = { .x = 0, .y = 0 },
  .dims = { .width = COLNO, .height = ROWNO }
};

int pet_count;

/*
 * Initialize our tile cache.
 */
void nds_init_tile_cache()
{
  int i;

  map->num_cache_entries = (MAX_TILE_SLOTS - 1) / (tile_width_in_tiles * tile_height_in_tiles);

  map->tile_cache = (tile_cache_entry_t *)malloc(sizeof(tile_cache_entry_t) * map->num_cache_entries);

  for (i = 0; i < map->num_cache_entries; i++) {
    map->tile_cache[i].glyph = -1;
  }
}

/*
 * Returns the cache slot number for the given glyph, if one exists.
 */
int nds_find_cache_slot(coord_t coords)
{
  int glyph = map->glyphs[coords.y][coords.x];
  int i;
  int ch, colour;
  unsigned int special;
  int tile = glyph2tile[glyph];

  mapglyph(glyph, &ch, &colour, &special, coords.x, coords.y);

  for (i = 0; i < map->num_cache_entries; i++) {
    if ((map->tile_cache[i].glyph == glyph) && (map->tile_cache[i].colour == colour) && 
        (map->tile_cache[i].special == special) && (map->tile_cache[i].ch == ch) &&
        (map->tile_cache[i].tile == tile)) {
      return i;
    }
  }

  return -1;
}

/*
 * Allocate a cache slot for the given glyph.  Note, this may evict an entry
 * if the cache is full.
 */
int nds_allocate_cache_slot(coord_t coords)
{ 
  int glyph = map->glyphs[coords.y][coords.x];
  int cache_slot = -1;
  int i;

  int oldest_diff = 0;
  int oldest;

  int ch, colour;
  unsigned int special;

  for (i = 0; i < map->num_cache_entries; i++) {
    int diff;

    if (map->tile_cache[i].glyph < 0) {
      cache_slot = i;

      break;
    } else if (map->tile_cache[i].last_used < 0) {
      continue;
    }

    diff = moves - map->tile_cache[i].last_used;

    if (diff > oldest_diff) {
      oldest = i;
      oldest_diff = diff;
    }
  }

  if (cache_slot < 0) {
    cache_slot = oldest;
  }

  mapglyph(glyph, &ch, &colour, &special, coords.x, coords.y);

  map->tile_cache[cache_slot].glyph = glyph;
  map->tile_cache[cache_slot].colour = colour;
  map->tile_cache[cache_slot].special = special;
  map->tile_cache[cache_slot].ch = ch;
  map->tile_cache[cache_slot].tile = glyph2tile[glyph];

  return cache_slot;
}

/*
 * Copy a tile from our tile image, loaded previously, into tile RAM.  In the
 * case of tiles with a width or height larger than 8 pixels, this will result
 * in multiple tile positions being occupied in video RAM.
 */
void nds_load_graphics_tile(int tile_idx, coord_t coords) 
{
  int glyph = map->glyphs[coords.y][coords.x];
  int i, j, x;
  int width, height, bpp, row_bytes;
  int bmp_tile_x, bmp_tile_y;
  u8 *bmp_row_start;
  u16 *tile_row_start;

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

  row_bytes = tile_width_in_tiles * bpp;

  /* Now calculate the pointer which points to the start of the BMP row */

  bmp_tile_y = glyph2tile[glyph] / width_in_tiles;
  bmp_tile_x = width_in_tiles - glyph2tile[glyph] % width_in_tiles;

  bmp_row_start = tiles.bitmap + tiles.bitmap_length - 
                  bmp_tile_y * TILE_HEIGHT * width_in_tiles * row_bytes - 
                  bmp_tile_x * row_bytes ;

  for (j = 0; j < tile_height_in_tiles; j++) {
    int y;

    for (y = 0; y < 8; y++) {
      for (i = 0; i < tile_width_in_tiles; i++) {
        tile_row_start = tile_ram + (tile_idx + j * tile_width_in_tiles + i) * bpp * 8 / 2 + y * bpp / 2; 

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
void nds_load_text_tile(int tile_idx, coord_t coords) 
{
  int glyph = map->glyphs[coords.y][coords.x];
  int ch, color;
  unsigned int special;
  char tmp[BUFSZ];
  int tile_x, y, i;
  u16 *tile_ptr;
  unsigned char *img_data;
  int black = MAP_COLOUR(CLR_BLACK);

  /* Alright, now convert the glyph to a character */

  mapglyph(glyph, &ch, &color, &special, coords.x, coords.y);

  /* Mmm... hacky... */

  if (color == CLR_BLACK) {
    color = CLR_BLUE;
  }

  if (((special & MG_PET) && iflags.hilite_pet) ||
      ((special & MG_DETECT) && iflags.use_inverse)) {
    sprintf(tmp, "\e[7m%c", ch);
  } else {
    sprintf(tmp, "%c", ch);
  }

  /* Now draw the character to a PPM image... yes, this is inefficient :) */

  clear_ppm(text_img, black);

  draw_string(map_font, tmp, text_img, 
              text_img->width / 2 - font_char_w / 2, 
              text_img->height / 2 - font_char_h / 2, 
              -1, -1);

  img_data = (unsigned char *)text_img->bitmap;
  tile_ptr = tile_ram + tile_idx * 64 / 2;

  /* Now copy the contents of the PPM to tile RAM */

  for (y = 0; y < text_img->height; y++) {
    for (tile_x = 0; tile_x < tile_width_in_tiles; tile_x++) {
      int tile_y = y & 0xF0;
      int tile_row = y & 0x0F;
      u16 *row_ptr = tile_ptr + 
                     ((tile_y * tile_width_in_tiles + tile_x) * 64 +
                      tile_row * 8) / 2;

      for (i = 0; i < 4; i++, img_data += 2) {
        u8 c0 = ((img_data[0] != black) ? color : 0) + 1;
        u8 c1 = ((img_data[1] != black) ? color : 0) + 1;

        row_ptr[i] = (c1 << 8) |
                     c0;
      }
    }
  }
}

int nds_load_tile(coord_t coords)
{
  int cache_slot;
  int tile_idx;

  if ((cache_slot = nds_find_cache_slot(coords)) < 0) {
    cache_slot = nds_allocate_cache_slot(coords);
    tile_idx = CACHE_SLOT_TO_TILE_INDEX(cache_slot);

    NDS_LOAD_TILE(tile_idx, coords);
  } else {
    tile_idx = CACHE_SLOT_TO_TILE_INDEX(cache_slot);
  }

  map->tile_cache[cache_slot].last_used = moves;

  return tile_idx;
}

/*
 * Plot the specified tile to the map.  Note, this represents the tile index
 * as present in the BMP file, *not* the tile RAM index.
 */
void nds_draw_tile(coord_t coords)
{
  int glyph = map->glyphs[coords.y][coords.x];
  int midx, tidx;
  int i, j;
  int palette;

  if (glyph < 0) {
    tidx = 0x0000;
  } else {
    tidx = nds_load_tile(coords);
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
    switch (iflags.cursor) {
      case 0:
        palette = COORDS_ARE_EQUAL(coords, map->cursor) ? 3 : 2;
        break;

      case 1:
        palette = (((coords.x != u.ux) || (coords.y != u.uy)) && COORDS_ARE_EQUAL(coords, map->cursor)) ? 3 : 2;
        break;

      case 2:
        palette = 2;
        break;

      default:
        break;
    }
  } else {
    palette = 2;
  }

  coord_t screen_tile_coords = coord_subtract(coords, map->viewport.start);

  midx = screen_tile_coords.y * tile_height_in_tiles * 32 + screen_tile_coords.x * tile_width_in_tiles;

  for (j = 0; j < tile_height_in_tiles; j++, midx += 32) {
    for (i = 0; i < tile_width_in_tiles; i++) {
      map_ram[midx + i] = tidx | (palette << 12); 

      if (tidx > 0) {
        tidx++;
      }
    }
  }

  /* 
   * Yeah, this is a weird place for this, BUT... here we put in a sprite
   * cursor if this is a pet and we're in tile mode.
   */
  if ((TILE_FILE != NULL) && iflags.hilite_pet) {
    int ch, color;
    unsigned int special;

    mapglyph(glyph, &ch, &color, &special, coords.x, coords.y);

    /*
    if (special & MG_PET) {
      oam_shadow.oamBuffer[++pet_count].x = x * tile_width_px;
      oam_shadow.oamBuffer[pet_count].y = y * tile_height_px;
      oam_shadow.oamBuffer[pet_count].isHidden = 0;
    }
    */
  }
}

/*
 * Initialize the graphical tile map.
 */
int nds_init_tiled_map(u16 *palette, int *pallen)
{
  char *fname = TILE_FILE;
  int i;

  tile_width_in_px = TILE_WIDTH;
  tile_width_in_tiles = TILE_WIDTH / 8;

  tile_height_in_px = TILE_HEIGHT;
  tile_height_in_tiles = TILE_HEIGHT / 8;

  /* Now load the tiles into memory */

  if (bmp_read(fname, &tiles) < 0) {
    return -1;
  }

  /* Compute the width and height of our image, in tiles */

  width_in_tiles = bmp_width(&tiles) / tile_width_in_px;
  height_in_tiles = bmp_height(&tiles) / tile_height_in_px;

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
  int img_w, img_h;
  int i;
  int palcnt;

  if ((map_font = read_bdf(FONT_FILE_NAME)) == NULL) {
    DEBUG_PRINT("Unable to open '%s'\n", FONT_FILE_NAME);

    return -1;
  }

  if ((palcnt = nds_load_palette(FONT_PALETTE_NAME, palette + 1)) < 0) {
    return -1;
  }

  /* Generate our inverse palette */

  for (i = 1; i < palcnt + 1; i++) {
    palette[i + 256] = palette[i] ^ 0x7FFF;
  }

  *pallen = 512;

  /* Now figure out our dimensions */

  text_dims(map_font, "#", &font_char_w, &font_char_h);

  img_w = ROUND_UP(font_char_w);
  img_h = ROUND_UP(font_char_h);

  text_img = alloc_ppm(img_w, img_h);

  tile_width_in_px = img_w;
  tile_width_in_tiles = img_w / 8;

  tile_height_in_px = img_h;
  tile_height_in_tiles = img_h / 8;

  /* Lastly, create out "blank" tile */

  for (i = 0; i < tile_width_in_tiles * tile_height_in_tiles * 32; i++) {
    tile_ram[i] = 0x0101;
  }

  return 8;
}

/*
 * Get the user sprite set up and drawn.
 */
void nds_clear_sprites()
{
  int i;

  for (i = 0; i < map->sprite_count; i++) {
    map->sprites[i].hidden = true;
  }
}

void nds_draw_sprites()
{
  int i;

  for (i = 0; i < map->sprite_count; i++) {
    sprite_t *sprite = &(map->sprites[i]);

    oamSet(
      &oamMain,                // Video subsystem to use
      sprite->index,           // The index of the allocate sprite
      sprite->coords.x,        // Our coordinates
      sprite->coords.y,            
      2,                       // Sprite priority
      0,                       // Alpha value for sprite
      SpriteSize_64x64,        // Obviously the size
      SpriteColorFormat_Bmp,   // and format
      sprite->gfx,             // Pointer to our graphics memory
      0,                       // Rotation index
      false,                   // Don't double size of rotated sprites
      sprite->hidden,          // Whether or not this sprite is hidden
      false, false,            // Horizontal and vertical flip
      false                    // Mosaic
    );
  }

  oamUpdate(&oamMain);
}

void nds_set_graphics_cursor(coord_t coords)
{
  if (! POINT_IN_RECT(coords, map->viewport) ||
      ! iflags.cursor) {

    return;
  } 

  map->sprites[0].coords = coords;
  map->sprites[0].hidden = false;
}

void nds_highlight_tile(coord_t coords)
{
  if (! POINT_IN_RECT(coords, map->viewport)) {
    return;
  } 

  map->sprites[1].coords = coords;
  map->sprites[1].hidden = false;
}

void nds_clear_highlight(int x, int y)
{
  map->sprites[1].hidden = true;
}

void nds_render_cursor(int sprite_num, int r, int g, int b)
{
  map->sprites[sprite_num].gfx = oamAllocateGfx(&oamMain, SpriteSize_64x64, SpriteColorFormat_Bmp);
  map->sprites[sprite_num].index = map->sprite_count++;

  int x, y;

  for (y = 0; y < tile_height_in_px; y++) {
    for (x = 0; x < tile_width_in_px; x++) {
      if ((x != 0) && (x != (tile_width_in_px - 1)) && 
          (y != 0) && (y != (tile_height_in_px - 1))) {
        continue;
      }

      if ( ((x == 0) || (x == (tile_width_in_px - 1))) &&
           ((y > 2) && (y < (tile_height_in_px - 3))) ) {
        continue;
      }

      if ( ((y == 0) || (y == (tile_width_in_px - 1))) &&
           ((x > 2) && (x < (tile_height_in_px - 3))) ) {
        continue;
      }

      u16 *tptr = map->sprites[sprite_num].gfx + y * 64 + x;

      *tptr = ARGB16(1, r, g, b);
    }
  }
}

void nds_render_highlighter(int sprite_num, int r, int g, int b)
{
  map->sprites[sprite_num].gfx = oamAllocateGfx(&oamMain, SpriteSize_64x64, SpriteColorFormat_Bmp);
  map->sprites[sprite_num].index = map->sprite_count++;

  int x, y;

  for (y = 0; y < tile_height_in_px; y++) {
    for (x = 0; x < tile_width_in_px; x++) {
      u16 *tptr = map->sprites[sprite_num].gfx + y * 64 + x;

      *tptr = ARGB16(1, r, g, b);
    }
  }
}

void nds_init_sprite(int bpp)
{
  /* First, initialize the sprite subsystem */

  oamInit(&oamMain, SpriteMapping_Bmp_2D_256, false);
  
  /* Let's draw our highlight thinger */

  nds_render_cursor(0, 63, 63, 63);
  //nds_render_cursor(1, 31, 0, 0);
  nds_render_highlighter(1, 0, 0, 63);
}

/*
 * Here we do two things.  First, we load the BMP into memory.  We throw an
 * error if it isn't an indexed file of some kind (bpp <= 8).  After the file 
 * is loaded, we populate the palette provided.  This involves converting the 
 * 24-bit RGB tuplets to 15-bit NDS palette entries.
 */
int nds_init_map()
{
  u16 *palette;
  int i, x, y;

  int bpp;
  u16 palette_data[512];
  int palette_length;

  map = (nds_map_t *)malloc(sizeof(nds_map_t));
  memset(map, 0, sizeof(nds_map_t));

  /* 
   * Alright, load the tile file or font data, depending on the rendering
   * mode.
   */

  if ((bpp = NDS_INIT_MAP(palette_data, &palette_length)) < 0) {
    return -1;
  }

  /* Initialize the data we need to manage the map and tiles */

  nds_clear_map();
  nds_init_tile_cache();

  map->viewport.dims.width = 32 / tile_width_in_tiles; 
  map->viewport.dims.height = 24 / tile_height_in_tiles;

  /* Now initialize our graphics layer */

  /*
   * Set up the tile RAM starting at 16k.  At 65k, the bitmap data starts
   * for the menu layer, so we stick the map RAM 2k before that, 
   * which maximizes the amount of tile RAM we have.
   */

  switch (bpp) {
    case 4:
      REG_BG1CNT = BG_32x32 | MAP_BASE | TILE_BASE | BG_COLOR_16 | BG_PRIORITY_3; 
      REG_DISPCNT |= DISPLAY_BG1_ACTIVE;

      palette = (u16 *)BG_PALETTE + MAP_PALETTE_BASE;
      break;

    case 8:
      REG_BG3CNT = BG_RS_32x32 | MAP_BASE | TILE_BASE | BG_PRIORITY_3; 
      REG_DISPCNT |= DISPLAY_BG3_ACTIVE;

      REG_BG3PA = 1 << 8;
      REG_BG3PB = 0;
      REG_BG3PC = 0;
      REG_BG3PD = 1 << 8;

      vramSetBankE(VRAM_E_LCD);
      palette = VRAM_E_EXT_PALETTE[3][2];

      break;

    default:
      DEBUG_PRINT("Sorry, %d bpp tile files aren't supported.\n", bmp_bpp(&tiles));

      return -1;
  }

  /* Alright, time to copy over the palette data. */

  for (i = 0; i < palette_length; i++) {
    palette[i] = palette_data[i];
  }

  /* If we're using extended palettes, get the VRAM set up. */

  if (bpp == 8) {
    vramSetBankE(VRAM_E_BG_EXT_PALETTE);
  }

  nds_init_sprite(bpp);

  return 0;
}

nds_map_t *nds_get_map()
{
  return map;
}

void nds_clear_map()
{
  memset(map_ram, 0, 32 * 24 * 2);

  if (map != NULL) {
    int x, y;

    for (y = 0; y < ROWNO; y++) {
      for (x = 0; x < COLNO; x++) {
        map->glyphs[y][x] = -1;
      }
    }
  }
}

rectangle_t nds_minimap_dims()
{
  int minimap_x = MINIMAP_X;
  int minimap_y = system_font->height + 2;
  rectangle_t res;

  res.start.x = minimap_x - 1;
  res.start.y = minimap_y - 1;
  res.dims.width = COLNO * 2 + 1;
  res.dims.height = ROWNO * 2 + 1;

  return res;
}

void nds_draw_minimap()
{
  u16 *sub_vram = (u16 *)BG_BMP_RAM_SUB(4);
  int x, y;
  rectangle_t dims = nds_minimap_dims();

  int rx1, ry1, rx2, ry2;

  rx1 = dims.start.x;
  ry1 = dims.start.y;
  rx2 = RECT_END_X(dims);
  ry2 = RECT_END_Y(dims);

  for (y = 0; y < ROWNO; y++) {
    for (x = 0; x < COLNO; x++) {
      int glyph = map->glyphs[y][x];
      int typ = levl[x][y].typ;
      u16 colour;

      if (glyph < 0) {
        colour = 0x00;
      } else if ((x == u.ux) && (y == u.uy)) {
        colour = C_YOU;
      } else if (glyph_is_normal_monster(glyph)) {
        colour = C_MON; 
      } else if (glyph_is_pet(glyph)) {
        colour = C_PET;
      } else if (IS_WALL(typ)) {
        colour = C_WALL;
      } else if (IS_STWALL(typ)) {
        colour = 0x00;
      } else if (IS_DOOR(typ)) {
        colour = C_DOOR;
      } else if (typ == STAIRS) {
        colour = C_STAIRS;
      } else if (typ == ALTAR) {
        colour = C_ALTAR;
      } else if (typ == CORR) {
        colour = C_CORR;
      } else if (IS_FURNITURE(typ)) {
        colour = C_FURNITURE;
      } else if (IS_ROOM(typ)) {
        colour = C_ROOM;
      } else {
        colour = C_WALL;
      }

      sub_vram[(y + (ry1 + 1) / 2) * 256 + x + (rx1 + 1) / 2] = (colour << 8) | colour;
      sub_vram[(y + (ry1 + 1) / 2) * 256 + x + (rx1 + 1) / 2 + 128] = (colour << 8) | colour;
    }
  }

  nds_draw_rect_outline(dims, C_MAPBORDER, sub_vram);

  int width = map->viewport.dims.width;
  int height = map->viewport.dims.height;

  rectangle_t visible_rect = {
    .start = { .x = map->center.x * 2 - width + rx1, .y = map->center.y * 2 - height + ry1 },
    .dims = { .width = width * 2 + 1, .height = height * 2 + 1 }
  };

  nds_draw_rect_outline(visible_rect, C_VISBORDER, sub_vram);
}

void nds_clear_minimap()
{
  u16 *sub_vram = (u16 *)BG_BMP_RAM_SUB(4);
  int x, y;
  rectangle_t map_rect = nds_minimap_dims();

  for (y = 0; y < ROWNO; y++) {
    for (x = 0; x < COLNO; x++) {
      sub_vram[(y + map_rect.start.y + 1) * 256 + x + map_rect.start.x + 1] = 0x0000;
      sub_vram[(y + map_rect.start.y + 1) * 256 + x + map_rect.start.x + 1 + 128] = 0x0000;
    }
  }

  nds_draw_hline(map_rect.start.x - 1, map_rect.start.y - 1, COLNO * 2 + 3, C_MAPBORDER, sub_vram);
  nds_draw_hline(map_rect.start.x - 1, RECT_END_Y(map_rect) + 1, COLNO * 2 + 3, C_MAPBORDER, sub_vram);

  nds_draw_vline(map_rect.start.x - 1, map_rect.start.y - 1, ROWNO * 2 + 3, C_MAPBORDER, sub_vram);
  nds_draw_vline(RECT_END_X(map_rect) + 2, map_rect.start.y - 1, ROWNO * 2 + 3, C_MAPBORDER, sub_vram);
}

void nds_draw_map(coord_t *center)
{
  swiWaitForVBlank();

  if (center != NULL) {
    nds_map_set_center(*center);
  }

  pet_count = 0;

  nds_clear_sprites();

  if (map != NULL) {
    int x, y;

    for (y = map->viewport.start.y; y < RECT_END_Y(map->viewport); y++) {
      for (x = map->viewport.start.x; x < RECT_END_X(map->viewport); x++) {
        coord_t cur = { .x = x, .y = y };

        nds_draw_tile(cur);
      }
    }

    if (TILE_FILE != NULL) {
      nds_set_graphics_cursor(map->cursor);
    }

    nds_draw_minimap(map);
  } else {
    nds_clear_map();
    nds_clear_minimap();
  }

  nds_draw_sprites();
}

coord_t nds_map_translate_coords(coord_t coords)
{
  coord_t res = {
    .x = map->viewport.start.x + coords.x,
    .y = map->viewport.start.y + coords.y
  };

  return res;
}

int nds_map_tile_width()
{
  return tile_width_in_px;
}

int nds_map_tile_height()
{
  return tile_height_in_px;
}

coord_t nds_map_get_center()
{
  return map->center;
}

void nds_map_set_center(coord_t center)
{
  int width = map->viewport.dims.width;
  int height = map->viewport.dims.height;

  if ((center.x + width / 2) > COLNO) {
    map->center.x = COLNO - width / 2;
  } else if ((center.x - width / 2) < 0) {
    map->center.x = width / 2;
  } else {
    map->center.x = center.x;
  }

  if ((center.y + height / 2) > ROWNO) {
    map->center.y = ROWNO - height / 2;
  } else if ((center.y - height / 2) < 0) {
    map->center.y = height / 2;
  } else {
    map->center.y = center.y;
  }

  map->viewport.start.x = map->center.x - width / 2;
  map->viewport.start.y = map->center.y - height / 2;

  map->dirty = 1;
}

void nds_map_set_cursor(coord_t cursor)
{
  map->cursor = cursor;
  map->dirty = 1;
}

coord_t nds_map_relativize(coord_t coords)
{
  int u_center_px = (u.ux - map->viewport.start.x) * tile_width_in_px + tile_width_in_px / 2;
  int u_center_py = (u.uy - map->viewport.start.y) * tile_height_in_px + tile_height_in_px / 2;

  coord_t res = {
    .x = coords.x - u_center_px,
    .y = coords.y - u_center_py
  };

  return res;
}
