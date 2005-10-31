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
#include "i387-tdep.h"
#include "i386-tdep.h"
#include "osabi.h"

#include "i386-macosx-thread-status.h"

#include <mach/thread_status.h>

#include "i386-macosx-tdep.h"

/* Unused on the x86 side of things -- this is only needed to
   help distinguish between ppc32 and ppc64 binaries on the PPC side. */

enum gdb_osabi osabi_seen_in_attached_dyld = GDB_OSABI_UNKNOWN;


#define supply_unsigned_int(regnum, val)\
store_unsigned_integer (buf, 4, val); \
supply_register(regnum, buf);

#define collect_unsigned_int(regnum, addr)\
regcache_collect (regnum, buf); \
(* (addr)) = extract_unsigned_integer (buf, 4);

void
i386_macosx_fetch_gp_registers (gdb_i386_thread_state_t *sp_regs)
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
  collect_unsigned_int (9, &sp_regs->efl);
  collect_unsigned_int (10, &sp_regs->cs);
  collect_unsigned_int (11, &sp_regs->ss);
  collect_unsigned_int (12, &sp_regs->ds);
  collect_unsigned_int (13, &sp_regs->es);
  collect_unsigned_int (14, &sp_regs->fs);
  collect_unsigned_int (15, &sp_regs->gs);
}

/* When we get the FPU registers from the inferior we will either get
   valid register contents or we will get a block of 0's if the FPU has
   no register state.  In the latter case we don't want to push that 
   block of 0's into the FPU as valid state, so remember whether we got
   anything.  */

static int fpu_initialized_p = 0;

/* Fetching the the registers from the inferior into our reg cache.
   FP_REGS is a structure that mirrors the Mach structure
   struct i386_float_state.  The "hw_state" buffer inside that
   structure mirrors the Mach struct i386_fx_save, which is identical
   to the FXSAVE/FXRSTOR instructions' format.  */

void
i386_macosx_fetch_fp_registers (gdb_i386_thread_fpstate_t *fp_regs)
{
  /* If fp_regs->initialized is 0, the kernel has returned a big bucket of
     0's and we wouldn't want to push those back into the FPU when we
     restore them.  */
  if (fp_regs->initialized)
    fpu_initialized_p = 1;
  else
    {
      fpu_initialized_p = 0;
      /* There are some cases were gdb gets tricky and pushes the all-0's
         fpu reg state into the inferior despite our best efforts.  If the
         FPU control word aka FCW (IA32) aka fx_control (Mach) aka 
         I387_FCTRL_REGNUM (gdb) gets pushed to the inferior process with all
         0's, the exception mask is set so that all exceptions are thrown.
         Most notably, the Inexact-Result (Precision) Exception (#P), cf
         Volume 1 of the Intel docs, sec 8.5.6.  The Precision exception is
         very likely to be hit the next time you do any FP operation. 

         To prevent this problem, instead of saving a 0 value as the FCW's
         contents, save 0x37f which is the documented initial value when
         FINIT/FNINIT or FSAVE/FNSAVE instruction is executed to initialize
         the FPU (cf Intel docs Volume 1, 8.1.4, "x87 FPU Control Word")  */
      fp_regs->hw_state[1] = 0x03;
      fp_regs->hw_state[0] = 0x3f;

      /* Yeah, and the same thing in the MXCSR.  All the exception masks
         should be enabled.  */
      fp_regs->hw_state[25] = 0x1f;
      fp_regs->hw_state[24] = 0x80;
    }

  /* Either way, store the buffer (either valid registers or 0's) into the
     local register cache.  In the 0's case, we don't want to accidentally
     display old data--better off displaying all 0's so the user can figure
     out what's up. */

  i387_swap_fxsave (current_regcache, &fp_regs->hw_state);
  i387_supply_fxsave (current_regcache, -1, &fp_regs->hw_state);
}

/* Get the floating point registers from our local register cache
   and stick them in FP_REGS in for sending to the inferior via a
   syscall.  If the local register cache has valid FP values, this
   function returns 1.  If the local register cache does not have
   valid FP values -- and so FP_REGS should not be pushed into the
   inferior -- this function returns 0.  */

int
i386_macosx_store_fp_registers (gdb_i386_thread_fpstate_t *fp_regs)
{
  if (fpu_initialized_p == 0)
    return 0;
    
  memset (fp_regs, 0, sizeof (gdb_i386_thread_fpstate_t));
  fp_regs->fpkind = GDB_i386_FP_SSE2; /* Corresponds to Mach's FP_FXSR */
  fp_regs->initialized = 1;
  fp_regs->exc_status = 0;
  i387_fill_fxsave ((unsigned char *) &fp_regs->hw_state, -1);
  i387_swap_fxsave (current_regcache, &fp_regs->hw_state);

  return 1;
}

