#include <nds.h>
#include <stdio.h>

#include "hack.h"
#include "func_tab.h"

#include "nds_win.h"
#include "nds_charbuf.h"
#include "nds_gfx.h"
#include "nds_util.h"
#include "nds_status.h"
#include "nds_msg.h"
#include "nds_cmd.h"

#include "ppm-lite.h"
#include "ds_kbd.h"
#include "nds_map.h"

#include "nds_win_gfx.h"

#define PROMPT_LAYER_WIDTH 40

/* Some prototypes. */

int _nds_display_yes_no_prompt(char *prompt);

/* Our allocated window structures */

nds_nhwindow_t *windows[MAX_WINDOWS];
struct font *system_font;

/* Some variables that we'll use when drawing the screen */

int map_rows;
int map_cols;

/**********************************
 * Some internal window functions.
 **********************************/

void _nds_win_append_text(nds_nhwindow_t *win, const char *str)
{
  iprintf("%s\n", str);

  if (win->buffer == NULL) {
    win->buffer = nds_charbuf_create(0);
  }

  nds_charbuf_append(win->buffer, str);
}

void _nds_win_destroy_text(nds_nhwindow_t *win)
{
  nds_charbuf_destroy(win->buffer);

  win->buffer = NULL;
}

void _nds_win_destroy_menu(nds_nhwindow_t *win)
{
  nds_menu_t *menu = win->menu;

  if (menu != NULL) {
    NULLFREE(menu->items);
    NULLFREE(menu->prompt);
  }

  NULLFREE(win->menu);
}

/*************************
 * Basic Window Functions
 *************************/

/*
 * Initialize our window port.  Of course, there are no arguments but we
 * also initialize the graphics subsystem, here, so we'll be prepared for
 * later.
 */
void nds_init_nhwindows(int *argc, char **argv)
{
  int i;

  system_font = read_bdf("font.bdf");

  if (system_font == NULL) {
    iprintf("Error loading font!\n");

    return;
  }

  if (nds_init_map(&map_rows, &map_cols)) {
    iprintf("Error loading tiles!\n");

    return;
  }

  keysSetRepeat(30, 2);

  nds_init_cmd();

  for (i = 0; i < MAX_WINDOWS; i++) {
    windows[i] = NULL;
  }

  /* Set up our palettes. */

  BG_PALETTE[253] = RGB15(0, 31, 0);
  BG_PALETTE[254] = RGB15(0, 0, 0);
  BG_PALETTE[255] = RGB15(31, 31, 31);

  BG_PALETTE_SUB[255] = RGB15(31,31,31);
  BG_PALETTE_SUB[253] = RGB15(31,0, 0);
  BG_PALETTE_SUB[252] = RGB15(0,31, 0);

  iflags.window_inited = true;
}

