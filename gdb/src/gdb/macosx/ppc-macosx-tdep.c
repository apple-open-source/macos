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
#include "gdbcmd.h"
#include "arch-utils.h"
#include "floatformat.h"
#include "gdbtypes.h"
#include "regcache.h"
#include "reggroups.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "dummy-frame.h"
#include "gdb_assert.h"
#include "complaints.h"
#include "user-regs.h"
#include "objfiles.h"

#include "libbfd.h"

#include "elf-bfd.h"
#include "dis-asm.h"
#include "ppc-tdep.h"
#include "gdbarch.h"
#include "osabi.h"

#include "ppc-macosx-tdep.h"
#include "ppc-macosx-regs.h"
#include "ppc-macosx-regnums.h"
#include "ppc-macosx-tdep.h"
#include "ppc-macosx-frameinfo.h"

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <mach/machine.h>

/* static */ void
rs6000_value_to_register (struct frame_info *frame,
                          int regnum,
                          struct type *type,
                          const gdb_byte *from);
/* static */ void
rs6000_register_to_value (struct frame_info *frame,
                          int regnum,
                          struct type *type,
                          gdb_byte *to);

#undef XMALLOC
#define XMALLOC(TYPE) ((TYPE*) xmalloc (sizeof (TYPE)))

/*
   0x030     c linkage
   0x040     paramsavelen
   0x008     pad
   0x040     siginfo_t
        int     si_signo;                signal number
        int     si_errno;                errno association
        int     si_code;                 signal code
        pid_t   si_pid;                  sending process
        uid_t   si_uid;                  sender's ruid
        int     si_status;               exit value
        void    *si_addr;                faulting instruction
        union sigval si_value;           signal value
        long    si_band;                 band event for SIGPOLL
        unsigned int    pad[7];          Reserved for Future Use
   0x020     ucontext
        int             uc_onstack;
        sigset_t        uc_sigmask;      signal mask used by this context (unsigned int)
        stack_t         uc_stack;        stack used by this context
                        void    *ss_sp;          signal stack base
                        size_t  ss_size;         signal stack length
                        int     ss_flags;        SA_DISABLE and/or SA_ONSTACK
        struct ucontext *uc_link;        pointer to resuming context
        size_t          uc_mcsize;       size of the machine context passed in
        mcontext_t      uc_mcontext;     machine specific context
   0x410     struct mcontext32
   - 0x020   - ppc_exception_state_t
   - 0x0a0   - ppc_thread_state_t
   - 0x108   - ppc_float_state_t
   - 0x240   - ppc_vector_state_t
   0x410     struct mcontext64
   - 0x020   - ppc_exception_state_t
   - 0x0a0   - ppc_thread_state64_t
   - 0x108   - ppc_float_state_t
   - 0x240   - ppc_vector_state_t
   0x0e0     redzone
*/

const unsigned int PPC_FRAME_MAGIC = 0xfe30a3d7;

const unsigned int PPC_SIGCONTEXT_PC_OFFSET = 0x90;
const unsigned int PPC_SIGCONTEXT_SP_OFFSET = 0x9c;

static int ppc_debugflag = 0;
static unsigned int ppc_max_frame_size = UINT_MAX;

static void ppc_macosx_init_abi (struct gdbarch_info info,
                                 struct gdbarch *gdbarch);

static void ppc_macosx_init_abi_64 (struct gdbarch_info info,
                                    struct gdbarch *gdbarch);

static int ppc_macosx_get_longjmp_target (CORE_ADDR * pc);
static int ppc_64_macosx_get_longjmp_target (CORE_ADDR * pc);
static struct value *ppc_value_of_builtin_frame_fp_reg (struct frame_info *);

CORE_ADDR
ppc_frame_unwind_sp_for_dereferencing (struct frame_info *next_frame, 
                                       void **this_cache);
void
ppc_debug (const char *fmt, ...)
{
  va_list ap;
  if (ppc_debugflag)
    {
      va_start (ap, fmt);
      vfprintf (stderr, fmt, ap);
      va_end (ap);
    }
}

void
ppc_print_extra_frame_info (struct frame_info *next_frame, void **this_cache)
{
  struct ppc_frame_cache *cache;
  struct ppc_function_boundaries *bounds;
  struct ppc_function_properties *props;

  if (get_frame_type (get_prev_frame (next_frame)) == DUMMY_FRAME)
    return;

  cache = ppc_frame_cache (next_frame, this_cache);

  if (get_frame_type (next_frame) == SIGTRAMP_FRAME)
    {
      printf_filtered (" This function was called from a signal handler.\n");
    }
  else
    {
      printf_filtered
        (" This function was not called from a signal handler.\n");
    }

  bounds = ppc_frame_function_boundaries (next_frame, this_cache);
  if (bounds != NULL)
    {
      ppc_print_boundaries (bounds);
    }
  else
    {
      printf_filtered
        (" Unable to determine function boundary information.\n");
    }

  props = ppc_frame_function_properties (next_frame, this_cache);
  if (props != NULL)
    {
      ppc_print_properties (props);
    }
  else
    {
      printf_filtered
        (" Unable to determine function property information.\n");
    }
}

static struct ppc_frame_cache *
ppc_alloc_frame_cache (void)
{
  struct ppc_frame_cache *cache;
  int i;

  cache = FRAME_OBSTACK_ZALLOC (struct ppc_frame_cache);

  cache->magic = PPC_FRAME_MAGIC;

  cache->saved_regs =
    (CORE_ADDR *) frame_obstack_zalloc ((NUM_REGS) * sizeof (CORE_ADDR));

  cache->prev_pc = (CORE_ADDR) -1;
  cache->prev_sp = (CORE_ADDR) -1;

  cache->sp = (CORE_ADDR) -1;
  cache->fp = (CORE_ADDR) -1;
  cache->pc = (CORE_ADDR) -1;
  cache->sp_for_dereferencing = (CORE_ADDR) -1;

  for (i = 0; i < NUM_REGS; i++)
    cache->saved_regs[i] = -1;
  cache->saved_regs_valid = 0;

  cache->properties_valid = 0;

  cache->boundaries_status = 0;

  return cache;
}

