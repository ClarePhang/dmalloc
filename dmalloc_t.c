/*
 * test program for malloc code
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
 */

/*
 * Test program for the malloc library.  Current it is interactive although
 * should be script based.
 */

#include <stdio.h>				/* for stdin */

#include "argv.h"
#include "malloc_dbg.h"

/*
 * NOTE: this is only needed to test certain features of the library.
 */
#include "conf.h"

#if INCLUDE_RCS_IDS
static	char	*rcs_id =
  "$Id: dmalloc_t.c,v 1.35 1994/02/18 23:19:52 gray Exp $";
#endif

#define INTER_CHAR		'i'
#define DEFAULT_ITERATIONS	10000
#define MAX_POINTERS		1024
#define MAX_ALLOC		(1024 * 1024)

/* pointer tracking structure */
struct pnt_info_st {
  long			pi_crc;			/* crc of storage */
  int			pi_size;		/* size of storage */
  void			*pi_pnt;		/* pnt to storage */
  struct pnt_info_st	*pi_next;		/* pnt to next */
};

typedef struct pnt_info_st pnt_info_t;

static	pnt_info_t	pointer_grid[MAX_POINTERS];

/* argument variables */
static	int		default_itern = DEFAULT_ITERATIONS; /* # of iters */
static	char		interactive = ARGV_FALSE;	/* interactive flag */
static	int		max_alloc = MAX_ALLOC;		/* amt of mem to use */
static	char		silent = ARGV_FALSE;		/* silent flag */
static	char		verbose = ARGV_FALSE;		/* verbose flag */

static	argv_t		arg_list[] = {
  { INTER_CHAR,	"interactive",		ARGV_BOOL,		&interactive,
      NULL,			"turn on interactive mode" },
  { 'm',	"max-alloc",		ARGV_INT,		&max_alloc,
      "bytes",			"maximum allocation to test" },
  { 's',	"silent",		ARGV_BOOL,		&silent,
      NULL,			"do not display messages" },
  { 't',	"times",		ARGV_INT,		&default_itern,
      "number",			"number of iterations to run" },
  { 'v',	"verbose",		ARGV_BOOL,		&verbose,
      NULL,			"enables verbose messages" },
  { ARGV_LAST }
};

/*
 * hexadecimal STR to integer translation
 */
static	long	hex_to_long(char * str)
{
  long		ret;
  
  /* strip off spaces */
  for (; *str == ' ' || *str == '\t'; str++);
  
  /* skip a leading 0[xX] */
  if (*str == '0' && (*(str + 1) == 'x' || *(str + 1) == 'X'))
    str += 2;
  
  for (ret = 0;; str++) {
    if (*str >= '0' && *str <= '9')
      ret = ret * 16 + (*str - '0');
    else if (*str >= 'a' && *str <= 'f')
      ret = ret * 16 + (*str - 'a' + 10);
    else if (*str >= 'A' && *str <= 'F')
      ret = ret * 16 + (*str - 'A' + 10);
    else
      break;
  }
  
  return ret;
}

/*
 * read an address from the user
 */
static	void	*get_address(void)
{
  char	line[80];
  void	*pnt;
  
  do {
    (void)printf("Enter a hex address: ");
    (void)fgets(line, sizeof(line), stdin);
  } while (line[0] == '\0');
  
  pnt = (void *)hex_to_long(line);
  
  return pnt;
}

/*
 * try ITERN random program iterations, returns 1 on success else 0
 */
