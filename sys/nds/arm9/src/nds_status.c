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

#define STATUS_LINE_COUNT 3

struct ppm *status_img = NULL;

char status_lines[STATUS_LINE_COUNT][BUFSZ];
int cur_status_line = 0;

int name_printed = 0;

int _nds_status_line_dirty(char *str)
{
  int ret;

  if (strcmp(status_lines[cur_status_line], str) == 0) {
    ret = 0;
  } else {
    strcpy(status_lines[cur_status_line], str);

    ret = 1;
  }

  cur_status_line++;

  if (cur_status_line >= STATUS_LINE_COUNT) {
    cur_status_line = 0;
  }

  return ret;
}

void nds_update_status(char *str)
{
  u16 *vram = (u16 *)BG_BMP_RAM_SUB(4);
  int text_h;

  if (status_img == NULL) {
    int i;

    status_img = alloc_ppm(256, 10);

    for (i = 0; i < STATUS_LINE_COUNT; i++) {
      *(status_lines[i]) = '\0';
    }
  }

  if (cur_status_line == 0) {
    char *name;
    int i;

    if (! _nds_status_line_dirty(str)) {
      return;
    }

    for (i = 0; i < strlen(str); i++) {
      if ((str[i] == ':') && (strncmp(str + i - 2, "St", 2) == 0)) {
        name = str;
        str[i - 3] = '\0';
        str = str + i - 2;
      }
    }

    if (! name_printed) {
      nds_draw_text(system_font, name,
                    3, 3, 254, 255, vram);

      name_printed = 1;
    }

    clear_ppm(status_img);

    draw_string(system_font, str, status_img, 
                0, 0, 1,
                255, 0, 255);

    draw_ppm_bw(status_img, vram, STATUS_X, STATUS_Y, 256, 254, 255);
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

    if (_nds_status_line_dirty(str)) {
      clear_ppm(status_img);

      draw_string(system_font, str, status_img, 
                  0, 0, 1,
                  255, 0, 255);

      draw_ppm_bw(status_img, vram, STATUS_X, STATUS_Y + text_h, 256, 254, 255);
    }

    if (_nds_status_line_dirty(cutptr)) {
      clear_ppm(status_img);

      draw_string(system_font, cutptr, status_img, 
                  0, 0, 1,
                  255, 0, 255);

      draw_ppm_bw(status_img, vram, STATUS_X, STATUS_Y + text_h * 2, 256, 254, 255);
    }
  }
}
