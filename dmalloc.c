/*
 * program that handles the malloc debug variables.
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
 * This is the malloc_dbg program which is designed to enable the user
 * to easily set the environmental variables that control the malloc-debug
 * library capabilities.
 *
 * NOTE: all stdout output from this program is designed to be run through
 *   eval by default.  Any messages for the user should be fprintf to stderr.
 */

#include <stdio.h>				/* for stderr */

#define MALLOC_DEBUG_DISABLE

#include "argv.h"				/* for argument processing */

#include "malloc_dbg.h"
#include "conf.h"

#include "compat.h"
#include "dbg_tokens.h"
#include "error_str.h"
#include "error_val.h"
#include "malloc_loc.h"
#include "version.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: dmalloc.c,v 1.31 1993/11/23 07:42:04 gray Exp $";
#endif

#define HOME_ENVIRON	"HOME"			/* home directory */
#define SHELL_ENVIRON	"SHELL"			/* for the type of shell */
#define DEFAULT_CONFIG	"%s/.mallocrc"		/* default config file */
#define TOKENIZE_CHARS	" \t,="			/* for tag lines */

#define NO_VALUE		(-1)		/* no value ... value */
#define TOKENS_PER_LINE		5		/* num debug toks per line */

/* local variables */
LOCAL	char	printed		= FALSE;	/* did we outputed anything? */

/* argument variables */
LOCAL	char	*address	= NULL;		/* for ADDRESS */
LOCAL	char	bourne		= FALSE;	/* set bourne shell output */
LOCAL	char	cshell		= FALSE;	/* set c-shell output */
LOCAL	char	clear		= FALSE;	/* clear variables */
LOCAL	int	debug		= NO_VALUE;	/* for DEBUG */
LOCAL	int	errno_to_print	= NO_VALUE;	/* to print the error string */
LOCAL	char	*inpath		= NULL;		/* for config-file path */
LOCAL	int	interval	= NO_VALUE;	/* for setting INTERVAL */
LOCAL	char	keep		= FALSE;	/* keep settings override -r */
LOCAL	char	list_tags	= FALSE;	/* list rc tags */
LOCAL	char	list_tokens	= FALSE;	/* list debug tokens */
LOCAL	char	*logpath	= NULL;		/* for LOGFILE setting */
LOCAL	argv_array_t	minus;			/* tokens to remove */
LOCAL	argv_array_t	plus;			/* tokens to add */
LOCAL	char	remove_auto	= FALSE;	/* auto-remove settings */
LOCAL	char	*start		= NULL;		/* for START settings */
LOCAL	char	verbose		= FALSE;	/* verbose flag */
LOCAL	char	*tag		= NULL;		/* maybe a tag argument */

LOCAL	argv_t	args[] = {
  { 'a',	"address",	ARGV_CHARP,	&address,
      "address:#",		"stop when malloc sees address" },
  { 'b',	"bourne",	ARGV_BOOL,	&bourne,
      NULL,			"set output for bourne shells" },
  { ARGV_OR },
  { 'C',	"c-shell",	ARGV_BOOL,	&cshell,
      NULL,			"set output for C-type shells" },
  { 'c',	"clear",	ARGV_BOOL,	&clear,
      NULL,			"clear all variables not set" },
  { 'd',	"debug-mask",	ARGV_HEX,	&debug,
      "value",			"hex flag to set debug mask" },
  { 'e',	"errno",	ARGV_INT,	&errno_to_print,
      "errno",			"print error string for errno" },
  { 'f',	"file",		ARGV_CHARP,	&inpath,
      "path",			"config if not ~/.mallocrc" },
  { 'i',	"interval",	ARGV_INT,	&interval,
      "value",			"check heap every number times" },
  { 'k',	"keep",		ARGV_BOOL,	&keep,
      NULL,			"keep settings (override -r)" },
  { 'l',	"logfile",	ARGV_CHARP,	&logpath,
      "path",			"file to log messages to" },
  { 'L',	"list-tags",	ARGV_BOOL,	&list_tags,
      NULL,			"list tags in rc file" },
  { 'm',	"minus",	ARGV_CHARP | ARGV_ARRAY,	&minus,
      "token(s)",		"del tokens from current debug" },
  { 'p',	"plus",		ARGV_CHARP | ARGV_ARRAY,	&plus,
      "token(s)",		"add tokens to current debug" },
  { 'r',	"remove",	ARGV_BOOL,	&remove_auto,
      NULL,			"remove other settings if tag" },
  { 's',	"start",	ARGV_CHARP,	&start,
      "file:line",		"start check heap after this" },
  { 'T',	"list-tokens",	ARGV_BOOL,	&list_tokens,
      NULL,			"list debug tokens" },
  { 'v',	"verbose",	ARGV_BOOL,	&verbose,
      NULL,			"turn on verbose output" },
  { ARGV_MAYBE,	NULL,		ARGV_CHARP,	&tag,
      "tag",			"debug token to find in rc" },
  { ARGV_LAST }
};

