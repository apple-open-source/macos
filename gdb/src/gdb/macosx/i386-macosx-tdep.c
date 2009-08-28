/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002, 2005
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
#include "libbfd.h"
#include "objfiles.h"

#include "i387-tdep.h"
#include "i386-tdep.h"
#include "amd64-tdep.h"
#include "osabi.h"
#include "ui-out.h"
#include "symtab.h"
#include "frame.h"
#include "gdb_assert.h"

#include <mach/thread_status.h>
#include <sys/sysctl.h>

#include "i386-macosx-tdep.h"

#define supply_unsigned_int(regnum, val)\
store_unsigned_integer (buf, 4, val); \
regcache_raw_supply (current_regcache, regnum, buf);

#define collect_unsigned_int(regnum, addr)\
regcache_raw_collect (current_regcache, regnum, buf); \
(* (addr)) = extract_unsigned_integer (buf, 4);

#define supply_unsigned_int64(regnum, val)\
store_unsigned_integer (buf, 8, val); \
regcache_raw_supply (current_regcache, regnum, buf);

#define collect_unsigned_int64(regnum, addr)\
regcache_raw_collect (current_regcache, regnum, buf); \
(* (addr)) = extract_unsigned_integer (buf, 8);

static int x86_64_macosx_get_longjmp_target (CORE_ADDR *pc);
static int i386_macosx_get_longjmp_target (CORE_ADDR *pc);

void
i386_macosx_fetch_gp_registers (gdb_i386_thread_state_t *sp_regs)
{
  gdb_byte buf[4];
  supply_unsigned_int (0, sp_regs->eax);
  supply_unsigned_int (1, sp_regs->ecx);
  supply_unsigned_int (2, sp_regs->edx);
  supply_unsigned_int (3, sp_regs->ebx);
  supply_unsigned_int (4, sp_regs->esp);
  supply_unsigned_int (5, sp_regs->ebp);
  supply_unsigned_int (6, sp_regs->esi);
  supply_unsigned_int (7, sp_regs->edi);
  supply_unsigned_int (8, sp_regs->eip);
  supply_unsigned_int (9, sp_regs->eflags);
  supply_unsigned_int (10, sp_regs->cs);
  supply_unsigned_int (11, sp_regs->ss);
  supply_unsigned_int (12, sp_regs->ds);
  supply_unsigned_int (13, sp_regs->es);
  supply_unsigned_int (14, sp_regs->fs);
  supply_unsigned_int (15, sp_regs->gs);
}

void
i386_macosx_fetch_gp_registers_raw (gdb_i386_thread_state_t *sp_regs)
{
  regcache_raw_supply (current_regcache, 0, &sp_regs->eax);
  regcache_raw_supply (current_regcache, 1, &sp_regs->ecx);
  regcache_raw_supply (current_regcache, 2, &sp_regs->edx);
  regcache_raw_supply (current_regcache, 3, &sp_regs->ebx);
  regcache_raw_supply (current_regcache, 4, &sp_regs->esp);
  regcache_raw_supply (current_regcache, 5, &sp_regs->ebp);
  regcache_raw_supply (current_regcache, 6, &sp_regs->esi);
  regcache_raw_supply (current_regcache, 7, &sp_regs->edi);
  regcache_raw_supply (current_regcache, 8, &sp_regs->eip);
  regcache_raw_supply (current_regcache, 9, &sp_regs->eflags);
  regcache_raw_supply (current_regcache, 10, &sp_regs->cs);
  regcache_raw_supply (current_regcache, 11, &sp_regs->ss);
  regcache_raw_supply (current_regcache, 12, &sp_regs->ds);
  regcache_raw_supply (current_regcache, 13, &sp_regs->es);
  regcache_raw_supply (current_regcache, 14, &sp_regs->fs);
  regcache_raw_supply (current_regcache, 15, &sp_regs->gs);
}

void
i386_macosx_store_gp_registers (gdb_i386_thread_state_t *sp_regs)
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
  collect_unsigned_int (9, &sp_regs->eflags);
  collect_unsigned_int (10, &sp_regs->cs);
  collect_unsigned_int (11, &sp_regs->ss);
  collect_unsigned_int (12, &sp_regs->ds);
  collect_unsigned_int (13, &sp_regs->es);
  collect_unsigned_int (14, &sp_regs->fs);
  collect_unsigned_int (15, &sp_regs->gs);
}