void nds_player_selection()
{
  int i, k, n;
  char pick4u = 'n', thisch, lastch = 0;
  char pbuf[PROMPT_LAYER_WIDTH], plbuf[PROMPT_LAYER_WIDTH];
  winid win;
  anything any;
  menu_item *selected = 0;

  /* prevent an unnecessary prompt */
  rigid_role_checks();

  /* Should we randomly pick for the player? */
  if (!flags.randomall &&
      (flags.initrole == ROLE_NONE || flags.initrace == ROLE_NONE ||
       flags.initgend == ROLE_NONE || flags.initalign == ROLE_NONE)) {

    char *prompt = build_plselection_prompt(pbuf, PROMPT_LAYER_WIDTH, flags.initrole,
                                            flags.initrace, flags.initgend, flags.initalign);
    int res = _nds_display_yes_no_prompt(prompt);

    if (res < 0) {
give_up:
      return;
    }

    if (res) {
      pick4u = 'y';
    }
  }

  root_plselection_prompt(plbuf, PROMPT_LAYER_WIDTH - 1,
                          flags.initrole, flags.initrace, flags.initgend, flags.initalign);

  /* Select a role, if necessary */
  /* we'll try to be compatible with pre-selected race/gender/alignment,
   * but may not succeed */

  if (flags.initrole < 0) {
    char rolenamebuf[PROMPT_LAYER_WIDTH];

    /* Process the choice */
    if (pick4u == 'y' || flags.initrole == ROLE_RANDOM || flags.randomall) {
      /* Pick a random role */
      flags.initrole = pick_role(flags.initrace, flags.initgend,
                                 flags.initalign, PICK_RANDOM);
      if (flags.initrole < 0) {
        iprintf("Incompatible role!");

        flags.initrole = randrole();
      }
    } else {
      /* Prompt for a role */

      win = create_nhwindow(NHW_MENU);
      start_menu(win);

      any.a_void = 0;         /* zero out all bits */

      for (i = 0; roles[i].name.m; i++) {
        if (ok_role(i, flags.initrace, flags.initgend,
                    flags.initalign)) {

          any.a_int = i+1;	/* must be non-zero */
          thisch = lowc(roles[i].name.m[0]);

          if (thisch == lastch) thisch = highc(thisch);

          if (flags.initgend != ROLE_NONE && flags.initgend != ROLE_RANDOM) {
            if (flags.initgend == 1  && roles[i].name.f)
              Strcpy(rolenamebuf, roles[i].name.f);
            else
              Strcpy(rolenamebuf, roles[i].name.m);
          } else {
            if (roles[i].name.f) {
              Strcpy(rolenamebuf, roles[i].name.m);
              Strcat(rolenamebuf, "/");
              Strcat(rolenamebuf, roles[i].name.f);
            } else 
              Strcpy(rolenamebuf, roles[i].name.m);
          }	

          add_menu(win, NO_GLYPH, &any, thisch,
                   0, ATR_NONE, an(rolenamebuf), MENU_UNSELECTED);

          lastch = thisch;
        }
      }

      any.a_int = pick_role(flags.initrace, flags.initgend,
                            flags.initalign, PICK_RANDOM)+1;

      if (any.a_int == 0)	/* must be non-zero */
        any.a_int = randrole()+1;

      add_menu(win, NO_GLYPH, &any , '*', 0, ATR_NONE,
               "Random", MENU_UNSELECTED);

      any.a_int = i+1;	/* must be non-zero */

      add_menu(win, NO_GLYPH, &any , 'q', 0, ATR_NONE,
               "Quit", MENU_UNSELECTED);

      Sprintf(pbuf, "Pick a role for your %s", plbuf);

      end_menu(win, pbuf);

      n = select_menu(win, PICK_ONE, &selected);

      destroy_nhwindow(win);

      /* Process the choice */
      if (n != 1 || selected[0].item.a_int == any.a_int)
        goto give_up;		/* Selected quit */

      flags.initrole = selected[0].item.a_int - 1;
      free((genericptr_t) selected),	selected = 0;
    }

    (void)  root_plselection_prompt(plbuf, PROMPT_LAYER_WIDTH - 1,
                                    flags.initrole, flags.initrace, flags.initgend, flags.initalign);
  }

  /* Select a race, if necessary */
  /* force compatibility with role, try for compatibility with
   * pre-selected gender/alignment */

  if (flags.initrace < 0 || !validrace(flags.initrole, flags.initrace)) {
    /* pre-selected race not valid */

    if (pick4u == 'y' || flags.initrace == ROLE_RANDOM || flags.randomall) {
      flags.initrace = pick_race(flags.initrole, flags.initgend,
                                 flags.initalign, PICK_RANDOM);
      if (flags.initrace < 0) {
        iprintf("Incompatible race!");
        flags.initrace = randrace(flags.initrole);
      }
    } else {	/* pick4u == 'n' */
      /* Count the number of valid races */
      n = 0;	/* number valid */
      k = 0;	/* valid race */

      for (i = 0; races[i].noun; i++) {
        if (ok_race(flags.initrole, i, flags.initgend,
                    flags.initalign)) {
          n++;
          k = i;
        }
      }

      if (n == 0) {
        for (i = 0; races[i].noun; i++) {
          if (validrace(flags.initrole, i)) {
            n++;
            k = i;
          }
        }
      }

      /* Permit the user to pick, if there is more than one */
      if (n > 1) {
        win = create_nhwindow(NHW_MENU);

        start_menu(win);
        any.a_void = 0;         /* zero out all bits */

        for (i = 0; races[i].noun; i++)
          if (ok_race(flags.initrole, i, flags.initgend,
                      flags.initalign)) {
            any.a_int = i+1;	/* must be non-zero */
            add_menu(win, NO_GLYPH, &any, races[i].noun[0],
                     0, ATR_NONE, races[i].noun, MENU_UNSELECTED);
          }

        any.a_int = pick_race(flags.initrole, flags.initgend,
                              flags.initalign, PICK_RANDOM)+1;

        if (any.a_int == 0)	/* must be non-zero */
          any.a_int = randrace(flags.initrole)+1;

        add_menu(win, NO_GLYPH, &any , '*', 0, ATR_NONE,
                 "Random", MENU_UNSELECTED);

        any.a_int = i+1;	/* must be non-zero */

        add_menu(win, NO_GLYPH, &any , 'q', 0, ATR_NONE,
                 "Quit", MENU_UNSELECTED);

        Sprintf(pbuf, "Pick the race of your %s", plbuf);
        end_menu(win, pbuf);

        n = select_menu(win, PICK_ONE, &selected);
        destroy_nhwindow(win);

        if (n != 1 || selected[0].item.a_int == any.a_int)
          goto give_up;		/* Selected quit */

        k = selected[0].item.a_int - 1;
        free((genericptr_t) selected),	selected = 0;
      }

      flags.initrace = k;
    }

    (void)  root_plselection_prompt(plbuf, PROMPT_LAYER_WIDTH - 1,
                                    flags.initrole, flags.initrace, flags.initgend, flags.initalign);
  }

  /* Select a gender, if necessary */
  /* force compatibility with role/race, try for compatibility with
   * pre-selected alignment */
  if (flags.initgend < 0 || !validgend(flags.initrole, flags.initrace,
                                       flags.initgend)) {
    /* pre-selected gender not valid */
    if (pick4u == 'y' || flags.initgend == ROLE_RANDOM || flags.randomall) {
      flags.initgend = pick_gend(flags.initrole, flags.initrace,
                                 flags.initalign, PICK_RANDOM);
      if (flags.initgend < 0) {
        iprintf("Incompatible gender!");
        flags.initgend = randgend(flags.initrole, flags.initrace);
      }
    } else {	/* pick4u == 'n' */
      /* Count the number of valid genders */
      n = 0;	/* number valid */
      k = 0;	/* valid gender */

      for (i = 0; i < ROLE_GENDERS; i++) {
        if (ok_gend(flags.initrole, flags.initrace, i,
                    flags.initalign)) {
          n++;
          k = i;
        }
      }

      if (n == 0) {
        for (i = 0; i < ROLE_GENDERS; i++) {
          if (validgend(flags.initrole, flags.initrace, i)) {
            n++;
            k = i;
          }
        }
      }

      /* Permit the user to pick, if there is more than one */
      if (n > 1) {
        win = create_nhwindow(NHW_MENU);
        start_menu(win);

        any.a_void = 0;         /* zero out all bits */

        for (i = 0; i < ROLE_GENDERS; i++)
          if (ok_gend(flags.initrole, flags.initrace, i,
                      flags.initalign)) {
            any.a_int = i+1;
            add_menu(win, NO_GLYPH, &any, genders[i].adj[0],
                     0, ATR_NONE, genders[i].adj, MENU_UNSELECTED);
          }

        any.a_int = pick_gend(flags.initrole, flags.initrace,
                              flags.initalign, PICK_RANDOM)+1;

        if (any.a_int == 0)	/* must be non-zero */
          any.a_int = randgend(flags.initrole, flags.initrace)+1;

        add_menu(win, NO_GLYPH, &any , '*', 0, ATR_NONE,
                 "Random", MENU_UNSELECTED);

        any.a_int = i+1;	/* must be non-zero */

        add_menu(win, NO_GLYPH, &any , 'q', 0, ATR_NONE,
                 "Quit", MENU_UNSELECTED);

        Sprintf(pbuf, "Pick the gender of your %s", plbuf);
        end_menu(win, pbuf);

        n = select_menu(win, PICK_ONE, &selected);
        destroy_nhwindow(win);

        if (n != 1 || selected[0].item.a_int == any.a_int)
          goto give_up;		/* Selected quit */

        k = selected[0].item.a_int - 1;
        free((genericptr_t) selected),	selected = 0;
      }

      flags.initgend = k;
    }

    (void)  root_plselection_prompt(plbuf, PROMPT_LAYER_WIDTH - 1,
                                    flags.initrole, flags.initrace, flags.initgend, flags.initalign);
  }

  /* Select an alignment, if necessary */
  /* force compatibility with role/race/gender */
  if (flags.initalign < 0 || !validalign(flags.initrole, flags.initrace,
                                         flags.initalign)) {
    /* pre-selected alignment not valid */
    if (pick4u == 'y' || flags.initalign == ROLE_RANDOM || flags.randomall) {
      flags.initalign = pick_align(flags.initrole, flags.initrace,
                                   flags.initgend, PICK_RANDOM);
      if (flags.initalign < 0) {
        iprintf("Incompatible alignment!");
        flags.initalign = randalign(flags.initrole, flags.initrace);
      }
    } else {	/* pick4u == 'n' */
      /* Count the number of valid alignments */
      n = 0;	/* number valid */
      k = 0;	/* valid alignment */

      for (i = 0; i < ROLE_ALIGNS; i++) {
        if (ok_align(flags.initrole, flags.initrace, flags.initgend,
                     i)) {
          n++;
          k = i;
        }
      }

      if (n == 0) {
        for (i = 0; i < ROLE_ALIGNS; i++) {
          if (validalign(flags.initrole, flags.initrace, i)) {
            n++;
            k = i;
          }
        }
      }

      /* Permit the user to pick, if there is more than one */
      if (n > 1) {
        win = create_nhwindow(NHW_MENU);
        start_menu(win);

        any.a_void = 0;         /* zero out all bits */

        for (i = 0; i < ROLE_ALIGNS; i++)
          if (ok_align(flags.initrole, flags.initrace,
                       flags.initgend, i)) {
            any.a_int = i+1;
            add_menu(win, NO_GLYPH, &any, aligns[i].adj[0],
                     0, ATR_NONE, aligns[i].adj, MENU_UNSELECTED);
          }

        any.a_int = pick_align(flags.initrole, flags.initrace,
                               flags.initgend, PICK_RANDOM)+1;

        if (any.a_int == 0)	/* must be non-zero */
          any.a_int = randalign(flags.initrole, flags.initrace)+1;

        add_menu(win, NO_GLYPH, &any , '*', 0, ATR_NONE,
                 "Random", MENU_UNSELECTED);

        any.a_int = i+1;	/* must be non-zero */

        add_menu(win, NO_GLYPH, &any , 'q', 0, ATR_NONE,
                 "Quit", MENU_UNSELECTED);

        Sprintf(pbuf, "Pick the alignment of your %s", plbuf);
        end_menu(win, pbuf);

        n = select_menu(win, PICK_ONE, &selected);
        destroy_nhwindow(win);

        if (n != 1 || selected[0].item.a_int == any.a_int)
          goto give_up;		/* Selected quit */

        k = selected[0].item.a_int - 1;
        free((genericptr_t) selected),	selected = 0;
      }

      flags.initalign = k;
    }
  }
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
  win->img = NULL;

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

  if (window->img) {
    free_ppm(window->img);
  }

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
      NULLFREE(window->map);

      break;

    case NHW_MESSAGE:
      _nds_win_destroy_text(window);

      break;

    case NHW_STATUS:
      _nds_win_destroy_text(window);

      break;

    case NHW_MENU:
      if (window->buffer) {
        _nds_win_destroy_text(window);
      } else {
        _nds_win_destroy_menu(window);
      }

      break;

    case NHW_TEXT:
      _nds_win_destroy_text(window);

      break;
  }

  window->buffer = NULL;
  window->menu = NULL;
  window->map = NULL;

  window->dirty = 0;
}

