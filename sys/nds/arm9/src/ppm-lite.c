/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*-
   ppm-lite.c --- simple, self-contained code for manipulating
   PBM, PGM, and PPM data.

   Copyright © 2001 Jamie Zawinski <jwz@jwz.org>

   Permission to use, copy, modify, distribute, and sell this software and its
   documentation for any purpose is hereby granted without fee, provided that
   the above copyright notice appear in all copies and that both that
   copyright notice and this permission notice appear in supporting
   documentation.  No representations are made about the suitability of this
   software for any purpose.  It is provided "as is" without express or 
   implied warranty.
 */

#include "config.h"
#include "ppm-lite.h"

#include "config.h"
#include "ppm-lite.h"
#include "font-bdf.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <nds.h>

#undef countof
#define countof(x) (sizeof((x))/sizeof(*(x)))

struct ppm *alloc_ppm(int width, int height)
{
  struct ppm *img;

  img = (struct ppm *)malloc(sizeof(struct ppm));

  img->width = width;
  img->height = height;
  img->rgba = (unsigned char *)malloc(width * height * 4);

  clear_ppm(img);

  return img;
}

void clear_ppm(struct ppm *img)
{
  memset(img->rgba, 0, img->width * img->height * 4);
}

/* Reads a non-blank line, stripping comments and blank lines.
   At EOF, either exits, or returns 0, depending on exit_p.
 */
static char *
read_ppm_line (FILE *in, char *buf, size_t size,
               const char *file, int *lineP, int exit_p)
{
  char *s;

  if (file[0] == '-' && file[1] == 0)
    file = "<stdin>";

  while (1)
    {
      if (!fgets (buf, size-1, in))
        {
          if (!exit_p) return 0;
          iprintf ("%s: %d: premature EOF\n",
                   file, *lineP);
          return NULL;
        }

      (*lineP)++;

      s = strchr(buf, '#');    /* delete comments to end of line */
      if (s)
        {
          s[0] = '\n';
          s[1] = 0;
        }

      s = buf;
      while (*s == ' ' || *s == '\t' ||  /* skip white */
             *s == '\n' || *s == '\r')
        s++;

      if (*s != 0)
        break;    /* non-blank line: return it.  Else read another. */
    }
  return buf;
}


/* Reads a PBM, PGM, or PPM file, binary or ASCII, and returns a `struct ppm'
   that contains RGBA data.  File name of "-" means stdin.
 */
