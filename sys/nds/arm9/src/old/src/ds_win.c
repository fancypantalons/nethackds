#include "hack.h"
#include "func_tab.h"
#include "ds_main.h"
#include "ds_win.h"

extern u8 process_special_keystrokes;	// from ds_kbd.c

nds_nhwindow wins[MAX_WINDOWS];
//boolean nds_draw_text = false;
// these functions are in the order they go in doc/window.doc

static char prev_line[32][86];
static s16 prev_back=0, prev_write=0;

// LOW-LEVEL ROUTINES:

void nds_raw_print(const char* str) {
	static u16 x=0,y=33;
	while (*str) {
		draw_char(x,y,(u8)(*(str++)));
		x++;
		if (x > 78) {
			x = 0;
			y++;
			if (y > 34) y = 33;
		}
	}
	draw_char(x,y,219);
}

void nds_raw_print_bold(const char* str) {
	raw_print(str);
}

void nds_curs(winid win, int x, int y) {
	wins[win].cx = (u8)x-1;
	wins[win].cy = (u8)y;
	if (wins[win].type == NHW_MAP) {
 		wins[win].center_x=x-1;
 		wins[win].center_y=y;
	}
	nds_win_updated(win);
}

#if 0	// old code for putstr(WIN_MESSAGE,...)
			while (w->cx + len + 1 > LINE_LEN - MORE_LEN) {
				while (*str == ' ') str++;
				strncpy(buf,str,LINE_LEN-MORE_LEN-w->cx);
				buf[LINE_LEN-MORE_LEN-w->cx] = '\0';
				i = (u32)strrchr(buf,' ');
				if (i == 0) {
    				if (!just_did_more) goto do_more;
    				else i = LINE_LEN-MORE_LEN-w->cx-1;
				} else {
    				i -= (u32)buf;
				}
				strncpy(buf,str,i+1);
				buf[i+1]= '\0';
				draw_win_string(win,buf);
				nds_win_updated(win);
				just_did_more = 0;
do_more:
				draw_win_string(win,MORE);
				str+=i;	len -= i;
				nds_win_updated(win);
//				if (*str == ' ') { str++; len--; };
				while ((i=nds_nhgetch()) != '\n' && i != ' ');
				just_did_more = 1;
				wins[win].wrote_this_turn = 1;	// for prev_line
				nds_clear_nhwindow(win);
			}
			draw_win_string(win,str);
			nds_win_updated(win);
			wins[win].wrote_this_turn = 1;
			w->cx++;
#endif // 0

void nds_putstr(winid win, int attr, const char* str) {
	//int len = strlen(str);
	u32 i;
	//u16 just_did_more = 0;;
	//char buf[BUFSZ];
	nds_nhwindow* w = &wins[win];
	char* tok;
	//nds_raw_print("~");
	switch(wins[win].type) {
		case NHW_MESSAGE:
			tok=strtok(str," ");
			while (tok != NULL) {
				if ((w->cx + strlen(tok) + 1) > LINE_LIMIT) do_more();
				while (strlen(tok) > LINE_LIMIT) {
					char c = tok[LINE_LIMIT-1];
					tok[LINE_LIMIT-1] = '\0';
					draw_win_string(win,tok);
					draw_win_string(win," ");
					tok[LINE_LIMIT-1] = c;
					tok += LINE_LIMIT-1;
					do_more();
				}
				draw_win_string(win,tok);
				draw_win_string(win," ");
				nds_win_updated(win);
				tok = strtok(NULL," ");
			}
			break;
		case NHW_STATUS:
			for (i=0;i<85;i++) w->data[i+85*w->cy] = 0;
			draw_win_string(win,str);
			break;
		case NHW_MENU:
			if (strlen(str) + wins[win].x + 4 > 85 && strlen(str) < 85-4) {
		 		//wins[win].cols = strlen(str)+7;
		 		wins[win].x = 85 - strlen(str) - 4;
			}
			/* fall thru */
		case NHW_TEXT:
			//pline("w:%d %7X",strlen(str),wins[win].text);
			while (*str) {
				*(wins[win].text) = (u16)(*str);
    			wins[win].text++;
    			str++;
			}
			*(wins[win].text) = '\n';
   			wins[win].text++;
			*(wins[win].text) = '\0';
			//pline(">%7X ",wins[win].text);
			//pline("hello %08X",(u32)wins[win].text);
			break;
	}
	nds_win_updated(win);
	//nds_raw_print("~");
}

void nds_get_nh_event() {
	// NOOP
}

int nds_nhgetch() {
	u16 e;
	while ((e = get_event()) == 0 || IS_MEVENT(e) || (EVENT_C(e) & 0x7F) == 0) do_vblank();
	return EVENT_C(e);
}

int nds_nh_poskey(int *x, int *y, int *mod) {
	u16 e;
	while ((e = get_event()) == 0 || (!IS_MEVENT(e) && (EVENT_C(e) & 0x7F) == 0)) do_vblank();
	if (IS_MEVENT(e)) {
		*x = EVENT_X(e)+1;
		*y = EVENT_Y(e);
		*mod = CLICK_1;
		return 0;
	}	
	return EVENT_C(e);
}

