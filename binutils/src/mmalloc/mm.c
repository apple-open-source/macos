/* Build the entire mmalloc library as a single object module. This
   avoids having clients pick up part of their allocation routines
   from mmalloc and part from libc, which results in undefined
   behavior.  It should also still be possible to build the library
   as a standard library with multiple objects.

   Copyright 1996, 2000 Free Software Foundation

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
Boston, MA 02111-1307, USA. */

#include "config.h"

#include "mmprivate.h"

/* The mmalloc() package can use a single implicit malloc descriptor
   for mmalloc/mrealloc/mfree operations which do not supply an explicit
   descriptor.  For these operations, sbrk() is used to obtain more core
   from the system, or return core.  This allows mmalloc() to provide
   backwards compatibility with the non-mmap'd version. */

struct mdesc *__mmalloc_default_mdp = NULL;

#ifdef HAVE_UNISTD_H
#include <unistd.h>	/* Prototypes for lseek, sbrk (maybe) */
#endif

#include "mcalloc.c"
#include "mfree.c"
#include "mmalloc.c"
#include "mmemalign.c"
#include "mmstats.c"
#include "mmtrace.c"
#include "mrealloc.c"
#include "mvalloc.c"
#include "attach.c"
#include "detach.c"
#include "keys.c"
#include "sbrk-sup.c"
#include "mmap-sup.c"
#include "malloc-sup.c"
#include "check-sup.c"
