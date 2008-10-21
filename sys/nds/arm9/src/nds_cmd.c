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

#define KEY_MAGIC 0xDECAFBAD

#define M(c) (0x80 | (c))
#define C(c) (0x1f & (c))

#define COLOFFS  2

#define KEY_CONFIG_FILE "keys.cnf"

#define CMD_CONFIG     0xFF
#define CMD_PAN_RIGHT  0xFE
#define CMD_PAN_LEFT   0xFD
#define CMD_PAN_UP     0xFC
#define CMD_PAN_DOWN   0xFB
#define CMD_OPT_TOGGLE 0xFA
#define CMD_SHOW_KEYS  0xF9

/* The number of buffered command characters we'll support. */

#define MAXCMDS 10
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
  u16 key;
  char command[INPUT_BUFFER_SIZE];
} nds_keymap_entry_t;

typedef enum {
  CMDLOOP_STANDARD,
  CMDLOOP_CONFIG,
  CMDLOOP_WHATDOES
} nds_cmdloop_op_type_t;

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

/* Since this isn't exported anywhere, we duplicate the definition here. */

extern struct Bool_Opt
{
        const char *name;
        boolean *addr, initvalue;
        int optflags;
} boolopt[];

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
#define DIR_RIGHT      4
#define DIR_DOWN_LEFT  5
#define DIR_DOWN       6
#define DIR_DOWN_RIGHT 7
#define DIR_WAIT       8

#define NUMDIRS        9

char direction_keys[NUMDIRS];

/* We use this array for indexing into the key config list */

nds_keymap_entry_t *keymap = NULL;
int numkeys = 0;

u16 chord_keys;
u16 cmd_key;

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
  u16 chord_keys_config, cmd_key_config;

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

  if (nds_load_key_config() < 0) {
    char tmp[INPUT_BUFFER_SIZE];

    NULLFREE(keymap);
    numkeys = 0;

    chord_keys = KEY_R;
    tmp[1] = '\0';

    nds_add_keymap_entry(KEY_A, ",");
    nds_add_keymap_entry(KEY_B, "s");
    nds_add_keymap_entry(KEY_X, "o");
    nds_add_keymap_entry(KEY_Y, "\x4");

    tmp[0] = direction_keys[DIR_UP];
    nds_add_keymap_entry(KEY_UP, tmp);

    tmp[0] = direction_keys[DIR_DOWN];
    nds_add_keymap_entry(KEY_DOWN, tmp);

    tmp[0] = direction_keys[DIR_LEFT];
    nds_add_keymap_entry(KEY_LEFT, tmp);

    tmp[0] = direction_keys[DIR_RIGHT];
    nds_add_keymap_entry(KEY_RIGHT, tmp);

    nds_add_keymap_entry(KEY_RIGHT | KEY_R, "\xFE");
    nds_add_keymap_entry(KEY_LEFT | KEY_R, "\xFD");
    nds_add_keymap_entry(KEY_UP | KEY_R, "\xFC");
    nds_add_keymap_entry(KEY_DOWN | KEY_R, "\xFB");
  }

  /* 
   * If the configured chordkey set doesn't match what we read from file, or
   * set as the default, then we use the new chordkey, and blank the key
   * config... the user has to rebind.
   */
  if ((chord_keys_config = nds_key_string_to_mask(iflags.chordkeys)) != chord_keys) {
    NULLFREE(keymap);
    numkeys = 0;
    chord_keys = chord_keys_config;
  } 

  if (nds_count_bits(cmd_key_config = nds_key_string_to_mask(iflags.cmdkey)) == 1) {
    cmd_key = cmd_key_config;
  } else {
    iprintf("Invalid command key: %s\n", iflags.cmdkey);

    return -1;
  }

  memcpy(helpline1, nds_parse_key_string(iflags.helpline1), sizeof(helpline1));
  memcpy(helpline2, nds_parse_key_string(iflags.helpline2), sizeof(helpline2));

  memset(input_buffer, 0, sizeof(input_buffer));

  return 0;
}

/*
 * Convert pressed keys into a user-presentable string.
 */
