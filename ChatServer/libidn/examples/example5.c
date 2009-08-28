/* example5.c --- Example TLD checking.
 * Copyright (C) 2004, 2005, 2006, 2007  Simon Josefsson
 *
 * This file is part of GNU Libidn.
 *
 * GNU Libidn is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GNU Libidn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GNU Libidn; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Get stringprep_locale_charset, etc. */
#include <stringprep.h>

/* Get idna_to_ascii_8z, etc. */
#include <idna.h>

/* Get tld_check_4z. */
#include <tld.h>

/*
 * Compiling using libtool and pkg-config is recommended:
 *
 * $ libtool cc -o example5 example5.c `pkg-config --cflags --libs libidn`
 * $ ./example5
 * Input domain encoded as `UTF-8': fooß.no
 * Read string (length 8): 66 6f 6f c3 9f 2e 6e 6f
 * ToASCII string (length 8): fooss.no
 * ToUnicode string: U+0066 U+006f U+006f U+0073 U+0073 U+002e U+006e U+006f
 * Domain accepted by TLD check
 *
 * $ ./example5
 * Input domain encoded as `UTF-8': gr€€n.no
 * Read string (length 12): 67 72 e2 82 ac e2 82 ac 6e 2e 6e 6f
 * ToASCII string (length 16): xn--grn-l50aa.no
 * ToUnicode string: U+0067 U+0072 U+20ac U+20ac U+006e U+002e U+006e U+006f
 * Domain rejected by TLD check, Unicode position 2
 *
 */

int
main (int argc, char *argv[])
{
  char buf[BUFSIZ];
  char *p;
  uint32_t *r;
  int rc;
  size_t errpos, i;

  printf ("Input domain encoded as `%s': ", stringprep_locale_charset ());
  fflush (stdout);
  fgets (buf, BUFSIZ, stdin);
  buf[strlen (buf) - 1] = '\0';

  printf ("Read string (length %d): ", strlen (buf));
  for (i = 0; i < strlen (buf); i++)
    printf ("%02x ", buf[i] & 0xFF);
  printf ("\n");

  p = stringprep_locale_to_utf8 (buf);
  if (p)
    {
      strcpy (buf, p);
      free (p);
    }
  else
    printf ("Could not convert string to UTF-8, continuing anyway...\n");

  rc = idna_to_ascii_8z (buf, &p, 0);
  if (rc != IDNA_SUCCESS)
    {
      printf ("idna_to_ascii_8z failed (%d): %s\n", rc, idna_strerror (rc));
      return 2;
    }

  printf ("ToASCII string (length %d): %s\n", strlen (p), p);

  rc = idna_to_unicode_8z4z (p, &r, 0);
  free (p);
  if (rc != IDNA_SUCCESS)
    {
      printf ("idna_to_unicode_8z4z failed (%d): %s\n",
	      rc, idna_strerror (rc));
      return 2;
    }

  printf ("ToUnicode string: ");
  for (i = 0; r[i]; i++)
    printf ("U+%04x ", r[i]);
  printf ("\n");

  rc = tld_check_4z (r, &errpos, NULL);
  free (r);
  if (rc == TLD_INVALID)
    {
      printf ("Domain rejected by TLD check, Unicode position %d\n", errpos);
      return 1;
    }
  else if (rc != TLD_SUCCESS)
    {
      printf ("tld_check_4z() failed (%d): %s\n", rc, tld_strerror (rc));
      return 2;
    }

  printf ("Domain accepted by TLD check\n");

  return 0;
}
