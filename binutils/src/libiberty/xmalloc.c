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

/*

@deftypefn Replacement void* xmalloc (size_t)

Allocate memory without fail.  If @code{malloc} fails, this will print
a message to @code{stderr} (using the name set by
@code{xmalloc_set_program_name},
if any) and then call @code{xexit}.  Note that it is therefore safe for
a program to contain @code{#define malloc xmalloc} in its source.

@end deftypefn

@deftypefn Replacement void* xrealloc (void *@var{ptr}, size_t @var{size})
Reallocate memory without fail.  This routine functions like @code{realloc},
but will behave the same as @code{xmalloc} if memory cannot be found.

@end deftypefn

@deftypefn Replacement void* xcalloc (size_t @var{nelem}, size_t @var{elsize})

Allocate memory without fail, and set it to zero.  This routine functions
like @code{calloc}, but will behave the same as @code{xmalloc} if memory
cannot be found.

@end deftypefn

@deftypefn Replacement void xmalloc_set_program_name (const char *@var{name})

You can use this to set the name of the program used by
@code{xmalloc_failed} when printing a failure message.

@end deftypefn

@deftypefn Replacement void xmalloc_failed (size_t)

This function is not meant to be called by client code, and is listed
here for completeness only.  If any of the allocation routines fail, this
function will be called to print an error message and terminate execution.

@end deftypefn

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "ansidecl.h"
#include "libiberty.h"

#define HAVE_EFENCE 1
#define HAVE_MMALLOC 1
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

#if HAVE_MMALLOC
#include <mmalloc.h>
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

#if HAVE_MMALLOC
int use_mmalloc = 1;
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

void
xmalloc_failed (size)
     size_t size;
{
#ifdef HAVE_SBRK
  extern char **environ;
  size_t allocated;
  
  if (first_break != NULL)
    allocated = (char *) sbrk (0) - first_break;
  else
    allocated = (char *) sbrk (0) - (char *) &environ;
  fprintf (stderr,
	   "\n%s%sout of memory allocating %lu bytes after a total of %lu bytes\n",
	   name, *name ? ": " : "",
	   (unsigned long) size, (unsigned long) allocated);
#else /* ! HAVE_SBRK */
  fprintf (stderr,
	   "\n%s%sout of memory allocating %lu bytes\n",
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
  
  if (0) { 
  }
#if HAVE_EFENCE
  else if (use_efence) {
    val = efence_malloc (size);
  }
#endif
#if HAVE_MMALLOC
  else if (use_mmalloc) {
    val = mmalloc (md, size);
  }
#endif
  else {
    val = malloc (size);
  }

  if (val == NULL)
    xmalloc_failed (size);
  
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

  if (0) {
  }
#if HAVE_EFENCE
  else if (use_efence) {
    val = efence_calloc (nelem, elsize);
  }
#endif
#if HAVE_MMALLOC
  else if (use_mmalloc) {
    val = mcalloc (md, nelem, elsize);
  }
#endif
  else {
    val = calloc (nelem, elsize);
  }

  if (val == NULL)
    xmalloc_failed (nelem * elsize);
  
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

  if (0) {
  }
#if HAVE_EFENCE
  else if (use_efence) {
    if (ptr != NULL)
      val = efence_realloc (ptr, size);
    else
      val = efence_malloc (size);
  }
#endif
#if HAVE_MMALLOC
  else if (use_mmalloc) {
    if (ptr != NULL)
      val = mrealloc (md, ptr, size);
    else
      val = mmalloc (md, size);
  }
#endif
  else {
    if (ptr != NULL)
      val = realloc (ptr, size);
    else
      val = malloc (size);
  }

  if (val == NULL)
    xmalloc_failed (size);

  return val;
}

void
xmfree (md, ptr)
     PTR md;
     PTR ptr;
{
  if (ptr == NULL)
    return;

  if (0) {
  }
#if HAVE_EFENCE
  else if (use_efence) {
    efence_free (ptr);
  }
#endif
#if HAVE_MMALLOC 
  else if (use_mmalloc) {
    mfree (md, ptr);
  }
#endif
  else {
    free (ptr);
  }
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
