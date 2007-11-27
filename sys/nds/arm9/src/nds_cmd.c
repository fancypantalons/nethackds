#include <nds.h>

#include "hack.h"

#include "nds_win.h"
#include "nds_cmd.h"
#include "nds_gfx.h"
#include "ppm-lite.h"

#define M(c) (0x80 | (c))
#define C(c) (0x1f & (c))

#define COLWIDTH 60

/*
 * Missing commands:
 *
 * conduct
 * ride
 * extended commands
 */

typedef struct {
  char f_char;
  char *name;
  int x1;
  int y1;
  int x2;
  int y2;
} nds_cmd_t;

static nds_cmd_t cmdlist[] = {
	{C('d'), "Kick", 0, 0},
/*
#ifdef WIZARD
	{C('e'), TRUE, wiz_detect},
	{C('f'), TRUE, wiz_map},
	{C('g'), TRUE, wiz_genesis},
	{C('i'), TRUE, wiz_identify},
#endif
#ifdef WIZARD
	{C('o'), TRUE, wiz_where},
#endif
	{C('p'), TRUE, doprev_message},
*/
	{C('t'), "Teleport", 0, 0},
/*
#ifdef WIZARD
	{C('v'), TRUE, wiz_level_tele},
	{C('w'), TRUE, wiz_wish},
#endif
*/
	{C('x'), "Attributes", 0, 0},
	{'a', "Apply", 0, 0},
	{'A', "Armor", 0, 0},
	{M('a'), "Adjust", 0, 0},
	{'c', "Close", 0, 0},
	{'C', "Call", 0, 0},
	{M('c'), "Chat", 0, 0},
	{'D', "Drop", 0, 0},
	{M('d'), "Dip", 0, 0},
	{'e', "Eat", 0, 0},
	{'E', "Engrave", 0, 0},
	{M('e'), "Enhance", 0, 0},
	{'f', "Fire", 0, 0},
	{M('f'), "Force", 0, 0},
	{'i', "Inventory", 0, 0},
	{'I', "Type-Inv", 0, 0},
	{M('i'), "Invoke", 0, 0},
	{M('j'), "Jump", 0, 0},
	{M('l'), "Loot", 0, 0},
	{M('m'), "Monster", 0, 0},
	{M('n'), "Name", 0, 0},
	{'o', "Open", 0, 0},
	{'O', "Set", 0, 0},
	{M('o'), "Sacrifice", 0, 0},
	{'p', "Pay", 0, 0},
	{'P', "Put On", 0, 0},
	{M('p'), "Pray", 0, 0},
	{'q', "Drink", 0, 0},
	{'Q', "Quiver", 0, 0},
	{'r', "Read", 0, 0},
	{'R', "Remove", 0, 0},
	{M('r'), "Rub", 0, 0},
	{'s', "Search", 0, 0},
	{'S', "Save", 0, 0},
	{M('s'), "Sit", 0, 0},
	{'t', "Throw", 0, 0},
	{'T', "Take Off", 0, 0},
	{M('t'), "Turn", 0, 0},
	{M('u'), "Untrap", 0, 0},
	{'v', "Version", 0, 0},
	{'V', "History", 0, 0},
	{'w', "Wield", 0, 0},
	{'W', "Wear", 0, 0},
	{M('w'), "Wipe", 0, 0},
	{'x', "Swap", 0, 0},
	{'X', "Explore", 0, 0},
	{'z', "Zap", 0, 0},
	{'Z', "Cast", 0, 0},
	{'<', "Up", 0, 0},
	{'>', "Down", 0, 0},
//	{'/', "What Is", 0, 0},
	{'&', "What Does", 0, 0},
	{'?', "Help", 0, 0},
	{'.', "Wait", 0, 0},
	{',', "Pickup", 0, 0},
	{':', "Look", 0, 0},
	{';', "What Is", 0, 0},
	{'^', "Id", 0, 0},
	{'\\', "Discoveries", 0, 0},	
	{'@', "Toggle Pickup", 0, 0},
	{M('2'), "Two Weapon", 0, 0},
        /*
	{WEAPON_SYM,  TRUE, doprwep},
	{ARMOR_SYM,  TRUE, doprarm},
	{RING_SYM,  TRUE, doprring},
	{AMULET_SYM, TRUE, dopramulet},
	{TOOL_SYM, TRUE, doprtool},
        */
	{'*', "In Use", 0, 0},
        /*
	{GOLD_SYM, TRUE, doprgold},
	{SPBOOK_SYM, TRUE, dovspell},
        */
        {'#', "Ex-Cmd", 0, 0},
        {0, NULL , 0, 0}
};

