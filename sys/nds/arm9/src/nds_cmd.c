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

#define COLWIDTH 60
#define COLOFFS  4

#define KEY_CONFIG_FILE "keys.cnf"

#define CMD_CONFIG     0x0100

#define INPUT_BUFFER_SIZE 32

/*
 * Missing commands:
 *
 * conduct
 * ride
 * extended commands
 */

typedef struct {
  u16 f_char;
  char *name;

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

static nds_cmd_t cmdlist[] = {
//	{C('p'), TRUE, doprev_message},
	{M('a'), "Adjust"},
	{'a', "Apply"},
	{'A', "Armor"},
	{C('x'), "Attributes"},
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
	{'o', "Open"},
	{'p', "Pay"},
	{',', "Pickup"},
	{M('p'), "Pray"},
	{'P', "Put On"},
	{'Q', "Quiver"},
	{'r', "Read"},
        {'\001', "Redo"},
	{'R', "Remove"},
	{M('r'), "Rub"},
	{M('o'), "Sacrifice"},
	{'S', "Save"},
	{'s', "Search"},
	{'O', "Set"},
	{M('s'), "Sit"},
	{'x', "Swap"},
	{'T', "Take Off"},
	{C('t'), "Teleport"},
	{'t', "Throw"},
//	{'@', "Toggle Pickup"},
	{M('2'), "Two Weapon"},
	{M('t'), "Turn"},
	{'I', "Type-Inv"},
	{'<', "Up"},
	{M('u'), "Untrap"},
//	{'v', "Version"},
	{'.', "Wait"},
	{'&', "What Does"},
	{';', "What Is"},
	{'w', "Wield"},
	{'W', "Wear"},
	{M('w'), "Wipe"},
	{'z', "Zap"},
//	{'/', "What Is"},
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

int cmd_rows;
int cmd_cols;

int input_buffer[INPUT_BUFFER_SIZE];

/* We use this array for indexing into the key config list */

static nds_key_t keys[] = {
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
  { -1, NULL }
};

u16 key_map[] = {
  ',', 's', 'o', C('d'),
  0, 0,
  'l', 'h', 'k', 'j'
};

u16 *vram = (u16 *)BG_BMP_RAM(12);

u16 cmd_key = KEY_L;
u16 scroll_key = KEY_R;

nds_cmd_t nds_cmd_loop();
nds_cmd_t nds_kbd_cmd_loop();
void nds_load_key_config();

void nds_init_cmd()
{
  int cur_x = COLOFFS;
  int cur_y = 0;

  int cur_row = 0;
  int cur_col = 0;

  int i;
  struct ppm *img = alloc_ppm(256, 192);

  if (flags.debug) {
    int idx = 0;

    for (; cmdlist[idx].name != NULL; idx++);

    for (i = 0; wiz_cmdlist[i].name != NULL; i++) {
      cmdlist[idx++] = wiz_cmdlist[i];
    }
  }

  for (i = 0; cmdlist[i].name != NULL; i++) {
    int text_h;

    text_dims(system_font, cmdlist[i].name, NULL, &text_h);

    if ((cur_y + text_h) > 192) {
      cmd_rows = cur_row + 1;

      cur_col++;
      cur_x += COLWIDTH + COLOFFS;

      cur_row = 0;
      cur_y = 0;
    }

    draw_string(system_font, cmdlist[i].name, img,
                cur_x, cur_y, 1,
                255, 0, 255);

    cmdlist[i].row = cur_row;
    cmdlist[i].col = cur_col;

    cmdlist[i].x1 = cur_x;
    cmdlist[i].x2 = cur_x + COLWIDTH;
    cmdlist[i].y1 = cur_y;
    cmdlist[i].y2 = cur_y + text_h;

    cur_row++;

    if (flags.debug) {
      cur_y += text_h;
    } else {
      cur_y += text_h + 2;
    }
  }

  draw_ppm_bw(img, vram, 0, 0, 256, 254, 255);

  free_ppm(img);

  cmd_cols = cur_col + 1;

  nds_load_key_config();

  if (iflags.lefthanded) {
    cmd_key = KEY_R;
    scroll_key = KEY_L;
  }

  memset(input_buffer, 0, sizeof(input_buffer));
}

int nds_map_key(u16 pressed)
{
  int i;

  for (i = 0; keys[i].key > 0; i++) {
    if (pressed & keys[i].key) {
      return key_map[i];
    }
  }
  
  return 0;
}

void nds_load_key_config()
{
  FILE *fp = fopen(fqname(KEY_CONFIG_FILE, CONFIGPREFIX, 0), "r");
  u8 buffer[BUFSZ];
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
  ANY_P ids[6];
  nds_cmd_t cmd;

  win = create_nhwindow(NHW_MENU);
  start_menu(win);

  ids[0].a_int = 0;
  ids[1].a_int = 'k';
  ids[2].a_int = 'j';
  ids[3].a_int = 'h';
  ids[4].a_int = 'l';
  ids[5].a_int = 1;

  add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Direction Keys", 0);

  add_menu(win, NO_GLYPH, &(ids[1]), 0, 0, 0, "Up", 0);
  add_menu(win, NO_GLYPH, &(ids[2]), 0, 0, 0, "Down", 0);
  add_menu(win, NO_GLYPH, &(ids[3]), 0, 0, 0, "Left", 0);
  add_menu(win, NO_GLYPH, &(ids[4]), 0, 0, 0, "Right", 0);

  add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Other", 0);

  add_menu(win, NO_GLYPH, &(ids[5]), 0, 0, 0, "Game Command", 0);

  end_menu(win, "What do you want to assign to this key?");

  if (select_menu(win, PICK_ONE, &sel) <= 0) {
    cmd.f_char = 0;
    cmd.name = NULL;
  } else if (sel->item.a_int == 1) {
    cmd = nds_cmd_loop(1);
  } else {
    cmd.f_char = sel->item.a_int;

    switch (cmd.f_char) {
      case 'k':
        cmd.name = "Move Up";
        break;

      case 'j':
        cmd.name = "Move Down";
        break;

      case 'h':
        cmd.name = "Move Left";
        break;

      case 'l':
        cmd.name = "Move Right";
        break;

      default:
        cmd.name = NULL;
        break;
    }
  }

  NULLFREE(sel);

  destroy_nhwindow(win);

  return cmd;
}

void nds_config_key()
{
  int i;
  int pressed;
  nds_key_t key = { 0, NULL };
  nds_cmd_t cmd;
  char buf[BUFSZ];

  nds_draw_prompt("Press the key to modify.");

  while (1) {
    swiWaitForVBlank();

    scanKeys();

    pressed = keysDown();

    /* We don't let the user configure these */

    if ((pressed & KEY_L) ||
        (pressed & KEY_R)) {
      continue;
    } else if (pressed) {
      break;
    }
  }

  nds_clear_prompt();

  for (i = 0; keys[i].key > 0; i++) {
    if (pressed & keys[i].key) {
      key = keys[i];
      break;
    }
  }

  cmd = nds_get_config_cmd();

  if (cmd.name == NULL) {
    return;
  }

  key_map[i] = cmd.f_char;

  sprintf(buf, "Mapped %s to %s.", key.name, cmd.name);

  clear_nhwindow(WIN_MESSAGE);
  putstr(WIN_MESSAGE, ATR_NONE, buf);

  nds_save_key_config();
}

void nds_swap_handedness()
{
  u16 tmp = cmd_key;

  cmd_key = scroll_key;
  scroll_key = tmp;

  nds_save_key_config();

  clear_nhwindow(WIN_MESSAGE);

  if (cmd_key == KEY_L) {
    putstr(WIN_MESSAGE, ATR_NONE, "Switched to right-handed mode.");
  } else {
    putstr(WIN_MESSAGE, ATR_NONE, "Switched to left-handed mode.");
  }
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
      ch = (tmp_x > 0) ? 'l' : 'h';
    } else if (ABS(tmp_y) > 2 * ABS(tmp_x)) {
      ch = (tmp_y > 0) ? 'j' : 'k';
    } else if (tmp_y > 0) {
      ch = (tmp_x > 0) ? 'n' : 'b';
    } else {
      ch = (tmp_x > 0) ? 'u' : 'y';
    }

    if (dist > 1024) {
      input_buffer[0] = ch;
      ch = 'g';
    }
  }

  return ch;
}

