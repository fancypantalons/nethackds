#include <nds.h>

#include "hack.h"
#include "nds_debug.h"
#include "nds_win.h"
#include "nds_config.h"
#include "nds_util.h"
#include "nds_cmd.h"

typedef struct {
  u16 key;
  char command[INPUT_BUFFER_SIZE];
} nds_keymap_entry_t;

nds_keymap_entry_t *keymap = NULL;

int numkeys = 0;

u16 chord_keys;
u16 cmd_key;

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
  char *direction_keys = nds_get_direction_keys();
  nds_cmd_t *cmdlist = nds_get_cmdlist();

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

int nds_command_key_pressed(int pressed)
{
  return pressed & cmd_key;
}

int nds_chord_key_held(int pressed)
{
  return pressed & chord_keys;
}

int nds_chord_key_pressed(int pressed)
{
  return (pressed & chord_keys) &&
         (! (pressed & ~chord_keys));
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

  nds_input_buffer_append(&(commands[0][1]));

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
    DEBUG_PRINT("Unable to read magic number!\n");
    return -1;
  }

  if (magic != KEY_MAGIC) {
    chord_keys = KEY_R;

    fclose(fp);
    fp = fopen(fqname(KEY_CONFIG_FILE, CONFIGPREFIX, 0), "r");

    for (i = 0; i < OLD_NUMKEYS; i++) {
      if ((ret = fread(buffer, 1, sizeof(buffer), fp)) < sizeof(buffer)) {
        DEBUG_PRINT("Unable to read keymap (%d != %d).\n", ret, sizeof(buffer));
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
      DEBUG_PRINT("Unable to read chord keys.\n");
      return -1; 
    }

    if ((ret = fread(&mapcnt, 1, sizeof(mapcnt), fp)) < sizeof(mapcnt)) {
      DEBUG_PRINT("Unable to read key count.\n");
      return -1;
    }

    for (i = 0; i < mapcnt; i++) {
      if ((ret = fread(&key, 1, sizeof(key), fp)) < sizeof(key)) {
        DEBUG_PRINT("Unable to read keymap key (%d != %d, %d of %d).\n", ret, sizeof(key), i, numkeys);
        return -1;
      }

      if ((ret = fread(buffer, 1, sizeof(buffer), fp)) < sizeof(buffer)) {
        DEBUG_PRINT("Unable to read keymap command.\n");
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

/*
 * Key Configuration UI.
 */

nds_cmd_t nds_get_direction()
{
  winid win;
  menu_item *sel;
  ANY_P ids[25];
  nds_cmd_t cmd;
  int i, j, k;
  int res;

  char tmp[2];
  char *direction_keys = nds_get_direction_keys();

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
    nds_input_buffer_push(direction_keys[sel->item.a_int - 9]);

    cmd.f_char = 'g';
    cmd.name = nds_command_to_string(nds_input_buffer_shiftall());
  } else {
    nds_input_buffer_push(direction_keys[sel->item.a_int - 17]);

    cmd.f_char = 'F';
    cmd.name = nds_command_to_string(nds_input_buffer_shiftall());
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

            nds_input_buffer_append((char *)tmp);
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

  if (! nds_input_buffer_is_empty()) {
    strcat(command, nds_input_buffer_shiftall());

    DEBUG_PRINT("cmd: %s\n", command);
    sprintf(buf, "Mapped %s to %s x%s.", nds_key_to_string(key), cmd.name, command);
  } else {
    sprintf(buf, "Mapped %s to %s.", nds_key_to_string(key), cmd.name);
  }

  nds_add_keymap_entry(key, command);

  clear_nhwindow(WIN_MESSAGE);
  putstr(WIN_MESSAGE, ATR_NONE, buf);

  nds_save_key_config();

  nds_flush(0);
}

/*
 * Display the key configuration currently available.
 */

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

/*
 * Initializer function for the key config module.
 */

int nds_key_config_init()
{
  u16 chord_keys_config;
  u16 cmd_key_config;
  char *direction_keys = nds_get_direction_keys();

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
    DEBUG_PRINT("Invalid command key: %s\n", iflags.cmdkey);

    return -1;
  }

  return 0;
}
