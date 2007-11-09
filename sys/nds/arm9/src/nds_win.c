#include <nds.h>

#include "hack.h"
#include "func_tab.h"

#include "nds_win.h"
#include "nds_gfx.h"

#include "ppm-lite.h"

#define NULLFREE(ptr) { if (ptr != NULL) { free(ptr); ptr = NULL; } }

/* Our allocated window structures */

nds_nhwindow_t *windows[MAX_WINDOWS];
struct font *system_font;

/**********************************
 * Some internal window functions.
 **********************************/

void _nds_win_append_text(nds_nhwindow_t *win, char *str)
{
  char **ptr;

  ptr = (char **)realloc(win->buffer->text, 
                         win->buffer->lines + 1);

  if (ptr == NULL) {
    iprintf("Uhoh, ran out of memory!\n");
    exit(0);
  } else {
    win->buffer->text = ptr;
  }

  win->buffer->text[win->buffer->lines++] = strdup(str);
}

void _nds_win_destroy_text(nds_nhwindow_t *win)
{
  nds_charbuf_t *buf = win->buffer;
  int i;

  if ((buf != NULL) && (buf->text != NULL)) {
    for (i = 0; i < buf->lines; i++) {
      NULLFREE(buf->text[i]);
    }

    NULLFREE(buf->text);
  }
}

void _nds_win_destroy_menu(nds_nhwindow_t *win)
{
  nds_menu_t *menu = win->menu;

  if (menu != NULL) {
    NULLFREE(menu->items);
    NULLFREE(menu->prompt);
  }
}

/*************************
 * Basic Window Functions
 *************************/

/*
 * Initialize our window port.  Of course, there are no arguments but we
 * also initialize the graphics subsystem, here, so we'll be prepared for
 * later.
 */
int nds_init_nhwindows(int *argc, char **argv)
{
  int i;

  system_font = read_bdf("/NetHack/font.bdf");

  if (system_font == NULL) {
    iprintf("Error loading font!\n");

    return -1;
  }

  /* TODO: Load our tiles here. */

  for (i = 0; i < MAX_WINDOWS; i++) {
    windows[i] = NULL;
  }

  /* Set up our palettes. */

  BG_PALETTE_SUB[254] = RGB15(0, 0, 0);
  BG_PALETTE_SUB[255] = RGB15(31,31,31);

  iflags.window_inited = true;

  return 0;
}

/*
 * Create a new window structure, and allocate it an ID, which really
 * ends up being an index into the windows global array.
 */
void nds_clear_nhwindow(winid win); // Just a necessary prototype.
int nds_create_nhwindow(int type)
{
  nds_nhwindow_t *win = (nds_nhwindow_t *)malloc(sizeof(nds_nhwindow_t));
  int i;
  int id = -1;

  memset(win, 0, sizeof(nds_nhwindow_t));

  /* Allocate a window ID. */

  for (i = 0; i < MAX_WINDOWS; i++) {
    if (windows[i] == NULL) {
      windows[i] = win;
      id = i;

      break;
    }
  }

  /* If we couldn't allocate an ID, return an error. */

  if (id < 0) {
    NULLFREE(win);

    return WIN_ERR;
  }

  /* Initialize the window. */
  win->type = type;

  nds_clear_nhwindow(id);

  return id;
}

/*
 * Destroy the window specified and recycle the window slot.
 */
void nds_destroy_nhwindow(winid win)
{
  nds_nhwindow_t *window = windows[win];

  nds_clear_nhwindow(win);

  NULLFREE(window);

  windows[win] = NULL;
}

/*
 * Clear the given window.  This resets the window to it's initial state.  This
 * might be a bit overkill, but it allows me to reuse the code in
 * nds_create_nhwindow.
 */
