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

#include "ppc-macosx-regs.h"
#include "ppc-macosx-frameops.h"
#include "ppc-macosx-frameinfo.h"
#include "ppc-macosx-tdep.h"

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "target.h"
#include "gdbcore.h"
#include "symfile.h"
#include "objfiles.h"
#include "regcache.h"

#include <string.h>

void
ppc_frame_cache_saved_regs (struct frame_info *frame)
{
  if (frame->saved_regs) {
    return;
  }

  frame_saved_regs_zalloc (frame);
    
  ppc_frame_saved_regs (frame, frame->saved_regs);
}

/* Put here the code to store, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.  */

void
ppc_frame_saved_regs (struct frame_info *frame, CORE_ADDR *saved_regs)
{
  CORE_ADDR prev_sp = 0;
  ppc_function_properties *props;
  int i;

  if (ppc_frame_cache_properties (frame, NULL)) { 
    ppc_debug ("frame_initial_stack_address: unable to find properties of " 
	       "function containing 0x%lx\n", (unsigned long) frame->pc);
    return;
  }    
  props = frame->extra_info->props;
  CHECK_FATAL (props != NULL);  

  /* record stored stack pointer */
  if (! props->frameless) {
    prev_sp = saved_regs[SP_REGNUM] = read_memory_unsigned_integer (frame->frame, 4);
  } else {
    prev_sp = saved_regs[SP_REGNUM] = frame->frame;
  }

  /* I am not sure how this is supposed to work.  The saved_regs array is supposed
     to hold the location of the address where the register is saved.  This is tricky
     in the bottom-most frame, but it looks like this cache doesn't get used then.
     So for now I am leaving out all the complex cases in ppc_frame_find_pc. 

     I am also not doing the CALL_DUMMY case, here.  I think that should be
     handled by the CALL_DUMMY unwinder.  */

  if (get_frame_type (frame) == SIGTRAMP_FRAME) 
    {
      CORE_ADDR psp = read_memory_unsigned_integer (frame->frame, 4);
      saved_regs[PC_REGNUM] = psp + PPC_SIGCONTEXT_PC_OFFSET;
    }
  else
    {
      if (prev_sp == 0)
	saved_regs[PC_REGNUM] = 0;
      else
	{
	  saved_regs[PC_REGNUM] = prev_sp + props->lr_offset;
	}
    }
  
  if (props->cr_saved) {
    saved_regs[CR_REGNUM] = prev_sp + props->cr_offset;
  }
  if (props->lr_saved) {
    saved_regs[LR_REGNUM] = prev_sp + props->lr_offset;
  }

  if (props->frameless && ((props->saved_fpr != -1) || (props->saved_gpr != -1))) {
    ppc_debug ("frame_find_saved_regs: "
	       "registers marked as saved in frameless function; ignoring\n");
    return;
  }

  if (props->saved_fpr >= 0) {						
    for (i = props->saved_fpr; i < 32; i++) {				
      long offset = props->fpr_offset + ((i - props->saved_fpr) * sizeof (FP_REGISTER_TYPE));
      saved_regs[FP0_REGNUM + i] = prev_sp + offset;
    }									
  }									
									
  if (props->saved_gpr >= 0) {						
    for (i = props->saved_gpr; i < 32; i++) {				
      long offset = props->gpr_offset + ((i - props->saved_gpr) * sizeof (REGISTER_TYPE));
      saved_regs[GP0_REGNUM + i] = prev_sp + offset;
    }									
  }									
}
