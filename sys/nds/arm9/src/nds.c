#include <malloc.h>
#include <nds.h>
#include <nds/arm9/console.h>
#include <stdio.h>
#include <fat.h>

#include "hack.h"
#include "ds_kbd.h"
#include "nds_win.h"
#include "nds_gfx.h"

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

  videoSetMode(MODE_5_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE);
  videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);

  vramSetMainBanks(VRAM_A_MAIN_BG_0x06000000,
                   VRAM_B_MAIN_BG_0x06020000,
                   VRAM_C_SUB_BG_0x06200000,
                   VRAM_D_MAIN_BG_0x06040000);

  vramSetBankE(VRAM_E_LCD);
  vramSetBankF(VRAM_F_LCD);

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

  /* Console layer */
  BG0_CR = BG_MAP_BASE(8) | BG_TILE_BASE(0) | BG_PRIORITY_3;

  /* Prompt and Status/Message layers */
  BG2_CR = BG_BMP8_256x256 | BG_BMP_BASE(2) | BG_PRIORITY_0;
  BG3_CR = BG_BMP8_256x256 | BG_BMP_BASE(7) | BG_PRIORITY_1;

  BG2_XDX = 1 << 8;
  BG2_XDY = 0;
  BG2_YDX = 0;
  BG2_YDY = 1 << 8;

  BG3_XDX = 1 << 8;
  BG3_XDY = 0;
  BG3_YDX = 0;
  BG3_YDY = 1 << 8;

  /* Sub screen setup. */

  /* Keyboard layer */
  SUB_BG0_CR = BG_MAP_BASE(8) | BG_TILE_BASE(0) | BG_16_COLOR;

  /* Menu/Text and Map layers */
  SUB_BG2_CR = BG_BMP8_256x256 | BG_BMP_BASE(2);
  SUB_BG3_CR = BG_BMP8_256x256 | BG_BMP_BASE(7);

  SUB_BG2_XDX = 1 << 8;
  SUB_BG2_XDY = 0;
  SUB_BG2_YDX = 0;
  SUB_BG2_YDY = 1 << 8;

  SUB_BG3_XDX = 1 << 8;
  SUB_BG3_XDY = 0;
  SUB_BG3_YDX = 0;
  SUB_BG2_YDY = 1 << 8;

  /* Now init our console. */
  /* Set up the palette entries for our text, while we're here. */

  BG_PALETTE[255] = RGB15(31,31,31);

  consoleInitDefault((u16 *)SCREEN_BASE_BLOCK(8),
                     (u16 *)CHAR_BASE_BLOCK(0),
                     16);

  irqInit();
  irqEnable(IRQ_VBLANK);
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
  init_screen();

  if (! fatInitDefault())
  {
    iprintf("Unable to initialize FAT driver!\n");

    return 0;
  }

  kbd_init();
  initoptions();

  choose_windows(DEFAULT_WINDOW_SYS);

  init_nhwindows(NULL, NULL);

  /* TODO: Display the copyright thinger and title screen. */

  /* TODO: In here, we'll put the new/pre-existing game selector, 
     options menu, and so forth. */

  display_gamewindows();

  {
    winid win = create_nhwindow(NHW_MENU);
    menu_item *sel[3];
    ANY_P id1, id2, id3;

    id1.a_int = 1;
    id2.a_int = 2;
    id3.a_int = 3;

    start_menu(win);
    add_menu(win, NO_GLYPH, &id1, 0, 0, 0, "Item 1", 0);
    add_menu(win, NO_GLYPH, &id2, 0, 0, 0, "Item 2", 0);
    add_menu(win, NO_GLYPH, &id3, 0, 0, 0, "Item 3", 0);
    end_menu(win, "Select An Item");

    select_menu(win, PICK_ANY, sel);
  }

  iprintf("Ready!\n");

  return 0;
}
