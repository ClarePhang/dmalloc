/*
 * functions for testing of string routines arguments.
 *
 * Copyright 1995 by Gray Watson
 *
 * This file is part of the dmalloc package.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * NON-COMMERCIAL purpose and without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies, and that the name of Gray Watson not be used in
 * advertising or publicity pertaining to distribution of the document
 * or software without specific, written prior permission.
 *
 * Please see the PERMISSIONS file or contact the author for information
 * about commercial licenses.
 *
 * Gray Watson makes no representations about the suitability of the
 * software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be contacted at gray.watson@letters.com
 */

/*
 * This file contains functions to be used to verify the arguments of
 * string functions.   If enabled these can discover problems with
 * heap-based strings (such as fence errors) much closer to the error.
 */

#define DMALLOC_DISABLE

#include "dmalloc.h"
#include "conf.h"

#include "chunk.h"
#include "debug_val.h"
#include "error.h"
#include "dmalloc_loc.h"
#include "arg_check.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: arg_check.c,v 1.13 1995/06/21 18:19:58 gray Exp $";
#endif

#if HAVE_BCMP
/*
 * dummy function for checking bcmp's arguments.
 */
EXPORT	int	_dmalloc_bcmp(const void * b1, const void * b2,
			      const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("bcmp", b1, CHUNK_PNT_LOOSE, len) != NOERROR
	|| _chunk_pnt_check("bcmp", b2, CHUNK_PNT_LOOSE, len) != NOERROR)
      _dmalloc_message("bad pointer argument found in bcmp");
  }
  return bcmp(b1, b2, len);
}
#endif

#if HAVE_BCOPY
/*
 * dummy function for checking bcopy's arguments.
 */
EXPORT	void	_dmalloc_bcopy(const char * from, char * to,
			       const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("bcopy", from, CHUNK_PNT_LOOSE, len) != NOERROR
	|| _chunk_pnt_check("bcopy", to, CHUNK_PNT_LOOSE, len) != NOERROR)
      _dmalloc_message("bad pointer argument found in bcopy");
  }
  bcopy(from, to, len);
}
#endif

#if HAVE_MEMCMP
/*
 * dummy function for checking memcmp's arguments.
 */
EXPORT	int	_dmalloc_memcmp(const void * b1, const void * b2,
				const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("memcmp", b1, CHUNK_PNT_LOOSE, len) != NOERROR
	|| _chunk_pnt_check("memcmp", b2, CHUNK_PNT_LOOSE, len) != NOERROR)
      _dmalloc_message("bad pointer argument found in memcmp");
  }
  return memcmp(b1, b2, len);
}
#endif

#if HAVE_MEMCPY
/*
 * dummy function for checking memcpy's arguments.
 */
EXPORT	char	*_dmalloc_memcpy(char * to, const char * from,
				 const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("memcpy", to, CHUNK_PNT_LOOSE, len) != NOERROR
	|| _chunk_pnt_check("memcpy", from, CHUNK_PNT_LOOSE, len) != NOERROR)
      _dmalloc_message("bad pointer argument found in memcpy");
  }
  return (char *)memcpy(to, from, len);
}
#endif

#if HAVE_MEMSET
/*
 * dummy function for checking memset's arguments.
 */
EXPORT	char	*_dmalloc_memset(void * buf, const char ch,
				 const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("memset", buf, CHUNK_PNT_LOOSE, len) != NOERROR)
      _dmalloc_message("bad pointer argument found in memset");
  }
  return (char *)memset(buf, ch, len);
}
#endif

#if HAVE_INDEX
/*
 * dummy function for checking index's arguments.
 */
EXPORT	char	*_dmalloc_index(const char * str, const char ch)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("index", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in index");
  }
  return (char *)index(str, ch);
}
#endif

#if HAVE_RINDEX
/*
 * dummy function for checking rindex's arguments.
 */
