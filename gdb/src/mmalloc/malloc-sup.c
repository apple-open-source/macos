/* Support for mmalloc using system malloc()

This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include <stdlib.h>
#include <string.h>

#include "mmprivate.h"

static PTR
morecore_malloc (mdp, size)
  struct mdesc *mdp;
  int size;
{
  abort ();
}

static void
mfree_malloc (md, ptr)
  PTR md;
  PTR ptr;
{
  free (ptr);
}

static PTR
mmalloc_malloc (md, size)
  PTR md;
  size_t size;
{
  return malloc (size);
}

static PTR
mrealloc_malloc (md, ptr, size)
  PTR md;
  PTR ptr;
  size_t size;
{
  return realloc (ptr, size);
}

struct mdesc *
mmalloc_malloc_create ()
{
  struct mdesc *ret = NULL;

  ret = (struct mdesc *) malloc (sizeof (struct mdesc));
  memset ((char *) ret, 0, sizeof (struct mdesc));

  ret->child = NULL;
  ret->fd = -1;

  ret->mfree_hook = mfree_malloc;
  ret->mmalloc_hook = mmalloc_malloc;
  ret->mrealloc_hook = mrealloc_malloc;
  ret->morecore = morecore_malloc;
  ret->abortfunc = abort;

  return ret;
}
