#include <malloc.h>
#include <nds.h>
#include <nds/arm9/console.h>
#include <stdio.h>
#include <fat.h>

#ifdef _DEBUG_
#  include <debug_stub.h>
#  include <debug_tcp.h>
#endif

#include "hack.h"
#include "dlb.h"
#include "ds_kbd.h"
#include "nds_win.h"
#include "nds_gfx.h"

int console_enabled = 0;
int was_console_layer_visible = 0;
int debug_mode = 0;

/*
 * Key interrupt handler.  Right now, I only use this to toggle the console
 * layer on and off.
 */
void keysInterruptHandler()
{
  if (console_enabled) {
    BG0_CR = BG_MAP_BASE(4) | BG_TILE_BASE(0) | BG_16_COLOR;

    if (! was_console_layer_visible) {
      DISPLAY_CR ^= DISPLAY_BG0_ACTIVE;
    }

    console_enabled = 0;
  } else {
    was_console_layer_visible = DISPLAY_CR & DISPLAY_BG0_ACTIVE;

    BG0_CR = BG_MAP_BASE(12) | BG_TILE_BASE(10) | BG_16_COLOR;
    DISPLAY_CR |= DISPLAY_BG0_ACTIVE;

    console_enabled = 1;

    mallinfo_dump();
  }
}

/*
 * Here we'll power on the screen, initialize the memory bases, and
 * get our console set up.
 *
 * The game status, messages, and so forth use BG2.
 * The debugging console uses BG0.
 *
 * On the sub screen, BG0 displays the map, and BG2 is used for menus
 * and other non-map, transient entities.
 *
 * Our palettes are set up such that the internal main and sub palettes
 * are in the first 256 entries.  This is used for things like the
 * internal keyboard, menus, and so forth.  The palettes for tiles and
 * so forth are placed in the latter half of palette memory.
 */
void init_screen()
{
  powerON(POWER_ALL_2D | POWER_SWAP_LCDS);
  lcdMainOnBottom();

  videoSetMode(MODE_5_2D | 
               DISPLAY_BG1_ACTIVE | 
               DISPLAY_BG_EXT_PALETTE | 
               DISPLAY_SPR_ACTIVE | 
               DISPLAY_SPR_1D_LAYOUT);

  videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);

  vramSetMainBanks(VRAM_A_MAIN_BG_0x06000000,
                   VRAM_B_MAIN_SPRITE_0x06400000,
                   VRAM_C_SUB_BG_0x06200000,
                   VRAM_D_MAIN_BG_0x06020000);

  /*
   * Set up the main screen.  We're using BG0 for the console, BG2
   * for input prompts and so forth, and BG3 for the status display.
   * The priorities are juggled around so BG0 is on the bottom, with
   * BG2 and BG3 being on top.
   *
   * On the sub display, we use BG2 for menus, text, and so forth,
   * and BG3 for the playfield.
   */

  /* Main screen setup. */

  /* Prompt and Status/Message layers */
  SUB_BG2_CR = BG_BMP8_256x256 | BG_BMP_BASE(0) | BG_PRIORITY_1;
  SUB_BG3_CR = BG_BMP8_256x256 | BG_BMP_BASE(4) | BG_PRIORITY_2;

  SUB_BG2_XDX = 1 << 8;
  SUB_BG2_XDY = 0;
  SUB_BG2_YDX = 0;
  SUB_BG2_YDY = 1 << 8;

  SUB_BG3_XDX = 1 << 8;
  SUB_BG3_XDY = 0;
  SUB_BG3_YDX = 0;
  SUB_BG3_YDY = 1 << 8;

  /* Sub screen setup. */

  /* Keyboard / Console Layer */
  BG0_CR = BG_MAP_BASE(4) | BG_TILE_BASE(0) | BG_16_COLOR | BG_PRIORITY_0;

  /* Map Layer (colour depth is selected in nds_init_map */
  BG1_CR = BG_32x32 | BG_MAP_BASE(8) | BG_TILE_BASE(6) | BG_PRIORITY_3;

  BLEND_CR = BLEND_ALPHA | BLEND_SRC_SPRITE | BLEND_DST_BG1;
  BLEND_AB = 0x0010;

  /* Menu/Text Layer */
  BG2_CR = BG_BMP8_256x256 | BG_BMP_BASE(2) | BG_PRIORITY_2;

  BG2_XDX = 1 << 8;
  BG2_XDY = 0;
  BG2_YDX = 0;
  BG2_YDY = 1 << 8;

  /* Command Layer */

  BG3_CR = BG_BMP8_256x256 | BG_BMP_BASE(12) | BG_PRIORITY_1;

  BG3_XDX = 1 << 8;
  BG3_XDY = 0;
  BG3_YDX = 0;
  BG3_YDY = 1 << 8;

  /* Now init our console. */
  /* Set up the palette entries for our text, while we're here. */

  consoleInitDefault((u16 *)BG_MAP_RAM(12),
                     (u16 *)BG_TILE_RAM(10),
                     16);

  irqInit();
  irqEnable(IRQ_VBLANK | IRQ_KEYS);
  irqSet(IRQ_KEYS, keysInterruptHandler);
  REG_KEYCNT |= 0x8000 | 0x4000 | KEY_L | KEY_R;

