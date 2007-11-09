
//#include <stdio.h>
#include "hack.h"
#include "sp_lev.h"
#include "dlb.h"
//#include "func_tab.h"
#include "ds_main.h"
#include "ds_win.h"
//#include "ds_resetmem.h"
#include <malloc.h>
#include <fcntl.h>
#include <fat.h>
#include <unistd.h>
#include <stdio.h>

//#include "nds/registers_alt.h"

void on_irq();
#ifdef D
#undef D
#endif


//int vram_pos = 0;
//#define D(s) 	BG_GFX[16*1024+(vram_pos+=2)] = RGB15(31,0,0) | BIT(15);

//static u16 *pos = ((u16*)(0x06000000 + 32*256*2 + 5));
//#define D(s)	((*(pos+=1)) = 0xFFFF);
//#define E(s)	((*(pos+=1)) = 0x801F);

//#define REG_VCOUNT        ((vuint16*)0x04000006)

int main() {
/*	u32 i;
	for (i = 0; i < (131072); i++)
	{
		BG_GFX[i] = BG_GFX_SUB[i] = 0;
	} */
	//ClearMemory();
	
top:
	powerON(POWER_ALL_2D | POWER_SWAP_LCDS);
	videoSetMode(MODE_5_2D | DISPLAY_BG2_ACTIVE);
	videoSetModeSub(MODE_5_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG2_ACTIVE);
	vramSetBankA(VRAM_A_MAIN_BG_0x06000000); // BG2, event buf, fonts
	vramSetBankB(VRAM_B_MAIN_BG_0x06020000);	// for storage (tileset)
	vramSetBankC(VRAM_C_SUB_BG_0x06200000);
	vramSetBankD(VRAM_D_MAIN_BG_0x06040000);	// for storage (tileset)
	vramSetBankE(VRAM_E_LCD);	// for storage (WIN_TEXT)
	vramSetBankF(VRAM_F_LCD);	// for storage (WIN_TEXT)
	BG2_CR = BG_BMP16_256x256;
	BG2_XDX = 1<<8;
	BG2_XDY = 0;
	BG2_YDX = 0;
	BG2_YDY = 1<<8;
	BG2_CY = 0;
	BG2_CX = 0;
	SUB_BG0_CR = BG_TILE_BASE(0) | BG_MAP_BASE(8) | BG_PRIORITY(0) | BG_16_COLOR;
	SUB_BG2_CR = BG_BMP16_256x256 | BG_BMP_BASE(2);
	SUB_BG2_XDX = 1<<8;
	SUB_BG2_XDY = 0;
	SUB_BG2_YDX = 0;
	SUB_BG2_YDY = 1<<8;
	SUB_BG2_CY = 0;
	SUB_BG2_CX = 0;

	//u32 iii;

	// Enable the V-blank interrupt
	REG_IME = 0;
	IRQ_HANDLER = on_irq;
	REG_IE = IRQ_VBLANK;
	REG_IF = ~0;
	REG_DISPSTAT = DISP_VBLANK_IRQ;
	REG_IME = 1;
	

	register int fd;

	
	windowprocs.win_raw_print = nds_raw_print;
	windowprocs.win_raw_print_bold = nds_raw_print_bold;
//E("printers set")
	
	nds_init_fonts();
	nds_fatal_err("  \n");
//	nds_fatal_err("asdf4242");
//u32 i;
//for (i=0;i<50;i++) { E(""); swiWaitForVBlank(); }
//return 0;
	
	
	/*nds_raw_print("testing raw_print...");
	draw_char(10,10,(u8)'N');*/
	swiWaitForVBlank();
	swiWaitForVBlank();
	swiWaitForVBlank();
	swiWaitForVBlank();
	swiWaitForVBlank();
	swiWaitForVBlank();
	
//E("printers set")
//if (
	if (!fatInitDefault()) {
		nds_fatal_err("\nError initializing FAT drivers.\n");
		nds_fatal_err("Make sure the game is patched with the correct DLDI.\n");
		nds_fatal_err(" (see http://chishm.drunkencoders.com/DLDI/ for more info).\n");
		nds_fatal_err("\n\nUnable to access filesystem.\nCannot continue.\n");
		return 1;
	}
    //	while (1) E("")

	//swiWaitForVBlank();
//} else while (1) D("")
	swiWaitForVBlank();
	swiWaitForVBlank();
	swiWaitForVBlank();
	swiWaitForVBlank();
	
	chdir("/NetHack");
// for (i=0;i++) { D(""); swiWaitForVBlank(); }
	if (!nds_load_kbd()) {
		nds_fatal_err("\n\nError loading keyboard graphics.\nCannot continue.\n");
		return 1;	// die
	}
	kbd_init();
// for (i=0;i++) { E(""); swiWaitForVBlank(); }
	nds_init_buttons();
	
// for (i=0;i++) { D(""); swiWaitForVBlank(); }
	IPC->mailData = 0x00424242;	// to arm7: everything has init'ed
	while ((IPC->mailData & 0xFFFFFF00) != 0x42424200);	// wait for arm7's reply
	if (IPC->mailData & 0x00000001) {	// it's a DS lite
		swap_font(false);
	} else if (access("/NetHack/swapfont",04) != -1) {
		swap_font(false);
	}
	
// for (i=0;i++) { E(""); swiWaitForVBlank(); }
	
	hname = "NetHack";
	choose_windows(DEFAULT_WINDOW_SYS);
// for (i=0;i++) { D(""); swiWaitForVBlank(); }
	
	//hackdir = "/nethack";
	initoptions();
	if (!nds_load_tiles()) {
		nds_fatal_err("\n\nNo tileset could be loaded.\nCannot continue.\n");
		return 1;
	}
	u.uhp = 1;	/* prevent RIP on early quits */
	u.ux = 0;	/* prevent flush_screen() */
	init_nhwindows(0,0);
	display_gamewindows();	// need this for askname()
	
	nds_curs(WIN_MAP,0,30);		// put cursor offscreen
	nds_clear_nhwindow(WIN_MAP);	// somehow there is garbage in these 
	nds_clear_nhwindow(WIN_MESSAGE);// windows even after create_nhwindow
	nds_clear_nhwindow(WIN_STATUS);	// calls clear_nhwindow
	//process_options(argc, argv);
// for (i=0;i++) { E(""); swiWaitForVBlank(); }
	if (!*plname)
		askname();
	if (!stricmp(plname,"wizard")) 
		wizard = 1;
// for (i=0;i++) { D(""); swiWaitForVBlank(); }
	plnamesuffix();
	set_savefile_name();
//	nds_raw_print(SAVEF);
// for (i=0;i++) { E(""); swiWaitForVBlank(); }
	Strcpy(lock,plname);
	Strcat(lock,"-99");
	regularize(lock);
// for (i=0;i++) { D(""); swiWaitForVBlank(); }
	fd = create_levelfile(0, (char *)0);
	if (fd < 0) {
		raw_print("Cannot create lock file");
	} else {
		hackpid = 1;
		write(fd, (genericptr_t) &hackpid, sizeof(hackpid));
		close(fd);
	}
// for (i=0;i++) { D(""); swiWaitForVBlank(); }

	x_maze_max = COLNO-1;
	if (x_maze_max % 2)
		x_maze_max--;
	y_maze_max = ROWNO-1;
	if (y_maze_max % 2)
		y_maze_max--;
		
// for (i=0;i++) { E(""); swiWaitForVBlank(); }
	vision_init();

	dlb_init();
	
	

// for (i=0;i++) { D(""); swiWaitForVBlank(); }
	if ((fd = restore_saved_game()) >= 0) {

		pline("Restoring save file...");
		//mark_synch();	/* flush output */

		if(!dorecover(fd))
			goto not_recovered;
		check_special_room(FALSE);
		if (discover)
			You("are in non-scoring discovery mode.");

		if (discover || wizard) {
			if(yn("Do you want to keep the save file?") == 'n'){
				(void) delete_savefile();
			}
		}

		flags.move = 0;
	} else {
not_recovered:
	// TODO: add code to load a game
		player_selection();
		//boolean l = flags.legacy;
		//flags.legacy = false;
		newgame();
		//flags.legacy = l;
		if (discover)
			You("are in non-scoring discovery mode.");
	
		flags.move = 0;
		set_wear();
		(void) pickup(1);
		read_engr_at(u.ux,u.uy);
	}
		
	// it's safe to turn on the vblank intr now
	REG_IME = 1;
// for (i=0;i++) { E(""); swiWaitForVBlank(); }
		
	moveloop();
// for (i=0;i++) { D(""); swiWaitForVBlank(); }
goto top;
	return 0;
}

