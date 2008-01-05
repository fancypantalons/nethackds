#include <string.h>

#include "ds_kbd.h"
#include "nds_util.h"
#include "nds_gfx.h"

const nds_kbd_key row0[] = {
	{16,(u16)'`'}, {16,(u16)'1'}, {16,(u16)'2'}, {16,(u16)'3'}, {16,(u16)'4'}, 
	{16,(u16)'5'}, {16,(u16)'6'}, {16,(u16)'7'}, {16,(u16)'8'}, {16,(u16)'9'}, 
	{16,(u16)'0'}, {16,(u16)'-'}, {16,(u16)'='}, {32,(u16)'\b'}, {0,0}};
const nds_kbd_key row1[] = {
	{24,(u16)'\t'}, {16,(u16)'q'}, {16,(u16)'w'}, {16,(u16)'e'}, {16,(u16)'r'}, 
	{16,(u16)'t'}, {16,(u16)'y'}, {16,(u16)'u'}, {16,(u16)'i'}, {16,(u16)'o'}, 
	{16,(u16)'p'}, {16,(u16)'['}, {16,(u16)']'}, {24,(u16)'\\'}, {0,0}};
const nds_kbd_key row2[] = {
	{32,K_CAPS}, {16,(u16)'a'}, {16,(u16)'s'}, {16,(u16)'d'}, {16,(u16)'f'}, 
	{16,(u16)'g'}, {16,(u16)'h'}, {16,(u16)'j'}, {16,(u16)'k'}, {16,(u16)'l'}, 
	{16,(u16)';'}, {16,(u16)'\''}, {32,(u16)'\n'}, {0,0}};
const nds_kbd_key row3[] = {
	{40,K_SHIFT}, {16,(u16)'z'}, {16,(u16)'x'}, {16,(u16)'c'}, {16,(u16)'v'}, 
	{16,(u16)'b'}, {16,(u16)'n'}, {16,(u16)'m'}, {16,(u16)','}, {16,(u16)'.'}, 
	{16,(u16)'/'}, {40,K_SHIFT}, {0,0}};
const nds_kbd_key row4[] = {
	{32,K_CTRL}, {24,K_ALT}, {128,(u16)' '}, {24,K_ALT}, {32,K_CTRL}, {0,0}};
char shifts[] = "`~1!2@3#4$5%6^7&8*9(0)-_=+[{]}\\|;:\'\",<.>/?";
const nds_kbd_key *kbdrows[] = {row0, row1, row2, row3, row4};

static int shift = false, ctrl = false, alt = false, caps = false;

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

u16 kbd_mod_code(u16 ret) {
	if (ret & K_MODIFIER) return ret;
	if (caps && !shift) {
		if (ret >= 'a' && ret <= 'z') ret -= 0x20;
	}
	if (shift) {
		char* temp;
		if (!caps && ret >= 'a' && ret <= 'z') ret -= 0x20;
		if ((temp = strchr(shifts,ret)) != NULL) ret = *(temp + 1);
	}
	if (alt) {
		ret |= 0x80;
	}
	if (ctrl/* && ret >= 'a' && ret < 'a'+32*/) {
		ret = ret & 0x1f;
	}
	return ret;
}

void kbd_set_color_from_pos(u16 r, u16 k,u8 color) {
	u16 ii,xx=0,jj;
	u16* map[] = { (u16*)(BG_MAP_RAM(4)+3*32*2), 
	 (u16*)(BG_MAP_RAM(5)+3*32*2), (u16*)(BG_MAP_RAM(6)+3*32*2),
	 (u16*)(BG_MAP_RAM(7)+3*32*2) };
	for (ii=0;ii<k;ii++) {
		xx += kbdrows[r][ii].width >> 3;
	}
	for (ii=0;ii<(kbdrows[r][k].width>>3);ii++) {
		for (jj=0;jj<4;jj++) {
			map[jj][(10+r*2)*32+ii+xx+1] = (map[jj][(10+r*2)*32+ii+xx+1] & 0x0FFF) | (color << 12);
			map[jj][(10+r*2+1)*32+ii+xx+1] = (map[jj][(10+r*2+1)*32+ii+xx+1] & 0x0FFF) | (color << 12);
		}
	}
}

void kbd_set_color_from_code(u16 code,u8 color) {
	u16 r,k;
	/*if (code == '\x1b') {
		
	}*/
	for (r=0;r<5;r++) {
		for (k=0;kbdrows[r][k].width!=0;k++) {
			if (kbd_mod_code(kbdrows[r][k].code) == code)  {
				kbd_set_color_from_pos(r,k,color);
			}
			// do not break!! there may be >1 key with this code (modifier keys)
		}
	}
}

void kbd_set_map() {
	BG0_CR = BG_TILE_BASE(0) | BG_MAP_BASE(4 + (caps | (shift<<1))) | BG_PRIORITY(0) | BG_16_COLOR;
	
}

// down-left, down, down-right, left, pause, right, up-left, up, up-right

const u8 vi_move[] = { K_DOWN_LEFT, K_DOWN, K_DOWN_RIGHT,
                       K_LEFT, K_NOOP, K_RIGHT,
                       K_UP_LEFT, K_UP, K_UP_RIGHT };

