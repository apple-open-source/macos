/* Support for an sbrk-like function that uses mmap.
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

#include "config.h"

#if defined(HAVE_MMAP)

#include "mmprivate.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>	/* Prototypes for lseek */
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

#include <mach/mach.h>

#define HAVE_MSYNC

#if !defined (MAP_ANONYMOUS) && defined (MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

/* Cache the pagesize for the current host machine.  Note that if the host
   does not readily provide a getpagesize() function, we need to emulate it
   elsewhere, not clutter up this file with lots of kluges to try to figure
   it out. */

static size_t pagesize;
#if NEED_DECLARATION_GETPAGESIZE
extern int getpagesize PARAMS ((void));
#endif

#define PAGE_ALIGN(addr) (caddr_t) (((long)(addr) + pagesize - 1) & ~(pagesize - 1))

/* Return MAP_PRIVATE if MDP represents /dev/zero.  Otherwise, return
   MAP_SHARED.  */

#define MAP_PRIVATE_OR_SHARED(MDP) ((MDP -> flags & MMALLOC_SHARED) \
                                    ? MAP_PRIVATE \
                                    : MAP_SHARED)

/*  Get core for the memory region specified by MDP, using SIZE as the
    amount to either add to or subtract from the existing region.  Works
    like sbrk(), but using mmap(). */

PTR
__mmalloc_mmap_morecore (mdp, size)
  struct mdesc *mdp;
  int size;
{
  PTR result = NULL;
  off_t foffset;        /* File offset at which new mapping will start */
  size_t mapbytes;      /* Number of bytes to map */
  caddr_t moveto;       /* Address which will become the new top */
  caddr_t mapto;        /* Address we actually mapped to */
  char buf = 0;         /* Single byte to write to extend mapped file */
  int flags = 0;        /* Flags for mmap() */

  if (pagesize == 0)
    {
      pagesize = getpagesize ();
    }

  if (size == 0)
    {
      /* Just return the current "break" value. */
      return (mdp -> breakval);
    }

  if (size < 0)
    {
      /* We are deallocating memory.  If the amount requested would cause
         us to try to deallocate back past the base of the mmap'd region
         then do nothing, and return NULL.  Otherwise, deallocate the
         memory and return the old break value. */

      if (mdp -> breakval + size >= mdp -> base)
        {
          result = (PTR) mdp -> breakval;
          moveto = PAGE_ALIGN (mdp -> breakval + size);
#ifdef HAVE_MSYNC
#ifdef MS_SYNC
          msync (moveto, (size_t) (mdp -> top - moveto), MS_SYNC | MS_INVALIDATE);
#else
          msync (moveto, (size_t) (mdp -> top - moveto));
#endif
#endif
          munmap (moveto, (size_t) (mdp -> top - moveto));
          mdp -> breakval += size;
          mdp -> top = moveto;
          return result;
        }
      else
        {
          return NULL;
        }
    }

  /* We are allocating memory.  Make sure we have an open file
     descriptor and then go on to get the memory. */

#ifndef MAP_ANONYMOUS
  if (mdp -> fd < 0)
    {
      char buf[64];
      sprintf (buf, "/tmp/mmalloc.XXXXXX");

      mdp -> fd = mkstemp (buf);
      if (mdp -> fd < 0)
        {
          fprintf (stderr, "unable to create default mmalloc allocator: %s",
		   strerror (errno));
          return NULL;
        }

      if (unlink (buf) != 0)
        {
          fprintf (stderr, "unable to unlink map file for default mmalloc allocator: %s\n", 
		   strerror (errno));
        }
    }
#endif

  if (mdp -> breakval + size <= mdp -> top)
    {
      result = (PTR) mdp -> breakval;
      mdp -> breakval += size;
      return result;
    }

  /* The request would move us past the end of the currently
     mapped memory, so map in enough more memory to satisfy
     the request.  This means we also have to grow the mapped-to
     file by an appropriate amount, since mmap cannot be used
     to extend a file. */

  moveto = PAGE_ALIGN (mdp -> breakval + size);
  mapbytes = moveto - mdp -> top;
  foffset = mdp -> top - mdp -> base;

#ifdef MAP_ANONYMOUS
  if (mdp -> fd < 0)
    {
      flags = MAP_PRIVATE | MAP_ANONYMOUS;
    }
#endif

  if (mdp -> fd > 0)
    {
      /* FIXME:  Test results of lseek() and write() */
      lseek (mdp -> fd, foffset + mapbytes - 1, SEEK_SET);
      write (mdp -> fd, &buf, 1);

#ifdef MAP_FILE
      flags = MAP_PRIVATE_OR_SHARED (mdp) | MAP_FILE;
#else 
      flags = MAP_PRIVATE_OR_SHARED (mdp)
#endif
    }

  if (mdp -> base == 0)
    {
      /* Let mmap pick the map start address */
      mapto = mmap (0, mapbytes, PROT_READ | PROT_WRITE,
                    flags, mdp -> fd, foffset);
      if (mapto != (caddr_t) -1)
        {
          mdp -> base = mdp -> breakval = mapto;
          mdp -> top = mdp -> base + mapbytes;
          result = (PTR) mdp -> breakval;
          mdp -> breakval += size;
        }
    }
  else
    {
      vm_address_t r_start;
      vm_size_t r_size;
      vm_region_basic_info_data_t r_data;
      mach_msg_type_number_t r_info_size;
      port_t r_object_name;
      kern_return_t kret;

      r_start = (vm_address_t) mdp -> top;
      r_info_size = VM_REGION_BASIC_INFO_COUNT;
      kret = vm_region (mach_task_self(), &r_start, &r_size,
			VM_REGION_BASIC_INFO, (vm_region_info_t) &r_data,
			&r_info_size, &r_object_name);
      if ((kret != KERN_SUCCESS) && (kret != KERN_INVALID_ADDRESS))
	{
	  return NULL;
	}
      if ((kret == KERN_SUCCESS) && (r_start < (mdp -> top + mapbytes)))
	{
	  return NULL;
	}

      mapto = mmap (mdp -> top, mapbytes, PROT_READ | PROT_WRITE,
                    flags | MAP_FIXED, mdp -> fd, foffset);
      if (mapto == mdp -> top)
        {
          mdp -> top = moveto;
          result = (PTR) mdp -> breakval;
          mdp -> breakval += size;
        }
      else
	{
	  return NULL;
	}
    }

  return (result);
}

