/*
 * Create font map for AFM files.
 * Copyright (c) 1995, 1996, 1997 Markku Rossi.
 *
 * Author: Markku Rossi <mtr@iki.fi>
 */

/*
 * This file is part of GNU enscript.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#if ENABLE_NLS
#include <libintl.h>
#define _(String) gettext (String)
#else
#define _(String) String
#endif

#if HAVE_LC_MESSAGES
#include <locale.h>
#endif

#include "afm.h"
#include "getopt.h"


/*
 * Definitions.
 */

#define HANDLE_ERROR(msg)					\
  if (error != AFM_SUCCESS)					\
    {								\
      char buf[256];						\
      afm_error_to_string (error, buf);				\
      fprintf (stderr, "%s: %s: %s\n", program, msg, buf);	\
      exit (1);							\
    }


/*
 * Prototypes for static functions.
 */

static void usage ();


/*
 * Static variables.
 */

/* Options. */

/*
 * --output-file, -p
 *
 * The name of the file to which font map is stored.  If name is NULL,
 * leaves output to stdout.
 */
static char *fname = "font.map";


static char *program;

static struct option long_options[] =
{
  {"output-file",	required_argument,	0, 'p'},
  {"help",		no_argument,		0, 'h'},
  {"version",		no_argument,		0, 'V'},
  {NULL, 0, 0, 0},
};

/*
 * Global functions.
 */

int
main (int argc, char *argv[])
{
  AFMError error;
  AFMHandle afm;
  AFMFont font;
  int i;
  FILE *ofp;
  FILE *mfp;

  program = strrchr (argv[0], '/');
  if (program == NULL)
    program = argv[0];
  else
    program++;

  /* Make getopt_long() to use our modified programname. */
  argv[0] = program;

  /* Internationalization. */
#if HAVE_SETLOCALE
#if HAVE_LC_MESSAGES
  setlocale (LC_MESSAGES, "");
#endif
#endif
#if ENABLE_NLS
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
#endif

  /* Handle arguments. */
  while (1)
    {
      int option_index = 0;
      int c;

      c = getopt_long (argc, argv, "p:h", long_options, &option_index);
      if (c == -1)
	break;

      switch (c)
	{
	case 'h':		/* help */
	  usage ();
	  exit (0);

	case 'p':		/* output file */
	  /* Check output file "-". */
	  if (strcmp (optarg, "-") == 0)
	    fname = NULL;
	  else
	    fname = optarg;
	  break;

	case 'V':		/* version number */
	  printf ("%s for GNU %s %s\n", program, PACKAGE, VERSION);
	  exit (0);
	  break;

	case '?':		/* errors in arguments */
	  usage ();
	  exit (1);
	  break;
	}
    }

  /* Open output file. */
  printf (_("file=%s\n"), fname ? fname : _("stdout"));
  if (fname)
    {
      ofp = fopen (fname, "w");
      if (ofp == NULL)
	{
	  char buf[256];

	  sprintf (buf, _("%s: couldn't open output file \"%s\""),
		   program, fname);
	  perror (buf);
	  exit (1);
	}
      mfp = stdout;
    }
  else
    {
      ofp = stdout;
      mfp = stderr;
    }

  error = afm_create (NULL, 0, &afm);
  HANDLE_ERROR (_("couldn't create AFM library"));

  for (i = optind; i < argc; i++)
    {
      fprintf (mfp, "%s...\n", argv[i]);
      error = afm_open_file (afm, AFM_I_MINIMUM, argv[i], &font);
      if (error == AFM_SUCCESS)
	{
	  char *cp;
	  char *sf;
	  int len;

	  cp = strrchr (argv[i], '/');
	  if (cp == NULL)
	    cp = argv[i];
	  else
	    cp++;

	  sf = strrchr (argv[i], '.');
	  if (sf)
	    len = sf - cp;
	  else
	    len = strlen (cp);

	  fprintf (ofp, "%-30s\t%.*s\n", font->global_info.FontName, len, cp);
	  (void) afm_close_font (font);
	}
      else
	{
	  char buf[256];
	  afm_error_to_string (error, buf);
	  fprintf (mfp, "%s: %s\n", program, buf);
	}
    }

  if (fname)
    fclose (ofp);

  return 0;
}


/*
 * Static functions.
 */

static void
usage ()
{
  printf (_("\
Usage: %s [OPTION]... FILE...\n\
Mandatory arguments to long options are mandatory for short options too.\n\
  -h, --help              print this help and exit\n\
  -p, --output-file=NAME  print output to file NAME (default file is\n\
                          font.map).  If FILE is `-', leavy output to\n\
                          stdout.\n\
  -V, --version           print version number\n"), program);
}