struct ppm *
read_ppm (const char *file)
{
  int stdin_p = !strcmp(file, "-");
  FILE *in;
  char buf [10240];
  int max;
  char dummy;
  struct ppm *ppm = (struct ppm *) calloc (1, sizeof(*ppm));
  int line = 1;

  if (stdin_p)
    in = stdin;
  else
    {
      in = fopen (file, "r");
      if (!in)
        {
          iprintf("%.255s\n", file);
          return NULL;
        }
    }

  read_ppm_line (in, buf, sizeof(buf), file, &line, 1);  /* header line */
  if (buf[0] != 'P' ||
      buf[1] < '1' ||
      buf[1] > '6' ||
      buf[2] != '\n')
    {
      iprintf ("%s: not a PBM, PGM, or PPM file\n",
               file);
      return NULL;
    }

  ppm->type = buf[1] - '0';
  if (ppm->type < 1 || ppm->type > 6)
    abort();
  

  read_ppm_line (in, buf, sizeof(buf), file, &line, 1); /* width/height line */

  if (2 != sscanf (buf, "%d %d %c", &ppm->width, &ppm->height, &dummy) ||
      ppm->width < 1 || ppm->height < 1)
    {
      iprintf ("%s: %d: bogus PNM file: %s\n",
               file, line, buf);
      return NULL;
    }

  ppm->rgba = (unsigned char *)
    calloc (1, (ppm->width + 2) * (ppm->height + 2) * 4);

  if (!ppm->rgba)
    {
      iprintf ("%s: out of memory (%d x %d)\n",
               file, ppm->width, ppm->height);
      return NULL;
    }

  if (ppm->type == 1 || ppm->type == 4)
    max = 1;
  else
    {
      read_ppm_line (in, buf, sizeof(buf), file, &line, 1);   /* maxval line */
      if (1 != sscanf (buf, "%d %c", &max, &dummy) || max < 1)
        {
          iprintf ("%s: %d: bogus PNM file: %s\n",
                   file, line, buf);
          return NULL;
        }

      if (max > 255)
        {
          iprintf ("%s: %d: maxval must be <= 255\n",
                   file, line);
          return NULL;
        }
    }

  if (ppm->type == 1 || ppm->type == 2 || ppm->type == 3)
    {
      unsigned char *out = ppm->rgba;
      unsigned char *end = out + (ppm->width * ppm->height * 4);
      int tick = 0;

      while (read_ppm_line (in, buf, sizeof(buf), file, &line, 0))
        {
          char *s = buf;
          while (*s && (*s != '\r' && *s != '\n'))
            {
              int any = 0;
              int n = 0;
              while (*s == ' ' || *s == '\t') s++;

              if (*s >= '0' && *s <= '9')
                {
                  any = 1;
                  while (*s >= '0' && *s <= '9')
                    n = (n * 10) + (*s++ - '0');
                }

              if (n > max)
                {
                  iprintf ("%s: %d: value (%d) exceeds max (%d)\n",
                           file, line, n, max);
                  return NULL;
                }

              if (*s &&
                  *s != ' ' && *s != '\t' &&
                  *s != '\r' && *s != '\n')
                {
                  iprintf ("%s: %d: bogus PNM file: '%c'\n",
                           file, line, *s);
                  return NULL;
                }

              if (!any)            /* got nothing */
                ;
              else if (ppm->type == 1)  /* PBM */
                {
                  *out++ = (n ? 255 : 0);
                  *out++ = (n ? 255 : 0);
                  *out++ = (n ? 255 : 0);
                  *out++ = 255;
                }
              else if (ppm->type == 2) /* PGM */
                {
                  n = (max == 255 ? n : ((255 * n) / max));
                  *out++ = n;
                  *out++ = n;
                  *out++ = n;
                  *out++ = 255;
                }
              else if (ppm->type == 3) /* PPM */
                {
                  n = (max == 255 ? n : ((255 * n) / max));
                  *out++ = n;

                  if (++tick == 3)  /* fill in alpha */
                    {
                      tick = 0;
                      *out++ = 255;
                    }
                }
              else
                abort();
            }

          if (out > end)
            break;
        }

      if (out < end)
        iprintf ("%s: %d: premature end of data\n",
                 file, line);
    }
  else if (ppm->type == 4 || ppm->type == 5 || ppm->type == 6)
    {
      int L;
      unsigned char *out = ppm->rgba;
      unsigned char *end = out + (ppm->width * ppm->height * 4);
      int tick = 0;
      int x = 0;

      while ((L = fread (buf, 1, sizeof(buf)-1, in)) > 0)
        {
          int i;
          if (ppm->type == 4)  /* PBM */
            {
              for (i = 0; i < L; i++)
                {
                  int j;
                  for (j = 7; j >= 0; j--)
                    {
                      int n = ((buf[i] >> j) & 1) ? 255 : 0;
                      out[0] = out[1] = out[2] = n;
                      out[3] = 255;

                      if (x++ >= ppm->width)
                        {
                          /* eol pads to byte boundary */
                          x = 0;
                          break;
                        }
                      out += 4;
                    }
                }
              if (out > end)
                break;
            }
          else if (ppm->type == 5)  /* PGM */
            {
              for (i = 0; i < L; i++)
                {
                  int n = (max == 255 ? buf[i] : ((255 * buf[i]) / max));
                  out[0] = out[1] = out[2] = n;
                  out[3] = 255;
                  out += 4;
                }
              if (out > end)
                break;
            }
          else if (ppm->type == 6)  /* PPM */
            {
              for (i = 0; i < L; i++)
                {
                  int n = (max == 255 ? buf[i] : ((255 * buf[i]) / max));
                  *out++ = n;

                  if (++tick == 3)  /* fill in alpha */
                    {
                      tick = 0;
                      *out++ = 255;
                    }
                }
              if (out > end)
                break;
            }
          else
            abort();
        }
    }
  else
    abort();


  if (!stdin_p)
    fclose (in);

  return ppm;
}



/* Write a PBM, PGM, or PPM file, binary or ASCII, depending on the value
   of the `type' slot.  File name of "-" means stdout.
 */