void nds_putstr(winid win, int attr, const char *str)
{
  char *c;
  nds_nhwindow_t *window = windows[win];

  /* Initialize our text buffer if it wasn't already done. */

  while ((c = index(str, '\n')) != 0) {
    char tmp;

    tmp = *c;
    *c = '\0';

    if (win == WIN_STATUS) {
      nds_update_status((char *)str);
    } else {
      _nds_win_append_text(window, str);
    }
    
    *c = tmp;

    str = c + 1;
  }

  if (*str) {
    if (win == WIN_STATUS) {
      nds_update_status((char *)str);
    } else {
      _nds_win_append_text(window, str);
    }
  }

  if (win == WIN_MESSAGE) {
    nds_update_msg(window, 0);
  }
}

/***************************
 * Window Display Functions
 ***************************/

struct ppm *prompt_img = NULL;
int prompt_y;

void nds_draw_prompt(char *prompt)
{
  u16 *vram = (u16 *)BG_BMP_RAM_SUB(4);
  int prompt_h;

  text_dims(system_font, (char *)prompt, NULL, &prompt_h);

  if (prompt_img == NULL) {
    prompt_img = alloc_ppm(252, prompt_h);
    prompt_y = 192 - prompt_h * 2 - 4;
  }

  /* Draw the prompt */

  clear_ppm(prompt_img);
  draw_string(system_font, (char *)prompt, prompt_img, 0, 0, 1, 255, 0, 255);
  draw_ppm_bw(prompt_img, vram, 4, prompt_y, 256, 254, 255);
}