void
i386_macosx_store_gp_registers_raw (gdb_i386_thread_state_t *sp_regs)
{
  regcache_raw_collect (current_regcache, 0, &sp_regs->eax);
  regcache_raw_collect (current_regcache, 1, &sp_regs->ecx);
  regcache_raw_collect (current_regcache, 2, &sp_regs->edx);
  regcache_raw_collect (current_regcache, 3, &sp_regs->ebx);
  regcache_raw_collect (current_regcache, 4, &sp_regs->esp);
  regcache_raw_collect (current_regcache, 5, &sp_regs->ebp);
  regcache_raw_collect (current_regcache, 6, &sp_regs->esi);
  regcache_raw_collect (current_regcache, 7, &sp_regs->edi);
  regcache_raw_collect (current_regcache, 8, &sp_regs->eip);
  regcache_raw_collect (current_regcache, 9, &sp_regs->eflags);
  regcache_raw_collect (current_regcache, 10, &sp_regs->cs);
  regcache_raw_collect (current_regcache, 11, &sp_regs->ss);
  regcache_raw_collect (current_regcache, 12, &sp_regs->ds);
  regcache_raw_collect (current_regcache, 13, &sp_regs->es);
  regcache_raw_collect (current_regcache, 14, &sp_regs->fs);
  regcache_raw_collect (current_regcache, 15, &sp_regs->gs);
}

void
x86_64_macosx_fetch_gp_registers (gdb_x86_thread_state64_t *sp_regs)
{
  unsigned char buf[8];
  supply_unsigned_int64 (AMD64_RAX_REGNUM, sp_regs->rax);
  supply_unsigned_int64 (AMD64_RBX_REGNUM, sp_regs->rbx);
  supply_unsigned_int64 (AMD64_RCX_REGNUM, sp_regs->rcx);
  supply_unsigned_int64 (AMD64_RDX_REGNUM, sp_regs->rdx);
  supply_unsigned_int64 (AMD64_RDI_REGNUM, sp_regs->rdi);
  supply_unsigned_int64 (AMD64_RSI_REGNUM, sp_regs->rsi);
  supply_unsigned_int64 (AMD64_RBP_REGNUM, sp_regs->rbp);
  supply_unsigned_int64 (AMD64_RSP_REGNUM, sp_regs->rsp);
  supply_unsigned_int64 (AMD64_R8_REGNUM, sp_regs->r8);
  supply_unsigned_int64 (AMD64_R8_REGNUM + 1, sp_regs->r9);
  supply_unsigned_int64 (AMD64_R8_REGNUM + 2, sp_regs->r10);
  supply_unsigned_int64 (AMD64_R8_REGNUM + 3, sp_regs->r11);
  supply_unsigned_int64 (AMD64_R8_REGNUM + 4, sp_regs->r12);
  supply_unsigned_int64 (AMD64_R8_REGNUM + 5, sp_regs->r13);
  supply_unsigned_int64 (AMD64_R8_REGNUM + 6, sp_regs->r14);
  supply_unsigned_int64 (AMD64_R8_REGNUM + 7, sp_regs->r15);
  supply_unsigned_int64 (AMD64_RIP_REGNUM, sp_regs->rip);
  supply_unsigned_int64 (AMD64_EFLAGS_REGNUM, sp_regs->rflags);
  supply_unsigned_int64 (AMD64_CS_REGNUM, sp_regs->cs);
  supply_unsigned_int64 (AMD64_FS_REGNUM, sp_regs->fs);
  supply_unsigned_int64 (AMD64_GS_REGNUM, sp_regs->gs);
}

void
x86_64_macosx_fetch_gp_registers_raw (gdb_x86_thread_state64_t *sp_regs)
{
  regcache_raw_supply (current_regcache, AMD64_RAX_REGNUM, &sp_regs->rax);
  regcache_raw_supply (current_regcache, AMD64_RBX_REGNUM, &sp_regs->rbx);
  regcache_raw_supply (current_regcache, AMD64_RCX_REGNUM, &sp_regs->rcx);
  regcache_raw_supply (current_regcache, AMD64_RDX_REGNUM, &sp_regs->rdx);
  regcache_raw_supply (current_regcache, AMD64_RDI_REGNUM, &sp_regs->rdi);
  regcache_raw_supply (current_regcache, AMD64_RSI_REGNUM, &sp_regs->rsi);
  regcache_raw_supply (current_regcache, AMD64_RBP_REGNUM, &sp_regs->rbp);
  regcache_raw_supply (current_regcache, AMD64_RSP_REGNUM, &sp_regs->rsp);
  regcache_raw_supply (current_regcache, AMD64_R8_REGNUM, &sp_regs->r8);
  regcache_raw_supply (current_regcache, AMD64_R8_REGNUM + 1, &sp_regs->r9);
  regcache_raw_supply (current_regcache, AMD64_R8_REGNUM + 2, &sp_regs->r10);
  regcache_raw_supply (current_regcache, AMD64_R8_REGNUM + 3, &sp_regs->r11);
  regcache_raw_supply (current_regcache, AMD64_R8_REGNUM + 4, &sp_regs->r12);
  regcache_raw_supply (current_regcache, AMD64_R8_REGNUM + 5, &sp_regs->r13);
  regcache_raw_supply (current_regcache, AMD64_R8_REGNUM + 6, &sp_regs->r14);
  regcache_raw_supply (current_regcache, AMD64_R8_REGNUM + 7, &sp_regs->r15);
  regcache_raw_supply (current_regcache, AMD64_RIP_REGNUM, &sp_regs->rip);
  regcache_raw_supply (current_regcache, AMD64_EFLAGS_REGNUM, &sp_regs->rflags);
  regcache_raw_supply (current_regcache, AMD64_CS_REGNUM, &sp_regs->cs);
  regcache_raw_supply (current_regcache, AMD64_FS_REGNUM, &sp_regs->fs);
  regcache_raw_supply (current_regcache, AMD64_GS_REGNUM, &sp_regs->gs);
}