void
write_ppm (struct ppm *ppm, const char *file)
{
  int stdout_p = !strcmp(file, "-");
  FILE *out;
  unsigned char *s, *end;
  char buf[10240];

  if (stdout_p)
    out = stdout;
  else
    {
      out = fopen (file, "w");
      if (!out)
        {
          iprintf("%.255s", file);
          return;
        }
    }

  fprintf (out, "P%d\n", ppm->type);
  fprintf (out, "%d %d\n", ppm->width, ppm->height);

  if (ppm->type != 1 && ppm->type != 4)
    fprintf (out, "%d\n", 255);

  s = ppm->rgba;
  end = s + (ppm->width * ppm->height * 4);

  if (ppm->type == 1)  /* PBM */
    {
      int col = 0;
      char *o = buf;
      while (s < end)
        {
          int p = (s[0] + s[1] + s[2]) / 3;
          *o++ = (p > 127 ? '1' : '0');
          *o++ = ' ';
          s += 4;
          col += 2;
          if (col >= 68)
            {
              o--;
              *o++ = '\n';
              *o = 0;
              fputs (buf, out);
              o = buf;
              *o = 0;
              col = 0;
            }
        }

      if (o > buf)
        {
          o--;
          *o++ = '\n';
          *o = 0;
          fputs (buf, out);
        }
    }
  else if (ppm->type == 2)  /* PGM */
    {
      int col = 0;
      while (s < end)
        {
          int p = (s[0] + s[1] + s[2]) / 3;
          fprintf (out, "%3d ", p);
          s += 4;
          col += 4; /* close enough */
          if (col >= 66)
            {
              fprintf (out, "\n");
              col = 0;
            }
        }
      if (col != 0)
        fprintf (out, "\n");
    }
  else if (ppm->type == 3)  /* PPM */
    {
      int col = 0;
      while (s < end)
        {
          fprintf (out, "%3d %3d %3d ", s[0], s[1], s[2]);
          s += 4;
          col += 12;  /* close enough */
          if (col >= 60)
            {
              fprintf (out, "\n");
              col = 0;
            }
        }
      if (col != 0)
        fprintf (out, "\n");
    }

  else if (ppm->type == 4)  /* PBM */
    {
      char *obuf = buf;
      char *oend = buf + sizeof(buf) - 10;
      int x = 0;
      while (s < end)
        {
          int byte = 0;
          int i;
          for (i = 7; i >= 0; i--)
            {
              int p = (s[0] + s[1] + s[2]) / 3;
              if (p > 127)
                byte |= (1 << i);
              s += 4;

              if (++x >= ppm->width)
                {
                  /* eol pads to byte boundary */
                  x = 0;
                  break;
                }
            }
          *obuf++ = byte;

          if (obuf >= oend)
            {
              fwrite (buf, 1, obuf-buf, out);
              obuf = buf;
            }
        }
      if (obuf != buf)
        fwrite (buf, 1, obuf-buf, out);
    }
  else if (ppm->type == 5)  /* PGM */
    {
      char *obuf = buf;
      char *oend = buf + sizeof(buf) - 10;
      while (s < end)
        {
          *obuf++ = ((s[0] + s[1] + s[2]) / 3);
          s += 4;
          if (obuf >= oend)
            {
              fwrite (buf, 1, obuf-buf, out);
              obuf = buf;
            }
        }
      if (obuf != buf)
        fwrite (buf, 1, obuf-buf, out);
    }
  else if (ppm->type == 6)  /* PPM */
    {
      char *obuf = buf;
      char *oend = buf + sizeof(buf) - 10;
      while (s < end)
        {
          *obuf++ = *s++;
          *obuf++ = *s++;
          *obuf++ = *s++;
          s++;
          if (obuf >= oend)
            {
              fwrite (buf, 1, obuf-buf, out);
              obuf = buf;
            }
        }
      if (obuf != buf)
        fwrite (buf, 1, obuf-buf, out);
    }
  else
    abort();



  if (!stdout_p)
    fclose (out);
  else
    fflush (out);
}



/* Manipulating image data
 */

