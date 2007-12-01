#include <nds.h>
#include <stdio.h>

#include "hack.h"

#include "nds_win.h"
#include "nds_cmd.h"
#include "nds_gfx.h"
#include "ppm-lite.h"
#include "nds_util.h"
#include "nds_map.h"

#define M(c) (0x80 | (c))
#define C(c) (0x1f & (c))

#define COLWIDTH 60

#define KEY_CONFIG_FILE "keys.cnf"

/*
 * Missing commands:
 *
 * conduct
 * ride
 * extended commands
 */

typedef struct {
  char f_char;
  char *name;
  int x1;
  int y1;
  int x2;
  int y2;
} nds_cmd_t;

static nds_cmd_t cmdlist[] = {
//	{C('p'), TRUE, doprev_message},
	{M('a'), "Adjust"},
	{'a', "Apply"},
	{'A', "Armor"},
	{C('x'), "Attributes"},
	{'C', "Call"},
	{'Z', "Cast"},
	{M('c'), "Chat"},
        {0, "Key Config"},
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
	{'R', "Remove"},
	{M('r'), "Rub"},
        {0, "Handedness"},
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

/* We use this array for indexing into the key config list */

static u16 keys[] = { 
  KEY_A, KEY_B, 
  KEY_X, KEY_Y,
  KEY_SELECT, KEY_START, 
  KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN, -1
};

u8 key_map[] = {
  ',', 's', '<', '>',
  0, 0,
  'l', 'h', 'k', 'j'
};

u16 *vram = (u16 *)BG_BMP_RAM(12);

u16 cmd_key = KEY_L;
u16 scroll_key = KEY_R;

char nds_cmd_loop();
void nds_load_key_config();

void nds_init_cmd()
{
  int cur_x = 0;
  int cur_y = 0;
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
      cur_x += COLWIDTH + 1;
      cur_y = 0;
    }

    draw_string(system_font, cmdlist[i].name, img,
                cur_x, cur_y, 1,
                255, 0, 255);

    cmdlist[i].x1 = cur_x;
    cmdlist[i].x2 = cur_x + COLWIDTH;
    cmdlist[i].y1 = cur_y;
    cmdlist[i].y2 = cur_y + text_h;

    if (flags.debug) {
      cur_y += text_h;
    } else {
      cur_y += text_h + 2;
    }
  }

  draw_ppm_bw(img, vram, 0, 0, 256, 254, 255);

  free_ppm(img);

  nds_load_key_config();
}

int nds_map_key(u16 pressed)
{
  int i;

  for (i = 0; keys[i] > 0; i++) {
    if (pressed & keys[i]) {
      return key_map[i];
    }
  }
  
  return 0;
}

