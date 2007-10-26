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

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "target.h"
#include "gdbcore.h"
#include "symfile.h"
#include "objfiles.h"
#include "regcache.h"

#include "ppc-macosx-regs.h"
#include "ppc-macosx-regnums.h"
#include "ppc-macosx-thread-status.h"
#include "ppc-macosx-regs.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-inferior.h"

extern macosx_inferior_status *macosx_status;

static void
validate_inferior_registers (int regno)
{
  int i;
  if (regno == -1)
    {
      for (i = 0; i < NUM_REGS; i++)
        {
          if (!register_cached (i))
            fetch_inferior_registers (i);
        }
    }
  else if (!register_cached (regno))
    {
      fetch_inferior_registers (regno);
    }
}

/* Read register values from the inferior process.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
fetch_inferior_registers (int regno)
{
  thread_t current_thread = ptid_get_tid (inferior_ptid);

  if ((regno == -1) || PPC_MACOSX_IS_GP_REGNUM (regno)
      || PPC_MACOSX_IS_GSP_REGNUM (regno))
    {
      gdb_ppc_thread_state_64_t gp_regs;
      unsigned int gp_count = GDB_PPC_THREAD_STATE_64_COUNT;
      kern_return_t ret = thread_get_state
        (current_thread, GDB_PPC_THREAD_STATE_64, (thread_state_t) & gp_regs,
         &gp_count);
      if (ret != KERN_SUCCESS)
	{
	  printf ("Error calling thread_get_state for GP registers for thread 0x%ulx", current_thread);
	  MACH_CHECK_ERROR (ret);
	}
      ppc_macosx_fetch_gp_registers_64 (&gp_regs);
    }

  if ((regno == -1) || PPC_MACOSX_IS_FP_REGNUM (regno)
      || PPC_MACOSX_IS_FSP_REGNUM (regno))
    {
      gdb_ppc_thread_fpstate_t fp_regs;
      unsigned int fp_count = GDB_PPC_THREAD_FPSTATE_COUNT;
      kern_return_t ret = thread_get_state
        (current_thread, GDB_PPC_THREAD_FPSTATE, (thread_state_t) & fp_regs,
         &fp_count);
      if (ret != KERN_SUCCESS)
	{
	  printf ("Error calling thread_get_state for FP registers for thread 0x%ulx", current_thread);
	  MACH_CHECK_ERROR (ret);
	}
      ppc_macosx_fetch_fp_registers (&fp_regs);
    }

  if ((regno == -1) || PPC_MACOSX_IS_VP_REGNUM (regno)
      || PPC_MACOSX_IS_VSP_REGNUM (regno))
    {
      gdb_ppc_thread_vpstate_t vp_regs;
      unsigned int vp_count = GDB_PPC_THREAD_VPSTATE_COUNT;
      kern_return_t ret = thread_get_state
        (current_thread, GDB_PPC_THREAD_VPSTATE, (thread_state_t) & vp_regs,
         &vp_count);
      if (ret != KERN_SUCCESS)
	{
	  printf ("Error calling thread_get_state for Vector registers for thread 0x%ulx", current_thread);
	  MACH_CHECK_ERROR (ret);
	}
      ppc_macosx_fetch_vp_registers (&vp_regs);
    }
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (int regno)
{
  int current_pid;
  thread_t current_thread;

  current_pid = ptid_get_pid (inferior_ptid);
  current_thread = ptid_get_tid (inferior_ptid);

  validate_inferior_registers (regno);

  if ((regno == -1) || PPC_MACOSX_IS_GP_REGNUM (regno)
      || PPC_MACOSX_IS_GSP_REGNUM (regno))
    {
      gdb_ppc_thread_state_64_t gp_regs;
      kern_return_t ret;
      ppc_macosx_store_gp_registers_64 (&gp_regs);
      ret = thread_set_state (current_thread, GDB_PPC_THREAD_STATE_64,
                              (thread_state_t) & gp_regs,
                              GDB_PPC_THREAD_STATE_64_COUNT);
      MACH_CHECK_ERROR (ret);
    }

  if ((regno == -1) || PPC_MACOSX_IS_FP_REGNUM (regno)
      || PPC_MACOSX_IS_FSP_REGNUM (regno))
    {
      gdb_ppc_thread_fpstate_t fp_regs;
      kern_return_t ret;
      ppc_macosx_store_fp_registers (&fp_regs);
      ret = thread_set_state (current_thread, GDB_PPC_THREAD_FPSTATE,
                              (thread_state_t) & fp_regs,
                              GDB_PPC_THREAD_FPSTATE_COUNT);
      MACH_CHECK_ERROR (ret);
    }

  if ((regno == -1) || PPC_MACOSX_IS_VP_REGNUM (regno)
      || PPC_MACOSX_IS_VSP_REGNUM (regno))
    {
      gdb_ppc_thread_vpstate_t vp_regs;
      kern_return_t ret;
      ppc_macosx_store_vp_registers (&vp_regs);
      ret = thread_set_state (current_thread, GDB_PPC_THREAD_VPSTATE,
                              (thread_state_t) & vp_regs,
                              GDB_PPC_THREAD_VPSTATE_COUNT);
      MACH_CHECK_ERROR (ret);
    }
}

void
macosx_complete_child_target (struct target_ops *target)
{
}