void nds_clear_prompt()
{
  u16 *vram = (u16 *)BG_BMP_RAM_SUB(4);

  clear_ppm(prompt_img);
  draw_ppm_bw(prompt_img, vram, 4, prompt_y, 256, 254, 255);
}

/*
 * Display a scrollable window the first item index specified.
 * 'butwidth' corresponds to the width of the buttons we want to
 * display.
 */

struct ppm *up_arrow = NULL;
struct ppm *down_arrow = NULL; 
struct ppm *okay_button = NULL;
struct ppm *cancel_button = NULL;

void _nds_copy_header_pixels(char *src, long *buf)
{
  while (*src) {
    int rgb[3];

    HEADER_PIXEL(src, rgb);

    *buf = (rgb[0] << 16) | (rgb[1] << 8) | rgb[2];
    buf++;
  }
}

void _nds_draw_scroller(nds_nhwindow_t *window, int clear)
{
  u16 *vram = (u16 *)BG_BMP_RAM(2);

  int start_x, end_x, start_y, end_y;
  int tag_w, tag_h;

  int maxidx;

  if (up_arrow == NULL) {
    up_arrow = alloc_ppm(16, 16);
    down_arrow = alloc_ppm(16, 16);
    okay_button = alloc_ppm(16, 16);
    cancel_button = alloc_ppm(16, 16);

    _nds_copy_header_pixels(up_arrow_data, (long *)up_arrow->rgba);
    _nds_copy_header_pixels(down_arrow_data, (long *)down_arrow->rgba);
    _nds_copy_header_pixels(okay_data, (long *)okay_button->rgba);
    _nds_copy_header_pixels(cancel_data, (long *)cancel_button->rgba);
  }

  start_x = (256 / 2 - (window->width / 2));
  end_x = 256 / 2 + (window->width / 2);

  if (window->buffer == NULL) {
    text_dims(system_font, "*", &tag_w, &tag_h);

    tag_w *= 4;
    start_x -= tag_w;
  }

  if (start_x < 0) {
    end_x -= start_x;
    start_x = 0;
  }

  if (window->height > 192 - 32) {
    start_y = 16;
    end_y = 192 - 16;
  } else {
    start_y = 192 / 2 - window->height / 2;
    end_y = 192 / 2 + window->height / 2;
  }

  swiWaitForVBlank();

  if (window->buffer == NULL) {
    nds_menu_t *menu = window->menu;
    int cur_y = 0;
    int i;

    if (clear) {
      nds_fill(vram, 254);
    }

    if (! window->img) {
      window->img = alloc_ppm(end_x - start_x, tag_h);
    }

    for (i = window->topidx; (i < menu->count) && (cur_y < (end_y - start_y)); i++) {
      if (clear || menu->items[i].refresh) {
        char tag[3] = "  ";

        clear_ppm(window->img);

        if (menu->items[i].selected) {
          if (menu->items[i].count > 0) {
            sprintf(tag, "%d", menu->items[i].count);
          } else {
            strcpy(tag, "* ");
          }
        }

        if (menu->items[i].id.a_int == 0) {
          draw_string(system_font,
                      menu->items[i].title,
                      window->img, 0, 0, 1,
                      255, 0, 255);
        } else {
          draw_string(system_font,
                      tag,
                      window->img, 0, 0, 1,
                      255, 0, 255);

          draw_string(system_font,
                      menu->items[i].title,
                      window->img, tag_w, 0, 1,
                      255, 0, 255);
        }

        if (! clear) {
          int width = end_x - start_x;

          if (width > 256) {
            width = 256;
          }

          nds_draw_rect(start_x, start_y + cur_y, 
                        width, menu->items[i].height,
                        254, vram);
        }

        draw_ppm_bw(window->img, vram, start_x, start_y + cur_y, 
                    256, 254, (menu->items[i].highlighted) ? 253 : 255);

        menu->items[i].refresh = 0;
      }

      menu->items[i].x = start_x + tag_w;
      menu->items[i].y = start_y + cur_y;

      cur_y += menu->items[i].height;
    } 

    /* Aight, render the offscreen buffer. */

    window->bottomidx = i;
    maxidx = menu->count;

    if (menu->how == PICK_ANY) {
      draw_ppm_bw(okay_button, vram,
                  256 - okay_button->width * 2 - 8, 192 - okay_button->height,
                  256, 254, 255);
    }
  } else {
    nds_charbuf_t *charbuf = window->buffer;
    int cur_y = 0;
    int i;

    if (! window->img) {
      window->img = alloc_ppm(end_x - start_x, end_y - start_y);
    } else {
      clear_ppm(window->img);
    }

    for (i = window->topidx; (i < charbuf->count) && (cur_y < (end_y - start_y)); i++) {
      draw_string(system_font,
                  charbuf->lines[i].text,
                  window->img, 0, cur_y, 1,
                  255, 0, 255);

      cur_y += charbuf->lines[i].height;
    }

    window->bottomidx = i;
    maxidx = charbuf->count;

    if (clear) {
      nds_fill(vram, 254);
    }

    draw_ppm_bw(window->img, vram, start_x, start_y, 
                256, 254, 255);
  }

  if (window->topidx > 0) {
    draw_ppm_bw(up_arrow, vram, 
                256 / 2 - up_arrow->width / 2, 0, 
                256, 254, 255);
  }

  if (window->bottomidx < maxidx) {
    draw_ppm_bw(down_arrow, vram, 
                256 / 2 - down_arrow->width / 2, 192 - down_arrow->height, 
                256, 254, 255);
  }

  draw_ppm_bw(cancel_button, vram,
              256 - cancel_button->width, 192 - cancel_button->height,
              256, 254, 255);
}