int nds_nh_poskey(int *x, int *y, int *mod)
{
  touchPosition coords = { .x = 0, .y = 0 };
  touchPosition lastCoords;

  /* Clear out any taps that happen to be occuring right now. */

  nds_flush();

  while(1) {
    int key;
    int pressed;
    int held;

    lastCoords = coords;
    coords = touchReadXY();

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

    if (pressed && (key = nds_map_key(pressed))) {
      return key;
    } else if (pressed & cmd_key) {
      char c = nds_cmd_loop(0);

      if (c != 0) {
        return c;
      }
    }

    if (((lastCoords.x != 0) || (lastCoords.y != 0)) &&
        ((coords.x == 0) && (coords.y == 0))) {
      nds_map_translate_coords(lastCoords.px, lastCoords.py, x, y);

      *mod = CLICK_1;

      return 0;
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

  iprintf("Load got %x\n", fp);

  if (fp == (FILE *)0) {
    return;
  }

  if ((ret = fread(buffer, 1, cnt, fp)) < cnt) {
    iprintf("Only got %d, wanted %d\n", ret, cnt);
  } else {
    memcpy(key_map, buffer, cnt);
  }

  fclose(fp);
}

void nds_save_key_config()
{
  FILE *fp = fopen(fqname(KEY_CONFIG_FILE, CONFIGPREFIX, 0), "w");

  iprintf("Save got %x\n", fp);

  if (fp == (FILE *)0) {
    return;
  }

  fwrite(key_map, 1, sizeof(key_map), fp);
  fclose(fp);
}

int nds_get_config_cmd()
{
  winid win;
  menu_item *sel;
  ANY_P ids[6];
  int ret;

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
    ret = -1;
  } else if (sel->item.a_int == 1) {
    ret = nds_cmd_loop(1);
  } else {
    ret = sel->item.a_int;
  }

  NULLFREE(sel);

  destroy_nhwindow(win);

  return ret;
}

void nds_config_key()
{
  int i;
  int pressed;
  int key = -1;
  int cmd;

  nds_draw_prompt("Press a key.");

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

  for (i = 0; keys[i] > 0; i++) {
    if (pressed & keys[i]) {
      key = i;
      break;
    }
  }

  cmd = nds_get_config_cmd();

  if (cmd < 0) {
    return;
  }

  key_map[i] = cmd;

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

nds_cmd_t nds_find_command(int x, int y)
{
  int i;
  nds_cmd_t cmd = {-1, NULL};

  for (i = 0; cmdlist[i].name != NULL; i++) {
    if ((x >= cmdlist[i].x1) && (x <= cmdlist[i].x2) &&
        (y >= cmdlist[i].y1) && (y <= cmdlist[i].y2)) {
      cmd = cmdlist[i];

      break;
    }
  }

  return cmd;
}

char nds_cmd_loop(int in_config)
{
  nds_cmd_t curcmd = { -1, NULL };
  nds_cmd_t picked_cmd = { -1, NULL };

  touchPosition coords = { .x = 0, .y = 0 };
  touchPosition lastCoords;

  /* Initialize our display */
  DISPLAY_CR |= DISPLAY_BG3_ACTIVE;

  /*
   * Now, we loop until either a command is tapped and selected, or the left
   * button is released.
   */
  while (1) {
    int held;
    int pressed;

    swiWaitForVBlank();

    lastCoords = coords;
    coords = touchReadXY();

    scanKeys();

    pressed = keysDown();
    held = keysHeld();

    if (in_config && (pressed & KEY_B)) {
      break;
    } else if (! in_config && ! (held & cmd_key)) {
      break;
    }

    if ((coords.x != 0) && (coords.y != 0)) {
      nds_cmd_t cmd = nds_find_command(coords.px, coords.py);

      if (cmd.f_char != curcmd.f_char) {
        if (curcmd.name != NULL) {
          nds_draw_text(system_font, curcmd.name, curcmd.x1, curcmd.y1,
                        254, 255, vram);
        }

        if (cmd.name != NULL) {
          nds_draw_text(system_font, cmd.name, cmd.x1, cmd.y1, 
                        254, 253, vram);
        }

        curcmd = cmd;
      }
    } else if ((lastCoords.x != 0) && (lastCoords.y != 0)) {
      nds_cmd_t cmd = nds_find_command(lastCoords.px, lastCoords.py);

      if (cmd.name != NULL) {
        nds_draw_text(system_font, cmd.name, cmd.x1, cmd.y1, 
                      254, 255, vram);

        picked_cmd = cmd;

        break;
      }
    }
  }

  /*
   * This happens if the user releases L while pressing a command.  Basically,
   * we want to remove the item highlight.
   */

  if ((coords.x != 0) && (coords.y != 0)) {
    nds_cmd_t cmd = nds_find_command(coords.px, coords.py);

    if (cmd.name != NULL) {
      nds_draw_text(system_font, cmd.name, cmd.x1, cmd.y1, 
                    254, 255, vram);
    }
  }

  DISPLAY_CR ^= DISPLAY_BG3_ACTIVE;
  
  if (picked_cmd.name == NULL) {
    return 0;
  } else if (picked_cmd.f_char != 0) {
    return picked_cmd.f_char;
  } else if (strcasecmp(picked_cmd.name, "Key Config") == 0) {
    nds_config_key();
  } else if (strcasecmp(picked_cmd.name, "Handedness") == 0) {
    nds_swap_handedness();
  }

  return 0;
}

