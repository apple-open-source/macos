// Implementation file for the -*- C++ -*- dynamic memory management header.
// Copyright (C) 1996, 1997, 1998 Free Software Foundation

// This file is part of GNU CC.

// GNU CC is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.

// GNU CC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with GNU CC; see the file COPYING.  If not, write to
// the Free Software Foundation, 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA. 

// As a special exception, if you link this library with other files,
// some of which are compiled with GCC, to produce an executable,
// this library does not by itself cause the resulting executable
// to be covered by the GNU General Public License.
// This exception does not however invalidate any other reasons why
// the executable file might be covered by the GNU General Public License.

#pragma implementation "new"

#ifdef APPLE_KERNEL_EXTENSION
// Avoid any references (such as in vtables)
// that would cause routines related to exception handling to be pulled in.
#define virtual
#endif

#include "new"

#ifdef APPLE_KERNEL_EXTENSION
#undef  virtual
#elif defined(MACOSX)
#include "apple/keymgr.h"	/* for thread safety */
#endif

const std::nothrow_t std::nothrow = { };

using std::new_handler;
new_handler __new_handler;

new_handler
set_new_handler (new_handler handler)
{
#if defined(MACOSX) && !defined(APPLE_KERNEL_EXTENSION)
  new_handler prev_handler =
    (new_handler) _keymgr_get_per_thread_data (KEYMGR_NEW_HANLDER_KEY);
  _keymgr_set_per_thread_data (KEYMGR_NEW_HANLDER_KEY, handler);
#else
  new_handler prev_handler = __new_handler;
  __new_handler = handler;
#endif
  return prev_handler;
}