char *string_for_key(u16 key)
{
  switch (key) {
    case KEY_A:
      return "A";

    case KEY_B:
      return "B";

    case KEY_X:
      return "X";

    case KEY_Y:
      return "Y";

    case KEY_R:
      return "R";

    case KEY_L:
      return "L";

    case KEY_UP:
      return "Up";

    case KEY_DOWN:
      return "Down";

    case KEY_LEFT:
      return "Left";

    case KEY_RIGHT:
      return "Right";

    case KEY_START:
      return "Start";

    case KEY_SELECT:
      return "Select";
  } 

  return "(none)";
}

u16 nds_string_to_key(char *str)
{
  if (strcasecmp(str, "a") == 0) {
    return KEY_A;
  } else if (strcasecmp(str, "b") == 0) {
    return KEY_B;
  } else if (strcasecmp(str, "x") == 0) {
    return KEY_X;
  } else if (strcasecmp(str, "y") == 0) {
    return KEY_Y;
  } else if (strcasecmp(str, "l") == 0) {
    return KEY_L;
  } else if (strcasecmp(str, "r") == 0) {
    return KEY_R;
  } else if (strcasecmp(str, "up") == 0) {
    return KEY_UP;
  } else if (strcasecmp(str, "down") == 0) {
    return KEY_DOWN;
  } else if (strcasecmp(str, "left") == 0) {
    return KEY_LEFT;
  } else if (strcasecmp(str, "right") == 0) {
    return KEY_RIGHT;
  } else if (strcasecmp(str, "start") == 0) {
    return KEY_START;
  } else if (strcasecmp(str, "select") == 0) {
    return KEY_SELECT;
  } else {
    return 0;
  }
}

char *nds_key_to_string(int key)
{
  static char buffer[BUFSZ];
  int i;

  buffer[0] = '\0';

  for (i = 0; i < 16; i++) {
    int mask = (1 << i);

    if ((chord_keys & mask) && (key & mask)) {
      strcat(buffer, string_for_key(key & mask));
      strcat(buffer, "+");
    }
  }

  if (key & ~chord_keys) {
    strcat(buffer, string_for_key(key & ~chord_keys));
  } else {
    buffer[strlen(buffer) - 1] = '\0';
  }

  return buffer;
}

char *nds_command_to_string(char *command)
{
  int i;

  if (strlen(command) != 1) {
    return command;
  }

  switch (*command) {
    case CMD_CONFIG:
      return "Key Config";

    case CMD_PAN_RIGHT:
      return "Pan R";

    case CMD_PAN_LEFT:
      return "Pan L";

    case CMD_PAN_UP:
      return "Pan U";

    case CMD_PAN_DOWN:
      return "Pan D";

    case CMD_SHOW_KEYS:
      return "Show Keys";
  }

  for (i = 0; i < NUMDIRS; i++) {
    if (direction_keys[i] == *command) {
      switch (i) {
        case DIR_UP_LEFT:
          return "Up-Left";

        case DIR_UP:
          return "Up";

        case DIR_UP_RIGHT:
          return "Up-Right";

        case DIR_LEFT:
          return "Left";

        case DIR_WAIT:
          return "Wait";

        case DIR_RIGHT:
          return "Right";

        case DIR_DOWN_LEFT:
          return "Down-Left";

        case DIR_DOWN:
          return "Down";

        case DIR_DOWN_RIGHT:
          return "Down-Right";
      }
    }
  }

  for (i = 0; cmdlist[i].f_char; i++) {
    if (cmdlist[i].f_char == *command) {
      return cmdlist[i].name;
    }
  }
  
  return "";
}

/*
 * Translate the chordkey string into a keymask.
 */
u16 *nds_parse_key_string(char *keystr)
{
  static u16 keyarr[13];

  char *end;
  int i = 0;

  memset(keyarr, 0, sizeof(keyarr));

  while ((end = index(keystr, ',')) != NULL) {
    *end = '\0';

    keyarr[i++] = nds_string_to_key(nds_strip(keystr, 1, 1));

    keystr = end + 1;
  }

  keyarr[i++] = nds_string_to_key(nds_strip(keystr, 1, 1));

  return keyarr;
}

u16 nds_key_string_to_mask(char *keystr)
{
  u16 *keyarr = nds_parse_key_string(keystr);
  u16 keys = 0;
  int i;

  for (i = 0; keyarr[i]; i++) {
    keys |= keyarr[i];
  }

  return keys;
}

/*
 * Add an entry to the keymap.
 */