//HIGH-LEVEL ROUTINES:

void nds_print_glyph(winid win, XCHAR_P x, XCHAR_P y, int glyph) {
	if (win != WIN_MAP) return;
	if (!nds_ascii_graphics) {
 		if (glyph_is_pet(glyph)) {
	 		wins[win].data[y*wins[win].cols+x-1] = glyph2tile[glyph] | 0x8000;
		} else {
	 		wins[win].data[y*wins[win].cols+x-1] = glyph2tile[glyph];
		}
	} else {
		int ch, clr;
  		unsigned int sp;
		mapglyph(glyph,&ch,&clr,&sp,x,y);
		u16 temp = ((u8)ch & 0xFF) | ((clr & 0xF) << (sp & MG_PET ? 12 : 8));
		wins[win].data[y*wins[win].cols+x-1] = temp;
	}
	nds_win_updated(win);
}

char nds_yn_function(const char* query, const char* resp, CHAR_P def) {
	u8 opsks = process_special_keystrokes;
	process_special_keystrokes = 0;
	if (wins[WIN_MESSAGE].wrote_this_turn == 1) {
		u8 i;
		draw_win_string(WIN_MESSAGE," --More--");
		while ((i=nds_nhgetch()) != '\n' && i != ' ');
		nds_clear_nhwindow(WIN_MESSAGE);
	}
	register char q;
	boolean digit_ok, allow_num;
	char prompt[QBUFSZ];
	u16 *d = &(wins[WIN_MESSAGE].data[0]), *cx = &(wins[WIN_MESSAGE].cx);
	// taken from tty_yn_function in win/tty/topl.c
	if (resp) {
		char *rb, respbuf[QBUFSZ];

		allow_num = (index(resp, '#') != 0);
		Strcpy(respbuf, resp);
    	/* any acceptable responses that follow <esc> aren't displayed */
		if ((rb = index(respbuf, '\033')) != 0) *rb = '\0';
		Sprintf(prompt, "%s [%s] ", query, respbuf);
		if (def) Sprintf(eos(prompt), "(%c) ", def);
		pline("%s", prompt);
		//(*cx)--;	// putstr adds a trailing space // no it doesn't
		// pline will call win_updated
		
		//swiWaitForVBlank();					// let it draw ques, then
		//wins[WIN_MESSAGE].vis = false;		// suppress normal drawing
		//wins[WIN_MAP].vis = false;	// might go over onto the map if input is long
	} else {
		pline("%s ", query);
		(*cx)--;	// putstr adds a trailing space
		q = nds_nhgetch() & 0xFF;
		d[(*cx)++] = q;
		(*cx)++;
		wins[WIN_MESSAGE].wrote_this_turn = 1;
		nds_win_updated(WIN_MESSAGE);
		goto clean_up;	// don't want to exit with p_s_ks cleared
		//return q;
	}
	do {	/* loop until we get valid input */
	    q = lowc((char)nds_nhgetch());
	    digit_ok = allow_num && digit(q);
	    if (q == '\033') {
			if (index(resp, 'q'))
		    	q = 'q';
			else if (index(resp, 'n'))
			    q = 'n';
			else
		    	q = def;
			break;
	    } else if (index(quitchars, q)) {
			q = def;
			break;
	    }
	    if (!index(resp, q) && !digit_ok) {
			q = (char)0;
	    } else if (q == '#' || digit_ok) {
			char z;
			int n_len = 0;
			long value = 0;
			//draw_char((*cx)++,0,'#');
			//n_len++;
			//digit_string[1] = '\0';
			if (q != '#') {
				draw_char((*cx)++,0,q);
				n_len++;
			    q = '#';
			}
			do {	/* loop until we get a non-digit */
			    z = lowc((char)nds_nhgetch());
			    if (digit(z)) {
					value = (10 * value) + (z - '0');
					if (value < 0) break;	/* overflow: try again */
					d[(*cx)++]=z;
					n_len++;
			    } else if (z == 'y' || index(quitchars, z)) {
					if (z == '\033')  value = -1;	/* abort */
					z = '\n';	/* break */
			    } else if (z == '\b') {
					if (n_len <= 1) { value = -1;  break; }
					else { 
     					value /= 10;  
						d[--(*cx)]=z;
          				n_len--;
          			}
			    } else {
					value = -1;	/* abort */
					break;
			    }
				nds_win_updated(WIN_MESSAGE);
			} while (z != '\n');
			if (value > 0) yn_number = value;
			else if (value == 0) q = 'n';		/* 0 => "no" */
			else {	/* remove number from top line, then try again */
				while (n_len) {
					d[--(*cx)]=z;
    				n_len--;
				}
				nds_win_updated(WIN_MESSAGE);
				q = '\0';
			}
	    }
	} while(!q);

	if (q != '#') {
		d[(*cx)++]=q;
	}
	nds_win_updated(WIN_MESSAGE);
	wins[WIN_MESSAGE].wrote_this_turn = 1;
	wins[WIN_MESSAGE].vis = true;
	wins[WIN_MESSAGE].cx++;

clean_up:
	process_special_keystrokes = opsks;
	return q;
}
	
