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
#include "gdbcore.h"
#include "target.h"
#include "floatformat.h"
#include "symtab.h"
#include "regcache.h"
#include "i387-tdep.h"
#include "i386-tdep.h"
#include "osabi.h"

#include "i386-macosx-thread-status.h"

#include <mach/thread_status.h>

#include "i386-macosx-tdep.h"

#define supply_unsigned_int(regnum, val)\
store_unsigned_integer (buf, 4, val); \
supply_register(regnum, buf);

#define collect_unsigned_int(regnum, addr)\
regcache_collect (regnum, buf); \
(* (addr)) = extract_unsigned_integer (buf, 4);

void i386_macosx_fetch_gp_registers (gdb_i386_thread_state_t *sp_regs)
{
  char buf[4];
  supply_unsigned_int (0, sp_regs->eax);
  supply_unsigned_int (1, sp_regs->ecx);
  supply_unsigned_int (2, sp_regs->edx);
  supply_unsigned_int (3, sp_regs->ebx);
  supply_unsigned_int (4, sp_regs->esp);
  supply_unsigned_int (5, sp_regs->ebp);
  supply_unsigned_int (6, sp_regs->esi);
  supply_unsigned_int (7, sp_regs->edi);
  supply_unsigned_int (8, sp_regs->eip);
  supply_unsigned_int (9, sp_regs->efl);
  supply_unsigned_int (10, sp_regs->cs);
  supply_unsigned_int (11, sp_regs->ss);
  supply_unsigned_int (12, sp_regs->ds);
  supply_unsigned_int (13, sp_regs->es);
  supply_unsigned_int (14, sp_regs->fs);
  supply_unsigned_int (15, sp_regs->gs);
}

void i386_macosx_store_gp_registers (gdb_i386_thread_state_t *sp_regs)
{
  unsigned char buf[4];
  collect_unsigned_int (0, &sp_regs->eax);
  collect_unsigned_int (1, &sp_regs->ecx);
  collect_unsigned_int (2, &sp_regs->edx);
  collect_unsigned_int (3, &sp_regs->ebx);
  collect_unsigned_int (4, &sp_regs->esp);
  collect_unsigned_int (5, &sp_regs->ebp);
  collect_unsigned_int (6, &sp_regs->esi);
  collect_unsigned_int (7, &sp_regs->edi);
  collect_unsigned_int (8, &sp_regs->eip);
  collect_unsigned_int (9, &sp_regs->efl);
  collect_unsigned_int (10, &sp_regs->cs);
  collect_unsigned_int (11, &sp_regs->ss);
  collect_unsigned_int (12, &sp_regs->ds);
  collect_unsigned_int (13, &sp_regs->es);
  collect_unsigned_int (14, &sp_regs->fs);
  collect_unsigned_int (15, &sp_regs->gs);
}

void i386_macosx_fetch_fp_registers (gdb_i386_thread_fpstate_t *fp_regs)
{
  if ((fp_regs->fpkind == GDB_i386_FP_387) && (fp_regs->initialized))
    i387_supply_fsave ((unsigned char *) &fp_regs->hw_state);
  else if ((fp_regs->fpkind == GDB_i386_FP_SSE2) && (fp_regs->initialized))
    i387_supply_fxsave ((unsigned char *) &fp_regs->hw_state);
  else
    i387_supply_fxsave (NULL);
}

void i386_macosx_store_fp_registers (gdb_i386_thread_fpstate_t *fp_regs)
{
#if 0
  fp_regs->fpkind = GDB_i386_FP_SSE2;
  fp_regs->initialized = 1;
  i387_fill_fxsave ((unsigned char *) &fp_regs->hw_state, -1);
  fp_regs->exc_status = 0;
#else
  fp_regs->fpkind = GDB_i386_FP_387;
  fp_regs->initialized = 1;
  i387_fill_fsave ((unsigned char *) &fp_regs->hw_state, -1);
  fp_regs->exc_status = 0;
#endif
}

/* mread -- read memory (unsigned) and apply a bitmask */

static unsigned long mread (addr, len, mask)
     CORE_ADDR addr;
     unsigned long len, mask;
{
  long ret = read_memory_unsigned_integer (addr, len);
  if (mask) { ret &= mask; }
  return ret;
}

CORE_ADDR
i386_macosx_skip_trampoline_code (pc)
     CORE_ADDR pc;
{
  unsigned char opcode1 = (unsigned char) mread (pc, 1, 0);
  CORE_ADDR new_pc = pc;

  /* first test:  static shlib jumptable */
  if (opcode1 == 0xe9)				/* jmpl xxxx */
    return (pc + mread (pc+1, 4, 0) + 5);

  /* second test: dynamic shlib jumptable (1st entry point) */
  if (opcode1             == 0xe8   &&
      mread (pc+1,  4, 0) == 0      &&		/* calll pc+5 */
      mread (pc+5,  1, 0) == 0x58   &&		/* popl  %eax */
      mread (pc+6,  2, 0) == 0x908b &&		/* movl y(%eax),%edx */
      mread (pc+12, 2, 0) == 0xe2ff)		/* jmpl  %edx */
    pc = new_pc = mread (pc + mread (pc+8, 4, 0) + 5, 4, 0);
  /* and fall thru to do next test (both might succeed) */

#if 0
  /* third test: dynamic shlib table (as yet unresolved entry) */
  if (mread (pc,   2, 0) == 0x808d &&		/* leal y(%eax),%eax */
      mread (pc+6, 1, 0) == 0x50   &&		/* pushl %eax */
      mread (pc+7, 1, 0) == 0xe9)		/* jmpl dyld */
    pc = new_pc = (CORE_ADDR) get_symbol_stub_real_address (pc, NULL);
#endif

  return new_pc;
}

int i386_macosx_in_solib_return_trampoline (CORE_ADDR pc, char *name)
{
  return 0;
}

int i386_macosx_in_solib_call_trampoline (CORE_ADDR pc, char *name)
{
  if (i386_macosx_skip_trampoline_code (pc) != pc) { return 1; }
  return 0;
}

static CORE_ADDR
i386_macosx_sigcontext_addr (struct frame_info *frame)
{
  int sigcontext_offset = 24;

  return read_memory_unsigned_integer (frame->frame + sigcontext_offset, 4);
}

static void
i386_macosx_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* We support the SSE registers.  */
  tdep->num_xmm_regs = I386_NUM_XREGS - 1;
  set_gdbarch_num_regs (gdbarch, I386_SSE_NUM_REGS);

  tdep->struct_return = reg_struct_return;

  tdep->sigcontext_addr = i386_macosx_sigcontext_addr;
  tdep->sc_pc_offset = 12 * 4;
  tdep->sc_sp_offset = 9 * 4;

  tdep->jb_pc_offset = 20;
}

static enum gdb_osabi
i386_mach_o_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "mach-o-be") == 0
      || strcmp (bfd_get_target (abfd), "mach-o-le") == 0
      || strcmp (bfd_get_target (abfd), "mach-o-fat") == 0)
    return GDB_OSABI_DARWIN;

  return GDB_OSABI_UNKNOWN;
}

void
_initialize_i386_macosx_tdep (void)
{
  gdbarch_register_osabi_sniffer (bfd_arch_i386, bfd_target_mach_o_flavour,
				  i386_mach_o_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_i386, 0, GDB_OSABI_DARWIN,
			  i386_macosx_init_abi);
}