int nds_nh_poskey(int *x, int *y, int *mod)
{
  touchPosition coords;

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

  /* Clear out any taps that happen to be occuring right now. */

  nds_flush();

  while(1) {
    int key = 0;
    int pressed;
    int held;

    scan_touch_screen();
    scanKeys();

    pressed = keysDownRepeat();
    held = keysHeld();

    if (held & scroll_key) {
      int cx, cy;
      int changed = 0;

      nds_map_get_center(&cx, &cy);

      if (pressed & KEY_UP) {
        cy--;
        changed |= 1;
      } 
      
      if (pressed & KEY_DOWN) {
        cy++;
        changed |= 1;
      } 
      
      if (pressed & KEY_LEFT) {
        cx--;
        changed |= 1;
      } 
      
      if (pressed & KEY_RIGHT) {
        cx++;
        changed |= 1;
      }

      if (changed) {
        nds_draw_map(windows[WIN_MAP]->map, &cx, &cy);
      } else {
        swiWaitForVBlank();
      }

      continue;
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
      key = nds_map_key(pressed);
    }

    switch (key) {
      case 0:
        break;

      case CMD_CONFIG:
        nds_config_key();
        break;

      default:
        return key;
    }

    if (get_touch_coords(&coords)) {
      return nds_handle_click(coords.px, coords.py, x, y, mod);
    }
  }

  return 0;
}

