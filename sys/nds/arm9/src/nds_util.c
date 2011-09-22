#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "hack.h"
#include "nds_main.h"
#include "nds_util.h"

touchPosition raw_touch_coords = { .rawx = 0, .rawy = 0 };

coord_t touch_coords = { .x = 0, .y = 0 };
coord_t old_touch_coords = { .x = 0, .y = 0 };

int was_touching = 0;
int touching = 0;

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

    pressed = nds_keysDown();

    if (pressed & keys) {
      break;
    }
  }
}

void nds_flush(int ignore)
{
  memset(&old_touch_coords, 0, sizeof(old_touch_coords));
  memset(&touch_coords, 0, sizeof(touch_coords));

  while (1) {
    int held;

    swiWaitForVBlank();
    scanKeys();

    held = nds_keysHeld();

    if ((held & ~ignore) == 0) {
      return;
    }
  }
}

u16 _nds_handedness_swap(u16 keys)
{
  if (iflags.lefthanded) {
    int l = keys & KEY_L;
    int r = keys & KEY_R;

    keys &= ~(KEY_L | KEY_R);

    keys |= l ? KEY_R : 0;
    keys |= r ? KEY_L : 0;
  }

  return keys;
}

u16 nds_keysDown()
{
  while (nds_power_state() == POWER_STATE_ASLEEP) {
    swiWaitForVBlank();
  }

  return _nds_handedness_swap(keysDown());
}

u16 nds_keysDownRepeat()
{
  while (nds_power_state() == POWER_STATE_ASLEEP) {
    swiWaitForVBlank();
  }

  return _nds_handedness_swap(keysDownRepeat());
}

u16 nds_keysHeld()
{
  while (nds_power_state() == POWER_STATE_ASLEEP) {
    swiWaitForVBlank();
  }

  return _nds_handedness_swap(keysHeld());
}

u16 nds_keysUp()
{
  while (nds_power_state() == POWER_STATE_ASLEEP) {
    swiWaitForVBlank();
  }

  return _nds_handedness_swap(keysUp());
}

/*
 * Various utility functions for interacting with the touch screen.
 */

void scan_touch_screen()
{
  raw_touch_coords = touchReadXY();

  old_touch_coords = touch_coords;
  was_touching = touching;

  if ((raw_touch_coords.rawx == 0) || (raw_touch_coords.rawy == 0)) {
    touching = 0;
  } else {
    touch_coords.x = raw_touch_coords.px;
    touch_coords.y = raw_touch_coords.py;

    touching = 1;
  }
}

int touch_down_in(rectangle_t region)
{
  return touching && POINT_IN_RECT(touch_coords, region);
}

int touch_was_down_in(rectangle_t region)
{
  return touching && POINT_IN_RECT(old_touch_coords, region) && ! POINT_IN_RECT(touch_coords, region);
}

int touch_released_in(rectangle_t region)
{
  return ! touching && was_touching && POINT_IN_RECT(old_touch_coords, region);
}

int get_tap_coords(coord_t *coords)
{
  if (! was_touching || touching) {
    return 0;
  } else {
    *coords = old_touch_coords;

    return 1;
  }
}

coord_t get_touch_coords()
{
  return touch_coords;
}

coord_t coord_add(coord_t a, coord_t b)
{
  coord_t ret = {
    .x = a.x + b.x,
    .y = a.y + b.y
  };

  return ret;
}

coord_t coord_subtract(coord_t a, coord_t b)
{
  coord_t ret = {
    .x = a.x - b.x,
    .y = a.y - b.y
  };

  return ret;
}

/*
 * Returns the string with the trailing whitespace stripped off, and the
 * pointer advanced to point past the leading whitespace.
 */
char *nds_strip(char *str, int front, int back)
{
  char *end = str + strlen(str) - 1;

  while (front && *str && ISWHITESPACE(*str)) {
    str++;
  }

  while (back && (end >= str) && ISWHITESPACE(*end)) {
    *end = '\0';
    end--;
  }

  return str;
}

/*
 * Count the number of bits in the given value.
 */
int nds_count_bits(int val)
{
  int cnt = 0;

  while (val) {
    if (val & 1) {
      cnt++;
    }

    val >>= 1;
  }

  return cnt;
}

int nds_ends_width(char *str, char *suffix)
{
  if (strlen(str) < strlen(suffix)) {
    return 0;
  } else {
    return strcmp(str + strlen(str) - strlen(suffix), suffix) == 0;
  }
}