void
x86_64_macosx_store_gp_registers (gdb_x86_thread_state64_t *sp_regs)
{
  unsigned char buf[8];
  collect_unsigned_int64 (AMD64_RAX_REGNUM, &sp_regs->rax);
  collect_unsigned_int64 (AMD64_RBX_REGNUM, &sp_regs->rbx);
  collect_unsigned_int64 (AMD64_RCX_REGNUM, &sp_regs->rcx);
  collect_unsigned_int64 (AMD64_RDX_REGNUM, &sp_regs->rdx);
  collect_unsigned_int64 (AMD64_RDI_REGNUM, &sp_regs->rdi);
  collect_unsigned_int64 (AMD64_RSI_REGNUM, &sp_regs->rsi);
  collect_unsigned_int64 (AMD64_RBP_REGNUM, &sp_regs->rbp);
  collect_unsigned_int64 (AMD64_RSP_REGNUM, &sp_regs->rsp);
  collect_unsigned_int64 (AMD64_R8_REGNUM, &sp_regs->r8);
  collect_unsigned_int64 (AMD64_R8_REGNUM + 1, &sp_regs->r9);
  collect_unsigned_int64 (AMD64_R8_REGNUM + 2, &sp_regs->r10);
  collect_unsigned_int64 (AMD64_R8_REGNUM + 3, &sp_regs->r11);
  collect_unsigned_int64 (AMD64_R8_REGNUM + 4, &sp_regs->r12);
  collect_unsigned_int64 (AMD64_R8_REGNUM + 5, &sp_regs->r13);
  collect_unsigned_int64 (AMD64_R8_REGNUM + 6, &sp_regs->r14);
  collect_unsigned_int64 (AMD64_R8_REGNUM + 7, &sp_regs->r15);
  collect_unsigned_int64 (AMD64_RIP_REGNUM, &sp_regs->rip);
  collect_unsigned_int64 (AMD64_EFLAGS_REGNUM, &sp_regs->rflags);
  collect_unsigned_int64 (AMD64_CS_REGNUM, &sp_regs->cs);
  collect_unsigned_int64 (AMD64_FS_REGNUM, &sp_regs->fs);
  collect_unsigned_int64 (AMD64_GS_REGNUM, &sp_regs->gs);
}

void
x86_64_macosx_store_gp_registers_raw (gdb_x86_thread_state64_t *sp_regs)
{
  regcache_raw_collect (current_regcache, AMD64_RAX_REGNUM, &sp_regs->rax);
  regcache_raw_collect (current_regcache, AMD64_RBX_REGNUM, &sp_regs->rbx);
  regcache_raw_collect (current_regcache, AMD64_RCX_REGNUM, &sp_regs->rcx);
  regcache_raw_collect (current_regcache, AMD64_RDX_REGNUM, &sp_regs->rdx);
  regcache_raw_collect (current_regcache, AMD64_RDI_REGNUM, &sp_regs->rdi);
  regcache_raw_collect (current_regcache, AMD64_RSI_REGNUM, &sp_regs->rsi);
  regcache_raw_collect (current_regcache, AMD64_RBP_REGNUM, &sp_regs->rbp);
  regcache_raw_collect (current_regcache, AMD64_RSP_REGNUM, &sp_regs->rsp);
  regcache_raw_collect (current_regcache, AMD64_R8_REGNUM, &sp_regs->r8);
  regcache_raw_collect (current_regcache, AMD64_R8_REGNUM + 1, &sp_regs->r9);
  regcache_raw_collect (current_regcache, AMD64_R8_REGNUM + 2, &sp_regs->r10);
  regcache_raw_collect (current_regcache, AMD64_R8_REGNUM + 3, &sp_regs->r11);
  regcache_raw_collect (current_regcache, AMD64_R8_REGNUM + 4, &sp_regs->r12);
  regcache_raw_collect (current_regcache, AMD64_R8_REGNUM + 5, &sp_regs->r13);
  regcache_raw_collect (current_regcache, AMD64_R8_REGNUM + 6, &sp_regs->r14);
  regcache_raw_collect (current_regcache, AMD64_R8_REGNUM + 7, &sp_regs->r15);
  regcache_raw_collect (current_regcache, AMD64_RIP_REGNUM, &sp_regs->rip);
  regcache_raw_collect (current_regcache, AMD64_EFLAGS_REGNUM, &sp_regs->rflags);
  regcache_raw_collect (current_regcache, AMD64_CS_REGNUM, &sp_regs->cs);
  regcache_raw_collect (current_regcache, AMD64_FS_REGNUM, &sp_regs->fs);
  regcache_raw_collect (current_regcache, AMD64_GS_REGNUM, &sp_regs->gs);
}

