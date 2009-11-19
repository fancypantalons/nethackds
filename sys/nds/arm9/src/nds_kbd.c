/*
 *
 * Graphical keyboard implementation (this code was originally lifted from...
 * somewhere... though I can't remember where, at this point) and then heavily
 * modified and cleaned up.
 *
 * The keyboard is rendered on a 32x32 text background with the usual
 * 8x8 tiles (for a total of 256x256 pixels).  Each key is rendered as
 * some number of tiles horizontally, as defined by the key width in the
 * row definition, and currently, 2 tiles vertically.
 *
 * Additionally, there are four unique keyboard layouts, each of which 
 * using the same set of tiles.  These layouts corresponds to the normal,
 * unshifted layout, the shifted layout, the capslock layout, and the shift
 * + capslock layout.
 */

#include <string.h>

#include "nds_kbd.h"
#include "nds_util.h"
#include "nds_gfx.h"

#include "nds_kbd_layout.h"

// down-left, down, down-right, left, pause, right, up-left, up, up-right

char shifts[] = "`~1!2@3#4$5%6^7&8*9(0)-_=+[{]}\\|;:\'\",<.>/?";

static int shift = false, ctrl = false, alt = false, caps = false;

// clear this to prevent alt-b, f5, and f6 from having their special effects
// it's cleared during getlin, yn_function, etc
u8 process_special_keystrokes = 1;

void kbd_init() {
  u16 palette[4];

  nds_load_file("/NetHack/kbd.bin", (void *)BG_TILE_RAM(0));
  nds_load_file("/NetHack/kbd.map", (void *)BG_MAP_RAM(4));
  nds_load_palette("/NetHack/kbd.pal", palette);

  BG_PALETTE[0] = RGB15(0, 0, 0);   /* Regular background */
  BG_PALETTE[16] = RGB15(0, 0, 0);

  BG_PALETTE[1] = palette[0];       /* Key background, normal   */
  BG_PALETTE[17] = palette[1];      /* Key background, selected */

  BG_PALETTE[2] = palette[2];       /* Key text, normal */
  BG_PALETTE[18] = palette[3];      /* Key text, selected */
}

void kbd_set_map() {
  REG_BG0CNT = BG_TILE_BASE(0) | BG_MAP_BASE(4 + (caps | (shift<<1))) | BG_PRIORITY(0) | BG_COLOR_16;
}

/*
 * Here we take a key code and apply any modifiers, and return the resultant
 * code.
 */
u16 kbd_mod_code(u16 ret) {
  char* temp;

  if (ret & K_MODIFIER) {
    /* If it's a modified key, just return it straight away */

    return ret;
  }

  if ((ret >= 'a') && (ret <= 'z') && ((caps && !shift) || (shift && !caps))) {
    /* If it's a letter and it should be upper case, up-case it */

    ret -= 0x20;
  }
  else if (((temp = strchr(shifts, ret)) != NULL) && shift) {
    /* If it's one of the numbers and we're shifted, switch to the shifted version */

    ret = *(temp + 1);
  }

  if (alt) {
    ret |= 0x80;
  }

  if (ctrl) {
    ret &= 0x1F;
  }

  return ret;
}

void kbd_set_color_from_pos(u16 row, u16 key_in_row, u8 color) {
  u16 x, y;
  u16 x_start = 0;

  /* Calculate the map number and then the pointer to the layout */
  int mapNum = caps | (shift << 1);
  u16* map = (u16*)(BG_MAP_RAM(4 + mapNum) + 13 * 32);

  /* Calculate the map offset of the key that was depressed. */
  for (y = 0; y < key_in_row; y++) {
    x_start += kbdrows[row][y].width / 8;
  }

  /* Now update the tile for the key to change its colour. */
  for (y = 0; y < 2; y++) {
    for (x = 0; x < (kbdrows[row][key_in_row].width / 8); x++) {
      /*
       * There's two rows per key, and each row is 32 half-words
       * wide.  The key then starts at some x offset, and then the
       * key is composed of some number of columns.  Finally, we
       * skip the first tile, as it's the black, background tile
       *
       * Knowing that, the following calculation should be self-
       * explanatory, right?
       */
      int idx = (row * 2 + y) * 32 + x + x_start + 1;

      map[idx] = (map[idx] & 0x0FFF) | (color << 12);
    }
  }
}

void kbd_set_color_from_code(u16 code, u8 color) {
  u16 row, col;

  /*
   * Search for the key that, including modifiers, matches the
   * passed-in key, and set the colour accordingly.
   *
   * Note, there may be multiple keys of the same type 
   * (two shifts, two alts), so we don't break out of the 
   * loop even if we find the key.
   */
  for (row = 0; kbdrows[row] != NULL; row++) {
    for (col = 0; kbdrows[row][col].width != 0; col++) {
      if (kbd_mod_code(kbdrows[row][col].code) == code) {
        kbd_set_color_from_pos(row, col, color);
      }
    }
  }
}

