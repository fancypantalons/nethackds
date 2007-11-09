/*
 * Various utility functions.
 */

#ifndef _NDS_UTIL_H_
#define _NDS_UTIL_H_

#include <stdio.h>

void memcpy16(void *dest, void *src, int count);
int nds_load_file(char *fname, void *dest);

#endif