/* Fetching the the registers from the inferior into our reg cache.
   FP_REGS is a structure that mirrors the Mach structure
   struct i386_float_state.  The "fpu_fcw" field inside that
   structure is the start of a block which is identical
   to the FXSAVE/FXRSTOR instructions' format.  */

void
i386_macosx_fetch_fp_registers (gdb_i386_float_state_t *fp_regs)
{
  i387_swap_fxsave (current_regcache, (uint8_t *) &fp_regs->fpu_fcw);
  i387_supply_fxsave (current_regcache, -1, &fp_regs->fpu_fcw);
}

void
i386_macosx_fetch_fp_registers_raw (gdb_i386_float_state_t *fp_regs)
{
  i387_supply_fxsave (current_regcache, -1, &fp_regs->fpu_fcw);
}

void
x86_64_macosx_fetch_fp_registers (gdb_x86_float_state64_t *fp_regs)
{
  i387_swap_fxsave (current_regcache, (uint8_t *) &fp_regs->fpu_fcw);
  i387_supply_fxsave (current_regcache, -1, &fp_regs->fpu_fcw);
}

void
x86_64_macosx_fetch_fp_registers_raw (gdb_x86_float_state64_t *fp_regs)
{
  i387_supply_fxsave (current_regcache, -1, &fp_regs->fpu_fcw);
}

/* Get the floating point registers from our local register cache
   and stick them in FP_REGS in for sending to the inferior via a
   syscall.  If the local register cache has valid FP values, this
   function returns 1.  If the local register cache does not have
   valid FP values -- and so FP_REGS should not be pushed into the
   inferior -- this function returns 0.  */

int
i386_macosx_store_fp_registers (gdb_i386_float_state_t *fp_regs)
{
  memset (fp_regs, 0, sizeof (gdb_i386_float_state_t));
  i387_fill_fxsave ((unsigned char *) &fp_regs->fpu_fcw, -1);
  i387_swap_fxsave (current_regcache, (uint8_t *) &fp_regs->fpu_fcw);

  return 1;
}

int
i386_macosx_store_fp_registers_raw (gdb_i386_float_state_t *fp_regs)
{
  memset (fp_regs, 0, sizeof (gdb_i386_float_state_t));
  i387_fill_fxsave ((unsigned char *) &fp_regs->fpu_fcw, -1);

  return 1;
}


int
x86_64_macosx_store_fp_registers (gdb_x86_float_state64_t *fp_regs)
{
  memset (fp_regs, 0, sizeof (gdb_x86_float_state64_t));
  i387_fill_fxsave ((unsigned char *) &fp_regs->fpu_fcw, -1);
  i387_swap_fxsave (current_regcache, (uint8_t *) &fp_regs->fpu_fcw);

  return 1;
}

int
x86_64_macosx_store_fp_registers_raw (gdb_x86_float_state64_t *fp_regs)
{
  memset (fp_regs, 0, sizeof (gdb_x86_float_state64_t));
  i387_fill_fxsave ((unsigned char *) &fp_regs->fpu_fcw, -1);

  return 1;
}

static CORE_ADDR
i386_macosx_thread_state_addr_1 (CORE_ADDR start_of_func, CORE_ADDR pc,
                                 CORE_ADDR ebp, CORE_ADDR esp);

/* On entry to _sigtramp, the ESP points to the start of a
   'struct sigframe' (cf xnu's bsd/dev/i386/unix_signal.c).
   The 6th word in the struct sigframe is a 'struct ucontext *'.

   struct ucontext (cf sys/_types.h)'s 7th word is a 
   __darwin_size_t uc_mcsize to indicate the size of the
   saved machine context, and the 8th word is a struct mcontext *
   pointing to the saved context structure.

   struct mcontext (cf i386/ucontext.h) has three structures in it,
     i386_exception_state_t
     i386_thread_state_t
     i386_float_state_t
   this function has to return the address of the struct i386_thread_state
   (defined in <mach/i386/thread_status.h>) inside the struct mcontext.  */

static CORE_ADDR
i386_macosx_thread_state_addr (struct frame_info *frame)
{
  gdb_byte buf[4];
  CORE_ADDR esp, ebp;
  frame_unwind_register (frame, I386_ESP_REGNUM, buf);
  esp = extract_unsigned_integer (buf, 4);
  frame_unwind_register (frame, I386_EBP_REGNUM, buf);
  ebp = extract_unsigned_integer (buf, 4);
  return i386_macosx_thread_state_addr_1 (get_frame_func (frame), 
                                          get_frame_pc (frame), ebp, esp);
}

