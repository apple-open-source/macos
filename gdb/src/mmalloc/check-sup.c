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

/* Each memory allocation is bounded by a header structure and a trailer
   byte.  I.E.

	<size><magicword><user's allocation><magicbyte>

   The pointer returned to the user points to the first byte in the
   user's allocation area.  The magic word can be tested to detect
   buffer underruns and the magic byte can be tested to detect overruns. */

#define MAGICWORD (unsigned int) 0xfedabeeb /* Active chunk */
#define MAGICWORDFREE (unsigned int) 0xdeadbeef	/* Inactive chunk */
#define MAGICBYTE ((char) 0xd7)

struct hdr
{
  size_t size;			/* Exact size requested by user.  */
  unsigned long int magic; /* Magic number to check header integrity.  */
};

static void checkhdr PARAMS ((struct mdesc *, CONST struct hdr *));
static void mfree_check PARAMS ((PTR, PTR));
static PTR mmalloc_check PARAMS ((PTR, size_t));
static PTR mrealloc_check PARAMS ((PTR, PTR, size_t));

/* Check the magicword and magicbyte, and if either is corrupted then
   call the emergency abort function specified for the heap in use. */

static void
checkhdr (mdp, hdr)
  struct mdesc *mdp;
  CONST struct hdr *hdr;
{
  if (hdr -> magic != MAGICWORD ||
      ((char *) &hdr[1])[hdr -> size] != MAGICBYTE)
    {
      (*mdp -> abortfunc)();
    }
}

static void
mfree_check (md, ptr)
  PTR md;
  PTR ptr;
{
  struct hdr *hdr = ((struct hdr *) ptr) - 1;
  struct mdesc *mdp = (struct mdesc *) md;
  
  checkhdr (mdp, hdr);
  hdr->magic = MAGICWORDFREE;
  mfree (mdp->child, (PTR) hdr);
}

static PTR
mmalloc_check (md, size)
  PTR md;
  size_t size;
{
  struct hdr *hdr;
  struct mdesc *mdp = (struct mdesc *) md;
  size_t nbytes = sizeof (struct hdr) + size + 1;

  hdr = (struct hdr *) mmalloc (mdp->child, nbytes);
  if (hdr != NULL)
    {
      hdr->size = size;
      hdr->magic = MAGICWORD;
      hdr++;
      *((char *) hdr + size) = MAGICBYTE;
    }
  return ((PTR) hdr);
}

static PTR
mrealloc_check (md, ptr, size)
  PTR md;
  PTR ptr;
  size_t size;
{
  struct hdr *hdr = ((struct hdr *) ptr) - 1;
  struct mdesc *mdp = (struct mdesc *) md;
  size_t nbytes = sizeof (struct hdr) + size + 1;

  checkhdr (mdp, hdr);
  hdr = (struct hdr *) mrealloc (mdp->child, (PTR) hdr, nbytes);
  if (hdr != NULL)
    {
      hdr->size = size;
      hdr++;
      *((char *) hdr + size) = MAGICBYTE;
    }
  return ((PTR) hdr);
}

struct mdesc *
mmalloc_check_create (struct mdesc *child)
{
  struct mdesc *ret = NULL;

  ret = (struct mdesc *) malloc (sizeof (struct mdesc));
  memset ((char *) ret, 0, sizeof (struct mdesc));

  ret->child = child;

  ret->mfree_hook = mfree_check;
  ret->mmalloc_hook = mmalloc_check;
  ret->mrealloc_hook = mrealloc_check;
  ret->abortfunc = abort;

  if (child != NULL)
    ret->flags = child->flags;

  return ret;
}
