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
#include "gdbarch.h"
#include "arch-utils.h"

#include "macosx-nat-mutils.h"

#include "arm-tdep.h"
#include "arm-macosx-thread-status.h"
#include "arm-macosx-tdep.h"

static inline unsigned int
collect_unsigned_int (int regnum)
{
  gdb_byte buf[4];
  regcache_raw_collect (current_regcache, regnum, buf);
  return extract_unsigned_integer (buf, 4);
}

static inline void
supply_unsigned_int (int regnum, unsigned int val)
{
  gdb_byte buf[4];
  store_unsigned_integer (buf, 4, val);
  regcache_raw_supply (current_regcache, regnum, buf);
}

/* Fetch General Purpose Registers.  */
void
arm_macosx_fetch_gp_registers (struct gdb_arm_thread_state *gp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_GP_REGS; i++)
    supply_unsigned_int (ARM_R0_REGNUM + i, gp_regs->r[i]);
  supply_unsigned_int (ARM_PS_REGNUM, gp_regs->cpsr);
}

void
arm_macosx_fetch_gp_registers_raw (struct gdb_arm_thread_state *gp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_GP_REGS; i++)
	regcache_raw_supply (current_regcache, ARM_R0_REGNUM + i, &gp_regs->r[i]);
  regcache_raw_supply (current_regcache, ARM_PS_REGNUM, &gp_regs->cpsr);
}


/* Store General Purpose Registers.  */
void
arm_macosx_store_gp_registers (struct gdb_arm_thread_state *gp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_GP_REGS; i++)
    gp_regs->r[i] = collect_unsigned_int (ARM_R0_REGNUM + i);
  gp_regs->cpsr = collect_unsigned_int (ARM_PS_REGNUM);
}

void
arm_macosx_store_gp_registers_raw (struct gdb_arm_thread_state *gp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_GP_REGS; i++)
	regcache_raw_collect (current_regcache, ARM_R0_REGNUM + i, &gp_regs->r[i]);
  regcache_raw_collect (current_regcache, ARM_PS_REGNUM, &gp_regs->cpsr);
}

/* Fetch VFP Registers.  */

void
arm_macosx_fetch_vfp_registers (struct gdb_arm_thread_fpstate *fp_regs)
{
  int i;
  uint32_t *r = fp_regs->r;
  for (i = 0; i < ARM_MACOSX_NUM_VFP_REGS; i++)
	regcache_raw_supply (current_regcache, ARM_FIRST_VFP_REGNUM + i, &r[i]);
  regcache_raw_supply (current_regcache, ARM_FPSCR_REGNUM, &fp_regs->fpscr);
}

void
arm_macosx_fetch_vfp_registers_raw (struct gdb_arm_thread_fpstate *fp_regs)
{
  int i;
  uint32_t *r = fp_regs->r;
  for (i = 0; i < ARM_MACOSX_NUM_VFP_REGS; i++)
	regcache_raw_supply (current_regcache, ARM_FIRST_VFP_REGNUM + i, &r[i]);
  regcache_raw_supply (current_regcache, ARM_FPSCR_REGNUM, &fp_regs->fpscr);
}

/* Store VFP Registers.  */

void
arm_macosx_store_vfp_registers (struct gdb_arm_thread_fpstate *fp_regs)
{
  int i;
  uint32_t *r = fp_regs->r;
  for (i = 0; i < ARM_MACOSX_NUM_VFP_REGS; i++)
    r[i] = collect_unsigned_int (ARM_FIRST_VFP_REGNUM + i);
  fp_regs->fpscr = collect_unsigned_int (ARM_FPSCR_REGNUM);
}

void
arm_macosx_store_vfp_registers_raw (struct gdb_arm_thread_fpstate *fp_regs)
{
  int i;
  uint32_t *r = fp_regs->r;
  for (i = 0; i < ARM_MACOSX_NUM_VFP_REGS; i++)
	regcache_raw_collect (current_regcache, ARM_FIRST_VFP_REGNUM + i, &r[i]);
  regcache_raw_collect (current_regcache, ARM_FPSCR_REGNUM, &fp_regs->fpscr);
}