static	int	do_random(const int itern)
{
  int		iterc, last, amount, max = max_alloc;
  pnt_info_t	*freep = pointer_grid, *usedp = NULL;
  pnt_info_t	*pntp, *lastp, *thisp;
  
  malloc_errno = last = 0;
  
  /* initialize free list */
  for (pntp = pointer_grid; pntp < pointer_grid + MAX_POINTERS; pntp++) {
    pntp->pi_size = 0;
    pntp->pi_pnt = NULL;
    pntp->pi_next = pntp + 1;
  }
  /* redo the last next pointer */
  (pntp - 1)->pi_next = NULL;
  
  for (iterc = 0; iterc < itern;) {
    int		which;
    
    if (malloc_errno != last && ! silent) {
      (void)printf("ERROR: iter %d: %s(%d)\n",
		   iterc, malloc_strerror(malloc_errno), malloc_errno);
      last = malloc_errno;
    }
    
    which = (rand() % 20) / 10;
    
    /*
     * < 10 means alloc as long as we have enough memory and there are
     * free slots else we free
     */
    if (which == 0 && max > 10 && freep != NULL) {
      for (;;) {
	amount = rand() % (max / 2);
	if (amount > 0)
	  break;
#if ALLOW_ALLOC_ZERO_SIZE
	if (amount == 0)
	  break;
#endif
      }
      
      which = (rand() % 30) / 10;
      if (which == 0) {
	pntp = freep;
	pntp->pi_pnt = CALLOC(char, amount);
	
	if (verbose)
	  (void)printf("%d: calloc %d of max %d into slot %d.  got %#lx\n",
		       iterc + 1, amount, max, pntp - pointer_grid,
		       (long)pntp->pi_pnt);
      }
      else if (which == 1) {
	pntp = freep;
	pntp->pi_pnt = MALLOC(amount);
	
	if (verbose)
	  (void)printf("%d: malloc %d of max %d into slot %d.  got %#lx\n",
		       iterc + 1, amount, max, pntp - pointer_grid,
		       (long)pntp->pi_pnt);
      }
      else {
	pntp = pointer_grid + (rand() % MAX_POINTERS);
	if (pntp->pi_pnt == NULL)
	  continue;
	
	pntp->pi_pnt = REMALLOC(pntp->pi_pnt, amount);
	max += pntp->pi_size;
	
	if (verbose)
	  (void)printf("%d: realloc %d from %d of max %d slot %d.  got %#lx\n",
		       iterc + 1, pntp->pi_size, amount, max,
		       pntp - pointer_grid, (long)pntp->pi_pnt);
      }
      
      if (pntp->pi_pnt == NULL) {
	if (! silent)
	  (void)printf("allocation of %d returned error on iteration #%d\n",
		       amount, iterc + 1);
	iterc++;
	continue;
      }
      
      /* set the size and take it off the free-list and put on used list */
      pntp->pi_size = amount;
      
      if (pntp == freep) {
	freep = pntp->pi_next;
	pntp->pi_next = usedp;
	usedp = pntp;
      }
      
      max -= amount;
      iterc++;
      continue;
    }
    
    /*
     * choose a rand slot to free and make sure it is not a free-slot
     */
    pntp = pointer_grid + (rand() % MAX_POINTERS);
    if (pntp->pi_pnt == NULL)
      continue;
    
    FREE(pntp->pi_pnt);
    
    if (verbose)
      (void)printf("%d: free'd %d bytes from slot %d (%#lx)\n",
		   iterc + 1, pntp->pi_size, pntp - pointer_grid,
		   (long)pntp->pi_pnt);
    
    pntp->pi_pnt = NULL;
    
    /* find pnt in the used list */
    for (thisp = usedp, lastp = NULL; lastp != NULL;
	 lastp = thisp, thisp = thisp->pi_next)
      if (thisp == pntp)
	break;
    if (lastp == NULL)
      usedp = pntp->pi_next;
    else
      lastp->pi_next = pntp->pi_next;
    
    pntp->pi_next = freep;
    freep = pntp;
    
    max += pntp->pi_size;
    iterc++;
  }
  
  /* free used pointers */
  for (pntp = pointer_grid; pntp < pointer_grid + MAX_POINTERS; pntp++)
    if (pntp->pi_pnt != NULL)
      FREE(pntp->pi_pnt);
  
  if (malloc_errno == 0)
    return 1;
  else
    return 0;
}

/*
 * do some special tests, returns 1 on success else 0
 */