struct ppm *
copy_ppm (struct ppm *ppm)
{
  struct ppm *ppm2 = (struct ppm *) calloc (1, sizeof(*ppm));
  int n;
  memcpy (ppm2, ppm, sizeof(*ppm2));
  n = ppm->width * ppm->height * 4;
  ppm2->rgba = (unsigned char *) calloc (1, n);
  if (!ppm2->rgba)
    {
      iprintf ("out of memory (%d x %d)\n",
               ppm->width, ppm->height);
      return NULL;
    }
  memcpy (ppm2->rgba, ppm->rgba, n);
  return ppm2;
}


void
free_ppm (struct ppm *ppm)
{
  memset (ppm->rgba, 0xDE, ppm->width * ppm->height * 4);
  free (ppm->rgba);
  memset (ppm, 0xDE, sizeof(*ppm));
  free (ppm);
}


void
get_pixel (struct ppm *ppm,
           int x, int y,
           unsigned char *rP,
           unsigned char *gP,
           unsigned char *bP,
           unsigned char *aP)
{
  if (x < 0 || x >= ppm->width ||
      y < 0 || y >= ppm->height)
    {
      *rP = *gP = *bP = *aP = 0;
    }
  else
    {
      unsigned char *p = ppm->rgba + (((y * ppm->width) + x) * 4);
      *rP = *p++;
      *gP = *p++;
      *bP = *p++;
      *aP = *p;
    }
}

void
put_pixel (struct ppm *ppm,
           int x, int y,
           unsigned char r,
           unsigned char g,
           unsigned char b,
           unsigned char a)
{
  if (x < 0 || x >= ppm->width ||
      y < 0 || y >= ppm->height ||
      a <= 0)
    {
      /* no-op */
    }
  else
    {
      unsigned char *p = ppm->rgba + (((y * ppm->width) + x) * 4);
      if (a == 255)
        {
          p[0] = r;
          p[1] = g;
          p[2] = b;
          p[3] = a;
        }
      else
        {
          /* dest[xyz] = (old[xyz] * (1-new[a])) + (new[xyz] * new[a])
             dest[a]   = old[a] + (new[a] * (1-old[a]))
           */
          unsigned char a2 = 255 - a;
          p[0] = ((p[0] * a2) + (r * a)) / 255;
          p[1] = ((p[1] * a2) + (g * a)) / 255;
          p[2] = ((p[2] * a2) + (b * a)) / 255;
          p[3] = p[3] + (a * (255 - p[3]) / 255);
        }
    }
}


/* Paste part of one image into another, with all necessary clipping.
   Alpha controls the blending of the new image into the old image
    (and any alpha already in the new image is also taken into account.)
  If override_fg/bg is an RGB value (0xNNNNNN) then any dark/light values
    in the source image are assumed to be that, and only the source image's
    intensity and alpha are used.  (Note that 0 is a color: black.  To not
    specify override_color at all, pass in -1 (or ~0, which is the same
    thing.)
 */
void
paste_ppm (struct ppm *into, int to_x, int to_y,
           struct ppm *from, int from_x, int from_y, int w, int h,
           unsigned int override_fg, unsigned int override_bg,
           int alpha)
{
  int i, j;
  unsigned char ofr, ofg, ofb;
  unsigned char obr, obg, obb;

  if (alpha <= 0)
    return;

  ofr = (override_fg >> 16) & 0xFF;
  ofg = (override_fg >>  8) & 0xFF;
  ofb = (override_fg      ) & 0xFF;
  obr = (override_bg >> 16) & 0xFF;
  obg = (override_bg >>  8) & 0xFF;
  obb = (override_bg      ) & 0xFF;

  override_fg = (override_fg <= 0xFFFFFF);
  override_bg = (override_bg <= 0xFFFFFF);

# define MIDPOINT (3 * 160)  /* around 62% intensity */

  for (j = 0; j < h; j++)
    for (i = 0; i < w; i++)
      {
        unsigned char r, g, b, a;
        get_pixel (from, from_x + i, from_y + j, &r, &g, &b, &a);
        if (alpha < 255) a = a * alpha / 255;

        if (override_fg || override_bg)
          {
            if (r + g + b < MIDPOINT)  /* image's foreground color */
              {
                if (override_fg)
                  {
                    r = 255-((255-r) * (255-ofr) / 255);
                    g = 255-((255-g) * (255-ofg) / 255);
                    b = 255-((255-b) * (255-ofb) / 255);
                  }
              }
            else                    /* image's background color */
              {
                if (override_bg)
                  {
                    r = r * obr / 255;
                    g = g * obg / 255;
                    b = b * obb / 255;
                  }
              }
          }

        put_pixel (into,   to_x + i,   to_y + j,  r,  g,  b,  a);
      }
# undef MIDPOINT
}

