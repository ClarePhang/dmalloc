/*
 * version string for the library
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
 * $Id: version.h,v 1.34 1994/05/11 19:32:42 gray Exp $
 */

#ifndef __VERSION_H__
#define __VERSION_H__

#include "malloc_loc.h"				/* for LOCAL */

/*
 * NOTE to gray: whenever this is changed, a corresponding entry
 * should be entered in:
 *
 *	Changlog
 *	News
 *	malloc.texi
 */
LOCAL	char	*malloc_version = "2.1.0b";

/* Version Date: $Date: 1994/05/11 19:32:42 $ */

#endif /* ! __VERSION_H__ */
