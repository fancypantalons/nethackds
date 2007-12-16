/*
 * Various utility functions.
 */

#ifndef _NDS_UTIL_H_
#define _NDS_UTIL_H_

#define NULLFREE(ptr) { if (ptr != NULL) { free(ptr); ptr = NULL; } }

#define ISWHITESPACE(c) ((c == ' ') || (c == '\t'))

#define ABS(x) (((x) < 0) ? -(x) : (x))

#include <stdio.h>

void memcpy16(void *dest, void *src, int count);
int nds_load_file(char *fname, void *dest);
void nds_wait_key(int keys);
void nds_flush();

void scan_touch_screen();
int touch_down_in(int x, int y, int x2, int y2);
int touch_was_down_in(int x, int y, int x2, int y2);
int touch_released_in(int x, int y, int x2, int y2);
int get_touch_coords(touchPosition *coords);
#endif
