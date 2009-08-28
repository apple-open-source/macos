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
#include "arch-utils.h"

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
          if (!register_cached (i))
            fetch_inferior_registers (i);
        }
    }
  else if (!register_cached (regno))
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
  
/* ifdef the following code so that gdb doesn't send the new
   GDB_x86_THREAD_STATE constant when built on an older x86 MacOS X 10.4
   system that won't recognize it.  In Leopard this is unnecessary.  */
   
  if (TARGET_OSABI == GDB_OSABI_UNKNOWN)
    {
      /* Attaching to a process.  Let's figure out what kind it is. */
      gdb_x86_thread_state_t gp_regs;
      struct gdbarch_info info;
      unsigned int gp_count = GDB_x86_THREAD_STATE_COUNT;
      kern_return_t ret = thread_get_state
        (current_thread, GDB_x86_THREAD_STATE, (thread_state_t) & gp_regs,
         &gp_count);
      if (ret != KERN_SUCCESS)
	{
	  printf ("Error calling thread_get_state for GP registers for thread 0x%ulx", current_thread);
	  MACH_CHECK_ERROR (ret);
	}

      gdbarch_info_init (&info);
      gdbarch_info_fill (current_gdbarch, &info);
      info.byte_order = gdbarch_byte_order (current_gdbarch);
      if (gp_regs.tsh.flavor == GDB_x86_THREAD_STATE64)
        {
          info.osabi = GDB_OSABI_DARWIN64;
          info.bfd_arch_info = bfd_lookup_arch (bfd_arch_i386, bfd_mach_x86_64);
        }
      else
        {
          info.osabi = GDB_OSABI_DARWIN;
          info.bfd_arch_info = bfd_lookup_arch (bfd_arch_i386, 
                                                           bfd_mach_i386_i386);
        }
      gdbarch_update_p (info);
    }

  if (TARGET_OSABI == GDB_OSABI_DARWIN64)
    {
      if ((regno == -1) || IS_GP_REGNUM_64 (regno))
        {
          gdb_x86_thread_state_t gp_regs;
          unsigned int gp_count = GDB_x86_THREAD_STATE_COUNT;
          kern_return_t ret = thread_get_state
            (current_thread, GDB_x86_THREAD_STATE, (thread_state_t) & gp_regs,
             &gp_count);
	  if (ret != KERN_SUCCESS)
	    {
	      printf ("Error calling thread_get_state for GP registers for thread 0x%ulx", current_thread);
	      MACH_CHECK_ERROR (ret);
	    }
          x86_64_macosx_fetch_gp_registers (&gp_regs.uts.ts64);
          fetched++;
        }

      if ((regno == -1) 
          || IS_FP_REGNUM_64 (regno)
          || IS_VP_REGNUM_64 (regno))
        {
          gdb_x86_float_state_t fp_regs;
          unsigned int fp_count = GDB_x86_FLOAT_STATE_COUNT;
          kern_return_t ret = thread_get_state
            (current_thread, GDB_x86_FLOAT_STATE, (thread_state_t) & fp_regs,
             &fp_count);
	  if (ret != KERN_SUCCESS)
	    {
	      printf ("Error calling thread_get_state for float registers for thread 0x%ulx", current_thread);
	      MACH_CHECK_ERROR (ret);
	    }
          x86_64_macosx_fetch_fp_registers (&fp_regs.ufs.fs64);
          fetched++;
        }
    }
  else
    {
      if ((regno == -1) || IS_GP_REGNUM (regno))
        {
          gdb_x86_thread_state_t gp_regs;
          unsigned int gp_count = GDB_x86_THREAD_STATE_COUNT;
          kern_return_t ret = thread_get_state
            (current_thread, GDB_x86_THREAD_STATE, (thread_state_t) & gp_regs,
             &gp_count);
	  if (ret != KERN_SUCCESS)
	    {
	      printf ("Error calling thread_get_state for GP registers for thread 0x%ulx", current_thread);
	      MACH_CHECK_ERROR (ret);
	    }
          i386_macosx_fetch_gp_registers (&(gp_regs.uts.ts32));
          fetched++;
        }

      if ((regno == -1) 
          || IS_FP_REGNUM (regno)
          || i386_sse_regnum_p (current_gdbarch, regno)
          || i386_mxcsr_regnum_p (current_gdbarch, regno))
        {
          gdb_i386_float_state_t fp_regs;
          unsigned int fp_count = GDB_i386_FLOAT_STATE_COUNT;
          kern_return_t ret = thread_get_state
            (current_thread, GDB_i386_FLOAT_STATE, (thread_state_t) & fp_regs,
             &fp_count);
	  if (ret != KERN_SUCCESS)
	    {
	      printf ("Error calling thread_get_state for float registers for thread 0x%ulx", current_thread);
	      MACH_CHECK_ERROR (ret);
	    }
          i386_macosx_fetch_fp_registers (&fp_regs);
          fetched++;
        }
    }

  if (! fetched)
    {
      warning ("unknown register %d", regno);
      regcache_raw_supply (current_regcache, regno, NULL);
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

  if (TARGET_OSABI == GDB_OSABI_DARWIN64)
    {
      if ((regno == -1) || IS_GP_REGNUM_64 (regno))
        {
          gdb_x86_thread_state_t gp_regs;
          kern_return_t ret;
          gp_regs.tsh.flavor = GDB_x86_THREAD_STATE64;
          gp_regs.tsh.count = GDB_x86_THREAD_STATE64_COUNT;
          x86_64_macosx_store_gp_registers (&gp_regs.uts.ts64);
          ret = thread_set_state (current_thread, GDB_x86_THREAD_STATE,
                                  (thread_state_t) & gp_regs,
                                  GDB_x86_THREAD_STATE_COUNT);
          MACH_CHECK_ERROR (ret);
        }

      if ((regno == -1)
          || IS_FP_REGNUM_64 (regno)
          || IS_VP_REGNUM_64 (regno))
        {
          gdb_x86_float_state_t fp_regs;
          kern_return_t ret;
          fp_regs.fsh.flavor = GDB_x86_FLOAT_STATE64;
          fp_regs.fsh.count = GDB_x86_FLOAT_STATE64_COUNT;
          if (x86_64_macosx_store_fp_registers (&fp_regs.ufs.fs64))
            {
               ret = thread_set_state (current_thread, GDB_x86_FLOAT_STATE,
                                      (thread_state_t) & fp_regs,
                                      GDB_x86_FLOAT_STATE_COUNT);
               MACH_CHECK_ERROR (ret);
            }
        }
    }
  else
    {
      if ((regno == -1) || IS_GP_REGNUM (regno))
        {
          gdb_x86_thread_state_t gp_regs;
          kern_return_t ret;
          gp_regs.tsh.flavor = GDB_x86_THREAD_STATE32;
          gp_regs.tsh.count = GDB_x86_THREAD_STATE32_COUNT;
          i386_macosx_store_gp_registers (&(gp_regs.uts.ts32));
          ret = thread_set_state (current_thread, GDB_x86_THREAD_STATE,
                                  (thread_state_t) & gp_regs,
                                  GDB_x86_THREAD_STATE_COUNT);
          MACH_CHECK_ERROR (ret);
        }

      if ((regno == -1)
          || IS_FP_REGNUM (regno)
          || i386_sse_regnum_p (current_gdbarch, regno)
          || i386_mxcsr_regnum_p (current_gdbarch, regno))
        {
          gdb_i386_float_state_t fp_regs;
          kern_return_t ret;
          if (i386_macosx_store_fp_registers (&fp_regs))
            {
               ret = thread_set_state (current_thread, GDB_i386_FLOAT_STATE,
                                      (thread_state_t) & fp_regs,
                                      GDB_i386_FLOAT_STATE_COUNT);
               MACH_CHECK_ERROR (ret);
            }
        }
    }
}