EXPORT	char	*_dmalloc_rindex(const char * str, const char ch)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("rindex", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in rindex");
  }
  return (char *)rindex(str, ch);
}
#endif

#if HAVE_STRCAT
/*
 * dummy function for checking strcat's arguments.
 */
EXPORT	char	*_dmalloc_strcat(char * to, const char * from)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("strcat", to, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			 strlen(to) + strlen(from) + 1) != NOERROR
	|| _chunk_pnt_check("strcat", from,
			    CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in strcat");
  }
  return (char *)strcat(to, from);
}
#endif

#if HAVE_STRCMP
/*
 * dummy function for checking strcmp's arguments.
 */
EXPORT	int	_dmalloc_strcmp(const char * s1, const char * s2)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("strcmp", s1, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			 0) != NOERROR
	|| _chunk_pnt_check("strcmp", s2, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			    0) != NOERROR)
      _dmalloc_message("bad pointer argument found in strcmp");
  }
  return strcmp(s1, s2);
}
#endif

#if HAVE_STRLEN
/*
 * dummy function for checking strlen's arguments.
 */
EXPORT	DMALLOC_SIZE	_dmalloc_strlen(const char * str)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("strlen", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in strlen");
  }
  return strlen(str);
}
#endif

#if HAVE_STRTOK
/*
 * dummy function for checking strtok's arguments.
 */
EXPORT	char	*_dmalloc_strtok(char * str, const char * sep)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if ((str != NULL
	 && _chunk_pnt_check("strtok", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			     0) != NOERROR)
	|| _chunk_pnt_check("strtok", sep,
			    CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in strtok");
  }
  return (char *)strtok(str, sep);
}
#endif

#if HAVE_BZERO
/*
 * dummy function for checking bzero's arguments.
 */
EXPORT	void	_dmalloc_bzero(void * buf, const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("bzero", buf, CHUNK_PNT_LOOSE, len) != NOERROR)
      _dmalloc_message("bad pointer argument found in bzero");
  }
  bzero(buf, len);
}
#endif

#if HAVE_MEMCCPY
/*
 * dummy function for checking memccpy's arguments.
 */
EXPORT	char	*_dmalloc_memccpy(char * s1, const char * s2, const char ch,
				  const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    /* maybe len maybe first ch */
    if (_chunk_pnt_check("memccpy", s1, CHUNK_PNT_LOOSE, 0) != NOERROR
	|| _chunk_pnt_check("memccpy", s2, CHUNK_PNT_LOOSE, 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in memccpy");
  }
  return (char *)memccpy(s1, s2, ch, len);
}
#endif

#if HAVE_MEMCHR
/*
 * dummy function for checking memchr's arguments.
 */
EXPORT	char	*_dmalloc_memchr(const char * s1, const char ch,
				 const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("memchr", s1, CHUNK_PNT_LOOSE, len) != NOERROR)
      _dmalloc_message("bad pointer argument found in memchr");
  }
  return (char *)memchr(s1, ch, len);
}
#endif

#if HAVE_STRCHR
/*
 * dummy function for checking strchr's arguments.
 */
EXPORT	char	*_dmalloc_strchr(const char * str, const char ch)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("strchr", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in strchr");
  }
  return (char *)strchr(str, ch);
}
#endif

#if HAVE_STRRCHR
/*
 * dummy function for checking strrchr's arguments.
 */
EXPORT	char	*_dmalloc_strrchr(const char * str, const char ch)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("strrchr", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in strrchr");
  }
  return (char *)strrchr(str, ch);
}
#endif

#if HAVE_STRCPY
/*
 * dummy function for checking strcpy's arguments.
 */
EXPORT	char	*_dmalloc_strcpy(char * to, const char * from)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("strcpy", to, CHUNK_PNT_LOOSE,
			 strlen(from) + 1) != NOERROR
	|| _chunk_pnt_check("strcpy", from,
			    CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in strcpy");
  }
  return (char *)strcpy(to, from);
}
#endif