void nds_getlin(const char* ques, char* input) {
	nds_getlin2(ques,input,BUFSZ);
}
	
void nds_getlin2(const char* ques, char* input, int maxlen) {
	u8 opsks = process_special_keystrokes;
	process_special_keystrokes = 0;
	if (wins[WIN_MESSAGE].wrote_this_turn == 1) {
		u8 i;
		draw_win_string(WIN_MESSAGE," --More--");
		while ((i=nds_nhgetch()) != '\n' && i != ' ');
		nds_clear_nhwindow(WIN_MESSAGE);
	}
	draw_win_string(WIN_MESSAGE,ques);
	nds_win_updated(WIN_MESSAGE);
	do_vblank();						// let it draw ques, then
	wins[WIN_MESSAGE].vis = false;		// suppress normal drawing
	wins[WIN_MAP].vis = false;	// might go over onto the map if input is long
	char c;
	int len = 0;
	wins[WIN_MESSAGE].cx++;
	if (wins[WIN_MESSAGE].cx >= 85) {
  				wins[WIN_MESSAGE].cy++;
  				wins[WIN_MESSAGE].cx=0;
	}	
	while ((c = (char)(nds_nhgetch() & 0x7F)) != '\n' && c != '\033') {
		if (c != '\b' && len < maxlen-1) {
  			input[len++] = c;
			draw_char(wins[WIN_MESSAGE].cx,wins[WIN_MESSAGE].cy,c);
			draw_char(wins[WIN_MESSAGE].cx,32+wins[WIN_MESSAGE].cy,c);
			wins[WIN_MESSAGE].cx++;
			if (wins[WIN_MESSAGE].cx >= 85) {
		  				wins[WIN_MESSAGE].cy++;
		  				wins[WIN_MESSAGE].cx=0;
			}	
		} else if (c == '\b' && len > 0) {
  			input[--len] = '\0';
  			if (wins[WIN_MESSAGE].cx > 0) --wins[WIN_MESSAGE].cx;
  			else {
  				--wins[WIN_MESSAGE].cy;
  				wins[WIN_MESSAGE].cx=84;
			}
			draw_char(wins[WIN_MESSAGE].cx,wins[WIN_MESSAGE].cy,' ');
			draw_char(wins[WIN_MESSAGE].cx,32+wins[WIN_MESSAGE].cy,' ');
		}
	}
	if (c == '\033') {
		input[0] = c;
		len = 1;
	}
	input[len] = '\0';
	wins[WIN_MESSAGE].vis = true;
	wins[WIN_MESSAGE].wrote_this_turn = 1;
	wins[WIN_MAP].vis = true;
	nds_clear_nhwindow(WIN_MESSAGE);	// calls win_updated
	process_special_keystrokes = opsks;
}

int nds_get_ext_cmd() {
	
	int i = -1;
	char cmd[BUFSZ];
	nds_getlin("#",cmd);

	// ripped from win/win32/mswproc.c:
    for (i = 0; extcmdlist[i].ef_txt != (char *)0; i++)
        if (!strcmpi(cmd, extcmdlist[i].ef_txt)) break;

    if (extcmdlist[i].ef_txt == (char *)0) {
        pline("%s: unknown extended command.", cmd);
        i = -1;
    }
    
    return i;
}