static void
validate_inferior_registers (int regno)
{
  int i;
  if (regno == -1)
    {
      for (i = 0; i < NUM_GREGS; i++)
        {
          if (!register_cached (i))
            fetch_inferior_registers (i);
        }
      if (!register_cached (ARM_PS_REGNUM))
	fetch_inferior_registers (ARM_PS_REGNUM);
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
  int i;
  thread_t current_thread = ptid_get_tid (inferior_ptid);

  if (TARGET_OSABI == GDB_OSABI_UNKNOWN)
    {
      /* Attaching to a process.  Let's figure out what kind of system
         we're on. */
      struct gdb_arm_thread_fpstate vfp_regs;
      struct gdbarch_info info;
      unsigned int vfp_count = GDB_ARM_THREAD_FPSTATE_COUNT;
      kern_return_t ret = thread_get_state
        (current_thread, GDB_ARM_THREAD_FPSTATE, (thread_state_t) & vfp_regs,
        &vfp_count);
      
      gdbarch_info_init (&info);
      gdbarch_info_fill (current_gdbarch, &info);
      info.byte_order = gdbarch_byte_order (current_gdbarch);
      if (ret == KERN_SUCCESS)
        {
          info.osabi = GDB_OSABI_DARWINV6;
          info.bfd_arch_info = bfd_lookup_arch (bfd_arch_arm, bfd_mach_arm_6);
        }
      else
        {
          info.osabi = GDB_OSABI_DARWIN;
          info.bfd_arch_info = bfd_lookup_arch (bfd_arch_arm, 0);
        }
      gdbarch_update_p (info);
    }
  
  if ((regno == -1) || ARM_MACOSX_IS_GP_RELATED_REGNUM (regno))
    {
      struct gdb_arm_thread_state gp_regs;
      unsigned int gp_count = GDB_ARM_THREAD_STATE_COUNT;
      kern_return_t ret = thread_get_state
        (current_thread, GDB_ARM_THREAD_STATE, (thread_state_t) & gp_regs,
         &gp_count);
      if (ret != KERN_SUCCESS)
       {
         printf ("Error calling thread_get_state for GP registers for thread 0x%ulx", 
		  current_thread);
         MACH_CHECK_ERROR (ret);
       }
      MACH_CHECK_ERROR (ret);
      arm_macosx_fetch_gp_registers (&gp_regs);
    }

  if ((regno == -1) || ARM_MACOSX_IS_FP_RELATED_REGNUM (regno))
    {
      /* We don't have F0-F7, though they need to exist in our register
         numbering scheme so we can connect to remote gdbserver's that use
	 FSF register numbers.  */
      for (i = ARM_F0_REGNUM; i <= ARM_F7_REGNUM; i++)
	set_register_cached (i, 1);
      set_register_cached (ARM_FPS_REGNUM, 1);
    }

  if (((regno == -1) || ARM_MACOSX_IS_VFP_RELATED_REGNUM (regno))
      && (gdbarch_osabi (current_gdbarch) == GDB_OSABI_DARWINV6))
    {
      struct gdb_arm_thread_fpstate fp_regs;
      unsigned int fp_count = GDB_ARM_THREAD_FPSTATE_COUNT;
      kern_return_t ret;
      ret = thread_get_state (current_thread, GDB_ARM_THREAD_FPSTATE,
                              (thread_state_t) & fp_regs,
                              &fp_count);

      if (ret != KERN_SUCCESS)
       {
         printf ("Error calling thread_get_state for VFP registers for thread 0x%ulx", 
		  current_thread);
         MACH_CHECK_ERROR (ret);
       }
      arm_macosx_fetch_vfp_registers (&fp_regs);
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

  if ((regno == -1) || ARM_MACOSX_IS_GP_RELATED_REGNUM (regno))
    {
      struct gdb_arm_thread_state gp_regs;
      kern_return_t ret;
      arm_macosx_store_gp_registers (&gp_regs);
      ret = thread_set_state (current_thread, GDB_ARM_THREAD_STATE,
                              (thread_state_t) & gp_regs,
                              GDB_ARM_THREAD_STATE_COUNT);
      MACH_CHECK_ERROR (ret);
    }
  if (((regno == -1) || ARM_MACOSX_IS_VFP_RELATED_REGNUM (regno))
      && (gdbarch_osabi (current_gdbarch) == GDB_OSABI_DARWINV6))
    {
      struct gdb_arm_thread_fpstate fp_regs;
      kern_return_t ret;
      arm_macosx_store_vfp_registers (&fp_regs);
      ret = thread_set_state (current_thread, GDB_ARM_THREAD_FPSTATE,
                              (thread_state_t) & fp_regs,
                              GDB_ARM_THREAD_FPSTATE_COUNT);
      MACH_CHECK_ERROR (ret);
    }
}

void
macosx_complete_child_target (struct target_ops *target)
{
}