/*
 * list of bourne shells
 */
LOCAL	char	*sh_shells[] = { "sh", "ash", "bash", "ksh", "zsh", NULL };

/*
 * try a check out the shell env variable to see what form of shell
 * commands we should output
 */
LOCAL	void	choose_shell(void)
{
  const char	*shell, *shellp;
  int		shellc;
  
  shell = (const char *)getenv(SHELL_ENVIRON);
  if (shell == NULL) {
    cshell = TRUE;
    return;
  }
  
  shellp = (char *)rindex(shell, '/');
  if (shellp == NULL)
    shellp = shell;
  else
    shellp++;
  
  for (shellc = 0; sh_shells[shellc] != NULL; shellc++)
    if (strcmp(sh_shells[shellc], shellp) == 0) {
      bourne = TRUE;
      return;
    }
  
  cshell = TRUE;
}

/*
 * hexadecimal STR to long translation
 */
LOCAL	long	hex_to_long(const char * str)
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
 * dump the current flags set in the debug variable VAL
 */
LOCAL	void	dump_debug(const int val)
{
  attr_t	*attrp;
  int		tokc = 0, work = val;
  
  if (val == 0) {
    (void)fprintf(stderr, "   none\n");
    return;
  }
  
  for (attrp = attributes; attrp->at_string != NULL; attrp++) {
    if (attrp->at_value != 0 && (work & attrp->at_value) == attrp->at_value) {
      if (tokc == 0)
	(void)fprintf(stderr, "   %s", attrp->at_string);
      else if (tokc == TOKENS_PER_LINE - 1)
	(void)fprintf(stderr, ", %s\n", attrp->at_string);
      else
	(void)fprintf(stderr, ", %s", attrp->at_string);
      tokc = (tokc + 1) % TOKENS_PER_LINE;
      work &= ~attrp->at_value;
    }
  }
  
  if (tokc != 0)
    (void)fprintf(stderr, "\n");
  
  if (work != 0)
    (void)fprintf(stderr, "%s: warning, unknown debug flags: %#x\n",
		  argv_program, work);
}

/*
 * translate TOK into its proper value which is returned
 */
LOCAL	long	token_to_value(const char * tok)
{
  int	attrc;
  long	ret = 0;
  
  for (attrc = 0; attributes[attrc].at_string != NULL; attrc++) {
    if (strcmp(tok, attributes[attrc].at_string) == 0)
      break;
  }
  
  if (attributes[attrc].at_string == NULL) {
    (void)fprintf(stderr, "%s: unknown token '%s'\n",
		  argv_program, tok);
  }
  else
    ret = attributes[attrc].at_value;
  
  return ret;
}

/*
 * process the user configuration looking for the TAG_FIND.  if it is
 * null then look for DEBUG_VALUE in the file and return the token for
 * it in STRP.  routine returns the new debug value matching tag.
 */
