/* BFD library -- caching of file descriptors.

   Copyright 1990, 1991, 1992, 1993, 1994, 1996, 2000, 2001, 2002,
   2003, 2004 Free Software Foundation, Inc.

   Hacked by Steve Chamberlain of Cygnus Support (steve@cygnus.com).

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/*
SECTION
	File caching

	The file caching mechanism is embedded within BFD and allows
	the application to open as many BFDs as it wants without
	regard to the underlying operating system's file descriptor
	limit (often as low as 20 open files).  The module in
	<<cache.c>> maintains a least recently used list of
	<<bfd_cache_max_open>> files, and exports the name
	<<bfd_cache_lookup>>, which runs around and makes sure that
	the required BFD is open. If not, then it chooses a file to
	close, closes it and opens the one wanted, returning its file
	handle.

*/

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

static bfd_boolean bfd_cache_delete (bfd *);

/*
INTERNAL_FUNCTION
	BFD_CACHE_MAX_OPEN macro

DESCRIPTION
	The maximum number of files which the cache will keep open at
	one time.

.#define BFD_CACHE_MAX_OPEN 10

*/

/* The number of BFD files we have open.  */

static unsigned int open_files;
static unsigned int bfd_cache_max_open = BFD_CACHE_MAX_OPEN;

/*
FUNCTION
	bfd_set_cache_max_open

SYNOPSIS
	void bfd_set_cache_max_open(unsigned int nmax);

DESCRIPTION
	Set the maximum number of files which the cache will keep
	open at one time.

*/

void
bfd_set_cache_max_open (nmax)
     unsigned int nmax;
{
  bfd_cache_max_open = nmax;
}

/*
INTERNAL_FUNCTION
	bfd_last_cache

SYNOPSIS
	extern bfd *bfd_last_cache;

DESCRIPTION
	Zero, or a pointer to the topmost BFD on the chain.  This is
	used by the <<bfd_cache_lookup>> macro in @file{libbfd.h} to
	determine when it can avoid a function call.
*/

bfd *bfd_last_cache = NULL;

/*
INTERNAL_FUNCTION
	bfd_cache_lookup macro

DESCRIPTION
	Check to see if the required BFD is the same as the last one
	looked up. If so, then it can use the stream in the BFD with
	impunity, since it can't have changed since the last lookup;
	otherwise, it has to perform the complicated lookup function.
 
.#define bfd_cache_lookup_null(x) \
.    ((x) == bfd_last_cache ? \
.      (FILE *) (bfd_last_cache->iostream) : \
.       bfd_cache_lookup_worker (x))

.#define bfd_cache_lookup(x) \
.    ((bfd_cache_lookup_null (x) != NULL) ? \
.     (bfd_cache_lookup_null (x)) : \
.     (bfd_assert (__FILE__,__LINE__), (FILE *) NULL))

.#define BFD_CACHE_ITERATOR(abfd) \
.    for (abfd = (bfd_last_cache != NULL) ? bfd_last_cache->lru_prev : NULL; \
.         abfd != NULL; \
.         abfd = (abfd == bfd_last_cache) ? NULL : abfd->lru_prev)
*/

/* Insert a BFD into the cache.  */

static void
insert (bfd *abfd)
{
  BFD_ASSERT ((abfd->flags & BFD_IN_MEMORY) == 0);
  if (bfd_last_cache == NULL)
    {
      abfd->lru_next = abfd;
      abfd->lru_prev = abfd;
    }
  else
    {
      abfd->lru_next = bfd_last_cache;
      abfd->lru_prev = bfd_last_cache->lru_prev;
      abfd->lru_prev->lru_next = abfd;
      abfd->lru_next->lru_prev = abfd;
    }
  bfd_last_cache = abfd;
}

/* Remove a BFD from the cache.  */

static void
snip (bfd *abfd)
{
  abfd->lru_prev->lru_next = abfd->lru_next;
  abfd->lru_next->lru_prev = abfd->lru_prev;
  if (abfd == bfd_last_cache)
    {
      bfd_last_cache = abfd->lru_next;
      if (abfd == bfd_last_cache)
	bfd_last_cache = NULL;
    }
}

/* We need to open a new file, and the cache is full.  Find the least
   recently used cacheable BFD and close it.  */

static bfd_boolean
close_one (void)
{
  bfd *kill = NULL;

  BFD_CACHE_ITERATOR (kill)
    {
      BFD_ASSERT ((kill->flags & BFD_IN_MEMORY) == 0);
      if (kill->cacheable)
	{
	  kill->where = real_ftell ((FILE *) kill->iostream);
	  return bfd_cache_delete (kill);
	}
    }

  return TRUE;
}

/* Close a BFD and remove it from the cache.  */

static bfd_boolean
bfd_cache_delete (bfd *abfd)
{
  bfd_boolean ret;

  BFD_ASSERT ((abfd->flags & BFD_IN_MEMORY) == 0);
  BFD_ASSERT (open_files > 0);

  if (fclose ((FILE *) abfd->iostream) == 0)
    ret = TRUE;
  else
    {
      ret = FALSE;
      bfd_set_error (bfd_error_system_call);
    }

  snip (abfd);

  abfd->iostream = NULL;
  --open_files;

  return ret;
}

/*
INTERNAL_FUNCTION
	bfd_cache_init

SYNOPSIS
	bfd_boolean bfd_cache_init (bfd *abfd);

DESCRIPTION
	Add a newly opened BFD to the cache.
*/