#ifdef _DEBUG_
  scanKeys();
  
  int pressed = keysDown();

  if (pressed & KEY_START) {
    debug_mode = 1;
  }

  if (pressed & KEY_SELECT) {
    struct tcp_debug_comms_init_data init_data = {
      .port = 30000
    };

    iprintf("Preparing to initialize debugger...\n");

    if (! init_debug(&tcpCommsIf_debug, &init_data)) {
      iprintf("Failed to initialize debugger stub...\n");
    } else {
      debugHalt();
    }
  }
#endif
}

/*
 * Right now, we do nothing, but the plan is to have this jump back
 * to the main menu.
 */
void nethack_exit()
{
}

void mallinfo_dump()
{
  struct mallinfo info = mallinfo();

  iprintf("Done\n");

  iprintf("Arena: %d\n", info.arena);
  iprintf("Ordblks: %d\n", info.ordblks);
  iprintf("Uordblks: %d\n", info.uordblks);
  iprintf("Fordblks: %d\n", info.fordblks);
}

int main()
{
  int fd;

  srand(IPC->time.rtc.hours * 60 * 60 + IPC->time.rtc.minutes * 60 + IPC->time.rtc.seconds);

  init_screen();

  if (! fatInitDefault())
  {
    iprintf("Unable to initialize FAT driver!\n");

    return 0;
  }

  chdir("/NetHack");

  kbd_init();
  initoptions();

  /* Gotta initialize this before the command list is generated */

  if (debug_mode) {
    iprintf("Enabling debug mode.\n");

    flags.debug = 1;
  }

  /* Initialize some nethack constants */

  x_maze_max = COLNO-1;

  if (x_maze_max % 2)
    x_maze_max--;

  y_maze_max = ROWNO-1;

  if (y_maze_max % 2)
    y_maze_max--;

  /* Now get the window system set up */

  choose_windows(DEFAULT_WINDOW_SYS);
  init_nhwindows(NULL, NULL);

  fd = create_levelfile(0, (char *)NULL);

  if (fd < 0) {
    iprintf("Cannot create lock file");
  } else {
    hackpid = 1;
    write(fd, (genericptr_t) &hackpid, sizeof(hackpid));
    close(fd);
  }

  vision_init();
  dlb_init();

  /* TODO: Display the copyright thinger and title screen. */

  display_gamewindows();

  /* Now restore or start a new game */

  set_savefile_name();

  if (((fd = restore_saved_game()) >= 0) && dorecover(fd)) {
    check_special_room(FALSE);
  } else {
    player_selection();
    newgame();
    set_wear();

    (void) pickup(1);
  }

  flags.move = 0;

  moveloop();

  return 0;
}