struct ppc_frame_cache *
ppc_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct ppc_frame_cache *cache;

  if (*this_cache)
    {
      cache = *this_cache;
      gdb_assert (cache->magic == PPC_FRAME_MAGIC);
      return cache;
    }

  cache = ppc_alloc_frame_cache ();
  *this_cache = cache;

  cache->sp = ppc_frame_unwind_sp (next_frame, this_cache);
  if (cache->sp == 0)
    return cache;

  cache->fp = ppc_frame_unwind_fp (next_frame, this_cache);

  /* The PC field has the value of the function start unless we can't
     determine a function start address, e.g. in a completely stripped
     executable.  In that case we'll use the current PC value.  */

  cache->pc = frame_func_unwind (next_frame);
  if (cache->pc == 0)
    cache->pc = frame_pc_unwind (next_frame);

  /* A stack pointer which can be dereferenced to find the caller's
     stack pointer and follow the chain.  */

  cache->sp_for_dereferencing = ppc_frame_unwind_sp_for_dereferencing (next_frame, this_cache);

  extern int frame_debug;
  if (frame_debug)
    {
      fprintf_unfiltered (gdb_stdlog, "\
{{ ppc_frame_cache (nxframe=%d) ",
			  frame_relative_level (next_frame));
      fprintf_unfiltered (gdb_stdlog, "->");
      fprintf_unfiltered (gdb_stdlog, " pc=0x%s", paddr_nz (cache->pc));
      fprintf_unfiltered (gdb_stdlog, " sp=0x%s", paddr_nz (cache->sp));
      fprintf_unfiltered (gdb_stdlog, " fp=0x%s", paddr_nz (cache->fp));
      fprintf_unfiltered (gdb_stdlog, " }}\n");
    }

  return cache;
}

/* Assuming a frame chain of: (outer) prev <-> this <-> next (inner)
   return the value of prev's pc.  */

CORE_ADDR
ppc_frame_find_prev_pc (struct frame_info *next_frame, void **this_cache)
{
  struct ppc_frame_cache *cache;
  ppc_function_properties *props = NULL;
  CORE_ADDR prev_sp, this_pc;

  cache = ppc_frame_cache (next_frame, this_cache);

  props = ppc_frame_function_properties (next_frame, this_cache);
  if (props == NULL)
    return 0;

  prev_sp = ppc_frame_find_prev_sp (next_frame, this_cache);
  this_pc = frame_pc_unwind (next_frame);

  /* Has the return address (link register) been saved to memory already?  */
  if (props->lr_saved && props->lr_saved < this_pc)
    {
      return read_memory_unsigned_integer (prev_sp + props->lr_offset,
                                       gdbarch_addr_bit (current_gdbarch) / 8);
    }
  else if (props->lr_reg >= 0
           && props->lr_invalid
           && this_pc > props->lr_invalid
           && this_pc <= props->lr_valid_again)
    {
      /* Or is the link register value in a register?  */
      return frame_unwind_register_unsigned (next_frame, props->lr_reg);
    }
  else
    {
      return frame_unwind_register_unsigned (next_frame, PPC_MACOSX_LR_REGNUM);
    }
}

/* Given NEXT_FRAME, find the fp for NEXT_FRAME->prev, i.e. the 'this' frame. 
   If this function is in its prologue, the address of what the frame pointer
   WILL be set to is returned.  This gives you a consistent fp as you stepi
   through the function prologue/body but it means that you can't dereference
   the stored memory at the fp and get the caller's fp.  */

CORE_ADDR
ppc_frame_unwind_fp (struct frame_info *next_frame, void **this_cache)
{
  struct ppc_frame_cache *cache;
  ppc_function_properties *props;
  CORE_ADDR this_fp, this_pc, this_func, next_sp;

  cache = ppc_frame_cache (next_frame, this_cache);

  props = ppc_frame_function_properties (next_frame, this_cache);
  if (props == NULL)
    return frame_unwind_register_unsigned (next_frame, SP_REGNUM);

  if (props->frameless)
    return frame_unwind_register_unsigned (next_frame, SP_REGNUM);

  this_pc = frame_pc_unwind (next_frame);
  this_func = frame_func_unwind (next_frame);

  /* The stack pointer (r1) hasn't yet been changed, but it will be moved
     a little while later.  Compute its final address and return that as 
     the frame ptr value.  
     It is desirable that the frame pointer's value not change while
     stepping through a function so the user doesn't see spurious 
     "function stepped into" type messages from gdb.  */

  if (props->stack_offset_pc != INVALID_ADDRESS
      && this_pc >= this_func
      && this_pc <= props->stack_offset_pc)
    {
      CORE_ADDR this_sp;
      this_sp = frame_unwind_register_unsigned (next_frame, SP_REGNUM);
      return this_sp - props->offset;
    }

  /* No frame pointer is used in this function, so return the stack pointer's
     value.  To get to this conditional, the stack pointer (r1) has already
     been adjusted for this function's stack frame so no additional adjustment 
     is necessary.  */

  if (props->frameptr_used == 0)
    return frame_unwind_register_unsigned (next_frame, SP_REGNUM);

  /* The stack pointer (r1) has been moved to its final address but the
     value hasn't been copied into the frame pointer reg (often r30) yet.
     Return the value in r1 as the frame pointer value.  */

  if (props->frameptr_used && props->frameptr_reg > 0
      && props->frameptr_pc != INVALID_ADDRESS
      && this_pc > props->stack_offset_pc
      && this_pc <= props->frameptr_pc)
    {
      return frame_unwind_register_unsigned (next_frame, SP_REGNUM);
    }

  /* With a few safety checks, we can safely return the value of the frame
     pointer register at this point.  */

  if (props->frameptr_used && props->frameptr_reg > 0)
    {
      /* Be a little bit careful here.  We are trying to unwind the
	 frameptr register, but we might get it wrong (for instance
	 because we mis-parsed a prologue and got the stored location
	 wrong.)  So if the fp value looks ridiculous, fall back
	 on the SP value - which we seem to get right more
	 consistently... */

      next_sp = get_frame_register_unsigned (next_frame, SP_REGNUM);
      this_fp = frame_unwind_register_unsigned (next_frame, 
                                                props->frameptr_reg);
      if (this_fp >= next_sp && this_fp - next_sp < 0xfffff)
          return this_fp;
      complaint (&symfile_complaints, 
                                  "sanity check failed in ppc_frame_unwind_fp");
    }

  /* And if all else fails, return the value of the stack pointer (r1) 
     register.  */

  return frame_unwind_register_unsigned (next_frame, SP_REGNUM);
}

/* Given NEXT_FRAME, find the stack pointer for NEXT_FRAME->prev,
   i.e. the 'this' frame which can be dereferenced.  This is almost
   identical to ppc_frame_unwind_sp, except that we don't try to do
   anything clever with the prologue parsing, since that really never
   helps us for this, and sometimes leads us astray.
   I could eliminate this function, and go back to straight grabbing
   the SP_REGNUM value, but this stuff always needs fiddling, and I'm
   not going to bet we won't have to put something BACK in here later.
  */