static	int	check_special(void)
{
  void	*pnt;
  
#if ALLOW_REALLOC_NULL
  pnt = REMALLOC(NULL, 10);
  if (pnt == NULL) {
    if (! silent)
      (void)printf("   ERROR: re-allocation of 0L returned error.\n");
  }
  else
    FREE(pnt);
#else
  pnt = REMALLOC(NULL, 10);
  if (pnt != NULL) {
    if (! silent)
      (void)printf("   ERROR: re-allocation of 0L did not return error.\n");
    FREE(pnt);
  }
#endif
  
#if ALLOW_FREE_NULL
  {
    int		hold = malloc_errno;
    
    malloc_errno = 0;
    
    FREE(NULL);
    if (malloc_errno != 0 && ! silent)
      (void)printf("   ERROR: free of 0L returned error.\n");
    
    malloc_errno = hold;
  }
#else
  {
    int		hold = malloc_errno;
    malloc_errno = 0;
    
    FREE(NULL);
    if (malloc_errno == 0 && ! silent)
      (void)printf("   ERROR: free of 0L did not return error.\n");
    
    malloc_errno = hold;
  }
#endif
  
  {
    int		hold = malloc_errno;
    malloc_errno = 0;
    
    pnt = MALLOC((1 << LARGEST_BLOCK) + 1);
    if (pnt != NULL) {
      if (! silent)
	(void)printf("   ERROR: allocation of > largest allowed size did not return error.\n");
      FREE(pnt);
    }
    
    malloc_errno = hold;
  }
  
  if (malloc_errno == 0)
    return 1;
  else
    return 0;
}

/*
 * run the interactive section of the program
 */
