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

#include "i386-macosx-thread-status.h"

#include "i386-macosx-tdep.h"

#define FETCH_REG(rdata, rnum, sdata) \
store_unsigned_integer ((rdata) + (REGISTER_BYTE (rnum)), (REGISTER_RAW_SIZE (rnum)), (sdata))

#define STORE_REG(rdata, rnum, sdata) \
(sdata) = extract_unsigned_integer ((rdata) + (REGISTER_BYTE (rnum)), (REGISTER_RAW_SIZE (rnum)))

void i386_macosx_fetch_gp_registers (unsigned char *rdata, gdb_i386_thread_state_t *gp_regs)
{
}

void i386_macosx_store_gp_registers (unsigned char *rdata, gdb_i386_thread_state_t *gp_regs)
{
}

void i386_macosx_fetch_sp_registers (unsigned char *rdata, gdb_i386_thread_state_t *sp_regs)
{
  /* these need to match the REGISTER_NAMES table from tm-i386.h */
  FETCH_REG (rdata, 0, sp_regs->eax);
  FETCH_REG (rdata, 1, sp_regs->ecx);
  FETCH_REG (rdata, 2, sp_regs->edx);
  FETCH_REG (rdata, 3, sp_regs->ebx);
  FETCH_REG (rdata, 4, sp_regs->esp);
  FETCH_REG (rdata, 5, sp_regs->ebp);
  FETCH_REG (rdata, 6, sp_regs->esi);
  FETCH_REG (rdata, 7, sp_regs->edi);
  FETCH_REG (rdata, 8, sp_regs->eip);
  FETCH_REG (rdata, 9, sp_regs->eflags);
  FETCH_REG (rdata, 10, sp_regs->cs);
  FETCH_REG (rdata, 11, sp_regs->ss);
  FETCH_REG (rdata, 12, sp_regs->ds);
  FETCH_REG (rdata, 13, sp_regs->es);
  FETCH_REG (rdata, 14, sp_regs->fs);
  FETCH_REG (rdata, 15, sp_regs->gs);
}

void i386_macosx_store_sp_registers (unsigned char *rdata, gdb_i386_thread_state_t *sp_regs)
{
  /* these need to match the REGISTER_NAMES table from tm-i386.h */
  STORE_REG (rdata, 0, sp_regs->eax);
  STORE_REG (rdata, 1, sp_regs->ecx);
  STORE_REG (rdata, 2, sp_regs->edx);
  STORE_REG (rdata, 3, sp_regs->ebx);
  STORE_REG (rdata, 4, sp_regs->esp);
  STORE_REG (rdata, 5, sp_regs->ebp);
  STORE_REG (rdata, 6, sp_regs->esi);
  STORE_REG (rdata, 7, sp_regs->edi);
  STORE_REG (rdata, 8, sp_regs->eip);
  STORE_REG (rdata, 9, sp_regs->eflags);
  STORE_REG (rdata, 10, sp_regs->cs);
  STORE_REG (rdata, 11, sp_regs->ss);
  STORE_REG (rdata, 12, sp_regs->ds);
  STORE_REG (rdata, 13, sp_regs->es);
  STORE_REG (rdata, 14, sp_regs->fs);
  STORE_REG (rdata, 15, sp_regs->gs);
}

void i386_macosx_fetch_fp_registers (unsigned char *rdata, gdb_i386_thread_fpstate_t *fp_regs)
{
}

void i386_macosx_store_fp_registers (unsigned char *rdata, gdb_i386_thread_fpstate_t *fp_regs)
{
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

CORE_ADDR
sigtramp_saved_pc (frame)
     struct frame_info *frame;
{
  return 0;
}