void nds_player_selection() {
	anything any;
	int one_choice = 0, i, n;
	char acc;
	menu_item* sel;
	winid win;
	boolean used_r = false;
	// hopefully this will keep it from always returning Arc-Dwa-Fem-Law
	for (i=0;i<IPC->time.rtc.seconds;i++) rand();
	win = nds_create_nhwindow(NHW_MENU);
	if (flags.initrole < 0) {
		nds_start_menu(win);
		for (i=0;roles[i].name.m != 0;i++) {
			any.a_int = i+1;
			acc = roles[i].name.m[0]+0x20;	// m[0] is always uppercase
			if (acc == 'r') {
   				if (!used_r) used_r = true;
   				else acc -= 0x20;	// make 2nd 'r' capital
			}
			nds_add_menu(win,NO_GLYPH,&any,acc,0,0,roles[i].name.m,false);
		}
		any.a_int = randrole()+1;
		nds_add_menu(win,NO_GLYPH,&any,'*',0,0,"Random",false);
		any.a_int = -1;
		nds_add_menu(win,NO_GLYPH,&any,'q',0,0,"Quit",false);
		nds_end_menu(win,"Select a role");
		n = nds_select_menu(win,PICK_ONE,&sel);
		nds_free_menu(win);
		if (n == 0) flags.initrole = randrole();
		else if (n == -1 || sel->item.a_int == -1) {
			nds_destroy_nhwindow(win);
  			nds_exit_nhwindows("Goodbye.");
		} else {
			flags.initrole = sel->item.a_int - 1;
			free(sel);
		}
	}
	//pline("%d: %s ",flags.initrole,roles[flags.initrole].name.m);
	if (flags.initrace < 0) {
		nds_start_menu(win);
		one_choice = 0;
		for (i=0;races[i].noun != 0;i++) {
			if (!validrace(flags.initrole,i)) continue;
			if (one_choice == 0) one_choice = i+1;
			else if (one_choice != -1) one_choice = -1;
			any.a_int = i+1;
			acc = races[i].noun[0];
			nds_add_menu(win,NO_GLYPH,&any,acc,0,0,races[i].noun,false);
		}
		if (one_choice > 0) flags.initrace = one_choice - 1;
		else {
			any.a_int = randrace(flags.initrole) + 1;
			nds_add_menu(win,NO_GLYPH,&any,'*',0,0,"Random",false);
			any.a_int = -1;
			nds_add_menu(win,NO_GLYPH,&any,'q',0,0,"Quit",false);
			nds_end_menu(win,"Select a race");
			n = nds_select_menu(win,PICK_ONE,&sel);
			nds_free_menu(win);
//			char buf[BUFSZ];
			//sprintf(buf,"picked %d %d",n,(n == 1 ? sel->item.a_int : -127));
//			nds_raw_print(buf);
			
			if (n == 0) flags.initrace = randrace(flags.initrole);
			else if (n == -1 || sel->item.a_int == -1) {
				nds_destroy_nhwindow(win);
  				nds_exit_nhwindows("Goodbye.");
			} else {
   				flags.initrace = sel->item.a_int - 1;
   				free(sel);
			}
		}
	}
	//pline("%d: %s ",flags.initrace,races[flags.initrace].noun);
	if (flags.initgend < 0) {
		nds_start_menu(win);
		one_choice = 0;
		for (i=0;genders[i].adj != 0;i++) {
			if (!validgend(flags.initrole,flags.initrace,i)) continue;
			if (one_choice == 0) one_choice = i+1;
			else if (one_choice != -1) one_choice = -1;
			any.a_int = i+1;
			acc = genders[i].adj[0];
			nds_add_menu(win,NO_GLYPH,&any,acc,0,0,genders[i].adj,false);
		}
		if (one_choice > 0) flags.initgend = one_choice - 1;
		else {
			any.a_int = randgend(flags.initrole,flags.initrace) + 1;
			nds_add_menu(win,NO_GLYPH,&any,'*',0,0,"Random",false);
			any.a_int = -1;
			nds_add_menu(win,NO_GLYPH,&any,'q',0,0,"Quit",false);
			nds_end_menu(win,"Select a gender");
			n = nds_select_menu(win,PICK_ONE,&sel);
			nds_free_menu(win);
			if (n == 0) flags.initgend = randgend(flags.initrole,flags.initrace);
			else if (n == -1 || sel->item.a_int == -1) {
				nds_destroy_nhwindow(win);
  				nds_exit_nhwindows("Goodbye.");
			} else {
   				flags.initgend = sel->item.a_int - 1;
   				free(sel);
			}
		}
	}
	//pline("%d: %s ",flags.initgend,genders[flags.initgend].adj);
	if (flags.initalign < 0) {
		nds_start_menu(win);
		one_choice = 0;
		for (i=0;aligns[i].noun != 0;i++) {
			if (!validalign(flags.initrole,flags.initrace,i)) continue;
			if (one_choice == 0) one_choice = i+1;
			else if (one_choice != -1) one_choice = -1;
			any.a_int = i+1;
			acc = aligns[i].adj[0];
			nds_add_menu(win,NO_GLYPH,&any,acc,0,0,aligns[i].adj,false);
		}
		if (one_choice > 0) flags.initalign = one_choice - 1;
		else {
			any.a_int = randalign(flags.initrole,flags.initrace) + 1;
			nds_add_menu(win,NO_GLYPH,&any,'*',0,0,"Random",false);
			any.a_int = -1;
			nds_add_menu(win,NO_GLYPH,&any,'q',0,0,"Quit",false);
			nds_end_menu(win,"Select an alignment");
			n = nds_select_menu(win,PICK_ONE,&sel);
			nds_free_menu(win);
			if (n == 0) flags.initalign = randalign(flags.initrole,flags.initrace);
			else if (n == -1 || sel->item.a_int == -1) {
				nds_destroy_nhwindow(win);
  				nds_exit_nhwindows("Goodbye.");
			} else {
   				flags.initalign = sel->item.a_int - 1;
   				free(sel);
			}
		}
	}
	//pline("%d: %s ",flags.initalign,races[flags.initalign].adj);
}

