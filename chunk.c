/*
 * Memory chunk low-level allocation routines
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
 * $Id: chunk.c,v 1.177 2003/05/13 16:38:07 gray Exp $
 */

/*
 * This file contains algorithm level routine for the heap.  They handle the
 * manipulation and administration of chunks of memory.
 */

#include <ctype.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif

#define DMALLOC_DISABLE

#include "conf.h"

#if STORE_TIMEVAL
#ifdef TIMEVAL_INCLUDE
# include TIMEVAL_INCLUDE
#endif
#else
# if STORE_TIME
#  ifdef TIME_INCLUDE
#   include TIME_INCLUDE
#  endif
# endif
#endif

#include "dmalloc.h"

#include "chunk.h"
#include "chunk_loc.h"
#include "compat.h"
#include "debug_val.h"
#include "dmalloc_loc.h"
#include "dmalloc_tab.h"
#include "error.h"
#include "error_val.h"
#include "heap.h"
#include "protect.h"

#if INCLUDE_RCS_IDS
#if IDENT_WORKS
#ident "@(#) $Id: chunk.c,v 1.177 2003/05/13 16:38:07 gray Exp $"
#else
static	char	*rcs_id =
  "@(#) $Id: chunk.c,v 1.177 2003/05/13 16:38:07 gray Exp $";
#endif
#endif

/*
 * Library Copyright and URL information for ident and what programs
 */
#if IDENT_WORKS
#ident "@(#) $Copyright: Dmalloc package Copyright 2000 by Gray Watson $"
#ident "@(#) $URL: Source for dmalloc available from http://dmalloc.com/ $"
#else
static	char	*copyright =
  "@(#) $Copyright: Dmalloc package Copyright 2000 by Gray Watson $";
static	char	*source_url =
  "@(#) $URL: Source for dmalloc available from http://dmalloc.com/ $";
#endif

#if LOCK_THREADS
#if IDENT_WORKS
#ident "@(#) $Information: lock-threads is enabled $"
#else
static char *information = "@(#) $Information: lock-threads is enabled $";
#endif
#endif

/*
 * local variables
 */

/*
 * Skip list of our free list sorted by size in bytes.  Bit of a hack
 * here.  Basically we cannot do a alloc for the structure and we'd
 * like it to be static storage so we allocate an array of them to
 * make sure we have enough forward pointers, when all we need is
 * SKIP_SLOT_SIZE(MAX_SKIP_LEVEL + 1) bytes.
 */
static	skip_alloc_t	skip_free_alloc[MAX_SKIP_LEVEL /* read note ^^ */];
static	skip_alloc_t	*skip_free_list = skip_free_alloc;

/* skip list of all of our allocated or free blocks by address */
static	skip_alloc_t	skip_address_alloc[MAX_SKIP_LEVEL /* read note ^^ */];
static	skip_alloc_t	*skip_address_list = skip_address_alloc;

/* update slots which we use to update the skip lists */
static	skip_alloc_t	skip_update[MAX_SKIP_LEVEL /* read note ^^ */];

/* linked list of slots of various sizes */
static	skip_alloc_t	*entry_free_list[MAX_SKIP_LEVEL];
/* linked list of blocks of the sizes */
static	entry_block_t	*entry_blocks[MAX_SKIP_LEVEL];

/* administrative structures */
static	char		fence_bottom[FENCE_BOTTOM_SIZE];
static	char		fence_top[FENCE_TOP_SIZE];
static	int		bit_sizes[BASIC_BLOCK];	/* number bits for sub-block */

/* memory stats */
static	unsigned long	alloc_current = 0;	/* current memory usage */
static	unsigned long	alloc_maximum = 0;	/* maximum memory usage  */
static	unsigned long	alloc_cur_given = 0;	/* current mem given */
static	unsigned long	alloc_max_given = 0;	/* maximum mem given  */
static	unsigned long	alloc_total = 0;	/* total allocation */
static	unsigned long	alloc_one_max = 0;	/* maximum at once */
static	unsigned long	free_space_bytes = 0;	/* count the free bytes */

/* pointer stats */
static	unsigned long	alloc_cur_pnts = 0;	/* current pointers */
static	unsigned long	alloc_max_pnts = 0;	/* maximum pointers */
static	unsigned long	alloc_tot_pnts = 0;	/* current pointers */

/* admin counts */
static	unsigned long	heap_check_c = 0;	/* count of heap-checks */
static	unsigned long	user_block_c = 0;	/* count of blocks */
static	unsigned long	admin_block_c = 0;	/* count of admin blocks */
static	unsigned long	extern_block_c = 0;	/* count of extern blocks */

/* alloc counts */
static	unsigned long	func_malloc_c = 0;	/* count the mallocs */
static	unsigned long	func_calloc_c = 0;	/* # callocs, done in alloc */
static	unsigned long	func_realloc_c = 0;	/* count the reallocs */
static	unsigned long	func_recalloc_c = 0;	/* count the reallocs */
static	unsigned long	func_memalign_c = 0;	/* count the memaligns */
static	unsigned long	func_valloc_c = 0;	/* count the veallocs */
static	unsigned long	func_free_c = 0;	/* count the frees */

/**************************** skip list routines *****************************/

/*
 * static int random_level
 *
 * DESCRIPTION:
 *
 * Return a random level to be associated with a new free-list entry.
 *
 * RETURNS:
 *
 * Random level from 0 to max_level - 1.
 *
 * ARGUMENTS:
 *
 * max_level -> Maximum level of the free-list.
 */
static	int	random_level(const int max_level)
{
  int	level_c;
  
  for (level_c = 0; level_c < max_level; level_c++) {
    /*
     * Basically we count the number of times that the random number
     * generator returns an odd number in a row.  On average this
     * should return 0 1/2 the time, 1 1/4 of the time, 2 1/8 of a
     * time, and N 1/(2^(N - 1)) of the time.  This is what we want.
     * We could test for this in the configure scripts.
     *
     * Since many machines return random numbers which aren't that
     * random, there may be better ways of doing this.  In the past I
     * had (random() % 10000 >= 5000) or something but I'd rather not
     * have the % overhead here.
     */
    if (random() & 1) {
      break;
    }
  }
  
  return level_c;
}

/*
 * static skip_alloc_t *find_address
 *
 * DESCRIPTION:
 *
 * Look for a specific address in the skip list.  If it exist then a
 * pointer to the matching slot is returned otherwise NULL.  Either
 * way, the links that were traversed to get there are set in the
 * update slot which has the maximum number of levels.
 *
 * RETURNS:
 *
 * Success - Pointer to the slot which matches the block-num and size
 * pair.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * address -> Address we are looking for.
 *
 * exact_b -> Set to 1 to find the exact pointer.  If 0 then the
 * address could be inside a block.
 *
 * update_p -> Pointer to the skip_alloc entry we are using to hold
 * the update pointers.
 */
static	skip_alloc_t	*find_address(const void *address, const int exact_b,
				      skip_alloc_t *update_p)
{
  int		level_c;
  skip_alloc_t 	*slot_p, *found_p = NULL, *next_p;
  
  /* skip_address_max_level */
  level_c = MAX_SKIP_LEVEL - 1;
  slot_p = skip_address_list;
  
  /* traverse list to smallest entry */
  while (1) {
    
    /* next on we are looking for */
    next_p = slot_p->sa_next_p[level_c];
    
    /*
     * sort by address
     */
    
    /* are we are at the end of a row? */
    if (next_p == NULL
	|| next_p == found_p
	|| (char *)next_p->sa_mem > (char *)address) {
      /* just go down a level */
    }
    else if ((char *)next_p->sa_mem == (char *)address) {
      /* found it and go down a level */
      found_p = next_p;
    }
    /*
     * (char *)next_p->sa_mem < (char *)address
     */
    else if ((! exact_b)
	     && ((char *)next_p->sa_mem + next_p->sa_user_size >
		 (char *)address)) {
      /*
       * if we are doing loose searches and this block contains this
       * pointer then we have a match
       */
      found_p = next_p;
    }
    else {
      /* next slot is less, go right */
      slot_p = next_p;
      continue;
    }
    
    /* we are lowering the level */
    
    update_p->sa_next_p[level_c] = slot_p;
    if (level_c == 0) {
      break;
    }
    level_c--;
  }
  
  return found_p;
}

/*
 * static skip_alloc_t *find_free_size
 *
 * DESCRIPTION:
 *
 * Look for a specific size in the free skip list.  If it exist then a
 * pointer to the matching slot is returned otherwise NULL.  Either
 * way, the links that were traversed to get there are set in the
 * update slot which has the maximum number of levels.
 *
 * RETURNS:
 *
 * Success - Pointer to the slot which matches the size pair.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * address -> Address we are looking for.
 *
 * update_p -> Pointer to the skip_alloc entry we are using to hold
 * the update pointers.
 */
static	skip_alloc_t	*find_free_size(const unsigned int size,
					skip_alloc_t *update_p)
{
  int		level_c, cmp;
  skip_alloc_t 	*slot_p, *found_p = NULL, *next_p;
  
  /* skip_free_max_level */
  level_c = MAX_SKIP_LEVEL - 1;
  slot_p = skip_free_list;
  
  /* traverse list to smallest entry */
  while (1) {
    
    /* next on we are looking for */
    next_p = slot_p->sa_next_p[level_c];
    
    /* are we are at the end of a row? */
    if (next_p == NULL
	|| next_p == found_p) {
      /* just go down a level */
    }
    else {
      cmp = next_p->sa_total_size - size;
      if (cmp < 0) {
	/* next slot is less, go right */
	slot_p = next_p;
	continue;
      }
      else if (cmp == 0) {
	/*
	 * we found a match but it may not be the first slot with this
	 * size and we want the first match
	 */
	found_p = next_p;
      }
    }
    
    /* we are lowering the level */
    
    update_p->sa_next_p[level_c] = slot_p;
    if (level_c == 0) {
      break;
    }
    level_c--;
  }
  
  /* space should be free */
  if (found_p != NULL && (! BIT_IS_SET(found_p->sa_flags, ALLOC_FLAG_FREE))) {
    dmalloc_errno = ERROR_ADDRESS_LIST;
    dmalloc_error("find_free_size");
    return NULL;
  }
  
  return found_p;
}

/*
 * static int insert_slot
 *
 * DESCRIPTION:
 *
 * Insert an address entry into a skip list.
 *
 * RETURNS:
 *
 * Success - 1
 *
 * Failure - 0
 *
 * ARGUMENTS:
 *
 * slot_p <-> Slot that we are inserting into the skip list.
 *
 * free_b -> Insert a free address in the free-size list otherwise it
 * will go into the used address list.
 */
static	int	insert_slot(skip_alloc_t *slot_p, const int free_b)
{
  skip_alloc_t	*adjust_p, *update_p;
  int		level_c;
  
  update_p = skip_update;
  
  if (free_b) {
    (void)find_free_size(slot_p->sa_total_size, update_p);
    /*
     * NOTE: we can get a new_p because there might be other blocks of
     * the same size which we will be inserting before.
     */
  }
  else if (find_address(slot_p->sa_mem, 1 /* exact */, update_p) != NULL) {
    /*
     * we should not have found it since that means that someone has
     * the same size and block-num
     */
    dmalloc_errno = ERROR_ADDRESS_LIST;
    dmalloc_error("insert_slot");
    return 0;
  }
  
  /* update the block skip list */
  for (level_c = 0; level_c <= slot_p->sa_level_n; level_c++) {
    /*
     * We are inserting our new slot after each of the slots in the
     * update array.  So for each level, we get the slot we are
     * adjusting, we take it's next pointers and set them in the new
     * slot, and we point its next pointers to the new slot.
     */
    adjust_p = update_p->sa_next_p[level_c];
    slot_p->sa_next_p[level_c] = adjust_p->sa_next_p[level_c];
    adjust_p->sa_next_p[level_c] = slot_p;
  }
  
  return 1;
}