/*
 * Draws a PPM image direct to VRAM.  This produces a black-and-white image.
 */
void draw_ppm_bw(struct ppm *ppm, unsigned short *target, 
                 int px, int py, int width,
                 int black, int white)
{
  int x, y;
  long *rgba = (long *)ppm->rgba;

  target += (py * width + px) / 2;

  for (y = py; y < py + ppm->height; y++) {
    for (x = px; x < px + ppm->width; x += 2) {
      if (x & 0x01) {
        *target = (*target & 0xFF00) | (*rgba++ != 0 ? white : black) << 8;
        target++;
        x++;
      } 

      if ((x - px) == (ppm->width - 1)) {
        *target = (*target & 0x00FF) | (*rgba++ != 0 ? white : black);

        break;
      }

      if ((x - px) < ppm->width) {
        *target++ = (rgba[0] != 0 ? white : black) |
                    ((rgba[1] != 0 ? white : black) << 8);

        rgba += 2;
      }
    }

    target += (width - ppm->width) / 2;
  }
}

/* Returns a copy of the PPM, scaled larger or smaller.
   When scaling down, it dithers; when scaling up, it does not.
 */
struct ppm *
scale_ppm (struct ppm *ppm, double scale)
{
  int x, y;
  struct ppm *ppm2;

  if (scale == 1.0) return copy_ppm (ppm);
  if (scale <= 0.0) return 0;

  ppm2 = (struct ppm *) malloc (sizeof(*ppm2));
  memcpy (ppm2, ppm, sizeof(*ppm2));
  ppm2->width  = (ppm->width  + 0.5) * scale;
  ppm2->height = (ppm->height + 0.5) * scale;

  ppm2->rgba = (unsigned char *)
    calloc (1, (ppm2->width) * (ppm2->height) * 4);
  if (!ppm2->rgba)
    {
      iprintf ("out of memory (%d x %d)\n",
               ppm2->width, ppm2->height);
      return NULL;
    }

  if (scale < 1.0)
    for (y = 0; y < ppm2->height; y++)
      for (x = 0; x < ppm2->width; x++)
        {
          int x1 = (x - 0) / scale;
          int x2 = (x + 1) / scale;
          int y1 = (y - 0) / scale;
          int y2 = (y + 1) / scale;
          int xx, yy;
          int tr = 0, tg = 0, tb = 0, ta = 0;
          int total_pixels = (x2-x1) * (y2-y1);

          unsigned char *p = ppm2->rgba + (((y * ppm2->width) + x) * 4);

          if (total_pixels <= 0)
            continue;

          for (yy = y1; yy < y2; yy++)
            for (xx = x1; xx < x2; xx++)
              {
                unsigned char r, g, b, a;
                get_pixel (ppm,
                           (xx < 0 ? 0 : xx),
                           (yy < 0 ? 0 : yy),
                           &r, &g, &b, &a);
                tr += r;
                tg += g;
                tb += b;
                ta += a;
              }

          tr /= total_pixels;
          tg /= total_pixels;
          tb /= total_pixels;
          ta /= total_pixels;

          /* put_pixel would do the wrong thing with alpha here */
          p[0] = tr;
          p[1] = tg;
          p[2] = tb;
          p[3] = ta;
        }

  else     /* scale > 1.0 */

    for (y = 0; y < ppm2->height; y++)
      for (x = 0; x < ppm2->width; x++)
        {
          unsigned char r, g, b, a;
          unsigned char *p = ppm2->rgba + (((y * ppm2->width) + x) * 4);
          get_pixel (ppm,
                     (x + 0.5) / scale,
                     (y + 0.5) / scale,
                     &r, &g, &b, &a);
          /* put_pixel would do the wrong thing with alpha here */
          p[0] = r;
          p[1] = g;
          p[2] = b;
          p[3] = a;
        }

  return ppm2;
}


