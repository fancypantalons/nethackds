#include <nds.h>
#include <stdio.h>

#include "hack.h"
#include "func_tab.h"

#include "nds_win.h"
#include "nds_cmd.h"
#include "nds_gfx.h"
#include "ppm-lite.h"
#include "nds_util.h"
#include "nds_map.h"
#include "ds_kbd.h"

#define M(c) (0x80 | (c))
#define C(c) (0x1f & (c))

#define COLOFFS  2

#define KEY_CONFIG_FILE "keys.cnf"

#define CMD_CONFIG    0xFF
#define CMD_PAN_RIGHT 0xFE
#define CMD_PAN_LEFT  0xFD
#define CMD_PAN_UP    0xFC
#define CMD_PAN_DOWN  0xFB
#define KEY_CHORD     0x00010000

/* The number of buffered command characters we'll support. */
#define INPUT_BUFFER_SIZE 32

#define CLICK_2_FRAMES 30

/* The number of available keymapping slots */
#define NUMKEYS 20

/*
 * Missing commands:
 *
 * conduct
 * ride
 * extended commands
 */

typedef struct {
  s16 f_char;
  char *name;

  int page;
  int row;
  int col;

  int x1;
  int y1;
  int x2;
  int y2;

  int focused;
  int highlighted;

  int refresh;
} nds_cmd_t;

typedef struct {
  int key;
  char *name;
} nds_key_t;

typedef char cmdset_t[NUMKEYS][INPUT_BUFFER_SIZE];

static nds_cmd_t cmdlist[] = {
	{M('a'), "Adjust"},
	{'a', "Apply"},
	{'A', "Armor"},
	{C('x'), "Attributes"},
	{'@', "Auto-pickup"},
	{'C', "Call"},
	{'Z', "Cast"},
	{M('c'), "Chat"},
	{'c', "Close"},
	{M('d'), "Dip"},
	{'\\', "Discoveries"},	
	{'>', "Down"},
	{'q', "Drink"},
	{'D', "Drop"},
	{'e', "Eat"},
	{'E', "Engrave"},
	{M('e'), "Enhance"},
        {'#', "Ex-Cmd"},
	{'/', "Ex-What Is"},
	{'X', "Explore"},
	{'f', "Fire"},
	{M('f'), "Force"},
	{'?', "Help"},
	{'V', "History"},
	{'^', "Id"},
	{'*', "In Use"},
	{'i', "Inventory"},
	{M('i'), "Invoke"},
	{M('j'), "Jump"},
        {CMD_CONFIG, "Key Config"},
	{C('d'), "Kick"},
	{':', "Look"},
	{M('l'), "Loot"},
	{M('m'), "Monster"},
	{M('n'), "Name"},
	{M('o'), "Offer"},
	{'o', "Open"},
	{'p', "Pay"},
	{',', "Pickup"},
	{M('p'), "Pray"},
	{C('p'), "Prev Mesg"},
	{'P', "Put On"},
	{'Q', "Quiver"},
	{'r', "Read"},
        {'\001', "Redo"},
	{'R', "Remove"},
	{M('r'), "Rub"},
	{'S', "Save"},
	{'s', "Search"},
	{'O', "Set"},
	{M('s'), "Sit"},
	{'x', "Swap"},
	{'T', "Take Off"},
	{C('t'), "Teleport"},
	{'t', "Throw"},
	{M('2'), "Two Weapon"},
	{M('t'), "Turn"},
	{'I', "Type-Inv"},
	{'<', "Up"},
	{M('u'), "Untrap"},
	{'v', "Version"},
	{'.', "Wait"},
	{'&', "What Does"},
	{';', "What Is"},
	{'w', "Wield"},
	{'W', "Wear"},
	{M('w'), "Wipe"},
	{'z', "Zap"},
        /*
	{WEAPON_SYM,  TRUE, doprwep},
	{ARMOR_SYM,  TRUE, doprarm},
	{RING_SYM,  TRUE, doprring},
	{AMULET_SYM, TRUE, dopramulet},
	{TOOL_SYM, TRUE, doprtool},
        */
        /*
	{GOLD_SYM, TRUE, doprgold},
	{SPBOOK_SYM, TRUE, dovspell},
        */
        {0, NULL},
        {0, NULL},
        {0, NULL},
        {0, NULL},
        {0, NULL},
        {0, NULL},
        {0, NULL},
        {0, NULL}
};

static nds_cmd_t wiz_cmdlist[] = {
#ifdef WIZARD
	{C('e'), "Wiz-Detect"},
	{C('g'), "Wiz-Genesis"},
	{C('i'), "Wiz-Identify"},
	{C('f'), "Wiz-Map"},
	{C('v'), "Wiz-Tele"},
	{C('o'), "Wiz-Where"},
	{C('w'), "Wiz-Wish"},
#endif
        {0, NULL}
};

u16 *vram = (u16 *)BG_BMP_RAM(12);

nds_cmd_t ***cmd_matrix;
u16 **cmd_pages;
int cmd_page_count;
int cmd_page_size;

int cmd_col_width = 0;
int cmd_page_rows = 0;
int cmd_page_cols = 0;

int cmd_rows = 0;
int cmd_cols = 0;

int cmd_cur_page = 0;

char input_buffer[INPUT_BUFFER_SIZE];

/* These are our direction keys, ordered ul,u,ur,l,c,r,dl,d,dr */
#define DIR_UP_LEFT    0
#define DIR_UP         1
#define DIR_UP_RIGHT   2
#define DIR_LEFT       3
#define DIR_WAIT       4
#define DIR_RIGHT      5
#define DIR_DOWN_LEFT  6
#define DIR_DOWN       7
#define DIR_DOWN_RIGHT 8

#define NUMDIRS        9

char direction_keys[NUMDIRS];