/*
 * static int alloc_slots
 *
 * DESCRIPTION:
 *
 * Allocate a block of new slots of a certain size and add them to the
 * free list.  If there are none in the linked list then we will
 * allocate a block of the size.
 *
 * RETURNS:
 *
 * Success - Valid pointer to a single block that was allocated for
 * the slots.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * level_n -> Number of the level we are looking for.  Set to 0 to
 * have it be chosen at random.
 *
 * extern_mem_pp <- Pointer to a void * which will be set to a chunk
 * of memory that was allocated externally and needs to be accounted
 * for.
 *
 * extern_np <- Pointer to an integer which will be set to the number
 * of blocks in the external memory chunk.
 */
static	void	*alloc_slots(const int level_n, void **extern_mem_pp,
			     int *extern_np)
{
  skip_alloc_t	*new_p;
  entry_block_t	*block_p;
  unsigned int	*magic3_p, magic3;
  int		size, new_c;
  
  /* we need to allocate a new block of the slots of this level */
  block_p = _dmalloc_heap_alloc(BLOCK_SIZE, extern_mem_pp, extern_np);
  if (block_p == NULL) {
    /* error code set in _dmalloc_heap_alloc */
    return NULL;
  }
  memset(block_p, 0, BLOCK_SIZE);
  admin_block_c++;
  
  /* intialize the block structure */
  block_p->eb_magic1 = ENTRY_BLOCK_MAGIC1;
  block_p->eb_level_n = level_n;
  block_p->eb_magic2 = ENTRY_BLOCK_MAGIC2;
  
  /* add the block on the entry block linked list */
  block_p->eb_next_p = entry_blocks[level_n];
  entry_blocks[level_n] = block_p;
  
  /* put the magic3 at the end of the block */
  magic3_p = (unsigned int *)((char *)block_p + BLOCK_SIZE -
			      sizeof(*magic3_p));
  magic3 = ENTRY_BLOCK_MAGIC3;
  memcpy(magic3_p, &magic3, sizeof(*magic3_p));
  
  /* get the size of the slot */
  size = SKIP_SLOT_SIZE(level_n);
  
  /* add in all of the unused slots to the linked list */
  new_c = 1;
  for (new_p = &block_p->eb_first_slot;
       (char *)new_p + size < (char *)magic3_p;
       new_p = (skip_alloc_t *)((char *)new_p + size)) {
    new_p->sa_level_n = level_n;
    new_p->sa_next_p[0] = entry_free_list[level_n];
    entry_free_list[level_n] = new_p;
    new_c++;
  }
  
  dmalloc_message("created %d new entries for level %d", new_c, level_n);
  
  /* extern pointer information set in _dmalloc_heap_alloc */
  return block_p;
}

/*
 * static int remove_slot
 *
 * DESCRIPTION:
 *
 * Remove a slot from the skip list.
 *
 * RETURNS:
 *
 * Success - 1
 *
 * Failure - 0
 *
 * ARGUMENTS:
 *
 * delete_p -> Pointer to the block we are deleting from the list.
 *
 * update_p -> Pointer to the skip_alloc entry we are using to hold
 * the update pointers.
 */
static	int	remove_slot(skip_alloc_t *delete_p, skip_alloc_t *update_p)
{
  skip_alloc_t	*adjust_p;
  int		level_c;
  
  /* update the block skip list */
  for (level_c = 0; level_c <= MAX_SKIP_LEVEL; level_c++) {
    
    /*
     * The update node holds pointers to the slots which are pointing
     * to the one we want since we need to update those pointers
     * ahead.
     */
    adjust_p = update_p->sa_next_p[level_c];
    
    /*
     * If the pointer in question is not pointing to the deleted slot
     * then the deleted slot is shorter than this level and we are
     * done.  This is guaranteed if we have a proper skip list.
     */
    if (adjust_p->sa_next_p[level_c] != delete_p) {
      break;
    }
    
    /*
     * We are deleting a slot after each of the slots in the update
     * array.  So for each level, we get the slot we are adjusting, we
     * set it's next pointers to the next pointers at the same level
     * from the deleted slot.
     */
    adjust_p->sa_next_p[level_c] = delete_p->sa_next_p[level_c];
  }
  
  /*
   * Sanity check here, we should always have at least 1 pointer to
   * the found node that we are deleting.
   */
  if (level_c == 0) {
    dmalloc_errno = ERROR_ADDRESS_LIST;
    dmalloc_error("remove_slot");
    return 0;
  }
  
  return 1;
}

/*
 * static skip_alloc_t *get_slot
 *
 * DESCRIPTION:
 *
 * Get a new slot of a certain size.  This calls alloc_slot and then
 * does a whole bunch of things if alloc_slots generates the need for
 * two new slots.  Jumping through hoops to get this right.
 *
 * RETURNS:
 *
 * Success - Valid skip-alloc pointer.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * None.
 */
static	skip_alloc_t	*get_slot(void)
{
  skip_alloc_t	*new_p;
  int		level_n, extern_n, slot_size;
  void		*admin_mem, *extern_mem;
  
  /* generate the level for our new slot */
  level_n = random_level(MAX_SKIP_LEVEL);
  slot_size = SKIP_SLOT_SIZE(level_n);
  
  /* get an extry from the free list */
  new_p = entry_free_list[level_n];
  if (new_p != NULL) {
    /* shift the linked list over */
    entry_free_list[level_n] = new_p->sa_next_p[0];
    /* zero our slot entry */
    memset(new_p, 0, slot_size);
    new_p->sa_level_n = level_n;
    return new_p;
  }
  
  /* add in all of the unused slots to the linked list */
  admin_mem = alloc_slots(level_n, &extern_mem, &extern_n);
  if (admin_mem == NULL) {
    /* error code set in alloc_slots */
    return NULL;
  }
  
  /* get one for the admin memory */
  new_p = entry_free_list[level_n];
  if (new_p == NULL) {
    /*
     * HUH?  This isn't right.  We should have created a whole bunch
     * of addresses
     */
    dmalloc_errno = ERROR_ADDRESS_LIST;
    dmalloc_error("get_slot");
    return NULL;
  }
  entry_free_list[level_n] = new_p->sa_next_p[0];
  memset(new_p, 0, slot_size);
  new_p->sa_flags = ALLOC_FLAG_ADMIN;
  new_p->sa_mem = admin_mem;
  new_p->sa_total_size = BLOCK_SIZE;
  new_p->sa_level_n = level_n;
  
  if (extern_n > 0) {
    /* get one for the admin memory */
    new_p = entry_free_list[level_n];
    if (new_p == NULL) {
      /*
       * HUH?  This isn't right.  We should have created a whole bunch
       * of addresses
       */
      dmalloc_errno = ERROR_ADDRESS_LIST;
      dmalloc_error("get_slot");
      return NULL;
    }
    entry_free_list[level_n] = new_p->sa_next_p[0];
    memset(new_p, 0, slot_size);
    new_p->sa_flags = ALLOC_FLAG_EXTERN;
    new_p->sa_mem = extern_mem;
    new_p->sa_total_size = extern_n * BLOCK_SIZE;
    new_p->sa_level_n = level_n;
    extern_block_c++;
  }
  
  /* now get one for the user */
  new_p = entry_free_list[level_n];
  if (new_p == NULL) {
    /*
     * HUH?  This isn't right.  We should have created a whole bunch
     * of addresses
     */
    dmalloc_errno = ERROR_ADDRESS_LIST;
    dmalloc_error("get_slot");
    return NULL;
  }
  entry_free_list[level_n] = new_p->sa_next_p[0];
  memset(new_p, 0, slot_size);
  new_p->sa_level_n = level_n;
  
  /* level_np set up top */
  return new_p;
}

/*
 * static skip_alloc_t *insert_address
 *
 * DESCRIPTION:
 *
 * Insert an address entry into a skip list.
 *
 * RETURNS:
 *
 * Success - Valid slot pointer.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * address -> Address we are inserting into the address list.
 *
 * free_b -> Insert a free address in the free-size list otherwise it
 * will go into the used address list.
 *
 * tot_size -> Total size of the chunk that we are inserting into the
 * list.
 */
static	skip_alloc_t	*insert_address(void *address, const int free_b,
					const unsigned int tot_size)
{
  skip_alloc_t	*new_p;
  
  /* get a new entry */
  new_p = get_slot();
  if (new_p == NULL) {
    /* error code set in get_slot */
    return NULL;
  }
  if (free_b) {
    new_p->sa_flags = ALLOC_FLAG_FREE;
  }
  else {
    new_p->sa_flags = ALLOC_FLAG_USER;
  }
  new_p->sa_mem = address;
  new_p->sa_total_size = tot_size;
  
  /* now try and insert the slot into the skip-list */
  if (! insert_slot(new_p, free_b)) {
    /* error code set in insert_slot */
    return NULL;
  }
  
  return new_p;
}

/*
 * static void *allocate_memory
 *
 * DESCRIPTION:
 *
 * Allocate memory on the heap and account for external blocks.
 *
 * RETURNS:
 *
 * Success - Valid slot pointer.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * size -> Size of the memory that we want to allocate.
 */
static	void	*allocate_memory(const unsigned int size)
{
  void		*mem, *extern_mem;
  skip_alloc_t	*slot_p;
  int		extern_n;
  
  /* allocate the memory and record if there were any external blocks */
  mem = _dmalloc_heap_alloc(size, &extern_mem, &extern_n);
  if (mem == HEAP_ALLOC_ERROR) {
    /* error code set in _dmalloc_heap_alloc */
    return NULL;
  }
  
  if (extern_n > 0) {
    slot_p = insert_address(extern_mem, 0 /* used */, extern_n * BLOCK_SIZE);
    if (slot_p == NULL) {
      /* error code set in insert_address */
      return NULL;
    }
    /* set the flags to be external memory */ 
    slot_p->sa_flags = ALLOC_FLAG_EXTERN;
  }
  
  return mem;
}

/******************************* misc routines *******************************/

/*
 * static int expand_chars
 *
 * DESCRIPTION:
 *
 * Copies a buffer into a output buffer while translates
 * non-printables into %03o octal values.  If it can, it will also
 * translate certain \ characters (\r, \n, etc.) into \\%c.  The
 * routine is useful for printing out binary values.
 *
 * Note: It does _not_ add a \0 at the end of the output buffer.
 *
 * RETURNS:
 *
 * Returns the number of characters added to the output buffer.
 *
 * ARGUMENTS:
 *
 * buf - the buffer to convert.
 *
 * buf_size - size of the buffer.  If < 0 then it will expand till it
 * sees a \0 character.
 *
 * out - destination buffer for the convertion.
 *
 * out_size - size of the output buffer.
 */
static	int	expand_chars(const void *buf, const int buf_size,
			     char *out, const int out_size)
{
  int			buf_c;
  const unsigned char	*buf_p, *spec_p;
  char	 		*out_p = out, *bounds_p;
  
  /* setup our max pointer */
  bounds_p = out + out_size;
  
  /* run through the input buffer, counting the characters as we go */
  for (buf_c = 0, buf_p = (const unsigned char *)buf;; buf_c++, buf_p++) {
    
    /* did we reach the end of the buffer? */
    if (buf_size < 0) {
      if (*buf_p == '\0') {
	break;
      }
    }
    else {
      if (buf_c >= buf_size) {
	break;
      }
    }
    
    /* search for special characters */
    for (spec_p = (unsigned char *)SPECIAL_CHARS + 1;
	 *(spec_p - 1) != '\0';
	 spec_p += 2) {
      if (*spec_p == *buf_p) {
	break;
      }
    }
    
    /* did we find one? */
    if (*(spec_p - 1) != '\0') {
      if (out_p + 2 >= bounds_p) {
	break;
      }
      out_p += loc_snprintf(out_p, bounds_p - out_p, "\\%c", *(spec_p - 1));
      continue;
    }
    
    /* print out any 7-bit printable characters */
    if (*buf_p < 128 && isprint(*buf_p)) {
      if (out_p + 1 >= bounds_p) {
	break;
      }
      *out_p = *(char *)buf_p;
      out_p += 1;
    }
    else {
      if (out_p + 4 >= bounds_p) {
	break;
      }
      out_p += loc_snprintf(out_p, bounds_p - out_p, "\\%03o", *buf_p);
    }
  }
  /* try to punch the null if we have space in case the %.*s doesn't work */
  if (out_p < bounds_p) {
    *out_p = '\0';
  }
  
  return out_p - out;
}