/* Support for debug registers, boosted mostly from i386-linux-nat.c.  */

#ifndef DR_FIRSTADDR
#define DR_FIRSTADDR 0
#endif

#ifndef DR_LASTADDR
#define DR_LASTADDR 3
#endif

#ifndef DR_STATUS
#define DR_STATUS 6
#endif

#ifndef DR_CONTROL
#define DR_CONTROL 7
#endif


/* This function handles the case of an i386 or an x86_64 inferior.
   In the i386 case, only the lower 32 bits of the argument VALUE are
   used.  */

static void
i386_macosx_dr_set (int regnum, uint64_t value)
{
  thread_t current_thread;
  x86_debug_state_t dr_regs;
  unsigned int dr_count = x86_DEBUG_STATE_COUNT;
  kern_return_t ret;
  thread_array_t thread_list;
  unsigned int nthreads;
  int i;

  gdb_assert (regnum >= 0 && regnum <= DR_CONTROL);

  /* We have to set the watchpoint value in all the threads.  */
  ret = task_threads (macosx_status->task, &thread_list, &nthreads);
  if (ret != KERN_SUCCESS)
    {
      printf_unfiltered ("Error getting the task threads for task: 0x%x.\n",
			 (int) macosx_status->task);
      MACH_CHECK_ERROR (ret);
    }

  for (i = 0; i < nthreads; i++)
    {
      current_thread = thread_list[i];

      if (TARGET_OSABI == GDB_OSABI_DARWIN64)
        {
          dr_regs.dsh.flavor = x86_DEBUG_STATE64;
          dr_regs.dsh.count = x86_DEBUG_STATE64_COUNT;
          ret = thread_get_state (current_thread, x86_DEBUG_STATE,
                                  (thread_state_t) &dr_regs, &dr_count);

          if (ret != KERN_SUCCESS)
            {
              printf_unfiltered ("Error reading debug registers thread 0x%x via thread_get_state\n", (int) current_thread);
              MACH_CHECK_ERROR (ret);
            }
          
          switch (regnum) 
            {
            case 0:
              dr_regs.uds.ds64.__dr0 = value;
              break;
            case 1:
              dr_regs.uds.ds64.__dr1 = value;
              break;
            case 2:
              dr_regs.uds.ds64.__dr2 = value;
              break;
            case 3:
              dr_regs.uds.ds64.__dr3 = value;
              break;
            case 4:
              dr_regs.uds.ds64.__dr4 = value;
              break;
            case 5:
              dr_regs.uds.ds64.__dr5 = value;
              break;
            case 6:
              dr_regs.uds.ds64.__dr6 = value;
              break;
            case 7:
              dr_regs.uds.ds64.__dr7 = value;
              break;
            }
          
          ret = thread_set_state (current_thread, x86_DEBUG_STATE,
                                  (thread_state_t) &dr_regs, dr_count);

          if (ret != KERN_SUCCESS)
            {
              printf_unfiltered ("Error writing debug registers thread "
                                 "0x%x via thread_get_state\n", 
                                 (int) current_thread);
              MACH_CHECK_ERROR (ret);
            }
        }
      else
        {
          uint32_t val_32 = value & 0xffffffff;

          dr_regs.dsh.flavor = x86_DEBUG_STATE32;
          dr_regs.dsh.count = x86_DEBUG_STATE32_COUNT;
          dr_count = x86_DEBUG_STATE_COUNT;
          ret = thread_get_state (current_thread, x86_DEBUG_STATE, 
                                  (thread_state_t) &dr_regs, &dr_count);
          
          if (ret != KERN_SUCCESS)
            {
              printf_unfiltered ("Error reading debug registers thread 0x%x via thread_get_state\n", (int) current_thread);
              MACH_CHECK_ERROR (ret);
            }
          
          switch (regnum) 
            {
            case 0:
              dr_regs.uds.ds32.__dr0 = val_32;
              break;
            case 1:
              dr_regs.uds.ds32.__dr1 = val_32;
              break;
            case 2:
              dr_regs.uds.ds32.__dr2 = val_32;
              break;
            case 3:
              dr_regs.uds.ds32.__dr3 = val_32;
              break;
            case 4:
              dr_regs.uds.ds32.__dr4 = val_32;
              break;
            case 5:
              dr_regs.uds.ds32.__dr5 = val_32;
              break;
            case 6:
              dr_regs.uds.ds32.__dr6 = val_32;
              break;
            case 7:
              dr_regs.uds.ds32.__dr7 = val_32;
              break;
            }
          
          ret = thread_set_state (current_thread, x86_DEBUG_STATE, 
                                  (thread_state_t) &dr_regs, dr_count);

          if (ret != KERN_SUCCESS)
            {
              printf_unfiltered ("Error writing debug registers thread "
                                 "0x%x via thread_get_state\n", 
                                 (int) current_thread);
              MACH_CHECK_ERROR (ret);
            }
        }
#if HAVE_TASK_SET_STATE          
      /* Now call task_set_state with the values of the last thread we
         set -- gdb doesn't support putting watchpoints on individual threads
         so it doesn't matter which one we use.  The task_set_state call here
         will make the kernel set the watchpoints on any newly-created 
         threads.  */

      ret = task_set_state (macosx_status->task, x86_DEBUG_STATE, 
                              (thread_state_t) &dr_regs, dr_count);
      if (ret != KERN_SUCCESS)
        {
          printf_unfiltered ("Error writing debug registers task "
                             "0x%x via thread_set_state\n", 
                             (int) macosx_status->task);
          MACH_CHECK_ERROR (ret);
        }
#endif
    }
  ret = vm_deallocate (mach_task_self (), (vm_address_t) thread_list, 
			(nthreads * sizeof (int)));
}


