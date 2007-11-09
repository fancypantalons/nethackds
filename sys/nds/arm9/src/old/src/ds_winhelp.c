#include "hack.h"
//#include "func_tab.h"
#include "ds_main.h"
#include "ds_win.h"

// all helper functions used by ds_win.c

u16 *ebuf = (u16*)(&BG_GFX[256*192]);
		// store the queue just past mainscreen display data
u16 ebuf_read = 0, ebuf_write = 0;
u8 nds_updated = 0;	// windows that have been updated and should be redrawn

bool has_event() {
	return ((ebuf[ebuf_read] & EVENT_SET) || (ebuf_read < ebuf_write));
		// read < write should never happen without EVENT_SET, but
		// just in case...
}

u16 get_event() {
	if (!has_event()) return 0;
	u16 r = ebuf[ebuf_read];
	ebuf[ebuf_read] = 0;
	ebuf_read++;
	if (ebuf_read > ebuf_write) {
		ebuf_write++;
 		if (ebuf_write >= MAX_EBUF) ebuf_write = 0;
	}
	if (ebuf_read >= MAX_EBUF) ebuf_read = 0;
	return r;
}

void put_key_event(u8 c) {
	ebuf[ebuf_write++] = EVENT_SET | (u16)c;
	if (ebuf_write >= MAX_EBUF) ebuf_write = 0;
}

void put_mouse_event(u8 x, u8 y) {
	ebuf[ebuf_write++] = EVENT_SET | MEVENT_FLAG | (u16)x | (((u16)y) << 7);
	if (ebuf_write >= MAX_EBUF) ebuf_write = 0;
}

winid find_unused_window(int dir) {
	u16 i;
	if (dir==0) {
		for (i=3;i<MAX_WINDOWS;i++) {
			if (wins[i].type == 0) return i;
		}
	} else {
		for (i=MAX_WINDOWS-1;i>=3;i--) {
			if (wins[i].type == 0) return i;
		}
	}
	return WIN_ERR;
}

void nds_win_updated(winid win) {
	nds_updated |= (1 << win);
}

// helper func for putstr(WIN_MESSAGE);
void do_more() {
	draw_win_string(WIN_MESSAGE,MORE_MSG);
	nds_win_updated(WIN_MESSAGE);
	char i;
	while ((i=nds_nhgetch()) != '\n' && i != ' ');
	wins[WIN_MESSAGE].wrote_this_turn = 1;
	nds_clear_nhwindow(WIN_MESSAGE);
	wins[WIN_MESSAGE].cx = wins[WIN_MESSAGE].cy = 0;
}
			
		
