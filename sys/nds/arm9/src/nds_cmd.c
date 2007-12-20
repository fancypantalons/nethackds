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

#define CMD_CONFIG     0x00FF
#define KEY_SCROLL     0x00010000

#define INPUT_BUFFER_SIZE 32

#define CLICK_2_FRAMES 30

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

/* We use this array for indexing into the key config list */

#define NUMKEYS 20

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
  { KEY_A | KEY_SCROLL, "A+R" },
  { KEY_B | KEY_SCROLL, "B+R" },
  { KEY_X | KEY_SCROLL, "X+R" },
  { KEY_Y | KEY_SCROLL, "Y+R" },
  { KEY_SELECT | KEY_SCROLL, "Select+R" },
  { KEY_START | KEY_SCROLL, "Start+R" },
};

char key_map[NUMKEYS][INPUT_BUFFER_SIZE] = {
  ",", "s", "o", "\x4",
  "", "",
  "l", "h", "k", "j"
  "", "", "", "",
  "", "",
};

u16 cmd_key = KEY_L;
u16 scroll_key = KEY_R;

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

  if (iflags.cmdwindow) {
    nds_render_cmd_pages();
  }

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

  for (i = 0; i < NUMKEYS; i++) {
    /* 
     * If the scroll key is held, but this key mapping doesn't include a
     * scroll key, skip.
     */
    if ((pressed & scroll_key) && ! (keys[i].key & KEY_SCROLL)) {
      continue;
    }

    if (pressed & keys[i].key) {
      strcpy(input_buffer, &(key_map[i][1]));

      return key_map[i][0];
    }
  }
  
  return 0;
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
  ANY_P ids[10];
  nds_cmd_t cmd;

  win = create_nhwindow(NHW_MENU);
  start_menu(win);

  ids[0].a_int = 0;
  ids[1].a_int = 'y';
  ids[2].a_int = 'k';
  ids[3].a_int = 'u';
  ids[4].a_int = 'h';
  ids[5].a_int = 'l';
  ids[6].a_int = 'b';
  ids[7].a_int = 'j';
  ids[8].a_int = 'n';
  ids[9].a_int = 1;

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

  end_menu(win, "What do you want to assign to this key?");

  if (select_menu(win, PICK_ONE, &sel) <= 0) {
    cmd.f_char = 0;
    cmd.name = NULL;
  } else if (sel->item.a_int == 1) {
    cmd = nds_cmd_loop(1);
  } else {
    cmd.f_char = sel->item.a_int;

    switch (cmd.f_char) {
      case 'y':
        cmd.name = "Move Up-Left";
        break;

      case 'k':
        cmd.name = "Move Up";
        break;

      case 'u':
        cmd.name = "Move Up-Right";
        break;

      case 'h':
        cmd.name = "Move Left";
        break;

      case 'l':
        cmd.name = "Move Right";
        break;

      case 'b':
        cmd.name = "Move Down-Left";
        break;

      case 'j':
        cmd.name = "Move Down";
        break;

      case 'n':
        cmd.name = "Move Down-Right";
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
  int held;
  nds_key_t key = { 0, NULL };
  nds_cmd_t cmd;
  char buf[BUFSZ];
  char tmp[2];

  nds_flush();

  nds_draw_prompt("Press the key to modify.");

  while (1) {
    swiWaitForVBlank();

    scanKeys();

    held = keysHeld();

    /* We don't let the user configure these */

    if ((held & cmd_key) ||
        (held == scroll_key)) {
      continue;
    } else if (held) {
      for (i = 0; i < NUMKEYS; i++) {
        if ((held & scroll_key) && ! (keys[i].key & KEY_SCROLL)) {
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

  if (cmd.f_char == 0) {
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

  nds_flush();
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

int nds_get_input(int *x, int *y, int *mod)
{
  touchPosition coords;
  int held_frames = 0;

  /* Set one, initialize these! */

  *x = 0;
  *y = 0;
  *mod = 0;

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
      }
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
    } else if (held | pressed) {
      key = nds_map_key(held | pressed);
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
    
    if (held_frames > CLICK_2_FRAMES) {
      coords = touchReadXY();

      nds_map_translate_coords(coords.px, coords.py, x, y);
      *mod = CLICK_2;

      return 0;
    } else if (get_tap_coords(&coords)) {
      return nds_handle_click(coords.px, coords.py, x, y, mod);
    }

    if (held & KEY_TOUCH) {
      held_frames++;
    } else {
      held_frames = 0;
    }
  }

  return 0;
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

        clear_ppm(img);
        draw_string(system_font, cur_cmd->name, img,
                    0, 0, 1, 255, 0, 255);
        draw_ppm_bw(img, cmd_pages[cur_page], cur_cmd->x1, cur_cmd->y1 - yoffs, 256, 254, 255);
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
      clear_ppm(img);

      draw_string(system_font, cmdlist[i].name,
                  img, 0, 0, 1,
                  255, 0, 255);

      draw_ppm_bw(img, vram, 
                  cmdlist[i].x1, cmdlist[i].y1,
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
    refresh = (newcmd != curcmd);

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
    int held;
    int pressed;

    swiWaitForVBlank();

    /* If the page has changed, we need to render the new one. */

    if (cmd_cur_page != displayed_page) {
      dmaCopy(cmd_pages[cmd_cur_page], vram + 128 * cmdlist[0].y1, cmd_page_size);

      if (cmd_page_count > 1) {
        nds_draw_rect(0, 0, 256, up_arrow->height, 254, vram);
        nds_draw_rect(0, 192 - up_arrow->height, 256, up_arrow->height, 254, vram);

        if (cmd_cur_page != 0) {
          draw_ppm_bw(up_arrow, vram, 
                      256 / 2 - up_arrow->width / 2, 0,
                      256, 254, 255);
        }      

        if (cmd_cur_page < (cmd_page_count - 1)) {
          draw_ppm_bw(down_arrow, vram, 
                      256 / 2 - down_arrow->width / 2, 192 - down_arrow->height,
                      256, 254, 255);
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

    pressed = keysDown();
    held = keysHeld();
    coords = get_touch_coords();

    if (((in_config || ! iflags.holdmode) && (pressed & KEY_B)) ||
        ((! in_config && ! (held & cmd_key) && iflags.holdmode)) ||
        (! iflags.holdmode && (pressed & cmd_key)) ) {

      nds_flush();

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
      } else if (held_frames > CLICK_2_FRAMES) {
        char buffer[BUFSZ];
        int len;

        getlin("Enter Repeat Count", buffer);
        len = strlen(buffer);

        if (*buffer) {
          picked_cmd.name = curcmd->name;
          picked_cmd.f_char = buffer[0];
          strcpy(input_buffer, buffer + 1);
          input_buffer[len - 1] = curcmd->f_char;
          input_buffer[len] = '\0';

          break;
        } else {
          curcmd->highlighted = 0;
          curcmd->refresh = 1;

          refresh = 1;

          nds_flush();
        }
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

  DISPLAY_CR ^= DISPLAY_BG2_ACTIVE;
  BG2_CR = old_bg_cr;

  return picked_cmd;
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