static	void	do_interactive(void)
{
  int		len;
  char		line[128], *linep;
  void		*pnt;
  
  (void)printf("Malloc test program.  Type 'help' for assistance.\n");
  
  for (;;) {
    (void)printf("> ");
    (void)fgets(line, sizeof(line), stdin);
    linep = (char *)index(line, '\n');
    if (linep != NULL)
      *linep = '\0';
    
    len = strlen(line);
    if (len == 0)
      continue;
    
    if (strncmp(line, "?", len) == 0
	|| strncmp(line, "help", len) == 0) {
      (void)printf("\thelp      - print this message\n\n");
      
      (void)printf("\tmalloc    - allocate memory\n");
      (void)printf("\tcalloc    - allocate/clear memory\n");
      (void)printf("\trealloc   - reallocate memory\n");
      (void)printf("\tfree      - deallocate memory\n\n");
      
      (void)printf("\tmap       - map the heap to the logfile\n");
      (void)printf("\tstats     - dump heap stats to the logfile\n");
      (void)printf("\tunfreed   - list the unfree memory to the logfile\n\n");
      
      (void)printf("\tverify    - check out a memory address (or all heap)\n");
      (void)printf("\toverwrite - overwrite some memory to test errors\n");
#if HAVE_SBRK
      (void)printf("\tsbrk       - call sbrk to test external areas\n\n");
#endif
      
      (void)printf("\trandom    - randomly execute a number of [de] allocs\n");
      (void)printf("\tspecial   - run some special tests\n\n");
      
      (void)printf("\tquit      - quit this test program\n");
      continue;
    }
    
    if (strncmp(line, "quit", len) == 0)
      break;
    
    if (strncmp(line, "malloc", len) == 0) {
      int	size;
      
      (void)printf("How much to malloc: ");
      (void)fgets(line, sizeof(line), stdin);
      size = atoi(line);
      (void)printf("malloc(%d) returned: %#lx\n", size, (long)MALLOC(size));
      continue;
    }
    
    if (strncmp(line, "calloc", len) == 0) {
      int	size;
      
      (void)printf("How much to calloc: ");
      (void)fgets(line, sizeof(line), stdin);
      size = atoi(line);
      (void)printf("calloc(%d) returned: %#lx\n",
		   size, (long)CALLOC(char, size));
      continue;
    }
    
    if (strncmp(line, "realloc", len) == 0) {
      int	size;
      
      pnt = get_address();
      
      (void)printf("How much to realloc: ");
      (void)fgets(line, sizeof(line), stdin);
      size = atoi(line);
      
      (void)printf("realloc(%#lx, %d) returned: %#lx\n",
		   (long)pnt, size, (long)REMALLOC(pnt, size));
      
      continue;
    }
    
    if (strncmp(line, "free", len) == 0) {
      pnt = get_address();
      FREE(pnt);
      continue;
    }
    
    if (strncmp(line, "map", len) == 0) {
      malloc_log_heap_map();
      (void)printf("Done.\n");
      continue;
    }
    
    if (strncmp(line, "stats", len) == 0) {
      malloc_log_stats();
      (void)printf("Done.\n");
      continue;
    }
    
    if (strncmp(line, "unfreed", len) == 0) {
      malloc_log_unfreed();
      (void)printf("Done.\n");
      continue;
    }
    
    if (strncmp(line, "overwrite", len) == 0) {
      char	*overwrite = "WOW!";
      
      pnt = get_address();
      bcopy(overwrite, (char *)pnt, strlen(overwrite));
      (void)printf("Done.\n");
      continue;
    }
    
#if HAVE_SBRK
    /* call sbrk directly */
    if (strncmp(line, "sbrk", len) == 0) {
      int	size;
      
      (void)printf("How much to sbrk: ");
      (void)fgets(line, sizeof(line), stdin);
      size = atoi(line);
      (void)printf("sbrk(%d) returned: %#lx\n", size, (long)sbrk(size));
      continue;
    }
#endif
    
    /* do random heap hits */
    if (strncmp(line, "random", len) == 0) {
      int	itern;
      
      (void)printf("How many iterations[%d]: ", default_itern);
      (void)fgets(line, sizeof(line), stdin);
      if (line[0] == '\0' || line[0] == '\n')
	itern = default_itern;
      else
	itern = atoi(line);
      
      if (do_random(itern))
	(void)printf("It succeeded.\n");
      else
	(void)printf("It failed.\n");
      
      continue;
    }
    
    /* do special checks */
    if (strncmp(line, "special", len) == 0) {
      if (check_special())
	(void)printf("It succeeded.\n");
      else
	(void)printf("It failed.\n");
      
      continue;
    }
    
    if (strncmp(line, "verify", len) == 0) {
      int	ret;
      
      (void)printf("If the address is 0, verify will check the whole heap.\n");
      pnt = get_address();
      ret = malloc_verify((char *)pnt);
      (void)printf("malloc_verify(%#lx) returned: %s\n",
		   (long)pnt,
		   (ret == MALLOC_VERIFY_NOERROR ? "success" : "failure"));
      continue;
    }
    
    (void)printf("Unknown command '%s'.  Type 'help' for assistance.\n", line);
  }
}

int	main(int argc, char ** argv)
{
  int	ret;
  
  argv_process(arg_list, argc, argv);
  
  if (silent && (verbose || interactive))
    silent = ARGV_FALSE;
  
  (void)srand(time(0) ^ 0xDEADBEEF);
  
  if (interactive)
    do_interactive();
  else {
    if (! silent)
      (void)printf("Running %d tests (use -%c for interactive)...\n",
		   default_itern, INTER_CHAR);
    (void)fflush(stdout);
    
    ret = do_random(default_itern);
    if (! silent)
      (void)printf("   %s.\n", (ret == 1 ? "Succeeded" : "Failed"));
  }
  
  if (! silent)
    (void)printf("Running special tests...\n");
  ret = check_special();
  if (! silent)
    (void)printf("   %s.\n", (ret == 1 ? "Succeeded" : "Failed"));
  
  /* you will need to uncomment this if you can't auto-shutdown */
#if 0
  /* shutdown the alloc routines */
  malloc_shutdown();
#endif
  
  argv_cleanup(arg_list);
  
  ret = malloc_verify(NULL);
  if (! silent)
    (void)printf("Final malloc_verify returned: %s\n",
		 (ret == MALLOC_VERIFY_NOERROR ? "success" : "failure"));
  
  if (malloc_errno == 0)
    exit(0);
  else
    exit(1);
}