static uint64_t
i386_macosx_dr_get (int regnum)
{
  int current_pid;
  thread_t current_thread;
  x86_debug_state_t dr_regs;
  kern_return_t ret;
  unsigned int dr_count = x86_DEBUG_STATE_COUNT;

  gdb_assert (regnum >= 0 && regnum <= DR_CONTROL);

  current_pid = ptid_get_pid (inferior_ptid);
  current_thread = ptid_get_tid (inferior_ptid);

  if (TARGET_OSABI == GDB_OSABI_DARWIN64)
    {
      dr_regs.dsh.flavor = x86_DEBUG_STATE64;
      dr_regs.dsh.count = x86_DEBUG_STATE64_COUNT;
      ret = thread_get_state (current_thread, x86_DEBUG_STATE,
                              (thread_state_t) &dr_regs, &dr_count);
      if (ret != KERN_SUCCESS)
        {
          printf_unfiltered ("Error reading debug registers thread 0x%x via thread_get_state\n", (int) current_thread);
          MACH_CHECK_ERROR (ret);
        }

      switch (regnum) 
        {
          case 0:
            return dr_regs.uds.ds64.__dr0;
          case 1:
            return dr_regs.uds.ds64.__dr1;
          case 2:
            return dr_regs.uds.ds64.__dr2;
          case 3:
            return dr_regs.uds.ds64.__dr3;
          case 4:
            return dr_regs.uds.ds64.__dr4;
          case 5:
            return dr_regs.uds.ds64.__dr5;
          case 6:
            return dr_regs.uds.ds64.__dr6;
          case 7:
            return dr_regs.uds.ds64.__dr7;
          default:
            return -1;
        }
    }
  else
    {
      dr_regs.dsh.flavor = x86_DEBUG_STATE32;
      dr_regs.dsh.count = x86_DEBUG_STATE32_COUNT;
      dr_count = x86_DEBUG_STATE_COUNT;
      ret = thread_get_state (current_thread, x86_DEBUG_STATE, 
                              (thread_state_t) &dr_regs, &dr_count);
    
      if (ret != KERN_SUCCESS)
        {
          printf_unfiltered ("Error reading debug registers thread 0x%x via thread_get_state\n", (int) current_thread);
          MACH_CHECK_ERROR (ret);
        }
    
      switch (regnum) 
        {
          case 0:
            return dr_regs.uds.ds32.__dr0;
          case 1:
            return dr_regs.uds.ds32.__dr1;
          case 2:
            return dr_regs.uds.ds32.__dr2;
          case 3:
            return dr_regs.uds.ds32.__dr3;
          case 4:
            return dr_regs.uds.ds32.__dr4;
          case 5:
            return dr_regs.uds.ds32.__dr5;
          case 6:
            return dr_regs.uds.ds32.__dr6;
          case 7:
            return dr_regs.uds.ds32.__dr7;
          default:
            return -1;
        }
    }
}

