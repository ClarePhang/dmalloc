/*
 * local definitions for the user allocation level
 *
 * Copyright 1992 by Gray Watson and the Antaire Corporation
 *
 * This file is part of the malloc-debug package.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library (see COPYING-LIB); if not, write to the
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * The author of the program may be contacted at gray.watson@antaire.com
 *
 * $Id: dmalloc_loc.h,v 1.18 1993/04/30 20:02:48 gray Exp $
 */

#ifndef __MALLOC_LOC_H__
#define __MALLOC_LOC_H__

#include "conf.h"				/* for HAVE_BCMP */

/* fence post checking defines */
#define FENCE_BOTTOM		(1 << ALLOCATION_ALIGNMENT_IN_BITS)
#define FENCE_TOP		sizeof(long)
#define FENCE_OVERHEAD		(FENCE_BOTTOM + FENCE_TOP)
#define FENCE_MAGIC_BASE	0xC0C0AB1B
#define FENCE_MAGIC_TOP		0xFACADE69

/*
 * env variables
 */
#define ADDRESS_ENVIRON		"MALLOC_ADDRESS"
#define DEBUG_ENVIRON		"MALLOC_DEBUG"
#define INTERVAL_ENVIRON	"MALLOC_INTERVAL"
#define LOGFILE_ENVIRON		"MALLOC_LOGFILE"
#define START_ENVIRON		"MALLOC_START"

/******************************* useful defines ******************************/

/*
 * global variable and procedure scoping for code readability
 */
#undef	EXPORT
#define	EXPORT

#undef	IMPORT
#define	IMPORT		extern

#undef	LOCAL
#define	LOCAL		static

/*
 * standard int return codes
 */
#undef	ERROR
#define	ERROR		(-1)

#undef	NOERROR
#define	NOERROR		0

/*
 * generic constants
 */
#undef	NULL
#define NULL		0

#undef	NULLC
#define NULLC		'\0'

#undef	FALSE
#define FALSE		0

#undef	TRUE
#define TRUE		(! FALSE)

/*
 * standard i/o file descriptors
 */
#undef	STDIN
#define	STDIN		0		/* fileno(stdin) */

#undef	STDOUT
#define	STDOUT		1		/* fileno(stdout) */

#undef	STDERR
#define	STDERR		2		/* fileno(stderr) */

/*
 * min/max macros
 *
 * WARNING: these use their arguments multiple times which may be bad
 */
#undef MAX
#define MAX(a,b)	(((a) > (b)) ? (a) : (b))
#undef MIN
#define MIN(a,b)	(((a) < (b)) ? (a) : (b))

/*
 * bitflag tools for Variable and a Flag
 */
#undef BIT_FLAG
#define BIT_FLAG(x)		(1 << (x))
#undef BIT_SET
#define BIT_SET(v,f)		(v) |= (f)
#undef BIT_CLEAR
#define BIT_CLEAR(v,f)		(v) &= ~(f)
#undef BIT_IS_SET
#define BIT_IS_SET(v,f)		((v) & (f))

/*
 * some defines to standardize memory functions
 */
#if HAVE_BCMP == 0
#define bcmp(s1, s2, len)	memcmp((char *)(s1), (char *)(s2), (len))
#endif

#if HAVE_BCOPY == 0
#define bcopy(from, to, len)	(void)memcpy((char *)(to), (char *)(from), len)
#endif

#endif /* ! __MALLOC_LOC_H__ */