PTR
mmalloc_findbase (size, base)
  size_t size;
  size_t base;
{
  vm_address_t last = base;

  if (last == 0) 
    last = 0xd0000000;

  for (;;) {

    vm_address_t r_start;
    vm_size_t r_size;
    vm_region_basic_info_data_t r_data;
    mach_msg_type_number_t r_info_size;
    port_t r_object_name;
    kern_return_t kret;

    r_start = last;
    r_info_size = VM_REGION_BASIC_INFO_COUNT;

    kret = vm_region (mach_task_self(), &r_start, &r_size,
		      VM_REGION_BASIC_INFO, (vm_region_info_t) &r_data,
		      &r_info_size, &r_object_name);
    if ((kret == KERN_INVALID_ADDRESS) && ((last + size) > last))
      {
	return last;
      }
    if (kret != KERN_SUCCESS) 
      {
	return NULL;
      }
    if ((r_start - last) >= size)
      {
	return last;
      }

    last = r_start + r_size;
  }
}

PTR
mmalloc_findbase_hidden (size)
  size_t size;
{
  int fd;
  int flags;
  caddr_t base = NULL;

  if (pagesize == 0)
    {
      pagesize = getpagesize ();
    }

#ifdef MAP_ANONYMOUS
  flags = MAP_PRIVATE | MAP_ANONYMOUS;
  fd = -1;
#else
#ifdef MAP_FILE
  flags = MAP_PRIVATE | MAP_FILE;
#else
  flags = MAP_PRIVATE;
#endif
#endif

#ifndef MAP_ANONYMOUS
  {
    char buf[64];
    sprintf (buf, "/tmp/mmalloc.XXXXXX");
  
    fd = mkstemp (buf);
    if (fd < 0)
      {
	fprintf (stderr, "unable to create default mmalloc allocator: %s",
		 strerror (errno));
	return NULL;
      }
  
    if (unlink (buf) != 0)
      {
	fprintf (stderr, "unable to unlink map file for default mmalloc allocator: %s", 
		 strerror (errno));
      }
  }
#endif

  size = (size_t) PAGE_ALIGN ((PTR) size);
  base = mmap (0, size, PROT_READ | PROT_WRITE, flags, fd, 0);
  if (base != (caddr_t) -1)
    {
#ifdef MS_SYNC
      msync (base, (size_t) size, MS_SYNC | MS_INVALIDATE);
#else
      msync (base, (size_t) size);
#endif
      munmap (base, (size_t) size);
    }
  if (fd != -1)
    {
      close (fd);
    }
  if (base == 0)
    {
      /* Don't allow mapping at address zero.  We use that value
	 to signal an error return, and besides, it is useful to
	 catch NULL pointers if it is unmapped.  Instead start
	 at the next page boundary. */
      base = (caddr_t) getpagesize ();
    }
  else if (base == (caddr_t) -1)
    {
      base = NULL;
    }
  return ((PTR) base);
}

void mmalloc_endpoints (md, start, end)
     PTR md;
     size_t *start;
     size_t *end;
{
  struct mdesc *mdp = MD_TO_MDP (md);

  if (pagesize == 0)
    {
      pagesize = getpagesize ();
    }

  if (mdp->child)
    return mmalloc_endpoints (mdp->child, start, end);

  *start = (size_t) mdp->base;
  *end = (size_t) PAGE_ALIGN (mdp->breakval);
}

#endif	/* defined(HAVE_MMAP) */