/*
 * static void get_pnt_info
 *
 * DESCRIPTION:
 *
 * With a slot, set a number of pointers to places within the block.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * slot_p -> Pointer to a slot structure that we are getting info on.
 *
 * info_p <-> Pointer to an info structure that we are filling with
 * information.
 */
static	void	get_pnt_info(const skip_alloc_t *slot_p, pnt_info_t *info_p)
{
  info_p->pi_fence_b = BIT_IS_SET(slot_p->sa_flags, ALLOC_FLAG_FENCE);
  info_p->pi_valloc_b = BIT_IS_SET(slot_p->sa_flags, ALLOC_FLAG_VALLOC);
  
  info_p->pi_alloc_start = slot_p->sa_mem;
  
  if (info_p->pi_fence_b) {
    if (info_p->pi_valloc_b) {
      info_p->pi_user_start = (char *)info_p->pi_alloc_start + BLOCK_SIZE;
      info_p->pi_fence_bottom = (char *)info_p->pi_user_start -
	FENCE_BOTTOM_SIZE;
    }
    else {
      info_p->pi_fence_bottom = info_p->pi_alloc_start;
      info_p->pi_user_start = (char *)info_p->pi_alloc_start +
	FENCE_BOTTOM_SIZE;
    }
  }
  else {
    info_p->pi_fence_bottom = NULL;
    info_p->pi_user_start = info_p->pi_alloc_start;
  }
  
  info_p->pi_user_bounds = (char *)info_p->pi_user_start +
    slot_p->sa_user_size;
  
  info_p->pi_alloc_bounds = (char *)slot_p->sa_mem + slot_p->sa_total_size;
  
  if (info_p->pi_fence_b) {
    info_p->pi_fence_top = info_p->pi_user_bounds;
    info_p->pi_upper_bounds = (char *)info_p->pi_alloc_bounds - FENCE_TOP_SIZE;
  }
  else {
    info_p->pi_fence_top = NULL;
    info_p->pi_upper_bounds = info_p->pi_alloc_bounds;
  }
}

/*
 * static char *display_pnt
 *
 * DESCRIPTION:
 *
 * Write into a buffer a discription of a pointer.
 *
 * RETURNS:
 *
 * Pointer to buffer 1st argument.
 *
 * ARGUMENTS:
 *
 * user_pnt -> Pointer that we are displaying.
 *
 * alloc_p -> Pointer to the skip slot which we are displaying.
 *
 * buf <-> Passed in buffer which will be filled with a description of
 * the pointer.
 *
 * buf_size -> Size of the buffer in bytes.
 */
static	char	*display_pnt(const void *user_pnt, const skip_alloc_t *alloc_p,
			     char *buf, const int buf_size)
{
  char	*buf_p, *bounds_p;
  int	elapsed_b;
  
  buf_p = buf;
  bounds_p = buf_p + buf_size;
  
  buf_p += loc_snprintf(buf_p, bounds_p - buf_p, "%#lx",
			(unsigned long)user_pnt);
  
#if STORE_SEEN_COUNT
  buf_p += loc_snprintf(buf_p, bounds_p - buf_p, "|s%lu", alloc_p->sa_seen_c);
#endif
  
#if STORE_ITERATION_COUNT
  buf_p += loc_snprintf(buf_p, bounds_p - buf_p, "|i%lu",
			alloc_p->sa_iteration);
#endif
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ELAPSED_TIME)) {
    elapsed_b = 1;
  }
  else {
    elapsed_b = 0;
  }
  if (elapsed_b || BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_CURRENT_TIME)) {
#if STORE_TIMEVAL
    {
      char	time_buf[64];
      buf_p += loc_snprintf(buf_p, bounds_p - buf_p, "|w%s",
			    _dmalloc_ptimeval(&alloc_p->sa_timeval, time_buf,
					      sizeof(time_buf), elapsed_b));
    }
#else
#if STORE_TIME
    {
      char	time_buf[64];
      buf_p += loc_snprintf(buf_p, bounds_p - buf_p, "|w%s",
			    _dmalloc_ptime(&alloc_p->sa_time, time_buf,
					   sizeof(time_buf), elapsed_b));
    }
#endif
#endif
  }
  
#if LOG_THREAD_ID
  {
    char	thread_id[256];
    
    buf_p += loc_snprintf(buf_p, bounds_p - buf_p, "|t");
    THREAD_ID_TO_STRING(thread_id, sizeof(thread_id), alloc_p->sa_thread_id);
    buf_p += loc_snprintf(buf_p, bounds_p - buf_p, "%s", thread_id);
  }
#endif
  
  return buf;
}

/*
 * static void log_error_info
 *
 * DESCRIPTION:
 *
 * Logging information about a pointer, usually during an error
 * condition.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * now_file -> File from where we generated the error.
 *
 * now_line -> Line number from where we generated the error.
 *
 * prev_file -> File of a previous location.  For instance the
 * location of a previous free() if we tried to free something twice.
 *
 * prev_line -> Line number of a previous location.  For instance the
 * location of a previous free() if we tried to free something twice.
 *
 * user_pnt -> Pointer in question.
 *
 * size -> Size that the user requested including any fence space.
 *
 * reason -> Reason string why something happened.
 *
 * where -> What routine is calling log_error_info.  For instance
 * malloc or chunk_check.
 */
static	void	log_error_info(const char *now_file,
			       const unsigned int now_line,
			       const char *prev_file,
			       const unsigned int prev_line,
			       const void *user_pnt, const unsigned int size,
			       const char *reason, const char *where)
{
  static int	dump_bottom_b = 0, dump_top_b = 0;
  char		out[(DUMP_SPACE + FENCE_BOTTOM_SIZE + FENCE_TOP_SIZE) * 4];
  char		where_buf[MAX_FILE_LENGTH + 64];
  char		where_buf2[MAX_FILE_LENGTH + 64];
  const char	*reason_str;
  const void	*dump_pnt = user_pnt;
  int		out_len, dump_size, offset;
  
  /* get a proper reason string */
  if (reason == NULL) {
    reason_str = dmalloc_strerror(dmalloc_errno);
  }
  else {
    reason_str = reason;
  }
  
  /* dump the pointer information */
  if (user_pnt == NULL) {
    dmalloc_message("%s: %s: from '%s' prev access '%s'",
		    where, reason_str,
		    _dmalloc_chunk_desc_pnt(where_buf, sizeof(where_buf),
					    now_file, now_line),
		    _dmalloc_chunk_desc_pnt(where_buf2, sizeof(where_buf2),
					    prev_file, prev_line));
  }
  else {
    dmalloc_message("%s: %s: pointer '%#lx' from '%s' prev access '%s'",
		    where, reason_str, (unsigned long)user_pnt,
		    _dmalloc_chunk_desc_pnt(where_buf, sizeof(where_buf),
					    now_file, now_line),
		    _dmalloc_chunk_desc_pnt(where_buf2, sizeof(where_buf2),
					    prev_file, prev_line));
  }
  
  /*
   * If we aren't logging bad space or we didn't error with an
   * overwrite error then don't log the bad bytes.
   */
  if ((! BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_BAD_SPACE))
      || (dmalloc_errno != ERROR_UNDER_FENCE
	  && dmalloc_errno != ERROR_OVER_FENCE
	  && dmalloc_errno != ERROR_FREE_NON_BLANK)) {
    return;
  }
  
  /* NOTE: display memory like this has the potential for generating a core */
  if (dmalloc_errno == ERROR_UNDER_FENCE) {
    /* NOTE: only dump out the proper fence-post area once */
    if (! dump_bottom_b) {
      out_len = expand_chars(fence_bottom, FENCE_BOTTOM_SIZE, out,
			     sizeof(out));
      dmalloc_message("Dump of proper fence-bottom bytes: '%.*s'",
		      out_len, out);
      dump_bottom_b = 1;
    }
    offset = -FENCE_BOTTOM_SIZE;
    dump_size = DUMP_SPACE + FENCE_BOTTOM_SIZE;
  }
  else if (dmalloc_errno == ERROR_OVER_FENCE && size > 0) {
    /* NOTE: only dump out the proper fence-post area once */
    if (! dump_top_b) {
      out_len = expand_chars(fence_top, FENCE_TOP_SIZE, out, sizeof(out));
      dmalloc_message("Dump of proper fence-top bytes: '%.*s'",
		      out_len, out);
      dump_top_b = 1;
    }
    /*
     * The size includes the bottom fence post area.  We want it to
     * align with the start of the top fence post area.
     */
    offset = size - FENCE_BOTTOM_SIZE - FENCE_TOP_SIZE;
    if (offset < 0) {
      offset = 0;
    }
    dump_size = DUMP_SPACE + FENCE_TOP_SIZE;
  }
  else {
    dump_size = DUMP_SPACE;
    offset = 0;
  }
  
  if (size > 0 && dump_size > size) {
    dump_size = size;
  }
  
  dump_pnt = (char *)user_pnt + offset;
  if (IS_IN_HEAP(dump_pnt)) {
    out_len = expand_chars(dump_pnt, dump_size, out, sizeof(out));
    dmalloc_message("Dump of '%#lx'%+d: '%.*s'",
		    (unsigned long)user_pnt, offset, out_len, out);
  }
  else {
    dmalloc_message("Dump of '%#lx'%+d failed: not in heap",
		    (unsigned long)user_pnt, offset);
  }
}

/*
 * static int fence_read
 *
 * DESCRIPTION
 *
 * Check a pointer for fence-post magic numbers.
 *
 * RETURNS:
 *
 * Success - 1 if the fence posts are good.
 *
 * Failure - 0 if they are not.
 *
 * ARGUMENTS:
 *
 * chunk_pnt -> Address we are checking.
 *
 * size -> Size of the block we are checking.
 */
static	int	fence_read(const void *chunk_pnt, const unsigned int size)
{
  /* check magic numbers in bottom of allocation block */
  if (memcmp(fence_bottom, (char *)chunk_pnt, FENCE_BOTTOM_SIZE) != 0) {
    dmalloc_errno = ERROR_UNDER_FENCE;
    return 0;
  }
  
  /* check numbers at top of allocation block */
  if (memcmp(fence_top, (char *)chunk_pnt + size - FENCE_TOP_SIZE,
	     FENCE_TOP_SIZE) != 0) {
    dmalloc_errno = ERROR_OVER_FENCE;
    return 0;
  }
  
  return 1;
}

/*
 * static void clear_alloc
 *
 * DESCRIPTION
 *
 * Setup allocations by writing fence post and doing any necessary
 * clearing of memory.
 *
 * RETURNS:
 *
 * Success - 1 if the fence posts are good.
 *
 * Failure - 0 if they are not.
 *
 * ARGUMENTS:
 *
 * info_p -> Pointer to information about the allocation.
 *
 * old_size -> If there was an old-size that we have copied into the
 * new pointer then set this.  If 0 then it will clear the entire
 * allocation.
 *
 * func_id -> ID of the function which is doing the allocation.  Used
 * to determine if we should 0 memory for [re]calloc.
 */
static	void	clear_alloc(pnt_info_t *info_p, const unsigned int old_size,
			    const int func_id)
{
  char	*old_bounds;
  
  /* if we are extending memory, write in our alloc chars */
  old_bounds = (char *)info_p->pi_user_start + old_size;
  if ((char *)info_p->pi_alloc_bounds > old_bounds) {
    if (func_id == DMALLOC_FUNC_CALLOC || func_id == DMALLOC_FUNC_RECALLOC) {
      (void)memset(old_bounds, 0,
		   (char *)info_p->pi_alloc_bounds - old_bounds);
    }
    else if (BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOC_BLANK)
	     || BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
      (void)memset(old_bounds, ALLOC_BLANK_CHAR,
		   (char *)info_p->pi_alloc_bounds - old_bounds);
    }
  }
  
  /* write in fence-post info */
  if (info_p->pi_fence_b) {
    memcpy(info_p->pi_fence_bottom, fence_bottom, FENCE_BOTTOM_SIZE);
    memcpy(info_p->pi_fence_top, fence_top, FENCE_TOP_SIZE);
  }
}

