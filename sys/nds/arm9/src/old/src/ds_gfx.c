#include "hack.h"
#include "ds_main.h"
#include "ds_gfx.h"
#include <stdlib.h>
#include "ds_errfont.h"
#include <errno.h>

u8 nds_ascii_graphics = 0;

// change these
#define DEF_TILE_WIDTH		8
#define DEF_TILE_HEIGHT		8
#define DEF_TILE_FILE		"/NetHack/tiles.bin"

// don't change these
#define TILE_WIDTH		iflags.wc_tile_width
#define TILE_HEIGHT		iflags.wc_tile_height
#define TILE_FILE		iflags.wc_tile_file
u16 SCREEN_COLS;
u16 SCREEN_ROWS;

// main screen (96k) image stored at BG_GFX + 0 = 0x06000000
// event queue (1k) stored at BG_GFX + 96k = 0x06018000
// font1 (16k) stored at BG_GFX + 97k = 0x06018400
// font2 (16k) stored at BG_GFX + 113k = 0x0601C400
// tileset (up to 255k) stored at BG_GFX + 129k = 0x06020400
// end of main-screen storage is at 0x06060000
u16* subfont_rgb_bin = (u16*)(0x06018400);
u16* subfont_bgr_bin = (u16*)(0x0601C400);
u16* top_font_bin;
u16* btm_font_bin;
u16* tiles_bin;// = (u16*)0x06020400;

// subpixel font palette - to use: 0x8000 | (value & fgcolor) | (~value & bgcolor)
u16 sfpal[] = {
	RGB15(  0,  0,  0), RGB15( 15,  9,  9), RGB15(  9, 15,  9), RGB15( 15, 15,  9), 
	RGB15(  9,  9, 15), RGB15( 15,  9, 15), RGB15(  9, 15, 15), RGB15( 23, 23, 23), 
	RGB15( 15, 15, 15), RGB15( 31, 15, 15), RGB15( 15, 31, 15), RGB15( 31, 31, 15), 
	RGB15( 15, 15, 31), RGB15( 31, 15, 31), RGB15( 15, 31, 31), RGB15( 31, 31, 31)
};

// if you are calling this function, not much should be happening after
// since it clobbers the font pointers
void nds_fatal_err(const char* msg) {
	static u8 x=2,y=1;
	u8 i = 0;
	//top_font_bin = btm_font_bin = &ds_subfont[0];
//	x = 2;
//	y = 1;
	for (i=0; msg[i] != '\0'; i++) {
		draw_char(x,y,msg[i]);
		x++;
		if (msg[i] == '\n' || x > 80) {
			x = 2;
			y++;
		}
	}
}

void swap_font(bool bottom) {
	if (!bottom) {
		if (top_font_bin == subfont_rgb_bin) top_font_bin = subfont_bgr_bin;
		else top_font_bin = subfont_rgb_bin;
	} else {
		if (btm_font_bin == subfont_rgb_bin) btm_font_bin = subfont_bgr_bin;
		else btm_font_bin = subfont_rgb_bin;
	}
}

bool nds_load_file(const char* name, u16* dest, u32 len) {
	FILE* f = fopen(name,"r");
	if (f == NULL) return false;
	u16 readbuf[1024];
	u32 i,l,wi=0;
	if (len == 0) len = 0xffffffff;	// max possible len
	for (i=0;i<1024;i++) readbuf[i] = 0;
	while ((l=fread(readbuf,2,1024,f)) > 0 && wi*2 < len) {
		for (i=0;i<(l) && wi*2<len;i++) {	// 0 to l/2
			dest[wi++] = readbuf[i];
		}
		for (i=0;i<1024;i++) readbuf[i] = 0;
	}
	fclose(f);
	return true;
}

extern int total_tiles_used;	//in tile.c, used only here

// c1 = rgb order, c2 = bgr order
#define c1(a,i)		(RGB15((a[i]>>3),(a[i+1]>>3),(a[i+2]>>3)))
#define c2(a,i)		(RGB15((a[i+2]>>3),(a[i+1]>>3),(a[i]>>3)))
#define TILE_BUFFER_SIZE		(TILE_WIDTH*TILE_HEIGHT*(total_tiles_used+1)*2)

