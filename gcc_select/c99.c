/*-
 * Copyright (c) 2002 Tim J. Robbins.
 * Copyright (c) 2007 Apple Inc.
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
 * c99 -- compile standard C programs
 *
 * This is essentially a wrapper around the system C compiler that forces
 * the compiler into C99 mode and handles some of the standard libraries
 * specially.
 */

#include <sys/cdefs.h>
/* __FBSDID("$FreeBSD: src/usr.bin/c99/c99.c,v 1.2 2002/10/08 02:19:54 tjr Exp $");*/

#include <sys/types.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>

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

int
main(int argc, char *argv[])
{
	int link = 1;
	int inputs = 0;
	int verbose = 0;

	addarg("cc");
	addarg("-std=iso9899:1999");
	addarg("-pedantic");
	addarg("-m32");
	addarg("-Wextra-tokens"); /* Radar 4205857 */
#if defined (__ppc__) || defined (__ppc64__)
	/* on ppc long double doesn't work */
	addarg("-mlong-double-64");
#endif
	addarg("-fmath-errno");  /* Radar 4011622 */
	addarg("-fno-builtin-pow");
	addarg("-fno-builtin-powl");
	addarg("-fno-builtin-powf");

	for (;;)
	  {
	    int ch = getopt(argc, argv, "cD:EgI:L:o:O:sU:W:l:");
	    if (optind >= argc && ch == -1)
	      break;
	    switch (ch)
	      {
	      case 'c':
		addarg ("-c");
		link = 0;
		break;
	      case 'D':
		add_def (optarg);
		break;
	      case 'E':
		addarg ("-E");
		link = 0;
		break;
	      case 'g':
		addarg ("-g");
		break;
	      case 'I':
		combine_and_addarg ("-I", optarg);
		break;
	      case 'L':
		combine_and_addarg ("-L", optarg);
		break;
	      case 'o':
		addarg ("-o");
		addarg (optarg);
		break;
	      case 'O':
		combine_and_addarg ("-O", optarg);
		break;
	      case 's':
		addarg ("-s");
		break;
	      case 'U':
		record_undef (optarg);
		combine_and_addarg ("-U", optarg);
		break;
	      case 'W':
		if (strcmp (optarg, "64") == 0)
		  addarg ("-m64");
		else if (strcmp (optarg, "verbose") == 0)
		  {
		    addarg ("-v");
		    verbose = 1;
		  }
		else
		  errx(EX_USAGE, "invalid argument `%s' to -W", optarg);
		break;
	      case 'l':
		addlib (optarg);
		break;
	      case -1:
		if (strcmp (argv[optind-1], "--") == 0)
		  {
		    while (optind < argc)
		      {
			if (argv[optind][0] == '-')
			  combine_and_addarg ("./", argv[optind]);
			else
			  addarg (argv[optind]);
			inputs++;
			optind++;
		      }
		  }
		else
		  {
		    addarg (argv[optind++]);
		    inputs++;
		  }
		break;
	      case '?':
		usage ();
		break;
	      }
	  }

	if (link && inputs > 0) {
	  addarg("-liconv");
	}
	if (verbose)
	  {
	    int i;
	    for (i = 0; args[i]; i++)
	      printf ("\"%s\" ", args[i]);
	    putchar ('\n');
	  }
	execv("/usr/bin/cc", args);
	err(EX_OSERR, "/usr/bin/cc");
}

/* Combine item1 and item2 and used them as one argument.
   This is used to combine "-O" with "1" to make "-O1".  */
void
combine_and_addarg (const char *item1, const char *item2)
{
  char *item = (char *) malloc (sizeof (char) * (strlen(item1) + strlen(item2) + 1));
  if (item == NULL)
    err (EX_OSERR, "malloc");
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
    undef_args = realloc(undef_args, sizeof(*undef_args) * undef_cargs);
    if (undef_args == NULL)
      err(EX_OSERR, "malloc");
  }

  if ((undef_args[undef_nargs++] = strdup(item)) == NULL)
    err(EX_OSERR, "strdup");
  undef_args[undef_nargs] = NULL;
}

/* Add command line macro def (-Dblah) in the argument list.
   If it is was undefined earlier than do not add.  Radar 3762036.  */
void
add_def (const char *item)
{
  u_int i;
  const char * equ_pos;
  
  equ_pos = strchr (item, '=');
  if (equ_pos == NULL)
    equ_pos = item + strlen (item);

  for (i = 0; i < undef_nargs; i++)
    if (strncmp (item, undef_args[i], equ_pos - item) == 0
	&& undef_args[i][equ_pos - item] == '\0')
      return;
  combine_and_addarg ("-D", item);
}

void
addarg(const char *item)
{
  if (nargs + 2 > cargs) {
    cargs += 16;
    if ((args = realloc(args, sizeof(*args) * cargs)) == NULL)
      err(EX_OSERR, "malloc");
  }
  
  args[nargs++] = strdup (item);
  
  if (args[nargs-1] == NULL)
    err(EX_OSERR, "strdup");
  args[nargs] = NULL;
}

void
addlib(const char *lib)
{
  if (strcmp(lib, "pthread") == 0)
    /* pthread functionality is in libc. */
    return;
  if (strcmp(lib, "rt") == 0)
    /* librt functionality is in libc or unimplemented. */
    return;
  if (strcmp(lib, "xnet") == 0)
    /* xnet functionality is in libc. */
    return;

  combine_and_addarg("-l", lib);
}

void
usage(void)
{
	fprintf(stderr,
"usage: c99 [-cEgs] [-D name[=value]] [-I directory] ... [-L directory] ...\n");
	fprintf(stderr,
"       [-o outfile] [-O optlevel] [-U name]... [-W 64] operand ...\n");
	exit(1);
}