CORE_ADDR
ppc_frame_unwind_sp_for_dereferencing (struct frame_info *next_frame,
                                       void **this_cache)
{
  /* We always just assume that this is in the STACK POINTER... */
  return frame_unwind_register_unsigned (next_frame, SP_REGNUM);
}

CORE_ADDR
ppc_frame_unwind_sp (struct frame_info *next_frame, void **this_cache)
{
  struct ppc_frame_cache *cache;
  ppc_function_properties *props;
  CORE_ADDR this_pc;

  cache = ppc_frame_cache (next_frame, this_cache);

  props = ppc_frame_function_properties (next_frame, this_cache);
  if (props == NULL)
    return frame_unwind_register_unsigned (next_frame, SP_REGNUM);

  this_pc = frame_pc_unwind (next_frame);

  /* When we're early in the prologue neither the stack pointer nor
     the frame pointer have been set yet - they still have the
     caller's values - but they'll be set up in a few instructions.
     Figure out what the real sp value will be for this frame and
     return that.  */

  if (this_pc >= frame_func_unwind (next_frame)
      && props->stack_offset_pc != INVALID_ADDRESS
      && this_pc <= props->stack_offset_pc)
    {
      return frame_unwind_register_unsigned 
                                    (next_frame, SP_REGNUM) - props->offset;
    }

  return frame_unwind_register_unsigned (next_frame, SP_REGNUM);
}


/* Assuming a frame chain of: (outer) prev <-> this <-> next (inner)
   return the value of prev's stack pointer.  */

CORE_ADDR
ppc_frame_find_prev_sp (struct frame_info *next_frame, void **this_cache)
{
  ppc_function_properties *props = NULL;
  CORE_ADDR this_sp, this_pc, this_func;

  /* Get 'this' frame's stack pointer and prologue analysis.  */

  this_sp = frame_unwind_register_unsigned (next_frame, SP_REGNUM);
  if (this_sp == 0)
    return 0;

  props = ppc_frame_function_properties (next_frame, this_cache);
  if (props == NULL)
    return 0;

  /* If 'this' is frameless, 'prev's stack pointer is the same as
     the stack pointer in 'this'.  */

  if (props->frameless)
    return this_sp;

  this_pc = frame_pc_unwind (next_frame);
  this_func = frame_func_unwind (next_frame);

  /* If we're in the 'this' prologue prior to frame setup, act as if the
     function was frameless, otherwise dereferencing the sp will go
     one frame too far.  */
  if (this_pc >= this_func
      && props->stack_offset_pc != INVALID_ADDRESS
      && this_pc <= props->stack_offset_pc)
    {
      return this_sp;
    }

  /* It might seem reasonable to use the frame pointer rather than the
     stack pointer to unwind the stack, but that's not right.
     The compiler actually moves the previous sp value to the location
     pointed to by the new stack pointer, when it moves the stack
     pointer.  And sometimes, it even overwrites the value pointed to
     by the frame pointer.  So while you need the frame pointer to get
     the locals, etc, you have to use the stack pointer to follow the
     chain.  */

  return read_memory_unsigned_integer (this_sp,
                                       gdbarch_addr_bit (current_gdbarch) /
                                       8);
}

CORE_ADDR
ppc_skip_prologue (CORE_ADDR pc)
{
  ppc_function_boundaries_request request;
  ppc_function_boundaries bounds;
  int ret;

  ppc_clear_function_boundaries_request (&request);
  ppc_clear_function_boundaries (&bounds);

  request.prologue_start = pc;
  ret = ppc_find_function_boundaries (&request, &bounds);
  if (ret != 0)
    {
      return 0;
    }

  return bounds.body_start;
}

const char *gdb_register_names[] = {
  "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
  "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
  "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
  "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
  "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
  "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
  "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",
  "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
  "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
  "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
  "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
  "pc", "ps", "cr", "lr", "ctr", "xer", "mq",
  "fpscr",
  "vscr", "vrsave"
};

const char *
ppc_register_name (int reg_nr)
{
  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= (sizeof (gdb_register_names) / sizeof (*gdb_register_names)))
    return NULL;
  return gdb_register_names[reg_nr];
}

/* Sequence of bytes for breakpoint instruction.  */

static const unsigned char *
ppc_breakpoint_from_pc (CORE_ADDR * addr, int *size)
{
  static unsigned char big_breakpoint[] = { 0x7f, 0xe0, 0x00, 0x08 };
  static unsigned char little_breakpoint[] = { 0x08, 0x00, 0xe0, 0x7f };
  *size = 4;
  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
    return big_breakpoint;
  else
    return little_breakpoint;
}

static struct type *
ppc_register_virtual_type (struct gdbarch *gdbarch, int n)
{
  if (((n >= PPC_MACOSX_FIRST_GP_REGNUM) && (n <= PPC_MACOSX_LAST_GP_REGNUM))
      || (n == PPC_MACOSX_PC_REGNUM)
      || (n == PPC_MACOSX_PS_REGNUM)
      || (n == PPC_MACOSX_LR_REGNUM)
      || (n == PPC_MACOSX_CTR_REGNUM)
      || (n == PPC_MACOSX_XER_REGNUM))
    {
      /* I think it's okay to always treat registers as long long.
	 We always use the 64 bit calls even on G4 systems, and let
	 the system cut this down to 32 bits.  */
      return builtin_type_unsigned_long_long;
    }
  if ((n >= PPC_MACOSX_FIRST_VP_REGNUM) && (n <= PPC_MACOSX_LAST_VP_REGNUM))
    return builtin_type_vec128;
  if ((n >= PPC_MACOSX_FIRST_FP_REGNUM) && (n <= PPC_MACOSX_LAST_FP_REGNUM))
    return builtin_type_double;
  return builtin_type_unsigned_int;
}

int
ppc_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
                         struct reggroup *group)
{
  if ((regnum == PPC_MACOSX_VSCR_REGNUM)
      || (regnum == PPC_MACOSX_VRSAVE_REGNUM))
    if (group == vector_reggroup)
      return 1;

  if (regnum == PPC_MACOSX_FPSCR_REGNUM)
    if (group == float_reggroup)
      return 1;

  return default_register_reggroup_p (gdbarch, regnum, group);
}

int
ppc_use_struct_convention (int gccp, struct type *value_type)
{
  if ((TYPE_LENGTH (value_type) == 16) && TYPE_VECTOR (value_type))
    return 0;

  return 1;
}

static CORE_ADDR
ppc_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct ppc_frame_cache *cache = ppc_frame_cache (next_frame, this_cache);

  return cache->fp;
}

