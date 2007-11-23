/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*-
   ppm-lite.h --- simple, self-contained code for manipulating
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

#ifndef __PPM_LITE__
#define __PPM_LITE__

extern const char *progname;

struct ppm {
  int type;                     /* 1-6 */
  int width, height;
  unsigned char *rgba;          /* 32 bpp, even for PBM and PGM data. */
};

/* Filenames below may be "-" for stdin/stdout. */
extern struct ppm *alloc_ppm(int width, int height);

extern void clear_ppm(struct ppm *ppm);

extern struct ppm *read_ppm (const char *filename);
extern void write_ppm (struct ppm *ppm, const char *filename);

extern struct ppm *copy_ppm (struct ppm *ppm);
extern void free_ppm (struct ppm *ppm);

extern void get_pixel (struct ppm *ppm,
                       int x, int y,    /* out of range is ok */
                       unsigned char *r_ret,
                       unsigned char *g_ret,
                       unsigned char *b_ret,
                       unsigned char *a_ret);
extern void put_pixel (struct ppm *ppm,
                       int x, int y,    /* out of range is ok */
                       unsigned char r,
                       unsigned char g,
                       unsigned char b,
                       unsigned char a);

/* Paste part of one image into another, with all necessary clipping.
   Alpha controls the blending of the new image into the old image
    (and any alpha already in the new image is also taken into account.)
  If override_color is an RGB value (0xNNNNNN) then any color values in
    the source image are assumed to be that, and only the source image's
    alpha is used.  (Note that 0 is a color: black.  To not specify
    override_color at all, pass in -1 (or ~0, which is the same thing.)
 */
extern void paste_ppm (struct ppm *into, int to_x, int to_y,
                       struct ppm *from, int from_x, int from_y, int w, int h,
                       unsigned int override_fg, unsigned int override_bg,
                       int alpha);

/*
 * Draws a B&W version of a font to the specified memory location, assumed
 * to be a paletted VRAM location.  The 'black' and 'white' parameters specify
 * the palette indexes to use for the non-coloured and coloured areas,
 * respectively.
 */
extern void draw_ppm_bw(struct ppm *ppm, unsigned short *target, 
                        int px, int py, int width,
                        int black, int white);

/* Returns a copy of the PPM, scaled larger or smaller.
   When scaling down, it dithers; when scaling up, it does not.
 */
extern struct ppm *scale_ppm (struct ppm *ppm, double scale);

/* Returns a copy of the PPM, blurred out.
   The PPM will be enlarged in both directions by 2*radius.
 */
extern struct ppm *blur_ppm (struct ppm *ppm, int radius);

#endif /* __PPM_LITE__ */