u16 kbd_xy2key(u8 x, u8 y) {
	//draw_char(0,32,x+1);
	//draw_char(1,32,y+1);
	if (x >= 104 && x < 152 && y >=24 && y < 72) {	// on arrow-pad
		u8 kx = (x-104)/16, ky = (y-24)/16;
		//nds_raw_print("ak ");
		return vi_move[kx+(2-ky)*3];
	}
	if (y >=80 && y < 96) {
		
		//nds_raw_print("ek ");
		if (x >= 8 && x < 24) return '\033';
		if (x >= 40 && x < 248) {	// F-key
			x -= 40;
			y = x/72;	// which section
			x -= y*72;	// offset in section
			if (x < 64) {
				return K_F(y*4+(x>>4)+1);	// section*4 + offset/16 + 1
			} else {
				return 0;
			}
		}
			
	}
		//nds_raw_print("mk ");
	s16 ox = x - 8, oy = y-104;
	if (ox < 0 || ox >= 240) return 0;
	if (oy < 0 || oy >= 80) return 0;
		//nds_raw_print("i ");
	u16 row = oy / 16;
	int i;
	for (i=0;ox > 0;ox -= kbdrows[row][i++].width);
	u16 ret = kbdrows[row][i-1].code;
	//draw_char(2,32,ret);
	return kbd_mod_code(ret);
}

void kbd_dotoggle(int* flag, int how) {
	switch (how) {
		case 0: *flag = false; return;
		case 1: *flag = true; return;
		default:
		case -1: *flag = !*flag; return;
	}
}

// which: K_SHIFT, K_CTRL, K_ALT, K_MODIFIER=all keys
// how: -1 = toggle, 0 = off, 1 = on
void kbd_togglemod(int which, int how) {
	//int old_shift = shift, old_ctrl = ctrl, old_alt = alt, old_caps = caps;
	switch (which) {
		case K_CTRL: kbd_dotoggle(&ctrl,how); break;
		case K_SHIFT: kbd_dotoggle(&shift,how); break;
		case K_ALT: kbd_dotoggle(&alt,how); break;
		case K_CAPS: kbd_dotoggle(&caps,how); break;
		case K_MODIFIER:
			kbd_dotoggle(&ctrl,how);
			kbd_dotoggle(&shift,how);
			kbd_dotoggle(&alt,how);
			// NOT caps!!  This is called to un-set shift, ctrl, and alt after
			// a key is pressed.  Unsetting caps here would cause it to be the
			// same as shift.
			break;
	}
	/*if (old_shift != shift) */kbd_set_color_from_code(K_SHIFT,shift);
	/*if (old_ctrl != ctrl) */kbd_set_color_from_code(K_CTRL,ctrl);
	/*if (old_alt != alt) */kbd_set_color_from_code(K_ALT,alt);
	/*if (old_caps != caps) */kbd_set_color_from_code(K_CAPS,caps);
	kbd_set_map();
}


// clear this to prevent alt-b, f5, and f6 from having their special effects
// it's cleared during getlin, yn_function, etc
u8 process_special_keystrokes = 1;

// run this every frame
// returns a key code if one has been typed, else returns 0
// assumes scankeys() was already called this frame (in real vblank handler)
u8 kbd_vblank() {
	
	static u16 last_code;		// the keycode of the last key pressed, so it can be un-hilited
        static u16 held_code;

        if (nds_keysDown() & KEY_UP) {
          return K_UP;
        } else if (nds_keysDown() & KEY_DOWN) {
          return K_DOWN;
        } else if (nds_keysDown() & KEY_LEFT) {
          return K_LEFT;
        } else if (nds_keysDown() & KEY_RIGHT) {
          return K_RIGHT;
        }
	
        last_code = held_code;

        if (nds_keysHeld() & KEY_TOUCH) {
                u16 the_x = IPC->touchXpx;
                u16 the_y = IPC->touchYpx;
		
		// get the keycode that corresponds to this key
		u16 keycode = kbd_xy2key(the_x,the_y);
		
		// if it's not a modifier, hilite it
		if (keycode && !(keycode & K_MODIFIER)) 
                        kbd_set_color_from_code(keycode,1);

		// set last_code so it can be un-hilited later
                held_code = keycode;
        } else {
                held_code = 0;
        }

        if ((last_code != 0) && (held_code != last_code) && ! (last_code & K_MODIFIER)) {
		kbd_set_color_from_code(last_code,0);	// clear the hiliting on this key
  		kbd_togglemod(K_MODIFIER,0);		// and also clear all modifiers (except caps)

		last_code = 0;
        }

	if (nds_keysDownRepeat() & KEY_TOUCH) {
                u16 the_x = IPC->touchXpx;
                u16 the_y = IPC->touchYpx;
		
		// get the keycode that corresponds to this key
		u16 keycode = kbd_xy2key(the_x,the_y);
		
		// if it's a modifier, toggle it
		if (keycode & K_MODIFIER) 
                        kbd_togglemod(keycode,-1);
		else if ((keycode & 0x7F) != 0) {	// it's an actual keystroke, return it
  			return (keycode & 0xFF);
		}
	}
	
	return 0;
}