/* The sigcontext (struct sigcontext) is put on the stack by the kernel
   and then _sigtramp() is called with an ESP below the sigcontext.
   This function exists to tell gdb how to find the start of the sigcontext
   when the PC is somewhere in the middle of _sigtramp(). 
   Because it's on the stack, its exact offset is different depending on
   whether you're before, in-the-middle-of, or after the _sigtramp 
   prologue frame-setting-up instructions. */

static CORE_ADDR
i386_macosx_sigcontext_addr (struct frame_info *frame)
{
  CORE_ADDR start_of_func = get_frame_func (frame);
  int offset = 0;
  CORE_ADDR push_ebp_addr = 0;
  CORE_ADDR mov_esp_ebp_addr = 0;
  CORE_ADDR pointer_to_ucontext, esp, ebp, pc, ucontext, uc_mcontext;
  char buf[4];
  int limit;

  pc = get_frame_pc (frame);

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

  frame_unwind_register (frame, I386_ESP_REGNUM, buf);
  esp = extract_unsigned_integer (buf, 4);
  frame_unwind_register (frame, I386_EBP_REGNUM, buf);
  ebp = extract_unsigned_integer (buf, 4);

  if (pc <= push_ebp_addr)
    pointer_to_ucontext = esp + 20;

  if (pc > push_ebp_addr && pc <= mov_esp_ebp_addr)
    pointer_to_ucontext = esp + 24;

  if (pc > mov_esp_ebp_addr)
    pointer_to_ucontext = ebp + 24;

  ucontext = read_memory_unsigned_integer (pointer_to_ucontext, 4);
  /* 'uc_mcontext' is 28 bytes into the 'struct ucontext' */
  uc_mcontext = read_memory_unsigned_integer (ucontext + 28, 4); 
  return uc_mcontext; /* uc_mcontext is a pointer to the struct sigcontext */
}

static CORE_ADDR
i386_integer_to_address (struct type *type, void *buf)
{
  char *tmp = alloca (TYPE_LENGTH (builtin_type_void_data_ptr));
  LONGEST val = unpack_long (type, buf);
  store_unsigned_integer (tmp, TYPE_LENGTH (builtin_type_void_data_ptr), val);
  return extract_unsigned_integer (tmp,
                                   TYPE_LENGTH (builtin_type_void_data_ptr));
}

/* From /usr/include/i386/signal.h and i386-tdep.h */
static int i386_macosx_sc_reg_offset[] =
{
   2 * 4,   /* EAX */
   4 * 4,   /* ECX */
   5 * 4,   /* EDX */
   3 * 4,   /* EBX */
   9 * 4,   /* ESP */
   8 * 4,   /* EBP */
   7 * 4,   /* ESI */
   6 * 4,   /* EDI */
  12 * 4,   /* EIP */
  11 * 4,   /* EFLAGS */
  13 * 4,   /* CS */
  -1,       /* SS */
  14 * 4,   /* DS */
  15 * 4,   /* ES */
  16 * 4,   /* FS */
  17 * 4    /* GS */
};

static void
i386_macosx_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* We support the SSE registers.  */
  tdep->num_xmm_regs = I386_NUM_XREGS - 1;
  set_gdbarch_num_regs (gdbarch, I386_SSE_NUM_REGS);

  set_gdbarch_skip_trampoline_code (gdbarch, macosx_skip_trampoline_code);
  set_gdbarch_dynamic_trampoline_nextpc (gdbarch,
                                         macosx_dynamic_trampoline_nextpc);

  set_gdbarch_in_solib_call_trampoline (gdbarch,
                                        macosx_in_solib_call_trampoline);
  set_gdbarch_in_solib_return_trampoline (gdbarch,
                                          macosx_in_solib_return_trampoline);

  tdep->struct_return = reg_struct_return;

  tdep->sigcontext_addr = i386_macosx_sigcontext_addr;
  tdep->sc_reg_offset = i386_macosx_sc_reg_offset;
  tdep->sc_num_regs = 16;

  tdep->jb_pc_offset = 20;
  set_gdbarch_integer_to_address (gdbarch, i386_integer_to_address);
}

static enum gdb_osabi
i386_mach_o_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "mach-o-le") == 0
      || strcmp (bfd_get_target (abfd), "mach-o-fat") == 0)
    return GDB_OSABI_DARWIN;

  return GDB_OSABI_UNKNOWN;
}


void
_initialize_i386_macosx_tdep (void)
{
  gdbarch_register_osabi_sniffer (bfd_arch_unknown, bfd_target_mach_o_flavour,
                                  i386_mach_o_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_i386, 0, GDB_OSABI_DARWIN,
                          i386_macosx_init_abi);

}
