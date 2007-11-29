#include <nds.h>
#include <stdio.h>

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
  touchPosition coords = { .x = 0, .y = 0 };
  int pressed;

  while (1) {
    scanKeys();

    coords = touchReadXY();
    pressed = keysHeld();

    if ((coords.x == 0) && (coords.y == 0) && (pressed == 0)) {
      return;
    }
  }
}
