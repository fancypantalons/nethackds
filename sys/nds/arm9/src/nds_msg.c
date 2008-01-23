#include <nds.h>

#include "hack.h"

#include "nds_win.h"
#include "nds_msg.h"
#include "nds_charbuf.h"
#include "nds_status.h"
#include "nds_util.h"
#include "ppm-lite.h"

#define MSG_HISTORY_LINES 50

char *message_history[MSG_HISTORY_LINES];

struct ppm *msg_img = NULL;

int text_h;

int calls = 0;

void nds_init_msg()
{
  int i;

  memset(message_history, 0, sizeof(message_history));

  for (i = 0; i < MSG_HISTORY_LINES; i++) {
    message_history[i] = (char *)malloc(BUFSZ / 2);
    message_history[i][0] = '\0';
  }
}

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
  int x, y, mod;

  nds_get_msg_pos(&msg_x, &msg_y, &msg_w, &msg_h);

  draw_string(system_font, "Press a key...", msg_img,
              0, cur_y,
              -1, -1);

  draw_ppm(msg_img, vram, msg_x, msg_y, 256);

  nh_poskey(&x, &y, &mod);

  nds_flush(0);
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

  int i;
  char *history_tmp[MSG_HISTORY_LINES];
  int new_count = 0;

  if (msg_img == NULL) {
    text_h = system_font->height;
  }

  nds_get_msg_pos(&msg_x, &msg_y, &msg_w, &msg_h);

  num_lines = msg_h / system_font->height;

  calls++;

  if (msg_img == NULL) {
    msg_img = alloc_ppm(256, num_lines * text_h);
  }

  clear_ppm(msg_img, MAP_COLOUR(CLR_BLACK));

  if (win->buffer == NULL) {
    return;
  }

  /* First, count how many new lines we have */

  for (i = 0; i < win->buffer->count; i++) {
    if (! win->buffer->lines[i].historied) {
      new_count++;
    }
  }

  /* Now roll the history forward */

  for (i = 0; i < MSG_HISTORY_LINES; i++) {
    history_tmp[(i + new_count) % MSG_HISTORY_LINES] = message_history[i];
  }

  for (i = 0; i < MSG_HISTORY_LINES; i++) {
    message_history[i] = history_tmp[i];
  }

  /* Now copy the new lines into the history */

  for (i = 0; i < win->buffer->count; i++) {
    if (win->buffer->lines[i].historied) {
      continue;
    }

    strcpy(message_history[--new_count], win->buffer->lines[i].text);
  }

  /* Next, we wrap the buffer for display */

  buffer = nds_charbuf_wrap(win->buffer, 256 - msg_x);

  while (curline < buffer->count) {
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

      clear_ppm(msg_img, MAP_COLOUR(CLR_BLACK));
    }

    buffer->lines[curline].historied = 1;

    draw_string(system_font, buffer->lines[curline].text, msg_img,
                0, cur_y, 
                -1, -1);

    cur_y += buffer->lines[curline].height;

    curline++;
    linecnt++;
  }

  draw_ppm(msg_img, vram, msg_x, msg_y, 256);

  if (blocking) {
    nds_msg_wait_key(cur_y);
  }

  nds_charbuf_destroy(win->buffer);

  win->buffer = buffer;
}

int nds_msg_history()
{
  winid win = create_nhwindow(NHW_TEXT);
  int i;

  for (i = 0; message_history[i][0] && (i < MSG_HISTORY_LINES); i++) {
    putstr(win, ATR_NONE | ATR_NOREFLOW, message_history[i]);
  }

  display_nhwindow(win, 1);
  destroy_nhwindow(win);
  
  return 0;
}