/************************** administration functions *************************/

/*
 * static int create_divided_chunks
 *
 * DESCRIPTION:
 *
 * Get a divided-block from the free list or heap allocation.
 *
 * RETURNS:
 *
 * Success - 1
 *
 * Failure - 0
 *
 * ARGUMENTS:
 *
 * div_size -> Size of the divided block that we are allocating.
 */
static	int	create_divided_chunks(const unsigned int div_size)
{
  void		*mem, *bounds_p;
  
  /* allocate a 1 block chunk that we will cut up into pieces */
  mem = allocate_memory(BLOCK_SIZE);
  if (mem == HEAP_ALLOC_ERROR) {
    return 0;
  }
  user_block_c++;
  
  /*
   * now run through the block and add the the locations to the
   * free-list
   */
  
  /* end of the block */
  bounds_p = (char *)mem + BLOCK_SIZE - div_size;
  
  for (; mem <= bounds_p; mem = (char *)mem + div_size) {
    /* insert the rest of the blocks into the free-list */
    if (insert_address(mem, 1 /* free list */, div_size) == NULL) {
      /* error set in insert_address */
      return 0;
    }
  }
  
  return 1;
}

/*
 * static skip_alloc_t *use_free_memory
 *
 * DESCRIPTION:
 *
 * Find a free memory chunk and remove it from the free list and put
 * it on the used list if available.
 *
 * RETURNS:
 *
 * Success - Valid slot pointer
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * size -> Size of the block that we are looking for.
 *
 * update_p -> Pointer to the skip_alloc entry we are using to hold
 * the update pointers.
 */
static	skip_alloc_t	*use_free_memory(const unsigned int size,
					 skip_alloc_t *update_p)
{
  skip_alloc_t	*slot_p;
  
  /* find a free block which matches the size */ 
  slot_p = find_free_size(size, update_p);
  if (slot_p == NULL) {
    return NULL;
  }
  
  /* sanity check */
  if (slot_p->sa_total_size != size) {
    dmalloc_errno = ERROR_ADDRESS_LIST;
    dmalloc_error("find_free_memory");
    return NULL;
  }
  
  /* remove from free list */
  if (! remove_slot(slot_p, update_p)) {
    /* error reported in remove_slot */
    return NULL;
  }
  free_space_bytes -= slot_p->sa_total_size;
  
  /* insert it into our address list */
  if (! insert_slot(slot_p, 0 /* used list */)) {
    /* error set in insert_slot */
    return NULL;
  }
  
  return slot_p;
}

/*
 * static skip_alloc_t *get_divided_memory
 *
 * DESCRIPTION:
 *
 * Get a divided memory block from the free list or heap allocation.
 *
 * RETURNS:
 *
 * Success - Valid skip slot pointer.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * size -> Size of the block that we are allocating.
 */
static	skip_alloc_t	*get_divided_memory(const unsigned int size)
{
  skip_alloc_t	*slot_p;
  unsigned int	need_size, bit_c;
  
  for (bit_c = 0; bit_c < BASIC_BLOCK; bit_c++) {
    if (bit_sizes[bit_c] >= size) {
      break;
    }
  }
  need_size = bit_sizes[bit_c];
  
  /* find a free block which matches the size */ 
  slot_p = use_free_memory(need_size, skip_update);
  if (slot_p != NULL) {
    return slot_p;
  }
  
  /* need to get more slots */
  if (! create_divided_chunks(need_size)) {
    /* errors dumped in  create_divided_chunks */
    return NULL;
  }
  
  /* now we ask again for the free memory */
  slot_p = use_free_memory(need_size, skip_update);
  if (slot_p == NULL) {
    /* huh?  This isn't right. */
    dmalloc_errno = ERROR_ADDRESS_LIST;
    dmalloc_error("get_divided_memory");
    return NULL;
  }
  
  return slot_p;
}

/*
 * static skip_alloc_t *get_memory
 *
 * DESCRIPTION:
 *
 * Get a block from the free list or heap allocation.
 *
 * RETURNS:
 *
 * Success - Valid skip slot pointer.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * size -> Size of the block that we are allocating.
 */
static	skip_alloc_t	*get_memory(const unsigned int size)
{
  skip_alloc_t	*slot_p, *update_p;
  void		*mem;
  unsigned int	need_size, block_n;
  
  /* do we need to print admin info? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ADMIN)) {
    dmalloc_message("need %d bytes", size);
  }
  
  /* do we have a divided block here? */
  if (size <= BLOCK_SIZE / 2) {
    return get_divided_memory(size);
  }
  
  /* round up to the nearest block size */
  need_size = size + BLOCK_SIZE - 1;
  block_n = need_size / BLOCK_SIZE;
  need_size = block_n * BLOCK_SIZE;
  
  update_p = skip_update;
  
  /* find a free block which matches the size */ 
  slot_p = use_free_memory(need_size, update_p);
  if (slot_p != NULL) {
    return slot_p;
  }
  
  /* if there are blocks that are larger than this */
  slot_p = update_p->sa_next_p[0];
  if (slot_p != NULL && slot_p->sa_total_size > size) {
    
    /*
     * now we ask again for the memory because we need to reset the
     * update pointer list
     */
    slot_p = use_free_memory(need_size, update_p);
    if (slot_p != NULL) {
      /* huh?  This isn't right. */
      dmalloc_errno = ERROR_ADDRESS_LIST;
      dmalloc_error("get_memory");
      return NULL;
    }
  }
  
  /* allocate the memory necessary for the new blocks */
  mem = allocate_memory(need_size);
  if (mem == HEAP_ALLOC_ERROR) {
    return NULL;
  }
  user_block_c += block_n;
  
  /* create our slot */
  slot_p = insert_address(mem, 0 /* used list */, need_size);
  if (slot_p == NULL) {
    /* error set in insert_address */
    return NULL;
  }
  
  return slot_p;
}

/*
 * static int check_slot
 *
 * Check out the pointer in a slot to make sure it is good.
 *
 * RETURNS:
 *
 * Success - 1
 *
 * Failure - 0
 *
 * ARGUMENTS:
 *
 * slot_p -> Slot that we are checking.
 *
 * user_pnt -> User pointer which was used to get the slot or NULL.
 */
static	int	check_slot(const skip_alloc_t *slot_p, const void *user_pnt)
{
  const char	*file, *name_p, *bounds_p;
  unsigned int	line;
  pnt_info_t	pnt_info;
  
  /* get pointer info */
  get_pnt_info(slot_p, &pnt_info);
  
  /*
   * since we did a loose match to find the block even with fence post
   * data, make sure that we have the user pointer
   */
  if (user_pnt != NULL && user_pnt != pnt_info.pi_user_start) {
    dmalloc_errno = ERROR_NOT_FOUND;
    return 0;
  }
  
  /*
   * If we have a valloc allocation then the _user_ pnt should be
   * block aligned otherwise the chunk_pnt should be.
   */
  if (BIT_IS_SET(slot_p->sa_flags, ALLOC_FLAG_VALLOC)
      && (! ON_BLOCK(pnt_info.pi_user_start))) {
    dmalloc_errno = ERROR_NOT_ON_BLOCK;
    return 0;
  }
  
  /* check out the fence-posts */
  if (BIT_IS_SET(slot_p->sa_flags, ALLOC_FLAG_FENCE)
      && (! fence_read(pnt_info.pi_fence_bottom, slot_p->sa_user_size))) {
    /* errno set in fence_read */
    return 0;
  }
  
  file = slot_p->sa_file;
  line = slot_p->sa_line;
  
  /* check line number */
  if (line > MAX_LINE_NUMBER) {
    dmalloc_errno = ERROR_BAD_LINE;
    return 0;
  }
  
  /*
   * Check file pointer only if file is not NULL and line is not 0
   * which implies that file is a return-addr.
   */
  if (file != DMALLOC_DEFAULT_FILE && line != DMALLOC_DEFAULT_LINE) {
    /* NOTE: we don't use strlen here because we might check too far */
    bounds_p = file + MAX_FILE_LENGTH;
    for (name_p = file; name_p < bounds_p && *name_p != '\0'; name_p++) {
    }
    if (name_p >= bounds_p
	|| name_p < file + MIN_FILE_LENGTH) {
      dmalloc_errno = ERROR_BAD_FILEP;
      return 0;
    }
  }
  
  return 1;
}

/*
 * static slot_alloc_t *find_slot
 *
 * Find a pointer's corresponding slot.
 *
 * RETURNS:
 *
 * Success - Pointer to the bblock
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * user_pnt -> Pointer we are tracking.
 *
 * prev_pp <- Pointer to a slot which, if not NULL, will be set to the
 * slot right before the one found.
 *
 * next_pp <- Pointer to a slot which, if not NULL, will be set to the
 * slot right after the one found.
 */
static	skip_alloc_t	*find_slot(const void *user_pnt,
				   skip_alloc_t **prev_pp,
				   skip_alloc_t **next_pp)
{
  skip_alloc_t	*slot_p, *update_p;
  
  if (user_pnt == NULL) {
    dmalloc_errno = ERROR_IS_NULL;
    return NULL;
  }
  
  update_p = skip_update;
  
  /* try to find the address with loose match */
  slot_p = find_address(user_pnt, 0 /* loose check */, update_p);
  if (slot_p == NULL) {
    /* not found */
    dmalloc_errno = ERROR_NOT_FOUND;
    return NULL;
  }
  
  if (! check_slot(slot_p, user_pnt)) {
    /* not found */
    dmalloc_errno = ERROR_NOT_FOUND;
    return NULL;
  }
  
  SET_POINTER(prev_pp, update_p->sa_next_p[0]);
  SET_POINTER(next_pp, slot_p->sa_next_p[0]);
  
  return slot_p;
}

/***************************** exported routines *****************************/

/*
 * int _dmalloc_chunk_startup
 * 
 * DESCRIPTION:
 *
 * Startup the low level malloc routines.
 *
 * RETURNS:
 *
 * Success - 1
 *
 * Failure - 0
 *
 * ARGUMENTS:
 *
 * None.
 */
int	_dmalloc_chunk_startup(void)
{
  unsigned FENCE_MAGIC_TYPE	value;
  char				*pos_p, *max_p;
  int				bit_c;
  
  value = FENCE_MAGIC_BOTTOM;
  max_p = fence_bottom + FENCE_BOTTOM_SIZE;
  for (pos_p = fence_bottom;
       pos_p < max_p;
       pos_p += sizeof(FENCE_MAGIC_TYPE)) {
    if (pos_p + sizeof(FENCE_MAGIC_TYPE) <= max_p) {
      memcpy(pos_p, (char *)&value, sizeof(FENCE_MAGIC_TYPE));
    }
    else {
      memcpy(pos_p, (char *)&value, max_p - pos_p);
    }
  }
  
  value = FENCE_MAGIC_TOP;
  max_p = fence_top + FENCE_TOP_SIZE;
  for (pos_p = fence_top; pos_p < max_p; pos_p += sizeof(FENCE_MAGIC_TYPE)) {
    if (pos_p + sizeof(FENCE_MAGIC_TYPE) <= max_p) {
      memcpy(pos_p, (char *)&value, sizeof(FENCE_MAGIC_TYPE));
    }
    else {
      memcpy(pos_p, (char *)&value, max_p - pos_p);
    }
  }
  
  /* initialize the bits array */
  for (bit_c = 0; bit_c < BASIC_BLOCK; bit_c++) {
    if ((1 << bit_c) >= CHUNK_SMALLEST_BLOCK) {
      bit_sizes[bit_c] = 1 << bit_c;
    }
  }
  
  return 1;
}

