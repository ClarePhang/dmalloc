/*
 * local defines for the low level memory routines
 *
 * Copyright 1993 by the Antaire Corporation
 *
 * This file is part of the malloc-debug package.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose and without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Antaire not be used in advertising or publicity pertaining to
 * distribution of the document or software without specific, written prior
 * permission.
 *
 * The Antaire Corporation makes no representations about the suitability of
 * the software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be contacted at gray.watson@antaire.com
 *
 * $Id: chunk_loc.h,v 1.20 1993/11/23 07:41:26 gray Exp $
 */

#ifndef __CHUNK_LOC_H__
#define __CHUNK_LOC_H__

/* defines for the malloc subsystem */

/* checking information */
#define MIN_FILE_LENGTH		    3		/* min "[a-zA-Z].c" length */
#define MAX_FILE_LENGTH		   40		/* max __FILE__ length */
#define MAX_LINE_NUMBER		10000		/* max __LINE__ value */

/* free info */
#define BLANK_CHAR		'\305'		/* to blank free space with */

/* log-bad-space info */
#define SPECIAL_CHARS		"e\033^^\"\"''\\\\n\nr\rt\tb\bf\fa\007"
#define DUMP_OFFSET		2		/* offset back to dump */
#define DUMP_SPACE		15		/* number of bytes to dump */
#define DUMP_SPACE_BUF		128		/* space for memory dump */

/*
 * the default smallest allowable allocations in bits.  this is
 * adjusted at runtime to conform to other settings.
 */
#define DEFAULT_SMALLEST_BLOCK	4

/* size of a block */
#define BLOCK_SIZE		(1 << BASIC_BLOCK)

/* pointer to the start of the block which holds PNT */
#define WHAT_BLOCK(pnt)		(((long)(pnt) / BLOCK_SIZE) * BLOCK_SIZE)

/* adjust internal PNT to user-space */
#define CHUNK_TO_USER(pnt)	((char *)(pnt) + pnt_below_adm)
#define USER_TO_CHUNK(pnt)	((char *)(pnt) - pnt_below_adm)

/* get the number of blocks to hold SIZE */
#define NUM_BLOCKS(size)	((size + (BLOCK_SIZE - 1)) / BLOCK_SIZE)

/*
 * number of ba_block entries is a bblock_adm_t which must fit in a
 * basic block
 */
#define BB_PER_ADMIN	((BLOCK_SIZE - \
			  (sizeof(long) + sizeof(int) + \
			   sizeof(struct bblock_adm_st *) + \
			   sizeof(long))) / sizeof(bblock_t))
/*
 * number of da_block entries in a dblock_adm_t which must fit in a
 * basic block
 */
#define DB_PER_ADMIN	((BLOCK_SIZE - (sizeof(long) + sizeof(long))) \
			 / sizeof(dblock_t))

#define CHUNK_MAGIC_BASE	0xDEA007	/* base magic number */
#define CHUNK_MAGIC_TOP		0x976DEAD	/* top magic number */

/* bb_flags values */
#define BBLOCK_ALLOCATED	0x3F		/* block has been allocated */
#define BBLOCK_START_USER	0x01		/* start of some user space */
#define BBLOCK_USER		0x02		/* allocated by user space */
#define BBLOCK_ADMIN		0x04		/* pointing to bblock admin */
#define BBLOCK_DBLOCK		0x08		/* pointing to divided block */
#define BBLOCK_DBLOCK_ADMIN	0x10		/* pointing to dblock admin */
#define BBLOCK_FREE		0x20		/* block is free */
#define BBLOCK_ADMIN_FREE	0x40		/* ba_count pnt to free slot */

/*
 * single divided-block administrative structure
 */
struct dblock_st {
  union {
    struct {
      unsigned short	nu_size;		/* size of contiguous area */
      unsigned short	nu_line;		/* line where it was alloced */
    } in_nums;
    
    struct bblock_st	*in_bblock;		/* pointer to the bblock */
  } db_info;
  
  /* to reference union and struct elements as db elements */
#define db_bblock	db_info.in_bblock	/* F */
#define db_size		db_info.in_nums.nu_size	/* U */
#define db_line		db_info.in_nums.nu_line	/* U */
  
  union {
    struct dblock_st	*pn_next;		/* next in the free list */
    char		*pn_file;		/* .c filename where alloced */
  } db_pnt;
  
  /* to reference union elements as db elements */
#define db_next		db_pnt.pn_next		/* F */
#define db_file		db_pnt.pn_file		/* U */
  
};
typedef struct dblock_st	dblock_t;

/*
 * single basic-block administrative structure
 */
struct bblock_st {
  unsigned short	bb_flags;		/* what it is */
  
  union {
    unsigned short	nu_bitn;		/* chunk size */
    unsigned short	nu_line;		/* line where it was alloced */
  } bb_num;
  
  /* to reference union elements as bb elements */
#define bb_bitn		bb_num.nu_bitn		/* D */
#define bb_line		bb_num.nu_line		/* U */
  
  union {
    unsigned int	in_count;		/* admin count number */
    dblock_t		*in_dblock;		/* pointer to dblock info */
    unsigned int	in_blockn;		/* number of blocks */
    unsigned int	in_size;		/* size of allocation */
  } bb_info;
  
  /* to reference union elements as bb elements */
#define bb_count	bb_info.in_count	/* A */
#define	bb_dblock	bb_info.in_dblock	/* D */
#define	bb_blockn	bb_info.in_blockn	/* F */
#define	bb_size		bb_info.in_size		/* U */
  
  union {
    struct dblock_adm_st	*pn_slotp;	/* pointer to db_admin block */
    struct bblock_adm_st	*pn_adminp;	/* pointer to bb_admin block */
    void			*pn_mem;	/* memory associated to it */
    struct bblock_st		*pn_next;	/* next in free list */
    char			*pn_file;	/* .c filename where alloced */
  } bb_pnt;
  
  /* to reference union elements as bb elements */
#define	bb_slotp	bb_pnt.pn_slotp		/* a */
#define	bb_adminp	bb_pnt.pn_adminp	/* A */
#define	bb_mem		bb_pnt.pn_mem		/* D (and tmp) */
#define	bb_next		bb_pnt.pn_next		/* F */
#define	bb_file		bb_pnt.pn_file		/* U */
  
};
typedef struct bblock_st	bblock_t;

/*
 * collection of bblock admin structures
 */
struct bblock_adm_st {
  long			ba_magic1;		/* bottom magic number */
  int			ba_count;		/* position in bblock array */
  bblock_t		ba_blocks[BB_PER_ADMIN]; /* bblock admin info */
  struct bblock_adm_st	*ba_next;		/* next bblock adm struct */
  long			ba_magic2;		/* top magic number */
};
typedef struct bblock_adm_st	bblock_adm_t;

/*
 * collection of dblock admin structures
 */
struct dblock_adm_st {
  long			da_magic1;		/* bottom magic number */
  dblock_t		da_block[DB_PER_ADMIN];	/* dblock admin info */
  long			da_magic2;		/* top magic number */
};
typedef struct dblock_adm_st	dblock_adm_t;

#endif /* ! __CHUNK_LOC_H__ */
