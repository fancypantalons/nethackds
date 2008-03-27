#include <malloc.h>
#include <nds.h>
#include <nds/arm9/console.h>
#include <stdio.h>
#include <unistd.h>
#include <fat.h>

#include "ndsx_ledblink.h"

#include <debug_stub.h>
#include <debug_tcp.h>

#include "hack.h"
#include "dlb.h"
#include "ds_kbd.h"
#include "nds_main.h"
#include "nds_win.h"
#include "nds_gfx.h"
#include "nds_util.h"
#include "bmp.h"
#include "wifi.h"
#include "nds_hearse.h"

#ifdef MENU_COLOR
#  include <pcre.h>
#endif

#define SPLASH_IMAGE "nhlogo.bmp"
#define SPLASH_PROMPT "Tap to begin, Adventurer!"

const unsigned char *_pcre_default_tables;

int console_enabled = 0;
int was_console_layer_visible = 0;
int debug_mode = 0;
int power_state = 0;
char *goodbye_msg = NULL;

int have_error = 0;

void mallinfo_dump();

/*
 * Key interrupt handler.  Right now, I only use this to toggle the console
 * layer on and off.
 */
void keysInterruptHandler()
{
  if (! console_enabled) {
    nds_show_console();
    mallinfo_dump();
  } else {
    nds_hide_console();
  }
}

/*
 * Right now, the main thing we do here is check for the lid state, so we
 * can power on/off as appropriate.
 */
void vsyncHandler()
{
  int lid_closed = (((~IPC->buttons)<<6) & KEY_LID ) ^ KEY_LID;

  switch (power_state) {
    case POWER_STATE_ON:
      if (lid_closed) {
        powerOFF(POWER_ALL_2D);
        REG_IPC_FIFO_TX = SET_LEDBLINK_ON;
        power_state = POWER_STATE_TRANSITIONING;
      }
      
      break;

    case POWER_STATE_TRANSITIONING:
      REG_IPC_FIFO_TX = SET_LEDBLINK_SLOW;
      power_state = POWER_STATE_ASLEEP;

      break;

    case POWER_STATE_ASLEEP:
      if (! lid_closed) {
        powerON(POWER_ALL_2D);
        REG_IPC_FIFO_TX = SET_LEDBLINK_OFF;
        power_state = POWER_STATE_ON;
      }

      break;

    default:
      power_state = POWER_STATE_ON;
      break;
  }
}

void nds_show_console()
{
  was_console_layer_visible = DISPLAY_CR & DISPLAY_BG0_ACTIVE;

  SUB_DISPLAY_CR |= DISPLAY_BG0_ACTIVE;

  console_enabled = 1;
}

void nds_hide_console()
{
  if (! was_console_layer_visible) {
    SUB_DISPLAY_CR ^= DISPLAY_BG0_ACTIVE;
  }

  console_enabled = 0;
}

/*
 * Get the power state of the DS.
 */
int nds_power_state()
{
  return power_state;
}

void nds_error()
{
  have_error = 1;
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

  /* Console and Status/Message layers */
  SUB_BG0_CR = BG_MAP_BASE(0) | BG_TILE_BASE(1) | BG_16_COLOR | BG_PRIORITY_0;
  SUB_BG3_CR = BG_BMP8_256x256 | BG_BMP_BASE(4) | BG_PRIORITY_2;

  SUB_BG3_XDX = 1 << 8;
  SUB_BG3_XDY = 0;
  SUB_BG3_YDX = 0;
  SUB_BG3_YDY = 1 << 8;

  /* Init the console */

  consoleInitDefault((u16 *)BG_MAP_RAM_SUB(0),
                     (u16 *)BG_TILE_RAM_SUB(1),
                     16);

  /* Sub screen setup. */

  /* Keyboard */
  BG0_CR = BG_MAP_BASE(4) | BG_TILE_BASE(0) | BG_16_COLOR | BG_PRIORITY_0;

  /* Menu/Text/Command Layer */
  BG2_CR = BG_BMP8_256x256 | BG_BMP_BASE(2) | BG_PRIORITY_2;

  BG2_XDX = 1 << 8;
  BG2_XDY = 0;
  BG2_YDX = 0;
  BG2_YDY = 1 << 8;

  irqInit();
  irqEnable(IRQ_VBLANK | IRQ_KEYS | IRQ_IPC_SYNC);

  irqSet(IRQ_KEYS, keysInterruptHandler);
  REG_KEYCNT |= 0x8000 | 0x4000 | KEY_SELECT | KEY_START;

  irqSet(IRQ_VBLANK, vsyncHandler);

  REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;

  NDSX_SetLedBlink_Off();

  scanKeys();
  
  int pressed = nds_keysDown();

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
  } else {
    wifi_setup();
  }
}

