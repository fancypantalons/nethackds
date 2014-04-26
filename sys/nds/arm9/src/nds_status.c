#include <nds.h>
#include <stdlib.h>
#include <string.h>

#include "hack.h"

#include "nds_debug.h"
#include "nds_win.h"
#include "nds_gfx.h"
#include "nds_util.h"
#include "nds_map.h"
#include "ppm-lite.h"

#define CUTCOUNT 2

struct ppm *status_img = NULL;

nds_charbuf_t *status_lines = NULL;
nds_charbuf_t *last_status_lines = NULL;

int name_printed = 0;

int status_bottom = 0;

void nds_update_status(char *str)
{
  u16 *vram = (u16 *)BG_BMP_RAM_SUB(4);
  int text_h = system_font->height;

  rectangle_t map_rect = nds_minimap_dims();
  int status_x = 2;
  int status_y = RECT_END_Y(map_rect) + 2;

  if (status_img == NULL) {
    status_img = alloc_ppm(256, system_font->height);
    status_lines = nds_charbuf_create(1);
  }

  if (status_lines->count == 0) {
    char *name;
    int i;

    for (i = 0; i < strlen(str); i++) {
      if ((str[i] == ':') && (strncmp(str + i - 2, "St", 2) == 0)) {
        name = str;
        str[i - 3] = '\0';
        str = str + i - 2;
      }
    }

    if (! name_printed) {
      nds_draw_text(system_font, name,
                    3, 0, vram);

      name_printed = 1;
    }

    nds_charbuf_append(status_lines, str, 0);
  } else {
    nds_charbuf_t *wrapped;
    int i;

    nds_charbuf_append(status_lines, str, 0);

    wrapped = nds_charbuf_wrap(status_lines, 256 - status_x);

    for (i = 0; i < wrapped->count; i++) {
      if ((last_status_lines != NULL) &&
          (i < last_status_lines->count) && 
          (strcmp(wrapped->lines[i].text, last_status_lines->lines[i].text) == 0)) {

        continue;
      }

      clear_ppm(status_img, MAP_COLOUR(CLR_BLACK));

      draw_string(system_font, wrapped->lines[i].text, status_img, 
                  0, 0, 
                  -1, -1);

      draw_ppm(status_img, vram, status_x, status_y + text_h * i, 256);
    }

    status_bottom = status_y + wrapped->count * text_h + 2;

    nds_draw_hline(0, status_bottom - 2, 256, 0, vram);
    nds_draw_hline(0, status_bottom - 1, 256, 0, vram);

    if (last_status_lines) {
      nds_charbuf_destroy(last_status_lines);
    }

    nds_charbuf_destroy(status_lines);

    status_lines = nds_charbuf_create(1);
    last_status_lines = wrapped;
  }
}

int nds_status_get_bottom()
{
  return status_bottom;
}
