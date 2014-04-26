#ifndef _NDS_DEBUG_H_
#define _NDS_DEBUG_H_

#define DEBUG_PRINT(fmt, ...) nds_debug_print(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

void nds_debug_print(char *file, int line, char *fmt, ...);

#endif
