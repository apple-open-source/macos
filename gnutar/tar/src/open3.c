/* Defines for Sys V style 3-argument open call.
   Copyright (C) 1988, 1994, 1995, 1996 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
   Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "system.h"

#if EMUL_OPEN3

/* open3.h -- #defines for the various flags for the Sys V style 3-argument
   open() call.  On BSD or System 5, the system already has this in an
   include file.  This file is needed for V7 and MINIX systems for the
   benefit of open3() in port.c, a routine that emulates the 3-argument call
   using system calls available on V7/MINIX.

   Written 1987-06-10 by Richard Todd.

   The names have been changed by John Gilmore, 1987-07-31, since Richard
   called it "bsdopen", and really this change was introduced in AT&T Unix
   systems before BSD picked it up.  */

/*-----------------------------------------------------------------------.
| open3 -- routine to emulate the 3-argument open system.		 |
| 									 |
| open3 (path, flag, mode);						 |
| 									 |
| Attempts to open the file specified by the given pathname.  The	 |
| following flag bits specify options to the routine.  Needless to say,	 |
| you should only specify one of the first three.  Function returns file |
| descriptor if successful, -1 and errno if not.			 |
`-----------------------------------------------------------------------*/

/* The routine obeys the following mode arguments:

   O_RDONLY	file open for read only
   O_WRONLY	file open for write only
   O_RDWR	file open for both read & write

   O_CREAT	file is created with specified mode if it needs to be
   O_TRUNC	if file exists, it is truncated to 0 bytes
   O_EXCL	used with O_CREAT--routine returns error if file exists  */

/* Call that if present in most modern Unix systems.  This version attempts
   to support all the flag bits except for O_NDELAY and O_APPEND, which are
   silently ignored.  The emulation is not as efficient as the real thing
   (at worst, 4 system calls instead of one), but there's not much I can do
   about that.  */

/* Array to give arguments to access for various modes FIXME, this table
   depends on the specific integer values of O_*, and also contains
   integers (args to 'access') that should be #define's.  */

static int modes[] =
  {
    04,				/* O_RDONLY */
    02,				/* O_WRONLY */
    06,				/* O_RDWR */
    06,				/* invalid, just cope: O_WRONLY+O_RDWR */
  };

/* Shut off the automatic emulation of open(), we'll need it. */
#undef open

int
open3 (char *path, int flags, int mode)
{
  int exists = 1;
  int call_creat = 0;

  /* We actually do the work by calling the open() or creat() system
     call, depending on the flags.  Call_creat is true if we will use
     creat(), false if we will use open().  */

  /* See if the file exists and is accessible in the requested mode.

     Strictly speaking we shouldn't be using access, since access checks
     against real uid, and the open call should check against euid.  Most
     cases real uid == euid, so it won't matter.  FIXME.  FIXME, the
     construction "flags & 3" and the modes table depends on the specific
     integer values of the O_* #define's.  Foo!  */

  if (access (path, modes[flags & 3]) < 0)
    {
      if (errno == ENOENT)
	{
	  /* The file does not exist.  */

	  exists = 0;
	}
      else
	{
	  /* Probably permission violation.  */

	  if (flags & O_EXCL)
	    {
	      /* Oops, the file exists, we didn't want it.  No matter
		 what the error, claim EEXIST.  */

	      errno = EEXIST;	/* FIXME: errno should be read-only */
	    }
	  return -1;
	}
    }

  /* If we have the O_CREAT bit set, check for O_EXCL.  */

  if (flags & O_CREAT)
    {
      if ((flags & O_EXCL) && exists)
	{
	  /* Oops, the file exists and we didn't want it to.  */

	  errno = EEXIST;	/* FIXME: errno should be read-only */
	  return -1;
	}

      /* If the file doesn't exist, be sure to call creat() so that it
	 will be created with the proper mode.  */

      if (!exists)
	call_creat = 1;
    }
  else
    {
      /* If O_CREAT isn't set and the file doesn't exist, error.  */

      if (!exists)
	{
	  errno = ENOENT;	/* FIXME: errno should be read-only */
	  return -1;
	}
    }

  /* If the O_TRUNC flag is set and the file exists, we want to call
     creat() anyway, since creat() guarantees that the file will be
     truncated and open()-for-writing doesn't.  (If the file doesn't
     exist, we're calling creat() anyway and the file will be created
     with zero length.)  */

  if ((flags & O_TRUNC) && exists)
    call_creat = 1;

  /* Actually do the call.  */

  if (call_creat)

    /* Call creat.  May have to close and reopen the file if we want
       O_RDONLY or O_RDWR access -- creat() only gives O_WRONLY.  */

    {
      int fd = creat (path, mode);

      if (fd < 0 || (flags & O_WRONLY))
	return fd;
      if (close (fd) < 0)
	return -1;

      /* Fall out to reopen the file we've created.  */
    }

  /* Calling old open, we strip most of the new flags just in case.  */

  return open (path, flags & (O_RDONLY | O_WRONLY | O_RDWR | O_BINARY));
}

#endif /* EMUL_OPEN3 */
