#ifndef _NDS_CHARBUF_H_
#define _NDS_CHARBUF_H_

typedef struct {
  char *text;
  int width;
  int height;
  int displayed;
} nds_line_t;

typedef struct {
  nds_line_t *lines;
  int count;
  int avail;
} nds_charbuf_t;

nds_charbuf_t *nds_charbuf_create();

void nds_charbuf_destroy(nds_charbuf_t *buffer);
nds_line_t *nds_charbuf_append(nds_charbuf_t *buffer, const char *str);
nds_charbuf_t *nds_charbuf_wrap(nds_charbuf_t *src, int maxwidth);

#endif