/* 
 * The splash screen code... just display BMP on the screen and wait for a 
 * tap event.
 */
int game_display_cr;
int game_sub_display_cr;

void splash_screen()
{
  bmp_t logo;
  int text_w, text_h;

  bmp_read(SPLASH_IMAGE, &logo);
  nds_draw_bmp(&logo, (u16 *)BG_BMP_RAM_SUB(4), BG_PALETTE_SUB);

  bmp_free(&logo);

  nds_fill((u16 *)BG_BMP_RAM(2), 0);

  text_dims(system_font, SPLASH_PROMPT, &text_w, &text_h);
  nds_draw_text(system_font, SPLASH_PROMPT,
                256 / 2 - text_w / 2,
                192 / 2 - text_h / 2,
                (u16 *)BG_BMP_RAM(2));

  videoSetMode(MODE_5_2D | 
               DISPLAY_BG_EXT_PALETTE | 
               DISPLAY_BG2_ACTIVE);

  videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);

  nds_wait_key(KEY_TOUCH);
}

/*
 * Right now, we do nothing, but the plan is to have this jump back
 * to the main menu.
 */
void nethack_exit()
{
  REG_IPC_FIFO_TX = 0xDEADBEEF;
}

void mallinfo_dump()
{
  struct mallinfo info = mallinfo();

  iprintf("Arena: %d\n", info.arena);
  iprintf("Ordblks: %d\n", info.ordblks);
  iprintf("Uordblks: %d\n", info.uordblks);
  iprintf("Fordblks: %d\n", info.fordblks);
}

void start_game()
{
  int fd;

  DISPLAY_CR = game_display_cr;
  SUB_DISPLAY_CR = game_sub_display_cr;

  nds_fill((u16 *)BG_BMP_RAM_SUB(4), 0);

  if (! *plname) {
    askname();
  }

  set_savefile_name();

  goodbye_msg = NULL;

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
}

void main_menu()
{
  while (1) {
    winid win = create_nhwindow(NHW_MENU);
    ANY_P ids[3];
    menu_item *sel;
    int ret;

    start_menu(win);

    ids[0].a_int = 1;
    ids[1].a_int = 2;
    ids[2].a_int = 3;

    add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Play", 0);
    add_menu(win, NO_GLYPH, &(ids[1]), 0, 0, 0, "Leaderboard", 0);
    add_menu(win, NO_GLYPH, &(ids[2]), 0, 0, 0, "Hearse", 0);

    end_menu(win, NULL);
    
    ret = select_menu(win, PICK_ONE, &sel);

    destroy_nhwindow(win);

    if (ret <= 0) {
      continue;
    }

    switch (sel->item.a_int)
    {
      case 1:
        start_game();
        return;

      case 2:
        break;

      case 3:
        nds_hearse();
        break;

      default:
        break;
    }

    NULLFREE(sel);
  }
}

int main()
{
  srand(IPC->time.rtc.hours * 60 * 60 + IPC->time.rtc.minutes * 60 + IPC->time.rtc.seconds);

  init_screen();

  if (! fatInitDefault())
  {
    iprintf("Unable to initialize FAT driver!\n");
    nds_show_console();

    return 0;
  }

  chdir("/NetHack");

  /* Initialize some nethack constants */

  x_maze_max = COLNO-1;

  if (x_maze_max % 2)
    x_maze_max--;

  y_maze_max = ROWNO-1;

  if (y_maze_max % 2)
    y_maze_max--;

  /* Now get the window system set up */
  choose_windows(DEFAULT_WINDOW_SYS);

  initoptions();

  /* Gotta initialize this before the command list is generated */

  if (debug_mode) {
    iprintf("Enabling debug mode.\n");

    flags.debug = 1;
  }

  init_nhwindows(NULL, NULL);

  game_display_cr = DISPLAY_CR;
  game_sub_display_cr = SUB_DISPLAY_CR;

  if (have_error) {
    nds_show_console();

    return 255;
  }

  /* Get PCRE set up. */

#ifdef MENU_COLOR
  _pcre_default_tables = pcre_maketables();
#endif

  /* Show our splash screen */

  splash_screen();

  /* Gotta init the keyboard here, so we have the right palette in place */

  kbd_init();

  main_menu();

  return 0;
}