/* We use this array for indexing into the key config list */

static nds_key_t keys[NUMKEYS] = {
  { KEY_A, "A" },
  { KEY_B, "B" },
  { KEY_X, "X" },
  { KEY_Y, "Y" },
  { KEY_SELECT, "Select" },
  { KEY_START, "Start" },
  { KEY_RIGHT, "Right" },
  { KEY_LEFT, "Left" },
  { KEY_UP, "Up" },
  { KEY_DOWN, "Down" },
  { KEY_A | KEY_CHORD, "A+R" },
  { KEY_B | KEY_CHORD, "B+R" },
  { KEY_X | KEY_CHORD, "X+R" },
  { KEY_Y | KEY_CHORD, "Y+R" },
  { KEY_SELECT | KEY_CHORD, "Select+R" },
  { KEY_START | KEY_CHORD, "Start+R" },
  { KEY_RIGHT | KEY_CHORD, "Right+R" },
  { KEY_LEFT | KEY_CHORD, "Left+R" },
  { KEY_UP | KEY_CHORD, "Up+R" },
  { KEY_DOWN | KEY_CHORD, "Down+R" },
};

cmdset_t key_map = {
  ",", "s", "o", "\x4",
  "", "",
  "l", "h", "k", "j",
  "", "", "", "",
  "", "",
  "\xFE", "\xFD", "\xFC", "\xFB"
};

u16 cmd_key = KEY_L;
u16 chord_key = KEY_R;

nds_cmd_t nds_cmd_loop();
nds_cmd_t nds_kbd_cmd_loop();
void nds_load_key_config();
void nds_render_cmd_pages();

void nds_init_cmd()
{
  int i;

  if (flags.debug) {
    int idx = 0;

    for (; cmdlist[idx].name != NULL; idx++);

    for (i = 0; wiz_cmdlist[i].name != NULL; i++) {
      cmdlist[idx++] = wiz_cmdlist[i];
    }
  }

  nds_render_cmd_pages();
  nds_load_key_config();

  if (iflags.lefthanded) {
    cmd_key = KEY_R;
    chord_key = KEY_L;
  }

  memset(input_buffer, 0, sizeof(input_buffer));

  for (i = 0; i < 256 * 192 / 2; i++) {
    vram[i] = 0xFEFE;
  };

  if (iflags.num_pad) {
    direction_keys[DIR_UP_LEFT] = '7';
    direction_keys[DIR_UP] = '8';
    direction_keys[DIR_UP_RIGHT] = '9';
    direction_keys[DIR_LEFT] = '4';
    direction_keys[DIR_WAIT] = '5';
    direction_keys[DIR_RIGHT] = '6';
    direction_keys[DIR_DOWN_LEFT] = '1';
    direction_keys[DIR_DOWN] = '2';
    direction_keys[DIR_DOWN_RIGHT] = '3';
  } else {
    direction_keys[DIR_UP_LEFT] = 'y';
    direction_keys[DIR_UP] = 'k';
    direction_keys[DIR_UP_RIGHT] = 'u';
    direction_keys[DIR_LEFT] = 'h';
    direction_keys[DIR_WAIT] = '.';
    direction_keys[DIR_RIGHT] = 'l';
    direction_keys[DIR_DOWN_LEFT] = 'b';
    direction_keys[DIR_DOWN] = 'j';
    direction_keys[DIR_DOWN_RIGHT] = 'n';
  }
}

/*
 * Convert the pressed keys into a set of commands, and return the number of
 * commands we've found.
 */
int nds_get_key_cmds(int pressed, cmdset_t commands)
{
  int i;
  int numcmds = 0;

  for (i = 0; i < NUMKEYS; i++) {
    commands[i][0] = '\0';
  }

  memset(commands, 0, sizeof(commands));

  for (i = 0; i < NUMKEYS; i++) {
    /* 
     * If the scroll key is held, but this key mapping doesn't include a
     * scroll key, skip.
     */
    if ((pressed & chord_key) && ! (keys[i].key & KEY_CHORD)) {
      continue;
    } else if (! (pressed & chord_key) && (keys[i].key & KEY_CHORD)) {
      continue;
    }

    if (pressed & keys[i].key) {
      strcpy(commands[numcmds++], key_map[i]);
    }
  }

  return numcmds;
}

/*
 * Here, we look at the set of commands and the pressed key to see if we
 * should be returning a direction of some type.
 */
int nds_convert_trigger_key(int pressed, cmdset_t commands, int numcmds, char *outcmd)
{
  int dirs[NUMDIRS];
  int i, j;
  int found_dir = 0;

  memset(dirs, 0, sizeof(dirs));
  *outcmd = 0;

  for (i = 0; i < numcmds; i++) {
    for (j = 0; j < NUMDIRS; j++) {
      char tmp[2];

      tmp[0] = direction_keys[j];
      tmp[1] = '\0';

      /* If this mapped key matches this direction key... */
      if (strcmp(commands[i], tmp) == 0) {
        dirs[j] = 1;
        found_dir = 1;
      }
    }
  }

  if (found_dir && ! (pressed & iflags.triggerkey)) {
    return -1;
  } else if (! found_dir) {
    return 0;
  }

  /* 
   * Alright, at this point, we know we have one or more direction keys, and 
   * we know the trigger key has been pressed, so we need to convert to the
   * final direction key.
   */

  if (dirs[DIR_UP] && dirs[DIR_LEFT]) {
    *outcmd = direction_keys[DIR_UP_LEFT];
  } else if (dirs[DIR_UP] && dirs[DIR_RIGHT]) {
    *outcmd = direction_keys[DIR_UP_RIGHT];
  } else if (dirs[DIR_DOWN] && dirs[DIR_LEFT]) {
    *outcmd = direction_keys[DIR_DOWN_LEFT];
  } else if (dirs[DIR_DOWN] && dirs[DIR_RIGHT]) {
    *outcmd = direction_keys[DIR_DOWN_RIGHT];
  } else {
    for (i = 0; i < NUMDIRS; i++) {
      if (dirs[i]) {
        *outcmd = direction_keys[i];
        break;
      }
    }
  }

  return (*outcmd != 0);
}