int _nds_handle_scroller_buttons(nds_nhwindow_t *window, int *refresh)
{
  int pressed;
  int count;
  int scroll_up, scroll_down;
  int ret;

  /* 
   * Get a few variables initialized 
   */

  if (window->pagesize == 0) {
    window->pagesize = window->bottomidx - window->topidx;
  }

  if (window->buffer) {
    count = window->buffer->count;
  } else {
    count = window->menu->count;
  }

  *refresh = 0;

  /* 
   * Check for input 
   */

  scanKeys();
  scan_touch_screen();
  pressed = keysDown();

  /* 
   * First, check OK/Cancel controls 
   */

  ret = 0;

  /* Check the buttons */

  if (pressed & KEY_A) {
    ret = 1;
  } else if (pressed & KEY_B) {
    ret = -1;
  }
  
  /* Now check the OK/Cancel buttons */

  if (touch_released_in(256 - cancel_button->width, 
                        192 - cancel_button->height,
                        256, 
                        192)) {
    ret = -1;
  }

  if ((window->menu != NULL) && (window->menu->how == PICK_ANY) &&
      touch_released_in(256 - okay_button->width * 2 - 8,
                        192 - okay_button->height,
                        256 - okay_button->width - 8,
                        192)) {
    ret = 1;
  }

  /* Return if OK/Cancel was triggered */

  if (ret != 0) {
    return ret;
  }
  
  /* 
   * Check scrolling controls 
   */

  /* First, check our buttons */

  scroll_up = pressed & KEY_UP;
  scroll_down = pressed & KEY_DOWN;

  /* Next, check if the scroll buttons were tapped */

  scroll_up |= touch_released_in(256 / 2 - up_arrow->width / 2, 
                                 0,
                                 256 / 2 + up_arrow->width / 2, 
                                 up_arrow->height);

  scroll_down |= touch_released_in(256 / 2 - down_arrow->width / 2,
                                   192 - down_arrow->height,
                                   256 / 2 + down_arrow->width / 2,
                                   192);

  /* And now to handle the actual scroll commands */

  if (scroll_up && (window->topidx > 0)) {
    window->topidx -= window->pagesize;
    *refresh = 1;
  } else if (scroll_down && (window->bottomidx < count)) {
    window->topidx += window->pagesize;
    *refresh = 1;
  }

  if (window->topidx < 0) {
    window->topidx = 0;
  } else if (window->bottomidx > count) {
    window->topidx = count - window->pagesize;
  }

  return 0;
}

int _nds_display_yes_no_prompt(char *prompt)
{
  char c = yn_function(prompt, ynchars, 0);

  return (c == 'y');
}

void _nds_display_map(nds_nhwindow_t *win, int blocking)
{
  nds_draw_map(win->map, NULL, NULL);
}

void _nds_display_message(nds_nhwindow_t *win, int blocking)
{
  nds_update_msg(win, blocking);
}

void _nds_display_status(nds_nhwindow_t *win, int blocking)
{
  // Don't really do anything here, now
}

void _nds_display_text(nds_nhwindow_t *win, int blocking)
{
  int refresh = 1;

  int width = 0; 
  int height = 0;
  
  int i;

  for (i = 0; i < win->buffer->count; i++) {
    if (width < win->buffer->lines[i].width) {
      width = win->buffer->lines[i].width;
    }

    height += win->buffer->lines[i].height;
  }

  win->width = width;
  win->height = height;
  win->topidx = 0;
  win->bottomidx = 0;
  win->pagesize = 0;

  /* 
   * Clear VRAM in the BG2 layer and then activate it.
   */

  DISPLAY_CR |= DISPLAY_BG2_ACTIVE;

  while (1) {
    if (refresh) {
      _nds_draw_scroller(win, 1);

      refresh = 0;
    } else {
      swiWaitForVBlank();
    }

    if (_nds_handle_scroller_buttons(win, &refresh)) {
      break;
    }
  }

  DISPLAY_CR ^= DISPLAY_BG2_ACTIVE;
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
      nds_update_msg(window, blocking);
      break;

    case NHW_MENU:
      if (window->buffer != NULL) { 
        _nds_display_text(window, blocking);
      } else {
        iprintf("I thought this wasn't supposed to happen...\n");
      }

      break;

    case NHW_TEXT:
      _nds_display_text(window, blocking);
      break;
  }
}

