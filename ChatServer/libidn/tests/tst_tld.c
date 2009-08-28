/* tst_tld.c --- Self tests for tld_*().
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

#include <stringprep.h>
#include <tld.h>

#include "utils.h"

struct tld
{
  const char *name;
  const char *tld;
  size_t inlen;
  uint32_t in[100];
  int rc;
  int errpos;
};

static const struct tld tld[] = {
  {
   "Simple valid French domain",
   "fr",
   3,
   {0x00E0, 0x00E2, 0x00E6},
   TLD_SUCCESS},
  {
   "Simple invalid French domain",
   "fr",
   5,
   {0x00E0, 0x00E2, 0x00E6, 0x4711, 0x0042},
   TLD_INVALID,
   3}
};

void
doit (void)
{
  size_t i;
  const Tld_table *tldtable;
  size_t errpos;
  int rc;

  for (i = 0; i < sizeof (tld) / sizeof (tld[0]); i++)
    {
      if (debug)
	printf ("TLD entry %d: %s\n", i, tld[i].name);

      if (debug)
	{
	  printf ("in:\n");
	  ucs4print (tld[i].in, tld[i].inlen);
	}

      tldtable = tld_default_table (tld[i].tld, NULL);
      if (tldtable == NULL)
	{
	  fail ("TLD entry %d tld_get_table (%s)\n", i, tld[i].tld);
	  if (debug)
	    printf ("FATAL\n");
	  continue;
	}

      rc = tld_check_4t (tld[i].in, tld[i].inlen, &errpos, tldtable);
      if (rc != tld[i].rc)
	{
	  fail ("TLD entry %d failed: %d\n", i, rc);
	  if (debug)
	    printf ("FATAL\n");
	  continue;
	}

      if (debug)
	printf ("returned %d expected %d\n", rc, tld[i].rc);

      if (rc != tld[i].rc)
	{
	  fail ("TLD entry %d failed\n", i);
	  if (debug)
	    printf ("ERROR\n");
	}
      else if (rc == TLD_INVALID)
	{
	  if (debug)
	    printf ("returned errpos %d expected errpos %d\n",
		    errpos, tld[i].errpos);

	  if (tld[i].errpos != errpos)
	    {
	      fail ("TLD entry %d failed because errpos %d != %d\n", i,
		    tld[i].errpos, errpos);
	      if (debug)
		printf ("ERROR\n");
	    }
	}
      else if (debug)
	printf ("OK\n");
    }
}