/*
 * Return the individual keys which have been mapped to direction keys, so
 * that, in trigger mode, we can ignore them when we're doing our input
 * flush.
 */
int nds_get_dir_keys()
{
  int i, j;
  int keymask;

  for (i = 0; i < NUMKEYS; i++) {
    for (j = 0; j < NUMDIRS; j++) {
      char tmp[2];

      tmp[0] = direction_keys[j];
      tmp[1] = '\0';

      if (strcmp(key_map[i], tmp) == 0) {
        keymask |= keys[i].key;
      }
    }
  }
  
  return keymask;
}

int nds_map_key(u16 pressed)
{
  cmdset_t commands;
  int numcmds = nds_get_key_cmds(pressed, commands);

  if (numcmds == 0) {
    /* Of course, if no command was mapped, return nothing */

    return 0;
  } else if (iflags.triggermode) {
    /*
     * If we're in trigger mode, try to convert the mapped keys into
     * a combined direction key.
     */

    char tmpcmd;
    int res;

    res = nds_convert_trigger_key(pressed, commands, numcmds, &tmpcmd);

    if (res < 0) {
      return 0;
    } else if (res > 0) {
      return tmpcmd;
    }
  }

  /* 
   * Finally, if nothing else, we take the first mapped command, get the second
   * and remaining characters, stuff them in our input buffer, and then return
   * the first character of the command.
   */

  strcpy(input_buffer, &(commands[0][1]));

  return commands[0][0];
}

void nds_load_key_config()
{
  FILE *fp = fopen(fqname(KEY_CONFIG_FILE, CONFIGPREFIX, 0), "r");
  u8 buffer[sizeof(key_map)];
  int cnt = sizeof(key_map);
  int ret;

  if (fp == (FILE *)0) {
    return;
  }

  if ((ret = fread(buffer, 1, cnt, fp)) < cnt) {
    iprintf("Only got %d, wanted %d\n", ret, cnt);
    return;
  } 
  
  memcpy(key_map, buffer, cnt);

  fclose(fp);
}

void nds_save_key_config()
{
  FILE *fp = fopen(fqname(KEY_CONFIG_FILE, CONFIGPREFIX, 0), "w");

  if (fp == (FILE *)0) {
    return;
  }

  fwrite(key_map, 1, sizeof(key_map), fp);

  fclose(fp);
}

nds_cmd_t nds_get_config_cmd()
{
  winid win;
  menu_item *sel;
  ANY_P ids[15];
  nds_cmd_t cmd;

  win = create_nhwindow(NHW_MENU);
  start_menu(win);

  ids[0].a_int = 0;
  ids[1].a_int = direction_keys[DIR_UP_LEFT];
  ids[2].a_int = direction_keys[DIR_UP];
  ids[3].a_int = direction_keys[DIR_UP_RIGHT];
  ids[4].a_int = direction_keys[DIR_LEFT];
  ids[5].a_int = direction_keys[DIR_RIGHT];
  ids[6].a_int = direction_keys[DIR_DOWN_LEFT];
  ids[7].a_int = direction_keys[DIR_DOWN];
  ids[8].a_int = direction_keys[DIR_DOWN_RIGHT];
  ids[9].a_int = 1;
  ids[10].a_int = CMD_PAN_UP;
  ids[11].a_int = CMD_PAN_DOWN;
  ids[12].a_int = CMD_PAN_LEFT;
  ids[13].a_int = CMD_PAN_RIGHT;
  ids[14].a_int = 2;

  add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Direction Keys", 0);

  add_menu(win, NO_GLYPH, &(ids[1]), 0, 0, 0, "Up-Left", 0);
  add_menu(win, NO_GLYPH, &(ids[2]), 0, 0, 0, "Up", 0);
  add_menu(win, NO_GLYPH, &(ids[3]), 0, 0, 0, "Up-Right", 0);
  add_menu(win, NO_GLYPH, &(ids[4]), 0, 0, 0, "Left", 0);
  add_menu(win, NO_GLYPH, &(ids[5]), 0, 0, 0, "Right", 0);
  add_menu(win, NO_GLYPH, &(ids[6]), 0, 0, 0, "Down-Left", 0);
  add_menu(win, NO_GLYPH, &(ids[7]), 0, 0, 0, "Down", 0);
  add_menu(win, NO_GLYPH, &(ids[8]), 0, 0, 0, "Down-Right", 0);

  add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Other", 0);

  add_menu(win, NO_GLYPH, &(ids[9]), 0, 0, 0, "Game Command", 0);
  add_menu(win, NO_GLYPH, &(ids[10]), 0, 0, 0, "Pan Up", 0);
  add_menu(win, NO_GLYPH, &(ids[11]), 0, 0, 0, "Pan Down", 0);
  add_menu(win, NO_GLYPH, &(ids[12]), 0, 0, 0, "Pan Left", 0);
  add_menu(win, NO_GLYPH, &(ids[13]), 0, 0, 0, "Pan Right", 0);
  add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, " ", 0);
  add_menu(win, NO_GLYPH, &(ids[14]), 0, 0, 0, "No Command", 0);

  end_menu(win, "What do you want to assign to this key?");

  if (select_menu(win, PICK_ONE, &sel) <= 0) {
    cmd.f_char = -1;
    cmd.name = NULL;
  } else if (sel->item.a_int == 1) {
    nds_flush(0);
    cmd = nds_cmd_loop(1);
  } else if (sel->item.a_int == 2) {
    nds_flush(0);

    cmd.f_char = 0;
    cmd.name = NULL;
  } else {
    cmd.f_char = sel->item.a_int;

    if (cmd.f_char == direction_keys[DIR_UP_LEFT]) {
      cmd.name = "Move Up-Left";
    } else if (cmd.f_char == direction_keys[DIR_UP]) {
      cmd.name = "Move Up";
    } else if (cmd.f_char == direction_keys[DIR_UP_RIGHT]) {
      cmd.name = "Move Up-Right";
    } else if (cmd.f_char == direction_keys[DIR_LEFT]) {
      cmd.name = "Move Left";
    } else if (cmd.f_char == direction_keys[DIR_RIGHT]) {
      cmd.name = "Move Right";
    } else if (cmd.f_char == direction_keys[DIR_DOWN_LEFT]) {
      cmd.name = "Move Down-Left";
    } else if (cmd.f_char == direction_keys[DIR_DOWN]) {
      cmd.name = "Move Down";
    } else if (cmd.f_char == direction_keys[DIR_RIGHT]) {
      cmd.name = "Move Down-Right";
    } else if (cmd.f_char == CMD_PAN_UP) {
      cmd.name = "Pan Up";
    } else if (cmd.f_char == CMD_PAN_DOWN) {
      cmd.name = "Pan Down";
    } else if (cmd.f_char == CMD_PAN_LEFT) {
      cmd.name = "Pan Left";
    } else if (cmd.f_char == CMD_PAN_RIGHT) {
      cmd.name = "Pan Right";
    } else {
      cmd.name = NULL;
    }
  }

  NULLFREE(sel);

  destroy_nhwindow(win);

  return cmd;
}

