/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.

   This file is part of GDB.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "bfd.h"

void
_initialize_macosx_nat ()
{
  struct rlimit limit;
  rlim_t reserve;

  getrlimit (RLIMIT_NOFILE, &limit);
  limit.rlim_cur = limit.rlim_max;
  setrlimit (RLIMIT_NOFILE, &limit);

  /* Reserve 10% of file descriptors for non-BFD uses, or 5, whichever
     is greater.  Allocate at least one file descriptor for use by
     BFD. */

  reserve = (int) limit.rlim_max * 0.1;
  reserve = (reserve > 5) ? reserve : 5;
  if (reserve >= limit.rlim_max) {
    bfd_set_cache_max_open (1);
  } else {
    bfd_set_cache_max_open (limit.rlim_max - reserve);
  }
}
