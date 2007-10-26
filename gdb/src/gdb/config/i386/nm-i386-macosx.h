/* Macro definitions for GDB on an Intel i386 running SVR4.
   Copyright (C) 1991, 1994 Free Software Foundation, Inc.
   Written by Fred Fish at Cygnus Support (fnf@cygnus.com)

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef NM_I386NEXT_H
#define NM_I386NEXT_H

#ifdef HAVE_X86_DEBUG_STATE32_T

/* Mac OS X supports the i386 hardware debugging registers.  */
#define I386_USE_GENERIC_WATCHPOINTS

#include "i386/nm-i386.h"

/* BUT... the i386 kdp target does not support watchpoints, and if we 
   try to set them we'll get awkward errors.  The nm-i386.h code 
   unconditionally turns on the HW watchpoints by overriding the 
   target_whatever macros.  We override these definitions so we can
   properly route the calls through the target vector, and so keep
   them out of the kdp side.  */
/* The only one we don't override is TARGET_REGION_OK_FOR_HW_WATCHPOINT.
   That isn't actually a target vector entry, but rather it is a way to
   override the one-argument target vector entry 
     to_region_size_ok_for_hw_watchpoint
   with a version that takes two arguments.  Somebody was getting a little
   lazy when they put this in.  */

#undef STOPPED_BY_WATCHPOINT
#undef TARGET_CAN_USE_HARDWARE_WATCHPOINT
#undef HAVE_CONTINUABLE_WATCHPOINT
#undef target_stopped_data_address
#undef target_insert_watchpoint
#undef target_remove_watchpoint
#undef target_insert_hw_breakpoint
#undef target_remove_hw_breakpoint

extern void i386_macosx_dr_set_control (unsigned long control);
#define I386_DR_LOW_SET_CONTROL(control) \
  i386_macosx_dr_set_control (control)

extern void i386_macosx_dr_set_addr (int regnum, CORE_ADDR addr);
#define I386_DR_LOW_SET_ADDR(regnum, addr) \
  i386_macosx_dr_set_addr (regnum, addr)

extern void i386_macosx_dr_reset_addr (int regnum);
#define I386_DR_LOW_RESET_ADDR(regnum) \
  i386_macosx_dr_reset_addr (regnum)

extern unsigned long i386_macosx_dr_get_status (void);
#define I386_DR_LOW_GET_STATUS() \
  i386_macosx_dr_get_status ()

/* Define this so we can skip the page-protection style watchpoints
   set up over in nm-macosx.h.  */

#define MACOSX_ACTUAL_HARDWARE_WATCHPOINTS_ARE_SUPPORTED 1

#endif /* HAVE_X86_DEBUG_STATE32_T */

#include "nm-macosx.h"
#define TARGET_NATIVE

#endif /* NM_I386NEXT_H */
