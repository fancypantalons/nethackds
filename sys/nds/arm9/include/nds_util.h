/*
 * Various utility functions.
 */

#ifndef _NDS_UTIL_H_
#define _NDS_UTIL_H_

#define NULLFREE(ptr) { if (ptr != NULL) { free(ptr); ptr = NULL; } }

#define ISWHITESPACE(c) ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'))

#define ABS(x) (((x) < 0) ? -(x) : (x))

#include <stdio.h>

typedef struct {
  int x;
  int y;
} coord_t;

typedef struct {
  int width;
  int height;
} dims_t;

typedef struct {
  coord_t start;
  dims_t dims;
} rectangle_t;

#define POINT_IN_RECT(c, r) ( ((c).x >= (r).start.x) && ((c).x <= ((r).start.x + (r).dims.width)) && \
                              ((c).y >= (r).start.y) && ((c).y <= ((r).start.y + (r).dims.height)) )

#define COORDS_ARE_EQUAL(a, b) (((a).x == (b).x) && ((a).y == (b).y))
#define RECT_END_X(r) ((r).start.x + (r).dims.width)
#define RECT_END_Y(r) ((r).start.y + (r).dims.height)

void memcpy16(void *dest, void *src, int count);
int nds_load_file(char *fname, void *dest);
void nds_wait_key(int keys);
void nds_flush(int ignore);
u16 nds_keysDown();
u16 nds_keysDownRepeat();
u16 nds_keysHeld();
u16 nds_keysUp();

void scan_touch_screen();
int touch_down_in(rectangle_t region);
int touch_was_down_in(rectangle_t region);
int touch_released_in(rectangle_t region);
int get_tap_coords(coord_t *coords);
coord_t get_touch_coords();

coord_t coord_add(coord_t a, coord_t b);
coord_t coord_subtract(coord_t a, coord_t b);

char *nds_strip(char *str, int front, int back);
int nds_count_bits(int val);
int nds_ends_width(char *str, char *suffix);

#endif