static CORE_ADDR
i386_macosx_thread_state_addr_1 (CORE_ADDR start_of_func, CORE_ADDR pc,
                                 CORE_ADDR ebp, CORE_ADDR esp)
{
  int offset = 0;
  CORE_ADDR push_ebp_addr = 0;
  CORE_ADDR mov_esp_ebp_addr = 0;
  CORE_ADDR address_of_struct_sigframe;
  CORE_ADDR address_of_struct_ucontext;
  CORE_ADDR address_of_struct_mcontext;
  int limit;

  /* We begin our function with a fun little hand-rolled prologue parser.
     These sorts of things NEVER come back to bite us years down the road,
     no sir-ee bob.  The only saving grace is that _sigtramp() is a tough
     function to screw up as it stands today.  Oh, and if we get this wrong,
     signal backtraces should break outright and we get nice little testsuite 
     failures. */

  limit = min (pc - start_of_func + 1, 16);
  while (offset < limit)
    {
      if (!push_ebp_addr)
        {
          /* push   %ebp   [ 0x55 ] */
          if (read_memory_unsigned_integer (start_of_func + offset, 1) == 0x55)
            {
              push_ebp_addr = start_of_func + offset;
              offset++;
            }
          else
            {
              /* If this isn't the push %ebp, and we haven't seen push %ebp yet,
                 skip whatever insn we're sitting on and keep looking for 
                 push %ebp.  It must occur before mov %esp, %ebp.  */
              offset++;
              continue;
            }
        }

      /* We've already seen push %ebp */
      /* Look for mov %esp, %ebp  [ 0x89 0xe5 || 0x8b 0xec ] */
      if (read_memory_unsigned_integer (start_of_func + offset, 2) == 0xe589
          || read_memory_unsigned_integer (start_of_func + offset, 2) == 0xec8b)
        {
          mov_esp_ebp_addr = start_of_func + offset;
          break;
        }
      offset++;  /* I'm single byte stepping through unknown instructions.  
                    SURELY this won't cause an improper match, cough cough. */
    }
  if (!push_ebp_addr || !mov_esp_ebp_addr)
    error ("Unable to analyze the prologue of _sigtramp(), giving up.");

  if (pc <= push_ebp_addr)
    address_of_struct_sigframe = esp + 0;

  if (pc > push_ebp_addr && pc <= mov_esp_ebp_addr)
    address_of_struct_sigframe = esp + 4;

  if (pc > mov_esp_ebp_addr)
    address_of_struct_sigframe = ebp + 4;

  address_of_struct_ucontext = read_memory_unsigned_integer 
                                (address_of_struct_sigframe + 20, 4);

  /* the element 'uc_mcontext' -- the pointer to the struct sigcontext -- 
     is 28 bytes into the 'struct ucontext' */
  address_of_struct_mcontext = read_memory_unsigned_integer 
                                (address_of_struct_ucontext + 28, 4); 

  return address_of_struct_mcontext + 12;
}

/* On entry to _sigtramp, r8 has the address of a ucontext_t structure.
   48 bytes into the ucontext_t we have the address of an mcontext structure.
   Starting 16 bytes into the mcontext structure, we have the saved registers,
   the mapping of them is handled by amd64_macosx_thread_state_reg_offset. 
   libSystem has eh_frame instructions for doing the same thing; they are of
   the form,
         DW_CFA_expression (1, expr(breg3 +48, deref , plus uconst 0x0018))
   On function entry we can find the ucontext_t structure off of R8; when 
   _sigtramp calls down into the handler we should look at RBX (reg3) to
   find it.
 */

static CORE_ADDR
amd64_macosx_thread_state_addr (struct frame_info *next_frame)
{
  gdb_byte buf[8];
  CORE_ADDR mcontext_addr, ucontext_addr;

  if (frame_relative_level (next_frame) == -1)
    {
      frame_unwind_register (next_frame, AMD64_R8_REGNUM, buf);
      mcontext_addr = extract_unsigned_integer (buf, 8);
    }
  else
    {
      frame_unwind_register (next_frame, AMD64_RBX_REGNUM, buf);
      mcontext_addr = extract_unsigned_integer (buf, 8);
    }

  ucontext_addr = read_memory_unsigned_integer (mcontext_addr + 48, 8);
  return ucontext_addr + 0x10;
}

