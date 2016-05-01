#include <malloc.h>
#include <nds.h>
#include <nds/arm9/console.h>
#include <stdio.h>
#include <unistd.h>
#include <fat.h>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>

#include "ndsx_ledblink.h"

#include <dswifi9.h>
#include <debug_stub.h>
#include <debug_tcp.h>

#include "hack.h"
#include "dlb.h"
#include "nds_debug.h"
#include "nds_kbd.h"
#include "nds_main.h"
#include "nds_win.h"
#include "nds_gfx.h"
#include "nds_util.h"
#include "bmp.h"

#include <pcre.h>

#define SPLASH_IMAGE "nhlogo.bmp"
#define SPLASH_PROMPT "Tap to begin, Adventurer!"

const unsigned char *_pcre_default_tables;

int console_enabled = 0;
int was_console_layer_visible = 0;
int debug_mode = 0;
int lid_closed = 0;
int power_state = 0;
int wifi_connected = 0;
char *goodbye_msg = NULL;

int have_error = 0;

int debugger_triggered = 0;

void mallinfo_dump();

/*
 * Key interrupt handler.  Right now, I only use this to toggle the console
 * layer on and off.
 */
void keysInterruptHandler()
{
  lid_closed = keysCurrent() & KEY_LID;

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
  switch (power_state) {
    case POWER_STATE_ON:
      if (lid_closed) {
        systemSleep();
        power_state = POWER_STATE_ASLEEP;
      }
      
      break;

    case POWER_STATE_ASLEEP:
      if (! lid_closed) {
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
  was_console_layer_visible = REG_DISPCNT & DISPLAY_BG0_ACTIVE;

  REG_DISPCNT_SUB |= DISPLAY_BG0_ACTIVE;

  console_enabled = 1;
}

void nds_hide_console()
{
  if (! was_console_layer_visible) {
    REG_DISPCNT_SUB ^= DISPLAY_BG0_ACTIVE;
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

void enable_interrupts()
{
  irqEnable(IRQ_KEYS);

  irqSet(IRQ_VBLANK, vsyncHandler);
  irqSet(IRQ_KEYS, keysInterruptHandler);

  REG_KEYCNT |= 0x8000 | 0x4000 | KEY_SELECT | KEY_START | KEY_LID;
}

void wifi_disconnect()
{
  Wifi_DisconnectAP();
}

void nds_break_into_debugger()
{
  struct in_addr ip, gateway, mask, dns1, dns2;

  struct tcp_debug_comms_init_data init_data = {
    .port = 30000
  };

  if (! Wifi_InitDefault(WFC_CONNECT)) {
    DEBUG_PRINT("Failed to connect!");
  } else {
    ip = Wifi_GetIPInfo(&gateway, &mask, &dns1, &dns2);

    DEBUG_PRINT("ip     : %s\n", inet_ntoa(ip) );
    DEBUG_PRINT("gateway: %s\n", inet_ntoa(gateway) );
    DEBUG_PRINT("mask   : %s\n", inet_ntoa(mask) );
    DEBUG_PRINT("dns1   : %s\n", inet_ntoa(dns1) );
    DEBUG_PRINT("dns2   : %s\n", inet_ntoa(dns2) );
  }

  if (init_debug(&tcpCommsIf_debug, &init_data)) {
    debugHalt();
  }
}

void check_debug()
{
  scanKeys();
  
  int pressed = nds_keysDown();

  if (pressed & KEY_START) {
    debug_mode = 1;
  }

  if (pressed & KEY_SELECT) {
    nds_break_into_debugger();
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

  /* Init the console */

  consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 0, 1, false, true);

  REG_DISPCNT_SUB ^= DISPLAY_BG0_ACTIVE;

  /* Main screen setup. */

  /* Console and Status/Message layers */
  REG_BG0CNT_SUB = BG_MAP_BASE(0) | BG_TILE_BASE(1) | BG_COLOR_16 | BG_PRIORITY_0;
  REG_BG3CNT_SUB = BG_BMP8_256x256 | BG_BMP_BASE(4) | BG_PRIORITY_2;

  REG_BG3PA_SUB = 1 << 8;
  REG_BG3PB_SUB = 0;
  REG_BG3PC_SUB = 0;
  REG_BG3PD_SUB = 1 << 8;

  /* Sub screen setup. */

  /* Keyboard */
  REG_BG0CNT = BG_MAP_BASE(4) | BG_TILE_BASE(0) | BG_COLOR_16 | BG_PRIORITY_0;

  /* Menu/Text/Command Layer */
  REG_BG2CNT = BG_BMP8_256x256 | BG_BMP_BASE(2) | BG_PRIORITY_1;

  REG_BG2PA = 1 << 8;
  REG_BG2PB = 0;
  REG_BG2PC = 0;
  REG_BG2PD = 1 << 8;
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
  powerOn(BIT(6));
}

void mallinfo_dump()
{
  struct mallinfo info = mallinfo();

  DEBUG_PRINT("Arena: %d\n", info.arena);
  DEBUG_PRINT("Ordblks: %d\n", info.ordblks);
  DEBUG_PRINT("Uordblks: %d\n", info.uordblks);
  DEBUG_PRINT("Fordblks: %d\n", info.fordblks);
}

void start_game()
{
  boolean resuming = FALSE; /* assume new game */
  int fd;

  REG_DISPCNT = game_display_cr;
  REG_DISPCNT_SUB = game_sub_display_cr;

  nds_fill((u16 *)BG_BMP_RAM_SUB(4), 0);

  if (! *plname) {
    askname();
  }

  set_savefile_name(TRUE);

  goodbye_msg = NULL;

  fd = create_levelfile(0, (char *)NULL);

  if (fd < 0) {
    DEBUG_PRINT("Cannot create lock file");
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
    resuming = TRUE;
    check_special_room(FALSE);
  } else {
    player_selection();
    newgame();

    (void) pickup(1);
  }

  moveloop(resuming);
}

boolean authorize_wizard_mode()
{
  return TRUE;
}

int main()
{
  srand(time(NULL));

  enable_interrupts();

  init_screen();
  check_debug();

  if (! fatInitDefault())
  {
    DEBUG_PRINT("Unable to initialize FAT driver!\n");
    nds_show_console();

    return 0;
  }

  DIR *tmp = opendir("/");
  struct dirent *ent;

  if (! tmp)
  {
    DEBUG_PRINT("Unable to open root directory!\n");
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
    DEBUG_PRINT("Enabling debug mode.\n");

    flags.debug = 1;
  }

  init_nhwindows(NULL, NULL);

  game_display_cr = REG_DISPCNT;
  game_sub_display_cr = REG_DISPCNT_SUB;

  if (have_error) {
    nds_show_console();

    return 255;
  }

  /* Get PCRE set up. */

  _pcre_default_tables = pcre_maketables();

  /* Show our splash screen */

  splash_screen();

  /* Gotta init the keyboard here, so we have the right palette in place */

  kbd_init();

  start_game();

  return 0;
}