void nds_display_file(const char* name, BOOLEAN_P complain) {
	FILE* f = fopen(name,"r");
	if (!f) {
		if (complain) {
			pline("Could not open file %s!",name);
		}
		return;
	}
	winid win = nds_create_nhwindow(NHW_TEXT);
	u8 readbuf[1024];
	u32 i,l,c=0;
	for (i=0;i<1024;i++) readbuf[i] = 0;
	while ((l=fread(readbuf,1,1000,f)) > 0) {
		// go back and write over the \n appended to the last chunk
		if (c++ > 0) wins[win].text--;	
		//pline("r:%d ",l);
		//readbuf[l] = '\0';
		nds_putstr(win,0,readbuf);
		for (i=0;i<1024;i++) readbuf[i] = 0;
	}
	fclose(f);
	nds_display_nhwindow(win,TRUE);
	nds_destroy_nhwindow(win);
}

void nds_update_inventory() {
	// NOOP
}

int nds_doprev_message() {
	// TODO
//	char buf[20];
	s16 read = prev_write - prev_back - 1, tback = prev_back;
//	sprintf(buf,"p:%d %d %d",read,prev_back,tback);
//	nds_raw_print(buf);
	
	if (read < 0) read += 32;
	wins[WIN_MESSAGE].wrote_this_turn = 2;
	nds_clear_nhwindow(WIN_MESSAGE);	// this clears prev_back & w_t_t
	wins[WIN_MESSAGE].cx = 0;
	draw_win_string(WIN_MESSAGE,prev_line[read]);
	nds_win_updated(WIN_MESSAGE);
	prev_back = tback + 1;
//	sprintf(buf,"p:%d %d %d",read,prev_back,tback);
//	nds_raw_print(buf);
	// set w_t_t = 2 so when nethack calls clear_nhwin it doesnt kill prev_back
	wins[WIN_MESSAGE].wrote_this_turn = 2;
	return 0;
}

// WINDOW UTILITY ROUTINES

void nds_init_nhwindows(int* argcp, char** argv) {
	// TODO
	iflags.window_inited = true;
}

void nds_exit_nhwindows(const char* str) {
	pline(str);
}

winid nds_create_nhwindow(int type) {
	winid ret = WIN_ERR;
	u16 i;
	switch(type) {
		case NHW_MAP:
			wins[0].type = type;
			wins[0].rows = 21;
			wins[0].cols = 79;
			wins[0].x = 0;
			wins[0].y = 1;
			wins[0].vis = true;
			wins[0].data = (u16*)alloc(2*wins[0].rows * wins[0].cols);
			ret = 0;
			break;
		case NHW_MESSAGE:
			wins[1].type = type;
			wins[1].rows = 1;
			wins[1].cols = 85;
			wins[1].x = 0;
			wins[1].y = 0;
			wins[1].vis = true;
			wins[1].data = (u16*)alloc(2*wins[1].rows * wins[1].cols);
			ret = 1;
			break;
		case NHW_STATUS:
			wins[2].type = type;
			wins[2].rows = 2;
			wins[2].cols = 85;
			wins[2].x = 0;
			wins[2].y = 22;
			wins[2].vis = true;
			wins[2].data = (u16*)alloc(2*wins[2].rows * wins[2].cols);
			ret = 2;
			break;
		case NHW_MENU:
			ret = find_unused_window(0);
			if (ret == WIN_ERR) return ret;
			wins[ret].type = type;
			wins[ret].rows = 21;
			wins[ret].cols = 85;
			wins[ret].x = 45;
			wins[ret].y = 1;
			wins[ret].cx = 1;
			wins[ret].cy = 2;
			wins[ret].vis = false;
			wins[ret].data = (u16*)alloc(2*wins[ret].rows * 85);
			// malloc the amount for the max size to avoid having to realloc
										// draw a nice border for the window
			wins[ret].data[0] = 201;	// upper-left corner, double-line
			wins[ret].data[20*85] = 200;	// bottom-left corner, double-line
			for (i=1;i<85;i++) {
				wins[ret].data[i] = 205;	// horizontal double-line
				wins[ret].data[i+85*20] = 205;
			}
			for (i=1;i<20;i++) {
				wins[ret].data[i*85] = 186;	// vertical double-line
			}
			wins[ret].text = WIN_TEXT(ret);
			break;
		case NHW_TEXT:
			ret = find_unused_window(1);
				// get last unused window so this one will be drawn on top
			if (ret == WIN_ERR) return ret;
			wins[ret].type = type;
			wins[ret].rows = 21;
			wins[ret].cols = 85;	// cover the whole screen
			wins[ret].x = 0;
			wins[ret].y = 1;
			wins[ret].vis = false;
			wins[ret].data = (u16*)alloc(2*wins[ret].rows * wins[ret].cols);
										// draw a nice border for the window
			wins[ret].data[0] = 201;	// upper-left corner, double-line
			wins[ret].data[20*85] = 200;	// bottom-left corner, double-line
			wins[ret].data[84] = 187;	// upper-right corner, double-line
			wins[ret].data[20*85+84] = 188;	// bottom-right corner, double-line
			for (i=1;i<84;i++) {
				wins[ret].data[i] = 205;	// horizontal double-line
				wins[ret].data[i+85*20] = 205;
			}
			for (i=1;i<20;i++) {
				wins[ret].data[i*85] = 186;	// vertical double-line
				wins[ret].data[i*85+84] = 186;
			}
			wins[ret].text = WIN_TEXT(ret);
			break;
	}
	if (type == NHW_TEXT || type == NHW_MENU) {
		for (i=0;i<2048;i++) wins[ret].text[i] = 0;
	}
	wins[ret].m.f = NULL;
	if (ret != WIN_ERR) nds_clear_nhwindow(ret);
	nds_win_updated(ret);
	return ret;
}
			