void nds_display_file(const char *fname, int complain)
{
  winid win = create_nhwindow(NHW_TEXT);
  char buf[BUFSZ];
  FILE *file = fopen(fname, "r");

  if (! file) {
    return;
  }

  while (fgets(buf, BUFSZ, file)) {
    putstr(win, ATR_NONE, buf);
  }

  display_nhwindow(win, 1);
  destroy_nhwindow(win);
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
                                    (window->menu->count + 1) * sizeof(nds_menuitem_t));

  if (items == NULL) {
    iprintf("Doheth, ran out of memory...\n");
  }

  window->menu->items = items;
  idx = window->menu->count++;

  items[idx].id = *id;
  strncpy(items[idx].title, str, 80);
  items[idx].selected = presel;
  items[idx].highlighted = 0;
  items[idx].count = presel ? -1 : 0;
}

/*
 * Here we attach the prompt to the menu;
 */
void nds_end_menu(winid win, const char *prompt)
{
  if (prompt != NULL) {
    windows[win]->menu->prompt = strdup(prompt);
  } else {
    windows[win]->menu->prompt = NULL;
  }
}

/*****************************************
 * Actually menu display and select code.
 *****************************************/

/*
 * Draws a scrollable window on the screen.  'how' is the standard
 * NetHack how types (PICK_NONE, PICK_ONE, PICK_ANY) and dictates 
 * whether this window will allow it's lines to be selected.  If so,
 * taps will cause items to be selected accordingly.  Note, this
 * only works with NHW_MENU windows.
 */
int _nds_do_menu(nds_nhwindow_t *window)
{
  int refresh = 1;
  int clear = 1;

  int ret = 1;

  /* 
   * Clear VRAM in the BG2 layer and then activate it.
   */

  if (window->menu->prompt) {
    nds_draw_prompt(window->menu->prompt);
  }

  while (1) {
    int i;
    int held;
    int tmp;

    if (refresh) {
      _nds_draw_scroller(window, clear);

      DISPLAY_CR |= DISPLAY_BG2_ACTIVE;

      refresh = 0;
      clear = 0;
    } else {
      swiWaitForVBlank();
    }

    tmp = _nds_handle_scroller_buttons(window, &refresh);
    held = keysHeld();

    if (tmp > 0) {
      goto DONE;
    } else if (tmp < 0) {
      ret = 0;

      goto DONE;
    } else if (refresh) {
      clear = 1;
    }

    if (window->menu->how != PICK_NONE) {
      for (i = window->topidx; i < window->bottomidx; i++) {
        int item_x, item_y, item_x2, item_y2;

        item_x = window->menu->items[i].x;
        item_y = window->menu->items[i].y;
        item_x2 = item_x + window->menu->items[i].width;
        item_y2 = item_y + window->menu->items[i].height;

        if (touch_down_in(item_x, item_y, item_x2, item_y2) &&
            ! window->menu->items[i].highlighted &&
            (window->menu->items[i].id.a_int != 0)) {

          window->menu->items[i].highlighted = 1;
          window->menu->items[i].refresh = 1;
          refresh = 1;
        } else if (touch_was_down_in(item_x, item_y, item_x2, item_y2) &&
                   window->menu->items[i].highlighted &&
                   (window->menu->items[i].id.a_int != 0)) {

          window->menu->items[i].highlighted = 0;
          window->menu->items[i].refresh = 1;
          refresh = 1;
        } else if (touch_released_in(item_x, item_y, item_x2, item_y2) &&
                   (window->menu->items[i].id.a_int != 0)) {

          int cnt = window->menu->items[i].count;

          window->menu->items[i].highlighted = 0;
          window->menu->items[i].refresh = 1;

          refresh = 1;

          /* 
           * If the count is 0, it's never been selected, so start off with
           * the "all" value (-1).
           *
           * If The count is -1, meaning "all", we start counting up, so start
           * with 1.
           *
           * And, last case, we've started counting for this item, so increment.
           */
          if ((held & KEY_L) || (held & KEY_R)) {
            if (cnt == -1) {
              window->menu->items[i].selected = 0;
              window->menu->items[i].count = 0;
            } else if (cnt == 1) {
              window->menu->items[i].count = -1;
            } else if (cnt > 0) {
              window->menu->items[i].count = cnt - 1;
            }
          } else {
            if (cnt == 0) {
              window->menu->items[i].selected = 1;
              window->menu->items[i].count = -1;
            } else if (cnt == -1) {
              window->menu->items[i].count = 1;
            } else {
              window->menu->items[i].count = cnt + 1;
            }
          }

          if (window->menu->how == PICK_ONE) {
            goto DONE;
          } 
        }
      }
    }
  }

DONE:

  DISPLAY_CR ^= DISPLAY_BG2_ACTIVE;

  if (window->menu->prompt) {
    nds_clear_prompt();
  }

  return ret;
}

/*
 * The nethack entry-point.
 */