/* Offsets into the struct i386_thread_state where we'll find the saved regs. */
/* From <mach/i386/thread_status.h and i386-tdep.h */
static int i386_macosx_thread_state_reg_offset[] =
{
   0 * 4,   /* EAX */
   2 * 4,   /* ECX */
   3 * 4,   /* EDX */
   1 * 4,   /* EBX */
   7 * 4,   /* ESP */
   6 * 4,   /* EBP */
   5 * 4,   /* ESI */
   4 * 4,   /* EDI */
  10 * 4,   /* EIP */
   9 * 4,   /* EFLAGS */
  11 * 4,   /* CS */
   8,       /* SS */
  12 * 4,   /* DS */
  13 * 4,   /* ES */
  14 * 4,   /* FS */
  15 * 4    /* GS */
};

/* Offsets into the struct x86_thread_state64 where we'll find the saved regs. */
/* From <mach/i386/thread_status.h and amd64-tdep.h */
static int amd64_macosx_thread_state_reg_offset[] =
{
  0 * 8,			/* %rax */
  1 * 8,			/* %rbx */
  2 * 8,			/* %rcx */
  3 * 8,			/* %rdx */
  5 * 8,			/* %rsi */
  4 * 8,			/* %rdi */
  6 * 8,			/* %rbp */
  7 * 8,			/* %rsp */
  8 * 8,			/* %r8 ... */
  9 * 8,
  10 * 8,
  11 * 8,
  12 * 8,
  13 * 8,
  14 * 8,
  15 * 8,			/* ... %r15 */
  16 * 8,			/* %rip */
  17 * 8,			/* %rflags */
  18 * 8,			/* %cs */
  -1,				/* %ss */
  -1,				/* %ds */
  -1,				/* %es */
  19 * 8,			/* %fs */
  20 * 8			/* %gs */
};


static CORE_ADDR
i386_integer_to_address (struct gdbarch *gdbarch, struct type *type, 
                         const gdb_byte *buf)
{
  gdb_byte *tmp = alloca (TYPE_LENGTH (builtin_type_void_data_ptr));
  LONGEST val = unpack_long (type, buf);
  store_unsigned_integer (tmp, TYPE_LENGTH (builtin_type_void_data_ptr), val);
  return extract_unsigned_integer (tmp,
                                   TYPE_LENGTH (builtin_type_void_data_ptr));
}


/* Align to 16 byte boundary */
static CORE_ADDR
i386_macosx_frame_align (struct gdbarch *gdbarch, CORE_ADDR addr)
{
   return (addr & -16);
}

static void
i386_macosx_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* We support the SSE registers.  */
  tdep->num_xmm_regs = I386_NUM_XREGS - 1;
  set_gdbarch_num_regs (gdbarch, I386_SSE_NUM_REGS);

  set_gdbarch_skip_trampoline_code (gdbarch, macosx_skip_trampoline_code);
  set_gdbarch_get_longjmp_target (gdbarch, i386_macosx_get_longjmp_target);

  set_gdbarch_in_solib_return_trampoline (gdbarch,
                                          macosx_in_solib_return_trampoline);

  tdep->struct_return = reg_struct_return;

  tdep->sigcontext_addr = i386_macosx_thread_state_addr;
  tdep->sc_reg_offset = i386_macosx_thread_state_reg_offset;
  tdep->sc_num_regs = 16;

  tdep->jb_pc_offset = 20;
  set_gdbarch_integer_to_address (gdbarch, i386_integer_to_address);
  set_gdbarch_frame_align (gdbarch, i386_macosx_frame_align);
}

static void
x86_macosx_init_abi_64 (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->wordsize = 8;

  amd64_init_abi (info, gdbarch);

  set_gdbarch_skip_trampoline_code (gdbarch, macosx_skip_trampoline_code);
  set_gdbarch_get_longjmp_target (gdbarch, x86_64_macosx_get_longjmp_target);

  set_gdbarch_in_solib_return_trampoline (gdbarch,
                                          macosx_in_solib_return_trampoline);

  tdep->struct_return = reg_struct_return;

  tdep->sigcontext_addr = amd64_macosx_thread_state_addr;
  tdep->sc_reg_offset = amd64_macosx_thread_state_reg_offset;
  tdep->sc_num_regs = ARRAY_SIZE (amd64_macosx_thread_state_reg_offset);

  tdep->jb_pc_offset = 148;
  set_gdbarch_integer_to_address (gdbarch, i386_integer_to_address);
}

static int
i386_mach_o_query_64bit ()
{
  int result;
  int supports64bit;
  size_t sz;
  
  sz = sizeof (supports64bit);
  result = sysctlbyname ("hw.optional.x86_64", &supports64bit, &sz, NULL, 0);
  return (result == 0
          && sz == sizeof (supports64bit)
          && supports64bit);
}

static enum gdb_osabi
i386_mach_o_osabi_sniffer (bfd *abfd)
{
  return generic_mach_o_osabi_sniffer (abfd, bfd_arch_i386,
				       bfd_mach_i386_i386, bfd_mach_x86_64,
				       i386_mach_o_query_64bit);
}