bool nds_load_tile_bmp(const char* name, u16* dest, u32 len) {
#define h	iflags.wc_tile_height
#define w	iflags.wc_tile_width
// bmpxy2off works ONLY inside nds_load_tile_bmp!
#define bmpxy2off(x,y)	(((y-(y%h))*iw+(y%h))*w + x*h)
	FILE* f = fopen(name,"r");
	u32 writeidx = 0;
	u32 i,j,l;
	s16 y;
	u32 off;
	s32 iw2, ih2;
	u16 iw = 0, ih = 0;
	//s32 ty;
	u16 depth;
	fseek(f, 10, SEEK_SET);
	fread(&off,4,1,f);
	fseek(f, 4, SEEK_CUR);
	fread(&iw2,4,1,f);
	fread(&ih2,4,1,f);
	fseek(f, 2, SEEK_CUR);
	fread(&depth,2,1,f);
	if (depth != 24) {
		fclose(f);
		return false;
	}
	y = ih2 - 1;	// some crazy person decided to store the lines in a .bmp backwards
	ih = ih2 / h;
	iw = iw2 / w;
	
	if (len == 0) len = 0xffffffff;
	
	fseek(f,off,SEEK_SET);

	u8 temp[3];
	while (y >= 0) {
		for (i=0;i<iw;i++) {
			writeidx = bmpxy2off(i*w,y);
			for (j=0;j<w;j++) {
				fread(temp,1,3,f);
				if (writeidx*2 < len) dest[writeidx++] = c2(temp,0);
			}
		}
		// x&3 == x%4
		if (((iw*w*3) & 3) != 0) fseek(f,4-((iw*w*3)&3),SEEK_CUR);
		y--;
	}
//*/

	fclose(f);
	return true;
#undef bmpxy2off
#undef h
#undef w
} 

bool nds_load_tile_file(char* name, u16* dest, u32 len) {
	char ext[4];
	u16 slen = strlen(name);
	strcpy(ext,name+slen-3);
//	nds_raw_print(name+len-3);
	if (strcmpi(ext,"bmp") == 0) {
//	nds_raw_print("=isbmp ");
		return nds_load_tile_bmp(name,dest,len);
	} else {	// assume .bin maybe w/ funny ext
		return nds_load_file(name,dest,len);
	}
}

bool nds_load_tiles() {
//char temp[30];
//sprintf(temp,"%d,%s ",TILE_FILE == NULL, TILE_FILE==NULL ? DEF_TILE_FILE : TILE_FILE);
//nds_raw_print(temp);
	char buf[64];
	s8 died1 = -1, died2 = -1;;
	if (TILE_FILE != NULL) {
		if (TILE_WIDTH == 0) TILE_WIDTH = DEF_TILE_WIDTH;
		if (TILE_HEIGHT == 0) TILE_HEIGHT = DEF_TILE_HEIGHT;
		tiles_bin = (u16*)malloc(TILE_BUFFER_SIZE);
		if (! nds_load_tile_file(TILE_FILE,tiles_bin,TILE_BUFFER_SIZE) ) {
			died1 = errno;
			free(tiles_bin);
		} else {
			goto finish;
		}
	}
	TILE_WIDTH = DEF_TILE_WIDTH;
	TILE_HEIGHT = DEF_TILE_HEIGHT;
	tiles_bin = (u16*)malloc(TILE_BUFFER_SIZE);
	if (! nds_load_tile_file(DEF_TILE_FILE,tiles_bin,TILE_BUFFER_SIZE) ) {
		died2 = errno;
		free(tiles_bin);
	}
	
	if (died1 != -1) {
		sprintf(buf,"Error loading tileset %s (errno=%d)\n",TILE_FILE,died1);
		if (died2 == -1) {
			nds_raw_print(buf);
		} else {
			nds_fatal_err(buf);
		}
	}
	if (died2 != -1) {
		sprintf(buf,"Error loading default tileset %s (errno=%d)\n",DEF_TILE_FILE,died2);
		nds_fatal_err(buf);
		return FALSE;
	}
		
	
finish:
	SCREEN_ROWS = 168 / TILE_HEIGHT;
	SCREEN_COLS = 256 / TILE_WIDTH;
	return TRUE;
//	nds_raw_print("r/c set");
}

