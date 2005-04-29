/* Low-level I/O routines for BFDs.

   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

   Written by Cygnus Support.

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

#include "sysdep.h"

#include "bfd.h"
#include "libbfd.h"

#include <limits.h>

#ifndef S_IXUSR
#define S_IXUSR 0100    /* Execute by owner.  */
#endif
#ifndef S_IXGRP
#define S_IXGRP 0010    /* Execute by group.  */
#endif
#ifndef S_IXOTH
#define S_IXOTH 0001    /* Execute by others.  */
#endif

file_ptr
real_ftell (FILE *file)
{
#if defined (HAVE_FTELLO64)
  return ftello64 (file);
#elif defined (HAVE_FTELLO)
  return ftello (file);
#else
  return ftell (file);
#endif
}

int
real_fseek (FILE *file, file_ptr offset, int whence)
{
#if defined (HAVE_FSEEKO64)
  return fseeko64 (file, offset, whence);
#elif defined (HAVE_FSEEKO)
  return fseeko (file, offset, whence);
#else
  return fseek (file, offset, whence);
#endif
}

/* Note that archive entries don't have streams; they share their parent's.
   This allows someone to play with the iostream behind BFD's back.

   Also, note that the origin pointer points to the beginning of a file's
   contents (0 for non-archive elements).  For archive entries this is the
   first octet in the file, NOT the beginning of the archive header.  */

static size_t
real_read (void *where, size_t a, size_t b, FILE *file)
{
  /* FIXME - this looks like an optimization, but it's really to cover
     up for a feature of some OSs (not solaris - sigh) that
     ld/pe-dll.c takes advantage of (apparently) when it creates BFDs
     internally and tries to link against them.  BFD seems to be smart
     enough to realize there are no symbol records in the "file" that
     doesn't exist but attempts to read them anyway.  On Solaris,
     attempting to read zero bytes from a NULL file results in a core
     dump, but on other platforms it just returns zero bytes read.
     This makes it to something reasonable. - DJ */
  if (a == 0 || b == 0)
    return 0;


#if defined (__VAX) && defined (VMS)
  /* Apparently fread on Vax VMS does not keep the record length
     information.  */
  return read (fileno (file), where, a * b);
#else
  return fread (where, a, b, file);
#endif
}

/* Return value is amount read.  */

bfd_size_type
bfd_bread (void *ptr, bfd_size_type size, bfd *abfd)
{
  while (abfd->my_archive != NULL)
    abfd = abfd->my_archive;

  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    {
      struct bfd_in_memory *bim;
      bfd_size_type get;

      bim = abfd->iostream;
      get = size;
      if (abfd->where + get > bim->size)
        {
          if (bim->size < (bfd_size_type) abfd->where)
            get = 0;
          else
            get = bim->size - abfd->where;
          bfd_set_error (bfd_error_file_truncated);
        }
      memcpy (ptr, bim->buffer + abfd->where, (size_t) get);
      abfd->where += get;
      return get;
    }
  else if ((abfd->flags & BFD_IO_FUNCS) != 0)
    {
      struct bfd_io_functions *bif;

      bif = (struct bfd_io_functions *) abfd->iostream;
      return (*bif->read_func) (bif->iodata, ptr, 1, size, abfd, abfd->where);
    } 
  else
    {
      int nread;

      nread = real_read (ptr, 1, (size_t) (size), bfd_cache_lookup (abfd));
  
      if (nread > 0)
	abfd->where += nread;
  
      /* Set bfd_error if we did not read as much data as we expected.

	 If the read failed due to an error set the bfd_error_system_call,
	 else set bfd_error_file_truncated.

	 A BFD backend may wish to override bfd_error_file_truncated to
	 provide something more useful (eg. no_symbols or wrong_format).  */

      if (nread < (int) (size))
	{
	  if (ferror (bfd_cache_lookup (abfd)))
	    bfd_set_error (bfd_error_system_call);
	  else
	    bfd_set_error (bfd_error_file_truncated);
	}

      return nread;
    }
}