/*
 * char *_dmalloc_chunk_desc_pnt
 *
 * DESCRIPTION:
 *
 * Write into a buffer a pointer description with file and
 * line-number.
 *
 * RETURNS:
 *
 * Pointer to buffer 1st argument.
 *
 * ARGUMENTS:
 *
 * buf <-> Passed in buffer which will be filled with a description of
 * the pointer.
 *
 * buf_size -> Size of the buffer in bytes.
 *
 * file -> File name, return address, or NULL.
 *
 * line -> Line number or 0.
 */
char	*_dmalloc_chunk_desc_pnt(char *buf, const int buf_size,
				 const char *file, const unsigned int line)
{
  if (file == DMALLOC_DEFAULT_FILE && line == DMALLOC_DEFAULT_LINE) {
    (void)loc_snprintf(buf, buf_size, "unknown");
  }
  else if (line == DMALLOC_DEFAULT_LINE) {
    (void)loc_snprintf(buf, buf_size, "ra=%#lx", (unsigned long)file);
  }
  else if (file == DMALLOC_DEFAULT_FILE) {
    (void)loc_snprintf(buf, buf_size, "ra=ERROR(line=%u)", line);
  }
  else {
    (void)loc_snprintf(buf, buf_size, "%.*s:%u", MAX_FILE_LENGTH, file, line);
  }
  
  return buf;
}

/*
 * int _dmalloc_chunk_read_info
 *
 * DESCRIPTION:
 *
 * Return some information associated with a pointer.
 *
 * RETURNS:
 *
 * Success - 1 pointer is okay
 *
 * Failure - 0 problem with pointer
 *
 * ARGUMENTS:
 *
 * user_pnt -> Pointer we are checking.
 *
 * where <- Where the check is being made from.
 *
 * user_size_p <- Pointer to an unsigned int which, if not NULL, will
 * be set to the size of bytes that the user requested.
 *
 * alloc_size_p <- Pointer to an unsigned int which, if not NULL, will
 * be set to the total given size of bytes including block overhead.
 *
 * file_p <- Pointer to a character pointer which, if not NULL, will
 * be set to the file where the pointer was allocated.
 *
 * line_p <- Pointer to a character pointer which, if not NULL, will
 * be set to the line-number where the pointer was allocated.
 *
 * ret_attr_p <- Pointer to a void pointer, if not NULL, will be set
 * to the return-address where the pointer was allocated.
 *
 * seen_cp <- Pointer to an unsigned long which, if not NULL, will be
 * set to the number of times the pointer has been "seen".
 *
 * valloc_bp <- Pointer to an integer which, if not NULL, will be set
 * to 1 if the pointer was allocated with valloc() otherwise 0.
 *
 * fence_bp <- Pointer to an integer which, if not NULL, will be set
 * to 1 if the pointer has the fence bit set otherwise 0.
 */
int	_dmalloc_chunk_read_info(const void *user_pnt, const char *where,
			    unsigned int *user_size_p,
			    unsigned int *alloc_size_p, char **file_p,
			    unsigned int *line_p, void **ret_attr_p,
			    unsigned long **seen_cp, int *valloc_bp,
			    int *fence_bp)
{
  skip_alloc_t	*slot_p;
  const void	*chunk_pnt;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    dmalloc_message("reading info about pointer '%#lx'",
		    (unsigned long)user_pnt);
  }
  
  SET_POINTER(seen_cp, NULL);
  
  /* find the pointer */
  slot_p = find_address(user_pnt, 1 /* exact */, skip_update);
  if (slot_p == NULL) {
    /* errno set in find_block */
    log_error_info(NULL, 0, NULL, 0, user_pnt, 0, NULL, where);
    dmalloc_error("_chunk_read_info");
    return 0;
  }
  chunk_pnt = slot_p->sa_mem;
  
  /* write info back to user space */
  SET_POINTER(user_size_p, slot_p->sa_user_size);
  SET_POINTER(alloc_size_p, slot_p->sa_total_size);
  if (slot_p->sa_file == DMALLOC_DEFAULT_FILE) {
    SET_POINTER(file_p, NULL);
  }
  else {
    SET_POINTER(file_p, (char *)slot_p->sa_file);
  }
  SET_POINTER(line_p, slot_p->sa_line);
  /* if the line is blank then the file will be 0 or the return address */
  if (slot_p->sa_line == DMALLOC_DEFAULT_LINE) {
    SET_POINTER(ret_attr_p, (char *)slot_p->sa_file);
  }
  else {
    SET_POINTER(ret_attr_p, NULL);
  }
#if STORE_SEEN_COUNT
  SET_POINTER(seen_cp, &slot_p->sa_seen_c);
#endif
  
  SET_POINTER(valloc_bp, BIT_IS_SET(slot_p->sa_flags, ALLOC_FLAG_VALLOC));
  SET_POINTER(fence_bp, BIT_IS_SET(slot_p->sa_flags, ALLOC_FLAG_FENCE));
  return 1;
}

/******************************* heap checking *******************************/

/*
 * int _dmalloc_chunk_heap_check
 *
 * DESCRIPTION:
 *
 * Run extensive tests on the entire heap.
 *
 * RETURNS:
 *
 * Success - 1 if the heap is okay
 *
 * Failure - 0 if a problem was detected
 *
 * ARGUMENTS:
 *
 * None.
 */
