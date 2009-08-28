/*-
 * Copyright (c) 2002 Tim J. Robbins.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * c89 -- compile standard C programs
 *
 * This is essentially a wrapper around the system C compiler that forces
 * the compiler into C89 mode and handles some of the standard libraries
 * specially.
 */

#include <sys/cdefs.h>
/* __FBSDID("$FreeBSD: src/usr.bin/c89/c89.c,v 1.2 2002/10/08 02:19:54 tjr Exp $");*/

#include <sys/types.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char **args;
u_int cargs, nargs;

void addarg(const char *);
void addlib(const char *);
void combine_and_addarg (const char *, const char *);
void usage(void);

/* Store -U args.  */
char **undef_args;
u_int undef_cargs, undef_nargs;
void record_undef(const char *);

void add_def(const char *);
void combine_and_addarg (const char *, const char *);

int dash_dash_seen = 0;

int
main(int argc, char *argv[])
{
	int ch, i;

	args = NULL;
	cargs = nargs = 0;

	while ((ch = getopt(argc, argv, "cD:EgI:L:o:O:sU:l:")) != -1) {
		if (ch == 'l') {
			/* Gone too far. Back up and get out. */
			if (argv[optind - 1][0] == '-')
				optind -= 1;
			else
				optind -= 2;
			break;
		} else if (ch == '?')
			usage();
	}

	addarg("cc");
	addarg("-std=iso9899:1990");
	addarg("-pedantic");
	addarg("-m32");
	for (i = 1; i < optind; i++) {
	  /* "--" indicates end of options. Radar 3761967.  */
	  if (strcmp (argv[i], "--") == 0)
	    dash_dash_seen = 1;
	  /* White space is OK between -O and 1 or 2. Radar 3762315.  */
	  else if (strcmp (argv[i], "-O") == 0) {
	    if (i+1 < argc) {
	      if (strcmp (argv[i+1], "1") == 0 || strcmp (argv[i+1], "0") == 0) {
		combine_and_addarg(argv[i], argv[i+1]);
		i++;
	      } else
		addarg(argv[i]);
	    }
	  } else if (strcmp (argv[i], "-U") == 0) {
	    /* Record undefined macros.  */
	    record_undef (argv[i+1]);
	    addarg(argv[i]);
	    addarg(argv[i+1]);
	    i++;
	  } else if (strncmp (argv[i], "-U", 2) == 0) {
	    /* Record undefined macros.  */
	    record_undef (argv[i]+2);
	    addarg(argv[i]);
	  } else if (strcmp (argv[i], "-L") == 0 && i+1 < argc) {
	    combine_and_addarg(argv[i], argv[i+1]);
	    i++;
	  } else if (strcmp (argv[i], "-D") == 0) {
	    add_def (argv[i+1]);
	    i++;
	  } else if (strncmp (argv[i], "-D", 2) == 0)
	    add_def (argv[i]+2);
	  else
	    addarg(argv[i]);
	}
	while (i < argc) {
		if (strncmp(argv[i], "-l", 2) == 0) {
			if (argv[i][2] != '\0')
				addlib(argv[i++] + 2);
			else {
				if (argv[++i] == NULL)
					usage();
				addlib(argv[i++]);
			}
		}
		else if (strcmp (argv[i], "-L") == 0 && i+1 < argc) {
		    combine_and_addarg(argv[i], argv[i+1]);
		    i+=2;
		}
		else if (strcmp (argv[i], "--") == 0) {
		  dash_dash_seen = 1;
		} else
		  addarg(argv[i++]);
	}
	execv("/usr/bin/cc", args);
	err(1, "/usr/bin/cc");
}

/* Combine item1 and item2 and used them as one argument.
   This is used to combine "-O" with "1" to make "-O1".  */
void
combine_and_addarg (const char *item1, const char *item2)
{
  char *item = (char *) malloc (sizeof (char) * (strlen(item1) + strlen(item2) + 1));
  strcpy (item, item1);
  strcat (item, item2);
  addarg (item);
}

/* Record macro undefs on the command line. -U. */
void 
record_undef (const char *item)
{
  if (undef_nargs + 1 > undef_cargs) {
    undef_cargs += 16;
    if ((undef_args = realloc(undef_args, sizeof(*undef_args) * undef_cargs)) == NULL)
      err(1, "malloc");
  }

  if ((undef_args[undef_nargs++] = strdup(item)) == NULL)
    err(1, "strdup");
  undef_args[undef_nargs] = NULL;
}

/* Add command line macro def (-Dblah) in the argument list.
   If it is was undefined earlier than do not add.  Radar 3762036.  */
void
add_def (const char *item)
{
  int i;
  char *updated_def;
  if (strlen (item) > 2)
    for (i = 0; i < undef_nargs; i++)
      {
	if (strcmp (item, undef_args[i]) == 0)
	  return;
	else {
	  int undef_len = strlen (undef_args[i]);
	  int def_len = strlen (item);
	  if (undef_len < def_len) {
	    const char *p = item + undef_len;
	    if (*p == '=') {
	      if (strncmp (item, undef_args[i], undef_len) == 0)
		return;
	    }
	  }
	    
	}
      }
  updated_def = (char *) malloc (strlen (item) + 3);
  sprintf (updated_def,"-D%s",item);
  addarg (updated_def);
}

void
addarg(const char *item)
{
  if (nargs + 2 > cargs) {
    cargs += 16;
    if ((args = realloc(args, sizeof(*args) * cargs)) == NULL)
      err(1, "malloc");
  }
  
  if (dash_dash_seen && strncmp (item, "-", 1) == 0)
    {
      char *updated_item = (char *) malloc (strlen (item) + 3);
      sprintf (updated_item,"./%s",item);
      args[nargs++] = updated_item;
    }
  else 
    {
      args[nargs++] = strdup (item);
    }
  
  if (args[nargs-1] == NULL)
    err(1, "strdup");
  args[nargs] = NULL;
}

void
addlib(const char *lib)
{

	if (strcmp(lib, "pthread") == 0)
		/* pthread functionality is in libc. */
                ;
	else if (strcmp(lib, "rt") == 0)
		/* librt functionality is in libc or unimplemented. */
		;
	else if (strcmp(lib, "xnet") == 0)
		/* xnet functionality is in libc. */
		;
	else {
		addarg("-l");
		addarg(lib);
	}
}

void
usage(void)
{
	fprintf(stderr,
"usage: c89 [-cEgs] [-D name[=value]] [-I directory] ... [-L directory] ...\n");
	fprintf(stderr,
"       [-o outfile] [-O optlevel] [-U name]... operand ...\n");
	exit(1);
}
