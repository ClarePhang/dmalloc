/*
 * Local defines for the low level memory routines
 *
 * Copyright 2000 by Gray Watson
 *
 * This file is part of the dmalloc package.
 *
 * Permission to use, copy, modify, and distribute this software for
 * any purpose and without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies, and that the name of Gray Watson not be used in advertising
 * or publicity pertaining to distribution of the document or software
 * without specific, written prior permission.
 *
 * Gray Watson makes no representations about the suitability of the
 * software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be contacted via http://dmalloc.com/
 *
 * $Id: chunk_loc.h,v 1.66 2003/05/13 18:16:33 gray Exp $
 */

#ifndef __CHUNK_LOC_H__
#define __CHUNK_LOC_H__

#include "conf.h"				/* up here for _INCLUDE */
#include "dmalloc_loc.h"			/* for DMALLOC_SIZE */

/* for thread-id types -- see conf.h */
#if LOG_THREAD_ID
#ifdef THREAD_INCLUDE
#include THREAD_INCLUDE
#endif
#endif

/* for time type -- see settings.h */
#if STORE_TIMEVAL
# ifdef TIMEVAL_INCLUDE
#  include TIMEVAL_INCLUDE
# endif
#else
# if STORE_TIME
#  ifdef TIME_INCLUDE
#   include TIME_INCLUDE
#  endif
# endif
#endif

/* log-bad-space info */
#define SPECIAL_CHARS		"\"\"''\\\\n\nr\rt\tb\bf\fa\007"

/*
 * Maximum level in the skip list.  This implies that we can only
 * store 2^32 entries optimally.  Needless to say this is plenty.
 */
#define MAX_SKIP_LEVEL		32

/* pointer to the start of the block which holds PNT */
#define BLOCK_NUM_TO_PNT(pnt)	(((long)(pnt) / BLOCK_SIZE) * BLOCK_SIZE)

/* adjust internal PNT to user-space */
#define CHUNK_TO_USER(pnt, f_b)	\
	((f_b) ? (char *)(pnt) + FENCE_BOTTOM_SIZE : (pnt))
#define USER_TO_CHUNK(pnt, f_b)	\
	((f_b) ? (char *)(pnt) - FENCE_BOTTOM_SIZE : (pnt))

/* get the number of blocks to hold SIZE */
#define NUM_BLOCKS(size)	(((size) + (BLOCK_SIZE - 1)) / BLOCK_SIZE)

/* NOTE: FENCE_BOTTOM_SIZE and FENCE_TOP_SIZE defined in settings.h */
#define FENCE_OVERHEAD_SIZE	(FENCE_BOTTOM_SIZE + FENCE_TOP_SIZE)
#define FENCE_MAGIC_BOTTOM	0xC0C0AB1B
#define FENCE_MAGIC_TOP		0xFACADE69
/* type of the fence magic values */
#define FENCE_MAGIC_TYPE	int
/* smallest allocated block */
#define CHUNK_SMALLEST_BLOCK	\
   (FENCE_BOTTOM_SIZE + DEFAULT_SMALLEST_ALLOCATION)

#define CHUNK_MAGIC_BOTTOM	0xDEA007	/* bottom magic number */
#define CHUNK_MAGIC_TOP		0x976DEAD	/* top magic number */

/* flags associated with the skip_alloc_t type */
#define ALLOC_FLAG_USER		BIT_FLAG(0)	/* slot is user allocated */
#define ALLOC_FLAG_FREE		BIT_FLAG(1)	/* slot is free */
#define ALLOC_FLAG_EXTERN	BIT_FLAG(2)	/* slot allocated externally */
#define ALLOC_FLAG_ADMIN	BIT_FLAG(3)	/* administrative space */

#define ALLOC_FLAG_FENCE	BIT_FLAG(10)	/* slot is fence posted */
#define ALLOC_FLAG_VALLOC	BIT_FLAG(11)	/* slot is block aligned */

/*
 * Below defines an allocation structure either on the free or used
 * list.  It tracks allocations that fit in partial, one, or many
 * basic-blocks.  It stores some optional fields for recording
 * information about the pointer.
 */
typedef struct skip_alloc_st {
  
  unsigned int		sa_flags;	/* what it is */
  unsigned int		sa_user_size;	/* size requested by user (wo fence) */
  unsigned int		sa_total_size;	/* total size of the block */
  
  void			*sa_mem;	/* pointer to the memory in question */
  const char		*sa_file;	/* .c filename where alloced */
  unsigned int		sa_line;	/* line where it was alloced */
  unsigned long		sa_use_iter;	/* when last ``used'' */
  
#if STORE_SEEN_COUNT
  unsigned long		sa_seen_c;	/* times pointer was seen */
#endif
#if STORE_ITERATION_COUNT
  unsigned long		sa_iteration;	/* interation when pointer alloced */
#endif
#if STORE_TIMEVAL
  TIMEVAL_TYPE 		sa_timeval;	/* time when pointer alloced */
#else
#if STORE_TIME
  TIME_TYPE		sa_time;	/* time when pointer alloced */
#endif
#endif
#if LOG_THREAD_ID
  THREAD_TYPE		sa_thread_id;	/* thread id which allocaed pnt */
#endif
  
  /*
   * Array of next pointers.  This may extend past the end of the
   * function if we allocate for space larger than the structure.
   */
  unsigned int		sa_level_n;
  struct skip_alloc_st	*sa_next_p[1];
  
} skip_alloc_t;

/*
 * This macro helps us determine how much memory we need to store to
 * hold all of the next pointers in the skip-list entry.  So if we are
 * at level 0 then this will have no extra next pointers since there
 * already is one inside of skip_alloc_t.
 */
#define SKIP_SLOT_SIZE(next_n)	\
	(sizeof(skip_alloc_t) + sizeof(skip_alloc_t *) * (next_n))

/* entry block magic numbers */
#define ENTRY_BLOCK_MAGIC1	0xEBEB1111	/* for the eb_magic1 field */
#define ENTRY_BLOCK_MAGIC2	0xEBEB2222	/* for the eb_magic2 field */
#define ENTRY_BLOCK_MAGIC3	0xEBEB3333	/* written at end of eb block*/

/*
 * The following structure is written at the front of a skip-list
 * entry administrative block.  
 */
typedef struct entry_block_st {
  unsigned int		eb_magic1;	/* magic number */
  unsigned int		eb_level_n;	/* the levels which are stored here */
  struct entry_block_st	*eb_next_p;	/* pointer to next block */
  unsigned int		eb_magic2;	/* magic number */
  
  skip_alloc_t		eb_first_slot;	/* first slot in the block */
  
  /*
   * the rest are after this one but we don't really know the size
   * because it is based on the skip-level.
   */
  
  /*
   * At the end of the block is the MAGIC3 value but we can't define
   * it in a structure.
   */
} entry_block_t;

/*
 * The following structure is used to figure out a number of bits of
 * information about a user allocation.
 */
typedef struct {
  int		pi_fence_b;		/* fence-posts are on for pointer */
  int		pi_valloc_b;		/* pointer is valloc-aligned */
  void		*pi_alloc_start;	/* pnt to start of allocation */
  void		*pi_fence_bottom;	/* pnt to the bottom fence area */
  void		*pi_user_start;		/* pnt to start of user allocation */
  void		*pi_user_bounds;	/* pnt past end of user allocation */
  void		*pi_fence_top;		/* pnt to the top fence area */
  void		*pi_upper_bounds;	/* pnt to highest available user area*/
  void		*pi_alloc_bounds;	/* pnt past end of total allocation */
} pnt_info_t;

#endif /* ! __CHUNK_LOC_H__ */
