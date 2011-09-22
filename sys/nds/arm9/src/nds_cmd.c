#include <nds.h>
#include <stdio.h>

#include "hack.h"
#include "func_tab.h"

#include "nds_win.h"
#include "nds_gfx.h"
#include "ppm-lite.h"
#include "nds_util.h"
#include "nds_map.h"
#include "nds_kbd.h"
#include "nds_config.h"
#include "nds_cmd.h"

#define M(c) (0x80 | (c))
#define C(c) (0x1f & (c))

#define COLOFFS  2

#define CLICK_2_FRAMES 30

/*
 * Missing commands:
 *
 * conduct
 * ride
 * extended commands
 */

typedef struct {
  int pressed;
  int released;
  int held;
  int touching;
  int tapped;

  int press_and_hold;
  int dragging;
  int drag_started;
  int drag_stopped;

  coord_t touch_coords;
  coord_t tap_coords;

  coord_t initial_touch_coords;
  coord_t drag_distance;

  int held_frames;
} nds_input_state_t;

nds_cmd_t cmdlist[] = {
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
        {CMD_SHOW_KEYS, "Show Keys"},
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
	//{'v', "Version"},
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

rectangle_t up_arrow_rect;
rectangle_t down_arrow_rect;

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
int input_buffer_pos = 0;

char direction_keys[NUMDIRS];

/* We use this array for indexing into the key config list */

u16 helpline1[13];
u16 helpline2[13];

nds_cmd_t nds_cmd_loop(nds_cmdloop_op_type_t optype);
nds_cmd_t nds_kbd_cmd_loop();
int nds_load_key_config();
void nds_render_cmd_pages();
u16 nds_key_string_to_mask();
u16 *nds_parse_key_string(char *keystr);
void nds_add_keymap_entry(u16 key, char command[INPUT_BUFFER_SIZE]);

int nds_init_cmd()
{
  int i;
  rectangle_t up_rect = {
    .start = { .x = 256 / 2 - up_arrow->width / 2, .y = 0 },
    .dims = { .width = up_arrow->width, .height = up_arrow->height }
  };
  rectangle_t down_rect = {
    .start = { .x = 256 / 2 - down_arrow->width / 2, .y = 192 - down_arrow->height },
    .dims = { .width = down_arrow->width, .height = down_arrow->height }
  };

  up_arrow_rect = up_rect;
  down_arrow_rect = down_rect;

  if (flags.debug) {
    int idx = 0;

    for (; cmdlist[idx].name != NULL; idx++);

    for (i = 0; wiz_cmdlist[i].name != NULL; i++) {
      cmdlist[idx++] = wiz_cmdlist[i];
    }
  }

  nds_render_cmd_pages();

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

  nds_key_config_init();

  memcpy(helpline1, nds_parse_key_string(iflags.helpline1), sizeof(helpline1));
  memcpy(helpline2, nds_parse_key_string(iflags.helpline2), sizeof(helpline2));

  input_buffer_pos = 0;

  return 0;
}

/* Input buffer management */

int nds_input_buffer_is_empty()
{
  return input_buffer_pos <= 0;
}

void nds_input_buffer_push(char c)
{
  input_buffer[input_buffer_pos++] = c;
}

void nds_input_buffer_append(char* str)
{
  int i = 0;

  for (i = 0; i < strlen(str); i++) {
    nds_input_buffer_push(str[i]);
  }
}

char nds_input_buffer_shift()
{
  int c = '\0';

  if (input_buffer_pos > 0) {
    c = input_buffer[0];

    memmove(input_buffer, input_buffer + 1, --input_buffer_pos);
  }

  return c;
}

char *nds_input_buffer_shiftall()
{
  static char buf[INPUT_BUFFER_SIZE];

  memcpy(buf, input_buffer, input_buffer_pos);
  buf[input_buffer_pos] = '\0';

  input_buffer_pos = 0;

  return buf;
}

char *nds_get_direction_keys()
{
  return direction_keys;
}

nds_cmd_t *nds_get_cmdlist()
{
  return cmdlist;
}

void nds_toggle_bool_option(char *name)
{
  int i;
  int optidx = -1;
  char buffer[BUFSZ];
  boolean *val;

  for (i = 0; boolopt[i].name; i++) {
    if (strcmp(boolopt[i].name, name) == 0) {
      optidx = i;
      break;
    }
  }

  if (optidx < 0) {
    return;
  }

  val = boolopt[optidx].addr;
  sprintf(buffer, "%s%s", *val ? "!" : "", boolopt[optidx].name);

  parseoptions(buffer, FALSE, FALSE);

  sprintf(buffer, "Set %s %s",
          boolopt[optidx].name,
          *val ? "ON" : "OFF");

  putstr(WIN_MESSAGE, ATR_NONE, buffer);
}

int nds_handle_click(coord_t coords, int *x, int *y, int *mod)
{
  coord_t map_coords = nds_map_translate_coords(coords);
  int ch;

  /* 
   * If the click is somewhere around the user, or we're not in compass
   * mode, pass the click back to the game engine directly.
   */
  if ( ((ABS(map_coords.x - u.ux) <= 1) && (ABS(map_coords.y - u.uy) <= 1)) ||
       (iflags.compassmode == 0) ) {
    *x = map_coords.x;
    *y = map_coords.y;
    *mod = CLICK_1;

    ch = 0;
  } else {
    coord_t tmp_coords = coords;
    int dist;

    if (iflags.compassmode == 1) {
      tmp_coords = nds_map_relativize(tmp_coords);
    } else {
      tmp_coords.x -= 128;
      tmp_coords.y -= 96;
    }

    dist = tmp_coords.x * tmp_coords.x + tmp_coords.y * tmp_coords.y;

    /* 
     * Here we take the click location and convert it to a direction key
     * based on a movement compass.
     */

    if (ABS(tmp_coords.x) > 2 * ABS(tmp_coords.y)) {
      ch = (tmp_coords.x > 0) ? direction_keys[DIR_RIGHT] : direction_keys[DIR_LEFT];
    } else if (ABS(tmp_coords.y) > 2 * ABS(tmp_coords.x)) {
      ch = (tmp_coords.y > 0) ? direction_keys[DIR_DOWN] : direction_keys[DIR_UP];
    } else if (tmp_coords.y > 0) {
      ch = (tmp_coords.x > 0) ? direction_keys[DIR_DOWN_RIGHT] : direction_keys[DIR_DOWN_LEFT];
    } else {
      ch = (tmp_coords.x > 0) ? direction_keys[DIR_UP_RIGHT] : direction_keys[DIR_UP_LEFT];
    }

    if (dist > 1024) {
      nds_input_buffer_push('g');
    }
  }

  return ch;
}

struct ppm *help_img = NULL;

void nds_render_key_help_string(u16 keys)
{
  u16 *vram = (u16 *)BG_BMP_RAM_SUB(4);

  if (help_img == NULL) {
    help_img = alloc_ppm(252, system_font->height * 2);
  }

  clear_ppm(help_img, MAP_COLOUR(CLR_BLACK));

  draw_string(system_font, nds_find_key_options(helpline1, keys), 
              help_img, 0, 0, -1, -1);

  draw_string(system_font, nds_find_key_options(helpline2, keys), 
              help_img, 0, system_font->height, -1, -1);

  draw_ppm(help_img, vram, 4, 192 - system_font->height * 2, 256);
}

coord_t _to_map_coords(coord_t coords)
{
  coord_t res = { 
    .x = coords.x / nds_map_tile_width(), 
    .y = coords.y / nds_map_tile_height()
  };

  return res;
}

nds_input_state_t _nds_get_input_state(nds_input_state_t prev_state)
{
  nds_input_state_t state = prev_state;

  scanKeys();
  scan_touch_screen();

  /*
   * First, we'll process the touchscreen tap/drag events.
   */
  state.held = nds_keysHeld();

  if (state.held & KEY_TOUCH) {
    state.touch_coords = _to_map_coords(get_touch_coords());
    state.touching = 1;
  } else {
    state.touching = 0;
  }

  if (state.touching && ! prev_state.touching) {
    state.initial_touch_coords = state.touch_coords;
  }

  /*
   * Alright, knowing the previous touch state and the current one, now
   * check for dragging.
   */
  if (prev_state.touching && state.touching && 
      ! COORDS_ARE_EQUAL(prev_state.initial_touch_coords, state.touch_coords)) {

    state.drag_distance = coord_subtract(prev_state.initial_touch_coords, state.touch_coords);

    if (! state.dragging) {
      state.dragging = 1;
      state.drag_started = 1;
    } else {
      state.drag_started = 0;
    }
  } else if (state.dragging) {
    state.dragging = 0;
    state.drag_stopped = 1;
  } else if (state.drag_stopped) {
    state.drag_stopped = 0;
  }

  /*
   * Now, check for key pressed events...
   */
  if (iflags.keyrepeat) {
    state.pressed = nds_keysDownRepeat();
  } else {
    state.pressed = nds_keysDown();
  }

  state.released = nds_keysUp();

  /*
   * Tap events...
   */
  state.tapped = get_tap_coords(&(state.tap_coords)) && ! state.dragging && ! state.drag_stopped;
  state.tap_coords = _to_map_coords(state.tap_coords);

  /*
   * And press-and-hold...
   */
  if (state.touching && ! state.dragging) {
    state.held_frames = prev_state.held_frames + 1;

    if (state.held_frames > CLICK_2_FRAMES) {
      state.press_and_hold = 1;
    }
  } else {
    state.press_and_hold = 0;
    state.held_frames = 0;
  }

  return state;
}

int nds_get_input(int *x, int *y, int *mod)
{
  // Tracks if, on the previous call, we returned a keypress that *wasn't*
  // a single chordkey press-and-release.  This way, if we enter this loop
  // while the chordkey is still held as part of a chorded key combo, 
  // we don't accidentally trigger an action when it's released.

  static int chord_key_was_held = 0;

  nds_input_state_t prev_state;
  nds_input_state_t state;

  coord_t map_center = { .x = u.ux, .y = u.uy };
  coord_t prev_map_center = map_center;

  memset(&state, 0, sizeof(state));

  *x = *y = *mod = 0;

  if (! nds_input_buffer_is_empty()) {
    return nds_input_buffer_shift();
  }

  nds_flush(~KEY_TOUCH);

  while(1) {
    int key = 0;

    prev_state = state;
    state = _nds_get_input_state(prev_state);

    if (nds_command_key_pressed(state.pressed)) {
      nds_cmd_t cmd;

      if (! iflags.holdmode) {
        nds_flush(0);
      }
      
      if (iflags.cmdwindow) {
        cmd = nds_cmd_loop(CMDLOOP_STANDARD);
      } else {
        cmd = nds_kbd_cmd_loop();
      }

      key = cmd.f_char;
    } else if (nds_chord_key_pressed(state.released)) {
      if (! chord_key_was_held) {
        key = nds_map_key(state.released);
      } else {
        chord_key_was_held = 0;
      }
    } else if (state.pressed && ! nds_chord_key_pressed(state.pressed)) {
      chord_key_was_held = nds_chord_key_held(state.held);

      key = nds_map_key(state.pressed | state.held);
    }

    switch (key) {
      case 0:
        if (iflags.keyhelp && (state.held != prev_state.held)) {
          nds_render_key_help_string(state.held);
        }

        break;

      case CMD_PAN_UP:
      case CMD_PAN_DOWN:
      case CMD_PAN_LEFT:
      case CMD_PAN_RIGHT:
        {
          if (key == CMD_PAN_UP) {
            map_center.y--;
          } else if (key == CMD_PAN_DOWN) {
            map_center.y++;
          } else if (key == CMD_PAN_LEFT) {
            map_center.x--;
          } else if (key == CMD_PAN_RIGHT) {
            map_center.x++;
          }
        }

        break;

      case CMD_CONFIG:
        nds_config_key();
        break;

      case CMD_OPT_TOGGLE:
        nds_toggle_bool_option(nds_input_buffer_shiftall());

        display_nhwindow(WIN_MAP, FALSE);

        break;

      case CMD_SHOW_KEYS:
        nds_show_keys();
        break;

      default:
        return key;
    }
    
    if (state.tapped) {
      return nds_handle_click(state.tap_coords, x, y, mod);
    } else if (state.press_and_hold) {

      coord_t coords = nds_map_translate_coords(state.touch_coords);

      *x = coords.x;
      *y = coords.y;
      *mod = CLICK_2;

      return 0;
    }

    swiWaitForVBlank();
    
    if (state.drag_stopped) {
      map_center = coord_add(map_center, state.drag_distance);
    }

    if (state.dragging) {
      coord_t drag_center = coord_add(map_center, state.drag_distance);

      nds_draw_map(&drag_center);
    } else if (! COORDS_ARE_EQUAL(map_center, prev_map_center))  {
      nds_draw_map(&map_center);

      prev_map_center = map_center;
    }
  }

  nds_clear_prompt();

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

        cur_cmd->region.start.x = cur_col * cmd_col_width + xoffs;
        cur_cmd->region.start.y = cur_row * system_font->height + yoffs;
        cur_cmd->region.dims.width = cmd_col_width;
        cur_cmd->region.dims.height = system_font->height;

        clear_ppm(img, MAP_COLOUR(CLR_BLACK));
        draw_string(system_font, cur_cmd->name, img,
                    0, 0, -1, -1);
        draw_ppm(img, cmd_pages[cur_page], cur_cmd->region.start.x, cur_cmd->region.start.y - yoffs, 256);
      }
    }
  }