bfd_boolean
bfd_cache_init (bfd *abfd)
{
  BFD_ASSERT (abfd->iostream != NULL);
  BFD_ASSERT ((abfd->flags & BFD_IN_MEMORY) == 0);
  
  while (open_files >= bfd_cache_max_open)
    {
      if (! close_one ())
	return FALSE;
    }
  insert (abfd);
  ++open_files;
  return TRUE;
}

/*
INTERNAL_FUNCTION
	bfd_cache_close

SYNOPSIS
	bfd_boolean bfd_cache_close (bfd *abfd);

DESCRIPTION
	Remove the BFD @var{abfd} from the cache. If the attached file is open,
	then close it too.

RETURNS
	<<FALSE>> is returned if closing the file fails, <<TRUE>> is
	returned if all is well.
*/

bfd_boolean
bfd_cache_close (bfd *abfd)
{
  BFD_ASSERT ((abfd->flags & BFD_IN_MEMORY) == 0);
  if (abfd->iostream == NULL)
    return TRUE;

  return bfd_cache_delete (abfd);
}

/*
INTERNAL_FUNCTION
	bfd_open_file

SYNOPSIS
	FILE* bfd_open_file (bfd *abfd);

DESCRIPTION
	Call the OS to open a file for @var{abfd}.  Return the <<FILE *>>
	(possibly <<NULL>>) that results from this operation.  Set up the
	BFD so that future accesses know the file is open. If the <<FILE *>>
	returned is <<NULL>>, then it won't have been put in the
	cache, so it won't have to be removed from it.
*/

FILE *
bfd_open_file (bfd *abfd)
{
  abfd->cacheable = TRUE;	/* Allow it to be closed later.  */

  while ((open_files + 1) >= bfd_cache_max_open)
    {
      if (! close_one ())
	return NULL;
    }

  switch (abfd->direction)
    {
    case read_direction:
    case no_direction:
      abfd->iostream = (PTR) fopen (abfd->filename, FOPEN_RB);
      break;
    case both_direction:
    case write_direction:
      if (abfd->opened_once)
	{
	  abfd->iostream = (PTR) fopen (abfd->filename, FOPEN_RUB);
	  if (abfd->iostream == NULL)
	    abfd->iostream = (PTR) fopen (abfd->filename, FOPEN_WUB);
	}
      else
	{
	  /* Create the file.

	     Some operating systems won't let us overwrite a running
	     binary.  For them, we want to unlink the file first.

	     However, gcc 2.95 will create temporary files using
	     O_EXCL and tight permissions to prevent other users from
	     substituting other .o files during the compilation.  gcc
	     will then tell the assembler to use the newly created
	     file as an output file.  If we unlink the file here, we
	     open a brief window when another user could still
	     substitute a file.

	     So we unlink the output file if and only if it has
	     non-zero size.  */
#ifndef __MSDOS__
	  /* Don't do this for MSDOS: it doesn't care about overwriting
	     a running binary, but if this file is already open by
	     another BFD, we will be in deep trouble if we delete an
	     open file.  In fact, objdump does just that if invoked with
	     the --info option.  */
	  struct stat s;

	  if (stat (abfd->filename, &s) == 0 && s.st_size != 0)
	    unlink (abfd->filename);
#endif
	  abfd->iostream = (PTR) fopen (abfd->filename, FOPEN_WUB);
	  abfd->opened_once = TRUE;
	}
      break;
    }

  if (abfd->iostream != NULL)
    {
      if (! bfd_cache_init (abfd))
	return NULL;
    }

  return (FILE *) abfd->iostream;
}

/*
INTERNAL_FUNCTION
	bfd_cache_lookup_worker

SYNOPSIS
	FILE *bfd_cache_lookup_worker (bfd *abfd);

DESCRIPTION
	Called when the macro <<bfd_cache_lookup>> fails to find a
	quick answer.  Find a file descriptor for @var{abfd}.  If
	necessary, it open it.  If there are already more than
	<<bfd_cache_max_open>> files open, it tries to close one first, to
	avoid running out of file descriptors.  
*/

FILE *
bfd_cache_lookup_worker (bfd *abfd)
{
  while (abfd->my_archive) 
    abfd = abfd->my_archive;

  BFD_ASSERT ((abfd->flags & BFD_IN_MEMORY) == 0);

  if (abfd->iostream != NULL)
    {
      /* Move the file to the start of the cache.  */
      if (abfd != bfd_last_cache)
	{
	  snip (abfd);
	  insert (abfd);
	}
    }
  else
    {
      if (bfd_open_file (abfd) == NULL)
	return NULL;
      if (abfd->where != (unsigned long) abfd->where)
	return NULL;
      if (real_fseek ((FILE *) abfd->iostream, abfd->where, SEEK_SET) != 0)
	return NULL;
    }

  return (FILE *) abfd->iostream;
}

/*
INTERNAL_FUNCTION
	bfd_cache_flush

SYNOPSIS
	void bfd_cache_flush (void);

DESCRIPTION
	Flushes BFD file cache.
*/

void
bfd_cache_flush (void)
{
  while (bfd_last_cache != NULL) {
    unsigned int prev_open = open_files;
    close_one ();
    if (open_files == prev_open) {
      break;
    }
  }
}