void nds_clear_nhwindow(winid win)
{
  nds_nhwindow_t *window = windows[win];

  /* Perform type-specific initialization. */

  switch (window->type) {
    case NHW_MAP:
      window->w = 256;
      window->h = 256;

      NULLFREE(window->glyphs);

      break;

    case NHW_MESSAGE:
      window->w = 256;
      window->h = 64;

      _nds_win_destroy_text(window);

      NULLFREE(window->buffer);

      break;

    case NHW_STATUS:
      window->w = 104;
      window->h = 128;

      _nds_win_destroy_text(window);
      NULLFREE(window->buffer);

      break;

    case NHW_MENU:
      window->w = 192;
      window->h = 224;

      if (window->buffer) {
        _nds_win_destroy_text(window);
        NULLFREE(window->buffer);
      } else {
        _nds_win_destroy_menu(window);
        NULLFREE(window->menu);
      }

      break;

    case NHW_TEXT:
      window->w = 256;
      window->h = 224;

      _nds_win_destroy_text(window);
      NULLFREE(window->buffer);

      break;
  }

  window->cx = 0;
  window->cy = 0;

  window->buffer = NULL;
  window->menu = NULL;
  window->glyphs = NULL;

  window->dirty = 0;
}

void nds_curs(winid win, int x, int y)
{
  windows[win]->cx = x;
  windows[win]->cy = y;
}

void nds_putstr(winid win, int attr, const char *str)
{
  char *c;
  nds_nhwindow_t *window = windows[win];

  /* Initialize our text buffer if it wasn't already done. */

  if (window->buffer == NULL) {
    window->buffer = (nds_charbuf_t *)malloc(sizeof(nds_charbuf_t));
    window->buffer->text = NULL;
    window->buffer->lines = 0;
  }

  while ((c = index(str, '\n')) != 0) {
    char tmp;

    tmp = *c;
    *c = '\0';

    _nds_win_append_text(window, str);

    *c = tmp;

    str = c + 1;
  }

  if (*str) {
    _nds_win_append_text(window, str);
  }
}

/***************************
 * Window Display Functions
 ***************************/

/*
 * Display a scrollable window the first item index specified.
 * 'butwidth' corresponds to the width of the buttons we want to
 * display.
 */
#define HMARGIN 3
#define VMARGIN 2
void _nds_draw_scroller(nds_nhwindow_t *window,
                        int x, int y,
                        int width, int height,
                        int topidx,
                        int butheight)
{
  u16 *vram = (u16 *)BG_BMP_RAM_SUB(2);
  int w = width + HMARGIN * 2;
  int h = height + VMARGIN * 2 + butheight * 2;
  int prompt_w = 0, prompt_h = 0;

  if ((x == 0) && (y == 0) && (width == 254) && (height = 192)) {
    nds_fill(vram, 254);
  }

  if (window->menu) {
    text_dims(system_font, window->menu->prompt, &prompt_w, &prompt_h);
  }

  w = width + HMARGIN * 2;
  h = height + VMARGIN * 2 + butheight;

  if (prompt_h > 0) {
    h += prompt_h + VMARGIN * 2;
  }

  if ((prompt_w + prompt_h * 2) > w) {
    w = prompt_w + prompt_h * 4;
  }

  nds_draw_rect(x, y, w, h, 254, vram);

  nds_draw_hline(x, y, w, 255, vram);
  nds_draw_hline(x, y + h - butheight, w, 255, vram);
  nds_draw_hline(x, y + h, w, 255, vram);

  nds_draw_vline(x, y, h, 255, vram);
  nds_draw_vline(x + w/2, y + h - butheight, butheight, 255, vram);
  nds_draw_vline(x + w, y, h, 255, vram);

  if (prompt_h != 0) {
    nds_draw_hline(x, y + prompt_h + VMARGIN * 2, w, 255, vram);
    nds_draw_vline(x + prompt_h, y, prompt_h + VMARGIN * 2, 255, vram);
    nds_draw_vline(x + w - prompt_h, y, prompt_h + VMARGIN * 2, 255, vram);

    nds_draw_text(system_font, window->menu->prompt,
                  x + (w / 2 - prompt_w / 2), y + 2,
                  254, 255,
                  vram);
  }

  switch (window->type) {
    case NHW_MENU:
      {
        nds_menu_t *menu = window->menu;
        struct ppm *img = alloc_ppm(width, height);
        int cur_y = 0;
        int i;

        for (i = 0; i < menu->count; i++) {
          draw_string(system_font,
                      menu->items[i].title,
                      img, 0, cur_y, 1,
                      255, 0, 255);

          cur_y += menu->items[i].height;
          
          /* These correspond to the real screen coords of the item. */
          /* We'll use them later to perform click handling.         */

          menu->items[i].x = x + HMARGIN;
          menu->items[i].y = y + cur_y + VMARGIN + butheight;
        } 

        draw_ppm_bw(img, vram, x + HMARGIN, y + butheight + VMARGIN, 
                    256, 254, 255);

        NULLFREE(img);
      }

      break;

    case NHW_TEXT:
      break;
  }

  iprintf("HERE\n");

  SUB_DISPLAY_CR |= DISPLAY_BG2_ACTIVE;
}