RENDER_DONE:

  free_ppm(img);
}

nds_cmd_t *nds_find_command(coord_t coords)
{
  int i;

  for (i = 0; cmdlist[i].name != NULL; i++) {
    if (POINT_IN_RECT(coords, cmdlist[i].region) &&
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
               cmdlist[i].region.start.x, cmdlist[i].region.start.y,
               256);
    }

    cmdlist[i].refresh = 0;
  }
}

nds_cmd_t *nds_cmd_loop_check_keys(int pressed, nds_cmd_t *curcmd, int *refresh)
{
  int row, col;
  nds_cmd_t *newcmd;

  int key_pressed = (pressed & KEY_DOWN) ||
                    (pressed & KEY_UP) ||
                    (pressed & KEY_LEFT) ||
                    (pressed & KEY_RIGHT);

  if ((cmd_page_count > 1) &&
      touch_released_in(up_arrow_rect) && 
      (cmd_cur_page != 0))  {

    cmd_cur_page--;
  } else if ((cmd_page_count > 1) &&
             touch_released_in(down_arrow_rect) && 
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

nds_cmd_t nds_cmd_loop(nds_cmdloop_op_type_t optype)
{
  static int refresh = 0;
  static nds_cmd_t *curcmd = NULL;
  static int displayed_page = -1;

  nds_cmd_t *tapped_cmd = curcmd;
  nds_cmd_t *tmpcmd;
  nds_cmd_t picked_cmd;
  u16 old_bg_cr;
  int held_frames = 0;

  coord_t coords = { .x = 0, .y = 0 };

  int prev_held = 0;
  int held = 0;

  /* 
   * Initialize our display.
   */

  /* Now get the display set up. */

  old_bg_cr = REG_BG2CNT;

  REG_BG2CNT = BG_BMP8_256x256 | BG_BMP_BASE(12) | BG_PRIORITY_1;
  REG_DISPCNT |= DISPLAY_BG2_ACTIVE;

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
      dmaCopy(cmd_pages[cmd_cur_page], vram + 128 * cmdlist[0].region.start.y, cmd_page_size);

      if (cmd_page_count > 1) {
        nds_draw_rect(up_arrow_rect, 254, vram);
        nds_draw_rect(down_arrow_rect, 254, vram);

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

    pressed = nds_keysDownRepeat();
    held = nds_keysHeld();
    coords = get_touch_coords();

    if ( 
        (
         ((optype == CMDLOOP_CONFIG) || ! iflags.holdmode) && (pressed & KEY_B)
        ) 
        ||
        (
          ((optype != CMDLOOP_CONFIG) && ! nds_command_key_pressed(held) && iflags.holdmode) ||
          (! iflags.holdmode && nds_command_key_pressed(pressed)) 
        )
       ) {

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
      nds_cmd_t *cmd = nds_find_command(coords);

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

          nds_input_buffer_append(bufptr);
          nds_input_buffer_push(curcmd->f_char);

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
      nds_cmd_t *cmd = nds_find_command(coords);

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

  REG_DISPCNT ^= DISPLAY_BG2_ACTIVE;
  REG_BG2CNT = old_bg_cr;

  /* 
   * If an extended command was requested, we need to get it from the
   * user.
   */
  if ((picked_cmd.f_char == '#') && (optype != CMDLOOP_WHATDOES)) {
    int idx = get_ext_cmd();

    if (idx >= 0) {
      /* Now stuff the command into our input buffer. */

      nds_input_buffer_append((char *)extcmdlist[idx].ef_txt);
    } else {
      picked_cmd.f_char = 0;
      picked_cmd.name = NULL;
    }
  }

  nds_flush(0);

  return picked_cmd;
}

nds_cmd_t nds_kbd_cmd_loop()
{
  nds_cmd_t cmd = { 0, NULL };
  int key = nds_prompt_char(NULL, NULL, 1);

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
      } else if (key == 2) {
        key = CMD_SHOW_KEYS;
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
  int cmdcnt;
  int i;

  winid win;
  ANY_P *ids = NULL;
  menu_item *sel = NULL;
  int selected;

  if (! nds_input_buffer_is_empty()) {
    char *buffer = nds_input_buffer_shiftall();

    for (i = 0; extcmdlist[i].ef_txt != NULL; i++) {
      if (strcmp(extcmdlist[i].ef_txt, buffer) == 0) {
        return i;
      }
    }
  }

  for (i = 0, cmdcnt = 0; extcmdlist[i].ef_txt != NULL; i++) {
    cmdcnt++;
  }

  ids = (ANY_P *)malloc(sizeof(ANY_P) * cmdcnt);

  win = create_nhwindow(NHW_MENU);

  start_menu(win);

  for (i = 0; i < cmdcnt; i++) {
    ids[i].a_int = i + 1;
    add_menu(win, NO_GLYPH, &(ids[i]), 0, 0, 0, extcmdlist[i].ef_txt, 0);
  }

  end_menu(win, "Which Extended Command?");

  if (select_menu(win, PICK_ONE, &sel) > 0) {
    selected = sel->item.a_int - 1;
  } else {
    selected = -1;
  }

  destroy_nhwindow(win);
  NULLFREE(ids);
  NULLFREE(sel);

  return selected;
}

void nds_number_pad(int thinger)
{
}