int	_dmalloc_chunk_heap_check(void)
{
#if 0
  skip_alloc_t	*this_p, *prev_p;
  unsigned int	undef = 0, start = 0;
  char		*byte_p;
  void		*pnt;
  int		bit_c, dblock_c = 0, bblock_c = 0, free_c = 0, fence_b;
  unsigned int	bb_c = 0, len, block_type;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    dmalloc_message("checking heap");
  }
  
  check_c++;
  
  /* start pointers */
  this_adm_p = bblock_adm_head;
  ahead_p = this_adm_p;
  
  /* test admin pointer validity */
  if (! IS_IN_HEAP(this_adm_p)) {
    dmalloc_errno = ERROR_BAD_ADMIN_P;
    dmalloc_error("_chunk_check");
    return 0;
  }
  
  /* test structure validity */
  if (this_adm_p->ba_magic1 != CHUNK_MAGIC_BOTTOM
      || this_adm_p->ba_magic2 != CHUNK_MAGIC_TOP) {
    dmalloc_errno = ERROR_BAD_ADMIN_MAGIC;
    dmalloc_error("_chunk_check");
    return 0;
  }
  
  /* verify count value */
  if (this_adm_p->ba_pos_n != bb_c) {
    dmalloc_errno = ERROR_BAD_ADMIN_COUNT;
    dmalloc_error("_chunk_check");
    return 0;
  }
  
  /* check out the basic blocks */
  prev_bblock_p = NULL;
  for (bblock_p = this_adm_p->ba_blocks;; prev_bblock_p = bblock_p++) {
    
    /* are we at the end of the bb_admin section */
    if (bblock_p >= this_adm_p->ba_blocks + BB_PER_ADMIN) {
      this_adm_p = this_adm_p->ba_next;
      bb_c += BB_PER_ADMIN;
      
      /* are we done? */
      if (this_adm_p == NULL) {
	break;
      }
      
      /* test admin pointer validity */
      if (! IS_IN_HEAP(this_adm_p)) {
	dmalloc_errno = ERROR_BAD_ADMIN_P;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /* test structure validity */
      if (this_adm_p->ba_magic1 != CHUNK_MAGIC_BOTTOM
	  || this_adm_p->ba_magic2 != CHUNK_MAGIC_TOP) {
	dmalloc_errno = ERROR_BAD_ADMIN_MAGIC;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /* verify count value */
      if (this_adm_p->ba_pos_n != bb_c) {
	dmalloc_errno = ERROR_BAD_ADMIN_COUNT;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      bblock_p = this_adm_p->ba_blocks;
    }
    
    /* check for no-allocation */
    if (! BIT_IS_SET(bblock_p->bb_flags, BBLOCK_ALLOCATED)) {
      undef++;
      continue;
    }
    
    /* we better not have seen a not-allocated block before */
    if (undef > 0 && bblock_p->bb_flags != BBLOCK_ADMIN_FREE) {
      dmalloc_errno = ERROR_BAD_BLOCK_ORDER;
      dmalloc_error("_chunk_check");
      return 0;
    }
    
    start = 0;
    
    /*
     * check for different types
     */
    block_type = BBLOCK_FLAG_TYPE(bblock_p->bb_flags);
    switch (block_type) {
      
      /* check a starting user-block */
    case BBLOCK_START_USER:
      
      /* check X blocks in a row */
      if (bblock_c != 0) {
	dmalloc_errno = ERROR_USER_NON_CONTIG;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      fence_b = BIT_IS_SET(bblock_p->bb_flags, BBLOCK_FENCE);
      
      /* mark the size in bits */
      NUM_BITS(bblock_p->bb_size, bit_c);
      bblock_c = NUM_BLOCKS(bblock_p->bb_size);
      /* valloc basic blocks gets 1 extra block below for any fence info */
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_VALLOC) && fence_b) {
	bblock_c++;
      }
      start = 1;
      
      /* check fence-posts for memory chunk */
      if (fence_b) {
	pnt = BLOCK_POINTER(this_adm_p->ba_pos_n +
			    (bblock_p - this_adm_p->ba_blocks));
	/* if we have valloc block and there is fence info then shift pnt up */
	if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_VALLOC)) {
	  pnt = (char *)pnt + (BLOCK_SIZE - FENCE_BOTTOM_SIZE);
	}
	if (! fence_read(pnt, bblock_p->bb_size)) {
	  log_error_info(NULL, NULL, bblock_p->bb_file, bblock_p->bb_line,
			 pnt, bblock_p->bb_size, NULL, "heap-check");
	  dmalloc_error("_chunk_check");
	  return 0;
	}
      }
      /* NOTE: NO BREAK HERE ON PURPOSE */
      
    case BBLOCK_USER:
      
      fence_b = BIT_IS_SET(bblock_p->bb_flags, BBLOCK_FENCE);
      
      /* check line number */
      if (bblock_p->bb_line > MAX_LINE_NUMBER) {
	dmalloc_errno = ERROR_BAD_LINE;
	log_error_info(NULL, NULL, bblock_p->bb_file, bblock_p->bb_line, NULL,
		       0, NULL, "heap-check");
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /*
       * Check out size, BLOCK_SIZE / 2 == 512 when dblock allocs take
       * over.  If we have a valloc then the size might be small.
       */
      if (((! BIT_IS_SET(bblock_p->bb_flags, BBLOCK_VALLOC))
	   && bblock_p->bb_size <= BLOCK_SIZE / 2)
	  || bblock_p->bb_size > (1 << LARGEST_BLOCK)) {
	dmalloc_errno = ERROR_BAD_SIZE;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /* check file pointer */
      if (bblock_p->bb_file != DMALLOC_DEFAULT_FILE
	  && bblock_p->bb_line != DMALLOC_DEFAULT_LINE) {
	len = strlen(bblock_p->bb_file);
	if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
	  dmalloc_errno = ERROR_BAD_FILEP;
	  dmalloc_error("_chunk_check");
	  return 0;
	}
      }
      
      /* check X blocks in a row */
      if (bblock_c == 0) {
	dmalloc_errno = ERROR_USER_NON_CONTIG;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      if (start == 0
	  && (prev_bblock_p == NULL
	      || ((! BIT_IS_SET(prev_bblock_p->bb_flags, BBLOCK_START_USER))
		  && (! BIT_IS_SET(prev_bblock_p->bb_flags, BBLOCK_USER)))
	      || bblock_p->bb_file != prev_bblock_p->bb_file
	      || bblock_p->bb_line != prev_bblock_p->bb_line
	      || bblock_p->bb_size != prev_bblock_p->bb_size)) {
	dmalloc_errno = ERROR_USER_NON_CONTIG;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      bblock_c--;
      /* NOTE: we should check above the allocated space if alloc_blank on */
      break;
      
    case BBLOCK_ADMIN:
      
      /* check the bblock_admin linked-list */
      if (bblock_p->bb_admin_p != ahead_p) {
	dmalloc_errno = ERROR_BAD_BLOCK_ADMIN_P;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /* check count against admin count */
      if (bblock_p->bb_pos_n != ahead_p->ba_pos_n) {
	dmalloc_errno = ERROR_BAD_BLOCK_ADMIN_C;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      ahead_p = ahead_p->ba_next;
      break;
      
    case BBLOCK_DBLOCK:
      
      /* check out bit_c */
      if (bblock_p->bb_bit_n >= BASIC_BLOCK) {
	dmalloc_errno = ERROR_BAD_DBLOCK_SIZE;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /* check out dblock pointer */
      if (! IS_IN_HEAP(bblock_p->bb_dblock)) {
	dmalloc_errno = ERROR_BAD_DBLOCK_POINTER;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /* verify mem pointer */
      if (bblock_p->bb_mem != BLOCK_POINTER(this_adm_p->ba_pos_n +
					    (bblock_p -
					     this_adm_p->ba_blocks))) {
	dmalloc_errno = ERROR_BAD_DBLOCK_MEM;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /* check dblock entry very closely if necessary */
      for (dblock_c = 0, dblock_p = bblock_p->bb_dblock;
	   dblock_p < bblock_p->bb_dblock +
	     (1 << (BASIC_BLOCK - bblock_p->bb_bit_n));
	   dblock_c++, dblock_p++) {
	
	/* check out dblock entry to see if it is not free */
	if (dblock_p->db_flags == DBLOCK_FREE) {
	  
	  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_LISTS)) {
	    dblock_t	*dblist_p;
	    
	    /* find the free block in the free list */
	    for (dblist_p = free_dblock[bblock_p->bb_bit_n];
		 dblist_p != NULL;
		 dblist_p = dblist_p->db_next) {
	      if (dblist_p == dblock_p) {
		break;
	      }
	    }
	    
	    /* did we not find it and we are reusing pointers */
	    if (dblist_p == NULL) {
	      dmalloc_errno = ERROR_BAD_FREE_LIST;
	      dmalloc_error("_chunk_check");
	      return 0;
	    }
	    else {
	      free_dblock_c[bblock_p->bb_bit_n]--;
	    }
	  }
	  
	  continue;
	}
	
	/*
	 * check out size, better be less than BLOCK_SIZE / 2 I have to
	 * check this twice.  Yick.
	 */
	if ((int)dblock_p->db_size > BLOCK_SIZE / 2) {
	  dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  log_error_info(NULL, NULL, dblock_p->db_file, dblock_p->db_line,
			 NULL, 0, NULL, "heap-check");
	  dmalloc_error("_chunk_check");
	  return 0;
	}
	
	if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
	  pnt = (char *)bblock_p->bb_mem +
	    dblock_c * (1 << bblock_p->bb_bit_n);
	  if (! fence_read(pnt, dblock_p->db_size)) {
	    return 0;
	  }
	}
      }
      break;
      
    case BBLOCK_DBLOCK_ADMIN:
      
      /* check out dblock pointer */
      if (! IS_IN_HEAP(bblock_p->bb_slot_p)) {
	dmalloc_errno = ERROR_BAD_DBADMIN_POINTER;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /* verify magic numbers */
      if (bblock_p->bb_slot_p->da_magic1 != CHUNK_MAGIC_BOTTOM
	  || bblock_p->bb_slot_p->da_magic2 != CHUNK_MAGIC_TOP) {
	dmalloc_errno = ERROR_BAD_DBADMIN_MAGIC;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /* check out each dblock_admin struct? */
      for (dblock_p = bblock_p->bb_slot_p->da_block;
	   dblock_p < bblock_p->bb_slot_p->da_block + DB_PER_ADMIN;
	   dblock_p++) {
	
	/* see if we've used this slot before */
	if (dblock_p->db_bblock == NULL && dblock_p->db_next == NULL) {
	  continue;
	}
	
	/* sanity check */
	if ((! IS_IN_HEAP(dblock_p->db_bblock))
	    || dblock_p->db_bblock->bb_flags != BBLOCK_DBLOCK) {
	  dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  log_error_info(NULL, NULL, dblock_p->db_file, dblock_p->db_line,
			 dblock_p->db_bblock, 0, NULL, "heap-check");
	  dmalloc_error("_chunk_check");
	  continue;
	}
	
	/* check out dblock pointer and next pointer (if free) */
	if (dblock_p->db_flags == DBLOCK_FREE) {
	  
	  /* should we verify that we have a block of free'd chars */
	  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
	    
	    /* find pointer to memory chunk */
	    pnt = (char *)dblock_p->db_bblock->bb_mem +
	      (dblock_p - dblock_p->db_bblock->bb_dblock) *
	      (1 << dblock_p->db_bblock->bb_bit_n);
	    
	    for (byte_p = (char *)pnt;
		 byte_p < (char *)pnt + (1 << dblock_p->db_bblock->bb_bit_n);
		 byte_p++) {
	      if (*byte_p != FREE_BLANK_CHAR) {
		dmalloc_errno = ERROR_FREE_NON_BLANK;
		log_error_info(NULL, NULL,
			       dblock_p->db_file, dblock_p->db_line, byte_p, 0,
			       NULL, "heap-check");
		dmalloc_error("_chunk_check");
		return 0;
	      }
	    }
	  }
	  
	  continue;
	}
	
	/* check out size, better be less than BLOCK_SIZE / 2 */
	if ((int)dblock_p->db_size > BLOCK_SIZE / 2) {
	  dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  log_error_info(NULL, NULL, dblock_p->db_file, dblock_p->db_line,
			 NULL, 0, NULL, "heap-check");
	  dmalloc_error("_chunk_check");
	  return 0;
	}
	
	/* check line number */
	if (dblock_p->db_line > MAX_LINE_NUMBER) {
	  dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  log_error_info(NULL, NULL, dblock_p->db_file, dblock_p->db_line,
			 NULL, 0, NULL, "heap-check");
	  dmalloc_error("_chunk_check");
	  return 0;
	}
	
	if (dblock_p->db_file != DMALLOC_DEFAULT_FILE
	    && dblock_p->db_line != DMALLOC_DEFAULT_LINE) {
	  len = strlen(dblock_p->db_file);
	  if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
	    dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	    /* should there be a log-error-info call here? */
	    dmalloc_error("_chunk_check");
	    return 0;
	  }
	}
      }
      break;
      
    case BBLOCK_START_FREE:
      
      /* check X blocks in a row */
      if (free_c != 0) {
	dmalloc_errno = ERROR_USER_NON_CONTIG;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      free_c = bblock_p->bb_block_n;
      
      if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_LISTS)) {
	
	/* find the free block in the free list */
	for (bblist_p = free_bblock[bblock_p->bb_bit_n];
	     bblist_p != NULL;
	     bblist_p = bblist_p->bb_next) {
	  if (bblist_p == bblock_p) {
	    break;
	  }
	}
	
	/* did we find it? */
	if (bblist_p == NULL) {
	  dmalloc_errno = ERROR_BAD_FREE_LIST;
	  dmalloc_error("_chunk_check");
	  return 0;
	}
	else {
	  free_bblock_c[bblock_p->bb_bit_n]--;
	}
      }
      /* NOTE: NO BREAK HERE ON PURPOSE */
      
    case BBLOCK_FREE:
      
      /* NOTE: check out free_lists, depending on debug value? */
      
      if (block_type == BBLOCK_FREE) {
	if (prev_bblock_p == NULL
	    || (prev_bblock_p->bb_flags != BBLOCK_FREE
		&& prev_bblock_p->bb_flags != BBLOCK_START_FREE)
	    || bblock_p->bb_bit_n != prev_bblock_p->bb_bit_n) {
	  dmalloc_errno = ERROR_FREE_NON_CONTIG;
	  dmalloc_error("_chunk_check");
	  return 0;
	}
      }
      free_c--;
      
      /* should we verify that we have a block of freed chars? */
      if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
	pnt = BLOCK_POINTER(this_adm_p->ba_pos_n +
			    (bblock_p - this_adm_p->ba_blocks));
	for (byte_p = (char *)pnt;
	     byte_p < (char *)pnt + BLOCK_SIZE;
	     byte_p++) {
	  if (*byte_p != FREE_BLANK_CHAR) {
	    dmalloc_errno = ERROR_FREE_NON_BLANK;
	    log_error_info(NULL, NULL, bblock_p->bb_file, bblock_p->bb_line,
			   byte_p, 0, NULL, "heap-check");
	    dmalloc_error("_chunk_check");
	    /* continue to check the rest of the free list */
	    break;
	  }
	}
      }
      break;
      
      /* externally used block */
    case BBLOCK_EXTERNAL:
      /* nothing much to check */
      break;
      
      /* pointer to first free slot */
    case BBLOCK_ADMIN_FREE:
      /* better be the last block and the count should match undef */
      if (bblock_p != this_adm_p->ba_blocks + (BB_PER_ADMIN - 1)
	  || bblock_p->bb_free_n != (BB_PER_ADMIN - 1) - undef) {
	dmalloc_errno = ERROR_BAD_ADMIN_COUNT;
	dmalloc_error("_chunk_check");
	return 0;
      }
      break;
      
    default:
      dmalloc_errno = ERROR_BAD_FLAG;
      dmalloc_error("_chunk_check");
      return 0;
      /* NOTREACHED */
      break;
    }
  }
  
  /*
   * any left over contiguous counters?
   */
  if (bblock_c > 0) {
    dmalloc_errno = ERROR_USER_NON_CONTIG;
    dmalloc_error("_chunk_check");
    return 0;
  }
  if (free_c > 0) {
    dmalloc_errno = ERROR_FREE_NON_CONTIG;
    dmalloc_error("_chunk_check");
    return 0;
  }
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_LISTS)) {
    
    /* any free bblock entries not accounted for? */
    for (bit_c = 0; bit_c < MAX_SLOTS; bit_c++) {
      if (free_bblock_c[bit_c] != 0) {
	dmalloc_errno = ERROR_BAD_FREE_LIST;
	dmalloc_error("_chunk_check");
	return 0;
      }
    }
    
    /* any free dblock entries not accounted for? */
    for (bit_c = 0; bit_c < BASIC_BLOCK; bit_c++) {
      if (free_dblock_c[bit_c] != 0) {
	dmalloc_errno = ERROR_BAD_FREE_LIST;
	dmalloc_error("_chunk_check");
	return 0;
      }
    }
  }
#endif
  
  return 1;
}

/*
 * int _dmalloc_chunk_pnt_check
 *
 * DESCRIPTION:
 *
 * Run extensive tests on a pointer.
 *
 * RETURNS:
 *
 * Success - 1 if the pointer is okay
 *
 * Failure - 0 if not
 *
 * ARGUMENTS:
 *
 * func -> Function string which is checking the pointer.
 *
 * user_pnt -> Pointer we are checking.
 *
 * exact_b -> Set to 1 to find the pointer specifically.  Otherwise we
 * can find the pointer inside of an allocation.
 *
 * min_size -> Make sure that pnt can hold at least that many bytes.
 * If -1 then do a strlen + 1 for the \0.  If 0 then ignore.
 */
