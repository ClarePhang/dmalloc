/*
 * compatibility functions for those systems who are missing them.
 *
 * Copyright 1993 by Gray Watson and the Antaire Corporation
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
 * Gray Watson and the Antaire Corporation make no representations about the
 * suitability of the software described herein for any purpose.  It is
 * provided "as is" without express or implied warranty.
 *
 * The author of the program may be contacted at gray.watson@antaire.com
 */

/*
 * This file holds the compatibility routines necessary for the library to
 * function just in case your system does not have them.
 */

#define MALLOC_DEBUG_DISABLE

#include "malloc_dbg.h"
#include "conf.h"

#include "compat.h"
#include "malloc_loc.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: compat.c,v 1.14 1993/07/23 05:37:48 gray Exp $";
#endif

#if HAVE_MEMCPY == 0 && HAVE_BCOPY == 0
/*
 * copy LEN characters from FROM to TO
 */
EXPORT	char	*memcpy(char * to, const char * from, MALLOC_SIZE len)
{
  char	*hold = to;
  
  for (; len > 0; len--, to++, from++)
    *to = *from;
  
  return hold;
}
#endif /* HAVE_MEMCPY == 0 && HAVE_BCOPY == 0 */

#if HAVE_MEMCMP == 0 && HAVE_BCMP == 0
/*
 * compare LEN characters, return -1,0,1 if STR1 is <,==,> STR2
 */
EXPORT	int	memcmp(const char * str1, const char * str2, MALLOC_SIZE len)
{
  for (; len > 0; len--, str1++, str2++)
    if (*str1 != *str2)
      return *str1 - *str2;
  
  return 0;
}
#endif /* HAVE_MEMCMP == 0 && HAVE_BCMP == 0 */

#if HAVE_MEMSET == 0
/*
 * set LEN characters in STR to character CH
 */
EXPORT	char	*memset(char * str, int ch, MALLOC_SIZE len)
{
  char	*hold = str;
  
  for (; len > 0; len--, str++)
    *str = (char)ch;
  
  return hold;
}
#endif /* HAVE_MEMSET == 0 */

#if HAVE_INDEX == 0
/*
 * find CH in STR by searching backwards through the string
 */
EXPORT	char	*index(const char * str, int ch)
{
  for (; *str != NULLC; str++)
    if (*str == (char)ch)
      return str;
  
  return NULL;
}
#endif /* HAVE_INDEX == 0 */

#if HAVE_RINDEX == 0
/*
 * find CH in STR by searching backwards through the string
 */
EXPORT	char	*rindex(const char * str, int ch)
{
  char	*pnt = NULL;
  
  for (; *str != NULLC; str++)
    if (*str == (char)ch)
      pnt = (char *)str;
  
  return pnt;
}
#endif /* HAVE_RINDEX == 0 */

#if HAVE_STRCAT == 0
/*
 * concatenate STR2 onto the end of STR1
 */
EXPORT	char	*strcat(char * str1, const char * str2)
{
  char	*hold = str1;
  
  for (; *str1 != NULLC; str1++);
  
  while (*str2 != NULLC)
    *str1++ = *str2++;
  *str1 = NULLC;
  
  return hold;
}
#endif /* HAVE_STRCAT == 0 */

#if HAVE_STRCMP == 0
/*
 * returns -1,0,1 on whether STR1 is <,==,> STR2
 */
EXPORT	int	strcmp(const char * str1, const char * str2)
{
  for (; *str1 != NULLC && *str1 == *str2; str1++, str2++);
  return *str1 - *str2;
}
#endif /* HAVE_STRCMP == 0 */

#if HAVE_STRLEN == 0
/*
 * return the length in characters of STR
 */
EXPORT	MALLOC_SIZE	strlen(const char * str)
{
  int	len;
  
  for (len = 0; *str != NULLC; str++, len++);
  
  return len;
}
#endif /* HAVE_STRLEN == 0 */

#if HAVE_STRTOK == 0
/*
 * get the next token from STR (pass in NULL on the 2nd, 3rd, etc. calls),
 * tokens are a list of characters deliminated by a character from DELIM.
 * writes null into STR to end token.
 */
EXPORT	char	*strtok(char * str, char * delim)
{
  static char	*last_str = "";
  char		*start, *delimp;
  
  /* no new strings to search? */
  if (str != NULL)
    last_str = str;
  else
    /* have we reached end of old one? */
    if (*last_str == NULLC)
      return NULL;
  
  /* parse through starting token deliminators */
  for (; *last_str != NULLC; last_str++) {
    for (delimp = delim; *delimp != NULLC; delimp++)
      if (*last_str == *delimp)
	break;
    
    /* is the character NOT in the delim list? */
    if (*delimp == NULLC)
      break;
  }
  
  /* did we reach the end? */
  if (*last_str == NULLC)
    return NULL;
  
  /* now start parsing through the string, could be NULLC already */
  for (start = last_str; *last_str != NULLC; last_str++)
    for (delimp = delim; *delimp != NULLC; delimp++)
      if (*last_str == *delimp) {
	/* punch NULL and point last_str past it */
	*last_str++ = NULLC;
	return start;
      }
  
  /* reached the end of the string */
  return start;
}
#endif /* HAVE_STRTOK == 0 */
