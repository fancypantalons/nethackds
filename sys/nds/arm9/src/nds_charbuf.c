#include <nds.h>
#include <stdlib.h>

#include "hack.h"

#include "nds_win.h"
#include "nds_util.h"

#define BLOCK_SIZE 10

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

nds_line_t *nds_charbuf_append(nds_charbuf_t *buffer, const char *str)
{
  nds_line_t *ptr;

  if (buffer->avail <= buffer->count) {
    ptr = (nds_line_t *)realloc(buffer->lines, 
                                (buffer->count + BLOCK_SIZE) * sizeof(nds_line_t));

    buffer->avail += BLOCK_SIZE;

    if (ptr == NULL) {
      iprintf("Uhoh, ran out of memory!\n");
      exit(0);
    } else {
      buffer->lines = ptr;
    }
  }

  buffer->lines[buffer->count].text = strdup(str);
  buffer->lines[buffer->count].displayed = 0;

  text_dims(system_font, 
            (char *)str, 
            &(buffer->lines[buffer->count].width), 
            &(buffer->lines[buffer->count].height));

  buffer->count = buffer->count + 1;

  return &(buffer->lines[buffer->count - 1]);
}

char *nds_eat_whitespace(char *ptr, int invert)
{
  while (ISWHITESPACE(*ptr) ^ invert) {
    ptr++;
  }

  return ptr;
}

void nds_charbuf_wrap_line(nds_charbuf_t *buffer, nds_line_t line, int maxwidth)
{
  char *text = line.text;
  char *startptr, *endptr, *prevptr;

  startptr = text;
  endptr = text;
  prevptr = NULL;

  /* Fast forward past any leading spaces, we might as well preserve them. */

  while (1) {
    int c;
    int width;

    /* We're actually eating non-whitespace characters, here */
    /* Okay, theoretically we're at the end of a word, so let's get the length */

    endptr = nds_eat_whitespace(endptr, 1);

    c = *endptr;
    *endptr = '\0';

    text_dims(system_font, startptr, &width, NULL);

    /* 
     * Okay, prevptr should be pointing to the previous word we found.  So, if
     * the width is too much, and we have a previous word that we've found,
     * we restart the current character, rewind to the previous word, put a
     * NULL char at it's end, and append the string.  Then we start working again
     * from the end of the previous word.
     *
     * Note, in all cases, once a wrapping is performed, we skip past any extra
     * whitespace, since we don't want that appearing on the next line.
     */
    if ((width > maxwidth) && (prevptr != NULL)) {
      *endptr = c;

      *prevptr = '\0';
      nds_charbuf_append(buffer, startptr);

      startptr = nds_eat_whitespace(prevptr + 1, 0);
      endptr = startptr;
      prevptr = NULL;
    } else if (width > maxwidth) {
      /*
       * Alright, the width is too long, but there's no previous word.  This 
       * means this is just a *really* long word.  That's bad.  We'll just
       * append it for now, but eventually, we should probably cut the string
       * and append the pieces.
       */

      iprintf("Uhoh, this word is very big... I'm scurr\n");

      nds_charbuf_append(buffer, startptr);
      
      startptr = nds_eat_whitespace(endptr + 1, 0);
      endptr = startptr;
      prevptr = NULL;
    } else if (c == '\0') {
      /*
       * This, of course, is the end of the string.  So we just append what
       * we've found and terminate.
       */

      nds_charbuf_append(buffer, startptr);
      break;
    } else {
      /*
       * And, of course, the case where we found a word, but the buffer hasn't
       * exceeded the max line width yet, and we aren't at the end of the string.
       * In this case, just restore the original character (remember, we placed
       * a NULL there in order to terminate the test string), and set up prevptr
       * to point to this location, the end of the current word (in case the next
       * word causes us to exceed the buffer width).
       */
      *endptr = c;

      prevptr = endptr;
      endptr = nds_eat_whitespace(endptr, 0);
    }
  }
}

nds_charbuf_t *nds_charbuf_wrap(nds_charbuf_t *src, int maxwidth)
{
  nds_charbuf_t *dest = nds_charbuf_create(src->count);
  int i;

  /* We know we'll need at least as many lines as we have in the source */

  for (i = 0; i < src->count; i++) {
    if (src->lines[i].width < maxwidth) {
      nds_line_t *line = nds_charbuf_append(dest, src->lines[i].text);

      line->displayed = src->lines[i].displayed;
    } else {
      nds_charbuf_wrap_line(dest, src->lines[i], maxwidth);
    }
  }

  return dest;
}
