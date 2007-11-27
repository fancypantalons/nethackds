/*
 * Various utility functions.
 */

#ifndef _NDS_UTIL_H_
#define _NDS_UTIL_H_

#define NULLFREE(ptr) { if (ptr != NULL) { free(ptr); ptr = NULL; } }

#define ISWHITESPACE(c) ((c == ' ') || (c == '\t') || (c == '\0'))

#include <stdio.h>

void memcpy16(void *dest, void *src, int count);
int nds_load_file(char *fname, void *dest);
void nds_wait_key(int keys);
void nds_flush_touch();

#endif