static CORE_ADDR
ppc_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, PC_REGNUM);
}

static void
ppc_frame_prev_register (struct frame_info *next_frame, void **this_cache,
			 /* APPLE LOCAL variable opt states.  */
                         int regnum, enum opt_state *optimizedp,
                         enum lval_type *lvalp, CORE_ADDR *addrp,
                         int *realnump, gdb_byte *valuep)
{
  struct ppc_frame_cache *cache = ppc_frame_cache (next_frame, this_cache);
  CORE_ADDR *saved_regs = NULL;
  ppc_function_properties *props;
  props = ppc_frame_function_properties (next_frame, this_cache);

  if (regnum == SP_REGNUM)
    {
      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_okay;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;

      if (valuep)
        {
          if (cache->prev_sp == (CORE_ADDR) - 1)
            cache->prev_sp = ppc_frame_find_prev_sp (next_frame, this_cache);
          store_unsigned_integer (valuep,
                                  register_size (current_gdbarch, regnum),
                                  cache->prev_sp);
        }

      return;
    }

  if (regnum == PC_REGNUM)
    {
      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_okay;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;

      if (valuep)
        {
          if (cache->prev_pc == (CORE_ADDR) - 1)
            cache->prev_pc = ppc_frame_find_prev_pc (next_frame, this_cache);
          store_unsigned_integer (valuep,
                                  register_size (current_gdbarch, regnum),
                                  cache->prev_pc);
        }

      return;
    }

  saved_regs = ppc_frame_saved_regs (next_frame, this_cache);

  if ((saved_regs != NULL) && (saved_regs[regnum] != -1))
    {
      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_okay;
      *lvalp = lval_memory;
      *addrp = cache->saved_regs[regnum];
      *realnump = -1;
      if (valuep)
        {
	  int wordsize;
	  int offset;

	  /* The FP & VP registers are the same size on 32 bit & 64 bit
	     processors.  The second branch of the if handles the GP
	     regs, since for a 32 bit task, the registers are 64 bits,
	     but only 32 bits are stored on the stack.  */

	  if (PPC_MACOSX_IS_FP_REGNUM (regnum) 
	      || PPC_MACOSX_IS_VP_REGNUM (regnum))
	    {
	      wordsize = register_size (current_gdbarch, regnum);
	      offset = 0;
	    }
	  else
	    {
	      wordsize = (gdbarch_tdep (current_gdbarch))->wordsize;
	      offset = register_size (current_gdbarch, regnum) - wordsize;
	      *((int *) valuep) = 0;
	    }

          read_memory (*addrp, valuep + offset, wordsize);
        }
      return;
    }

  frame_register_unwind (next_frame, regnum,
                         optimizedp, lvalp, addrp, realnump, valuep);
}

static struct frame_id
ppc_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  gdb_byte buf[8];
  CORE_ADDR fp;

  frame_unwind_register (next_frame, SP_REGNUM, buf);
  fp =
    extract_unsigned_integer (buf,
                              register_size (current_gdbarch, SP_REGNUM));

  return frame_id_build (fp, frame_pc_unwind (next_frame));
}

static void
ppc_frame_this_id (struct frame_info *next_frame, void **this_cache,
                   struct frame_id *this_id)
{
  ULONGEST prev_frame_addr = 0;
  struct ppc_frame_cache *cache = ppc_frame_cache (next_frame, this_cache);

  if (cache->sp == 0 || cache->fp == 0)
    {
      *this_id = null_frame_id;
      return;
    }

  /* This double dereference check is needed to see if we're on the
     last frame from a stripped executable or a thread that doesn't
     have a main() function to stop at.  If you remove this you'll get
     (best case), an extra frame on each thread with an address of 0x0.  */

  if (safe_read_memory_unsigned_integer
      (cache->sp_for_dereferencing, TARGET_PTR_BIT / 8, &prev_frame_addr))
    {
      if (safe_read_memory_unsigned_integer
          (prev_frame_addr, TARGET_PTR_BIT / 8, &prev_frame_addr))
        if (prev_frame_addr == 0)
          {
            *this_id = null_frame_id;
            return;
          }
    }

  (*this_id) = frame_id_build (cache->fp, cache->pc);
}

static struct ppc_frame_cache *
ppc_sigtramp_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  enum
  { EXCSTATE = 0x20, GPSTATE = 0xa0, GPSTATE64 = 0x130,
    FPSTATE = 0x108, VPSTATE = 0x240
  };

  struct ppc_frame_cache *cache;
  CORE_ADDR sigframe, context;
  ULONGEST length = 0;
  CORE_ADDR regs = 0, fpregs = 0, vpregs = 0;
  unsigned int reglen = 4;
  unsigned int i;

  if (*this_cache)
    return *this_cache;

  cache = ppc_alloc_frame_cache ();
  *this_cache = cache;

  cache->sp = frame_unwind_register_unsigned (next_frame, SP_REGNUM);
  if (cache->sp == 0)
    return cache;

  cache->pc = frame_func_unwind (next_frame);

  sigframe = read_memory_unsigned_integer (cache->sp, TARGET_PTR_BIT / 8);
  context =
    read_memory_unsigned_integer (sigframe + 0xd4, TARGET_PTR_BIT / 8);
  length = read_memory_unsigned_integer (sigframe + 0xd0, TARGET_INT_BIT / 8);

  switch (length)
    {
    case (EXCSTATE + GPSTATE + FPSTATE + VPSTATE):
      vpregs = context + EXCSTATE + GPSTATE + FPSTATE;
    case (EXCSTATE + GPSTATE + FPSTATE):
      fpregs = context + EXCSTATE + GPSTATE;
    case (EXCSTATE + GPSTATE):
      regs = context + EXCSTATE;
      reglen = 4;
      break;

    case (EXCSTATE + GPSTATE64 + FPSTATE + VPSTATE):
      vpregs = context + EXCSTATE + GPSTATE64 + FPSTATE;
    case (EXCSTATE + GPSTATE64 + FPSTATE):
      fpregs = context + EXCSTATE + GPSTATE64;
    case (EXCSTATE + GPSTATE64):
      regs = context + EXCSTATE;
      reglen = 8;
      break;

    default:
      warning ("unrecognized length (0x%lx) for sigtramp context",
               (unsigned long) length);
      break;
    }

  cache->sigtramp_gp_store_size = reglen;

  /* FIXME: need to see how these actually get laid out on 64-bit systems. */

  if (regs != 0)
    {
      /* The CR register is always stored in an unsigned int...  */
      int cr_offset = reglen - 4;

      cache->saved_regs[PPC_MACOSX_PC_REGNUM] = regs;
      cache->saved_regs[PPC_MACOSX_PS_REGNUM] = regs + (1 * reglen);
      for (i = 0; i < 32; i++)
        cache->saved_regs[PPC_MACOSX_FIRST_GP_REGNUM + i] =
          regs + ((i + 2) * reglen);
      cache->saved_regs[PPC_MACOSX_CR_REGNUM] = regs + (34 * reglen);
      cache->saved_regs[PPC_MACOSX_XER_REGNUM] = regs + (35 * reglen) - cr_offset;
      cache->saved_regs[PPC_MACOSX_LR_REGNUM] = regs + (36 * reglen) - cr_offset;
      cache->saved_regs[PPC_MACOSX_CTR_REGNUM] = regs + (37 * reglen) - cr_offset;
      cache->saved_regs[PPC_MACOSX_MQ_REGNUM] = regs + (38 * reglen) - cr_offset;
      cache->saved_regs[PPC_MACOSX_VRSAVE_REGNUM] = regs + (39 * reglen) - cr_offset;
    }

  if (fpregs != 0)
    {
      for (i = 0; i < 32; i++)
        cache->saved_regs[PPC_MACOSX_FIRST_FP_REGNUM + i] = fpregs + (i * 8);
      cache->saved_regs[PPC_MACOSX_FPSCR_REGNUM] = fpregs + (i * 8) + 4;
    }

  if (vpregs != 0)
    {
      for (i = 0; i < 32; i++)
        cache->saved_regs[PPC_MACOSX_FIRST_VP_REGNUM + i] = fpregs + (i * 16);
      cache->saved_regs[PPC_MACOSX_VSCR_REGNUM] = vpregs + (i * 16);
      /* cache->saved_regs[PPC_MACOSX_VRSAVE_REGNUM] = vpregs + (i * 16) +  (4 * 8); */
    }

  cache->saved_regs_valid = 1;

  return cache;
}