void nds_clear_nhwindow(winid win) {
	int x=0,y=0;
	nds_win_updated(win);	// must be first, all cases return instead of break
	switch (wins[win].type) {
		case NHW_MAP:
/*			write_val = total_tiles_used;
/*/			for (x=0;x<79;x++)
			for (y=0;y<21;y++)
				wins[win].data[y*79+x] = nds_ascii_graphics ? 0x0000 : total_tiles_used;//*/
				
		//	nds_raw_print("cleared WIN_MAP");
			return;
		case NHW_MESSAGE:
			if (wins[win].wrote_this_turn == 1) {	// written by putstr
				for (x=0;x<85;x++) prev_line[prev_write][x] = wins[win].data[x];
				prev_write++;
				if (prev_write >= 32) prev_write = 0;
			}
			if (wins[win].wrote_this_turn != 2)	prev_back = 0;
				// w_t_t == 2 means it was written by doprev_message
			wins[win].wrote_this_turn = 0;
			wins[win].cx = 0;
			for (x=0;x<85;x++)
				wins[win].data[x] = 0;
			return;
		case NHW_STATUS:
			for (x=0;x<85*2;x++)
				wins[win].data[x] = 0;
			return;
		case NHW_TEXT:
			for (y=1;y<20;y++)
				for (x=1;x<84;x++)
					wins[win].data[y*85+x] = 0;
			return;
		case NHW_MENU:
			for (y=1;y<20;y++)
				for (x=1;x<85;x++)
					wins[win].data[y*85+x] = 0;
			return;
	}
		
/*	for (;yy<h;yy++)
		for (;xx<w;xx++)
			wins[win].data[yy*real_w+xx] = 0;*/
}

void nds_display_nhwindow(winid win, BOOLEAN_P block) {
	u8 opsks; 	// old process_special_keystrokes
	if (wins[win].type != NHW_MENU && wins[win].type != NHW_TEXT) return;
	nds_clear_nhwindow(win);
//	u16 x,y;
/*	for (y=1;y<20;y++)
		for (x=1;x<85;x++)
			wins[win].data[y*85+x] = 0;//*/
	char buf[BUFSZ];
	u16* text = WIN_TEXT(win);
	u16 bcnt = 0;
	u8 ch;
	wins[win].vis = true;	/* Okay, weird... if this is w->vis it gives
							 * "level 0 pid" errors but not if its like this */
	nds_nhwindow* w = &wins[win];
	w->cx = 2;
	w->cy = 1;
	while (*text) {
		if (*text == '\n' || bcnt > 85-3) {
			buf[bcnt] = '\0';
			draw_win_string(win,buf);
			w->cx = 2;
			w->cy++;
			bcnt = 0;
		} else {
			if (w->cy >= 20) {
				w->cx = (wins[win].type == NHW_MENU ? 5 : 38);
				w->cy = 20;
				draw_win_string(win,"--More--");
				opsks = process_special_keystrokes;
				process_special_keystrokes = 0;
				while ((ch = nds_nhgetch()) != ' ' && ch != '\n');
				process_special_keystrokes = opsks;
				w->cy = 1;
				w->cx = 2;
				nds_clear_nhwindow(win);
			}
			buf[bcnt++] = (char)*text;
		}
		text++;
	}
	w->cx = (wins[win].type == NHW_MENU ? 5 : 38);
	w->cy = 20;
	draw_win_string(win,"--More--");
	opsks = process_special_keystrokes;
	process_special_keystrokes = 0;
	while ((ch = nds_nhgetch()) != ' ' && ch != '\n');
	process_special_keystrokes = opsks;
}

void nds_free_menu(winid win) {

	nds_nhmenuitem* mi = wins[win].m.f;
	nds_nhmenuitem* t;
	while (mi != NULL) {
		t = mi->n;
		//free(mi->title);
		free(mi);
		mi = t;
	}
	wins[win].m.f = NULL;
}

void nds_destroy_nhwindow(winid win) {
	wins[win].type = 0;
	wins[win].vis = false;
	if (wins[win].data != NULL) free(wins[win].data);
	if (wins[win].m.f != NULL) {
		nds_free_menu(win);
	}
	nds_win_updated(win);
}