int nds_select_menu(winid win, int how, menu_item **sel)
{
  nds_nhwindow_t *window = windows[win];
  int i, j;
  int width, height;
  int cnt = 0;
  int ret;

  /* To begin with, let's initialize sel */

  *sel = NULL;

  /*
   * First, we compute the dimensions of our menu and all of the items.
   */
  width = 0;
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

  window->x = 1;
  window->y = 1;
  window->width = width;
  window->height = height;
  window->topidx = 0;
  window->bottomidx = 0;
  window->pagesize = 0;
  window->menu->how = how;

  ret = _nds_do_menu(window);

  if ((how != PICK_NONE) && ret) {
    for (i = 0; i < window->menu->count; i++) {
      if (window->menu->items[i].selected) {
        cnt++;
      }
    }

    if (cnt > 0) {
      *sel = (menu_item *)malloc(sizeof(menu_item) * cnt);

      for (i = 0, j = 0; i < window->menu->count; i++) {
        if (window->menu->items[i].selected) {
          (*sel)[j].item = window->menu->items[i].id;
          (*sel)[j].count = window->menu->items[i].count;

          j++;
        }
      }
    } else {
      *sel = NULL;
    }
  } else if (! ret) {
    cnt = -1;
  }

  return cnt;
}

/************************************
 * Input Handling (virtual keyboard)
 ************************************/

int nds_nhgetch()
{
  iprintf("nds_nhgetch called...\n");
  nds_wait_key(KEY_A);

  return 0;
}

char nds_yn_function(const char *ques, const char *choices, CHAR_P def)
{
  winid win;
  menu_item *sel;
  ANY_P ids[3];
  int ret;
  int yn = 0;
  int ynaq = 0;

  iprintf("yn_function '%s'\n", ques);

  if (choices != NULL) {
    iprintf("yn_function choices '%s'\n", choices);
  }

  /* We're being asked for a direction... this is special. */
  if ((strstr(ques, "In what direction") != NULL) ||
      (strstr(ques, "in what direction") != NULL)) {
    /*
     * We're going to use nh_poskey to get a command from the user.  However,
     * we must handle clicks specially.  Unlike normal movement, you can't
     * just click anywhere to pick a direction.  Instead, the user will be
     * expected to click in one of the adjacent squares around the player,
     * and the click will then be translated into a movement character.
     */
    while (1) {
      int x, y, mod;
      int sym;

      nds_draw_prompt("Tap an adjacent square or press a direction key.");
      sym = nh_poskey(&x, &y, &mod);
      nds_clear_prompt();

      if (mod == CLICK_1) {
        if ((x == u.ux - 1) && (y == u.uy - 1)) {
          return 'y';
        } else if ((x == u.ux) && (y == u.uy - 1)) {
          return 'k';
        } else if ((x == u.ux + 1) && (y == u.uy - 1)) {
          return 'u';
        } else if ((x == u.ux - 1) && (y == u.uy)) {
          return 'h';
        } else if ((x == u.ux) && (y == u.uy)) {
          return '.';
        } else if ((x == u.ux + 1) && (y == u.uy)) {
          return 'l';
        } else if ((x == u.ux - 1) && (y == u.uy + 1)) {
          return 'b';
        } else if ((x == u.ux) && (y == u.uy + 1)) {
          return 'j';
        } else if ((x == u.ux + 1) && (y == u.uy + 1)) {
          return 'n';
        }
      } else {
        return sym;
      }
    }
  }

  if (choices == NULL) {
    // Just force a menu in these cases... hopefully '*' is an option.
    return '*';
  } 

  win = create_nhwindow(NHW_MENU);

  start_menu(win);
  
  if ((strcasecmp(choices, ynchars) == 0) ||
      (strcasecmp(choices, ynqchars) == 0) ||
      ((ynaq = strcasecmp(choices, ynaqchars)) == 0)) {
    yn = 1;

    ids[0].a_int = 'y';
    ids[1].a_int = 'n';

    add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Yes", 0);
    add_menu(win, NO_GLYPH, &(ids[1]), 0, 0, 0, "No", 0);

    if (ynaq) {
      ids[2].a_int = 'a';

      add_menu(win, NO_GLYPH, &(ids[2]), 0, 0, 0, "All", 0);
    }
  } else if (strcasecmp(choices, "rl") == 0) {

    ids[0].a_int = 'r';
    ids[1].a_int = 'l';

    add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Right Hand", 0);
    add_menu(win, NO_GLYPH, &(ids[1]), 0, 0, 0, "Left Hand", 0);
  } else {
    iprintf("I have no idea how to handle this.", choices);
    return -1;
  }

  end_menu(win, ques);

  if (select_menu(win, PICK_ONE, &sel) <= 0) {
    ret = yn ? 'n' : '\033';
  } else {
    ret = sel->item.a_int;
    free(sel);
  }

  destroy_nhwindow(win);

  return ret;
}

