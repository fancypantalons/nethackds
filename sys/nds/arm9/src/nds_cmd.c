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
	{C('d'), "Kick"},
//	{C('p'), TRUE, doprev_message},
	{C('t'), "Teleport"},
	{C('x'), "Attributes"},
	{'a', "Apply"},
	{'A', "Armor"},
	{M('a'), "Adjust"},
	{'c', "Close"},
	{'C', "Call"},
	{M('c'), "Chat"},
	{'D', "Drop"},
	{M('d'), "Dip"},
	{'e', "Eat"},
	{'E', "Engrave"},
	{M('e'), "Enhance"},
	{'f', "Fire"},
	{M('f'), "Force"},
	{'i', "Inventory"},
	{'I', "Type-Inv"},
	{M('i'), "Invoke"},
	{M('j'), "Jump"},
	{M('l'), "Loot"},
	{M('m'), "Monster"},
	{M('n'), "Name"},
	{'o', "Open"},
	{'O', "Set"},
	{M('o'), "Sacrifice"},
	{'p', "Pay"},
	{'P', "Put On"},
	{M('p'), "Pray"},
	{'q', "Drink"},
	{'Q', "Quiver"},
	{'r', "Read"},
	{'R', "Remove"},
	{M('r'), "Rub"},
	{'s', "Search"},
	{'S', "Save"},
	{M('s'), "Sit"},
	{'t', "Throw"},
	{'T', "Take Off"},
	{M('t'), "Turn"},
	{M('u'), "Untrap"},
	{'v', "Version"},
	{'V', "History"},
	{'w', "Wield"},
	{'W', "Wear"},
	{M('w'), "Wipe"},
	{'x', "Swap"},
	{'X', "Explore"},
	{'z', "Zap"},
	{'Z', "Cast"},
	{'<', "Up"},
	{'>', "Down"},
//	{'/', "What Is"},
	{'&', "What Does"},
	{'?', "Help"},
	{'.', "Wait"},
	{',', "Pickup"},
	{':', "Look"},
	{';', "What Is"},
	{'^', "Id"},
	{'\\', "Discoveries"},	
	{'@', "Toggle Pickup"},
	{M('2'), "Two Weapon"},
        /*
	{WEAPON_SYM,  TRUE, doprwep},
	{ARMOR_SYM,  TRUE, doprarm},
	{RING_SYM,  TRUE, doprring},
	{AMULET_SYM, TRUE, dopramulet},
	{TOOL_SYM, TRUE, doprtool},
        */
	{'*', "In Use"},
        /*
	{GOLD_SYM, TRUE, doprgold},
	{SPBOOK_SYM, TRUE, dovspell},
        */
        {'#', "Ex-Cmd"},
        {0, NULL},
        {0, NULL},
        {0, NULL},
        {0, NULL},
        {0, NULL},
        {0, NULL},
        {0, NULL},
        {0, NULL}
};


static nds_cmd_t wiz_cmdlist[] = {
#ifdef WIZARD
	{C('e'), "Wiz-Detect"},
	{C('f'), "Wiz-Map"},
	{C('g'), "Wiz-Genesis"},
	{C('i'), "Wiz-Identify"},
	{C('o'), "Wiz-Where"},
	{C('v'), "Wiz-Tele"},
	{C('w'), "Wiz-Wish"},
#endif
        {0, NULL}
};

u16 *vram = (u16 *)BG_BMP_RAM(12);

/* Here we draw our layer. */

void nds_init_cmd()
{
  int cur_x = 0;
  int cur_y = 0;
  int i;
  struct ppm *img = alloc_ppm(256, 192);

  if (flags.debug) {
    int idx = 0;

    for (; cmdlist[idx].name != NULL; idx++);

    for (i = 0; wiz_cmdlist[i].name != NULL; i++) {
      cmdlist[idx++] = wiz_cmdlist[i];
    }
  }

  for (i = 0; cmdlist[i].name != NULL; i++) {
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

    if (flags.debug) {
      cur_y += text_h;
    } else {
      cur_y += text_h + 2;
    }
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