/*
 * Draws a scrollable window on the screen.  'how' is the standard
 * NetHack how types (PICK_NONE, PICK_ONE, PICK_ANY) and dictates 
 * whether this window will allow it's lines to be selected.  If so,
 * taps will cause items to be selected accordingly.  Note, this
 * only works with NHW_MENU windows.
 */
void _nds_do_scroller(nds_nhwindow_t *window, 
                      int x, int y, 
                      int width, int height,
                      int how)
{
  int topidx = 0;
  int refresh = 1;

  /* 
     Clear VRAM in the BG2 layer and then activate it.
   */

  while (1) {
    swiWaitForVBlank();

    if (refresh) {
      _nds_draw_scroller(window, x, y, width, height, topidx, 10);

      refresh = 0;
    }
  }

  /* This'll initialize all the items to -1. */
}

void _nds_display_map(nds_nhwindow_t *win, int blocking)
{
}

void _nds_display_message(nds_nhwindow_t *win, int blocking)
{
}

void _nds_display_status(nds_nhwindow_t *win, int blocking)
{
}

void _nds_display_text_menu(nds_nhwindow_t *win, int blocking)
{
}

void _nds_display_chooser_menu(nds_nhwindow_t *win, int blocking)
{
}

void _nds_display_text(nds_nhwindow_t *win, int blocking)
{
}

/*
 * The window display entrypoint for nethack.  Here we call into the other
 * display functions we've defined.
 */
void nds_display_nhwindow(winid win, int blocking)
{
  nds_nhwindow_t *window = windows[win];

  switch (window->type) {
    case NHW_MAP:
      _nds_display_map(window, blocking);
      break;

    case NHW_MESSAGE:
      _nds_display_message(window, blocking);
      break;

    case NHW_STATUS:
      _nds_display_status(window, blocking);
      break;

    case NHW_MENU:
      if (window->buffer != NULL) { 
        _nds_display_text_menu(window, blocking);
      } else {
        iprintf("I thought this wasn't supposed to happen...\n");
      }

      break;

    case NHW_TEXT:
      _nds_display_text(window, blocking);
      break;
  }
}

/**********************
 * Menu handling logic
 **********************/

/*
 * Prepare a window for use as a menu.  Technically we should check if the 
 * type is correct... but, who needs error handling?
 */
void nds_start_menu(winid win)
{
  /* I get the impression this same menu could be prepped multiple times.
     Thus, we destroy the old items, if they're present. */

  if (windows[win]->menu != NULL) {
    NULLFREE(windows[win]->menu->items);
    NULLFREE(windows[win]->menu->prompt);
  } else {
    windows[win]->menu = (nds_menu_t *)malloc(sizeof(nds_menu_t));
  }

  windows[win]->menu->items = NULL;
  windows[win]->menu->count = 0;
}

/*
 * Add a menu item to a prepped window.  We completely ignore accelerators,
 * glyphs, and attributes right now.
 */
void nds_add_menu(winid win, int glyph, const ANY_P *id,
                  CHAR_P accel, CHAR_P gacc,
                  int attr, const char *str, BOOLEAN_P presel)
{
  nds_nhwindow_t *window = windows[win];
  nds_menuitem_t *items;
  int idx;

  items = (nds_menuitem_t *)realloc(window->menu->items, 
                                    sizeof(nds_menuitem_t) * (window->menu->count + 1));

  if (items == NULL) {
    iprintf("Doheth, ran out of memory...\n");
  }

  window->menu->items = items;
  idx = window->menu->count++;

  items[idx].id = *id;
  strncpy(items[idx].title, str, 80);
  items[idx].selected = presel;
}

/*
 * Here we attach the prompt to the menu;
 */