#if HAVE_STRNCPY
/*
 * dummy function for checking strncpy's arguments.
 */
EXPORT	char	*_dmalloc_strncpy(char * to, const char * from,
				  const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    /* len or until nullc */
    if (_chunk_pnt_check("strncpy", to, CHUNK_PNT_LOOSE, 0) != NOERROR
	|| _chunk_pnt_check("strncpy", from,
			    CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in strncpy");
  }
  return (char *)strncpy(to, from, len);
}
#endif

#if HAVE_STRCASECMP
/*
 * dummy function for checking strcasecmp's arguments.
 */
EXPORT	int	_dmalloc_strcasecmp(const char * s1, const char * s2)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("strcasecmp", s1,
			 CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0) != NOERROR
	|| _chunk_pnt_check("strcasecmp", s2,
			    CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in strcasecmp");
  }
  return strcasecmp(s1, s2);
}
#endif

#if HAVE_STRNCASECMP
/*
 * dummy function for checking strncasecmp's arguments.
 */
EXPORT	int	_dmalloc_strncasecmp(const char * s1, const char * s2,
				     const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    /* len or until nullc */
    if (_chunk_pnt_check("strncasecmp", s1,
			 CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0) != NOERROR
	|| _chunk_pnt_check("strncasecmp", s2,
			    CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in strncasecmp");
  }
  return strncasecmp(s1, s2, len);
}
#endif

#if HAVE_STRSPN
/*
 * dummy function for checking strspn's arguments.
 */
EXPORT	int	_dmalloc_strspn(const char * str, const char * list)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("strspn", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			 0) != NOERROR
	|| _chunk_pnt_check("strspn", list,
			    CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in strspn");
  }
  return strspn(str, list);
}
#endif

#if HAVE_STRCSPN
/*
 * dummy function for checking strcspn's arguments.
 */
EXPORT	int	_dmalloc_strcspn(const char * str, const char * list)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("strcspn", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			 0) != NOERROR
	|| _chunk_pnt_check("strcspn", list,
			    CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in strcspn");
  }
  return strcspn(str, list);
}
#endif

#if HAVE_STRNCAT
/*
 * dummy function for checking strncat's arguments.
 */
EXPORT	char	*_dmalloc_strncat(char * to, const char * from,
				  const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    /* either len or nullc */
    if (_chunk_pnt_check("strncat", to, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			 0) != NOERROR
	|| _chunk_pnt_check("strncat", from,
			    CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in strncat");
  }
  return (char *)strncat(to, from, len);
}
#endif

#if HAVE_STRNCMP
/*
 * dummy function for checking strncmp's arguments.
 */
EXPORT	int	_dmalloc_strncmp(const char * s1, const char * s2,
				 const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    /* either len or nullc */
    if (_chunk_pnt_check("strncmp", s1, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			 0) != NOERROR
	|| _chunk_pnt_check("strncmp", s2,
			    CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in strncmp");
  }
  return strncmp(s1, s2, len);
}
#endif

#if HAVE_STRPBRK
/*
 * dummy function for checking strpbrk's arguments.
 */
EXPORT	char	*_dmalloc_strpbrk(const char * str, const char * list)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("strpbrk", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			 0) != NOERROR
	|| _chunk_pnt_check("strpbrk", list,
			    CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in strpbrk");
  }
  return (char *)strpbrk(str, list);
}
#endif

#if HAVE_STRSTR
/*
 * dummy function for checking strstr's arguments.
 */
EXPORT	char	*_dmalloc_strstr(const char * str, const char * pat)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (_chunk_pnt_check("strstr", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			 0) != NOERROR
	|| _chunk_pnt_check("strstr", pat,
			    CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0) != NOERROR)
      _dmalloc_message("bad pointer argument found in strstr");
  }
  return (char *)strstr(str, pat);
}
#endif