/*
 * This is set to the FAST_COUNT_STACK macro for i386.  The return value
 * is 1 if no errors were encountered traversing the stack, and 0 otherwise.
 * It sets COUNT to the stack depth.  If PRINT_FUN is non-null, then 
 * it will be passed the pc & fp for each frame as it is encountered.
 */

/*
 * COUNT_LIMIT parameter sets a limit on the number of frames that
 * will be counted by this function.  -1 means unlimited.
 *
 * PRINT_LIMIT parameter sets a limit on the number of frames for
 * which the full information is printed.  -1 means unlimited.
 *
 */

int
i386_fast_show_stack (unsigned int count_limit, unsigned int print_limit,
                     unsigned int *count,
                     void (print_fun) (struct ui_out * uiout, int frame_num,
                                       CORE_ADDR pc, CORE_ADDR fp))
{
  CORE_ADDR fp, prev_fp;
  static CORE_ADDR sigtramp_start = 0;
  static CORE_ADDR sigtramp_end = 0;
  unsigned int i = 0;
  int more_frames;
  int err = 0;
  struct frame_info *fi;
  ULONGEST next_fp = 0;
  ULONGEST pc = 0;
  int wordsize = gdbarch_tdep (current_gdbarch)->wordsize;

  more_frames = fast_show_stack_trace_prologue (count_limit, print_limit, wordsize,
						&sigtramp_start, &sigtramp_end, 
						&i, &fi, print_fun);

  if (more_frames < 0)
    {
      err = 1;
      goto i386_count_finish;
    }

  if (i >= count_limit || !more_frames)
    goto i386_count_finish;

  /* gdb's idea of a stack frame is 8 bytes off from the actual
     values of EBP (gdb probably includes the saved esp and saved
     eip as part of the frame).  So pull off 8 bytes from the 
     "fp" to get an actual EBP value for walking the stack.  */

  fp = get_frame_base (fi);
  prev_fp = fp;             /* Start with a reasonable default value.  */
  fp = fp - 2 * wordsize;
  prev_fp = prev_fp - 2 * wordsize;
  while (1)
    {
      if ((sigtramp_start <= pc) && (pc <= sigtramp_end))
        {
          CORE_ADDR thread_state_at = 
                    i386_macosx_thread_state_addr_1 (sigtramp_start, pc, 
                                                     fp, prev_fp + 2 * wordsize);
          prev_fp = fp;
          if (!safe_read_memory_unsigned_integer (thread_state_at + 
                          i386_macosx_thread_state_reg_offset[I386_EBP_REGNUM], 
                           wordsize, &fp))
            goto i386_count_finish;
          if (!safe_read_memory_unsigned_integer (thread_state_at + 
                          i386_macosx_thread_state_reg_offset[I386_EIP_REGNUM], 
                           wordsize, &pc))
            goto i386_count_finish;
        }
      else
        {
          if (!safe_read_memory_unsigned_integer (fp, wordsize, &next_fp))
            goto i386_count_finish;
          if (next_fp == 0)
            goto i386_count_finish;
	  else if (fp == next_fp)
	    {
	      /* This shouldn't ever happen, but if it does we will
		 loop forever here, so protect against that.  */
	      warning ("Frame pointer point back at the previous frame");
	      err = 1;
	      goto i386_count_finish;
	    }
          if (!safe_read_memory_unsigned_integer (fp + wordsize, wordsize, &pc))
            goto i386_count_finish;
          if (pc == 0x0)
            goto i386_count_finish;
          prev_fp = fp;
          fp = next_fp;
        }

      /* Add 8 to the EBP to show the frame pointer as gdb likes
         to show it.  */

      /* Let's raise the load level here.  That will mean that if we are 
	 going to print the names, they will be accurate.  Also, it means
	 if the main executable has it's load-state lowered, we'll detect
	 main correctly.  */
      
      pc_set_load_state (pc, OBJF_SYM_ALL, 0);

      if (print_fun && (i < print_limit))
        print_fun (uiout, i, pc, fp + 2 * wordsize);
      i++;

      if (!backtrace_past_main && addr_inside_main_func (pc))
        goto i386_count_finish;

      if (i >= count_limit)
        goto i386_count_finish;
    }

i386_count_finish:
  if (print_fun)
    ui_out_end (uiout, ui_out_type_list);

  *count = i;
  return (!err);
}

static int
i386_macosx_get_longjmp_target_helper (int offset, CORE_ADDR *pc)
{
  CORE_ADDR jmp_buf;
  ULONGEST long_addr = 0;

  /* The first argument to longjmp is the pointer to the jump buf.
     The saved eip/rip there is offset by OFFSET as given above.  */

  jmp_buf = FETCH_POINTER_ARGUMENT (get_current_frame (), 0, 
                                    builtin_type_void_func_ptr);

  if (safe_read_memory_unsigned_integer
      (jmp_buf + offset, TARGET_PTR_BIT / 8, &long_addr))
    {
      *pc = long_addr;
      return 1;
    }
  else
    return 0;
}


