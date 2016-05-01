#include <nds.h>

#include "hack.h"

#include "nds_win.h"
#include "nds_getlin.h"
#include "nds_gfx.h"
#include "nds_debug.h"

#include "nds_kbd.h"
#include "ppm-lite.h"

#define HISTORY_SIZE 10

char input_history[HISTORY_SIZE][BUFSZ];
int history_count = 0;

void _nds_push_history(char *str)
{
  if (history_count >= HISTORY_SIZE) {
    int i;

    for (i = 0; i < history_count - 2; i++) {
      strcpy(input_history[i], input_history[i + 1]);
    }
  } else {
    history_count++;
  }

  strcpy(input_history[history_count - 1], str);
}

void nds_getlin(const char *prompt, char *buffer)
{
  static struct ppm *input_img = NULL;
  static int input_y;

  int history_pos = history_count;

  u16 *vram = (u16 *)BG_BMP_RAM_SUB(4);

  char front[BUFSZ];
  char back[BUFSZ];

  int curspos = 0;
  int length = 0;

  int done = 0;
  int text_h;

  text_h = system_font->height;

  if (input_img == NULL) {
    input_img = alloc_ppm(252, text_h); /* We'll presume the height is constant */
    input_y = 192 - text_h;
  }

  nds_draw_prompt((char *)prompt);

  /* Now initialize our buffers */

  front[0] = '\0';
  back[0] = '\0';

  /* First, display the keyboard and prompting layers */

  REG_DISPCNT |= DISPLAY_BG0_ACTIVE;

  /* Handle lingering taps. */

  nds_flush(~KEY_TOUCH);

  /* Now, enter the key loop */

  while (! done) {
    u8 key;
    int front_w;

    clear_ppm(input_img, MAP_COLOUR(CLR_BLACK));

    /* Alright, fill the prompting layer and draw out prompt. */

    text_dims(system_font, front, &front_w, NULL);

    draw_string(system_font, front, input_img, 0, 0, -1, -1);
    draw_string(system_font, back, input_img, front_w, 0, -1, -1);

    swiWaitForVBlank();

    draw_ppm(input_img, vram, 4, input_y, 256);
    nds_draw_vline(front_w + 4, input_y, text_h, C_CURSOR, vram);

    scanKeys();

    key = kbd_do_one_loop();

    switch (key) {
      case 0:
      case K_UP_LEFT:
      case K_UP_RIGHT:
      case K_NOOP:
      case K_DOWN_LEFT:
      case K_DOWN_RIGHT:
        continue;

      case K_UP:
        if (history_pos <= 0) {
          break;
        } else {
          history_pos--;
        }

        strcpy(front, input_history[history_pos]);
        *back = '\0';

        length = strlen(front);
        curspos = length;

        break;

      case K_DOWN:
        if (history_pos >= history_count) {
          break;
        } else {
          history_pos++;
        }

        if (history_pos == history_count) {
          *front = '\0';
        } else {
          strcpy(front, input_history[history_pos]);
        }

        *back = '\0';
        length = strlen(front);
        curspos = length;

        break;

      case '\n':
        done = 1;

        break;

      case '\b':
        if (curspos > 0) {
          front[--curspos] = '\0';

          length--;
        }

        break;

      case K_LEFT:
        if (curspos > 0) {
          memmove(back + 1, back, length - curspos + 1);

          back[0] = front[curspos - 1];
          front[--curspos] = '\0';
        }

        break;

      case K_RIGHT:
        if (curspos < length) {
          front[curspos++] = back[0];
          front[curspos] = '\0';

          memmove(back, back + 1, length - curspos + 1);
        }

        break;

      default:
        front[curspos++] = key;
        front[curspos] = '\0';

        length++;

        break;
    }
  }

  nds_flush(0);

  /* Yeah, it's cheesy... I could use draw_rect... but this works, too :) */

  nds_clear_prompt();

  clear_ppm(input_img, MAP_COLOUR(CLR_BLACK));
  draw_ppm(input_img, vram, 4, input_y, 256);

  REG_DISPCNT ^= DISPLAY_BG0_ACTIVE;

  strcpy(buffer, front);
  strcat(buffer, back);
  
  _nds_push_history(buffer);
}