LOCAL	long	process(const long debug_value, const char * tag_find,
			char ** strp)
{
  static char	token[128];
  FILE		*infile = NULL;
  char		path[1024], buf[1024];
  const char	*homep;
  char		found, cont;
  int		new_debug = 0;
  
  /* do we need to have a home variable? */
  if (inpath == NULL) {
    homep = (const char *)getenv(HOME_ENVIRON);
    if (homep == NULL) {
      (void)fprintf(stderr, "%s: could not find variable '%s'\n",
		    argv_program, HOME_ENVIRON);
      exit(1);
    }
    
    (void)sprintf(path, DEFAULT_CONFIG, homep);
    inpath = path;
  }
  
  infile = fopen(inpath, "r");
  if (infile == NULL) {
    (void)fprintf(stderr, "%s: could not read '%s': ", argv_program, inpath);
    perror("");
    exit(1);
  }
  
  /* read each of the lines looking for the tag */
  found = FALSE;
  cont = FALSE;
  
  while (fgets(buf, sizeof(buf), infile) != NULL) {
    char	*tokp, *endp;
    
    /* ignore comments and empty lines */
    if (buf[0] == '#' || buf[0] == '\n')
      continue;
    
    /* chop off the ending \n */
    endp = (char *)rindex(buf, '\n');
    if (endp != NULL)
      *endp = NULLC;
    
    /* get the first token on the line */
    tokp = (char *)strtok(buf, TOKENIZE_CHARS);
    if (tokp == NULL)
      continue;
    
    /* if we're not continuing then we need to process a tag */
    if (! cont) {
      (void)strcpy(token, tokp);
      new_debug = 0;
      
      if (tag_find != NULL && strcmp(tag_find, tokp) == 0)
	found = TRUE;
      
      tokp = (char *)strtok(NULL, TOKENIZE_CHARS);
    }
    
    cont = FALSE;
    
    do {
      /* do we have a continuation character */
      if (strcmp(tokp, "\\") == 0) {
	cont = TRUE;
	break;
      }
      
      /* are we processing the tag of choice? */
      if (found || tag_find == NULL)
	new_debug |= token_to_value(tokp);
      
      tokp = (char *)strtok(NULL, TOKENIZE_CHARS);
    } while (tokp != NULL);
    
    if (list_tags && ! cont) {
      if (verbose) {
	(void)fprintf(stderr, "%s:\n", token);
	dump_debug(new_debug);
      }
      else
	(void)fprintf(stderr, "%s\n", token);
    }
    else if (tag_find == NULL && ! cont && new_debug == debug_value) {
      found = TRUE;
      if (strp != NULL)
	*strp = token;
    }
    
    /* are we done? */
    if (found && ! cont)
      break;
  }
  
  (void)fclose(infile);
  
  /* did we find the correct value in the file? */
  if (tag_find == NULL && ! found) {
    if (strp != NULL)
      *strp = "unknown";
  }
  else if (! found && tag_find != NULL) {
    (void)fprintf(stderr, "%s: could not find tag '%s' in '%s'\n",
		  argv_program, tag_find, inpath);
    exit(1);
  }
  
  return new_debug;
}

/*
 * dump the current settings of the malloc variables
 */
LOCAL	void	dump_current(void)
{
  const char	*str;
  char		*tokp;
  int		num;
  
  str = (const char *)getenv(DEBUG_ENVIRON);
  if (str == NULL)
    (void)fprintf(stderr, "%s not set\n", DEBUG_ENVIRON);
  else {
    num = hex_to_long(str);
    (void)process(num, NULL, &tokp);
    (void)fprintf(stderr, "%s == '%#lx' (%s)\n", DEBUG_ENVIRON, num, tokp);
    
    if (verbose)
      dump_debug(num);
  }
  
  str = (const char *)getenv(ADDRESS_ENVIRON);
  if (str == NULL)
    (void)fprintf(stderr, "%s not set\n", ADDRESS_ENVIRON);
  else
    (void)fprintf(stderr, "%s == '%s'\n", ADDRESS_ENVIRON, str);
  
  str = (const char *)getenv(INTERVAL_ENVIRON);
  if (str == NULL)
    (void)fprintf(stderr, "%s not set\n", INTERVAL_ENVIRON);
  else {
    num = atoi(str);
    (void)fprintf(stderr, "%s == '%d'\n", INTERVAL_ENVIRON, num);
  }
  
  str = (const char *)getenv(LOGFILE_ENVIRON);
  if (str == NULL)
    (void)fprintf(stderr, "%s not set\n", LOGFILE_ENVIRON);
  else
    (void)fprintf(stderr, "%s == '%s'\n", LOGFILE_ENVIRON, str);
  
  str = (const char *)getenv(START_ENVIRON);
  if (str == NULL)
    (void)fprintf(stderr, "%s not set\n", START_ENVIRON);
  else
    (void)fprintf(stderr, "%s == '%s'\n", START_ENVIRON, str);
}

/*
 * output the code to set env VAR to VALUE
 */
LOCAL	void	set_variable(char * var, char * value)
{
  if (bourne) {
    (void)printf("%s=%s; export %s;\n", var, value, var);
    if (verbose)
      (void)fprintf(stderr, "Outputed: %s=%s; export %s;\n", var, value, var);
  }
  else {
    (void)printf("setenv %s %s;\n", var, value);
    if (verbose)
      (void)fprintf(stderr, "Outputed: setenv %s %s;\n", var, value);
  }
  
  printed = TRUE;
}

