#include <nds.h>
#include <stdio.h>
#include <string.h>

touchPosition touch_coords = { .x = 0, .y = 0 };
touchPosition old_touch_coords;

/*
 * Copy a block of memory 2 bytes at a time.  This is needed for
 * things like VRAM.
 *
 * TODO: Test if we can do this in 32-bit copies, instead.
 */
void memcpy16(void *dest, void *src, int count)
{
  u16 *s = (u16 *)src;
  u16 *d = (u16 *)dest;
  int i;

  for (i = 0; i < count / 2; i++) {
    d[i] = s[i];
  }

  /* If it's an odd count, copy the last byte, if needed.
     This relies on the control variable retaining it's value after
     the loop is complete.  But I can never remember if that's well
     defined behaviour... */
     
  if (count & 0x01) {
    d[i] = (d[i] & 0x00FF) | (s[i] << 8);
  }
}

/*
 * Read the contents of a file into a given memory location.  Since
 * this can be used for populating VRAM (in fact, right now, that's
 * all it's for), we do 16-bit copies from a work buffer, rather
 * than reading into memory directly.
 */
int nds_load_file(char *fname, void *dest)
{
  u8 *d = (u8 *)dest; /* Makes our pointer arithmetic cleaner... */

  FILE *file = fopen(fname, "r");
  u8 buf[1024];
  int pos, read;

  if (file == NULL) {
    return -1;
  }

  pos = 0;

  while ((read = fread(buf, 1, sizeof(buf), file)) > 0) {
    memcpy16(d + pos, buf, read);
    pos += read;
  }

  fclose(file);

  return 0;
}

void nds_wait_key(int keys)
{
  while(1) {
    int pressed;

    swiWaitForVBlank();
    scanKeys();

    pressed = keysDown();

    if (pressed & keys) {
      break;
    }
  }
}

void nds_flush()
{
  memset(&old_touch_coords, 0, sizeof(old_touch_coords));
  memset(&touch_coords, 0, sizeof(touch_coords));

  while (1) {
    swiWaitForVBlank();
    scanKeys();

    if (keysHeld() == 0) {
      return;
    }
  }
}

/*
 * Various utility functions for interacting with the touch screen.
 */

void scan_touch_screen()
{
  old_touch_coords = touch_coords;
  touch_coords = touchReadXY();
}

int touch_down_in(int x, int y, int x2, int y2)
{
  if ((touch_coords.x == 0) || (touch_coords.y == 0)) {
    return 0;
  }

  if ((touch_coords.px >= x) && (touch_coords.px <= x2) &&
      (touch_coords.py >= y) && (touch_coords.py <= y2)) {
    return 1;
  } else {
    return 0;
  }
}

int touch_was_down_in(int x, int y, int x2, int y2)
{
  if ((touch_coords.x == 0) || (touch_coords.y == 0)) {
    return 0;
  }

  if ((touch_coords.px >= x) && (touch_coords.px <= x2) &&
      (touch_coords.py >= y) && (touch_coords.py <= y2)) {
    return 0;
  }

  if ((old_touch_coords.px >= x) && (old_touch_coords.px <= x2) &&
      (old_touch_coords.py >= y) && (old_touch_coords.py <= y2)) {
    return 1;
  } else {
    return 0;
  }
}

int touch_released_in(int x, int y, int x2, int y2)
{
  if ((touch_coords.x != 0) || (touch_coords.y != 0)) {
    return 0;
  }

  if ((old_touch_coords.px >= x) && (old_touch_coords.px <= x2) &&
      (old_touch_coords.py >= y) && (old_touch_coords.py <= y2)) {
    return 1;
  } else {
    return 0;
  }
}

int get_touch_coords(touchPosition *coords)
{
  if ((old_touch_coords.x == 0) && (old_touch_coords.y == 0)) {
    return 0;
  } else if ((touch_coords.x != 0) || (touch_coords.y != 0)) {
    return 0;
  } 

  *coords = old_touch_coords;

  return 1;
}