u16 *vram = (u16 *)BG_BMP_RAM(12);

/* Here we draw our layer. */

void nds_init_cmd()
{
  int cur_x = 0;
  int cur_y = 0;
  int i;
  struct ppm *img = alloc_ppm(256, 192);

  for (i = 0; (cmdlist[i].f_char != 0) && (cmdlist[i].name != NULL); i++) {
    int text_h;

    text_dims(system_font, cmdlist[i].name, NULL, &text_h);

    if ((cur_y + text_h) > 192) {
      cur_x += COLWIDTH + 1;
      cur_y = 0;
    }

    draw_string(system_font, cmdlist[i].name, img,
                cur_x, cur_y, 1,
                255, 0, 255);

    cmdlist[i].x1 = cur_x;
    cmdlist[i].x2 = cur_x + COLWIDTH;
    cmdlist[i].y1 = cur_y;
    cmdlist[i].y2 = cur_y + text_h;

    cur_y += text_h + 2;
  }

  draw_ppm_bw(img, vram, 0, 0, 256, 254, 255);

  free_ppm(img);
}

nds_cmd_t nds_find_command(int x, int y)
{
  int i;
  nds_cmd_t cmd = {0, NULL};

  for (i = 0; (cmdlist[i].f_char != 0) && (cmdlist[i].name != NULL); i++) {
    if ((x >= cmdlist[i].x1) && (x <= cmdlist[i].x2) &&
        (y >= cmdlist[i].y1) && (y <= cmdlist[i].y2)) {
      cmd = cmdlist[i];

      break;
    }
  }

  return cmd;
}

char nds_do_cmd()
{
  char ret = 0;
  nds_cmd_t curcmd = { 0, NULL };

  touchPosition coords = { .x = 0, .y = 0 };
  touchPosition lastCoords;

  /* Initialize our display */
  DISPLAY_CR |= DISPLAY_BG3_ACTIVE;

  /*
   * Now, we loop until either a command is tapped and selected, or the left
   * button is released.
   */
  while (1) {
    int pressed;

    swiWaitForVBlank();

    lastCoords = coords;
    coords = touchReadXY();

    scanKeys();

    pressed = keysHeld();

    if (! (pressed & KEY_L)) {
      break;
    }

    if ((coords.x != 0) && (coords.y != 0)) {
      nds_cmd_t cmd = nds_find_command(coords.px, coords.py);

      if (cmd.f_char != curcmd.f_char) {
        if (curcmd.f_char != 0) {
          nds_draw_text(system_font, curcmd.name, curcmd.x1, curcmd.y1,
                        254, 255, vram);
        }

        if (cmd.f_char != 0) {
          nds_draw_text(system_font, cmd.name, cmd.x1, cmd.y1, 
                        254, 253, vram);
        }

        curcmd = cmd;
      }
    } else if ((lastCoords.x != 0) && (lastCoords.y != 0)) {
      nds_cmd_t cmd = nds_find_command(lastCoords.px, lastCoords.py);

      if (cmd.f_char != 0) {
        nds_draw_text(system_font, cmd.name, cmd.x1, cmd.y1, 
                      254, 255, vram);

        ret = cmd.f_char;

        break;
      }
    }
  }

  /*
   * This happens if the user releases L while pressing a command.  Basically,
   * we want to remove the item highlight.
   */

  if ((coords.x != 0) && (coords.y != 0)) {
    nds_cmd_t cmd = nds_find_command(coords.px, coords.py);

    if ((cmd.f_char != 0) || (cmd.name != NULL)) {
      nds_draw_text(system_font, cmd.name, cmd.x1, cmd.y1, 
                    254, 255, vram);
    }
  }

  DISPLAY_CR ^= DISPLAY_BG3_ACTIVE;

  return ret;
}

