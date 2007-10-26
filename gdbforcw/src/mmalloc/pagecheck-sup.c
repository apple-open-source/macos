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

#include <assert.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <mach/mach.h>
#include <mach/mach_error.h>

#include "mmprivate.h"

#if (defined __GNUC__)
#define __CHECK_FUNCTION ((__const char *) 0)
#else
#define __CHECK_FUNCTION __PRETTY_FUNCTION__
#endif

#define MACH_CHECK_ERROR(ret) \
mach_check_error (ret, __FILE__, __LINE__, __CHECK_FUNCTION);

#define MACH_CHECK_NOERROR(ret) \
mach_check_noerror (ret, __FILE__, __LINE__, __CHECK_FUNCTION);

static void
mach_check_error (kern_return_t ret, const char *file,
		  unsigned int line, const char *func)
{
  if (ret == KERN_SUCCESS)
    return;
  if (func == NULL)
    func = "[UNKNOWN]";

  fprintf (stderr, "fatal Mach error on line %u of \"%s\" in function \"%s\": %s\n",
	   line, file, func, mach_error_string (ret));
  abort ();
}

static size_t page_size ()
{
  static vm_size_t cached_page_size = 0;

  if (cached_page_size == 0)
    {
      kern_return_t kret = host_page_size (mach_host_self(), &cached_page_size);
      MACH_CHECK_ERROR (kret);
    }

  return cached_page_size;
}

static size_t round_up (size_t size, size_t page_size)
{
  size_t nsize = size;
  nsize += (page_size - 1);
  nsize -= (nsize % page_size);
  assert ((nsize - size) < page_size);
  return nsize;
}

static size_t round_down (size_t size, size_t page_size)
{
  size_t nsize = size;
  nsize -= (nsize % page_size);
  return nsize;
}

static void fill (void *buf, unsigned long c, size_t n)
{
  unsigned long *ptr = buf;
  while (n-- > 0) 
    *ptr++ = c;
}

static void check_filled (void *buf, unsigned long c, size_t n)
{
  unsigned long *ptr = buf;
  while (n-- > 0) 
    if (*ptr++ != c)
      abort ();
}

static PTR
morecore_pagecheck (mdp, size)
  struct mdesc *mdp;
  int size;
{
  abort ();
}

#define MAGIC_GUARD 0xfefefefe

int mmalloc_pagecheck_check_alloc = 0;
int mmalloc_pagecheck_check_info_guard = 0;
int mmalloc_pagecheck_check_buffer_guard = 0;
int mmalloc_pagecheck_zero_alloc = 0;
int mmalloc_pagecheck_zero_free = 0;
int mmalloc_pagecheck_guard_end = 0;

static vm_address_t
alloc_buffer (size, at_end)
     size_t size;
     int at_end;
{
  kern_return_t kret;
  vm_address_t base_address, info_page, pre_guard, buffer, post_guard, ptr;

  size_t buffer_size = round_up (size, page_size());
  size_t alloc_size = buffer_size + (3 * page_size());

  kret = vm_allocate (mach_task_self(), &base_address, alloc_size, 1);
  MACH_CHECK_ERROR (kret);

  info_page = base_address;
  pre_guard = info_page + page_size();
  buffer = pre_guard + page_size();
  post_guard = buffer + buffer_size;

  if (mmalloc_pagecheck_check_info_guard)
    fill ((void *) info_page, MAGIC_GUARD, page_size() / 4);
  else
    fill ((void *) (info_page + sizeof (size_t)), MAGIC_GUARD, 1);
    

  *((size_t *) info_page) = size;

  kret = vm_protect (mach_task_self(), info_page, page_size(), 0, VM_PROT_READ);
  MACH_CHECK_ERROR (kret);

  kret = vm_protect (mach_task_self(), pre_guard, page_size(), 0, VM_PROT_NONE);
  MACH_CHECK_ERROR (kret);

  kret = vm_protect (mach_task_self(), post_guard, page_size(), 0, VM_PROT_NONE);
  MACH_CHECK_ERROR (kret);

  if (mmalloc_pagecheck_check_buffer_guard)
    fill ((void *) buffer, MAGIC_GUARD, buffer_size / 4);

  if (at_end)
    ptr = buffer + (buffer_size - size);
  else
    ptr = buffer;

  if (mmalloc_pagecheck_zero_alloc)
    memset ((void *) ptr, 0, size);

  return ptr;
}

