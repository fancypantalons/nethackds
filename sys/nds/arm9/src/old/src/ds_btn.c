#include "hack.h"
#include "ds_main.h"
#include "ds_io.h"
#include <unistd.h>

extern u8 process_special_keystrokes;	// in ds_kbd.c

#define NDS_BUTTON_FILE			"buttons.dat"

#define NDS_MAPPABLE_MASK		(KEY_A | KEY_B | KEY_X | KEY_Y | KEY_START | KEY_SELECT)
#define NDS_MODIFIER_MASK		(KEY_L | KEY_R)
#define NDS_BUTTON_MASK			(NDS_MAPPABLE_MASK | NDS_MODIFIER_MASK)
#define NDS_NUM_MAPPABLE		6	// A, B, X, Y, Select, Start
#define NDS_NUM_MODIFIER		2	// R, L
#define NDS_CMD_LENGTH			16	// max. 15 keys / button + null terminator

// [mappable] * 2^[mods] things to map commands to, [cmd_length] chars per command
u8 nds_btn_cmds[NDS_NUM_MAPPABLE << NDS_NUM_MODIFIER][NDS_CMD_LENGTH];

const s16 mappables[] = { KEY_A, KEY_B, KEY_X, KEY_Y, KEY_SELECT, KEY_START };
const s16 modifiers[] = { KEY_L, KEY_R };
s16 nds_buttons_to_btnid(u16 kd, u16 kh) {
	if (!(kd & NDS_MAPPABLE_MASK)) return -1;
	u16 i, mods = 0;
	for (i=0;i<NDS_NUM_MODIFIER;i++) {
		if (kh & modifiers[i]) mods |= (1 << i);
	}
	for (i=0;i<NDS_NUM_MAPPABLE;i++) {
		if (kd & mappables[i]) return i + NDS_NUM_MAPPABLE * (mods);
	}
	return -1;
}
#if 0
void nds_btn_vblank() {
	swiWaitForVBlank();

static u32 ohb, no_pulse = 0;	// detect if the ARM7 has died
static u16 touched = 0;
static s16 xarr[5],yarr[5];
static u16 last_code;

	u16 the_key;
	
	scanKeys();
	u32 kd = keysDown();
	u32 kh = keysHeld();
	if (touched && (kh & KEY_TOUCH)) {
		touched++;
 		xarr[touched-1] = IPC->touchXpx;
 		yarr[touched-1] = IPC->touchYpx;
	} else if (touched) touched = 0;
	if (kd & KEY_TOUCH) {
 		touched++;
 		xarr[touched-1] = IPC->touchXpx;
 		yarr[touched-1] = IPC->touchYpx;
	}
	if (keysUp() & KEY_TOUCH) {
		if (last_code && !(last_code & K_MODIFIER)) kbd_set_color_from_code(last_code,0);
		last_code = 0;
	}
	the_key = 0;
	if (touched == 3) {
		u16 xtop=0, ytop=0, xbtm=0, ybtm=0, i, the_x=0, the_y=0;
		for (i=1;i<3;i++) {
			if (xarr[i] < xarr[xbtm]) xbtm = i;
			if (xarr[i] > xarr[xtop]) xtop = i;
			if (yarr[i] < yarr[ybtm]) ybtm = i;
			if (yarr[i] > yarr[ytop]) ytop = i;
		}
		
		for (i=1;i<3;i++) {
			if (i != xtop && i != xbtm && xarr[i] != -1) the_x = xarr[i];
			if (i != ytop && i != ybtm && yarr[i] != -1) the_y = yarr[i];
		}
		
		u16 keycode = kbd_xy2key(the_x,the_y);
		/*if (keycode == ('c' & 0x1f)) {	// ^C
			nds_draw_text = !nds_draw_text;
			u16 ii;
			for (ii=256*8;ii<256*(24-2)*8;ii++) BG_GFX[i] = 0;
			doredraw();
		}*/
		if (keycode && !(keycode & K_MODIFIER)) kbd_set_color_from_code(keycode,1);
		last_code = keycode;
		
		//the_key = 0;
		if (keycode & K_MODIFIER) kbd_togglemod(keycode,-1);
		// F-keys are not checked by xy2key
		else if ((keycode & 0x7F) != 0) {
  			//put_key_event(keycode & 0xFF);
  			the_key = keycode;
  			kbd_togglemod(K_MODIFIER,0);
		}
		touched = 0;
	}
	
	if (nds_button == -1) {
		if (kd & NDS_BUTTON_MASK) {
			nds_button = nds_buttons_to_btnid(kd, kh);
			if (nds_button != -1) {
				nds_temp_cmd = &nds_btn_cmds[nds_button][0];
				u16 i;
				for (i=0;i<NDS_CMD_LENGTH;i++) {
					nds_temp_cmd[i] = 0;
				}
				nds_clear_nhwindow(WIN_MESSAGE);
				wins[WIN_MESSAGE].cx = 0;
				wins[WIN_MESSAGE].cy = 0;
				draw_win_string(WIN_MESSAGE,"Enter the command, then press any button.");
				//nds_updated=1;
			}
		}
		if (the_key == '\x1b') nds_cmd_pos = 16;
	} else {
		if (kd & (0x0FFF)) {	// any key is pressed
			nds_clear_nhwindow(WIN_MESSAGE);
			nds_temp_cmd[nds_cmd_pos] = 0;
			nds_cmd_pos = 16;
			//char temp[30];
			//sprintf(temp,"map %d=%s! ",nds_button,nds_temp_cmd);
			//nds_raw_print(temp);
		}
		if (the_key != 0 && nds_cmd_pos < 15) {
			nds_temp_cmd[nds_cmd_pos++] = (u8)(the_key & 0xFF);
		}
	}
	
/*	if (ohb == IPC->heartbeat) {
 		no_pulse++;
 		if (no_pulse == 45) {	// no response for 0.75s
 			raw_print("Warning: The ARM7 may be dead. If the keyboard is not responding, this is why.");
		}
	} else if (no_pulse > 0) no_pulse = 0;
	ohb = IPC->heartbeat;*/
	
	//freeprint();
	
	if (nds_updated) {
		render_all_windows();
		nds_updated = 0;
	}
}
#endif
void nds_assign_button() {
	s16 nds_button = -1;	// == button_index * modifier_bitmask (see buttons_to_btnid)
	u16 nds_cmd_pos = 0;	// offset from start of nds_temp_cmd
	u8* nds_temp_cmd = NULL;	// set to &nds_btn_cmds[which][0]
	
	/*u16 old_msg[85];
	bool old_vis;
	u16 old_cx, old_cy;//*/
	u16 i;
	
 	/*// save win_message state
 	for (i=0;i<85;i++) {
		old_msg[i] = wins[WIN_MESSAGE].data[i];
	}
	old_vis = wins[WIN_MESSAGE].vis;
	old_cx = wins[WIN_MESSAGE].cx;
	old_cy = wins[WIN_MESSAGE].cy;
	wins[WIN_MESSAGE].vis = true;
	
	nds_clear_nhwindow(WIN_MESSAGE);
 	wins[WIN_MESSAGE].cx = 0;
	wins[WIN_MESSAGE].cy = 0;
	draw_win_string(WIN_MESSAGE,"Press a button to assign a command to it, or ESC to quit.");

	nds_button = -1;
	nds_cmd_pos = 0;
	nds_temp_cmd = NULL;
	while (nds_cmd_pos != 16) nds_btn_vblank();
	//*/
	
	pline("Press a button to assign a command to it, or ESC to quit.");
	// don't need an opsks here because this will only run if psks==1
	
// ---------------------------------
//  get the button to assign a macro to
	process_special_keystrokes = 0;
	u16 e;
	while (nds_button == -1) {
		do_vblank();
		// if we have an event, check if it's the user hitting esc
		// this if(){} copied from nds_nhgetch's while(){}
		if ((e = get_event()) != 0 && !IS_MEVENT(e) && (EVENT_C(e) & 0x7F) != 0) {
			if ((EVENT_C(e) & 0x7f) == '\033') break;
		}
		if (keysDown() & NDS_BUTTON_MASK) {
			nds_button = nds_buttons_to_btnid(keysDown(), keysHeld());
			if (nds_button != -1) {
				nds_temp_cmd = &nds_btn_cmds[nds_button][0];
				u16 i;
				for (i=0;i<NDS_CMD_LENGTH;i++) {
					nds_temp_cmd[i] = 0;
				}
				pline("Enter the command, then type Alt+B.");
				//nds_updated=1;
			}
		}
	}
	process_special_keystrokes = 1;
	// if we broke the loop because he pressed esc, exit the fn
	if ((EVENT_C(e) & 0x7f) == '\033') return;
	
// ---------------------------------
//  get the macro to assign to the button
	process_special_keystrokes = 0;
	u16 kc = 0;
	while ((kc = nhgetch()) != ('b' | 0x80) && nds_cmd_pos < NDS_CMD_LENGTH-1) {
		if ((kc & 0x7f) != 0) {
			nds_temp_cmd[nds_cmd_pos++] = (u8)(kc & 0xFF);
		}
	}
	kbd_set_color_from_code('b' | 0x80,0);	// clear the hiliting on alt+b
	kbd_togglemod(K_MODIFIER,0);		// and also clear all modifiers (except caps)
	
	process_special_keystrokes = 1;
	
// save command mappings to disk
	FILE* f = fopen(NDS_BUTTON_FILE,"w");
	fwrite(&nds_btn_cmds[0],NDS_CMD_LENGTH,(NDS_NUM_MAPPABLE << NDS_NUM_MODIFIER),f);
	fclose(f);
	
	clear_nhwindow(WIN_MESSAGE);
	
	/*for (i=0;i<85;i++) {
		wins[WIN_MESSAGE].data[i] = old_msg[i];
	}
	wins[WIN_MESSAGE].vis = old_vis;
	wins[WIN_MESSAGE].cx = old_cx;
	wins[WIN_MESSAGE].cy = old_cy;
	
	swiWaitForVBlank();
	render_all_windows();*/
}

void nds_check_buttons(u16 kd, u16 kh) {
	s16 btn = nds_buttons_to_btnid(kd,kh);
	if (btn == -1) return;
	u8* cmd = &nds_btn_cmds[btn][0];
	while (*cmd != 0) {
		put_key_event(*(cmd++));
	}
}
	
void nds_init_buttons() {
	u16 i,j;
	for (i=0;i<(NDS_NUM_MAPPABLE << NDS_NUM_MODIFIER);i++) {
		for (j=0;j<NDS_CMD_LENGTH;j++) {
			nds_btn_cmds[i][j] = 0;
		}
	}
	if (access(NDS_BUTTON_FILE,0444) == -1) return;
	FILE* f = fopen(NDS_BUTTON_FILE,"r");
	fread(&nds_btn_cmds[0],NDS_CMD_LENGTH,(NDS_NUM_MAPPABLE << NDS_NUM_MODIFIER),f);
	fclose(f);
}
