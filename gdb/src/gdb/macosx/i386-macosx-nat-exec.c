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
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "regcache.h"
#include "gdb_assert.h"
#include "i386-tdep.h"
#include "i387-tdep.h"
#include "gdbarch.h"

#include "i386-macosx-thread-status.h"
#include "i386-macosx-tdep.h"

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
          if (!deprecated_register_valid[i])
            fetch_inferior_registers (i);
        }
    }
  else if (!deprecated_register_valid[regno])
    {
      fetch_inferior_registers (regno);
    }
}

/* NOTE: the following code was just lifted from i386-tdep.c.  Ultra-cheesy,
   but it's time to get this thing building for submission...
   jmolenda/2004-05-17  */

static int
i386_sse_regnum_p (struct gdbarch *gdbarch, int regnum)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

#define I387_ST0_REGNUM tdep->st0_regnum
#define I387_NUM_XMM_REGS tdep->num_xmm_regs

  if (I387_NUM_XMM_REGS == 0)
    return 0;

  return (I387_XMM0_REGNUM <= regnum && regnum < I387_MXCSR_REGNUM);

#undef I387_ST0_REGNUM
#undef I387_NUM_XMM_REGS
}

/* NOTE: the following code was just lifted from i386-tdep.c.  Ultra-cheesy,
   but it's time to get this thing building for submission...
   jmolenda/2004-05-17  */

static int
i386_mxcsr_regnum_p (struct gdbarch *gdbarch, int regnum)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

#define I387_ST0_REGNUM tdep->st0_regnum
#define I387_NUM_XMM_REGS tdep->num_xmm_regs

  if (I387_NUM_XMM_REGS == 0)
    return 0;

  return (regnum == I387_MXCSR_REGNUM);

#undef I387_ST0_REGNUM
#undef I387_NUM_XMM_REGS
}


/* Read register values from the inferior process.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
fetch_inferior_registers (int regno)
{
  int current_pid;
  thread_t current_thread;
  int fetched = 0;

  current_pid = ptid_get_pid (inferior_ptid);
  current_thread = ptid_get_tid (inferior_ptid);

  if ((regno == -1) || IS_GP_REGNUM (regno))
    {
      gdb_i386_thread_state_t gp_regs;
      unsigned int gp_count = GDB_i386_THREAD_STATE_COUNT;
      kern_return_t ret = thread_get_state
        (current_thread, GDB_i386_THREAD_STATE, (thread_state_t) & gp_regs,
         &gp_count);
      MACH_CHECK_ERROR (ret);
      i386_macosx_fetch_gp_registers (&gp_regs);
      fetched++;
    }

  if ((regno == -1) 
      || IS_FP_REGNUM (regno)
      || i386_sse_regnum_p (current_gdbarch, regno)
      || i386_mxcsr_regnum_p (current_gdbarch, regno))
    {
      gdb_i386_thread_fpstate_t fp_regs;
      unsigned int fp_count = GDB_i386_THREAD_FPSTATE_COUNT;
      kern_return_t ret = thread_get_state
        (current_thread, GDB_i386_THREAD_FPSTATE, (thread_state_t) & fp_regs,
         &fp_count);
      MACH_CHECK_ERROR (ret);
      i386_macosx_fetch_fp_registers (&fp_regs);
      fetched++;
    }

  if (! fetched)
    {
      warning ("unknown register %d", regno);
      supply_register (regno, NULL);
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

  if ((regno == -1) || IS_GP_REGNUM (regno))
    {
      gdb_i386_thread_state_t gp_regs;
      kern_return_t ret;
      i386_macosx_store_gp_registers (&gp_regs);
      ret = thread_set_state (current_thread, GDB_i386_THREAD_STATE,
                              (thread_state_t) & gp_regs,
                              GDB_i386_THREAD_STATE_COUNT);
      MACH_CHECK_ERROR (ret);
    }

  if ((regno == -1)
      || IS_FP_REGNUM (regno)
      || i386_sse_regnum_p (current_gdbarch, regno)
      || i386_mxcsr_regnum_p (current_gdbarch, regno))
    {
      gdb_i386_thread_fpstate_t fp_regs;
      kern_return_t ret;
      if (i386_macosx_store_fp_registers (&fp_regs))
        {
           ret = thread_set_state (current_thread, GDB_i386_THREAD_FPSTATE,
                                  (thread_state_t) & fp_regs,
                                  GDB_i386_THREAD_FPSTATE_COUNT);
           MACH_CHECK_ERROR (ret);
        }
    }
}