void
i386_macosx_dr_set_control (unsigned long control)
{
  i386_macosx_dr_set (DR_CONTROL, control);
}

void
i386_macosx_dr_set_addr (int regnum, CORE_ADDR addr)
{
  gdb_assert (regnum >= 0 && regnum <= DR_LASTADDR - DR_FIRSTADDR);

  i386_macosx_dr_set (DR_FIRSTADDR + regnum, addr);
}

void
i386_macosx_dr_reset_addr (int regnum)
{
  gdb_assert (regnum >= 0 && regnum <= DR_LASTADDR - DR_FIRSTADDR);

  i386_macosx_dr_set (DR_FIRSTADDR + regnum, 0L);
}

unsigned long
i386_macosx_dr_get_status (void)
{
  return i386_macosx_dr_get (DR_STATUS);
}

/* I have no idea why this target vector entry gets passed in the target.
   That seems REALLY whacky...  */

int
i386_macosx_target_stopped_data_address (struct target_ops *target, 
                                         CORE_ADDR *addr)
{
  return i386_stopped_data_address (addr);
}

int 
i386_macosx_can_use_hw_breakpoint (int unused1, int unused2, int unused3)
{
  return 1;
}

void
macosx_complete_child_target (struct target_ops *target)
{
  target->to_can_use_hw_breakpoint = i386_macosx_can_use_hw_breakpoint;
  target->to_stopped_by_watchpoint = i386_stopped_by_watchpoint;
  target->to_stopped_data_address = i386_macosx_target_stopped_data_address;
  target->to_insert_watchpoint = i386_insert_watchpoint;
  target->to_remove_watchpoint = i386_remove_watchpoint;
  target->to_insert_hw_breakpoint = i386_insert_hw_breakpoint;
  target->to_remove_hw_breakpoint = i386_remove_hw_breakpoint;
  target->to_have_continuable_watchpoint = 1;
}