bool nds_load_kbd() {
#define NUM_FILES	3
	const char *files[] = {
	//	"subfont_rgb.bin","subfont_bgr.bin",
		"kbd.bin","kbd.pal","kbd.map",
	};
	const u16* dests[] = {
	//	subfont_rgb_bin, subfont_bgr_bin,
		(u16*)BG_TILE_RAM_SUB(0), BG_PALETTE_SUB, (u16*)BG_MAP_RAM_SUB(8),
	};
	
	char buf[64] = "\0";
	u16 i;
	for (i=0;i<NUM_FILES;i++) {
		if (! nds_load_file(files[i],dests[i],0)) {
			sprintf(buf,"Error opening %s (errno=%d)\n",files[i],errno);
			nds_fatal_err(buf);
			return FALSE;
		}
	}
#undef NUM_FILES
	
	return TRUE;
}

void nds_init_fonts() {
	// the font is now compiled in as ds_subfont for error reporting purposes
	// ds_subfont contains the bgr version
	//subfont_bgr_bin = &ds_subfont[0];
	u16 i;
	u16 t,t2;
	for (i=0;i<8*3*256;i++) {
		t = ds_subfont[i];
		t2 = t & 0x8000;
		t2 |= (t & 0x001f)<<10;
		t2 |= (t & 0x03e0);
		t2 |= (t & 0x7c00)>>10;
		subfont_bgr_bin[i] = t;
		subfont_rgb_bin[i] = t2;
	}
	top_font_bin = subfont_rgb_bin;
	btm_font_bin = subfont_bgr_bin;
}
	

// don't try to draw on the subscreen or Bad Things will happen
// y is measured from the top of the map window (y=8)
void draw_tile(u8 x, u8 y, u16 tile) {
	u32 vram_offset = (y & 0x7F)*TILE_HEIGHT*256+x*TILE_WIDTH+8*256, 
	  tile_offset = (tile & 0x7FFF)*TILE_WIDTH*TILE_HEIGHT;
	u16* fb = BG_GFX;
  	u8 xx,yy;
  	for (yy=0;yy<TILE_HEIGHT;yy++)
  	for (xx=0;xx<TILE_WIDTH;xx++) 
	  	fb[yy*256+xx+vram_offset] = tiles_bin[yy*TILE_WIDTH+xx+tile_offset] | BIT(15);
	if (tile & 0x8000) {
		fb[vram_offset+TILE_WIDTH-2] = fb[vram_offset+TILE_WIDTH-1] = 
		 fb[vram_offset+256+TILE_WIDTH-1] = 0x801f;	// put red mark in corner
	}
}

void draw_char(u8 x, u8 y, u8 c) {
	u32 vram_offset = (y & 0x1F)*8*256+x*3, tile_offset = c*24;
	u16* fb = BG_GFX;
	const u16* chardata = top_font_bin;
	if (y&32) {
		fb = &BG_GFX_SUB[16*1024];
		chardata = btm_font_bin;
	}
  	u8 xx,yy;
  	for (yy=0;yy<8;yy++)
  	for (xx=0;xx<3;xx++) 
	  	fb[yy*256+xx+vram_offset] = chardata[yy*3+xx+tile_offset] | BIT(15);
}

void draw_color_char(u8 x, u8 y, u8 c, u8 clr) {
	u32 vram_offset = (y & 0x1F)*8*256+x*3, tile_offset = c*24;
	u16* fb = BG_GFX;
	const u16* chardata = top_font_bin;
	if (y&32) {
		fb = &BG_GFX_SUB[16*1024];
		chardata = btm_font_bin;
	}
	/*u16 colormask = RGB15(0x1f >> (!(clr & C_RED) + !(clr & C_BRIGHT)),
		0x1f >> (!(clr & C_GREEN) + !(clr & C_BRIGHT)),
		0x1f >> (!(clr & C_BLUE) + !(clr & C_BRIGHT)));*/
  	u8 xx,yy;
	u16 val;
	u16 fgc = sfpal[clr&0xF], bgc = sfpal[clr>>4];
  	for (yy=0;yy<8;yy++) {
		for (xx=0;xx<3;xx++) {
			val = (chardata[yy*3+xx+tile_offset]);
			fb[yy*256+xx+vram_offset] = (val&fgc) | (~val&bgc) | 0x8000;
		}
	}
}