static void
ppc_sigtramp_frame_this_id (struct frame_info *next_frame, void **this_cache,
                            struct frame_id *this_id)
{
  struct ppc_frame_cache *cache =
    ppc_sigtramp_frame_cache (next_frame, this_cache);

  /* See the end of ppc_push_dummy_call.  */
  (*this_id) = frame_id_build (cache->fp, frame_pc_unwind (next_frame));
}

static void
ppc_sigtramp_frame_prev_register (struct frame_info *next_frame,
                                  void **this_cache,
				  /* APPLE LOCAL variable opt states.  */
                                  int regnum, enum opt_state *optimizedp,
                                  enum lval_type *lvalp, CORE_ADDR * addrp,
                                  int *realnump, gdb_byte *valuep)
{
  struct ppc_frame_cache *cache =
    ppc_sigtramp_frame_cache (next_frame, this_cache);
  CORE_ADDR *saved_regs = NULL;

  saved_regs = ppc_frame_saved_regs (next_frame, this_cache);

  if ((saved_regs != NULL) && (saved_regs[regnum] != -1))
    {
      int size;
      int offset = 0;

      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_okay;
      *lvalp = lval_memory;
      *addrp = cache->saved_regs[regnum];
      *realnump = -1;
      if (valuep)
        {
          int reg_size = register_size (current_gdbarch, regnum);
          if (regnum == PPC_MACOSX_CR_REGNUM || regnum == PPC_MACOSX_MQ_REGNUM)
              size = 4;
          else if (PPC_MACOSX_IS_GP_REGNUM (regnum) || PPC_MACOSX_IS_GSP_REGNUM (regnum))
              size = cache->sigtramp_gp_store_size;
          else
              size = reg_size;

	  gdb_assert (reg_size >= size);
	  offset = reg_size - size;

          if (reg_size > size)
            bzero ((char *) valuep, reg_size - size);

          read_memory (*addrp, valuep + offset, size);
        }
      return;
    }

  frame_register_unwind (next_frame, regnum,
                         optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind ppc_sigtramp_frame_unwind = {
  SIGTRAMP_FRAME,
  ppc_sigtramp_frame_this_id,
  ppc_sigtramp_frame_prev_register
};

static const struct frame_unwind *
ppc_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  char *name;

  find_pc_partial_function_no_inlined (pc, &name, NULL, NULL);
  if (legacy_pc_in_sigtramp (pc, name))
    return &ppc_sigtramp_frame_unwind;

  return NULL;
}

static const struct frame_unwind ppc_frame_unwind = {
  NORMAL_FRAME,
  ppc_frame_this_id,
  ppc_frame_prev_register
};

static const struct frame_unwind *
ppc_frame_sniffer (struct frame_info *next_frame)
{
  return &ppc_frame_unwind;
}

static const struct frame_base ppc_frame_base = {
  &ppc_frame_unwind,
  ppc_frame_base_address,
  ppc_frame_base_address,
  ppc_frame_base_address
};

/* Get the ith function argument for the current function.  */
CORE_ADDR
ppc_fetch_pointer_argument (struct frame_info *frame, int argi,
                            struct type *type)
{
  CORE_ADDR addr;
  static unsigned long long mask = 0xffffffff;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  addr = get_frame_register_unsigned (frame, 3 + argi);

  /* I am not sure whether the ABI specifies that the upper 32
     bits of the register holding a pointer need to be set to
     zero.  So clear them here just in case.  */

  if (tdep->wordsize == 4)
    addr &= mask;
  return addr;
}

static CORE_ADDR
ppc_integer_to_address (struct gdbarch *gdbarch, struct type *type, 
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
ppc_macosx_frame_align (struct gdbarch *gdbarch, CORE_ADDR addr)
{
   return (addr & -16);
}

static struct gdbarch *
ppc_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch_tdep *tdep;
  struct gdbarch *gdbarch;

  /* If there is already a candidate, use it.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  /* Allocate space for the new architecture.  */
  tdep = XMALLOC (struct gdbarch_tdep);
  gdbarch = gdbarch_alloc (&info, tdep);

  tdep->wordsize = -1;
  tdep->regs = 0;
  tdep->ppc_gp0_regnum = -1;
  tdep->ppc_toc_regnum = -1;
  tdep->ppc_ps_regnum = -1;
  tdep->ppc_cr_regnum = -1;
  tdep->ppc_lr_regnum = -1;
  tdep->ppc_ctr_regnum = -1;
  tdep->ppc_xer_regnum = -1;
  tdep->ppc_fpscr_regnum = -1;
  tdep->ppc_mq_regnum = -1;
  tdep->ppc_vr0_regnum = -1;
  tdep->ppc_vrsave_regnum = -1;
  tdep->ppc_ev0_regnum = -1;
  tdep->ppc_ev31_regnum = -1;
  tdep->lr_frame_offset = -1;

  tdep->wordsize = 4;
  tdep->ppc_toc_regnum = 2;
  tdep->ppc_cr_regnum = PPC_MACOSX_CR_REGNUM;
  tdep->ppc_ctr_regnum = PPC_MACOSX_CTR_REGNUM;
  tdep->ppc_xer_regnum = PPC_MACOSX_XER_REGNUM;
  tdep->ppc_sr0_regnum = 71;
  tdep->ppc_vr0_regnum = PPC_MACOSX_FIRST_VP_REGNUM;
  tdep->ppc_vrsave_regnum = PPC_MACOSX_VRSAVE_REGNUM;
  tdep->ppc_ev0_upper_regnum = -1;
  tdep->ppc_ev0_regnum = -1;
  tdep->ppc_ev31_regnum = -1;
  tdep->ppc_acc_regnum = -1;
  tdep->ppc_spefscr_regnum = -1;

  tdep->ppc_fp0_regnum = PPC_MACOSX_FIRST_FP_REGNUM;
  tdep->ppc_fpscr_regnum = PPC_MACOSX_FPSCR_REGNUM;

  tdep->ppc_lr_regnum = PPC_MACOSX_LR_REGNUM;
  tdep->ppc_gp0_regnum = PPC_MACOSX_FIRST_GP_REGNUM;

  set_gdbarch_num_regs (gdbarch, PPC_MACOSX_NUM_REGS);
  set_gdbarch_sp_regnum (gdbarch, PPC_MACOSX_SP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, PPC_MACOSX_PC_REGNUM);
  set_gdbarch_ps_regnum (gdbarch, PPC_MACOSX_PS_REGNUM);
  set_gdbarch_fp0_regnum (gdbarch, PPC_MACOSX_FIRST_FP_REGNUM);


  set_gdbarch_register_name (gdbarch, ppc_register_name);
  set_gdbarch_register_type (gdbarch, ppc_register_virtual_type);
  set_gdbarch_addr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_ptr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_short_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_int_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_long_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_float_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_long_double_bit (gdbarch, 16 * TARGET_CHAR_BIT);

  switch (info.byte_order)
    {
    case BFD_ENDIAN_BIG:
      set_gdbarch_float_format (gdbarch, &floatformat_ieee_single_big);
      set_gdbarch_double_format (gdbarch, &floatformat_ieee_double_big);
      set_gdbarch_long_double_format (gdbarch, &floatformat_ieee_double_big);
      break;
    case BFD_ENDIAN_LITTLE:
      set_gdbarch_float_format (gdbarch, &floatformat_ieee_single_little);
      set_gdbarch_double_format (gdbarch, &floatformat_ieee_double_little);
      set_gdbarch_long_double_format (gdbarch,
                                      &floatformat_ieee_double_little);
      break;
    default:
      internal_error (__FILE__, __LINE__,
                      "ppc_gdbarch_init: bad byte order for float format");
    }

  set_gdbarch_push_dummy_call (gdbarch, ppc_darwin_abi_push_dummy_call);
  set_gdbarch_frame_align (gdbarch, ppc_macosx_frame_align);
  set_gdbarch_frame_red_zone_size (gdbarch, 224);
  
  set_gdbarch_stab_reg_to_regnum (gdbarch, ppc_macosx_stab_reg_to_regnum);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, rs6000_dwarf2_reg_to_regnum);

  set_gdbarch_return_value (gdbarch, ppc_darwin_abi_return_value);

  set_gdbarch_deprecated_use_struct_convention (gdbarch, ppc_use_struct_convention);

  set_gdbarch_skip_prologue (gdbarch, ppc_skip_prologue);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_decr_pc_after_break (gdbarch, 0);
  set_gdbarch_deprecated_function_start_offset (gdbarch, 0);
  set_gdbarch_breakpoint_from_pc (gdbarch, ppc_breakpoint_from_pc);

  set_gdbarch_unwind_dummy_id (gdbarch, ppc_unwind_dummy_id);

  set_gdbarch_unwind_pc (gdbarch, ppc_unwind_pc);

  set_gdbarch_frame_args_skip (gdbarch, 0);

  set_gdbarch_skip_trampoline_code (gdbarch, macosx_skip_trampoline_code);

  set_gdbarch_in_solib_return_trampoline (gdbarch,
                                          macosx_in_solib_return_trampoline);

  set_gdbarch_print_insn (gdbarch, print_insn_big_powerpc);

  set_gdbarch_register_reggroup_p (gdbarch, ppc_register_reggroup_p);

  frame_unwind_append_sniffer (gdbarch, ppc_sigtramp_frame_sniffer);
  frame_unwind_append_sniffer (gdbarch, ppc_frame_sniffer);

  set_gdbarch_integer_to_address (gdbarch, ppc_integer_to_address);

  set_gdbarch_fetch_pointer_argument (gdbarch, ppc_fetch_pointer_argument);

  /* Hook in ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

  frame_base_set_default (gdbarch, &ppc_frame_base);

  return gdbarch;
}

/*
 * This is set to the FAST_COUNT_STACK macro for ppc.  The return value
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
ppc_fast_show_stack (unsigned int count_limit, unsigned int print_limit,
                     unsigned int *count,
                     void (print_fun) (struct ui_out * uiout, int frame_num,
                                       CORE_ADDR pc, CORE_ADDR fp))
{
  CORE_ADDR fp = 0;
  static CORE_ADDR sigtramp_start = 0;
  static CORE_ADDR sigtramp_end = 0;
  unsigned int i = 0;
  int err = 0;
  struct frame_info *fi;
  int more_frames = 1;
  ULONGEST prev_fp = 0;
  ULONGEST next_fp = 0;
  ULONGEST pc = 0;
  int wordsize = gdbarch_tdep (current_gdbarch)->wordsize;
  
  more_frames = fast_show_stack_trace_prologue (count_limit, print_limit, wordsize,
						&sigtramp_start, &sigtramp_end,
						&i, &fi, print_fun);

  if (more_frames < 0)
    {
      err = 1;
      goto ppc_count_finish;
    }

  if (i >= count_limit || !more_frames)
    goto ppc_count_finish;

  /* There's a complication with PPC.  We want to report the frame pointer, because
     that stays constant through the lifetime of the function, so it is the good
     "fingerprint" for the function.  But on MacOS X, the frame pointer is not
     guaranteed to point to the previous frame's address, sometimes that gets
     overwritten.  The STACK POINTER is what consistently points to the previous
     frame's address.  So use that for dereferencing.  */

  fp = get_frame_sp (fi);
  if (!safe_read_memory_unsigned_integer (fp, wordsize, &next_fp))
    goto ppc_count_finish;

  while (1)
    {
      prev_fp = fp;
      if ((sigtramp_start <= pc) && (pc <= sigtramp_end))
        {
          fp = next_fp + 0x70 + 0xc;
          if (!safe_read_memory_unsigned_integer (fp, wordsize, &next_fp))
            goto ppc_count_finish;
	  /* FIXME need to get pc from prev_fp */
          if (!safe_read_memory_unsigned_integer (fp - 0xc, wordsize, &pc))
            goto ppc_count_finish;
          fp = next_fp;
          if (!safe_read_memory_unsigned_integer (fp, wordsize, &next_fp))
            goto ppc_count_finish;
        }
      else
        {
          fp = next_fp;
          if (!safe_read_memory_unsigned_integer (fp, wordsize, &next_fp))
            goto ppc_count_finish;
          if (next_fp == 0)
            goto ppc_count_finish;
	  else if (fp == next_fp)
	    {
	      /* This shouldn't ever happen, but if it does we will
		 loop forever here, so protect against that.  */
	      warning ("Frame pointer point back at the previous frame");
	      err = 1;
	      goto ppc_count_finish;
	    }
          if (!safe_read_memory_unsigned_integer
              (fp + PPC_MACOSX_DEFAULT_LR_SAVE * wordsize, wordsize, &pc))
            goto ppc_count_finish;
        }

      /* Let's raise the load level here.  That will mean that if we are 
	 going to print the names, they will be accurate.  Also, it means
	 if the main executable has it's load-state lowered, we'll detect
	 main correctly.  */
      
      pc_set_load_state (pc, OBJF_SYM_ALL, 0);

      if (print_fun && (i < print_limit))
        print_fun (uiout, i, pc, fp);
      i++;

      if (!backtrace_past_main && addr_inside_main_func (pc))
        goto ppc_count_finish;

      if (i >= count_limit)
        goto ppc_count_finish;
    }

ppc_count_finish:
  if (print_fun)
    ui_out_end (uiout, ui_out_type_list);

  *count = i;
  return (!err);
}

