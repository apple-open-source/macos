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

#include "ppc-macosx-tdep.h"
#include "ppc-macosx-regs.h"
#include "ppc-macosx-regnums.h"
#include "ppc-macosx-tdep.h"
#include "ppc-macosx-frameinfo.h"

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

#include "libbfd.h"

#include "elf-bfd.h"
#include "dis-asm.h"
#include "ppc-tdep.h"
#include "gdbarch.h"
#include "osabi.h"

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <mach/machine.h>

extern int backtrace_past_main;

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
static enum gdb_osabi ppc_mach_o_osabi_sniffer_use_dyld_hint (bfd *abfd);

/* When we're doing native debugging, and we attach to a process,
   we start out by finding the in-memory dyld -- the osabi of that
   dyld is stashed away here for use when picking the right osabi of
   a fat file.  In the case of cross-debugging, none of this happens
   and this global remains untouched.  */

enum gdb_osabi osabi_seen_in_attached_dyld = GDB_OSABI_UNKNOWN;

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

static struct ppc_frame_cache *
ppc_alloc_frame_cache (void)
{
  struct ppc_frame_cache *cache;
  int i;

  cache = FRAME_OBSTACK_ZALLOC (struct ppc_frame_cache);
  cache->saved_regs =
    (CORE_ADDR *) frame_obstack_zalloc ((NUM_REGS) * sizeof (CORE_ADDR));

  cache->prev_pc = (CORE_ADDR) - 1;
  cache->prev_sp = (CORE_ADDR) - 1;

  cache->stack = (CORE_ADDR) - 1;
  cache->frame = (CORE_ADDR) - 1;
  cache->pc = (CORE_ADDR) - 1;

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
    return *this_cache;

  cache = ppc_alloc_frame_cache ();
  *this_cache = cache;

  cache->stack = frame_unwind_register_unsigned (next_frame, SP_REGNUM);
  if (cache->stack == 0)
    return cache;

  cache->frame = ppc_frame_find_prev_fp (next_frame, this_cache);

  cache->pc = frame_func_unwind (next_frame);

  return cache;
}

/* function implementations */

void
ppc_print_extra_frame_info (struct frame_info *next_frame, void **this_cache)
{
  struct ppc_frame_cache *cache;
  struct ppc_function_boundaries *bounds;
  struct ppc_function_properties *props;

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

static struct frame_id
ppc_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  char buf[8];
  CORE_ADDR fp;

  frame_unwind_register (next_frame, SP_REGNUM, buf);
  fp =
    extract_unsigned_integer (buf,
                              register_size (current_gdbarch, SP_REGNUM));

  return frame_id_build (fp, frame_pc_unwind (next_frame));
}

CORE_ADDR
ppc_frame_find_prev_pc (struct frame_info * next_frame, void **this_cache)
{

  struct ppc_frame_cache *cache;
  ppc_function_properties *props = NULL;
  CORE_ADDR prev;

  cache = ppc_frame_cache (next_frame, this_cache);

  props = ppc_frame_function_properties (next_frame, this_cache);
  if (props == NULL)
    return 0;

  prev = ppc_frame_find_prev_sp (next_frame, this_cache);

  if ((props->lr_saved) && (props->lr_saved < frame_pc_unwind (next_frame)))
    {
      return read_memory_unsigned_integer (prev + props->lr_offset,
                                           gdbarch_addr_bit (current_gdbarch)
                                           / 8);
    }
  else if ((props->lr_reg >= 0) &&
           (props->lr_invalid) &&
           (frame_pc_unwind (next_frame) > props->lr_invalid) &&
           (frame_pc_unwind (next_frame) <= props->lr_valid_again))
    {
      return frame_unwind_register_unsigned (next_frame, props->lr_reg);
    }
  else
    {
      return frame_unwind_register_unsigned (next_frame,
                                             PPC_MACOSX_LR_REGNUM);
    }
}