bfd_size_type
bfd_bwrite (const void *ptr, bfd_size_type size, bfd *abfd)
{
  while (abfd->my_archive != NULL)
    abfd = abfd->my_archive;

  if (abfd->flags & BFD_IN_MEMORY)
    {
      struct bfd_in_memory *bim = abfd->iostream;
      size = (size_t) size;
      if (abfd->where + size > bim->size)
	{
	  long newsize, oldsize = (bim->size + 127) & ~127;
	  bim->size = abfd->where + size;
	  /* Round up to cut down on memory fragmentation */
	  newsize = (bim->size + 127) & ~127;
	  if (newsize > oldsize)
	    {
	      bim->buffer = bfd_realloc (bim->buffer, newsize);
	      if (bim->buffer == 0)
		{
		  bim->size = 0;
		  return 0;
		}
	    }
	}
      memcpy (bim->buffer + abfd->where, ptr, (size_t) size);
      abfd->where += size;
      return size;
    }
  else if ((abfd->flags & BFD_IO_FUNCS) != 0)
    {
      struct bfd_io_functions *bif;

      bif = (struct bfd_io_functions *) abfd->iostream;
      return (*bif->write_func) (bif->iodata, ptr, 1, size, abfd, abfd->where);
    } 
  else 
    {
      long nwrote;
    
      nwrote = fwrite (ptr, 1, (size_t) (size),
		       bfd_cache_lookup (abfd));
      if (nwrote > 0)
	abfd->where += nwrote;
      if ((bfd_size_type) nwrote != size)
	{
#ifdef ENOSPC
	  if (nwrote >= 0)
	    errno = ENOSPC;
#endif
	  bfd_set_error (bfd_error_system_call);
	}
      return nwrote;
    }
}

file_ptr
bfd_tell (bfd *abfd)
{
  file_ptr ptr = 0;
  bfd *cur = abfd;

  while (cur->my_archive) {
    ptr -= cur->origin;
    cur = cur->my_archive;
  }

  if ((cur->flags & BFD_IN_MEMORY) != 0)
    {
      ptr += cur->where;
    }
  else if ((abfd->flags & BFD_IO_FUNCS) != 0)
    {
      ptr += cur->where;
    } 
  else
    {
      cur->where = real_ftell (bfd_cache_lookup (cur));
      ptr += cur->where;
    }

  return ptr;
}

int
bfd_flush (bfd *abfd)
{
  while (abfd->my_archive)
    abfd = abfd->my_archive;

  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    {
      return 0;
    } 
  else if ((abfd->flags & BFD_IO_FUNCS) != 0)
    {
      struct bfd_io_functions *bif;

      bif = (struct bfd_io_functions *) abfd->iostream;
      return (*bif->flush_func) (bif->iodata, abfd);
    } 
  else 
    {
      return fflush (bfd_cache_lookup (abfd));
    }
}

/* Returns 0 for success, negative value for failure (in which case
   bfd_get_error can retrieve the error code).  */
int
bfd_stat (bfd *abfd, struct stat *statbuf)
{
  FILE *f;
  int result;

  while (abfd->my_archive)
    abfd = abfd->my_archive;

  if ((abfd->flags & BFD_IN_MEMORY) != 0)
    {
      struct bfd_in_memory *b = (struct bfd_in_memory *) abfd->iostream;
      memset (statbuf, 0, sizeof (struct stat));
      statbuf->st_size = b->size;
      return 0;
    } 
  else if ((abfd->flags & BFD_IO_FUNCS) != 0)
    {
      struct bfd_io_functions *bif = (struct bfd_io_functions *) abfd->iostream;
      return (*bif->stat_func) (bif->iodata, abfd, statbuf);
    } 
  else 
    {
      f = bfd_cache_lookup (abfd);
      if (f == NULL)
	{
	  bfd_set_error (bfd_error_system_call);
	  return -1;
	}
      result = fstat (fileno (f), statbuf);
      if (result < 0)
	bfd_set_error (bfd_error_system_call);
      return result;
    }
}

/* Returns 0 for success, nonzero for failure (in which case bfd_get_error
   can retrieve the error code).  */

int
bfd_seek (bfd *abfd, file_ptr position, int direction)
{
#if 0
  int result;
  FILE *f;
  file_ptr file_position;
#endif

  /* For the time being, a BFD may not seek to it's end.  The problem
     is that we don't easily have a way to recognize the end of an
     element in an archive.  */

  BFD_ASSERT (direction == SEEK_SET || direction == SEEK_CUR);

  if (direction == SEEK_CUR && position == 0)
    return 0;

  while (abfd->my_archive != NULL)
    {
      if (direction == SEEK_SET)
	position += abfd->origin;
      abfd = abfd->my_archive;
    }

  if ((direction == SEEK_SET) && ((ufile_ptr) position == abfd->where))
    return 0;

  if ((position < 0) && (direction != SEEK_CUR)) {
    bfd_set_error (bfd_error_system_call);
    return -1;
  }

  if (((abfd->flags & BFD_IN_MEMORY) != 0)
      || (abfd->flags & BFD_IO_FUNCS) != 0)
    {
      if (direction == SEEK_SET) {
	abfd->where = position;
      } else {
	abfd->where += position;
      }
      return 0;
    }
  else 
    {  
      int result;

      BFD_ASSERT ((ufile_ptr) ftell (bfd_cache_lookup (abfd)) == abfd->where);

      result = real_fseek (bfd_cache_lookup (abfd), position, direction);
      if (result != 0)
	{
	  int hold_errno = errno;

	  /* Force redetermination of `where' field.  */
	  bfd_tell (abfd);

	  if (hold_errno == EINVAL)
	    bfd_set_error (bfd_error_file_truncated);
	  else
	    {
	      bfd_set_error (bfd_error_system_call);
	      errno = hold_errno;
	    }
	}
      else
	{
	  /* Adjust `where' field.  */
	  if (direction == SEEK_SET)
	    abfd->where = position;
	  else
	    abfd->where += position;
	}

      return result;
    }
}

