#include "font-bdf.h"

#define MAX_WINDOWS 8

extern struct font *system_font;

typedef struct {
  char **text;
  int lines;
} nds_charbuf_t;

typedef struct {
  ANY_P id;
  char title[80];
  int count;
  int selected;
  int x, y;
  int width, height;
} nds_menuitem_t;

typedef struct {
  nds_menuitem_t *items;
  char *prompt;
  u16 count;
} nds_menu_t;

typedef struct {
  u8 type;
  u16 w, h;   // Only used to report back to the core.
  u16 cx, cy; // Cursor position, in pixels.

  // Window type-specific structures.

  nds_charbuf_t *buffer;
  nds_menu_t *menu;
  u8 *glyphs;

  u8 dirty;
} nds_nhwindow_t;