void nds_config_key()
{
  int i;
  int held;
  nds_key_t key = { 0, NULL };
  nds_cmd_t cmd;
  char buf[BUFSZ];
  char tmp[2];

  nds_flush(0);

  nds_draw_prompt("Press the key to modify.");

  while (1) {
    swiWaitForVBlank();

    scanKeys();

    held = keysHeld();

    /* We don't let the user configure these */

    if ((held & cmd_key) ||
        (held == chord_key)) {
      continue;
    } else if (held) {
      for (i = 0; i < NUMKEYS; i++) {
        if ((held & chord_key) && ! (keys[i].key & KEY_CHORD)) {
          continue;
        }

        if (held & keys[i].key) {
          key = keys[i];
          goto HAVEKEY;
        }
      }
    }
  }

HAVEKEY:

  nds_clear_prompt();

  cmd = nds_get_config_cmd();

  if (cmd.f_char < 0) {
    return;
  }

  tmp[0] = cmd.f_char;
  tmp[1] = '\0';

  strcpy(key_map[i], tmp);

  if (*input_buffer) {
    strcat(key_map[i], input_buffer);
    strcpy(input_buffer, key_map[i]);

    input_buffer[strlen(input_buffer) - 1] = '\0';

    sprintf(buf, "Mapped %s to %s x%s (%s).", key.name, cmd.name, input_buffer, key_map[i]);

    input_buffer[0] = '\0';
  } else {
    sprintf(buf, "Mapped %s to %s.", key.name, cmd.name);
  }

  clear_nhwindow(WIN_MESSAGE);
  putstr(WIN_MESSAGE, ATR_NONE, buf);

  nds_save_key_config();

  nds_flush(0);
}

int nds_handle_click(int px, int py, int *x, int *y, int *mod)
{
  int mx, my;
  int ch;

  nds_map_translate_coords(px, py, &mx, &my);

  /* 
   * If the click is somewhere around the user, or we're not in compass
   * mode, pass the click back to the game engine directly.
   */
  if ( ( ((ABS(mx - u.ux) <= 1) && (ABS(my - u.uy) <= 1)) && (iflags.compassmode != 2) ) ||
       (iflags.compassmode == 0) ) {
    *x = mx;
    *y = my;
    *mod = CLICK_1;

    ch = 0;
  } else {
    int tmp_x = px;
    int tmp_y = py;
    int dist;

    if (iflags.compassmode == 1) {
      nds_map_relativize(&tmp_x, &tmp_y);
    } else {
      tmp_x -= 128;
      tmp_y -= 96;
    }

    dist = tmp_x * tmp_x + tmp_y * tmp_y;

    /* 
     * Here we take the click location and convert it to a direction key
     * based on a movement compass.
     */

    if (ABS(tmp_x) > 2 * ABS(tmp_y)) {
      ch = (tmp_x > 0) ? direction_keys[DIR_RIGHT] : direction_keys[DIR_LEFT];
    } else if (ABS(tmp_y) > 2 * ABS(tmp_x)) {
      ch = (tmp_y > 0) ? direction_keys[DIR_DOWN] : direction_keys[DIR_UP];
    } else if (tmp_y > 0) {
      ch = (tmp_x > 0) ? direction_keys[DIR_DOWN_RIGHT] : direction_keys[DIR_DOWN_LEFT];
    } else {
      ch = (tmp_x > 0) ? direction_keys[DIR_UP_RIGHT] : direction_keys[DIR_UP_LEFT];
    }

    if (dist > 1024) {
      input_buffer[0] = ch;
      ch = 'g';
    }
  }

  return ch;
}

