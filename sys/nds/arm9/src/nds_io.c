#include <nds.h>

#include "hack.h"

#include "nds_io.h"

#include <unistd.h>
#include <reent.h>
#include <sys/iosupport.h>
#include <errno.h>

#include "nds_cmd.h"
#include "nds_main.h"

int creat(const char* fn, mode_t mode) 
{
  return open(fn, O_CREAT | O_TRUNC | O_WRONLY, mode);
}

uid_t getuid() 
{
  return 0;
}

void error VA_DECL(const char*, line)
  VA_INIT(line, char *);
  char choice;

  nds_show_console();
  viprintf(line, VA_ARGS);

  choice = nds_yn_function("Should I break into the debugger?", "yn", 'y');

  if (choice == 'y') {
    nds_break_into_debugger();
  }

  VA_END();
}

void regularize(char *s)
{
  char *lp;

  for (lp = s; *lp; lp++)
    if ( *lp == '?' || *lp == '"' || *lp == '\\' ||
         *lp == '/' || *lp == '>' || *lp == '<'  ||
         *lp == '*' || *lp == '|' || *lp == ':'  || ((unsigned char)*lp > 127))
      *lp = '_';
}

char *get_username(int *lan_username_size)
{
  static char username_buffer[BUFSZ] = "NetHack\0";	// will get it from preferences at some point

  if (lan_username_size) *lan_username_size = strlen(username_buffer);
  return username_buffer;
}
