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

#include "macosx-nat-inferior.h"
#include "macosx-nat-cfmthread.h"

#include "defs.h"
#include "gdbcmd.h"
#include "breakpoint.h"
#include "annotate.h"

static CORE_ADDR lookup_address (const char *s)
{
  struct minimal_symbol *msym;
  msym = lookup_minimal_symbol (s, NULL, NULL);
  if (msym == NULL) {
    error ("unable to locate symbol \"%s\"", s);
  }
  return SYMBOL_VALUE_ADDRESS (msym);
}

void macosx_cfm_thread_init (macosx_cfm_thread_status *s)
{
  s->notify_debugger = 0;
  s->info_api_cookie = 0;
}

void macosx_cfm_thread_create (macosx_cfm_thread_status *s, task_t task)
{
  struct breakpoint *b;
  struct symtab_and_line sal;
  char buf[64];

  snprintf (buf, 64, "PrepareClosure + %s", core_addr_to_string (s->breakpoint_offset));
  s->notify_debugger = lookup_address ("PrepareClosure") + s->breakpoint_offset;

  INIT_SAL (&sal);
  sal.pc = s->notify_debugger;
  b = set_momentary_breakpoint (sal, NULL, bp_shlib_event);
  b->disposition = disp_donttouch;
  b->thread = -1;
  b->addr_string = savestring (buf, strlen (buf));

  breakpoints_changed ();
}

void macosx_cfm_thread_destroy (macosx_cfm_thread_status *s)
{
  s->notify_debugger = 0;
  s->info_api_cookie = 0;
}

void
_initialize_macosx_nat_cfmthread ()
{
}