int	_dmalloc_chunk_pnt_check(const char *func, const void *user_pnt,
			    const int exact_b, const int min_size)
{
  skip_alloc_t	*slot_p;
  unsigned int	min;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    dmalloc_message("checking pointer '%#lx'", (unsigned long)user_pnt);
  }
  
  /* find our slot which does a lot of other checking */
  slot_p = find_slot(user_pnt, NULL /* prev */, NULL /* next */);
  if (slot_p == NULL) {
    if (exact_b) {
      /* it wasn't found */
      dmalloc_errno = ERROR_NOT_FOUND;
      log_error_info(NULL, 0, NULL, 0, user_pnt, 0, NULL, "pointer-check");
      dmalloc_error(func);
      return 0;
    }
    else {
      /* the pointer might not be the heap or might be NULL */
      dmalloc_errno = ERROR_NONE;
      return 1;
    }
  }
  
  /* if min_size is < 0 then do a strlen and take into account the \0 */
  if (min_size != 0) {
    if (min_size > 0) {
      min = min_size;
    }
    else {
      min = strlen((char *)user_pnt) + 1;
    }
    
    if (BIT_IS_SET(slot_p->sa_flags, ALLOC_FLAG_FENCE)) {
      min += FENCE_OVERHEAD_SIZE;
    }
    
    /* do we overflow the memory slot */
    if ((char *)user_pnt + min >
	(char *)slot_p->sa_mem + slot_p->sa_user_size) {
      dmalloc_errno = ERROR_WOULD_OVERWRITE;
      log_error_info(NULL, 0, slot_p->sa_file, slot_p->sa_line,
		     user_pnt, 0, NULL, "pointer-check");
      dmalloc_error(func);
      return 0;
    }
  }
  
  return 1;
}

/************************** low-level user functions *************************/

/*
 * void *_dmalloc_chunk_malloc
 *
 * DESCRIPTION:
 *
 * Allocate a chunk of memory.
 *
 * RETURNS:
 *
 * Success - Valid pointer.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * file -> File-name or return-address location of the allocation.
 *
 * line -> Line-number location of the allocation.
 *
 * size -> Number of bytes to allocate.
 *
 * func_id -> Calling function-id as defined in dmalloc.h.
 *
 * alignment -> If greater than 0 then try to align the returned
 * block.
 */
void	*_dmalloc_chunk_malloc(const char *file, const unsigned int line,
			  const unsigned long size, const int func_id,
			  const unsigned int alignment)
{
  unsigned long	byte_n = size;
  int		valloc_b = 0, memalign_b = 0, fence_b;
  char		where_buf[MAX_FILE_LENGTH + 64], disp_buf[64];
  skip_alloc_t	*slot_p;
  pnt_info_t	pnt_info;
  const char	*trans_log;
  
  /* counts calls to malloc */
  if (func_id == DMALLOC_FUNC_CALLOC) {
    func_calloc_c++;
  }
  else if (alignment == BLOCK_SIZE) {
    func_valloc_c++;
    valloc_b = 1;
  }
  else if (alignment > 0) {
    func_memalign_c++;
    memalign_b = 1;
  }
  else if (func_id != DMALLOC_FUNC_REALLOC
	   && func_id != DMALLOC_FUNC_RECALLOC) {
    func_malloc_c++;
  }
  
#if ALLOW_ALLOC_ZERO_SIZE == 0
  if (byte_n == 0) {
    dmalloc_errno = ERROR_BAD_SIZE;
    log_error_info(file, line, NULL, 0, NULL, 0,
		   "bad zero byte allocation request", "malloc");
    dmalloc_error("_chunk_malloc");
    return MALLOC_ERROR;
  }
#endif
  
  /* have we exceeded the upper bounds */
  if (size > LARGEST_BLOCK) {
    dmalloc_errno = ERROR_TOO_BIG;
    log_error_info(file, line, NULL, 0, NULL, 0, NULL, "malloc");
    dmalloc_error("_chunk_malloc");
    return MALLOC_ERROR;
  }
  
  /* adjust the size */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
    byte_n += FENCE_OVERHEAD_SIZE;
    fence_b = 1;
  }
  else {
    fence_b = 0;
  }
  
  /*
   * If the user is requesting a page-aligned block of data then we
   * will need another block below the allocation just for the fence
   * information.  Ugh.
   */
  if (valloc_b && fence_b) {
    byte_n += BLOCK_SIZE;
  }
  
  /* get some space for our memory */
  slot_p = get_memory(byte_n);
  if (slot_p == NULL) {
    /* errno set in get_slot */
    return MALLOC_ERROR;
  }
  slot_p->sa_user_size = byte_n;
  
  /* initialize the bblocks */
  alloc_cur_given += slot_p->sa_total_size;
  alloc_max_given = MAX(alloc_max_given, alloc_cur_given);
  
  get_pnt_info(slot_p, &pnt_info);
  
  /* not clear the allocation */
  clear_alloc(&pnt_info, 0 /* no old-size */, func_id);
  
#if STORE_SEEN_COUNT
  slot_p->sa_seen_c++;
#endif
#if STORE_ITERATION_COUNT
  slot_p->sa_iteration = _dmalloc_iter_c;
#endif
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ELAPSED_TIME)
      || BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_CURRENT_TIME)) {
#if STORE_TIMEVAL
    GET_TIMEVAL(slot_p->sa_timeval);
#else
#if STORE_TIME
    slot_p->sa_time = time(NULL);
#endif
#endif
  }
  
#if LOG_THREAD_ID
  slot_p->sa_thread_id = THREAD_GET_ID();
#endif
  
  /* do we need to print transaction info? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    switch (func_id) {
    case DMALLOC_FUNC_CALLOC:
      trans_log = "calloc";
      break;
    case DMALLOC_FUNC_MEMALIGN:
      trans_log = "memalign";
      break;
    case DMALLOC_FUNC_VALLOC:
      trans_log = "valloc";
      break;
    default:
      trans_log = "alloc";
      break;
    }
    dmalloc_message("*** %s: at '%s' for %ld bytes, got '%s'",
		    trans_log,
		    _dmalloc_chunk_desc_pnt(where_buf, sizeof(where_buf),
					    file, line),
		    size, display_pnt(pnt_info.pi_user_start, slot_p, disp_buf,
				      sizeof(disp_buf)));
  }
  
#if MEMORY_TABLE_LOG
  if (func_id != DMALLOC_FUNC_REALLOC && func_id != DMALLOC_FUNC_RECALLOC) {
    _dmalloc_table_alloc(file, line, size);
  }
#endif
  
  /* monitor current allocation level */
  alloc_current += size;
  alloc_maximum = MAX(alloc_maximum, alloc_current);
  alloc_total += size;
  alloc_one_max = MAX(alloc_one_max, size);
  
  /* monitor pointer usage */
  alloc_cur_pnts++;
  alloc_max_pnts = MAX(alloc_max_pnts, alloc_cur_pnts);
  alloc_tot_pnts++;
  
  return pnt_info.pi_user_start;
}

/*
 * int _dmalloc_chunk_free
 *
 * DESCRIPTION:
 *
 * Free a user pointer from the heap.
 *
 * RETURNS:
 *
 * Success - FREE_NOERROR
 *
 * Failure - FREE_ERROR
 *
 * ARGUMENTS:
 *
 * file -> File-name or return-address location of the allocation.
 *
 * line -> Line-number location of the allocation.
 *
 * user_pnt -> Pointer we are freeing.
 *
 * realloc_b -> Set to 1 if realloc is calling free.
 */
int	_dmalloc_chunk_free(const char *file, const unsigned int line,
		       void *user_pnt, const int realloc_b)
{
  char		where_buf[MAX_FILE_LENGTH + 64];
  char		where_buf2[MAX_FILE_LENGTH + 64], disp_buf[64];
  skip_alloc_t	*slot_p, *prev_p, *next_p;
  
  /* counts calls to free */
  if (! realloc_b) {
    func_free_c++;
  }
  
  if (user_pnt == NULL) {
    
#if ALLOW_FREE_NULL_MESSAGE
    /* does the user want a specific message? */
    dmalloc_message("WARNING: tried to free(0) from '%s'",
		    _dmalloc_chunk_desc_pnt(where_buf, sizeof(where_buf),
					    file, line));
#endif
    
    /*
     * NOTE: we have here both a default in the settings.h file and a
     * runtime token in case people want to turn it on or off at
     * runtime.
     */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_ERROR_FREE_NULL)) {
      dmalloc_errno = ERROR_IS_NULL;
      log_error_info(file, line, NULL, 0, user_pnt, 0, "invalid pointer",
		     "free");
      dmalloc_error("_chunk_free");
      return FREE_ERROR;
    }
    
#if ALLOW_FREE_NULL
    return FREE_NOERROR;
#else
    dmalloc_errno = ERROR_IS_NULL;
    return FREE_ERROR;
#endif
  }
  
  /* find which block it is in */
  slot_p = find_slot(user_pnt, &prev_p, &next_p);
  if (slot_p == NULL) {
    /* errno set in find_block */
    log_error_info(file, line, NULL, 0, user_pnt, 0, NULL, "free");
    dmalloc_error("_chunk_free");
    return FREE_ERROR;
  }
  
  alloc_cur_pnts--;
  
#if STORE_SEEN_COUNT
  slot_p->sa_seen_c++;
#endif
  
  /* do we need to print transaction info? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    dmalloc_message("*** free: at '%s' pnt '%s': size %u, alloced at '%s'",
		    _dmalloc_chunk_desc_pnt(where_buf, sizeof(where_buf), file,
					    line),
		    display_pnt(user_pnt, slot_p, disp_buf, sizeof(disp_buf)),
		    slot_p->sa_user_size,
		    _dmalloc_chunk_desc_pnt(where_buf2, sizeof(where_buf2),
					    slot_p->sa_file, slot_p->sa_line));
  }
  
#if MEMORY_TABLE_LOG
  if (! realloc_b) {
    _dmalloc_table_free(slot_p->sa_file, slot_p->sa_line,
			slot_p->sa_user_size);
  }
#endif
  
  /* monitor current allocation level */
  alloc_current -= slot_p->sa_user_size;
  alloc_cur_given -= slot_p->sa_total_size;
  free_space_bytes += slot_p->sa_total_size;
  
  /* clear the memory */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_FREE_BLANK)
      || BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
    (void)memset(slot_p->sa_mem, FREE_BLANK_CHAR, slot_p->sa_total_size);
  }
  
  /*
   * XXX: should we do this?  it will screwup the seen counter.
   *
   * Check above and below the free bblock looking for neighbors that
   * are free so we can add them together and put them in a different
   * free slot.
   *
   * NOTE: all of these block's reuse-iter count will be moved ahead
   * because we are encorporating in this newly freed block.
   */
  
  if (prev_p != NULL && BIT_IS_SET(prev_p->sa_flags, ALLOC_FLAG_FREE)) {
    /*
     * XXX: we remove slot because we already have update1 filled with
     * the proper information
     */
    /* combine the two slots */
  }
  if (next_p != NULL && BIT_IS_SET(next_p->sa_flags, ALLOC_FLAG_FREE)) {
  }
  
  /* XXX update the slot flags, and put on free list */
  
  return FREE_NOERROR;
}

/*
 * void *_dmalloc_chunk_realloc
 *
 * DESCRIPTION:
 *
 * Re-allocate a chunk of memory either shrinking or expanding it.
 *
 * RETURNS:
 *
 * Success - Valid pointer.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * file -> File-name or return-address location of the allocation.
 *
 * line -> Line-number location of the allocation.
 *
 * old_user_pnt -> Old user pointer that we are reallocating.
 *
 * new_size -> New-size to change the pointer.
 *
 * func_id -> Calling function-id as defined in dmalloc.h.
 */
