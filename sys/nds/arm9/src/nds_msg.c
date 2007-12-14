#include <nds.h>

#include "hack.h"

#include "nds_win.h"
#include "nds_msg.h"
#include "nds_charbuf.h"
#include "nds_status.h"
#include "nds_util.h"
#include "ppm-lite.h"

struct ppm *msg_img = NULL;

int text_h;

int calls = 0;

void nds_get_msg_pos(int *x, int *y, int *w, int *h)
{
  *x = 2;
  *y = nds_status_get_bottom();

  *w = 256 - 2;
  *h = 192 - text_h * 2 - *y;
}

void nds_msg_wait_key(int cur_y)
{
  u16 *vram = (u16 *)BG_BMP_RAM_SUB(4);
  int msg_x, msg_y, msg_w, msg_h;

  nds_get_msg_pos(&msg_x, &msg_y, &msg_w, &msg_h);

  draw_string(system_font, "Press A...", msg_img,
              0, cur_y, 1,
              255, 0, 255);

  draw_ppm_bw(msg_img, vram, msg_x, msg_y, 256, 254, 255);

  nds_wait_key(KEY_A);
}

void nds_update_msg(nds_nhwindow_t *win, int blocking)
{
  u16 *vram = (u16 *)BG_BMP_RAM_SUB(4);
  nds_charbuf_t *buffer;
  int curline = 0;
  int linecnt = 0;
  int cur_y = 0;
  int msg_x, msg_y, msg_w, msg_h;
  int num_lines;

  nds_get_msg_pos(&msg_x, &msg_y, &msg_w, &msg_h);

  num_lines = msg_h / system_font->height;

  calls++;

  if (msg_img == NULL) {
    text_h = system_font->height;

    msg_img = alloc_ppm(256, num_lines * text_h);
  }

  clear_ppm(msg_img);

  if (win->buffer == NULL) {
    return;
  }

  buffer = nds_charbuf_wrap(win->buffer, 256 - msg_x);

  while (1) {
    if (curline >= buffer->count) {
      break;
    }

    if (buffer->lines[curline].displayed) {
      curline++;
      continue;
    }

    if (linecnt >= num_lines) {
      int i;

      nds_msg_wait_key(cur_y);

      /*
       * This is so we don't re-display lines that we've already done a
       * "Press A" on.
       */
      for (i = 0; i < curline; i++) {
        buffer->lines[i].displayed = 1;
      }

      cur_y = 0;
      linecnt = 0;

      clear_ppm(msg_img);
    }

    iprintf("M: %s\n", buffer->lines[curline].text);

    draw_string(system_font, buffer->lines[curline].text, msg_img,
                0, cur_y, 1,
                255, 0, 255);

    cur_y += buffer->lines[curline].height;

    curline++;
    linecnt++;
  }

  draw_ppm_bw(msg_img, vram, msg_x, msg_y, 256, 254, 255);

  if (blocking) {
    nds_msg_wait_key(cur_y);
  }

  nds_charbuf_destroy(win->buffer);

  win->buffer = buffer;
}