void draw_curs(u8 x, u8 y) {
	u32 vram_offset = y*TILE_HEIGHT*256+x*TILE_WIDTH+8*256;
  	u8 xx,yy;
  	for (xx=0;xx<TILE_WIDTH;xx++) {
	  	if ((xx & 1) == 0) BG_GFX[xx+vram_offset] = 0xFFFF;
		if (((xx ^ TILE_HEIGHT) & 1) == 1) BG_GFX[256*(TILE_HEIGHT-1)+xx+vram_offset] = 0xFFFF;
	}
  	for (yy=0;yy<TILE_HEIGHT;yy++) {
	  	if ((yy & 1) == 0) BG_GFX[yy*256+vram_offset] = 0xFFFF;
		if (((yy ^ TILE_WIDTH) & 1) == 1) BG_GFX[yy*256+TILE_WIDTH-1+vram_offset] = 0xFFFF;
	}
}

void draw_win_string(winid win, const char* s) {
	nds_nhwindow* w = &wins[win];
	u16* data = w->data;
	while (*s) {
		data[w->cy*w->cols+w->cx] = (u16)(*s++);
		w->cx++;
		if (w->cx >= w->cols || *s == '\n') {
			w->cx = 0;
			w->cy++;
		}
	}
}

//static u16 map_overlap_tx;

void render_window(winid win) {
	nds_nhwindow* w = &wins[win];
	u16* d = w->data;
	if (win == WIN_MAP) {
		s16 cx = w->center_x, ox = cx-((SCREEN_COLS>>1)-1), cols = w->cols;
		s16 cy = w->center_y, oy = cy-((SCREEN_ROWS>>1)-1), rows = w->rows;
		u16 xx, yy;
		if (ox < 0) ox = 0;
		if (ox > cols - SCREEN_COLS) ox = cols - SCREEN_COLS;
		if (oy < 0) oy = 0;
		if (oy > rows - SCREEN_ROWS) oy = rows - SCREEN_ROWS;
		u16 curx = w->cx, cury = w->cy;
		if (!nds_ascii_graphics) {
			for (yy=0;yy<SCREEN_ROWS;yy++){
				//for (xx=0;xx<map_overlap_tx;xx++) {
				for (xx=0;xx<SCREEN_COLS;xx++) {
					draw_tile(xx, yy, d[(yy+oy)*cols+xx+ox]);
				}
			}
			for (yy=8;yy<8+SCREEN_ROWS*TILE_HEIGHT;yy++) {
				for (xx=SCREEN_COLS*TILE_WIDTH;xx<256;xx++) {
					BG_GFX[yy*256+xx] = 0x8000;
				}
			}
			for (yy=8+SCREEN_ROWS*TILE_HEIGHT;yy<192-16;yy++) {
				for (xx=0;xx<256;xx++) {
					BG_GFX[yy*256+xx] = 0x8000;
				}
			}
			if (curx-ox < SCREEN_COLS) draw_curs(curx-ox,cury-oy);
			
		} else {
			for (yy=0;yy<21;yy++){
				for (xx=0;xx < 80;xx++) {
				//for (xx=0;xx < 80 && xx<map_overlap_tx;xx++) {
					draw_color_char(xx, yy+1, (u8)(d[yy*cols+xx] & 0xFF), (u8)((d[yy*cols+xx]>>8) & 0xFF));
				}
			}
			u8 tempc = (u8)((d[cury*cols+curx]>>8) & 0xFF);
			tempc = ((tempc & 0x0F) << 4) | ((tempc & 0xF0)>>4);	//swap fg & bg colors
			draw_color_char(curx, cury+1, (u8)(d[cury*cols+curx] & 0xFF), tempc);
		}
		/*for (yy=SCREEN_ROWS*TILE_HEIGHT+8;yy<168+8;yy++) {
			for (xx=0;xx<map_overlap_tx*TILE_WIDTH;xx++) {
				BG_GFX[yy*256+xx] = 0;
			}
		}*/
		/*for (yy=0;yy<168;yy++) {
			for (xx=map_overlap_tx*TILE_WIDTH;xx<256;xx++) {
				BG_GFX[yy*256+xx] = 0;
			}
		}*/
	} else if (win == WIN_MESSAGE) {
		u16 xx;
		for (xx=0;xx<w->cols;xx++) {
			draw_char(xx,0,d[xx]);		// win_message is always at 0,0, one
			draw_char(xx,32,d[xx]);		// row high, and drawn on both screens
			
		}
	/*} else if (wins[win].type == NHW_MENU) {
		u16 xx,yy,c=0;
		for (yy=0;yy<w->rows;yy++) {
			for (xx=0;xx<w->cols;xx++) {
				draw_char(xx+w->x,yy+w->y,d[c++]);
			}
		}*/
		
	} else {
		u16 xx,yy,c=0;
		for (yy=0;yy<w->rows;yy++) {
			c = yy*w->cols;	// hack for menu windows; they are always 85ch wide
			for (xx=0;xx<w->cols && xx + w->x < 85;xx++) {
				draw_char(xx+w->x,yy+w->y,d[c++]);
			}
		}
	}
}

