/* idn.c --- Command line interface to libidn.
 * Copyright (C) 2003, 2004, 2005, 2006, 2007  Simon Josefsson
 *
 * This file is part of GNU Libidn.
 *
 * GNU Libidn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GNU Libidn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Libidn; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <locale.h>

/* Gnulib headers. */
#include "error.h"
#include "gettext.h"
#define _(String) dgettext (PACKAGE, String)

/* Libidn headers. */
#include <stringprep.h>
#include <punycode.h>
#include <idna.h>
#ifdef WITH_TLD
# include <tld.h>
#endif

#include "idn_cmd.h"

#define GREETING \
  "Copyright 2002, 2003, 2004, 2005, 2006, 2007 Simon Josefsson.\n"	\
  "GNU Libidn comes with NO WARRANTY, to the extent permitted by law.\n" \
  "You may redistribute copies of GNU Libidn under the terms of\n"	\
  "the GNU Lesser General Public License.  For more information\n"	\
  "about these matters, see the file named COPYING.LIB.\n"

/* For error.c */
char *program_name;

int
main (int argc, char *argv[])
{
  struct gengetopt_args_info args_info;
  char readbuf[BUFSIZ];
  char *p, *r;
  uint32_t *q;
  unsigned cmdn = 0;
  int rc;

  setlocale (LC_ALL, "");
  program_name = argv[0];
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  if (cmdline_parser (argc, argv, &args_info) != 0)
    return EXIT_FAILURE;

  if (!args_info.stringprep_given &&
      !args_info.punycode_encode_given && !args_info.punycode_decode_given &&
      !args_info.idna_to_ascii_given && !args_info.idna_to_unicode_given)
    args_info.idna_to_ascii_given = 1;

  if ((args_info.stringprep_given ? 1 : 0) +
      (args_info.punycode_encode_given ? 1 : 0) +
      (args_info.punycode_decode_given ? 1 : 0) +
      (args_info.idna_to_ascii_given ? 1 : 0) +
      (args_info.idna_to_unicode_given ? 1 : 0) != 1)
    {
      error (0, 0, _("Only one of -s, -e, -d, -a or -u can be specified."));
      cmdline_parser_print_help ();
      return EXIT_FAILURE;
    }

  if (!args_info.quiet_given)
    fprintf (stderr, "%s %s\n" GREETING, PACKAGE, VERSION);

  if (args_info.debug_given)
    fprintf (stderr, _("Charset `%s'.\n"), stringprep_locale_charset ());

  if (!args_info.quiet_given && args_info.inputs_num == 0)
    fprintf (stderr, _("Type each input string on a line by itself, "
		       "terminated by a newline character.\n"));

  do
    {
      if (cmdn < args_info.inputs_num)
	{
	  strncpy (readbuf, args_info.inputs[cmdn++], BUFSIZ - 1);
	  readbuf[BUFSIZ - 1] = '\0';
	}
      else if (fgets (readbuf, BUFSIZ, stdin) == NULL)
	{
	  if (feof (stdin))
	    break;

	  error (EXIT_FAILURE, errno, _("Input error"));
	}

      if (readbuf[strlen (readbuf) - 1] == '\n')
	readbuf[strlen (readbuf) - 1] = '\0';

      if (args_info.stringprep_given)
	{
	  p = stringprep_locale_to_utf8 (readbuf);
	  if (!p)
	    error (EXIT_FAILURE, 0, _("Could not convert from %s to UTF-8."),
		   stringprep_locale_charset ());

	  q = stringprep_utf8_to_ucs4 (p, -1, NULL);
	  if (!q)
	    {
	      free (p);
	      error (EXIT_FAILURE, 0,
		     _("Could not convert from UTF-8 to UCS-4."));
	    }

	  if (args_info.debug_given)
	    {
	      size_t i;
	      for (i = 0; q[i]; i++)
		fprintf (stderr, _("input[%lu] = U+%04x\n"),
			 (unsigned long) i, q[i]);
	    }
	  free (q);

	  rc = stringprep_profile (p, &r,
				   args_info.profile_given ?
				   args_info.profile_arg : "Nameprep", 0);
	  free (p);
	  if (rc != STRINGPREP_OK)
	    error (EXIT_FAILURE, 0, _("stringprep_profile: %s"),
		   stringprep_strerror (rc));

	  q = stringprep_utf8_to_ucs4 (r, -1, NULL);
	  if (!q)
	    {
	      free (r);
	      error (EXIT_FAILURE, 0,
		     _("Could not convert from UTF-8 to UCS-4."));
	    }

	  if (args_info.debug_given)
	    {
	      size_t i;
	      for (i = 0; q[i]; i++)
		fprintf (stderr, _("output[%lu] = U+%04x\n"),
			 (unsigned long) i, q[i]);
	    }
	  free (q);

	  p = stringprep_utf8_to_locale (r);
	  free (r);
	  if (!p)
	    error (EXIT_FAILURE, 0, _("Could not convert from UTF-8 to %s."),
		   stringprep_locale_charset ());

	  fprintf (stdout, "%s\n", p);

	  free (p);
	}

      if (args_info.punycode_encode_given)
	{
	  size_t len, len2;

	  p = stringprep_locale_to_utf8 (readbuf);
	  if (!p)
	    error (EXIT_FAILURE, 0, _("Could not convert from %s to UTF-8."),
		   stringprep_locale_charset ());

	  q = stringprep_utf8_to_ucs4 (p, -1, &len);
	  free (p);
	  if (!q)
	    error (EXIT_FAILURE, 0,
		   _("Could not convert from UTF-8 to UCS-4."));

	  if (args_info.debug_given)
	    {
	      size_t i;
	      for (i = 0; i < len; i++)
		fprintf (stderr, _("input[%lu] = U+%04x\n"),
			 (unsigned long) i, q[i]);
	    }

	  len2 = BUFSIZ - 1;
	  rc = punycode_encode (len, q, NULL, &len2, readbuf);
	  free (q);
	  if (rc != PUNYCODE_SUCCESS)
	    error (EXIT_FAILURE, 0, _("punycode_encode: %s"),
		   punycode_strerror (rc));

	  readbuf[len2] = '\0';

	  p = stringprep_utf8_to_locale (readbuf);
	  if (!p)
	    error (EXIT_FAILURE, 0, _("Could not convert from UTF-8 to %s."),
		   stringprep_locale_charset ());

	  fprintf (stdout, "%s\n", p);

	  free (p);
	}

      if (args_info.punycode_decode_given)
	{
	  size_t len;

	  len = BUFSIZ;
	  q = (uint32_t *) malloc (len * sizeof (q[0]));
	  if (!q)
	    error (EXIT_FAILURE, ENOMEM, _("malloc"));

	  rc = punycode_decode (strlen (readbuf), readbuf, &len, q, NULL);
	  if (rc != PUNYCODE_SUCCESS)
	    {
	      free (q);
	      error (EXIT_FAILURE, 0, _("punycode_decode: %s"),
		     punycode_strerror (rc));
	    }

	  if (args_info.debug_given)
	    {
	      size_t i;
	      for (i = 0; i < len; i++)
		fprintf (stderr, _("output[%lu] = U+%04x\n"),
			 (unsigned long) i, q[i]);
	    }

	  q[len] = 0;
	  r = stringprep_ucs4_to_utf8 (q, -1, NULL, NULL);
	  free (q);
	  if (!r)
	    error (EXIT_FAILURE, 0,
		   _("Could not convert from UCS-4 to UTF-8."));

	  p = stringprep_utf8_to_locale (r);
	  free (r);
	  if (!r)
	    error (EXIT_FAILURE, 0, _("Could not convert from UTF-8 to %s."),
		   stringprep_locale_charset ());

	  fprintf (stdout, "%s\n", p);

	  free (p);
	}

      if (args_info.idna_to_ascii_given)
	{
	  p = stringprep_locale_to_utf8 (readbuf);
	  if (!p)
	    error (EXIT_FAILURE, 0, _("Could not convert from %s to UTF-8."),
		   stringprep_locale_charset ());

	  q = stringprep_utf8_to_ucs4 (p, -1, NULL);
	  free (p);
	  if (!q)
	    error (EXIT_FAILURE, 0,
		   _("Could not convert from UCS-4 to UTF-8."));

	  if (args_info.debug_given)
	    {
	      size_t i;
	      for (i = 0; q[i]; i++)
		fprintf (stderr, _("input[%lu] = U+%04x\n"),
			 (unsigned long) i, q[i]);
	    }

	  rc = idna_to_ascii_4z (q, &p,
				 (args_info.allow_unassigned_given ?
				  IDNA_ALLOW_UNASSIGNED : 0) |
				 (args_info.usestd3asciirules_given ?
				  IDNA_USE_STD3_ASCII_RULES : 0));
	  free (q);
	  if (rc != IDNA_SUCCESS)
	    error (EXIT_FAILURE, 0, _("idna_to_ascii_4z: %s"),
		   idna_strerror (rc));

#ifdef WITH_TLD
	  if (args_info.tld_flag)
	    {
	      size_t errpos;

	      rc = idna_to_unicode_8z4z (p, &q,
					 (args_info.allow_unassigned_given ?
					  IDNA_ALLOW_UNASSIGNED : 0) |
					 (args_info.usestd3asciirules_given ?
					  IDNA_USE_STD3_ASCII_RULES : 0));
	      if (rc != IDNA_SUCCESS)
		error (EXIT_FAILURE, 0, _("idna_to_unicode_8z4z (TLD): %s"),
		       idna_strerror (rc));

	      if (args_info.debug_given)
		{
		  size_t i;
		  for (i = 0; q[i]; i++)
		    fprintf (stderr, _("tld[%lu] = U+%04x\n"),
			     (unsigned long) i, q[i]);
		}

	      rc = tld_check_4z (q, &errpos, NULL);
	      free (q);
	      if (rc == TLD_INVALID)
		error (EXIT_FAILURE, 0, _("tld_check_4z (position %lu): %s"),
		       (unsigned long) errpos, tld_strerror (rc));
	      if (rc != TLD_SUCCESS)
		error (EXIT_FAILURE, 0, _("tld_check_4z: %s"),
		       tld_strerror (rc));
	    }
#endif

	  if (args_info.debug_given)
	    {
	      size_t i;
	      for (i = 0; p[i]; i++)
		fprintf (stderr, _("output[%lu] = U+%04x\n"),
			 (unsigned long) i, p[i]);
	    }

	  fprintf (stdout, "%s\n", p);

	  free (p);
	}

      if (args_info.idna_to_unicode_given)
	{
	  p = stringprep_locale_to_utf8 (readbuf);
	  if (!p)
	    error (EXIT_FAILURE, 0, _("Could not convert from %s to UTF-8."),
		   stringprep_locale_charset ());

	  q = stringprep_utf8_to_ucs4 (p, -1, NULL);
	  if (!q)
	    {
	      free (p);
	      error (EXIT_FAILURE, 0,
		     _("Could not convert from UCS-4 to UTF-8."));
	    }

	  if (args_info.debug_given)
	    {
	      size_t i;
	      for (i = 0; q[i]; i++)
		fprintf (stderr, _("input[%lu] = U+%04x\n"),
			 (unsigned long) i, q[i]);
	    }
	  free (q);

	  rc = idna_to_unicode_8z4z (p, &q,
				     (args_info.allow_unassigned_given ?
				      IDNA_ALLOW_UNASSIGNED : 0) |
				     (args_info.usestd3asciirules_given ?
				      IDNA_USE_STD3_ASCII_RULES : 0));
	  free (p);
	  if (rc != IDNA_SUCCESS)
	    error (EXIT_FAILURE, 0, _("idna_to_unicode_8z4z: %s"),
		   idna_strerror (rc));

	  if (args_info.debug_given)
	    {
	      size_t i;
	      for (i = 0; q[i]; i++)
		fprintf (stderr, _("output[%lu] = U+%04x\n"),
			 (unsigned long) i, q[i]);
	    }

#ifdef WITH_TLD
	  if (args_info.tld_flag)
	    {
	      size_t errpos;

	      rc = tld_check_4z (q, &errpos, NULL);
	      if (rc == TLD_INVALID)
		{
		  free (q);
		  error (EXIT_FAILURE, 0, _("tld_check_4z (position %lu): %s"),
			 (unsigned long) errpos, tld_strerror (rc));
		}
	      if (rc != TLD_SUCCESS)
		{
		  free (q);
		  error (EXIT_FAILURE, 0, _("tld_check_4z: %s"),
			 tld_strerror (rc));
		}
	    }
#endif

	  r = stringprep_ucs4_to_utf8 (q, -1, NULL, NULL);
	  free (q);
	  if (!r)
	    error (EXIT_FAILURE, 0,
		   _("Could not convert from UTF-8 to UCS-4."));

	  p = stringprep_utf8_to_locale (r);
	  free (r);
	  if (!r)
	    error (EXIT_FAILURE, 0, _("Could not convert from UTF-8 to %s."),
		   stringprep_locale_charset ());

	  fprintf (stdout, "%s\n", p);

	  free (p);
	}
    }
  while (!feof (stdin) && !ferror (stdin) && (args_info.inputs_num == 0 ||
					      cmdn < args_info.inputs_num));

  return EXIT_SUCCESS;
}