void nds_getlin(const char *prompt, char *buffer)
{
  static struct ppm *input_img = NULL;
  static int input_y;

  u16 *vram = (u16 *)BG_BMP_RAM_SUB(4);

  char front[BUFSZ];
  char back[BUFSZ];

  int curspos = 0;
  int length = 0;

  int done = 0;
  int text_h;

  text_dims(system_font, (char *)prompt, NULL, &text_h);

  if (input_img == NULL) {
    input_img = alloc_ppm(252, text_h); /* We'll presume the height is constant */
    input_y = 192 - text_h - 4;
  }

  nds_draw_prompt((char *)prompt);

  /* Now initialize our buffers */

  front[0] = '\0';
  back[0] = '\0';

  /* First, display the keyboard and prompting layers */

  DISPLAY_CR |= DISPLAY_BG0_ACTIVE;

  /* Now, enter the key loop */

  while (! done) {
    u8 key;
    int front_w;

    clear_ppm(input_img);

    /* Alright, fill the prompting layer and draw out prompt. */

    text_dims(system_font, front, &front_w, NULL);

    draw_string(system_font, front, input_img, 0, 0, 1, 255, 0, 255);
    draw_string(system_font, back, input_img, front_w, 0, 1, 255, 0, 255);

    swiWaitForVBlank();

    draw_ppm_bw(input_img, vram, 4, input_y, 256, 254, 255);
    nds_draw_vline(front_w + 4, input_y, text_h, 253, vram);

    scanKeys();

    key = kbd_vblank();

    switch (key) {
      case 0:
      case K_UP_LEFT:
      case K_UP:
      case K_UP_RIGHT:
      case K_NOOP:
      case K_DOWN_LEFT:
      case K_DOWN:
      case K_DOWN_RIGHT:
        continue;

      case '\n':
        done = 1;

        break;

      case '\b':
        if (curspos > 0) {
          front[--curspos] = '\0';

          length--;
        }

        break;

      case K_LEFT:
        if (curspos > 0) {
          memmove(back + 1, back, length - curspos + 1);

          back[0] = front[curspos - 1];
          front[--curspos] = '\0';
        }

        break;

      case K_RIGHT:
        if (curspos < length) {
          front[curspos++] = back[0];
          front[curspos] = '\0';

          memmove(back, back + 1, length - curspos + 1);
        }

        break;

      default:
        front[curspos++] = key;
        front[curspos] = '\0';

        length++;

        break;
    }
  }

  /* Yeah, it's cheesy... I could use draw_rect... but this works, too :) */

  nds_clear_prompt();

  clear_ppm(input_img);
  draw_ppm_bw(input_img, vram, 4, input_y, 256, 254, 255);

  DISPLAY_CR ^= DISPLAY_BG0_ACTIVE;

  strcpy(buffer, front);
  strcat(buffer, back);
}

int nds_get_ext_cmd()
{
  char buffer[BUFSZ];
  int i;

  getlin("Extended Command", buffer);

  for (i = 0; extcmdlist[i].ef_txt != NULL; i++) {
    if (strcmp(extcmdlist[i].ef_txt, buffer) == 0) {
      return i;
    }
  }

  return -1;
} 

void nds_number_pad(int thinger)
{
  // We'll never do anything here
}

/****************
 * Miscellaneous
 ****************/

void nds_raw_print(const char *str)
{
  iprintf("Raw: %s", str);
}

void nds_raw_print_bold(const char *str)
{
  iprintf("Raw bold: %s", str);
}

void nds_askname()
{
  getlin("Enter You Name:", plname);
}

void nds_get_nh_event()
{
  // We don't really do anything here yet
}

void nds_exit_nhwindows(const char *str)
{
  // Should return to the main menu, here
}

void nds_suspend_nhwindows()
{
  // Not sure what I should do here, yet...
}
 
void nds_resume_nhwindows()
{
  // Again, unsure...
}

void nds_print_glyph(winid win, XCHAR_P x, XCHAR_P y, int glyph)
{
  nds_nhwindow_t *window = windows[win];

  if (window->map == NULL) {
    int x, y;

    window->map = (nds_map_t *)malloc(sizeof(nds_map_t));

    for (y = 0; y < ROWNO; y++) {
      for (x = 0; x < COLNO; x++) {
        window->map->glyphs[y][x] = -1;
      }
    }
  }

  window->map->glyphs[y][x] = glyph;
}

void nds_nhbell()
{
  iprintf("*bing*\n");
}

int nds_doprev_message()
{
  iprintf("prev message... srsly yo\n");

  return 0;
}

void nds_delay_output()
{
  // Do nothing for now
}

void nds_preference_update(const char *preferences)
{
  iprintf("nds_preference_update called...\n");
}

void nds_outrip(winid win, int thinger)
{
  iprintf("nds_outrip stub called...\n");
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
    nds_player_selection,
    nds_askname,
    nds_get_nh_event,
    nds_exit_nhwindows,
    nds_suspend_nhwindows,
    nds_resume_nhwindows,
    nds_create_nhwindow,
    nds_clear_nhwindow,
    nds_display_nhwindow,
    nds_destroy_nhwindow,
    do_null,
    nds_putstr,
    nds_display_file,
    nds_start_menu,
    nds_add_menu,
    nds_end_menu,
    nds_select_menu,
    genl_message_menu,		/* no need for X-specific handling */
    do_null,                    // nds_update_inventory,
    do_null,                    // nds_mark_synch,
    do_null,                    // nds_wait_synch,
#ifdef CLIPPING
    do_null,
#endif
#ifdef POSITIONBAR
    donull,
#endif
    nds_print_glyph,
    nds_raw_print,
    nds_raw_print_bold,
    nds_nhgetch,
    nds_nh_poskey,
    nds_nhbell,
    nds_doprev_message,
    nds_yn_function,
    nds_getlin,	
    nds_get_ext_cmd,
    nds_number_pad,
    nds_delay_output,
#ifdef CHANGE_COLOR	/* only a Mac option currently */
	donull,
	donull,
#endif
    /* other defs that really should go away (they're tty specific) */
    do_null,
    do_null,
    nds_outrip,	
    nds_preference_update,
};