/* This is just the curve N^2.4 from 0.0-1.0, scaled to 0-256.
   We use this curve to emphasize the blur: it doesn't drop off linearly,
   but decays more sharply at the edges.
 */
static unsigned char blur_alpha_curve [256] = {
    0,   3,   5,   8,  10,  12,  15,  17,  19,  21,  24,  26,  28,  30,
   33,  35,  37,  39,  41,  44,  46,  48,  50,  52,  54,  56,  58,  60,
   62,  64,  66,  68,  70,  72,  74,  76,  78,  80,  82,  84,  86,  88,
   90,  91,  93,  95,  97,  99, 101, 102, 104, 106, 108, 109, 111, 113,
  114, 116, 118, 120, 121, 123, 124, 126, 128, 129, 131, 132, 134, 135,
  137, 139, 140, 142, 143, 145, 146, 147, 149, 150, 152, 153, 155, 156,
  157, 159, 160, 161, 163, 164, 165, 167, 168, 169, 170, 172, 173, 174,
  175, 177, 178, 179, 180, 181, 183, 184, 185, 186, 187, 188, 189, 190,
  191, 192, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 203, 204,
  205, 206, 207, 208, 209, 210, 211, 212, 212, 213, 214, 215, 216, 217,
  217, 218, 219, 220, 220, 221, 222, 223, 223, 224, 225, 225, 226, 227,
  227, 228, 229, 229, 230, 231, 231, 232, 232, 233, 234, 234, 235, 235,
  236, 236, 237, 237, 238, 238, 239, 239, 240, 240, 241, 241, 242, 242,
  243, 243, 243, 244, 244, 245, 245, 245, 246, 246, 246, 247, 247, 247,
  248, 248, 248, 249, 249, 249, 249, 250, 250, 250, 250, 251, 251, 251,
  251, 252, 252, 252, 252, 252, 253, 253, 253, 253, 253, 253, 253, 254,
  254, 254, 254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255
};


/* Returns a copy of the PPM, blurred out.
   The PPM will be enlarged in both directions by 2*radius.
 */
struct ppm *
blur_ppm (struct ppm *ppm, int radius)
{
  int x, y;
  struct ppm *ppm2;
  int x1, x2, y1, y2;
  int total_pixels;
  int maxx, maxy;

  if (radius == 0) return copy_ppm (ppm);
  if (radius < 0) radius = -radius;

  ppm2 = (struct ppm *) malloc (sizeof(*ppm2));
  memcpy (ppm2, ppm, sizeof(*ppm2));
  ppm2->width  = (ppm->width  + (radius * 2));
  ppm2->height = (ppm->height + (radius * 2));

  ppm2->rgba = (unsigned char *)
    calloc (1, (ppm2->width) * (ppm2->height) * 4);

  if (!ppm2->rgba)
    {
      iprintf ("out of memory (%d x %d)\n",
               ppm2->width, ppm2->height);
      return NULL;
    }


  total_pixels = (radius * 2) * (radius * 2);
  maxx = ppm->width  - 1;
  maxy = ppm->height - 1;

  y1 = -radius * 2;
  y2 = 0;
  for (y = 0; y < ppm2->height; y++, y1++, y2++)
    {
      unsigned char *p = ppm2->rgba + ((y * ppm2->width) * 4);
      x1 = -radius * 2;
      x2 = 0;

      for (x = 0; x < ppm2->width; x++, x1++, x2++, p += 4)
        {
          int xx, yy;
          unsigned int tr = 0, tg = 0, tb = 0, ta = 0;

          for (yy = y1; yy < y2; yy++)
            for (xx = x1; xx < x2; xx++)
              {
                unsigned char r, g, b, a;
                if (xx >= 0 && xx <= maxx &&
                    yy >= 0 && yy <= maxy)
                  get_pixel (ppm, xx, yy, &r, &g, &b, &a);
                else
                  r = g = b = a = 0;
                tr += r;
                tg += g;
                tb += b;
                ta += a;
              }

          tr /= total_pixels;
          tg /= total_pixels;
          tb /= total_pixels;
          ta /= total_pixels;

          if (ta > 255) abort();
          ta = blur_alpha_curve[ta];

          /* put_pixel would do the wrong thing with alpha here */
          p[0] = tr;
          p[1] = tg;
          p[2] = tb;
          p[3] = ta;
        }
    }

  return ppm2;
}