nds_cmd_t *nds_find_command(int x, int y)
{
  int i;

  for (i = 0; cmdlist[i].name != NULL; i++) {
    if ((x >= cmdlist[i].x1) && (x <= cmdlist[i].x2) &&
        (y >= cmdlist[i].y1) && (y <= cmdlist[i].y2)) {
      return &(cmdlist[i]);
    }
  }

  return NULL;
}

nds_cmd_t *nds_find_command_row_col(int row, int col)
{
  int i;

  for (i = 0; cmdlist[i].name != NULL; i++) {
    if ((row == cmdlist[i].row) && (col == cmdlist[i].col)) {
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
    int text_h;

    text_dims(system_font, "#", NULL, &text_h);

    img = alloc_ppm(COLWIDTH + COLOFFS, text_h);
  }

  for (i = 0; cmdlist[i].name != NULL; i++) {
    if (cmdlist[i].refresh) {
      clear_ppm(img);

      draw_string(system_font, cmdlist[i].name,
                  img, COLOFFS, 0, 1,
                  255, 0, 255);

      draw_ppm_bw(img, vram, 
                  cmdlist[i].x1 - COLOFFS, cmdlist[i].y1,
                  256,
                  cmdlist[i].focused ? 252 : 254, 
                  cmdlist[i].highlighted ? 253 : 255);
    }

    cmdlist[i].refresh = 0;
  }
}

nds_cmd_t *nds_cmd_loop_check_keys(int pressed, nds_cmd_t *curcmd, int *refresh)
{
  int row, col;
  nds_cmd_t *newcmd;

  if (! (pressed & KEY_DOWN) && ! (pressed & KEY_UP) &&
      ! (pressed & KEY_LEFT) && ! (pressed & KEY_RIGHT)) {
    return curcmd;
  }

  if (curcmd == NULL) {
    cmdlist[0].focused = 1;
    cmdlist[0].refresh = 1;

    *refresh = 1;

    return &(cmdlist[0]);
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
    row = 0;
  } else if (row >= cmd_rows) {
    row = cmd_rows - 1;
  }

  if (col < 0) {
    col = 0;
  } else if (col >= cmd_cols) {
    col = cmd_cols - 1;
  }

  newcmd = nds_find_command_row_col(row, col);

  if (newcmd != curcmd) {
    curcmd->highlighted = 0;
    curcmd->focused = 0;
    curcmd->refresh = 1;

    newcmd->focused = 1;
    newcmd->refresh = 1;

    *refresh = 1;
  }

  return newcmd;
}

nds_cmd_t nds_cmd_loop(int in_config)
{
  static int refresh = 0;
  static nds_cmd_t *curcmd = NULL;

  nds_cmd_t *picked_cmd = NULL;
  nds_cmd_t *tapped_cmd = curcmd;
  nds_cmd_t *tmpcmd;
  u16 old_bg_cr;

  touchPosition coords = { .x = 0, .y = 0 };
  touchPosition lastCoords;

  int prev_held = 0;

  /* 
   * Initialize our display.
   */

  /* Now get the display set up. */

  old_bg_cr = BG2_CR;

  BG2_CR = BG_BMP8_256x256 | BG_BMP_BASE(12) | BG_PRIORITY_1;
  DISPLAY_CR |= DISPLAY_BG2_ACTIVE;

  /*
   * Now, we loop until either a command is tapped and selected, or the left
   * button is released.
   */
  while (1) {
    int held;
    int pressed;

    swiWaitForVBlank();

    if (refresh) {
      nds_repaint_cmds();

      refresh = 0;
    }

    lastCoords = coords;
    coords = touchReadXY();

    scanKeys();

    prev_held = held;

    pressed = keysDown();
    held = keysHeld();

    if (((in_config || ! iflags.holdmode) && (pressed & KEY_B)) ||
        ((! in_config && ! (held & cmd_key) && iflags.holdmode)) ||
        (! iflags.holdmode && (pressed & cmd_key)) ) {

      nds_flush();

      break;
    } else if ((held & KEY_A) && curcmd) {
      curcmd->highlighted = 1;
      curcmd->refresh = 1;

      refresh = 1;
    } else if ((prev_held & KEY_A) && curcmd) {
      picked_cmd = curcmd;

      break;
    }

    if ((tmpcmd = nds_cmd_loop_check_keys(pressed, curcmd, &refresh)) != NULL) {
      if (curcmd != tmpcmd) {
        curcmd = tmpcmd;
        tapped_cmd = curcmd;
      }
    }

    if ((coords.x != 0) && (coords.y != 0)) {
      nds_cmd_t *cmd = nds_find_command(coords.px, coords.py);

      if (cmd != curcmd) {
        if (curcmd) {
          curcmd->highlighted = 0;
          curcmd->focused = 0;
          curcmd->refresh = 1;
        }

        cmd->highlighted = 1;
        cmd->focused = 1;
        cmd->refresh = 1;

        curcmd = cmd;

        refresh = 1;
      }
    } else if ((coords.x == 0) && (coords.y == 0) &&
               (lastCoords.x != 0) && (lastCoords.y != 0)) {
      nds_cmd_t *cmd = nds_find_command(lastCoords.px, lastCoords.py);

      refresh = 1;

      if (iflags.doubletap && (tapped_cmd != cmd)) {
        tapped_cmd = cmd;
        tapped_cmd->highlighted = 0;
        tapped_cmd->refresh = 1;
      } else if (cmd) {
        picked_cmd = cmd;

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

  DISPLAY_CR ^= DISPLAY_BG2_ACTIVE;
  BG2_CR = old_bg_cr;

  if (picked_cmd) {
    return *picked_cmd;
  } else {
    nds_cmd_t cmd = { 0, NULL };

    return cmd;
  }
}

nds_cmd_t nds_kbd_cmd_loop()
{
  nds_cmd_t cmd = { 0, NULL };
  int key = nds_prompt_char(NULL, NULL, cmd_key);

  cmd.f_char = key;
  cmd.name = "Dummy";

  return cmd;
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