void nds_add_keymap_entry(u16 key, char command[INPUT_BUFFER_SIZE])
{
  int i;
  int entry_index = -1;

  for (i = 0; i < numkeys; i++) {
    if (keymap[i].key == key) {
      entry_index = i;
      break;
    }
  }

  if (entry_index < 0) {
    keymap = (nds_keymap_entry_t *)realloc(keymap, sizeof(nds_keymap_entry_t) * (numkeys + 1));
    entry_index = numkeys++;
  }

  keymap[entry_index].key = key;
  strcpy(keymap[entry_index].command, command);
}

/*
 * This finds all the keys which have the chord keys currently pressed as
 * part of their definition, and returns a string representation.
 */
char *nds_find_key_options(u16 *helpline, u16 key)
{
  static char buffer[BUFSZ];
  int i, j;
  u16 chords_pressed = (key & chord_keys);

  buffer[0] = '\0';

  if (chords_pressed == 0) {
    return buffer;
  }

  for (i = 0; helpline[i]; i++) {
    for (j = 0; j < numkeys; j++) {
      u16 non_chord_keys = (keymap[j].key & ~chords_pressed);

      if (*(keymap[j].command) == 0) {
        continue;
      }

      if ((keymap[j].key & ~chord_keys) != helpline[i]) {
        continue;
      }

      if ((non_chord_keys & chord_keys) != 0) {
        continue;
      } 

      if ((keymap[j].key & chord_keys) != chords_pressed) {
        continue;
      }

      if (*buffer) {
        strcat(buffer, ", ");
      }

      strcat(buffer, nds_key_to_string(non_chord_keys));
      strcat(buffer, "=");
      strcat(buffer, nds_command_to_string(keymap[j].command));
    }
  }

  return buffer;
}

/*
 * Convert the pressed keys into a set of commands, and return the number of
 * commands we've found.
 */
int nds_get_key_cmds(int pressed, char commands[MAXCMDS][INPUT_BUFFER_SIZE])
{
  int i;
  int numcmds = 0;

  /* First, look for an exact command-key match. */
  for (i = 0; i < numkeys; i++) {
    if (pressed == keymap[i].key) {
      strcpy(commands[numcmds++], keymap[i].command);
    }
  }

  /* If there was no match, then we'll try for simultaneous commands */
  if (numcmds == 0) {
    for (i = 0; i < numkeys; i++) {
      if ((pressed & keymap[i].key) == keymap[i].key) {
        strcpy(commands[numcmds++], keymap[i].command);
      }
    }
  }

  return numcmds;
}

int nds_map_key(u16 pressed)
{
  char commands[MAXCMDS][INPUT_BUFFER_SIZE];
  int numcmds = nds_get_key_cmds(pressed, commands);

  if (numcmds == 0) {
    /* Of course, if no command was mapped, return nothing */

    return 0;
  }

  /* 
   * Finally, if nothing else, we take the first mapped command, get the second
   * and remaining characters, stuff them in our input buffer, and then return
   * the first character of the command.
   */

  strcpy(input_buffer, &(commands[0][1]));

  return commands[0][0];
}

#define OLD_NUMKEYS 20