void nds_end_menu(winid win, const char *prompt)
{
  windows[win]->menu->prompt = strdup(prompt);
}

/*****************************************
 * Actually menu display and select code.
 *****************************************/

/*
 * The nethack entry-point.
 */
void nds_select_menu(winid win, int how, menu_item **sel)
{
  nds_nhwindow_t *window = windows[win];
  int i, j;
  int width, height;
  int scrolling = 0;

  /*
   * First, we compute the dimensions of our menu and all of the items.
   */
  text_dims(system_font, window->menu->prompt, &width, NULL);

  height = 0;

  for (i = 0; i < window->menu->count; i++) {
    nds_menuitem_t *item = &(window->menu->items[i]);

    text_dims(system_font, item->title,
              &(item->width), &(item->height));

    if (width < item->width) {
      width = item->width;
    }

    height += item->height;
  }

  if (height > 192) {
    scrolling = 1;
    height = 192;
  }

  _nds_do_scroller(window, 1, 1, width, height, how);
  
  if (how != PICK_NONE) {
    for (i = 0, j = 0; i < window->menu->count; i++) {
      if (window->menu->items[i].selected) {
        menu_item *item = (menu_item *)malloc(sizeof(menu_item));

        item->item = window->menu->items[i].id;
        item->count = window->menu->items[i].count;

        sel[j++] = item;
      }
    }
  }
}

/****************
 * Miscellaneous
 ****************/

void nds_raw_print(const char *str)
{
  iprintf(str);
}

void nds_raw_print_bold(const char *str)
{
  iprintf(str);
}

void do_null()
{
}

struct window_procs nds_procs = {
    "NDS",
    /*WC_COLOR|WC_HILITE_PET|WC_ALIGN_MESSAGE|WC_ALIGN_STATUS|
      WC_INVERSE|WC_SCROLL_AMOUNT|WC_SCROLL_MARGIN|WC_MAP_MODE|
      WC_FONT_MESSAGE|WC_FONT_STATUS|WC_FONT_MENU|WC_FONT_TEXT|WC_FONT_MAP|
      WC_FONTSIZ_MESSAGE|WC_FONTSIZ_STATUS|WC_FONTSIZ_MENU|WC_FONTSIZ_TEXT|
      WC_TILE_WIDTH|WC_TILE_HEIGHT|WC_TILE_FILE|WC_VARY_MSGCOUNT|
      WC_WINDOWCOLORS|WC_PLAYER_SELECTION|WC_SPLASH_SCREEN|WC_POPUP_DIALOG,*/
    0L,
    0L,
    nds_init_nhwindows,
    do_null, //nds_player_selection,
    do_null, //nds_askname,
    do_null, //nds_get_nh_event,
    do_null, //nds_exit_nhwindows,
    do_null, //nds_suspend_nhwindows,
    do_null, //nds_resume_nhwindows,
    nds_create_nhwindow,
    nds_clear_nhwindow,
    nds_display_nhwindow,
    nds_destroy_nhwindow,
    nds_curs,
    nds_putstr,
    do_null, //nds_display_file,
    nds_start_menu,
    nds_add_menu,
    nds_end_menu,
    nds_select_menu,
    do_null, //genl_message_menu,		/* no need for X-specific handling */
    do_null, //nds_update_inventory,
    do_null, //nds_mark_synch,
    do_null, //nds_wait_synch,
#ifdef CLIPPING
    do_null, //nds_cliparound,
#endif
#ifdef POSITIONBAR
    donull,
#endif
    do_null, //nds_print_glyph,
    nds_raw_print,
    nds_raw_print_bold,
    do_null, //nds_nhgetch,
    do_null, //nds_nh_poskey,
    do_null, //nds_nhbell,
    do_null, //nds_doprev_message,
    do_null, //nds_yn_function,
    do_null, //nds_getlin,	
    do_null, //nds_get_ext_cmd,
    do_null, //nds_number_pad,
    do_null, //nds_delay_output,
#ifdef CHANGE_COLOR	/* only a Mac option currently */
	donull,
	donull,
#endif
    /* other defs that really should go away (they're tty specific) */
    do_null, //nds_start_screen,
    do_null, //nds_end_screen,
    do_null, //nds_outrip,	
    do_null, //nds_preference_update,
};