/* These hard coded offsets determined by looking at what setjmp actually
   sticks in the setjmp/longjmp jmp_buf structure.  */

#define X86_64_JMPBUF_SAVED_RIP_OFFSET 56
#define I386_JMPBUF_SAVED_EIP_OFFSET 48

static int
x86_64_macosx_get_longjmp_target (CORE_ADDR *pc)
{
  return i386_macosx_get_longjmp_target_helper (X86_64_JMPBUF_SAVED_RIP_OFFSET,
                                                pc);
}

static int
i386_macosx_get_longjmp_target (CORE_ADDR *pc)
{
  return i386_macosx_get_longjmp_target_helper (I386_JMPBUF_SAVED_EIP_OFFSET,
                                                pc);
}

/* We've stopped at a C++ throw or catch.
   Knowing the libstdc++ internal exception structure definitions, find
   the string with the name of the exception we're processing and return
   that string.

   In our current library, these functions are 
     __cxa_throw (void *obj, std::type_info *tinfo, void (*dest) (void *))
     __cxa_begin_catch (void *exc_obj_in) throw()
   In the case of __cxa_throw, its second argument is a pointer to an address
   of a symbol like _ZTIi ("typeinfo for int").
   In the case of __cxa_begin_catch, its argument is a pointer to a
   _Unwind_Exception struct.  This _Unwind_Exception struct is actually
   contained within (at the end of) a __cxa_exception struct.  So given
   the pointer to the _Unwind_Exception, we subtract the necessary amount
   to get the start of the __cxa_exception object.  The first element of
   the __cxa_exception struct is the pointer to a symbol like _ZTIi 
   ("typeinfo for int").  */

char *
i386_throw_catch_find_typeinfo (struct frame_info *curr_frame,
                                int exception_type)
{
  struct minimal_symbol *typeinfo_sym = NULL;
  char *typeinfo_str;

  if (gdbarch_osabi (current_gdbarch) == GDB_OSABI_DARWIN64)
    {
      if (exception_type == EX_EVENT_THROW)
        {
          CORE_ADDR typeinfo_ptr = 
              get_frame_register_unsigned (curr_frame, AMD64_RSI_REGNUM);
          typeinfo_sym = lookup_minimal_symbol_by_pc (typeinfo_ptr);
        }
      else
        {
          ULONGEST typeinfo_ptr;
          CORE_ADDR unwind_exception = 
              get_frame_register_unsigned (curr_frame, AMD64_RDI_REGNUM);
          /* The start of the __cxa_exception structure is the addr of the
             _Unwind_Exception element minus 80 bytes.  */
          if (safe_read_memory_unsigned_integer
              (unwind_exception - 80, 8, &typeinfo_ptr))
            typeinfo_sym = lookup_minimal_symbol_by_pc (typeinfo_ptr);
        }
    }
  else
    {
      CORE_ADDR ebp;
      ebp = get_frame_register_unsigned (curr_frame, I386_EBP_REGNUM);
      if (exception_type == EX_EVENT_THROW)
        {
          ULONGEST typeinfo_ptr;
          if (safe_read_memory_unsigned_integer (ebp + 12, 4, &typeinfo_ptr))
            typeinfo_sym = lookup_minimal_symbol_by_pc (typeinfo_ptr);
        }
      else
        {
          ULONGEST unwind_exception, typeinfo_ptr;
          if (safe_read_memory_unsigned_integer (ebp + 8, 4, &unwind_exception))
            {
              /* The start of the __cxa_exception structure is the addr of the
                 _Unwind_Exception element minus 48 bytes.  */
              if (safe_read_memory_unsigned_integer
                  (unwind_exception - 48, 4, &typeinfo_ptr))
                typeinfo_sym = lookup_minimal_symbol_by_pc (typeinfo_ptr);
            }
        }
    }

  if (!typeinfo_sym)
    return NULL;

  typeinfo_str =
    typeinfo_sym->ginfo.language_specific.cplus_specific.demangled_name;
  if ((typeinfo_str == NULL)
      || (strstr (typeinfo_str, "typeinfo for ") != typeinfo_str))
    return NULL;

  return typeinfo_str + strlen ("typeinfo for ");
}

void
_initialize_i386_macosx_tdep (void)
{
  gdbarch_register_osabi_sniffer (bfd_arch_unknown, bfd_target_mach_o_flavour,
                                  i386_mach_o_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_i386, 0, GDB_OSABI_DARWIN,
                          i386_macosx_init_abi);

  gdbarch_register_osabi (bfd_arch_i386, bfd_mach_x86_64,
                          GDB_OSABI_DARWIN64, x86_macosx_init_abi_64);
}