CORE_ADDR
ppc_frame_find_prev_fp (struct frame_info * next_frame, void **this_cache)
{
  struct ppc_frame_cache *cache;
  ppc_function_properties *props;
  CORE_ADDR prev_fp;

  cache = ppc_frame_cache (next_frame, this_cache);

  props = ppc_frame_function_properties (next_frame, this_cache);
  if (props == NULL)
    return frame_unwind_register_unsigned (next_frame, SP_REGNUM);

  if (props->frameptr_used && (props->frameptr_reg > 0))
    {
      /* Be a little bit careful here.  We are trying to unwind the frameptr register,
         but we might get it wrong (for instance because we mis-parsed a prolog and
         got the stored location wrong.)  So if the fp value looks ridiculous, fall
         back on the SP value - which we seem to get right more consistently... */

      CORE_ADDR this_sp = get_frame_register_unsigned (next_frame, SP_REGNUM);
      prev_fp = frame_unwind_register_unsigned (next_frame, props->frameptr_reg);
      if (prev_fp >= this_sp && prev_fp - this_sp < 0xfffff)
          return prev_fp;
    }

  return frame_unwind_register_unsigned (next_frame, SP_REGNUM);
}

CORE_ADDR
ppc_frame_find_prev_sp (struct frame_info * next_frame, void **this_cache)
{
  ppc_function_properties *props = NULL;
  CORE_ADDR sp;

  sp = frame_unwind_register_unsigned (next_frame, SP_REGNUM);
  if (sp == 0)
    return 0;

  props = ppc_frame_function_properties (next_frame, this_cache);
  if (props == NULL)
    return 0;

  if (props->frameless)
    return sp;

  return read_memory_unsigned_integer (sp,
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
ppc_register_virtual_type (int n)
{
  if (((n >= PPC_MACOSX_FIRST_GP_REGNUM) && (n <= PPC_MACOSX_LAST_GP_REGNUM))
      || (n == PPC_MACOSX_PC_REGNUM)
      || (n == PPC_MACOSX_PS_REGNUM)
      || (n == PPC_MACOSX_LR_REGNUM)
      || (n == PPC_MACOSX_CTR_REGNUM) || (n == PPC_MACOSX_XER_REGNUM))
    {
      /* I think it's okay to always treat registers as long long.  We always use
         the 64 bit calls even on G4 systems, and let the system cut this down to 32
         bits.  */
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

  return cache->frame;
}

static CORE_ADDR
ppc_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, PC_REGNUM);
}

static void
ppc_frame_prev_register (struct frame_info *next_frame, void **this_cache,
                         int regnum, int *optimizedp,
                         enum lval_type *lvalp, CORE_ADDR * addrp,
                         int *realnump, void *valuep)
{
  struct ppc_frame_cache *cache = ppc_frame_cache (next_frame, this_cache);
  CORE_ADDR *saved_regs = NULL;

  if (regnum == SP_REGNUM)
    {
      *optimizedp = 0;
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
      *optimizedp = 0;
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
      *optimizedp = 0;
      *lvalp = lval_memory;
      *addrp = cache->saved_regs[regnum];
      *realnump = -1;
      if (valuep)
        {
          int wordsize = (gdbarch_tdep (current_gdbarch))->wordsize;
          int offset = register_size (current_gdbarch, regnum) - wordsize;
          *((int *) valuep) = 0;

          read_memory (*addrp, ((char *) valuep) + offset,
                       wordsize);
        }
      return;
    }

  frame_register_unwind (next_frame, regnum,
                         optimizedp, lvalp, addrp, realnump, valuep);
}

static void
ppc_frame_this_id (struct frame_info *next_frame, void **this_cache,
                   struct frame_id *this_id)
{
  struct ppc_frame_cache *cache = ppc_frame_cache (next_frame, this_cache);

  if (cache->stack == 0)
    {
      *this_id = null_frame_id;
      return;
    }
  else
    {
      ULONGEST prev_frame_addr = 0;
      if (safe_read_memory_unsigned_integer
          (cache->stack, TARGET_PTR_BIT / 8, &prev_frame_addr))
        {
          if (safe_read_memory_unsigned_integer
              (prev_frame_addr, TARGET_PTR_BIT / 8, &prev_frame_addr))
            if (prev_frame_addr == 0)
              {
                *this_id = null_frame_id;
                return;
              }
        }
    }

  (*this_id) = frame_id_build (cache->frame, cache->pc);
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

  cache->stack = frame_unwind_register_unsigned (next_frame, SP_REGNUM);
  if (cache->stack == 0)
    return cache;

  cache->pc = frame_func_unwind (next_frame);
  cache->frame = cache->stack;

  sigframe = read_memory_unsigned_integer (cache->stack, TARGET_PTR_BIT / 8);
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
  (*this_id) = frame_id_build (cache->frame, frame_pc_unwind (next_frame));
}

static void
ppc_sigtramp_frame_prev_register (struct frame_info *next_frame,
                                  void **this_cache,
                                  int regnum, int *optimizedp,
                                  enum lval_type *lvalp, CORE_ADDR * addrp,
                                  int *realnump, void *valuep)
{
  struct ppc_frame_cache *cache =
    ppc_sigtramp_frame_cache (next_frame, this_cache);
  CORE_ADDR *saved_regs = NULL;

  saved_regs = ppc_frame_saved_regs (next_frame, this_cache);

  if ((saved_regs != NULL) && (saved_regs[regnum] != -1))
    {
      int size;
      int offset = 0;

      *optimizedp = 0;
      *lvalp = lval_memory;
      *addrp = cache->saved_regs[regnum];
      *realnump = -1;
      if (valuep)
        {
          int reg_size = register_size (current_gdbarch, regnum);
          if (regnum == PPC_MACOSX_CR_REGNUM)
            {
              size = 4;
              offset = reg_size - size;
            }
          else if (PPC_MACOSX_IS_GP_REGNUM (regnum) || PPC_MACOSX_IS_GSP_REGNUM (regnum))
            {
              size = cache->sigtramp_gp_store_size;
              offset = reg_size - size;
            }
          else
            {
              size = reg_size;
              offset = 0;
            }

          if (reg_size > size)
            bzero ((char *) valuep, reg_size - size);

          read_memory (*addrp, ((char *) valuep) + offset, size);
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

  find_pc_partial_function (pc, &name, NULL, NULL);
  if (PC_IN_SIGTRAMP (pc, name))
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
ppc_integer_to_address (struct type *type, void *buf)
{
  char *tmp = alloca (TYPE_LENGTH (builtin_type_void_data_ptr));
  LONGEST val = unpack_long (type, buf);
  store_unsigned_integer (tmp, TYPE_LENGTH (builtin_type_void_data_ptr), val);
  return extract_unsigned_integer (tmp,
                                   TYPE_LENGTH (builtin_type_void_data_ptr));
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

  tdep->wordsize = 4;
  tdep->ppc_lr_regnum = PPC_MACOSX_LR_REGNUM;
  tdep->ppc_gp0_regnum = PPC_MACOSX_FIRST_GP_REGNUM;

  set_gdbarch_num_regs (gdbarch, PPC_MACOSX_NUM_REGS);
  set_gdbarch_sp_regnum (gdbarch, PPC_MACOSX_SP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, PPC_MACOSX_PC_REGNUM);
  set_gdbarch_ps_regnum (gdbarch, PPC_MACOSX_PS_REGNUM);
  set_gdbarch_fp0_regnum (gdbarch, PPC_MACOSX_FIRST_FP_REGNUM);

  set_gdbarch_register_name (gdbarch, ppc_register_name);
  set_gdbarch_deprecated_max_register_raw_size (gdbarch, 16);
  set_gdbarch_deprecated_max_register_virtual_size (gdbarch, 16);
  set_gdbarch_deprecated_register_virtual_type (gdbarch,
                                                ppc_register_virtual_type);

  set_gdbarch_addr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_ptr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_short_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_int_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_long_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_float_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_long_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);

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

  set_gdbarch_stab_reg_to_regnum (gdbarch, ppc_macosx_stab_reg_to_regnum);

  set_gdbarch_return_value (gdbarch, ppc_darwin_abi_return_value);

  set_gdbarch_use_struct_convention (gdbarch, ppc_use_struct_convention);

  set_gdbarch_skip_prologue (gdbarch, ppc_skip_prologue);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_decr_pc_after_break (gdbarch, 0);
  set_gdbarch_function_start_offset (gdbarch, 0);
  set_gdbarch_breakpoint_from_pc (gdbarch, ppc_breakpoint_from_pc);

  set_gdbarch_unwind_dummy_id (gdbarch, ppc_unwind_dummy_id);

  set_gdbarch_unwind_pc (gdbarch, ppc_unwind_pc);

  set_gdbarch_frame_args_skip (gdbarch, 0);

  set_gdbarch_skip_trampoline_code (gdbarch, ppc_macosx_skip_trampoline_code);
  set_gdbarch_dynamic_trampoline_nextpc (gdbarch,
                                         ppc_macosx_dynamic_trampoline_nextpc);

  set_gdbarch_in_solib_call_trampoline (gdbarch,
                                        ppc_macosx_in_solib_call_trampoline);
  set_gdbarch_in_solib_return_trampoline (gdbarch,
                                          ppc_macosx_in_solib_return_trampoline);

  set_gdbarch_print_insn (gdbarch, print_insn_big_powerpc);

  set_gdbarch_register_reggroup_p (gdbarch, ppc_register_reggroup_p);

  frame_unwind_append_sniffer (gdbarch, ppc_sigtramp_frame_sniffer);
  frame_unwind_append_sniffer (gdbarch, ppc_frame_sniffer);

  set_gdbarch_integer_to_address (gdbarch, ppc_integer_to_address);

  set_gdbarch_fetch_pointer_argument (gdbarch, ppc_fetch_pointer_argument);

  set_gdbarch_deprecated_print_extra_frame_info (gdbarch,
                                                 ppc_print_extra_frame_info);

  /* Hook in ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

  frame_base_set_default (gdbarch, &ppc_frame_base);

  return gdbarch;
}

/*
 * This is set to the FAST_COUNT_STACK macro for ppc.  The return value
 * is 1 if no errors were encountered traversing the stack, and 0 otherwise.
 * it sets count to the stack depth.  If SHOW_FRAMES is 1, then it also
 * emits a list of frame info bits, with the pc & fp for each frame to
 * the current UI_OUT.  If GET_NAMES is 1, it also emits the names for
 * each frame (though this slows the function a good bit.)
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
ppc_fast_show_stack (int show_frames, int get_names,
                     unsigned int count_limit, unsigned int print_limit,
                     unsigned int *count,
                     void (print_fun) (struct ui_out * uiout, int frame_num,
                                       CORE_ADDR pc, CORE_ADDR fp))
{
  CORE_ADDR fp = 0;
  static CORE_ADDR sigtramp_start = 0;
  static CORE_ADDR sigtramp_end = 0;
  struct frame_info *fi = NULL;
  int i = 0;
  int err = 0;
  ULONGEST next_fp = 0;
  ULONGEST pc = 0;

  if (sigtramp_start == 0)
    {
      char *name;
      struct minimal_symbol *msymbol;

      msymbol = lookup_minimal_symbol ("_sigtramp", NULL, NULL);
      if (msymbol == NULL)
        warning
          ("Couldn't find minimal symbol for \"_sigtramp\" - backtraces may be unreliable");
      else
        {
          pc = SYMBOL_VALUE_ADDRESS (msymbol);
          if (find_pc_partial_function (pc, &name,
                                        &sigtramp_start, &sigtramp_end) == 0)
            {
              error
                ("Couldn't find _sigtramp symbol -- backtraces will be unreliable");
            }
        }
    }

  /* Get the first two frames.  If anything funky is going on, it will
     be here.  The second frame helps us get above frameless functions
     called from signal handlers.  Above these frames we have to deal
     with sigtramps and alloca frames, that is about all. */

  if (show_frames)
    ui_out_begin (uiout, ui_out_type_list, "frames");

  i = 0;
  if (i >= count_limit)
    goto ppc_count_finish;

  fi = get_current_frame ();
  if (fi == NULL)
    {
      err = 1;
      goto ppc_count_finish;
    }

  if (show_frames && print_fun && (i < print_limit))
    print_fun (uiout, i, get_frame_pc (fi), get_frame_base (fi));
  i = 1;

  do
    {
      if (i >= count_limit)
        goto ppc_count_finish;

      fi = get_prev_frame (fi);
      if (fi == NULL)
        goto ppc_count_finish;

      pc = get_frame_pc (fi);
      fp = get_frame_base (fi);

      if (show_frames && print_fun && (i < print_limit))
        print_fun (uiout, i, pc, fp);

      i++;

      if (!backtrace_past_main && inside_main_func (pc))
        goto ppc_count_finish;
    }
  while (i < 5);

  if (!safe_read_memory_unsigned_integer (fp, 4, &next_fp))
    goto ppc_count_finish;

  if (i >= count_limit)
    goto ppc_count_finish;

  while (1)
    {
      if ((sigtramp_start <= pc) && (pc <= sigtramp_end))
        {
          fp = next_fp + 0x70 + 0xc;
          if (!safe_read_memory_unsigned_integer (fp, 4, &next_fp))
            goto ppc_count_finish;
          if (!safe_read_memory_unsigned_integer (fp - 0xc, 4, &pc))
            goto ppc_count_finish;
          fp = next_fp;
          if (!safe_read_memory_unsigned_integer (fp, 4, &next_fp))
            goto ppc_count_finish;
        }
      else
        {
          fp = next_fp;
          if (!safe_read_memory_unsigned_integer (fp, 4, &next_fp))
            goto ppc_count_finish;
          if (next_fp == 0)
            goto ppc_count_finish;
          if (!safe_read_memory_unsigned_integer
              (fp + PPC_MACOSX_DEFAULT_LR_SAVE, 4, &pc))
            goto ppc_count_finish;
        }

      if (show_frames && print_fun && (i < print_limit))
        print_fun (uiout, i, pc, fp);
      i++;

      if (!backtrace_past_main && inside_main_func (pc))
        goto ppc_count_finish;

      if (i >= count_limit)
        goto ppc_count_finish;
    }

ppc_count_finish:
  if (show_frames)
    ui_out_end (uiout, ui_out_type_list);

  *count = i;
  return (!err);
}

CORE_ADDR
ppc_macosx_skip_trampoline_code (CORE_ADDR pc)
{
  CORE_ADDR newpc;

  newpc = dyld_symbol_stub_function_address (pc, NULL);
  if (newpc != 0)
    return newpc;

  newpc = decode_fix_and_continue_trampoline (pc);
  if (newpc != 0)
    return newpc;

  return 0;
}

CORE_ADDR
ppc_macosx_dynamic_trampoline_nextpc (CORE_ADDR pc)
{
  return dyld_symbol_stub_function_address (pc, NULL);
}

int
ppc_macosx_in_solib_return_trampoline (CORE_ADDR pc, char *name)
{
  return 0;
}

int
ppc_macosx_in_solib_call_trampoline (CORE_ADDR pc, char *name)
{
  if (ppc_macosx_skip_trampoline_code (pc) != 0)
    {
      return 1;
    }
  return 0;
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
                              struct type *type, const void *buf)
{
  int len = TYPE_LENGTH (type);
  int wordsize = gdbarch_tdep (current_gdbarch)->wordsize;
  int reg_size;
  int upper_half_size;
  int num_regs;
  int i;
  char reg_buf[8] = { 0 };


  if (len <= wordsize)
    {
      legacy_value_to_register (frame, regno, type, buf);
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
                              struct type *type, void *buf)
{
  int len = TYPE_LENGTH (type);
  int wordsize = gdbarch_tdep (current_gdbarch)->wordsize;
  int reg_size;
  int upper_half_size;
  int num_regs;
  int i;

  if (len <= wordsize)
    {
      legacy_register_to_value (frame, regno, type, buf);
      return;
    }

  reg_size = register_size (current_gdbarch, regno);
  upper_half_size = reg_size - wordsize;
  num_regs = len / wordsize;

  for (i = 0; i < num_regs; i++)
    {
      char reg_buf[8];

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
  set_gdbarch_long_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);

  set_gdbarch_push_dummy_call (gdbarch, ppc64_darwin_abi_push_dummy_call);
  set_gdbarch_return_value (gdbarch, ppc64_darwin_abi_return_value);

  set_gdbarch_get_longjmp_target (gdbarch, ppc_64_macosx_get_longjmp_target);
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

/* Two functions in one!  If this is a "bfd_archive" (read: a MachO fat file),
   recurse for each separate fork of the fat file.
   If this is not a fat file, detect whether the file is ppc32 or ppc64.
   Before either of these, check if we've already sniffed an appropriate
   OSABI from dyld (in the case of attaching to a process) and prefer that.  */

static enum gdb_osabi
ppc_mach_o_osabi_sniffer (bfd *abfd)
{
  enum gdb_osabi ret;
  ret = ppc_mach_o_osabi_sniffer_use_dyld_hint (abfd);
  if (ret == GDB_OSABI_DARWIN64 || ret == GDB_OSABI_DARWIN)
    return ret;

  if (bfd_check_format (abfd, bfd_archive))
    {
      enum gdb_osabi best = GDB_OSABI_UNKNOWN;
      enum gdb_osabi cur = GDB_OSABI_UNKNOWN;

      bfd *nbfd = NULL;
      for (;;)
        {
          nbfd = bfd_openr_next_archived_file (abfd, nbfd);

          if (nbfd == NULL)
            break;
          if (!bfd_check_format (nbfd, bfd_object))
            continue;

          cur = ppc_mach_o_osabi_sniffer (nbfd);
          if (cur == GDB_OSABI_DARWIN64 &&
              best != GDB_OSABI_DARWIN64 && ppc_mach_o_query_64bit ())
            best = cur;

          if (cur == GDB_OSABI_DARWIN &&
              best != GDB_OSABI_DARWIN64 && best != GDB_OSABI_DARWIN)
            best = cur;
        }
      return best;
    }

  if (!bfd_check_format (abfd, bfd_object))
    return GDB_OSABI_UNKNOWN;

  if (strcmp (bfd_get_target (abfd), "mach-o-be") == 0)
    {
      if (bfd_default_compatible (bfd_get_arch_info (abfd),
                                  bfd_lookup_arch (bfd_arch_powerpc,
                                                   bfd_mach_ppc64)))
        return GDB_OSABI_DARWIN64;

      if (bfd_default_compatible (bfd_get_arch_info (abfd),
                                  bfd_lookup_arch (bfd_arch_powerpc,
                                                   bfd_mach_ppc)))
        return GDB_OSABI_DARWIN;

      return GDB_OSABI_UNKNOWN;
    }

  return GDB_OSABI_UNKNOWN;
}

/* If we're attaching to a process, we start by finding the dyld that
   is loaded and go from there.  So when we're selecting the OSABI,
   prefer the osabi of the actually-loaded dyld when we can.  */

static enum gdb_osabi
ppc_mach_o_osabi_sniffer_use_dyld_hint (bfd *abfd)
{
  if (osabi_seen_in_attached_dyld == GDB_OSABI_UNKNOWN)
    return GDB_OSABI_UNKNOWN;

  bfd *nbfd = NULL;
  for (;;)
    {
      nbfd = bfd_openr_next_archived_file (abfd, nbfd);

      if (nbfd == NULL)
        break;
      if (!bfd_check_format (nbfd, bfd_object))
        continue;
      if (bfd_default_compatible (bfd_get_arch_info (nbfd),
                                  bfd_lookup_arch (bfd_arch_powerpc,
                                                   bfd_mach_ppc64))
          && osabi_seen_in_attached_dyld == GDB_OSABI_DARWIN64)
        return GDB_OSABI_DARWIN64;

      if (bfd_default_compatible (bfd_get_arch_info (nbfd),
                                  bfd_lookup_arch (bfd_arch_powerpc,
                                                   bfd_mach_ppc))
          && osabi_seen_in_attached_dyld == GDB_OSABI_DARWIN)
        return GDB_OSABI_DARWIN;
    }

  return GDB_OSABI_UNKNOWN;
}

#define PPC_JMP_LR 0x54
#define PPC_64_JMP_LR 0xa8

static int
ppc_macosx_get_longjmp_target_helper (unsigned int offset, CORE_ADDR * pc)
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
ppc_64_macosx_get_longjmp_target (CORE_ADDR * pc)
{
  return ppc_macosx_get_longjmp_target_helper (PPC_64_JMP_LR, pc);
}

static int
ppc_macosx_get_longjmp_target (CORE_ADDR * pc)
{
  return ppc_macosx_get_longjmp_target_helper (PPC_JMP_LR, pc);
}

void
_initialize_ppc_tdep ()
{
  struct cmd_list_element *cmd = NULL;

  register_gdbarch_init (bfd_arch_powerpc, ppc_gdbarch_init);

  gdbarch_register_osabi_sniffer (bfd_arch_unknown, bfd_target_mach_o_flavour,
                                  ppc_mach_o_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_powerpc, 0, GDB_OSABI_DARWIN,
                          ppc_macosx_init_abi);

  gdbarch_register_osabi (bfd_arch_powerpc, bfd_mach_ppc64,
                          GDB_OSABI_DARWIN64, ppc_macosx_init_abi_64);

  cmd = add_set_cmd ("ppc", class_obscure, var_boolean,
                     (char *) &ppc_debugflag,
                     "Set if printing PPC stack analysis debugging statements.",
                     &setdebuglist), add_show_from_set (cmd, &showdebuglist);

  cmd = add_set_cmd
    ("ppc-maximum-frame-size", class_obscure, var_uinteger,
     (char *) &ppc_max_frame_size,
     "Set the maximum size to expect for a valid PPC frame.", &setlist);
  add_show_from_set (cmd, &showlist);
}