/*
 * output the code to un-set env VAR
 */
LOCAL	void	unset_variable(char * var)
{
  if (bourne) {
    (void)printf("unset %s;\n", var);
    if (verbose)
      (void)fprintf(stderr, "Outputed: unset %s;\n", var);
  }
  else {
    (void)printf("unsetenv %s;\n", var);
    if (verbose)
      (void)fprintf(stderr, "Outputed: unsetenv %s;\n", var);
  }
  
  printed = TRUE;
}

/*
 * returns the string for ERRNUM
 */
LOCAL	char	*local_strerror(const int errnum)
{
  if (! IS_MALLOC_ERRNO(errnum))
    return malloc_errlist[ERROR_BAD_ERRNO];
  else
    return malloc_errlist[errnum];
}

EXPORT	int	main(int argc, char ** argv)
{
  char	buf[20];
  char	debug_set = FALSE;
  
  argv_help_string = "Sets malloc_dbg library env variables.";
  argv_version_string = malloc_version;
  
  argv_process(args, argc, argv);
  
  /* try to figure out the shell we are using */
  if (! bourne && ! cshell)
    choose_shell();
  
  /* get a new debug value from tag */
  if (tag != NULL) {
    if (debug != NO_VALUE)
      (void)fprintf(stderr, "%s: warning -d ignored, processing tag '%s'\n",
		    argv_program, tag);
    debug = process(0L, tag, NULL);
    debug_set = TRUE;
  }
  
  if (plus.aa_entryn > 0) {
    const char	*str;
    int		plusc;
    
    /* get current debug value and add tokens if possible */
    if (debug == NO_VALUE) {
      str = (const char *)getenv(DEBUG_ENVIRON);
      if (str == NULL)
	debug = 0;
      else
	debug = hex_to_long(str);
    }
    
    for (plusc = 0; plusc < plus.aa_entryn; plusc++)
      debug |= token_to_value(ARGV_ARRAY_ENTRY(plus, char *, plusc));
  }
  
  if (minus.aa_entryn > 0) {
    const char	*str;
    int		minusc;
    
    /* get current debug value and add tokens if possible */
    if (debug == NO_VALUE) {
      str = (const char *)getenv(DEBUG_ENVIRON);
      if (str == NULL)
	debug = 0;
      else
	debug = hex_to_long(str);
    }
    
    for (minusc = 0; minusc < minus.aa_entryn; minusc++)
      debug &= ~token_to_value(ARGV_ARRAY_ENTRY(minus, char *, minusc));
  }
  
  if (debug != NO_VALUE) {
    (void)sprintf(buf, "%#lx", debug);
    set_variable(DEBUG_ENVIRON, buf);
    
    /* should we clear the rest? */
    if (debug_set && remove_auto && ! keep)
      clear = TRUE;
  }
  
  if (address != NULL)
    set_variable(ADDRESS_ENVIRON, address);
  else if (clear)
    unset_variable(ADDRESS_ENVIRON);
  
  if (interval != NO_VALUE) {
    (void)sprintf(buf, "%d", interval);
    set_variable(INTERVAL_ENVIRON, buf);
  }
  else if (clear)
    unset_variable(INTERVAL_ENVIRON);
  
  if (logpath != NULL)
    set_variable(LOGFILE_ENVIRON, logpath);
  else if (clear)
    unset_variable(LOGFILE_ENVIRON);
  
  if (start != NULL)
    set_variable(START_ENVIRON, start);
  else if (clear)
    unset_variable(START_ENVIRON);
  
  if (errno_to_print != NO_VALUE) {
    (void)fprintf(stderr, "%s: malloc_errno value '%d' = \n",
		  argv_program, errno_to_print);
    (void)fprintf(stderr, "   '%s'\n", local_strerror(errno_to_print));
    printed = TRUE;
  }
  
  if (list_tags || list_tokens) {
    if (list_tags) {
      (void)fprintf(stderr, "Available Tags:\n");
      process(0L, NULL, NULL);
    }
    if (list_tokens) {
      attr_t	*attrp;
      (void)fprintf(stderr, "Debug Tokens:\n");
      for (attrp = attributes; attrp->at_string != NULL; attrp++)
	(void)fprintf(stderr, "%s\n", attrp->at_string);
    }
  }
  else if (! printed)
    dump_current();
  
  argv_cleanup(args);
  
  exit(0);
}
