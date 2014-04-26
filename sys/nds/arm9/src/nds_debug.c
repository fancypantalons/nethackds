#include <stdarg.h>
#include <stdio.h>
#include <nds.h>

#include "nds_debug.h"

void nds_debug_print(char *file, int line, char *fmt, ...)
{
  char buffer[1024];
  va_list ap;

  va_start(ap, fmt);

  sprintf(buffer, "%s:%d - %s", file, line, fmt);
  viprintf(buffer, ap);

  va_end(ap);
}
