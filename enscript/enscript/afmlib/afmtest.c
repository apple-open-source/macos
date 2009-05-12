/*
 * Tester program for the AFM library.
 * Copyright (c) 1995-1999 Markku Rossi.
 *
 * Author: Markku Rossi <mtr@iki.fi>
 */

/*
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

#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if STDC_HEADERS
#include <stdlib.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#include "afm.h"

/*
 * Types and definitions.
 */

#define HANDLE_ERROR(msg)				\
  if (error != AFM_SUCCESS)				\
    {							\
      char buf[256];					\
      afm_error_to_string (error, buf);			\
      fprintf (stderr, "afmtest: %s: %s\n", msg, buf);	\
      exit (1);						\
    }


/*
 * Prototypes for static functions.
 */

static void usage ();


/*
 * Static variables
 */

static char *program;


/*
 * Global functions.
 */

int
main (int argc, char *argv[])
{
  AFMHandle afm;
  AFMFont font;
  AFMError error;
  AFMNumber width, height;
  char buf[256];

  program = strrchr (argv[0], '/');
  if (program)
    program++;
  else
    program = argv[0];

  error = afm_create (NULL, 0, &afm);
  HANDLE_ERROR ("couldn't create library");

  if (argc < 2)
    {
      usage ();
      exit (1);
    }

  if (strcmp (argv[1], "dump") == 0 && argc == 3)
    {
      error = afm_open_file (afm, AFM_I_ALL, argv[2], &font);
      if (error != AFM_SUCCESS)
	{
	  fprintf (stderr, "%s: couldn't open font \"%s\", using default\n",
		   program, argv[2]);
	  error = afm_open_default_font (afm, &font);
	  HANDLE_ERROR ("couldn't open default font");
	}

      afm_font_dump (stdout, font);

      error = afm_close_font (font);
      HANDLE_ERROR ("couldn't close font");
    }
  else if (strcmp (argv[1], "stringwidth") == 0 && argc == 5)
    {
      error = afm_open_file (afm, AFM_I_ALL, argv[2], &font);
      HANDLE_ERROR ("couldn't open font");

      error = afm_font_encoding (font, AFM_ENCODING_ISO_8859_1, 0);
      HANDLE_ERROR ("couldn't encode font");

      error = afm_font_stringwidth (font, atof (argv[3]), argv[4],
				    strlen (argv[4]), &width, &height);
      printf ("stringwidth is [%g %g]\n", width, height);

      error = afm_close_font (font);
      HANDLE_ERROR ("couldn't close font");
    }
  else if (strcmp (argv[1], "chardump") == 0 && argc > 2)
    {
      int i, j;

      for (i = 2; i < argc; i++)
	{
	  error = afm_open_file (afm, AFM_I_COMPOSITES, argv[i], &font);
	  if (error != AFM_SUCCESS)
	    {
	      afm_error_to_string (error, buf);
	      fprintf (stderr, "%s: couldn't open AFM file \"%s\": %s\n",
		       program, argv[i], buf);
	      continue;
	    }

	  for (j = 0; j < font->num_character_metrics; j++)
	    {
	      AFMIndividualCharacterMetrics *cm;
	      cm = &font->character_metrics[j];

	      printf ("/%-30s %3ld glyph %s\n", cm->name, cm->character_code,
		      font->global_info.FontName);
	    }

	  for (j = 0; j < font->num_composites; j++)
	    {
	      AFMComposite *cc;
	      cc = &font->composites[j];

	      printf ("/%-30s -1 composite %s\n", cc->name,
		      font->global_info.FontName);
	    }

	  (void) afm_close_font (font);
	}
    }
  else
    {
      usage ();
      exit (1);
    }

  return 0;
}


/*
 * Static functions.
 */

static void
usage ()
{
  fprintf (stderr,
	   "Usage: %s dump file\n"
	   "       %s stringwidth file ptsize string\n"
	   "       %s chardump file [file ...]\n",
	   program, program, program);
}