/* Grub around in the argument list to find the exception object,
   and return the type info string (without the "typeinfo for " bits).
   CURR_FRAME is the __cxa_throw frame.
   NOTE: We are getting the mangled name of the typeinfo object, and
   demangling that.  We could instead look inside the object, and pull
   out the string description field, but then we have to know where this
   is in the typeinfo object, or call a function.  Getting the mangled
   name seems much safer & easier.
*/

char *
ppc_throw_catch_find_typeinfo (struct frame_info *curr_frame,
                               int exception_type)
{
  struct minimal_symbol *typeinfo_sym = NULL;
  ULONGEST typeinfo_ptr;
  char *typeinfo_str;

  if (exception_type == EX_EVENT_THROW)
    {
      frame_unwind_unsigned_register (curr_frame,
                                      PPC_MACOSX_FIRST_GP_REGNUM + 4,
                                      &typeinfo_ptr);
      typeinfo_sym = lookup_minimal_symbol_by_pc (typeinfo_ptr);

    }
  else
    {
      /* This is hacky, the runtime code gets a pointer to an _Unwind_Exception,
         which is actually contained in the __cxa_exception that we want.  But
         the function that does the cast is a static inline, so we can't see it.
         FIXME: we need to get the runtime to keep this so we aren't relying on
         the particular layout of the __cxa_exception...
         Anyway, then the first field of __cxa_exception is the type object. */
      ULONGEST type_obj_addr = 0;

      frame_unwind_unsigned_register (curr_frame,
                                      PPC_MACOSX_FIRST_GP_REGNUM + 3,
                                      &typeinfo_ptr);

      /* This is also a bit bogus.  We assume that an unsigned integer is the
         same size as an address on our system.  */
      if (safe_read_memory_unsigned_integer
          (typeinfo_ptr - 48, 4, &type_obj_addr))
        typeinfo_sym = lookup_minimal_symbol_by_pc (type_obj_addr);
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

/* If the type is larger than the wordsize for GP registers, we have to
   be careful to copy only the lower halves of the register values.  We
   have to support wordsize = 4, register size = 8 for 32 bit apps
   on G5's...  */

static int
ppc_macosx_convert_register_p (int regno, struct type *type)
{
  int wordsize = gdbarch_tdep (current_gdbarch)->wordsize;
  if (PPC_MACOSX_IS_GP_REGNUM (regno)
      && (TYPE_LENGTH (type) > wordsize)
      && (register_size (current_gdbarch, regno) > wordsize))
    return 1;
  else
    return 0;
}

static void
ppc_macosx_value_to_register (struct frame_info *frame, int regno,
                              struct type *type, const gdb_byte *buf)
{
  int len = TYPE_LENGTH (type);
  int wordsize = gdbarch_tdep (current_gdbarch)->wordsize;
  int reg_size;
  int upper_half_size;
  int num_regs;
  int i;
  gdb_byte reg_buf[8] = { 0 };


  if (len <= wordsize)
    {
      rs6000_value_to_register (frame, regno, type, buf);
      return;
    }

  reg_size = register_size (current_gdbarch, regno);
  upper_half_size = reg_size - wordsize;
  num_regs = len / wordsize;

  for (i = 0; i < num_regs; i++)
    {
      memcpy (reg_buf + upper_half_size, ((bfd_byte *) buf) + i * wordsize,
              wordsize);
      put_frame_register (frame, regno + i, reg_buf);
    }
}

static void
ppc_macosx_register_to_value (struct frame_info *frame, int regno,
                              struct type *type, gdb_byte *buf)
{
  int len = TYPE_LENGTH (type);
  int wordsize = gdbarch_tdep (current_gdbarch)->wordsize;
  int reg_size;
  int upper_half_size;
  int num_regs;
  int i;

  if (len <= wordsize)
    {
      rs6000_register_to_value (frame, regno, type, buf);
      return;
    }

  reg_size = register_size (current_gdbarch, regno);
  upper_half_size = reg_size - wordsize;
  num_regs = len / wordsize;

  for (i = 0; i < num_regs; i++)
    {
      gdb_byte reg_buf[8];

      get_frame_register (frame, regno + i, reg_buf);
      memcpy (((bfd_byte *) buf) + i * wordsize, reg_buf + upper_half_size,
              wordsize);
    }

}

static void
ppc_macosx_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  set_gdbarch_get_longjmp_target (gdbarch, ppc_macosx_get_longjmp_target);
  set_gdbarch_register_to_value (gdbarch, ppc_macosx_register_to_value);
  set_gdbarch_value_to_register (gdbarch, ppc_macosx_value_to_register);
  set_gdbarch_convert_register_p (gdbarch, ppc_macosx_convert_register_p);
  /* APPLE LOCAL: Replace built in fp user reg read callback.
     The ppc frame information knows if a frame uses a frame pointer and if
     so which register it actually is in. Overriding this allows us to modify
     the frame pointer intelligently using "print $fp = <expr>"  */
  user_reg_replace (gdbarch, "fp", ppc_value_of_builtin_frame_fp_reg);

}

static void
ppc_macosx_init_abi_64 (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->wordsize = 8;

  set_gdbarch_addr_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_ptr_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_short_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_int_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_long_long_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_float_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_long_double_bit (gdbarch, 16 * TARGET_CHAR_BIT);

  set_gdbarch_push_dummy_call (gdbarch, ppc64_darwin_abi_push_dummy_call);
  set_gdbarch_return_value (gdbarch, ppc64_darwin_abi_return_value);
  set_gdbarch_frame_red_zone_size (gdbarch, 288);
  set_gdbarch_frame_align (gdbarch, ppc_macosx_frame_align);
  
  set_gdbarch_get_longjmp_target (gdbarch, ppc_64_macosx_get_longjmp_target);
  /* APPLE LOCAL: Replace built in fp user reg read callback.
     The ppc frame information knows if a frame uses a frame pointer and if
     so which register it actually is in. Overriding this allows us to modify
     the frame pointer intelligently using "print $fp = <expr>"  */
  user_reg_replace (gdbarch, "fp", ppc_value_of_builtin_frame_fp_reg);
}

static int
ppc_mach_o_query_64bit ()
{
  host_basic_info_data_t info;
  mach_msg_type_number_t count;

  count = HOST_BASIC_INFO_COUNT;
  host_info (mach_host_self (), HOST_BASIC_INFO, (host_info_t) & info,
             &count);

  return (info.cpu_type == CPU_TYPE_POWERPC &&
          info.cpu_subtype == CPU_SUBTYPE_POWERPC_970);
}

/* Two functions in one!  If this is a fat file (bfd_archive with
   target name mach-o-fat) recurse for each separate fork of the fat
   file.  If this is not a fat file, detect whether the file is ppc32
   or ppc64.  Before either of these, check if we've already sniffed
   an appropriate OSABI from dyld (in the case of attaching to a
   process) and prefer that.  */

static enum gdb_osabi
ppc_mach_o_osabi_sniffer (bfd *abfd)
{
  return generic_mach_o_osabi_sniffer (abfd,
				       bfd_arch_powerpc,
				       bfd_mach_ppc, bfd_mach_ppc64,
				       ppc_mach_o_query_64bit);
}

#define PPC_JMP_LR 0x54
#define PPC_64_JMP_LR 0xa8

static int
ppc_macosx_get_longjmp_target_helper (unsigned int offset, CORE_ADDR *pc)
{
  CORE_ADDR jmp_buf;
  ULONGEST long_addr = 0;

  /* The first argument to longjmp (in $r3) is the pointer to the
     jump buf.  The stored lr there is offset by PPC_JMP_LR as
     given above.  */

  jmp_buf = read_register (PPC_MACOSX_FIRST_GP_REGNUM + 3);

  if (safe_read_memory_unsigned_integer
      (jmp_buf + offset, TARGET_PTR_BIT / 8, &long_addr))
    {
      *pc = long_addr;
      return 1;
    }
  else
    return 0;
}

static int
ppc_64_macosx_get_longjmp_target (CORE_ADDR *pc)
{
  return ppc_macosx_get_longjmp_target_helper (PPC_64_JMP_LR, pc);
}

static int
ppc_macosx_get_longjmp_target (CORE_ADDR *pc)
{
  return ppc_macosx_get_longjmp_target_helper (PPC_JMP_LR, pc);
}

/* Callback function for user_reg_replace (). This function will get the
   current value of the frame pointer for the current frame based off of
   the ppc specific frame information. If the frame uses a frame register
   other than the SP, it will return that register, otherwise it will return
   the SP value. This allows modification of the ppc frame pointer from
   expressions using the "print $fp = <expr>" format.  */
static struct value *
ppc_value_of_builtin_frame_fp_reg (struct frame_info *frame)
{
  CORE_ADDR frame_pc;
  ppc_function_properties *props;
  /* Get the ppc specific frame information for this frame.  */
  props = ppc_frame_function_properties (frame_next_hack (frame), 
					 frame_cache_hack (frame));
  if (props)
    {
      /* Get the current frame PC and make sure that our pc is 
         beyond the instruction that sets up the frame pointer register.  */
      frame_pc = get_frame_pc (frame);

      if (props->frameptr_used && props->frameptr_reg > 0
	 && props->frameptr_pc != INVALID_ADDRESS
	 && props->frameptr_pc <= frame_pc)
	{
	  /* We are using a Frame Pointer register other than
	     the stack pointer, return its value.  */
	  return value_of_register (props->frameptr_reg, frame);
	}
    }
  /* Default to using the SP as our frame pointer if we don't find
     any information to the contrary.  */
  return value_of_register (PPC_MACOSX_SP_REGNUM, frame);
}

void
_initialize_ppc_tdep ()
{
  register_gdbarch_init (bfd_arch_powerpc, ppc_gdbarch_init);

  gdbarch_register_osabi_sniffer (bfd_arch_unknown, bfd_target_mach_o_flavour,
                                  ppc_mach_o_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_powerpc, 0, GDB_OSABI_DARWIN,
                          ppc_macosx_init_abi);

  gdbarch_register_osabi (bfd_arch_powerpc, bfd_mach_ppc64,
                          GDB_OSABI_DARWIN64, ppc_macosx_init_abi_64);

  add_setshow_boolean_cmd ("ppc", class_obscure,
			   &ppc_debugflag, _("\
Set if printing PPC stack analysis debugging statements."), _("\
Show if printing PPC stack analysis debugging statements."), NULL,
			   NULL, NULL,
			   &setdebuglist, &showdebuglist);

  add_setshow_uinteger_cmd ("ppc-maximum-frame-size", class_obscure,
			    &ppc_max_frame_size, _("\
Set the maximum size to expect for a valid PPC frame."), _("\
Show the maximum size to expect for a valid PPC frame."), NULL,
			    NULL, NULL,
			    &setlist, &showlist);
}
