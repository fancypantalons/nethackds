
#ifndef DS_FILEIO_H
#define DS_FILEIO_H

// guarantee that this file will be included before the #defs
#include <fcntl.h>

#define E extern

E void VDECL(error, (const char *,...)) PRINTF_F(1,2);
E void FDECL(regularize, (char *));
E char* FDECL(get_username, (int *));

#endif