void	*_dmalloc_chunk_realloc(const char *file, const unsigned int line,
				void *old_user_pnt,
				const unsigned long new_size,
				const int func_id)
{
  skip_alloc_t	*slot_p;
  pnt_info_t	pnt_info;
  void		*new_user_pnt;
  unsigned int	old_size;
  
  /* counts calls to realloc */
  if (func_id == DMALLOC_FUNC_RECALLOC) {
    func_recalloc_c++;
  }
  else {
    func_realloc_c++;
  }
  
#if ALLOW_ALLOC_ZERO_SIZE == 0
  if (new_size == 0) {
    dmalloc_errno = ERROR_BAD_SIZE;
    log_error_info(file, line, NULL, 0, NULL, 0,
		   "bad zero byte allocation request", "realloc");
    dmalloc_error("_chunk_realloc");
    return REALLOC_ERROR;
  }
#endif
  
  /* by now malloc.c should have taken care of the realloc(NULL) case */
  if (old_user_pnt == NULL) {
    dmalloc_errno = ERROR_IS_NULL;
    log_error_info(file, line, NULL, 0, old_user_pnt, 0, "invalid pointer",
		   "realloc");
    dmalloc_error("_chunk_realloc");
    return REALLOC_ERROR;
  }
  
  /*
   * TODO: for bblocks it would be nice to examine the above memory
   * looking for free blocks that we can absorb into this one.
   */
  
  /* find the old pointer */
  slot_p = find_address(old_user_pnt, 1 /* exact */, skip_update);
  if (slot_p == NULL) {
    /* errno set in find_block */
    log_error_info(NULL, 0, NULL, 0, old_user_pnt, 0, NULL,
		   "_dmalloc_chunk_realloc");
    dmalloc_error("_chunk_read_info");
    return 0;
  }
  
  /* get info about the pointer */
  get_pnt_info(slot_p, &pnt_info);
  old_size = slot_p->sa_user_size;
  
  /* if we are not realloc copying and the size is the same */
  if ((char *)pnt_info.pi_user_start + new_size >
      (char *)pnt_info.pi_upper_bounds
      || BIT_IS_SET(_dmalloc_flags, DEBUG_REALLOC_COPY)
      || BIT_IS_SET(_dmalloc_flags, DEBUG_NEVER_REUSE)) {
    int	min_size;
    
    /* allocate space for new chunk */
    new_user_pnt = _dmalloc_chunk_malloc(file, line, new_size, func_id,
				    0 /* no align */);
    if (new_user_pnt == MALLOC_ERROR) {
      return REALLOC_ERROR;
    }
    
    /*
     * NOTE: _chunk_malloc() already took care of the fence stuff and
     * zeroing of memory.
     */
    
    /* copy stuff into new section of memory */
    min_size = MIN(new_size, old_size);
    if (min_size > 0) {
      memcpy(new_user_pnt, pnt_info.pi_user_start, min_size);
    }
    
    /* free old pointer */
    if (_dmalloc_chunk_free(file, line, old_user_pnt,
		       1 /* from realloc */) != FREE_NOERROR) {
      return REALLOC_ERROR;
    }
  }
  else {
    /* new pointer is the same as the old one */
    new_user_pnt = pnt_info.pi_user_start;
    
    /*
     * monitor current allocation level
     *
     * NOTE: we do this here since the malloc/free used above take care
     * on if in that section
     */
    alloc_current += new_size - old_size;
    alloc_maximum = MAX(alloc_maximum, alloc_current);
    alloc_total += new_size;
    alloc_one_max = MAX(alloc_one_max, new_size);
    
    /* monitor pointer usage */
    alloc_tot_pnts++;
    
    /* change the slot information */
    slot_p->sa_user_size = new_size;
    get_pnt_info(slot_p, &pnt_info);
    
    clear_alloc(&pnt_info, old_size, func_id);
    
#if STORE_SEEN_COUNT
    /* we see in inbound and outbound so we need to increment by 2 */
    slot_p->sa_seen_c += 2;
#endif
  }
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    const char	*trans_log;
    char	where_buf[MAX_FILE_LENGTH + 64];
    char	where_buf2[MAX_FILE_LENGTH + 64];
    
    if (func_id == DMALLOC_FUNC_RECALLOC) {
      trans_log = "recalloc";
    }
    else {
      trans_log = "realloc";
    }
    dmalloc_message("*** %s: at '%s' from '%#lx' (%u bytes) file '%s' to '%#lx' (%lu bytes)",
		    trans_log,
		    _dmalloc_chunk_desc_pnt(where_buf, sizeof(where_buf),
					    file, line),
		    (unsigned long)old_user_pnt, old_size,
		    _dmalloc_chunk_desc_pnt(where_buf2, sizeof(where_buf2),
					    slot_p->sa_file, slot_p->sa_line),
		    (unsigned long)new_user_pnt, new_size);
  }
  
#if MEMORY_TABLE_LOG
  _dmalloc_table_free(slot_p->sa_file, slot_p->sa_line, old_size);
  _dmalloc_table_alloc(file, line, new_size);
#endif
  
  /* finally, we update the file/line info */
  slot_p->sa_file = file;
  slot_p->sa_line = line;
  
  return new_user_pnt;
}

/***************************** diagnostic routines ***************************/

/*
 * void _dmalloc_chunk_log_stats
 *
 * DESCRIPTION:
 *
 * Log general statistics from the heap to the logfile.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * None.
 */
void	_dmalloc_chunk_log_stats(void)
{
  unsigned long	overhead, tot_space, wasted;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    dmalloc_message("dumping chunk statistics");
  }
  
  tot_space = alloc_current + free_space_bytes;
  overhead = admin_block_c * BLOCK_SIZE;
  if (alloc_max_given >= tot_space) {
    wasted = 0;
  }
  else {
    wasted = tot_space - alloc_max_given;
  }
  
  /* version information */
  dmalloc_message("basic-block %d bytes, alignment %d bytes, heap grows %s",
		  BLOCK_SIZE, ALLOCATION_ALIGNMENT,
		  (HEAP_GROWS_UP ? "up" : "down"));
  
  /* general heap information */
  dmalloc_message("heap: %#lx to %#lx, size %ld bytes (%ld blocks)",
		  (unsigned long)_dmalloc_heap_base,
		  (unsigned long)_dmalloc_heap_last,
		  (long)HEAP_SIZE, user_block_c + admin_block_c);
  dmalloc_message("heap checked %ld", heap_check_c);
  
  /* log user allocation information */
  dmalloc_message("alloc calls: malloc %lu, calloc %lu, realloc %lu, free %lu",
		  func_malloc_c, func_calloc_c, func_realloc_c, func_free_c);
  dmalloc_message("alloc calls: recalloc %lu, memalign %lu, valloc %lu",
		  func_recalloc_c, func_memalign_c, func_valloc_c);
  dmalloc_message(" currently in use: %lu bytes (%lu pnts)",
		  alloc_current, alloc_cur_pnts);
  dmalloc_message(" total memory allocated: %lu bytes (%lu pnts)",
		  alloc_total, alloc_tot_pnts);
  
  /* maximum stats */
  dmalloc_message(" max in use at one time: %lu bytes (%lu pnts)",
		  alloc_maximum, alloc_max_pnts);
  dmalloc_message("max alloced with 1 call: %lu bytes",
		  alloc_one_max);
  dmalloc_message("max alloc rounding loss: %lu bytes (%lu%%)",
		  alloc_max_given - alloc_maximum,
		  (alloc_max_given == 0 ? 0 :
		   ((alloc_max_given - alloc_maximum) * 100) /
		   alloc_max_given));
  dmalloc_message("max memory space wasted: %lu bytes (%lu%%)",
		  wasted,
		  (tot_space == 0 ? 0 : ((wasted * 100) / tot_space)));
  
  /* final stats */
  dmalloc_message("final user memory space: %ld blocks (%ld bytes)",
		  user_block_c, tot_space);
  dmalloc_message(" final admin overhead: %ld blocks, %ld bytes (%ld%%)",
		  admin_block_c, overhead,
		  (HEAP_SIZE == 0 ? 0 : (overhead * 100) / HEAP_SIZE));
  dmalloc_message(" final external space: %ld bytes (%ld blocks)",
		  extern_block_c * BLOCK_SIZE, extern_block_c);
  
#if MEMORY_TABLE_LOG
  dmalloc_message("top %d allocations:", MEMORY_TABLE_LOG);
  _dmalloc_table_log_info(MEMORY_TABLE_LOG, 1);
#endif
}

/*
 * void _dmalloc_chunk_log_changed
 *
 * DESCRIPTION:
 *
 * Dump the pointer information that has changed since a pointer in
 * time.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * mark -> Dmalloc counter used to mark a specific time so that
 * servers can check on the changed pointers.
 *
 * log_non_free_b -> If set to 1 then log the new not-freed
 * (i.e. used) pointers.
 *
 * log_free_b -> If set to 1 then log the new freed pointers.
 *
 * details_b -> If set to 1 then dump the individual pointer entries
 * instead of just the summary.
 */
void	_dmalloc_chunk_log_changed(const unsigned long mark,
				   const int log_not_freed_b,
				   const int log_freed_b, const int details_b)
{
  skip_alloc_t	*slot_p;
  void		*user_pnt;
  int		known_b, freed_b;
  char		out[DUMP_SPACE * 4], *which_str;
  char		where_buf[MAX_FILE_LENGTH + 64], disp_buf[64];
  int		unknown_size_c = 0, unknown_block_c = 0, out_len;
  int		size_c = 0, block_c = 0;
  
  if (log_not_freed_b && log_freed_b) {
    which_str = "not-freed and freed";
  }
  else if (log_not_freed_b) {
    which_str = "not-freed";
  }
  else if (log_freed_b) {
    which_str = "freed";
  }
  else {
    return;
  }
  
  dmalloc_message("dumping %s pointers changed since %lu:",
		  which_str, mark);
  
  /* clear out our memory table so we can fill it with pointer info */
  _dmalloc_table_clear();
  
  /* run through the blocks */
  for (slot_p = skip_address_list;; slot_p = slot_p->sa_next_p[0]) {
    
    freed_b = BIT_IS_SET(slot_p->sa_flags, ALLOC_FLAG_FREE);
    
    /*
     * check for different types
     */
    if (! (freed_b || BIT_IS_SET(slot_p->sa_flags, ALLOC_FLAG_USER))) {
      continue;
    }
    
    /* do we want to dump this one? */
    if (! ((log_not_freed_b && BIT_IS_SET(slot_p->sa_flags, ALLOC_FLAG_USER))
	   || (log_freed_b && freed_b))) {
      continue;
    }    
    /* is it too long ago? */
    if (slot_p->sa_use_iter <= mark) {
      continue;
    }
    
    /* unknown pointer? */
    if (slot_p->sa_file == DMALLOC_DEFAULT_FILE
	|| slot_p->sa_line == DMALLOC_DEFAULT_LINE) {
      unknown_block_c++;
      unknown_size_c += slot_p->sa_user_size;
      known_b = 0;
    }
    else {
      known_b = 1;
    }
    
    if (BIT_IS_SET(slot_p->sa_flags, ALLOC_FLAG_FENCE)) {
      user_pnt = CHUNK_TO_USER(slot_p->sa_mem, 1 /* fence */);
    }
    else {
      user_pnt = slot_p->sa_mem;
    }
    
    if (known_b || (! BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_KNOWN))) {
      if (details_b) {
	dmalloc_message(" %s freed: '%s' (%u bytes) from '%s'",
			(freed_b ? "   " : "not"),
			display_pnt(user_pnt, slot_p, disp_buf,
				    sizeof(disp_buf)),
			slot_p->sa_user_size,
			_dmalloc_chunk_desc_pnt(where_buf, sizeof(where_buf),
						slot_p->sa_file,
						slot_p->sa_line));
	
	if ((! freed_b)
	    && BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_NONFREE_SPACE)) {
	  out_len = expand_chars((char *)user_pnt, DUMP_SPACE,
				 out, sizeof(out));
	  dmalloc_message("  dump of '%#lx': '%.*s'",
			  (unsigned long)user_pnt, out_len, out);
	}
      }
      _dmalloc_table_alloc(slot_p->sa_file, slot_p->sa_line,
			   slot_p->sa_user_size);
    }
  }
  
  /* dump the summary and clear the table */
  _dmalloc_table_log_info(0, 0);
  _dmalloc_table_clear();
  
  /* copy out size of pointers */
  if (block_c > 0) {
    if (block_c - unknown_block_c > 0) {
      dmalloc_message(" known memory: %d pointer%s, %d bytes",
		      block_c - unknown_block_c,
		      (block_c - unknown_block_c == 1 ? "" : "s"),
		      size_c - unknown_size_c);
    }
    if (unknown_block_c > 0) {
      dmalloc_message(" unknown memory: %d pointer%s, %d bytes",
		      unknown_block_c, (unknown_block_c == 1 ? "" : "s"),
		      unknown_size_c);
    }
  }
}