int nds_load_key_config()
{
  /* We need this for backward compatibility */
  u16 oldkeys[OLD_NUMKEYS] = {
    KEY_A,
    KEY_B,
    KEY_X,
    KEY_Y,
    KEY_SELECT,
    KEY_START,
    KEY_RIGHT,
    KEY_LEFT,
    KEY_UP,
    KEY_DOWN,
    KEY_A | KEY_R,
    KEY_B | KEY_R,
    KEY_X | KEY_R,
    KEY_Y | KEY_R,
    KEY_SELECT | KEY_R,
    KEY_START | KEY_R,
    KEY_RIGHT | KEY_R,
    KEY_LEFT | KEY_R,
    KEY_UP | KEY_R,
    KEY_DOWN | KEY_R,
  };

  FILE *fp = fopen(fqname(KEY_CONFIG_FILE, CONFIGPREFIX, 0), "r");
  int ret;
  int magic;
  char buffer[INPUT_BUFFER_SIZE];
  int i;
  int mapcnt;

  if (fp == (FILE *)0) {
    return -1;
  }

  if ((ret = fread(&magic, 1, sizeof(magic), fp)) < sizeof(magic)) {
    iprintf("Unable to read magic number!\n");
    return -1;
  }

  if (magic != KEY_MAGIC) {
    chord_keys = KEY_R;

    fclose(fp);
    fp = fopen(fqname(KEY_CONFIG_FILE, CONFIGPREFIX, 0), "r");

    for (i = 0; i < OLD_NUMKEYS; i++) {
      if ((ret = fread(buffer, 1, sizeof(buffer), fp)) < sizeof(buffer)) {
        iprintf("Unable to read keymap (%d != %d).\n", ret, sizeof(buffer));
        return -1;
      }

      if (*buffer) {
        nds_add_keymap_entry(oldkeys[i], buffer);
      }
    }
  } else {
    int i;
    u16 key;

    if ((ret = fread(&chord_keys, 1, sizeof(chord_keys), fp)) < sizeof(chord_keys)) {
      iprintf("Unable to read chord keys.\n");
      return -1; 
    }

    if ((ret = fread(&mapcnt, 1, sizeof(mapcnt), fp)) < sizeof(mapcnt)) {
      iprintf("Unable to read key count.\n");
      return -1;
    }

    for (i = 0; i < mapcnt; i++) {
      if ((ret = fread(&key, 1, sizeof(key), fp)) < sizeof(key)) {
        iprintf("Unable to read keymap key (%d != %d, %d of %d).\n", ret, sizeof(key), i, numkeys);
        return -1;
      }

      if ((ret = fread(buffer, 1, sizeof(buffer), fp)) < sizeof(buffer)) {
        iprintf("Unable to read keymap command.\n");
        return -1;
      }

      nds_add_keymap_entry(key, buffer);
    }
  }

  fclose(fp);

  return 0;
}

void nds_save_key_config()
{
  FILE *fp = fopen(fqname(KEY_CONFIG_FILE, CONFIGPREFIX, 0), "w");
  int i;
  int magic = KEY_MAGIC;

  if (fp == (FILE *)0) {
    return;
  }

  fwrite(&magic, 1, sizeof(magic), fp);
  fwrite(&chord_keys, 1, sizeof(chord_keys), fp);
  fwrite(&numkeys, 1, sizeof(numkeys), fp);

  for (i = 0; i < numkeys; i++) {
    u16 key = keymap[i].key;

    fwrite(&key, 1, sizeof(key), fp);
    fwrite(keymap[i].command, 1, INPUT_BUFFER_SIZE, fp);
  }

  fclose(fp);
}

