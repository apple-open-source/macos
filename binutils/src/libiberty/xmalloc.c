/* memory allocation routines with error checking.
   Copyright 1989, 90, 91, 92, 93, 94 Free Software Foundation, Inc.
   
This file is part of the libiberty library.
Libiberty is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

Libiberty is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with libiberty; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "ansidecl.h"
#include "libiberty.h"

#define HAVE_EFENCE 1
#undef HAVE_SBRK

#include <stdio.h>

#ifdef __STDC__
#include <stddef.h>
#else
#define size_t unsigned long
#define ptrdiff_t long
#endif

#if VMS
#include <stdlib.h>
#include <unixlib.h>
#else
/* For systems with larger pointers than ints, these must be declared.  */
PTR malloc PARAMS ((size_t));
PTR realloc PARAMS ((PTR, size_t));
PTR calloc PARAMS ((size_t, size_t));
PTR sbrk PARAMS ((ptrdiff_t));
void free (PTR ptr);
#endif

#ifdef HAVE_EFENCE
#include "efence.h"
#endif

#include <assert.h>
#include <limits.h>

#define MAX_SIZE ULONG_MAX

#undef malloc
#undef realloc
#undef free

#if HAVE_EFENCE
int use_efence = 0;
#endif

/* The program name if set.  */
static const char *name = "";

#ifdef HAVE_SBRK
/* The initial sbrk, set when the program name is set. Not used for win32
   ports other than cygwin32.  */
static char *first_break = NULL;
#endif /* HAVE_SBRK */

void
xmalloc_set_program_name (s)
     const char *s;
{
  name = s;
#ifdef HAVE_SBRK
  /* Win32 ports other than cygwin32 don't have brk() */
  if (first_break == NULL)
    first_break = (char *) sbrk (0);
#endif /* HAVE_SBRK */
}

#if defined (USE_MMALLOC)

#include <mmalloc.h>

#else /* ! USE_MMALLOC */

PTR
mmalloc (md, size)
     PTR md;
     size_t size;
{
  assert (size < MAX_SIZE);
  return malloc (size);
}

PTR
mcalloc (md, nmemb, size)
     PTR md;
     size_t nmemb;
     size_t size;
{
  assert (nmemb < (MAX_SIZE / size));
  return calloc (nmemb, size);
}

PTR
mrealloc (md, ptr, size)
     PTR md;
     PTR ptr;
     size_t size;
{
  assert (size < MAX_SIZE);
  if (ptr == 0)         /* Guard against old realloc's */
    return malloc (size);
  else
    return realloc (ptr, size);
}

void
mfree (md, ptr)
     PTR md;
     PTR ptr;
{
  free (ptr);
}

#endif  /* USE_MMALLOC */

static void nomem (size_t size)
{
#ifdef HAVE_SBRK
  extern char **environ;
  size_t allocated;
  
  if (first_break != NULL)
    allocated = (char *) sbrk (0) - first_break;
  else
    allocated = (char *) sbrk (0) - (char *) &environ;
  fprintf (stderr,
	   "\n%s%sUnable to allocate %lu bytes (%lu bytes already allocated)\n",
	   name, *name ? ": " : "",
	   (unsigned long) size, (unsigned long) allocated);
#else /* ! HAVE_SBRK */
  fprintf (stderr,
	   "\n%s%sUnable to allocate %lu bytes\n",
	   name, *name ? ": " : "",
	   (unsigned long) size);
#endif /* HAVE_SBRK */
  abort ();
}

/* Like mmalloc but get error if no storage available, and protect against
   the caller wanting to allocate zero bytes.  Whether to return NULL for
   a zero byte request, or translate the request into a request for one
   byte of zero'd storage, is a religious issue. */

PTR
xmmalloc (md, size)
     PTR md;
     size_t size;
{
  PTR val;

  assert (size < MAX_SIZE);

  if (size == 0)
    return NULL;
  
#if HAVE_EFENCE
  if (use_efence) {
    val = efence_malloc (size);
  } else {
#endif
    val = mmalloc (md, size);
#if HAVE_EFENCE
  }
#endif

  if (val == NULL)
    nomem (size);
  
  return val;
}

PTR
xmcalloc (md, nelem, elsize)
     PTR md;
     size_t nelem;
     size_t elsize;
{
  PTR val;

  if (nelem == 0 || elsize == 0)
    return NULL;

  assert (nelem < (MAX_SIZE / elsize));

#if HAVE_EFENCE
  if (use_efence) {
    val = efence_calloc (nelem, elsize);
  } else {
#endif
    val = mcalloc (md, nelem, elsize);
#if HAVE_EFENCE
  }
#endif

  if (val == NULL)
    nomem (nelem * elsize);
  
  return val;
}

/* Like mrealloc but get error if no storage available.  */

PTR
xmrealloc (md, ptr, size)
     PTR md;
     PTR ptr;
     size_t size;
{
  PTR val;

  assert (size < MAX_SIZE);

#if HAVE_EFENCE
  if (use_efence) {
    if (ptr != NULL)
      val = efence_realloc (ptr, size);
    else
      val = efence_malloc (size);
  } else {
#endif
    if (ptr != NULL)
      val = mrealloc (md, ptr, size);
    else
      val = mmalloc (md, size);
#if HAVE_EFENCE
  }
#endif

  if (val == NULL)
      nomem (size);

  return val;
}

void
xmfree (md, ptr)
     PTR md;
     PTR ptr;
{
  if (ptr == NULL)
    return;

#if HAVE_EFENCE
  if (use_efence) {
    efence_free (ptr);
  } else {
#endif
    mfree (md, ptr);
#if HAVE_EFENCE
  }
#endif
}

/* Like malloc but get error if no storage available, and protect against
   the caller wanting to allocate zero bytes.  */

PTR
xmalloc (size)
     size_t size;
{
  assert (size < MAX_SIZE);
  return (xmmalloc ((PTR) NULL, size));
}

PTR
xcalloc (nelem, elsize)
     size_t nelem;
     size_t elsize;
{
  assert ((nelem * elsize) < MAX_SIZE);
  return (xmcalloc ((PTR) NULL, nelem, elsize));
}

/* Like mrealloc but get error if no storage available.  */

PTR
xrealloc (ptr, size)
     PTR ptr;
     size_t size;
{
  assert (size < MAX_SIZE);
  return (xmrealloc ((PTR) NULL, ptr, size));
}

void
xfree (ptr)
     PTR ptr;
{
  return xmfree ((PTR) NULL, ptr);
}
