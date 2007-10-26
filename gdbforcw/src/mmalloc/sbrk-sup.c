/* Support for sbrk() regions.
   Copyright 1992, 2000 Free Software Foundation, Inc.
   Contributed by Fred Fish at Cygnus Support.   fnf@cygnus.com

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

#include <unistd.h>	/* Prototypes for sbrk (maybe) */
#include <string.h>	/* Prototypes for memcpy, memmove, memset, etc */

#include "mmprivate.h"

static PTR sbrk_morecore PARAMS ((struct mdesc *, int));
#if NEED_DECLARATION_SBRK
extern PTR sbrk PARAMS ((int));
#endif

/* Use sbrk() to get more core. */

static PTR
sbrk_morecore (mdp, size)
  struct mdesc *mdp;
  int size;
{
  void *result;

  if ((result = sbrk (size)) == (void *) -1)
    {
      result = NULL;
    }
  else
    {
      mdp -> breakval += size;
      mdp -> top += size;
    }
  return (result);
}

/* Initialize the default malloc descriptor if this is the first time
   a request has been made to use the default sbrk'd region.

   Since no alignment guarantees are made about the initial value returned
   by sbrk, test the initial value and (if necessary) sbrk enough additional
   memory to start off with alignment to BLOCKSIZE.  We actually only need
   it aligned to an alignment suitable for any object, so this is overkill.
   But at most it wastes just part of one BLOCKSIZE chunk of memory and
   minimizes portability problems by avoiding us having to figure out
   what the actual minimal alignment is.  The rest of the malloc code
   avoids this as well, by always aligning to the minimum of the requested
   size rounded up to a power of two, or to BLOCKSIZE.

   Note that we are going to use some memory starting at this initial sbrk
   address for the sbrk region malloc descriptor, which is a struct, so the
   base address must be suitably aligned. */

struct mdesc *
mmalloc_sbrk_init ()
{
  PTR base;
  unsigned int adj;
  struct mdesc *ret = NULL;

  base = sbrk (0);
  adj = RESIDUAL (base, BLOCKSIZE);
  if (adj != 0)
    {
      sbrk (BLOCKSIZE - adj);
      base = sbrk (0);
    }
  ret = (struct mdesc *) sbrk (sizeof (struct mdesc));
  memset ((char *) ret, 0, sizeof (struct mdesc));
  ret -> morecore = sbrk_morecore;
  ret -> base = base;
  ret -> breakval = ret -> top = sbrk (0);
  ret -> fd = -1;

  return ret;
}
