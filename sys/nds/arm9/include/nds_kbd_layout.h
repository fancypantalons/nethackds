/*
 * Keyboard layout definition.  Each row represents a set of
 * keys, each of which have a width and a corresponding key code.
 *
 * Note, this must line up with the keyboard graphics.
 */

#define KEYBOARD_ESC_X_START 8
#define KEYBOARD_ESC_X_END 24

#define KEYBOARD_F_ROW_Y_START 80
#define KEYBOARD_F_ROW_Y_END 96

#define KEYBOARD_F_ROW_X_START 40
#define KEYBOARD_F_ROW_X_END 248

#define KEYBOARD_X_START 8
#define KEYBOARD_X_END 248
#define KEYBOARD_Y_START 104
#define KEYBOARD_Y_END 184

#define ARROW_PAD_X_START 104
#define ARROW_PAD_X_END 152
#define ARROW_PAD_Y_START 24
#define ARROW_PAD_Y_END 72

#define KEY_HEIGHT 16
#define ARROW_KEY_WIDTH 16
#define F_KEY_WIDTH 16

/* Four keys per section, eigth pixels separating the sections. */
#define F_KEYS_PER_SECTION 4
#define F_ROW_SECTION_WIDTH (F_KEY_WIDTH * F_KEYS_PER_SECTION) + 8

const nds_kbd_key row0[] = {
  { .width = 16, .code = (u16)'`'}, 
  { .width = 16, .code = (u16)'1'}, 
  { .width = 16, .code = (u16)'2'}, 
  { .width = 16, .code = (u16)'3'}, 
  { .width = 16, .code = (u16)'4'}, 
  { .width = 16, .code = (u16)'5'}, 
  { .width = 16, .code = (u16)'6'}, 
  { .width = 16, .code = (u16)'7'},
  { .width = 16, .code = (u16)'8'}, 
  { .width = 16, .code = (u16)'9'}, 
  { .width = 16, .code = (u16)'0'}, 
  { .width = 16, .code = (u16)'-'}, 
  { .width = 16, .code = (u16)'='}, 
  { .width = 32, .code = (u16)'\b'}, 
  { .width = 0, .code = 0}
};

const nds_kbd_key row1[] = {
  { .width = 24, .code = (u16)'\t'}, 
  { .width = 16, .code = (u16)'q'}, 
  { .width = 16, .code = (u16)'w'}, 
  { .width = 16, .code = (u16)'e'}, 
  { .width = 16, .code = (u16)'r'}, 
  { .width = 16, .code = (u16)'t'}, 
  { .width = 16, .code = (u16)'y'}, 
  { .width = 16, .code = (u16)'u'}, 
  { .width = 16, .code = (u16)'i'}, 
  { .width = 16, .code = (u16)'o'}, 
  { .width = 16, .code = (u16)'p'}, 
  { .width = 16, .code = (u16)'['}, 
  { .width = 16, .code = (u16)']'}, 
  { .width = 24, .code = (u16)'\\'}, 
  { .width = 0, .code = 0}
};

const nds_kbd_key row2[] = {
  { .width = 32, K_CAPS}, 
  { .width = 16, .code = (u16)'a'}, 
  { .width = 16, .code = (u16)'s'}, 
  { .width = 16, .code = (u16)'d'}, 
  { .width = 16, .code = (u16)'f'}, 
  { .width = 16, .code = (u16)'g'}, 
  { .width = 16, .code = (u16)'h'}, 
  { .width = 16, .code = (u16)'j'}, 
  { .width = 16, .code = (u16)'k'}, 
  { .width = 16, .code = (u16)'l'}, 
  { .width = 16, .code = (u16)';'}, 
  { .width = 16, .code = (u16)'\''}, 
  { .width = 32, .code = (u16)'\n'}, 
  { .width = 0, .code = 0}
};

const nds_kbd_key row3[] = {
  { .width = 40, .code = K_SHIFT}, 
  { .width = 16, .code = (u16)'z'}, 
  { .width = 16, .code = (u16)'x'}, 
  { .width = 16, .code = (u16)'c'}, 
  { .width = 16, .code = (u16)'v'}, 
  { .width = 16, .code = (u16)'b'}, 
  { .width = 16, .code = (u16)'n'}, 
  { .width = 16, .code = (u16)'m'}, 
  { .width = 16, .code = (u16)','}, 
  { .width = 16, .code = (u16)'.'}, 
  { .width = 16, .code = (u16)'/'}, 
  { .width = 40, .code = K_SHIFT}, 
  { .width = 0, 0}
};

const nds_kbd_key row4[] = {
  { .width = 32, .code = K_CTRL}, 
  { .width = 24, .code = K_ALT}, 
  { .width = 128, .code = (u16)' '}, 
  { .width = 24, .code = K_ALT}, 
  { .width = 32, .code = K_CTRL}, 
  { .width = 0, .code = 0}
};

const nds_kbd_key *kbdrows[] = {
  row0, 
  row1, 
  row2, 
  row3, 
  row4,
  NULL
};

const u8 vi_row0[] = {
  K_UP_LEFT, K_UP, K_UP_RIGHT 
};

const u8 vi_row1[] = {
  K_LEFT, K_NOOP, K_RIGHT
};

const u8 vi_row2[] = {
  K_DOWN_LEFT, K_DOWN, K_DOWN_RIGHT
};

const u8 *vi_move[] = { 
  vi_row0,
  vi_row1,
  vi_row2
};