void render_all_windows() {
	u16 i;

	//for (i=0;i<85;i++) draw_char(i,32+11,(char)wins[1].data[0]+1);
//	if (!iflags.window_inited) return;
	/*render_window(WIN_MAP);
	render_window(WIN_MESSAGE);
	render_window(WIN_STATUS);*/
	//u16 map_overlap_x = 256;
	/*u16 tw, th, sr, sc;
	if (nds_ascii_graphics) {
		tw = TILE_WIDTH;
		th = TILE_HEIGHT;
		sr = SCREEN_ROWS;
		sc = SCREEN_COLS;
		TILE_WIDTH = 3;
		TILE_HEIGHT = 8;
		SCREEN_ROWS = 21;
		SCREEN_COLS = 80;
	}*/
	for (i=0;i<MAX_WINDOWS;i++) {	// render map after
		if (wins[i].type != 0 && wins[i].vis) {
  		//	if (!(win_updated & (0xFF >> (MAX_WINDOWS-1-i)))) {
     			render_window(i);
		//	}
  			//if (wins[i].type == NHW_TEXT) map_overlap_x = 0;
  			//if (wins[i].type == NHW_MENU) map_overlap_x = wins[i].x * 3;
		}
	}
	/*if (nds_ascii_graphics) {
		TILE_WIDTH = tw;
		TILE_HEIGHT = th;
		SCREEN_ROWS = sr;
		SCREEN_COLS = sc;
	}*/
	//map_overlap_tx = map_overlap_x / (nds_ascii_graphics ? 3 : TILE_WIDTH);
	//if (WIN_MAP != WIN_ERR) render_window(WIN_MAP);
}

/*void draw_string(u8 x, u8 y, const char* s) {
	while (*s) {
		draw_char(x,y,(u8)(*(s++)));
		x+=3;
		if (x > 252) {
			x = 0;
			y += 8;
		}
	}
}

void draw_map_tile(u8 x, u8 y, u16 tile) {
	tile_map[y*78+x-1] = tile;
	draw_tile(x,y+1,tile);
}

void clear_area(u16 x1, u16 y1, u16 x2, u16 y2) {
	u32 vram_offset = y1*256+x1;
	u8 xx,yy, xd = x2-x1, yd = y2-y1;
	for (yy=0;yy<yd;yy++) {
		for (xx=0;xx<xd;xx++) {
			VRAM_A[yy*256+xx+vram_offset] = 0;
		}
	}
}

void undraw_curs(u8 x, u8 y) {
	draw_tile(x,y+1,tile_map[y*78+x-1]);
}
*/

	