void nds_start_menu(winid win) {
	wins[win].m.f = NULL;
 	wins[win].m.l = (nds_nhmenuitem*)NULL;	//(nds_nhmenuitem*)malloc(sizeof(nds_nhmenuitem));
	wins[win].m.width = 40 - 1 - 2 - 4;
	nds_clear_nhwindow(win);
	// 40 cols, 1 is border, 1 margin on each side, 4 for "a - " before item
//	wins[win].m.next_accel = 'a';
}

void nds_add_menu(winid win, int glyph, const ANY_P* id,
				CHAR_P accel, CHAR_P gacc,
				int attr, const char *str, BOOLEAN_P presel) {
	/*char buf[BUFSZ];
	sprintf(buf,"%8X ",id.a_void);
	if (accel == 0) raw_print(buf);*/
	//if (id.a_void == 0) nds_raw_print(str);
	nds_nhmenuitem* ni = (nds_nhmenuitem*)malloc(sizeof(nds_nhmenuitem));
	nds_nhmenu* m = &wins[win].m;
	ni->acc = accel;
	ni->gacc = gacc;
	//ni->title = malloc(strlen(str) + 1);
	strcpy(ni->title,str);
	if (strlen(str) + wins[win].x + 7 > 85 && strlen(str) < 85-7) {
 		//wins[win].cols = strlen(str)+7;
 		wins[win].x = 85 - strlen(str) - 7;
	}
	ni->mi.item.a_void = id->a_void;	// set all bits the same
	ni->mi.count = (presel ? -1 : 0);
	ni->n = NULL;
	
	if (m->f == NULL) m->f = m->l = ni;
	else {
		m->l->n = ni;
		m->l = ni;
	}
}

void nds_end_menu(winid win, const char* prompt) {
	if (prompt == NULL) return;
	nds_nhmenu* m = &wins[win].m;
	if (strlen(prompt) > m->width && strlen(prompt) < 85-7) m->width = strlen(prompt)+7;
	// add "Prompt" and a blank line to the top of the menu
	nds_nhmenuitem* ni = (nds_nhmenuitem*)malloc(sizeof(nds_nhmenuitem));
	nds_nhmenuitem* ni2 = (nds_nhmenuitem*)malloc(sizeof(nds_nhmenuitem));
	ni->acc = ni2->acc = 0;
	ni->gacc = ni2->gacc = 0;
	strcpy(ni->title,prompt);
	ni2->title[0] = '\0';
	ni->mi.item.a_void = ni2->mi.item.a_void = 0;
	ni->mi.count = ni2->mi.count = 0;
	ni->n = ni2;
	ni2->n = m->f;
	m->f = ni;
}

int nds_select_menu(winid win, int how, menu_item** sel) {
	u8 opsks = process_special_keystrokes;
	process_special_keystrokes = 0;
	// before drawing the menu, force a map update
	render_all_windows();
	wins[win].vis = true;
	u16 i, next_accel, ch;
	nds_nhmenuitem* mi = wins[win].m.f;
	nds_nhmenuitem* tms;	// first one on this page of the menu
	char accel_buf[4];	// "a -\0"
	nds_nhwindow* w = &wins[win];
	u16 x,y;
	//char accels[20];
	//u16 accel_num;
	s16 num_sel = -1;
	while (mi != NULL) {
		for (y=1;y<20;y++)
			for (x=1;x<85;x++)
				wins[win].data[y*85+x] = 0;
		next_accel = 'a';
		tms = mi;
		for (i=0;i<19 && mi != NULL;i++, mi = mi->n) {
			w->cx = 2;
			w->cy = i+1;
			if (mi->mi.item.a_void != 0) {
				if (mi->acc == 0) mi->acc = next_accel++;
				sprintf(accel_buf,"%c %c",mi->acc,
    				mi->mi.count != 0 ? '+' : '-');
				draw_win_string(win,accel_buf);
				w->cx++;
			}
			draw_win_string(win,mi->title);
		}
		nds_win_updated(win);
		while ((ch = nds_nhgetch()) != ' ' && ch != '\n') {
			if (ch == '\033') {
   				num_sel = -1;
   				*sel = NULL;
   				//nds_raw_print("bye");
   				goto menu_done;
			}
			if (how == PICK_NONE) continue;
			for (i=0, mi=tms; i<19 && mi!=NULL; i++, mi=mi->n) {
				if (mi->mi.item.a_void == 0) continue;
				if (mi->acc != ch && mi->gacc != ch && ch != ',') continue;
				w->cx = 2;
				w->cy = i+1;
				if (mi->mi.count != 0) mi->mi.count = 0;
				else mi->mi.count = -1;
				if (how == PICK_ONE) {
					*sel = malloc(sizeof(menu_item));
					memcpy(*sel,&(mi->mi),sizeof(menu_item));
					num_sel = 1;
					goto menu_done;
				}
				sprintf(accel_buf,"%c %c",mi->acc,
					mi->mi.count != 0 ? '+' : '-');
				draw_win_string(win,accel_buf);
			}
			nds_win_updated(win);
		}
		
		//while ((i = nds_nhgetch()) != ' ') swiWaitForVBlank();
		//nds_clear_nhwindow(win);
	}
	if (how == PICK_ANY) {
		u16 sel_counter=0;
		num_sel = 0;
		// loop once to count selected items...
		for (mi = wins[win].m.f;mi != NULL; mi=mi->n) {
			if (mi->mi.count != 0) num_sel++;
		}
		// allocate memory for *sel...
		*sel = malloc(sizeof(menu_item)*num_sel);
		// and loop again to fill *sel
		for (mi = wins[win].m.f;mi != NULL; mi=mi->n) {
			if (mi->mi.count != 0) 
   				memcpy(&((*sel)[sel_counter++]),
			  	  &mi->mi,sizeof(menu_item));
		}
	}
menu_done:
	if (win == WIN_INVEN) {
		nds_free_menu(WIN_INVEN);
	}
	wins[win].vis = false;
	nds_win_updated(win);
	process_special_keystrokes = opsks;
	return num_sel;
}