bfd_boolean
_bfd_io_close (bfd *abfd)
{
  if (abfd->flags & BFD_IN_MEMORY)
    {
      int ret = 0;
      
      struct bfd_in_memory *b = (struct bfd_in_memory *) abfd->iostream;
      BFD_ASSERT (b != NULL);

#if 0
      ret = munmap (b->buffer, b->size);
#endif

      abfd->iostream = NULL;
      BFD_ASSERT (ret == 0);
      
      return TRUE;
    } 
  else if (abfd->flags & BFD_IO_FUNCS)
    {
      struct bfd_io_functions *bif;
      
      bif = (struct bfd_io_functions *) abfd->iostream;
      return (*bif->close_func) (bif->iodata, abfd);
    }
  else
    {
      bfd_boolean ret = TRUE;
      ret = bfd_cache_close (abfd);

      /* If the file was open for writing and is now executable,
	 make it so */
      if (ret
	  && abfd->direction == write_direction
	  && abfd->flags & EXEC_P)
	{
	  struct stat buf;

	  if (stat (abfd->filename, &buf) == 0)
	    {
	      unsigned int mask = umask (0);

	      umask (mask);
	      chmod (abfd->filename,
		     (0x777
		      & (buf.st_mode | ((S_IXUSR | S_IXGRP | S_IXOTH) &~ mask))));
	    }
	}
      return (ret);
    }
}

/*
FUNCTION
	bfd_get_mtime

SYNOPSIS
	long bfd_get_mtime (bfd *abfd);

DESCRIPTION
	Return the file modification time (as read from the file system, or
	from the archive header for archive members).

*/

long
bfd_get_mtime (bfd *abfd)
{
  while (abfd->my_archive)
    abfd = abfd->my_archive;

  if (abfd->flags & (BFD_IO_FUNCS | BFD_IN_MEMORY))
    {
      return 0;
    }
  else 
    {
      FILE *fp;
      struct stat buf;

      fp = bfd_cache_lookup_null (abfd);
      if (fp == NULL)
	return 0;
      if (0 != fstat (fileno (fp), &buf))
	return 0;
    
      abfd->mtime = buf.st_mtime;		/* Save value in case anyone wants it */
    
      return abfd->mtime;
    }
}

/*
FUNCTION
	bfd_get_size

SYNOPSIS
	long bfd_get_size (bfd *abfd);

DESCRIPTION
	Return the file size (as read from file system) for the file
	associated with BFD @var{abfd}.

	The initial motivation for, and use of, this routine is not
	so we can get the exact size of the object the BFD applies to, since
	that might not be generally possible (archive members for example).
	It would be ideal if someone could eventually modify
	it so that such results were guaranteed.

	Instead, we want to ask questions like "is this NNN byte sized
	object I'm about to try read from file offset YYY reasonable?"
	As as example of where we might do this, some object formats
	use string tables for which the first <<sizeof (long)>> bytes of the
	table contain the size of the table itself, including the size bytes.
	If an application tries to read what it thinks is one of these
	string tables, without some way to validate the size, and for
	some reason the size is wrong (byte swapping error, wrong location
	for the string table, etc.), the only clue is likely to be a read
	error when it tries to read the table, or a "virtual memory
	exhausted" error when it tries to allocate 15 bazillon bytes
	of space for the 15 bazillon byte table it is about to read.
	This function at least allows us to answer the question, "is the
	size reasonable?".
*/

long
bfd_get_size (bfd *abfd)
{
  while (abfd->my_archive)
    abfd = abfd->my_archive;

  if (abfd->flags & BFD_IN_MEMORY)
    {
      return ((struct bfd_in_memory *) abfd->iostream)->size;
    } 
  else if (abfd->flags & BFD_IO_FUNCS)
    {
      return LONG_MAX;
    }
  else 
    {
      FILE *fp;
      struct stat buf;

      fp = bfd_cache_lookup (abfd);
      if (0 != fstat (fileno (fp), &buf))
	return 0;

      return buf.st_size;
    }
}