u16 kbd_xy2key(u8 x, u8 y) {

  /*
   * First, check if it's an arrow-pad key press.
   */
  if ((x >= ARROW_PAD_X_START) &&
      (x < ARROW_PAD_X_END) &&
      (y >= ARROW_PAD_Y_START) &&
      (y < ARROW_PAD_Y_END)) {

    u8 kx = (x - ARROW_PAD_X_START) / ARROW_KEY_WIDTH;
    u8 ky = (y - ARROW_PAD_Y_START) / KEY_HEIGHT;

    return vi_move[ky][kx];
  }

  /*
   * Next, it might be an F-row key.
   */
  if ((y >= KEYBOARD_F_ROW_Y_START) &&
      (y < KEYBOARD_F_ROW_Y_END)) {

    if ((x >= KEYBOARD_ESC_X_START) &&
        (x < KEYBOARD_ESC_X_END)) {
      /* Escape stands on it's own, so we check for it specially. */

      return '\e';
    } else if ((x >= KEYBOARD_F_ROW_X_START) &&
               (x < KEYBOARD_F_ROW_X_END)) {

      /* 
       * We need to calculate the F-key that was pressed. 
       *
       * Now, each F-key is 16 pixels wide, and there's 8
       * pixels separating each four-key grouping.  So we
       * end up with 72 pixels per section.
       *
       * So first, we calculate the section (16 * 4 + 8 = 72)
       * then compute the key number within that section.  Once
       * we have that, it's a simple calculation to get the 
       * key number itself.
       */
      int section, key_in_section;

      section = (x - KEYBOARD_F_ROW_X_START) / F_ROW_SECTION_WIDTH;
      key_in_section = (x - KEYBOARD_F_ROW_X_START - section * F_ROW_SECTION_WIDTH) / F_KEY_WIDTH;

      return (key_in_section < F_KEYS_PER_SECTION) 
        ? K_F(section * F_KEYS_PER_SECTION + key_in_section + 1) 
        : 0;
    }
  }

  /*
   * Lastly, it could be a tap in the keyboard proper.
   */
  if ((x >= KEYBOARD_X_START) &&
      (x < KEYBOARD_X_END) &&
      (y >= KEYBOARD_Y_START) &&
      (y < KEYBOARD_Y_END)) {

    int i;
    int cur_x = KEYBOARD_X_START;
    u16 row = (y - KEYBOARD_Y_START) / KEY_HEIGHT;

    for (i = 0; kbdrows[row][i].width != 0; cur_x += kbdrows[row][i++].width) {

      int end_x = cur_x + kbdrows[row][i].width;

      if ((x >= cur_x) && (x < end_x)) {
        return kbd_mod_code(kbdrows[row][i].code);
      }
    }
  }

  return 0;
}

void kbd_dotoggle(int* flag, toggle_type how) {
  switch (how) {
    case TOGGLE_FLIP:
      *flag = !*flag;
      break;

    case TOGGLE_DISABLE:
      *flag = false;
      break;
  }
}

void kbd_togglemod(int which, toggle_type how) {
  switch (which) {
    case K_CTRL: 
      kbd_dotoggle(&ctrl, how); 
      break;

    case K_SHIFT: 
      kbd_dotoggle(&shift, how); 
      break;

    case K_ALT: 
      kbd_dotoggle(&alt, how); 
      break;

    case K_CAPS: 
      kbd_dotoggle(&caps, how); 
      break;

    case K_MODIFIER:
      kbd_dotoggle(&ctrl, how);
      kbd_dotoggle(&shift, how);
      kbd_dotoggle(&alt, how);
      //
      // NOT caps!!  This is called to un-set shift, ctrl, and alt after
      // a key is pressed.  Unsetting caps here would cause it to be the
      // same as shift.
      break;
  }

  kbd_set_color_from_code(K_SHIFT, shift);
  kbd_set_color_from_code(K_CTRL, ctrl);
  kbd_set_color_from_code(K_ALT, alt);
  kbd_set_color_from_code(K_CAPS, caps);

  kbd_set_map();
}

/*
 * Executes an iteration of the keyboard loop.  This will return
 * a key, if one was pressed, or zero.
 *
 * Note, this assumes scankeys was already called.
 */
u8 kbd_do_one_loop() {

  static u16 last_code; /* The code of the previous key that was pressed */
  static u16 held_code; /* The code of the currently pressed key */
  u16 keys_down = nds_keysDownRepeat(); 

  if (keys_down & KEY_UP) {
    return K_UP;
  } else if (keys_down & KEY_DOWN) {
    return K_DOWN;
  } else if (keys_down & KEY_LEFT) {
    return K_LEFT;
  } else if (keys_down & KEY_RIGHT) {
    return K_RIGHT;
  }

  last_code = held_code;

  if (nds_keysHeld() & KEY_TOUCH) {
    u16 the_x = IPC->touchXpx;
    u16 the_y = IPC->touchYpx;

    // get the keycode that corresponds to this key
    u16 keycode = kbd_xy2key(the_x,the_y);

    // if it's not a modifier, hilite it
    if (keycode && ! (keycode & K_MODIFIER))  {
      kbd_set_color_from_code(keycode,1);
    }

    // set last_code so it can be un-hilited later
    held_code = keycode;
  } else {
    held_code = 0;
  }

  if ((last_code != 0) && (held_code != last_code) && ! (last_code & K_MODIFIER)) {

    kbd_set_color_from_code(last_code,0);	// clear the hiliting on this key
    kbd_togglemod(K_MODIFIER, TOGGLE_DISABLE);	// and also clear all modifiers (except caps)

    last_code = 0;
  }

  if (keys_down & KEY_TOUCH) {
    u16 the_x = IPC->touchXpx;
    u16 the_y = IPC->touchYpx;

    // get the keycode that corresponds to this key
    u16 keycode = kbd_xy2key(the_x,the_y);

    // if it's a modifier, toggle it
    if (keycode & K_MODIFIER) 
      kbd_togglemod(keycode, TOGGLE_FLIP);
    else if ((keycode & 0x7F) != 0) {	// it's an actual keystroke, return it
      return (keycode & 0xFF);
    }
  }

  return 0;
}

