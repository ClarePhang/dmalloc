/*
 * defines for the Malloc module
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
 * $Id: malloc.h,v 1.15 1993/02/12 04:50:06 gray Exp $
 */

#ifndef __MALLOC_H__
#define __MALLOC_H__

#include "malloc_lp.h"				/* leap-frog routines */

/*
 * malloc function return codes
 */
#define CALLOC_ERROR		0		/* error from calloc */
#define MALLOC_ERROR		0		/* error from malloc */
#define REALLOC_ERROR		0		/* error from realloc */

#define FREE_ERROR		0		/* error from free */
#define FREE_NOERROR		1		/* no error from free */

#define MALLOC_VERIFY_ERROR	0		/* checks failed, error */
#define MALLOC_VERIFY_NOERROR	1		/* checks passed, no error */

/*
 * default values if _malloc_file and _malloc_line are not set
 */
#define MALLOC_DEFAULT_FILE	"unknown"
#define MALLOC_DEFAULT_LINE	0

/*
 * global variable and procedure scoping for code readability
 */
#undef	EXPORT
#define	EXPORT

#undef	IMPORT
#define	IMPORT		extern

#undef	LOCAL
#define	LOCAL		static

#if __GNUC__ < 2
/*
 * prototype for memory copy.  needed for below macros.
 */
IMPORT	char	*memcpy(char * to, char * from, int length);
#endif

/*
 * memory copy: copy SIZE bytes from pointer FROM to pointer TO
 */
#define MEMORY_COPY(from, to, size)	(void)memcpy((char *)to, \
						     (char *)from, size)

/*
 * alloc macros to provide for memory debugging features.
 */
#undef ALLOC
#undef CALLOC
#undef MALLOC
#undef REALLOC
#undef REMALLOC
#undef FREE

#ifndef MALLOC_DEBUG_DISABLE

#define ALLOC(type, count) \
  (type *)_malloc_leap(__FILE__, __LINE__, \
		       (unsigned int)(sizeof(type) * (count)))

#define MALLOC(size) \
  (char *)_malloc_leap(__FILE__, __LINE__, (unsigned int)(size))

/* WARNING: notice that the arguments are REVERSED from normal calloc() */
#define CALLOC(type, count) \
  (type *)_calloc_leap(__FILE__, __LINE__, (unsigned int)(count), \
		       (unsigned int)sizeof(type))

#define REALLOC(ptr, type, count) \
  (type *)_realloc_leap(__FILE__, __LINE__, (char *)(ptr), \
			(unsigned int)(sizeof(type) * (count)))

#define REMALLOC(ptr, size) \
  (char *)_realloc_leap(__FILE__, __LINE__, (char *)(ptr), \
			(unsigned int)(size))

#define FREE(ptr) \
  _free_leap(__FILE__, __LINE__, (char *)(ptr))

#else /* MALLOC_DEBUG_DISABLE */

#define ALLOC(type, count) \
  (type *)malloc((unsigned int)(sizeof(type) * (count)))

#define MALLOC(size) \
  (char *)malloc((unsigned int)(size))

/* WARNING: notice that the arguments are REVERSED from normal calloc() */
#define CALLOC(type, count) \
  (type *)calloc((unsigned int)(count), (unsigned int)sizeof(type))

#define REALLOC(ptr, type, count) \
  (type *)realloc((char *)(ptr), (unsigned int)(sizeof(type) * (count)))

#define REMALLOC(ptr, size) \
  (char *)realloc((char *)(ptr), (unsigned int)(size))

#define FREE(ptr) \
  free((char *)(ptr))

#endif /* MALLOC_DEBUG_DISABLE */

/*
 * some small allocation macros
 */

#ifdef __GNUC__

/*
 * duplicate BUF of SIZE bytes
 */
#define BDUP(buf, size)	({ \
			  char	*_ret; \
			  int	_size = (size); \
			   \
			  _ret = MALLOC(_size); \
			  if (_ret != NULL) \
			    MEMORY_COPY((buf), _ret, _size); \
			   \
			  _ret; \
			})

/*
 * the strdup() function in macro form.  duplicate string STR
 */
#define STRDUP(str)	({ \
			  const char *_strp = (str); \
			  char	*_ret; \
			  int	_len; \
			   \
			  _len = strlen(_strp); \
			  _ret = MALLOC(_len + 1); \
			  if (_ret != NULL) \
			    MEMORY_COPY(_strp, _ret, _len + 1); \
			   \
			  _ret; \
			})

#else /* ! __GNUC__ */

/*
 * duplicate BUF of SIZE and return the new address in OUT
 */
#define BDUP(buf, size, out)	do { \
				  char	*_ret; \
				  int	_size = (size); \
				   \
				  _ret = MALLOC(_size); \
				  if (_ret != NULL) \
				    MEMORY_COPY((buf), _ret, _size); \
				   \
				  (out) = _ret; \
				} while(0)

/*
 * strdup() in macro form.  duplicate string STR and return a copy in OUT
 */
#define STRDUP(str, out)	do { \
				  const char *_strp = (str); \
				  char	*_ret; \
				  int	_len; \
				   \
				  _len = strlen(_strp); \
				  _ret = MALLOC(_len + 1); \
				  if (_ret != NULL) \
				    MEMORY_COPY(_strp, _ret, _len + 1); \
				   \
				  (out) = _ret; \
				} while(0)

#endif /* ! __GNUC__ */

/*<<<<<<<<<<  The below prototypes are auto-generated by fillproto */

/* logfile for dumping malloc info, MALLOC_LOGFILE env. var overrides this */
IMPORT	char		*malloc_logpath;

/* internal malloc error number for reference purposes only */
IMPORT	int		malloc_errno;

/*
 * shutdown memory-allocation module, provide statistics if necessary
 */
IMPORT	void	malloc_shutdown(void);

/*
 * allocate and return a SIZE block of bytes
 */
IMPORT	char	*malloc(unsigned int size);

/*
 * allocate and return a block of bytes able to hold NUM_ELEMENTS of elements
 * of SIZE bytes and zero the block
 */
IMPORT	char	*calloc(unsigned int num_elements, unsigned int size);

/*
 * resizes OLD_PNT to SIZE bytes and return the new space after either copying
 * all of OLD_PNT to the new area or truncating
 */
IMPORT	char	*realloc(char * old_pnt, unsigned int new_size);

/*
 * release PNT in the heap, returning FREE_[NO]ERROR
 */
IMPORT	int	free(char * pnt);

/*
 * call through to _heap_map function, returns [NO]ERROR
 */
IMPORT	int	malloc_heap_map(void);

/*
 * verify pointer PNT or if it equals 0, the entire heap.
 * returns MALLOC_VERIFY_[NO]ERROR
 */
IMPORT	int	malloc_verify(char * pnt);

/*
 * set the global debug functionality flags to DEBUG.
 * returns [NO]ERROR
 */
IMPORT	int	malloc_debug(long debug);

/*
 * examine pointer PNT and returns SIZE, and FILE / LINE info on it
 * if any of the pointers are not NULL.
 * returns NOERROR or ERROR depending on whether PNT is good or not
 */
IMPORT	int	malloc_examine(char * pnt, unsigned int * size,
			       char ** file, unsigned int * line);

/*
 * malloc version of strerror to return the string version of ERRNUM
 * returns the string for MALLOC_BAD_ERRNO if ERRNUM is out-of-range.
 */
IMPORT	char	*malloc_strerror(int errnum);

/*<<<<<<<<<<   This is end of the auto-generated output from fillproto. */

#endif /* ! __MALLOC_H__ */