const char *nds_get_bool_option()
{
  int i, j;
  int optcnt;

  winid win;
  menu_item *sel;
  ANY_P *ids;
  const char *res;

  for (i = 0, optcnt = 0; boolopt[i].name; i++) {
    if (boolopt[i].addr != NULL) {
      optcnt++;
    }
  }

  ids = (ANY_P *)malloc(sizeof(ANY_P) * optcnt);

  win = create_nhwindow(NHW_MENU);

  start_menu(win);

  for (i = 0, j = 0; boolopt[i].name; i++) {
    if ((boolopt[i].addr == NULL) || (boolopt[i].optflags != SET_IN_GAME)) {
      continue;
    }

    ids[j].a_int = i + 1;
    add_menu(win, NO_GLYPH, &(ids[j]), 0, 0, 0, boolopt[i].name, 0);

    j++;
  }

  end_menu(win, "Which Option Should Be Toggled?");

  if (select_menu(win, PICK_ONE, &sel) >= 0) {
    res = boolopt[sel->item.a_int - 1].name;
  } else {
    res = NULL;
  }

  destroy_nhwindow(win);
  NULLFREE(sel);
  NULLFREE(ids);

  return res;
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

nds_cmd_t nds_get_direction()
{
  winid win;
  menu_item *sel;
  ANY_P ids[25];
  nds_cmd_t cmd;
  int i, j, k;
  int res;

  char tmp[2];

  win = create_nhwindow(NHW_MENU);
  start_menu(win);

  ids[0].a_int = 0;

  for (i = 0, j = 1; i < 3; i++) {
    switch (i) {
      case 0:
        add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Walk", 0);
        break;

      case 1:
        add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Run", 0);
        break;

      case 2:
        add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Fight", 0);
        break;
    }

    for (k = 0; k < 8; j++, k++) {
      tmp[0] = direction_keys[k];
      tmp[1] = '\0';

      ids[j].a_int = j;

      add_menu(win, NO_GLYPH, &(ids[j]), 0, 0, 0, nds_command_to_string(tmp), 0);
    }
  }

  end_menu(win, "What Type of Movement?");
  res = select_menu(win, PICK_ONE, &sel);
  destroy_nhwindow(win);

  if (res <= 0) {
    cmd.f_char = -1;
    cmd.name = NULL;
  } else if (sel->item.a_int < 9) {
    tmp[0] = direction_keys[sel->item.a_int - 1];
    tmp[1] = '\0';

    cmd.f_char = direction_keys[sel->item.a_int - 1];
    cmd.name = nds_command_to_string(tmp);
  } else if (sel->item.a_int < 17) {
    input_buffer[0] = direction_keys[sel->item.a_int - 9];
    input_buffer[1] = '\0';

    cmd.f_char = 'g';
    cmd.name = nds_command_to_string(input_buffer);
  } else {
    input_buffer[0] = direction_keys[sel->item.a_int - 17];
    input_buffer[1] = '\0';

    cmd.f_char = 'F';
    cmd.name = nds_command_to_string(input_buffer);
  }

  NULLFREE(sel);

  return cmd;
}

nds_cmd_t nds_get_pan_direction()
{
  winid win;
  menu_item *sel;
  ANY_P ids[4];
  nds_cmd_t cmd;
  int res;

  win = create_nhwindow(NHW_MENU);
  start_menu(win);

  ids[0].a_int = CMD_PAN_UP;
  ids[1].a_int = CMD_PAN_DOWN;
  ids[2].a_int = CMD_PAN_LEFT;
  ids[3].a_int = CMD_PAN_RIGHT;

  add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Pan Up", 0);
  add_menu(win, NO_GLYPH, &(ids[1]), 0, 0, 0, "Pan Down", 0);
  add_menu(win, NO_GLYPH, &(ids[2]), 0, 0, 0, "Pan Left", 0);
  add_menu(win, NO_GLYPH, &(ids[3]), 0, 0, 0, "Pan Right", 0);

  end_menu(win, "What Direction?");
  res = select_menu(win, PICK_ONE, &sel);
  destroy_nhwindow(win);

  if (res <= 0) {
    cmd.f_char = -1;
    cmd.name = NULL;
  } else {
    cmd.f_char = sel->item.a_int;

    if (cmd.f_char == CMD_PAN_UP) {
      cmd.name = "Pan Up";
    } else if (cmd.f_char == CMD_PAN_DOWN) {
      cmd.name = "Pan Down";
    } else if (cmd.f_char == CMD_PAN_LEFT) {
      cmd.name = "Pan Left";
    } else if (cmd.f_char == CMD_PAN_RIGHT) {
      cmd.name = "Pan Right";
    }
  }

  NULLFREE(sel);

  return cmd;
}

nds_cmd_t nds_get_config_cmd(u16 key)
{
  winid win;
  menu_item *sel;
  ANY_P ids[15];
  nds_cmd_t cmd;
  char tmp[BUFSZ];
  int res;

  win = create_nhwindow(NHW_MENU);
  start_menu(win);

  ids[0].a_int = 1;
  ids[1].a_int = 2;
  ids[2].a_int = 3;
  ids[3].a_int = 4;
  ids[4].a_int = 5;

  add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Movement", 0);
  add_menu(win, NO_GLYPH, &(ids[1]), 0, 0, 0, "Game Command", 0);
  add_menu(win, NO_GLYPH, &(ids[2]), 0, 0, 0, "Toggle Option", 0);
  add_menu(win, NO_GLYPH, &(ids[3]), 0, 0, 0, "Map Panning", 0);
  add_menu(win, NO_GLYPH, &(ids[4]), 0, 0, 0, "No Command", 0);

  sprintf(tmp, "What do you want to assign to %s?", nds_key_to_string(key));
  end_menu(win, tmp);
  res = select_menu(win, PICK_ONE, &sel);
  destroy_nhwindow(win);

  if (res <= 0) {
    cmd.f_char = -1;
    cmd.name = NULL;
  } else {
    switch (sel->item.a_int) {
      case 1:
        cmd = nds_get_direction();
        break;

      case 2:
        nds_flush(0);
        cmd = nds_cmd_loop(CMDLOOP_CONFIG);
        break;

      case 3:
        {
          const char *tmp = nds_get_bool_option();

          if (tmp != NULL) {
            cmd.f_char = CMD_OPT_TOGGLE;
            cmd.name = "Toggle Option";

            strcpy(input_buffer, tmp);
          } else {
            cmd.f_char = -1;
            cmd.name = NULL;
          }
        }

        break;

      case 4:
        cmd = nds_get_pan_direction();
        break;

      case 5:
        nds_flush(0);

        cmd.f_char = 0;
        cmd.name = NULL;
        break;
    }
  }

  NULLFREE(sel);

  return cmd;
}

void nds_config_key()
{
  int prev_held = 0;
  int held = 0;
  nds_cmd_t cmd;
  char buf[BUFSZ];

  u16 key;
  char command[INPUT_BUFFER_SIZE];

  nds_flush(0);

  nds_draw_prompt("Press the key to modify.");

  while (1) {
    swiWaitForVBlank();

    scanKeys();

    prev_held = held;
    held = nds_keysHeld();

    /* We don't let the user configure these */

    if ((prev_held & chord_keys) && (held == 0) && (nds_count_bits(prev_held) == 1)) {
      key = prev_held;
      break;
    } else if ((held & cmd_key) ||
               ((held & ~chord_keys) == 0)) {
      continue;
    } else if (held) {
      key = held;
      break;
    }
  }

  nds_clear_prompt();

  cmd = nds_get_config_cmd(key);

  if (cmd.f_char < 0) {
    return;
  }

  command[0] = cmd.f_char;
  command[1] = '\0';

  if (*input_buffer) {
    strcat(command, input_buffer);
    strcpy(input_buffer, command);

    input_buffer[strlen(input_buffer) - 1] = '\0';

    sprintf(buf, "Mapped %s to %s x%s (%s).", nds_key_to_string(key), cmd.name, input_buffer, command);

    input_buffer[0] = '\0';
  } else {
    sprintf(buf, "Mapped %s to %s.", nds_key_to_string(key), cmd.name);
  }

  nds_add_keymap_entry(key, command);

  clear_nhwindow(WIN_MESSAGE);
  putstr(WIN_MESSAGE, ATR_NONE, buf);

  nds_save_key_config();

  nds_flush(0);
}

void nds_show_keys()
{
  winid win;
  int i;
  char buffer[BUFSZ];

  win = create_nhwindow(NHW_TEXT);

  for (i = 0; i < numkeys; i++) {
    sprintf(buffer, "%s = %s", nds_key_to_string(keymap[i].key), nds_command_to_string(keymap[i].command));
    putstr(win, ATR_NONE, buffer);
  }

  display_nhwindow(win, FALSE);
  destroy_nhwindow(win);
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
  if ( ((ABS(mx - u.ux) <= 1) && (ABS(my - u.uy) <= 1)) ||
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

int nds_get_input(int *x, int *y, int *mod)
{
  static int chord_pressed = 0;

  int tx, ty, old_tx, old_ty, start_tx, start_ty;
  int cx, cy;

  touchPosition coords;
  int held_frames = 0;
  int dragging = 0;
  int held = 0;
  int prev_held = 0;

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

    scanKeys();

    if (iflags.keyrepeat) {
      pressed = nds_keysDownRepeat();
    } else {
      pressed = nds_keysDown();
    }

    prev_held = held;
    held = nds_keysHeld();

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

      if (! iflags.holdmode) {
        nds_flush(0);
      }
      
      if (iflags.cmdwindow) {
        cmd = nds_cmd_loop(CMDLOOP_STANDARD);
      } else {
        cmd = nds_kbd_cmd_loop();
      }

      key = cmd.f_char;
    } else if ((prev_held & chord_keys) && (held == 0)) {
      if (! chord_pressed) {
        key = nds_map_key(prev_held);
      } else {
        chord_pressed = 0;
      }
    } else if (pressed & ~chord_keys) {
      key = nds_map_key(held | pressed);
      chord_pressed = 1;
    }

    switch (key) {
      case 0:
        if (iflags.keyhelp && (held != prev_held)) {
          nds_render_key_help_string(held);
        }

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

      case CMD_OPT_TOGGLE:
        nds_toggle_bool_option(input_buffer);
        *input_buffer = '\0';

        display_nhwindow(WIN_MAP, FALSE);

        break;

      case CMD_SHOW_KEYS:
        nds_show_keys();
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

  nds_clear_prompt();

  return 0;
}

struct obj *obj_for_let(char invlet)
{
  struct obj *otmp;

  for (otmp = invent; otmp; otmp = otmp->nobj) {
    if (otmp->invlet == invlet) {
      return otmp;
    }
  }

  return NULL;
}

int class_slot_for_class(char oclass)
{
  int i;

  for (i = 0; i < MAXOCLASSES; i++) {
    if (flags.inv_order[i] == oclass) {
      return i;
    }
  }

  return 0;
}

void _nds_insert_choice(char *choices, char let)
{
#ifdef SORTLOOT
  if (iflags.sortloot == 'f') {
    struct obj *otmp = obj_for_let(let);
    int len = strlen(choices);
    int i;

    for (i = 0; choices[i]; i++) {
      struct obj *ochoice = obj_for_let(choices[i]);

      if (strcmpi(cxname2(otmp), cxname2(ochoice)) < 0) {
        break;
      }
    }

    if (i == len) {
      choices[i] = let;
      choices[i + 1] = '\0';
    } else {
      memmove(&(choices[i + 1]), &(choices[i]), len - i + 1);
      choices[i] = let;
    }
  } else {
#endif
    char tmp[2];

    tmp[0] = let;
    tmp[1] = '\0';

    strcat(choices, tmp);
#ifdef SORTLOOT
  }
#endif
}

char *_nds_parse_choices(const char *ques)
{
  static char choices[BUFSZ];

  char choices_by_class[MAXOCLASSES][BUFSZ / MAXOCLASSES];
  char special_choices[BUFSZ / MAXOCLASSES];

  int i;

  char *ptr = index(ques, '[');
  char last_choice = -1;
  int have_hyphen = 0;
  
  if (ptr == NULL) {
    return NULL;
  } else {
    ptr++;
  }

  for (i = 0; i < MAXOCLASSES; i++) {
    choices_by_class[i][0] = '\0';
  }

  special_choices[0] = '\0';

  for (i = 0; ptr[i] && (ptr[i] != ']'); i++) {

    struct obj *otmp;

    if (strncmp(ptr + i, " or ", 4) == 0) {
      i += 3;
    } else if (ISWHITESPACE(ptr[i])) {
      continue;
    } else if ((ptr[i] == '-') && ! ISWHITESPACE(ptr[i + 1])) {
      have_hyphen = 1;
    } else if (ptr[i] == '$') {
        _nds_insert_choice(choices_by_class[class_slot_for_class(COIN_CLASS)], ptr[i]);
    } else if (have_hyphen) {
      int j;

      for (j = last_choice + 1; j <= ptr[i]; j++) {
        int idx;

        otmp = obj_for_let(j);
        idx = class_slot_for_class(otmp->oclass);

        _nds_insert_choice(choices_by_class[idx], j);
      }

      have_hyphen = 0;
    } else {
      if ((otmp = obj_for_let(ptr[i])) != NULL) {
        int idx = class_slot_for_class(otmp->oclass);

        _nds_insert_choice(choices_by_class[idx], ptr[i]);
      } else {
        char tmp[2];

        tmp[0] = ptr[i];
        tmp[1] = '\0';

        strcat(special_choices, tmp);
      }

      last_choice = ptr[i];
    }
  }

  choices[0] = '\0';

  for (i = 0; i < MAXOCLASSES; i++) {
    strcat(choices, choices_by_class[i]);
  }

  if (*special_choices) {
    strcat(choices, " ");
    strcat(choices, special_choices);
  }

  return choices;
}

char nds_yn_function(const char *ques, const char *cstr, CHAR_P def)
{
  char *choices;
  ANY_P header_id;
  ANY_P *ids;
  winid win;
  menu_item *sel;
  int ret;
  int yn = 0;
  int ynaq = 0;

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
      nds_flush(0);
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
  } else if (! iflags.cmdwindow) {
    return nds_prompt_char(ques, cstr, 0);
  } else if (strstr(ques, "Adjust letter to what") != NULL) {
    return nds_prompt_char(ques, cstr, 0);
  } else if (strstr(ques, "What command?") != NULL) {
    nds_cmd_t cmd;
    
    nds_draw_prompt("Select a command.");
    nds_flush(0);
    cmd = nds_cmd_loop(CMDLOOP_WHATDOES);
    nds_clear_prompt();

    return cmd.f_char;
  } else if (strstr(ques, "What do you look for?") != NULL) {
    return nds_prompt_char(ques, cstr, 0);
  }

  if ((index(ques, '[') == NULL) && (cstr == NULL)) {
    nds_draw_prompt(ques);
    return '*';
  }

  win = create_nhwindow(NHW_MENU);

  start_menu(win);
  
  if ((cstr != NULL) && 
      ((strcasecmp(cstr, ynchars) == 0) ||
       (strcasecmp(cstr, ynqchars) == 0) ||
       ((ynaq = strcasecmp(cstr, ynaqchars)) == 0))) {

    ids = (ANY_P *)malloc(sizeof(ANY_P) * 2);

    yn = 1;

    ids[0].a_int = 'y';
    ids[1].a_int = 'n';

    add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Yes", 0);
    add_menu(win, NO_GLYPH, &(ids[1]), 0, 0, 0, "No", 0);

    if (ynaq) {
      ids[2].a_int = 'a';

      add_menu(win, NO_GLYPH, &(ids[2]), 0, 0, 0, "All", 0);
    }
  } else if ((cstr != NULL) && (strcasecmp(cstr, "rl") == 0)) {

    ids = (ANY_P *)malloc(sizeof(ANY_P) * 2);

    ids[0].a_int = 'r';
    ids[1].a_int = 'l';

    add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Right Hand", 0);
    add_menu(win, NO_GLYPH, &(ids[1]), 0, 0, 0, "Left Hand", 0);
  } else {
    int i;
    char curclass = -1;

    choices = _nds_parse_choices(ques);

    ids = (ANY_P *)malloc(sizeof(ANY_P) * strlen(choices));
    header_id.a_int = 0;

    for (i = 0; i < strlen(choices); i++) {

      ids[i].a_int = choices[i];

      if (choices[i] == ' ') {
        add_menu(win, NO_GLYPH, &(header_id), 0, 0, 0, "Other", 0);
      } else if (choices[i] == '*') {
        add_menu(win, NO_GLYPH, &(ids[i]), 0, 0, 0, "Something from your inventory", 0);
      } else if ((choices[i] == '-') || (choices[i] == '.')) {
        add_menu(win, NO_GLYPH, &(ids[i]), 0, 0, 0, "Nothing/your finger", 0);
      } else if (choices[i] == '?') {
        continue;
      } else {
        int oclass;
        char oname[BUFSZ];

        if (choices[i] == '$') {
          oclass = COIN_CLASS;
          sprintf(oname, "%ld gold piece%s", u.ugold, plur(u.ugold));
        } else {
          struct obj *otmp = obj_for_let(choices[i]);

          oclass = otmp->oclass;
          strcpy(oname, doname(otmp));
        }

        if (oclass != curclass) {
          add_menu(win, NO_GLYPH, &(header_id), 0, 0, 0, let_to_name(oclass, FALSE), 0);

          curclass = oclass;
        } 

        add_menu(win, NO_GLYPH, &(ids[i]), 0, 0, 0, oname, 0);
      }
    }
  }

  end_menu(win, ques);

  if (select_menu(win, PICK_ONE, &sel) <= 0) {
    ret = yn ? 'n' : '\033';
  } else {
    ret = sel->item.a_int;
    free(sel);
  }

  free(ids);

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

    pressed = nds_keysDownRepeat();
    held = nds_keysHeld();
    coords = get_touch_coords();

    if ((((optype == CMDLOOP_CONFIG) || ! iflags.holdmode) && (pressed & KEY_B)) ||
        (((optype != CMDLOOP_CONFIG) && ! (held & cmd_key) && iflags.holdmode)) ||
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

  DISPLAY_CR ^= DISPLAY_BG2_ACTIVE;
  BG2_CR = old_bg_cr;

  /* 
   * If an extended command was requested, we need to get it from the
   * user.
   */
  if ((picked_cmd.f_char == '#') && (optype != CMDLOOP_WHATDOES)) {
    int idx = get_ext_cmd();

    if (idx >= 0) {
      /* Now stuff the command into our input buffer. */

      strcpy(input_buffer, extcmdlist[idx].ef_txt);
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

  if (*input_buffer) {
    char buffer[INPUT_BUFFER_SIZE];

    strcpy(buffer, input_buffer);
    *input_buffer = '\0';

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
