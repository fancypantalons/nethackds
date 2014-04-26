#include <nds.h>
#include <stdlib.h>

#include "hack.h"

#include "nds_debug.h"
#include "nds_charbuf.h"
#include "nds_win.h"
#include "nds_util.h"

#include "font-bdf.h"

#define BLOCK_SIZE 10

char wrap_buffer[BUFSZ * 2];

nds_charbuf_t *nds_charbuf_create(int initial_avail)
{
  nds_charbuf_t *buffer;

  buffer = (nds_charbuf_t *)malloc(sizeof(nds_charbuf_t));
  buffer->avail = initial_avail;
  buffer->count = 0;

  if (initial_avail > 0) {
    buffer->lines = (nds_line_t *)malloc(sizeof(nds_line_t) * initial_avail);
  } else {
    buffer->lines = NULL;
  }

  return buffer;
}

void nds_charbuf_destroy(nds_charbuf_t *buffer)
{
  int i;

  if ((buffer != NULL) && (buffer->lines != NULL)) {
    for (i = 0; i < buffer->count; i++) {
      NULLFREE(buffer->lines[i].text);
    }

    NULLFREE(buffer->lines);
  }

  NULLFREE(buffer);
}

nds_line_t *nds_charbuf_append(nds_charbuf_t *buffer, const char *str, int reflow)
{
  nds_line_t *ptr;

  if (buffer->avail <= buffer->count) {
    ptr = (nds_line_t *)realloc(buffer->lines, 
                                (buffer->count + BLOCK_SIZE) * sizeof(nds_line_t));

    buffer->avail += BLOCK_SIZE;

    if (ptr == NULL) {
      DEBUG_PRINT("Uhoh, ran out of memory!\n");
      exit(0);
    } else {
      buffer->lines = ptr;
    }
  }

  buffer->lines[buffer->count].text = strdup(str);
  buffer->lines[buffer->count].displayed = 0;
  buffer->lines[buffer->count].historied = 0;
  buffer->lines[buffer->count].reflow = reflow;

  text_dims(system_font, 
            (char *)str, 
            &(buffer->lines[buffer->count].width), 
            &(buffer->lines[buffer->count].height));

  buffer->count = buffer->count + 1;

  return &(buffer->lines[buffer->count - 1]);
}

/* Returns a pointer to the start of the next word in the given string. */
char *get_next_word_end(char *str)
{
  /* Advance to the next word. */
  while (ISWHITESPACE(*str) && *str) {
    str++;
  }

  /* Now advance to the end of the word. */
  while (! ISWHITESPACE(*str) && *str) {
    str++;
  }

  return str;
}

/*
 * Copies a string into the destination buffer from the start of the wrap
 * buffer, such that the string's width is less than or equal to the
 * required width.  It then removes the characters from the wrap buffer.
 * Returns the length of the string pulled out.
 */
int get_line_from_wrap_buffer(char *buffer, int buflen, char *dest, int maxwidth)
{
  char *end = buffer;
  int len;

  while (1) {
    char *ptr = get_next_word_end(end);
    char c = *ptr;
    int str_w;

    *ptr = '\0';
    text_dims(system_font, buffer, &str_w, NULL);
    *ptr = c;

    if (str_w >= maxwidth) {
      *end = '\0';
      end++;

      break;
    } else if (c == '\0') {
      end = ptr + 1;
      *end = '\0'; /* We need this so that after the memmove there's a '\0' */

      break;
    } else {
      end = ptr;
    }
  }

  strcpy(dest, buffer);
  len = end - buffer - 1;

  /* Now let's erase any leading whitespace */
  end = nds_strip(end, 1, 0);

  memmove(buffer, end, buflen - (end - buffer));

  return len;
}

nds_charbuf_t *nds_charbuf_wrap(nds_charbuf_t *src, int maxwidth)
{
  nds_charbuf_t *dest = nds_charbuf_create(src->count);

  char prefix[BUFSZ];
  int prefix_len;
  int prefix_width;

  char segment[BUFSZ];
  int segment_len;

  int new_paragraph = 1;
  int curline = 0;

  wrap_buffer[0] = '\0';

  while (*wrap_buffer || (curline < src->count)) {
    if (src->lines[curline].displayed) {
      curline++;
      continue;
    }

    if (new_paragraph && ! *wrap_buffer && (curline < src->count)) {
      int reflow = src->lines[curline].reflow;
      char *bufline = src->lines[curline++].text;
      char *line = nds_strip(bufline, 1, 1);

      prefix_len = line - bufline;

      strcpy(wrap_buffer, line);
      strcat(wrap_buffer, " ");
      strncpy(prefix, bufline, prefix_len); 
      prefix[prefix_len] = '\0';

      text_dims(system_font, prefix, &prefix_width, NULL);

      new_paragraph = 0;

      if ((curline > 0) && reflow) {
        nds_charbuf_append(dest, "", 0);
      }
    } else if (! new_paragraph && (curline < src->count)) {
      int reflow = src->lines[curline].reflow;
      char *line = nds_strip(src->lines[curline++].text, 1, 1); 

      if (*line) {
        strcat(wrap_buffer, line);
        strcat(wrap_buffer, " ");

        new_paragraph = (reflow == 0);
      } else {
        new_paragraph = 1;
      }
    }
    
    segment_len = get_line_from_wrap_buffer(wrap_buffer, sizeof(wrap_buffer), 
                                            segment, maxwidth - prefix_width);

    if (segment_len > 0) {
      char tmp[BUFSZ];

      strcpy(tmp, prefix);
      strcat(tmp, segment);

      nds_charbuf_append(dest, tmp, 0);
    }
  }

  return dest;
}
