/*
 * Non-failing memory allocation routines.
 * Copyright (c) 1995-1999 Markku Rossi.
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

#include <stdio.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if STDC_HEADERS

#include <stdlib.h>
#include <string.h>

#else /* no STDC_HEADERS */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#endif /* no STDC_HEADERS */

#if ENABLE_NLS
#include <libintl.h>
#define _(String) gettext (String)
#else
#define _(String) String
#endif

/*
 * Global functions.
 */

void *
xmalloc (size)
     size_t size;
{
  void *ptr;

  ptr = malloc (size);
  if (ptr == NULL)
    {
      fprintf (stderr, _("xmalloc(): couldn't allocate %d bytes\n"), size);
      exit (1);
    }

  return ptr;
}


void *
xcalloc (num, size)
     size_t num;
     size_t size;
{
  void *ptr;

  ptr = calloc (num, size);
  if (ptr == NULL)
    {
      fprintf (stderr, _("xcalloc(): couldn't allocate %d bytes\n"), size);
      exit (1);
    }

  return ptr;
}


void *
xrealloc (ptr, size)
     void *ptr;
     size_t size;
{
  void *nptr;

  if (ptr == NULL)
    return xmalloc (size);

  nptr = realloc (ptr, size);
  if (nptr == NULL)
    {
      fprintf (stderr, _("xrealloc(): couldn't reallocate %d bytes\n"), size);
      exit (1);
    }

  return nptr;
}


void
xfree (ptr)
     void *ptr;
{
  if (ptr == NULL)
    return;

  free (ptr);
}


char *
xstrdup (str)
     char *str;
{
  char *tmp;

  tmp = xmalloc (strlen (str) + 1);
  strcpy (tmp, str);

  return tmp;
}
