#include <nds.h>

#include "hack.h"

#include "nds_win.h"
#include "nds_msg.h"
#include "nds_charbuf.h"
#include "nds_util.h"
#include "ppm-lite.h"

#define NUM_LINES 5

#define MSG_IMG_X 3
#define MSG_IMG_Y 130

struct ppm *msg_img = NULL;

int text_h;

int calls = 0;

void nds_msg_wait_key(int cur_y)
{
  u16 *vram = (u16 *)BG_BMP_RAM_SUB(4);

  draw_string(system_font, "Press A...", msg_img,
              0, cur_y, 1,
              255, 0, 255);

  draw_ppm_bw(msg_img, vram, MSG_IMG_X, MSG_IMG_Y, 256, 254, 255);

  nds_wait_key(KEY_A);
}

void nds_update_msg(nds_nhwindow_t *win, int blocking)
{
  u16 *vram = (u16 *)BG_BMP_RAM_SUB(4);
  nds_charbuf_t *buffer;
  int curline = 0;
  int linecnt = 0;
  int cur_y = 0;

  calls++;

  if (msg_img == NULL) {
    text_dims(system_font, "#", NULL, &text_h);

    msg_img = alloc_ppm(256, NUM_LINES * text_h);
  }

  clear_ppm(msg_img);

  if (win->buffer == NULL) {
    return;
  }

  buffer = nds_charbuf_wrap(win->buffer, 256 - MSG_IMG_X - 3);

  while (1) {
    if (curline >= buffer->count) {
      break;
    }

    if (buffer->lines[curline].displayed) {
      curline++;
      continue;
    }

    if (linecnt >= NUM_LINES - 1) {
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

    draw_string(system_font, buffer->lines[curline].text, msg_img,
                0, cur_y, 1,
                255, 0, 255);

    cur_y += buffer->lines[curline].height;

    curline++;
    linecnt++;
  }

  draw_ppm_bw(msg_img, vram, MSG_IMG_X, MSG_IMG_Y, 256, 254, 255);

  if (blocking) {
    nds_msg_wait_key(cur_y);
  }

  nds_charbuf_destroy(win->buffer);

  win->buffer = buffer;
}
