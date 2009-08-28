/* tst_strerror.c --- Self tests for *_strerror().
 * Copyright (C) 2004, 2005, 2006, 2007  Simon Josefsson.
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
#include <stdarg.h>
#include <string.h>

#include <idna.h>
#include <pr29.h>
#include <punycode.h>
#include <stringprep.h>
#include <tld.h>

#include "utils.h"

#define SUCCESS "Success"
#define UNKNOWN "Unknown error"

void
doit (void)
{
  const char *p;

  /* Test success. */

  p = idna_strerror (0);
  if (strcmp (p, SUCCESS) != 0)
    fail ("idna_strerror (0) failed: %s\n", p);
  if (debug)
    printf ("idna_strerror (0) OK\n");

  p = pr29_strerror (0);
  if (strcmp (p, SUCCESS) != 0)
    fail ("pr29_strerror (0) failed: %s\n", p);
  if (debug)
    printf ("pr29_strerror (0) OK\n");

  p = punycode_strerror (0);
  if (strcmp (p, SUCCESS) != 0)
    fail ("punycode_strerror (0) failed: %s\n", p);
  if (debug)
    printf ("punycode_strerror (0) OK\n");

  p = stringprep_strerror (0);
  if (strcmp (p, SUCCESS) != 0)
    fail ("stringprep_strerror (0) failed: %s\n", p);
  if (debug)
    printf ("stringprep_strerror (0) OK\n");

  p = tld_strerror (0);
  if (strcmp (p, SUCCESS) != 0)
    fail ("tld_strerror (0) failed: %s\n", p);
  if (debug)
    printf ("tld_strerror (0) OK\n");

  /* Test unknown error. */

  p = idna_strerror (42);
  if (strcmp (p, UNKNOWN) != 0)
    fail ("idna_strerror (42) failed: %s\n", p);
  if (debug)
    printf ("idna_strerror (42) OK\n");

  p = pr29_strerror (42);
  if (strcmp (p, UNKNOWN) != 0)
    fail ("pr29_strerror (42) failed: %s\n", p);
  if (debug)
    printf ("pr29_strerror (42) OK\n");

  p = punycode_strerror (42);
  if (strcmp (p, UNKNOWN) != 0)
    fail ("punycode_strerror (42) failed: %s\n", p);
  if (debug)
    printf ("punycode_strerror (42) OK\n");

  p = stringprep_strerror (42);
  if (strcmp (p, UNKNOWN) != 0)
    fail ("stringprep_strerror (42) failed: %s\n", p);
  if (debug)
    printf ("stringprep_strerror (42) OK\n");

  p = tld_strerror (42);
  if (strcmp (p, UNKNOWN) != 0)
    fail ("tld_strerror (42) failed: %s\n", p);
  if (debug)
    printf ("tld_strerror (42) OK\n");
}
