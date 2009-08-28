/* tst_pr29.c --- Self tests for pr29_*().
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

#include <pr29.h>

#include "utils.h"

struct tv
{
  const char *name;
  size_t inlen;
  uint32_t in[100];
  int rc;
};

static const struct tv tv[] = {
  {
   "Problem Sequence A",
   3,
   {0x1100, 0x0300, 0x1161, 0},
   PR29_PROBLEM},
  {
   "Test Case",
   3,
   {0x0B47, 0x0300, 0x0B3E, 0},
   PR29_PROBLEM},
  {
   "Instability Example",
   3,
   {0x1100, 0x0300, 0x1161, 0x0323, 0},
   PR29_PROBLEM},
  {
   "Not a problem sequence 1",
   3,
   {0x1100, 0x1161, 0x0300, 0},
   PR29_SUCCESS},
  {
   "Not a problem sequence 2",
   3,
   {0x0300, 0x1100, 0x1161, 0},
   PR29_SUCCESS},
  {
   "Not a problem sequence 3",
   3,
   {0x1161, 0x1100, 0x0300, 0},
   PR29_SUCCESS},
  {
   "Not a problem sequence 4",
   3,
   {0x1161, 0x0300, 0x1100, 0},
   PR29_SUCCESS},
  {
   "Not a problem sequence 5",
   3,
   {0x1100, 0x00AA, 0x1161, 0},
   PR29_SUCCESS}
};

void
doit (void)
{
  size_t i;
  int rc;

  for (i = 0; i < sizeof (tv) / sizeof (tv[0]); i++)
    {
      if (debug)
	printf ("PR29 entry %d: %s\n", i, tv[i].name);

      if (debug)
	{
	  printf ("in:\n");
	  ucs4print (tv[i].in, tv[i].inlen);
	}

      rc = pr29_4 (tv[i].in, tv[i].inlen);
      if (rc != tv[i].rc)
	{
	  fail ("PR29 entry %d failed (expected %d): %d\n", i, tv[i].rc, rc);
	  if (debug)
	    printf ("FATAL\n");
	  continue;
	}

      rc = pr29_4z (tv[i].in);
      if (rc != tv[i].rc)
	{
	  fail ("PR29 entry %d failed (expected %d): %d\n", i, tv[i].rc, rc);
	  if (debug)
	    printf ("FATAL\n");
	  continue;
	}

      if (debug)
	printf ("OK\n");
    }
}