// MISC ROUTINES

void nds_make_sound() {
	// NOOP
}

void nds_nhbell() {
	// NOOP
}

void nds_mark_synch() {
	// NOOP
}

void nds_wait_synch() {
	// NOOP
}

void nds_delay_output() {
	swiWaitForVBlank();
	swiWaitForVBlank();
	swiWaitForVBlank();
}

void nds_askname() {
	// TODO
	
	nds_getlin2("Who are you?",plname,PL_NSIZ);
	if (*plname == '\033') strcpy(plname,"NetHacker");
}

void nds_cliparound(int x, int y) {
	wins[WIN_MAP].center_x = x-1;
	wins[WIN_MAP].center_y = y;
}

void nds_number_pad(int state) {
	// NOOP
}

void nds_suspend_nhwindows(const char* str) {
	// NOOP
}

void nds_resume_nhwindows() {
	// NOOP
}

void nds_start_screen() {
	// NOOP
}

void nds_end_screen() {
	// NOOP
}

void nds_outrip(winid win, int how) {
	// TODO
}

void nds_preference_update(const char* prefs) {
	// TODO
}

void nethack_exit(int code) {
	u16 i;
	for (i=0;i<60*5;i++) {
		nds_updated = 0xFF;
		do_vblank();	// wait 5 sec.
	}
	IPC->mailData = 0xDEADC0DE;	// tell arm7 to shut down the DS
}

struct window_procs nds_procs = {
    "NDS",
    /*WC_COLOR|WC_HILITE_PET|WC_ALIGN_MESSAGE|WC_ALIGN_STATUS|
	WC_INVERSE|WC_SCROLL_AMOUNT|WC_SCROLL_MARGIN|WC_MAP_MODE|
	WC_FONT_MESSAGE|WC_FONT_STATUS|WC_FONT_MENU|WC_FONT_TEXT|WC_FONT_MAP|
	WC_FONTSIZ_MESSAGE|WC_FONTSIZ_STATUS|WC_FONTSIZ_MENU|WC_FONTSIZ_TEXT|
	WC_TILE_WIDTH|WC_TILE_HEIGHT|WC_TILE_FILE|WC_VARY_MSGCOUNT|
	WC_WINDOWCOLORS|WC_PLAYER_SELECTION|WC_SPLASH_SCREEN|WC_POPUP_DIALOG,*/
	0L,
    0L,
    nds_init_nhwindows,		// TODO
    nds_player_selection,	// TODO
    nds_askname,			// TODO
    nds_get_nh_event,
    nds_exit_nhwindows,		// TODO
    nds_suspend_nhwindows,	// TODO
    nds_resume_nhwindows,	// TODO
    nds_create_nhwindow,	// TODO
    nds_clear_nhwindow,		// TODO
    nds_display_nhwindow,	// TODO
    nds_destroy_nhwindow,	// TODO
    nds_curs,
    nds_putstr,
    nds_display_file,		// TODO
    nds_start_menu,			// TODO
    nds_add_menu,			// TODO
    nds_end_menu,			// TODO
    nds_select_menu,		// TODO
    genl_message_menu,		/* no need for X-specific handling */
    nds_update_inventory,
    nds_mark_synch,			// TODO
    nds_wait_synch,			// TODO
#ifdef CLIPPING
    nds_cliparound,			// TODO
#endif
#ifdef POSITIONBAR
    donull,
#endif
    nds_print_glyph,
    nds_raw_print,
    nds_raw_print_bold,
    donull, //nds_nhgetch,
    nds_nh_poskey,
    nds_nhbell,				// TODO
    nds_doprev_message,		// currently a noop
    nds_yn_function,
    nds_getlin,				// TODO
    nds_get_ext_cmd,
    nds_number_pad,			// TODO
    nds_delay_output,		// TODO
#ifdef CHANGE_COLOR	/* only a Mac option currently */
	donull,
	donull,
#endif
    /* other defs that really should go away (they're tty specific) */
    nds_start_screen,		// TODO
    nds_end_screen,			// TODO
    nds_outrip,				// TODO
    nds_preference_update,	// TODO
};

