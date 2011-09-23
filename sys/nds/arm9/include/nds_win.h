#ifndef _NDS_WIN_H_
#define _NDS_WIN_H_

#include "font-bdf.h"
#include "nds_charbuf.h"
#include "nds_util.h"
#include "nds_map.h"

#define MAX_WINDOWS 8

#define C_YOU       16 
#define C_MON       17
#define C_PET       18
#define C_WALL      19
#define C_DOOR      20
#define C_ROOM      21
#define C_STAIRS    22
#define C_ALTAR     23
#define C_FURNITURE 24
#define C_CORR      25
#define C_MAPBORDER 26
#define C_VISBORDER 27
#define C_CURSOR    31

#define TITLE_MAX_LINES 4
#define MAX_PAGE        255

#define PICK_ONE_TYPE 0x0100

#define CAN_MULTI_PICK(i) (((i) == PICK_ANY) || ((i) == PICK_ONE_TYPE))

typedef struct {
  ANY_P id;
  int count;
  int selected;
  int highlighted;
  int refresh;
  rectangle_t region;
  char title[TITLE_MAX_LINES][64];
} nds_menuitem_t;

typedef struct {
  nds_menuitem_t *items;
  char *prompt;
  u16 count;
  int how;
  int focused_item;
  int tapped_item;
} nds_menu_t;

typedef struct {
  u8 type;
  rectangle_t region;
  int cur_page;
  int page_idxs[MAX_PAGE];
  int topidx, bottomidx;

  // Window type-specific structures.

  nds_charbuf_t *buffer;
  nds_menu_t *menu;
  nds_map_t *map;

  u8 dirty;
  struct ppm *img; 
} nds_nhwindow_t;

extern nds_nhwindow_t *windows[MAX_WINDOWS];
extern struct font *system_font;

extern struct ppm *up_arrow;
extern struct ppm *down_arrow;
extern struct ppm *okay_arrow;
extern struct ppm *cancel_arrow;

void nds_draw_prompt(const char *prompt);
void nds_clear_prompt();

char nds_prompt_char(const char *ques, const char *choices, int holdkey);

#endif
