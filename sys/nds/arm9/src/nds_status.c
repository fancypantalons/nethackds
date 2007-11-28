#include <nds.h>
#include <stdlib.h>
#include <string.h>

#include "hack.h"

#include "nds_win.h"
#include "nds_gfx.h"
#include "nds_util.h"
#include "ppm-lite.h"

#define CUTCOUNT 2

#define STATUS_X 3
#define STATUS_Y 64

struct ppm *status_img = NULL;

char *name = NULL;

void nds_update_status(char *str)
{
  int i;
  u16 *vram = (u16 *)BG_BMP_RAM_SUB(4);
  int text_h;
  int last = 1;

  if (status_img == NULL) {
    status_img = alloc_ppm(256, 30);
  }

  for (i = 0; i < strlen(str); i++) {
    if ((str[i] == ':') && (strncmp(str + i - 2, "St", 2) == 0)) {
      name = str;
      str[i - 3] = '\0';
      str = str + i - 2;

      last = 0;
    }
  }

  if (! last) {
    nds_draw_text(system_font, name,
                  3, 3, 254, 255, vram);

    clear_ppm(status_img);

    draw_string(system_font, str, status_img, 
                0, 0, 1,
                255, 0, 255);
  } else {
    int cnt = 0;
    char *cutptr;

    for (cutptr = str + strlen(str); cutptr != str; cutptr--) {
      if ((*cutptr == ':') && (++cnt == CUTCOUNT)) {
        break;
      }
    }

    while (! ISWHITESPACE(*cutptr)) {
      cutptr--;
    }

    *cutptr = '\0';
    cutptr++;

    text_dims(system_font, str, NULL, &text_h);

    draw_string(system_font, str, status_img, 
                0, text_h, 1,
                255, 0, 255);

    draw_string(system_font, cutptr, status_img, 
                0, text_h * 2, 1,
                255, 0, 255);

    draw_ppm_bw(status_img, vram, STATUS_X, STATUS_Y, 256, 254, 255);
  }
}
