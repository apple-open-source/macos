// -*- C++ -*- std::terminate, std::unexpected and friends.
// Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002
// Free Software Foundation
//
// This file is part of GCC.
//
// GCC is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// GCC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with GCC; see the file COPYING.  If not, write to
// the Free Software Foundation, 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA. 

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

#include "typeinfo"
#include "exception"
#include <cstdlib>
#include "unwind-cxx.h"
#include "exception_defines.h"

/* APPLE LOCAL begin keymgr */
#if defined(__APPLE__) && defined(__ppc__) && defined(PIC)
#include "bits/os_defines.h"
#endif
/* APPLE LOCAL end keymgr */

using namespace __cxxabiv1;

void
__cxxabiv1::__terminate (std::terminate_handler handler)
{
  try {
    handler ();
    std::abort ();
  } catch (...) {
    std::abort ();
  }
}

void
std::terminate ()
{
  /* APPLE LOCAL begin keymgr */
#if defined(__APPLE__) && defined(__ppc__) && defined(PIC)
  /*
   * If the Key Manager has a terminate function assigned to this thread, invoke that fn.
   * If not (KeyMgr has 0), use whatever is initialized into my local static pointer (above).
   */
  std::terminate_handler __keymgr_terminate_func = (std::terminate_handler)
    _keymgr_get_per_thread_data (KEYMGR_TERMINATE_HANDLER_KEY);
  if (__keymgr_terminate_func)
     __terminate_handler = __keymgr_terminate_func;
#endif /* __APPLE__ etc. */
  /* APPLE LOCAL end keymgr */
  __terminate (__terminate_handler);
}

void
__cxxabiv1::__unexpected (std::unexpected_handler handler)
{
  handler();
  std::terminate ();
}

void
std::unexpected ()
{
  /* APPLE LOCAL begin keymgr */
#if defined(__APPLE__) && defined(__ppc__) && defined(PIC)
  /* Similar to terminate case above. */
   std::unexpected_handler __keymgr_unexpected_func = (std::unexpected_handler)
     _keymgr_get_per_thread_data (KEYMGR_UNEXPECTED_HANDLER_KEY);
   if (__keymgr_unexpected_func)
     __unexpected_handler = __keymgr_unexpected_func;
#endif /* __APPLE__ etc. */
  /* APPLE LOCAL end keymgr */
  __unexpected (__unexpected_handler);
}

std::terminate_handler
std::set_terminate (std::terminate_handler func) throw()
{
  /* APPLE LOCAL begin keymgr */
#if defined(__APPLE__) && defined(__ppc__) && defined(PIC)
  std::terminate_handler old =
    (std::terminate_handler) _keymgr_get_per_thread_data (KEYMGR_TERMINATE_HANDLER_KEY);
  _keymgr_set_per_thread_data (KEYMGR_TERMINATE_HANDLER_KEY, (void *) func) ;
  if ( ! old)
    old = __terminate_handler;
#else
  std::terminate_handler old = __terminate_handler;
#endif /* __APPLE__ etc. */
  /* APPLE LOCAL end keymgr */
  __terminate_handler = func;
  return old;
}

std::unexpected_handler
std::set_unexpected (std::unexpected_handler func) throw()
{
  /* APPLE LOCAL begin keymgr */
#if defined(__APPLE__) && defined(__ppc__) && defined(PIC)
  std::unexpected_handler old =
    (std::unexpected_handler) _keymgr_get_per_thread_data (KEYMGR_UNEXPECTED_HANDLER_KEY);
  _keymgr_set_per_thread_data (KEYMGR_UNEXPECTED_HANDLER_KEY, (void *) func);
  if ( ! old)
    old = __unexpected_handler;
#else
  std::unexpected_handler old = __unexpected_handler;
#endif /* __APPLE__ etc. */
  /* APPLE LOCAL end keymgr */
  __unexpected_handler = func;
  return old;
}