int nds_get_input(int *x, int *y, int *mod)
{
  int tx, ty, old_tx, old_ty, start_tx, start_ty;
  int cx, cy;

  touchPosition coords;
  int held_frames = 0;
  int dragging = 0;

  /* Set one, initialize these! */

  *x = 0;
  *y = 0;
  *mod = 0;

  /* Initialize our own tracking variables */
  
  tx = 0;
  ty = 0;
  old_tx = 0;
  old_ty = 0;
  start_tx = 0;
  start_ty = 0;

  cx = -1;
  cy = -1;

  /* 
   * If there was characters stuffed in our input buffer, return one from
   * there, instead.
   */

  if (input_buffer[0] != 0) {
    int key = input_buffer[0];

    memmove(&(input_buffer[0]), &(input_buffer[1]), (INPUT_BUFFER_SIZE - 1) * sizeof(input_buffer[0]));

    input_buffer[INPUT_BUFFER_SIZE - 1] = 0;

    return key;
  }

  nds_flush(~KEY_TOUCH);

  while(1) {
    int key = 0;
    int pressed;
    int held;

    scanKeys();

    pressed = keysDownRepeat();
    held = keysHeld();
    scan_touch_screen();

    old_tx = tx;
    old_ty = ty;

    if (held & KEY_TOUCH) {
      coords = get_touch_coords();

      tx = coords.px / nds_map_tile_width();
      ty = coords.py / nds_map_tile_height();

      if ((start_tx == 0) && (start_ty == 0)) {
        start_tx = tx;
        start_ty = ty;
      }
    } else {
      tx = 0;
      ty = 0;
    }

    swiWaitForVBlank();

    if (pressed & cmd_key) {
      nds_cmd_t cmd;
      
      if (iflags.cmdwindow) {
        cmd = nds_cmd_loop(0);
      } else {
        cmd = nds_kbd_cmd_loop();
      }

      key = cmd.f_char;
    } else if (pressed) {
      key = nds_map_key(held | pressed);
    }

    switch (key) {
      case 0:
        break;

      case CMD_PAN_UP:
      case CMD_PAN_DOWN:
      case CMD_PAN_LEFT:
      case CMD_PAN_RIGHT:
        {
          nds_map_get_center(&cx, &cy);

          if (key == CMD_PAN_UP) {
            cy--;
          } else if (key == CMD_PAN_DOWN) {
            cy++;
          } else if (key == CMD_PAN_LEFT) {
            cx--;
          } else if (key == CMD_PAN_RIGHT) {
            cx++;
          }

          nds_draw_map(windows[WIN_MAP]->map, &cx, &cy);
        }

        break;

      case CMD_CONFIG:
        nds_config_key();
        break;

      default:
        return key;
    }
    
    if ((tx != 0) && (ty != 0) && (start_tx != 0) && (start_ty != 0) &&
        ((old_tx != tx) || (old_ty != ty))) {

      int diff_x = start_tx - tx;
      int diff_y = start_ty - ty;

      if ((ABS(diff_x) > 1) || (ABS(diff_y) > 1)) {
        int new_cx, new_cy;

        if ((cx < 0) && (cy < 0)) {
          nds_map_get_center(&cx, &cy);
        }

        dragging = 1;

        new_cx = cx + diff_x;
        new_cy = cy + diff_y;

        nds_draw_map(windows[WIN_MAP]->map, &new_cx, &new_cy);
      }
    }

    if (dragging && (tx == 0) && (ty == 0) && (old_tx == 0) && (old_ty == 0)) {
      start_tx = 0;
      start_ty = 0;
      cx = -1;
      cy = -1;

      dragging = 0;
    } else if (dragging) {
      continue;
    }
    
    if (get_tap_coords(&coords)) {
      return nds_handle_click(coords.px, coords.py, x, y, mod);
    }
    
    if (held_frames > CLICK_2_FRAMES) {
      nds_map_translate_coords(coords.px, coords.py, x, y);

      *mod = CLICK_2;

      return 0;
    } else if (held & KEY_TOUCH) {
      held_frames++;
    } else {
      held_frames = 0;
    }
  }

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
  int allow_none = 0;

  if (choices != NULL) {
    iprintf("yn_function choices '%s'\n", choices);
  }

  /* We're being asked for a direction... this is special. */
  if (strstr(ques, "Adjust letter to what") != NULL) {
    return nds_prompt_char(ques, choices, 0);
  } else if ((strstr(ques, "In what direction") != NULL) ||
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
      sym = nds_get_input(&x, &y, &mod);
      nds_clear_prompt();

      if (mod == CLICK_1) {
        if ((x == u.ux - 1) && (y == u.uy - 1)) {
          return direction_keys[DIR_UP_LEFT];
        } else if ((x == u.ux) && (y == u.uy - 1)) {
          return direction_keys[DIR_UP];
        } else if ((x == u.ux + 1) && (y == u.uy - 1)) {
          return direction_keys[DIR_UP_RIGHT];
        } else if ((x == u.ux - 1) && (y == u.uy)) {
          return direction_keys[DIR_LEFT];
        } else if ((x == u.ux) && (y == u.uy)) {
          return direction_keys[DIR_WAIT];
        } else if ((x == u.ux + 1) && (y == u.uy)) {
          return direction_keys[DIR_RIGHT];
        } else if ((x == u.ux - 1) && (y == u.uy + 1)) {
          return direction_keys[DIR_DOWN_LEFT];
        } else if ((x == u.ux) && (y == u.uy + 1)) {
          return direction_keys[DIR_DOWN];
        } else if ((x == u.ux + 1) && (y == u.uy + 1)) {
          return direction_keys[DIR_DOWN_RIGHT];
        }
      } else if (mod == CLICK_2) {
        if ((x == u.ux) && (y == u.uy)) {
          return '>';
        }
      } else {
        return sym;
      }
    }
  }

  allow_none = (strstr(ques, "[- ") != NULL);

  if ((choices == NULL) && ! allow_none) {
    return '*';
  }

  win = create_nhwindow(NHW_MENU);

  start_menu(win);
  
  if (allow_none) {
    ids[0].a_int = '*';
    ids[1].a_int = '-';

    add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Something from your inventory", 0);
    add_menu(win, NO_GLYPH, &(ids[1]), 0, 0, 0, "Your finger", 0);
  } else if ((strcasecmp(choices, ynchars) == 0) ||
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

void nds_render_cmd_pages()
{
  int cur_page = -1;
  int cur_row = 0;
  int cur_col = 0;
  int xoffs = 0;
  int yoffs = 0;

  int i;
  struct ppm *img;

  int cmd_cnt;
  int cmd_x, cmd_y;
  int char_w;

  /* 
   * First, let's compute some dimensions.  We'll need the column width, which
   * amounts to the size of the largest command, along with the total number
   * of commands in the list.
   */
  for (i = 0, cmd_cnt = 0; cmdlist[i].name != NULL; i++, cmd_cnt++) {
    int str_w;

    text_dims(system_font, cmdlist[i].name, &str_w, NULL);

    if (str_w > cmd_col_width) {
      cmd_col_width = str_w;
    }
  }

  text_dims(system_font, "#", &char_w, NULL);

  cmd_col_width += char_w;

  /* 
   * With the above, we can get the number of rows and columns per page, and
   * then the total number of pages.
   */
  cmd_page_cols = 256 / cmd_col_width;
  cmd_page_rows = 192 / system_font->height;

  if ((cmd_page_rows * cmd_page_cols) < cmd_cnt) {
    cmd_page_rows = (192 - up_arrow->height - down_arrow->height) / system_font->height;
  }

  cmd_page_count = cmd_cnt / (cmd_page_rows * cmd_page_cols);

  if ((cmd_page_count * cmd_page_rows * cmd_page_cols) < cmd_cnt) {
    cmd_page_count++;
  }

  cmd_pages = (u16 **)malloc(cmd_page_count * sizeof(u16 *));
  cmd_page_size = 256 * (cmd_page_rows * system_font->height);

  for (i = 0; i < cmd_page_count; i++) {
    cmd_pages[i] = (u16 *)malloc(cmd_page_size);
    memset(cmd_pages[i], 254, cmd_page_size);
  }

  /* Now initialize our command matrix */
  cmd_cols = cmd_page_cols;
  cmd_rows = cmd_cnt / cmd_cols;

  if ((cmd_rows * cmd_cols) < cmd_cnt) {
    cmd_rows++;
  }

  cmd_matrix = (nds_cmd_t ***)malloc(cmd_rows * sizeof(nds_cmd_t **));

  for (cur_row = 0; cur_row < cmd_rows; cur_row++) {
    cmd_matrix[cur_row] = (nds_cmd_t **)malloc(cmd_cols * sizeof(nds_cmd_t *));

    for (cur_col = 0; cur_col < cmd_cols; cur_col++) {
      cmd_matrix[cur_row][cur_col] = NULL;
    }
  }

  xoffs = 128 - (cmd_page_cols * cmd_col_width) / 2;

  if (cmd_page_count > 1) {
    yoffs = up_arrow->height;
  } else {
    yoffs = 192 / 2 - (cmd_rows * system_font->height) / 2;
  }

  /*
   * In this loop:
   *   i       - the index into the command lits.
   *   cur_row - the current page row.
   *   cmd_y   - the current command matrix row.
   *   cur_col - the current page column.
   *   cmd_x   - the current command matrix column.
   */
  img = alloc_ppm(cmd_col_width, system_font->height);

  for (i = 0, cur_page = 0; cur_page < cmd_page_count; cur_page++) {
    for (cur_col = 0, cmd_x = 0; cur_col < cmd_page_cols; cur_col++, cmd_x++) {
      for (cur_row = 0, cmd_y = cmd_page_rows * cur_page; (cur_row < cmd_page_rows) && (cmd_y < cmd_rows); cur_row++, cmd_y++, i++) {
        nds_cmd_t *cur_cmd;

        if (i >= cmd_cnt) {
          goto RENDER_DONE;
        }

        cmd_matrix[cmd_y][cmd_x] = &(cmdlist[i]);
        cur_cmd = cmd_matrix[cmd_y][cmd_x];

        cur_cmd->page = cur_page;
        cur_cmd->row = cmd_y;
        cur_cmd->col = cmd_x;
        cur_cmd->refresh = 0;

        cur_cmd->x1 = cur_col * cmd_col_width + xoffs;
        cur_cmd->y1 = cur_row * system_font->height + yoffs;
        cur_cmd->x2 = cur_cmd->x1 + cmd_col_width;
        cur_cmd->y2 = cur_cmd->y1 + system_font->height;

        clear_ppm(img, MAP_COLOUR(CLR_BLACK));
        draw_string(system_font, cur_cmd->name, img,
                    0, 0, -1, -1);
        draw_ppm(img, cmd_pages[cur_page], cur_cmd->x1, cur_cmd->y1 - yoffs, 256);
      }
    }
  }

RENDER_DONE:

  free_ppm(img);
}

nds_cmd_t *nds_find_command(int x, int y)
{
  int i;

  for (i = 0; cmdlist[i].name != NULL; i++) {
    if ((x >= cmdlist[i].x1) && (x <= cmdlist[i].x2) &&
        (y >= cmdlist[i].y1) && (y <= cmdlist[i].y2) &&
        (cmdlist[i].page == cmd_cur_page)) {

      return &(cmdlist[i]);
    }
  }

  return NULL;
}

nds_cmd_t *nds_find_command_row_col(int row, int col)
{
  int i;

  for (i = 0; cmdlist[i].name != NULL; i++) {
    if ((row == cmdlist[i].row) && 
        (col == cmdlist[i].col)) {

      return &(cmdlist[i]);
    }
  }

  return NULL;
}

void nds_repaint_cmds()
{
  static struct ppm *img = NULL;
  int i;

  if (img == NULL) {
    img = alloc_ppm(cmd_col_width, system_font->height);
  }

  for (i = 0; cmdlist[i].name != NULL; i++) {
    if (cmdlist[i].refresh && (cmd_cur_page == cmdlist[i].page)) {
      int fg, bg;

      fg = (cmdlist[i].highlighted) ? CLR_BRIGHT_GREEN : CLR_WHITE;
      bg = (cmdlist[i].focused) ? CLR_BLUE : CLR_BLACK;

      clear_ppm(img, MAP_COLOUR(bg));

      draw_string(system_font, cmdlist[i].name,
                  img, 0, 0,
                  fg, bg);

      draw_ppm(img, vram, 
               cmdlist[i].x1, cmdlist[i].y1,
               256);
    }

    cmdlist[i].refresh = 0;
  }
}

nds_cmd_t *nds_cmd_loop_check_keys(int pressed, nds_cmd_t *curcmd, int *refresh)
{
  int row, col;
  nds_cmd_t *newcmd;

  int up_arrow_x1 = 256 / 2 - up_arrow->width / 2;
  int up_arrow_y1 = 0;
  int up_arrow_x2 = 256 / 2 + up_arrow->width / 2;
  int up_arrow_y2 = up_arrow->height;

  int down_arrow_x1 = 256 / 2 - down_arrow->width / 2;
  int down_arrow_y1 = 192 - down_arrow->height;
  int down_arrow_x2 = 256 / 2 + down_arrow->width / 2;
  int down_arrow_y2 = 192;

  int key_pressed = (pressed & KEY_DOWN) ||
                    (pressed & KEY_UP) ||
                    (pressed & KEY_LEFT) ||
                    (pressed & KEY_RIGHT);

  if ((cmd_page_count > 1) &&
      touch_released_in(up_arrow_x1, up_arrow_y1, 
                        up_arrow_x2, up_arrow_y2) && 
      (cmd_cur_page != 0))  {

    cmd_cur_page--;
  } else if ((cmd_page_count > 1) &&
             touch_released_in(down_arrow_x1, down_arrow_y1, 
                               down_arrow_x2, down_arrow_y2) && 
             (cmd_cur_page < (cmd_page_count - 1)))  {

    cmd_cur_page++;
  }

  if (key_pressed && ((curcmd == NULL) || (curcmd->page != cmd_cur_page))) {
    newcmd = nds_find_command_row_col(cmd_cur_page * cmd_page_rows, 0);

    newcmd->focused = 1;
    newcmd->refresh = 1;

    *refresh = 1;

    return newcmd;
  } else if (! key_pressed) {
    newcmd = ((curcmd != NULL) && (curcmd->page == cmd_cur_page)) ? curcmd : NULL; 
    *refresh = (newcmd != curcmd);

    return newcmd;
  }

  row = curcmd->row;
  col = curcmd->col;

  if (pressed & KEY_UP) {
    row--;
  } else if (pressed & KEY_DOWN) {
    row++;
  } else if (pressed & KEY_LEFT) {
    col--;
  } else if (pressed & KEY_RIGHT) {
    col++;
  }

  if (row < 0) {
    row = cmd_rows - 1;
  } else if (row >= cmd_rows) {
    row = 0;
  }

  if (col < 0) {
    col = cmd_cols - 1;
  } else if (col >= cmd_cols) {
    col = 0;
  }

  newcmd = nds_find_command_row_col(row, col);

  if (newcmd == NULL) {
    newcmd = curcmd;
  } else if (newcmd != curcmd) {
    curcmd->highlighted = 0;
    curcmd->focused = 0;
    curcmd->refresh = 1;

    newcmd->focused = 1;
    newcmd->refresh = 1;

    *refresh = 1;

    cmd_cur_page = newcmd->page;
  }

  return newcmd;
}

nds_cmd_t nds_cmd_loop(int in_config)
{
  static int refresh = 0;
  static nds_cmd_t *curcmd = NULL;
  static int displayed_page = -1;

  nds_cmd_t *tapped_cmd = curcmd;
  nds_cmd_t *tmpcmd;
  nds_cmd_t picked_cmd;
  u16 old_bg_cr;
  int held_frames = 0;

  touchPosition coords = { .x = 0, .y = 0 };

  int prev_held = 0;
  int held = 0;

  /* 
   * Initialize our display.
   */

  /* Now get the display set up. */

  old_bg_cr = BG2_CR;

  BG2_CR = BG_BMP8_256x256 | BG_BMP_BASE(12) | BG_PRIORITY_1;
  DISPLAY_CR |= DISPLAY_BG2_ACTIVE;

  picked_cmd.f_char = 0;
  picked_cmd.name = NULL;

  /*
   * Now, we loop until either a command is tapped and selected, or the left
   * button is released.
   */
  while (1) {
    int pressed;

    swiWaitForVBlank();

    /* If the page has changed, we need to render the new one. */

    if (cmd_cur_page != displayed_page) {
      dmaCopy(cmd_pages[cmd_cur_page], vram + 128 * cmdlist[0].y1, cmd_page_size);

      if (cmd_page_count > 1) {
        nds_draw_rect(0, 0, 256, up_arrow->height, 254, vram);
        nds_draw_rect(0, 192 - up_arrow->height, 256, up_arrow->height, 254, vram);

        if (cmd_cur_page != 0) {
          draw_ppm(up_arrow, vram, 
                   256 / 2 - up_arrow->width / 2, 0,
                   256);
        }      

        if (cmd_cur_page < (cmd_page_count - 1)) {
          draw_ppm(down_arrow, vram, 
                   256 / 2 - down_arrow->width / 2, 192 - down_arrow->height,
                   256);
        }
      }

      displayed_page = cmd_cur_page;
    }

    /* If any of the items have changed, repaint */

    if (refresh) {
      nds_repaint_cmds();

      refresh = 0;
    }

    scan_touch_screen();
    scanKeys();

    prev_held = held;

    pressed = keysDownRepeat();
    held = keysHeld();
    coords = get_touch_coords();

    if (((in_config || ! iflags.holdmode) && (pressed & KEY_B)) ||
        ((! in_config && ! (held & cmd_key) && iflags.holdmode)) ||
        (! iflags.holdmode && (pressed & cmd_key)) ) {

      nds_flush(0);

      break;
    } else if (((held & KEY_A) || (held & KEY_X)) && curcmd) {
      curcmd->highlighted = 1;
      curcmd->refresh = 1;

      refresh = 1;
    } else if (((prev_held & KEY_A) || (prev_held & KEY_X)) && curcmd) {
      picked_cmd = *curcmd;

      break;
    }

    if ((tmpcmd = nds_cmd_loop_check_keys(pressed, curcmd, &refresh)) != NULL) {
      if (curcmd != tmpcmd) {
        curcmd = tmpcmd;
        tapped_cmd = curcmd;
      }
    }

    if (held & KEY_TOUCH) {
      nds_cmd_t *cmd = nds_find_command(coords.px, coords.py);

      if (cmd != curcmd) {
        if (curcmd) {
          curcmd->highlighted = 0;
          curcmd->focused = 0;
          curcmd->refresh = 1;
        }

        if (cmd) {
          cmd->highlighted = 1;
          cmd->focused = 1;
          cmd->refresh = 1;
        }

        curcmd = cmd;

        refresh = 1;
        held_frames = 0;
      } else if (curcmd == NULL) {
        held_frames = 0;
      } else if (held_frames > CLICK_2_FRAMES) {
        char buffer[BUFSZ];
        char *bufptr = buffer;
        int len;

        getlin("Enter Repeat Count", bufptr);

        if (*bufptr) {
          if (iflags.num_pad) {
            picked_cmd.name = curcmd->name;
            picked_cmd.f_char = 'n';
          } else {
            picked_cmd.name = curcmd->name;
            picked_cmd.f_char = bufptr[0];
            bufptr++;
          }

          len = strlen(bufptr);

          strcpy(input_buffer, bufptr);
          input_buffer[len] = curcmd->f_char;
          input_buffer[len + 1] = '\0';

          break;
        } else {
          curcmd->highlighted = 0;
          curcmd->refresh = 1;

          refresh = 1;

          nds_flush(0);
        }

        held_frames = 0;
      } else {
        held_frames++;
      }
    } else if (get_tap_coords(&coords)) {
      nds_cmd_t *cmd = nds_find_command(coords.px, coords.py);

      refresh = 1;
      held_frames = 0;

      if (iflags.doubletap && (tapped_cmd != cmd)) {
        tapped_cmd = cmd;
        tapped_cmd->highlighted = 0;
        tapped_cmd->refresh = 1;
      } else if (cmd) {
        picked_cmd = *cmd;

        break;
      } else if (curcmd) {
        curcmd->focused = 0;
        curcmd->highlighted = 0;
        curcmd->refresh = 1;
      }
    }
  }

  /*
   * This happens if the user releases L while pressing a command.  Basically,
   * we want to remove the item highlight.
   */

  if (curcmd) {
    curcmd->highlighted = 0;
    curcmd->refresh = 1;

    refresh = 1;
  }

  /* 
   * If an extended command was requested, we need to get it from the
   * user.
   */
  if (picked_cmd.f_char == '#') {
    int idx = get_ext_cmd();

    if (idx >= 0) {
      /* Now stuff the command into our input buffer. */

      strcpy(input_buffer, extcmdlist[idx].ef_txt);
    } else {
      picked_cmd.f_char = 0;
      picked_cmd.name = NULL;
    }
  }

  DISPLAY_CR ^= DISPLAY_BG2_ACTIVE;
  BG2_CR = old_bg_cr;

  nds_flush(0);

  return picked_cmd;
}

nds_cmd_t nds_kbd_cmd_loop()
{
  nds_cmd_t cmd = { 0, NULL };
  int key = nds_prompt_char(NULL, NULL, cmd_key);

  switch (key) {
    case K_UP_LEFT:
      key = (iflags.num_pad) ? '7' : 'y';
      break;

    case K_UP:
      key = (iflags.num_pad) ? '8' : 'k';
      break;

    case K_UP_RIGHT:
      key = (iflags.num_pad) ? '9' : 'u';
      break;

    case K_LEFT:
      key = (iflags.num_pad) ? '4' : 'h';
      break;

    case K_NOOP:
      key = (iflags.num_pad) ? '5' : '.';
      break;

    case K_RIGHT:
      key = (iflags.num_pad) ? '6' : 'l';
      break;

    case K_DOWN_LEFT:
      key = (iflags.num_pad) ? '1' : 'b';
      break;

    case K_DOWN:
      key = (iflags.num_pad) ? '2' : 'j';
      break;

    case K_DOWN_RIGHT:
      key = (iflags.num_pad) ? '3' : 'n';
      break;

    default:
      if (key == 1) {
        key = CMD_CONFIG;
      }

      break;
  }

  cmd.f_char = key;
  cmd.name = "Dummy";

  nds_flush(0);

  return cmd;
}

int nds_get_ext_cmd()
{
  char buffer[BUFSZ];
  int i;

  if (*input_buffer) {
    strcpy(buffer, input_buffer);
    *input_buffer = '\0';
  } else {
    getlin("Extended Command", buffer);
  }

  for (i = 0; extcmdlist[i].ef_txt != NULL; i++) {
    if (strcmp(extcmdlist[i].ef_txt, buffer) == 0) {
      return i;
    }
  }

  return -1;
} 

void nds_number_pad(int thinger)
{
}