static void
validate_buffer (address, pbuffer, psize)
  vm_address_t address;
  vm_address_t *pbuffer;
  size_t *psize;
{
  vm_address_t info_page, pre_guard, buffer, post_guard;
  size_t size, buffer_size;

  buffer = round_down (address, page_size());
  pre_guard = buffer - page_size();
  info_page = pre_guard - page_size();

  size = *((size_t *) info_page);
  buffer_size = round_up (size, page_size());

  post_guard = buffer + buffer_size;

  if (mmalloc_pagecheck_check_info_guard)
    check_filled ((void *) (info_page + sizeof (size_t)),
		  MAGIC_GUARD, ((page_size() - sizeof (size_t)) / 4));
  else
    check_filled ((void *) (info_page + sizeof (size_t)),
		  MAGIC_GUARD, 1);

  *pbuffer = buffer;
  *psize = size;
}

static PTR
mmalloc_pagecheck (md, size)
  PTR md;
  size_t size;
{
  vm_address_t ret = alloc_buffer (size, mmalloc_pagecheck_guard_end);

  if (mmalloc_pagecheck_check_alloc) {
    vm_address_t buffer;
    size_t size;

    validate_buffer (ret, &buffer, &size);
  }

  return (PTR) ret;
}

static void
mfree_pagecheck (md, ptr)
  PTR md;
  PTR ptr;
{
  kern_return_t kret;
  vm_address_t buffer;
  size_t size;

  validate_buffer ((vm_address_t) ptr, &buffer, &size);

  kret = vm_protect (mach_task_self(), buffer, size, 0, VM_PROT_READ | VM_PROT_WRITE);
  MACH_CHECK_ERROR (kret);

  if (mmalloc_pagecheck_zero_free)
    fill ((void *) buffer, MAGIC_GUARD, size / 4);

  kret = vm_protect (mach_task_self(), buffer, size, 0, VM_PROT_NONE);
  MACH_CHECK_ERROR (kret);
}

static PTR
mrealloc_pagecheck (md, ptr, nsize)
  PTR md;
  PTR ptr;
  size_t nsize;
{
  kern_return_t kret;
  vm_address_t buffer;
  size_t size;
  PTR new;

  validate_buffer ((vm_address_t) ptr, &buffer, &size);

  new = mmalloc_pagecheck (md, nsize);

  memcpy (new, ptr, (nsize < size) ? nsize : size);
  if (nsize > size)
    memset (((unsigned char *) new) + size, 0, nsize - size);
  
  if (mmalloc_pagecheck_zero_free)
    fill ((void *) buffer, MAGIC_GUARD, size / 4);

  kret = vm_protect (mach_task_self(), buffer, size, 0, VM_PROT_NONE);
  MACH_CHECK_ERROR (kret);

  return new;
}

struct mdesc *
mmalloc_pagecheck_create ()
{
  struct mdesc *ret = NULL;

  ret = (struct mdesc *) malloc (sizeof (struct mdesc));
  memset ((char *) ret, 0, sizeof (struct mdesc));

  ret->child = NULL;
  ret->fd = -1;

  ret->mfree_hook = mfree_pagecheck;
  ret->mmalloc_hook = mmalloc_pagecheck;
  ret->mrealloc_hook = mrealloc_pagecheck;
  ret->morecore = morecore_pagecheck;
  ret->abortfunc = abort;

  return ret;
}
