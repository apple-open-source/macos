/* lt__alloc.c -- internal memory management interface
   Copyright (C) 2004, 2006 Free Software Foundation, Inc.
   Originally by Gary V. Vaughan  <gary@gnu.org>

   NOTE: The canonical source of this file is maintained with the
   GNU Libtool package.  Report bugs to bug-libtool@gnu.org.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

As a special exception to the GNU Lesser General Public License,
if you distribute this file as part of a program or library that
is built using GNU libtool, you may include it under the same
distribution terms that you use for the rest of that program.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301  USA

*/

#if defined(LT_CONFIG_H)
#  include LT_CONFIG_H
#else
#  include <config.h>
#endif

#include <stdio.h>

#include "lt__alloc.h"
#include "lt__private.h"

static void alloc_die_default (void);

void (*lt__alloc_die) (void) = alloc_die_default;

/* Unless overridden, exit on memory failure.  */
static void
alloc_die_default (void)
{
  fprintf (stderr, "Out of memory.\n");
  exit (EXIT_FAILURE);
}

void *
lt__malloc (size_t n)
{
  void *mem;

  if (! (mem = malloc (n)))
    (*lt__alloc_die) ();

  return mem;
}

void *
lt__zalloc (size_t n)
{
  void *mem;

  if ((mem = lt__malloc (n)))
    memset (mem, 0, n);

  return mem;
}

void *
lt__realloc (void *mem, size_t n)
{
  if (! (mem = realloc (mem, n)))
    (*lt__alloc_die) ();

  return mem;
}

void *
lt__memdup (void const *mem, size_t n)
{
  void *newmem;

  if ((newmem = lt__malloc (n)))
    return memcpy (newmem, mem, n);

  return 0;
}

char *
lt__strdup (const char *string)
{
  return (char *) lt__memdup (string, strlen (string) +1);
}
