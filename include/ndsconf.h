/*	SCCS Id: @(#)ndsconf.h	3.4	2006/04/14	*/
/* Copyright (c) Stuart Pernsteiner 2006.		*/
/* NetHack may be freely redistributed.  See license for details. */

#ifndef NDSCONF_H
#define NDSCONF_H

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "nds_io.h"
#include "onames.h"

#ifdef SCR_MAIL
# define MAIL
#endif

#define NO_SIGNAL
#define NEED_VARARGS
#define USE_STDARG
#define PATHLEN		BUFSZ	/* maximum pathlength */
#define FILENAME	BUFSZ	/* maximum filename length (conservative) */
#define TRADITIONAL_GLYPHMAP	/* Store glyph mappings at level change time */
#define PC_LOCKING		/* Prevent overwrites of aborted or in-progress games */
				/* without first receiving confirmation. */
#define SELF_RECOVER		/* Allow the game itself to recover from an aborted game */
#define FCMASK	0660		/* file creation mask */
#define STRNCMPI
#define GCC_WARN
#define Rand()	rand()
#define NEARDATA

#define HACKDIR		"/NetHack"

#define NO_TERMS
#define ASCIIGRAPH
#define TEXTCOLOR		/* Color text */

#endif