void freeprint() {
	static struct mallinfo mi;
	mi = mallinfo();
	u32 ub = mi.uordblks;
	u32 fb = mi.fordblks;
	//u32 ob = mi.arena;
	s16 i;
	u8 temp;
	for (i=4;i>=0;i--) {
		temp = (fb & 0xF) + '0';	// last hex digit
		if (temp > '9') temp += 7;
		draw_char(i+80,33,temp);
		temp = (ub & 0xF) + '0';	// last hex digit
		if (temp > '9') temp += 7;
		draw_char(i+80,34,temp);
		fb >>= 4;
		ub >>= 4;
	}
}


void do_vblank() {
	swiWaitForVBlank();

// 
//static u32 ohb, no_pulse = 0;	// detect if the ARM7 has died

// ---------------------------
//  Handle the arrow buttons
	scanKeys();
	u32 kd = keysDown();
	u32 kh = keysHeld();
	// order of keys: Right, Left, Up, Down
	// map keys to dirs, depends on order of keys in nds/input.h
	//  and order of directions in ndir & sdir in decl.c
	const s8 k2d[] = {	// indexes into ndir/sdir, 10 = end of string = '\0'
		10, 4, 0, 10, 2, 3, 1, 10, 6, 5, 7	// no working combinations >= 11
	};
	// only do stuff if a key was pressed last frame
	if (kd & (KEY_RIGHT | KEY_LEFT | KEY_UP | KEY_DOWN)) {
		u16 dirs_down = 0;
		if (kh & KEY_LEFT) dirs_down++;
		if (kh & KEY_RIGHT) dirs_down++;
		if (kh & KEY_UP) dirs_down++;
		if (kh & KEY_DOWN) dirs_down++;
		if (dirs_down == 1 && !(kh & (KEY_R | KEY_L))) {
			if (iflags.num_pad) put_key_event(ndir[k2d[(kh >> 4) & 0xF]]);
			else put_key_event(sdir[k2d[(kh >> 4) & 0xF]]);
		} else if (dirs_down == 2 && (kh & (KEY_R | KEY_L))) {
			if (iflags.num_pad) put_key_event(ndir[k2d[(kh >> 4) & 0xF]]);
			else put_key_event(sdir[k2d[(kh >> 4) & 0xF]]);
		}
	}
	
// ---------------------------
//  Check for button macros
	nds_check_buttons(kd, kh);
	
// ---------------------------
//  Check for typing on the touchscreen kbd
	u8 keycode = kbd_vblank();
	if ((keycode & 0x7F) != 0) {	// it's an actual keystroke, return it
  		put_key_event(keycode & 0xFF);
	}

// ---------------------------
//  Print free RAM
	freeprint();
	
// ---------------------------
//  If the screen needs to be redrawn, do so now
	if (nds_updated) {
		render_all_windows();
		nds_updated = 0;
	}
}

// on_irq, do nothing
void on_irq() {
	REG_IME = 0;
	if(REG_IF & IRQ_VBLANK) {
		// Tell the DS we handled the VBLANK interrupt
		VBLANK_INTR_WAIT_FLAGS |= IRQ_VBLANK;
		REG_IF |= IRQ_VBLANK;
	} else {
		// Ignore all other interrupts
		REG_IF = REG_IF;
	}
	REG_IME=1;
}

void nds_debug_print(const char *format, va_list args) {	// __cdecl ?
#ifdef NDS_DEBUG
	char buf[128];	// raw_print will overwrite itself after ~160 chars anyway

	if(!format ) return 0;

	retval = vsprintf(buf, format, args);

	nds_raw_print(buf);
#endif	/* NDS_DEBUG */
}
