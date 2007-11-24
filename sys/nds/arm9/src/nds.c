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

int consoleEnabled = 0;
int wasConsoleLayerVisible = 0;

/*
 * Key interrupt handler.  Right now, I only use this to toggle the console
 * layer on and off.
 */
void keysInterruptHandler()
{
  if (consoleEnabled) {
    BG0_CR = BG_MAP_BASE(4) | BG_TILE_BASE(0) | BG_16_COLOR;

    if (! wasConsoleLayerVisible) {
      DISPLAY_CR ^= DISPLAY_BG0_ACTIVE;
    }

    consoleEnabled = 0;
  } else {
    wasConsoleLayerVisible = DISPLAY_CR & DISPLAY_BG0_ACTIVE;

    BG0_CR = BG_MAP_BASE(12) | BG_TILE_BASE(10) | BG_16_COLOR;
    DISPLAY_CR |= DISPLAY_BG0_ACTIVE;

    consoleEnabled = 1;
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

  videoSetMode(MODE_5_2D | DISPLAY_BG1_ACTIVE | DISPLAY_BG_EXT_PALETTE);
  videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);

  vramSetMainBanks(VRAM_A_MAIN_BG_0x06000000,
                   VRAM_B_MAIN_BG_0x06020000,
                   VRAM_C_SUB_BG_0x06200000,
                   VRAM_D_MAIN_BG_0x06040000);

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

  /* Keyboard layer */
  BG0_CR = BG_MAP_BASE(4) | BG_TILE_BASE(0) | BG_16_COLOR;

  /* Menu/Text and Map layers */
  BG2_CR = BG_BMP8_256x256 | BG_BMP_BASE(2);

  BG2_XDX = 1 << 8;
  BG2_XDY = 0;
  BG2_YDX = 0;
  BG2_YDY = 1 << 8;

  /* Now init our console. */
  /* Set up the palette entries for our text, while we're here. */

  BG_PALETTE_SUB[255] = RGB15(31,31,31);
  BG_PALETTE_SUB[253] = RGB15(31,0, 0);

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

  if ((pressed & KEY_SELECT) && 
      (pressed & KEY_START)) {

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

void test_thinger()
{
  winid win = create_nhwindow(NHW_TEXT);
  char buf[BUFSZ];
  int i;

  for (i = 0; i < 100; i++) {
    sprintf(buf, "Testing line number %d", i);

    putstr(win, ATR_NONE, buf);
  }

  display_nhwindow(win, 1);
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

  /* TODO: In here, we'll put the new/pre-existing game selector, 
     options menu, and so forth. */

  display_gamewindows();

  // test_thinger();

  player_selection();
  newgame();
  set_wear();

  flags.move = 0;

  (void) pickup(1);

  moveloop();

  iprintf("Ready!\n");

  return 0;
}
