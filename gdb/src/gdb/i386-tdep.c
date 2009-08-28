/* Intel 386 target-dependent stuff.

   Copyright 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996,
   1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005 Free Software
   Foundation, Inc.

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
#include "arch-utils.h"
#include "command.h"
#include "dummy-frame.h"
#include "dwarf2-frame.h"
#include "doublest.h"
#include "floatformat.h"
#include "frame.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "inferior.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "objfiles.h"
#include "osabi.h"
#include "regcache.h"
#include "reggroups.h"
#include "regset.h"
#include "symfile.h"
#include "symtab.h"
#include "target.h"
#include "value.h"
#include "gdb_assert.h"
#include "reggroups.h"
#include "dummy-frame.h"
#include "osabi.h"

#include "bfd.h"
#include "elf-bfd.h"
#include "dis-asm.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "i386-tdep.h"
#include "i387-tdep.h"

#include "x86-shared-tdep.h"

/* APPLE LOCAL get the prototype for macosx_skip_trampoline_code */
#include "macosx-tdep.h"

/* Register names.  */

static char *i386_register_names[] =
{
  "eax",   "ecx",    "edx",   "ebx",
  "esp",   "ebp",    "esi",   "edi",
  "eip",   "eflags", "cs",    "ss",
  "ds",    "es",     "fs",    "gs",
  "st0",   "st1",    "st2",   "st3",
  "st4",   "st5",    "st6",   "st7",
  "fctrl", "fstat",  "ftag",  "fiseg",
  "fioff", "foseg",  "fooff", "fop",
  "xmm0",  "xmm1",   "xmm2",  "xmm3",
  "xmm4",  "xmm5",   "xmm6",  "xmm7",
  "mxcsr"
};

static const int i386_num_register_names = ARRAY_SIZE (i386_register_names);

/* Register names for MMX pseudo-registers.  */

static char *i386_mmx_names[] =
{
  "mm0", "mm1", "mm2", "mm3",
  "mm4", "mm5", "mm6", "mm7"
};

static const int i386_num_mmx_regs = ARRAY_SIZE (i386_mmx_names);

struct type *builtin_type_vec128i_big = NULL;

static int
i386_mmx_regnum_p (struct gdbarch *gdbarch, int regnum)
{
  int mm0_regnum = gdbarch_tdep (gdbarch)->mm0_regnum;

  if (mm0_regnum < 0)
    return 0;

  return (regnum >= mm0_regnum && regnum < mm0_regnum + i386_num_mmx_regs);
}

/* SSE register?  */

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

#define I387_ST0_REGNUM (gdbarch_tdep (current_gdbarch)->st0_regnum)
#define I387_MM0_REGNUM (gdbarch_tdep (current_gdbarch)->mm0_regnum)
#define I387_NUM_XMM_REGS (gdbarch_tdep (current_gdbarch)->num_xmm_regs)

/* FP register?  */

int
i386_fp_regnum_p (int regnum)
{
  if (I387_ST0_REGNUM < 0)
    return 0;

  return (I387_ST0_REGNUM <= regnum && regnum < I387_FCTRL_REGNUM);
}

int
i386_fpc_regnum_p (int regnum)
{
  if (I387_ST0_REGNUM < 0)
    return 0;

  return (I387_FCTRL_REGNUM <= regnum && regnum < I387_XMM0_REGNUM);
}

/* Return the name of register REGNUM.  */

const char *
i386_register_name (int regnum)
{
  if (i386_mmx_regnum_p (current_gdbarch, regnum))
    return i386_mmx_names[regnum - I387_MM0_REGNUM];

  if (regnum >= 0 && regnum < i386_num_register_names)
    return i386_register_names[regnum];

  return NULL;
}

/* Convert a dbx register number REG to the appropriate register
   number used by GDB.  */

static int
i386_dbx_reg_to_regnum (int reg)
{
  /* This implements what GCC calls the "default" register map
     (dbx_register_map[]).  */

  if (reg >= 0 && reg <= 7)
    {
      /* General-purpose registers.  The debug info calls %ebp
         register 4, and %esp register 5.  */
      if (reg == 4)
        return 5;
      else if (reg == 5)
        return 4;
      else return reg;
    }
  else if (reg >= 12 && reg <= 19)
    {
      /* Floating-point registers.  */
      return reg - 12 + I387_ST0_REGNUM;
    }
  else if (reg >= 21 && reg <= 28)
    {
      /* SSE registers.  */
      return reg - 21 + I387_XMM0_REGNUM;
    }
  else if (reg >= 29 && reg <= 36)
    {
      /* MMX registers.  */
      return reg - 29 + I387_MM0_REGNUM;
    }

  /* This will hopefully provoke a warning.  */
  return NUM_REGS + NUM_PSEUDO_REGS;
}

/* Convert SVR4 register number REG to the appropriate register number
   used by GDB.  */

static int
i386_svr4_reg_to_regnum (int reg)
{
  /* This implements the GCC register map that tries to be compatible
     with the SVR4 C compiler for DWARF (svr4_dbx_register_map[]).  */

  /* The SVR4 register numbering includes %eip and %eflags, and
     numbers the floating point registers differently.  */
  if (reg >= 0 && reg <= 9)
    {
      /* General-purpose registers.  */
      return reg;
    }
  else if (reg >= 11 && reg <= 18)
    {
      /* Floating-point registers.  */
      return reg - 11 + I387_ST0_REGNUM;
    }
  else if (reg >= 21)
    {
      /* The SSE and MMX registers have the same numbers as with dbx.  */
      return i386_dbx_reg_to_regnum (reg);
    }

  /* This will hopefully provoke a warning.  */
  return NUM_REGS + NUM_PSEUDO_REGS;
}

/* Translate a .eh_frame register to DWARF register 
   gcc uses its internal numbering scheme in the eh_frame register numbers
   or something like that... v. gcc/config/i386/darwin.h:DWARF2_FRAME_REG_OUT */

static int
i386_adjust_ehframe_regnum (struct gdbarch *gdbarch, int r, int ehframe_p)
{
  if (ehframe_p == 0)
    return r;

  if (r == 5)
    return 4;
  if (r == 4)
    return 5;
  if (r >= 12 && r <= 19)
    return r - 1;

  return r;
}

#undef I387_ST0_REGNUM
#undef I387_MM0_REGNUM
#undef I387_NUM_XMM_REGS


/* This is the variable that is set with "set disassembly-flavor", and
   its legitimate values.  */
static const char att_flavor[] = "att";
static const char intel_flavor[] = "intel";
static const char *valid_flavors[] =
{
  att_flavor,
  intel_flavor,
  NULL
};
static const char *disassembly_flavor = att_flavor;


/* Use the program counter to determine the contents and size of a
   breakpoint instruction.  Return a pointer to a string of bytes that
   encode a breakpoint instruction, store the length of the string in
   *LEN and optionally adjust *PC to point to the correct memory
   location for inserting the breakpoint.

   On the i386 we have a single breakpoint that fits in a single byte
   and can be inserted anywhere.

   This function is 64-bit safe.  */

static const gdb_byte *
i386_breakpoint_from_pc (CORE_ADDR *pc, int *len)
{
  static gdb_byte break_insn[] = { 0xcc }; /* int 3 */

  *len = sizeof (break_insn);
  return break_insn;
}

/* According to the System V ABI, the registers %ebp, %ebx, %edi, %esi
   and %esp "belong" to the calling function.  Therefore these
   registers should be saved if they're going to be modified.  */

/* The maximum number of saved registers.  This should include all
   registers mentioned above, and %eip.  */
#define I386_NUM_SAVED_REGS	I386_NUM_GREGS

/* APPLE LOCAL: Needed by Fix and Continue
   Returns 1 if it found a pic base being set up, 0 if it did not. 
   If PICBASE_ADDR is non-NULL, it will be set to the value of the 
   picbase in this function.  If PICBASE_REG is non-NULL, it will 
   be set to the register that contains the pic base. */

int
i386_find_picbase_setup (CORE_ADDR pc, CORE_ADDR *picbase_addr, 
                         enum i386_regnum *picbase_reg)
{
  int limit = 32;  /* number made up by me; 32 bytes is enough for a prologue */
  int skip = 0;
  int found_call_insn = 0;
  unsigned char op;

  if (picbase_addr != NULL)
    *picbase_addr = -1;

  while (skip < limit)
    {
      int length = length_of_this_instruction (pc + skip);
      /* Did we just find a CALL instruction?  It's probably our 
         picbase setup call.  */
      if (length == 5)
        {
          uint32_t buf = read_memory_unsigned_integer (pc + skip, 1);
          if (buf == 0xe8)  /* 0xe8 == CALL disp32 */
            {
              found_call_insn = 1;
              break;
            }
        }
      skip += length;
    }

  /* We've hit our limit without finding a `call rel32' or we've hit
     some unexpected instruction.  Give up the search.  */
  if (!found_call_insn)
    return 0;

  /* pc + skip is now pointing at the start of a `call rel32' instruction
     which may be setting up the picbase. */

  uint32_t rel32 = read_memory_unsigned_integer (pc + skip + 1, 4);
  uint32_t offset_from = pc + skip + 5;
  struct minimal_symbol *dest;

  /* Old-style picbase setup, jumping to the next instruction and popping
     the value into the picbase reg.  */
  if (rel32 == 0x0)
    {
      /* Check for `pop r' (opcode 0x58 + rd). */
      op = read_memory_unsigned_integer (offset_from, 1);
      if ((op & 0xf8) != 0x58)
        return 0;

      if (picbase_addr != NULL)
        *picbase_addr = offset_from;
      if (picbase_reg != NULL)
        *picbase_reg = (enum i386_regnum) op & 0x7;
      return 1;
    }

  dest = lookup_minimal_symbol_by_pc ((uint32_t) rel32 + offset_from);
  if (dest == NULL || SYMBOL_LINKAGE_NAME (dest) == NULL)
    return 0;

  /* We're looking for a call to one of the __i686.get_pc_thunk.ax functions,
     where "ax" indicates the addr will be in EAX.  These also are emitted for
     EBX, ECX, and who knows, maybe even EDX.  */

  /* NB: I originally wrote this with a single strcmp and a little character
     peeking but this is not (currently) a performance-sensitive function and
     checking for each variant is easier to read. */

  if (strcmp (SYMBOL_LINKAGE_NAME (dest), "__i686.get_pc_thunk.ax") == 0)
    {
      if (picbase_addr != NULL)
        *picbase_addr = offset_from;
      if (picbase_reg != NULL)
        *picbase_reg = I386_EAX_REGNUM;
      return 1;
    }
  if (strcmp (SYMBOL_LINKAGE_NAME (dest), "__i686.get_pc_thunk.bx") == 0)
    {
      if (picbase_addr != NULL)
        *picbase_addr = offset_from;
      if (picbase_reg != NULL)
        *picbase_reg = I386_EBX_REGNUM;
      return 1;
    }
  if (strcmp (SYMBOL_LINKAGE_NAME (dest), "__i686.get_pc_thunk.cx") == 0)
    {
      if (picbase_addr != NULL)
        *picbase_addr = offset_from;
      if (picbase_reg != NULL)
        *picbase_reg = I386_ECX_REGNUM;
      return 1;
    }
  if (strcmp (SYMBOL_LINKAGE_NAME (dest), "__i686.get_pc_thunk.dx") == 0)
    {
      if (picbase_addr != NULL)
        *picbase_addr = offset_from;
      if (picbase_reg != NULL)
        *picbase_reg = I386_EDX_REGNUM;
      return 1;
    }

  /* Nope, it must have been a CALL to something else; give up. */
  return 0;  
}

/* Return PC of first real instruction.  */

static CORE_ADDR
i386_skip_prologue (CORE_ADDR start_pc)
{
  struct x86_frame_cache cache;
  CORE_ADDR pc;
  CORE_ADDR endaddr;

  x86_initialize_frame_cache (&cache, 4);

  /* APPLE LOCAL: We may get an incorrect ENDADDR from find_pc_partial_function
     if there are multiple dylibs loading at address 0 - it's possible
     we'll end up the with endadr of another dylib's function instead of
     the one we're really trying to put a breakpoint in.  On the other hand
     when we do the instruction parsing we could be parsing the wrong dylib
     as well so we're no worse off.  It'd be nice if gdb would remember,
     when the user says "break open", which object_file's open() it chose
     instead of just giving us a pc value...  */
  if (find_pc_partial_function_no_inlined (start_pc, NULL, NULL, &endaddr) == 0)
    endaddr = start_pc + 512;  /* 512 bytes is more than enough.  */
  endaddr = refine_prologue_limit (start_pc, endaddr, 3);
  pc = x86_analyze_prologue (start_pc, endaddr, &cache);
  return pc;
}

/* This function is 64-bit safe.  */

static CORE_ADDR
i386_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  gdb_byte buf[8];

  frame_unwind_register (next_frame, PC_REGNUM, buf);
  return extract_typed_address (buf, builtin_type_void_func_ptr);
}

static const struct frame_unwind i386_frame_unwind =
{
  NORMAL_FRAME,
  x86_frame_this_id,
  x86_frame_prev_register,
  NULL,   /* const struct frame_data *unwind_data */
  NULL,   /* frame_sniffer_ftype *sniffer */
  NULL    /* frame_prev_pc_ftype *prev_pc */
};

static const struct frame_unwind *
i386_frame_sniffer (struct frame_info *next_frame)
{
  return &i386_frame_unwind;
}


/* Signal trampolines.  */

static struct x86_frame_cache *
i386_sigtramp_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct x86_frame_cache *cache;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  CORE_ADDR addr;
  gdb_byte buf[4];

  if (*this_cache)
    return *this_cache;

  cache = x86_alloc_frame_cache (4);

  frame_unwind_register (next_frame, I386_ESP_REGNUM, buf);
  cache->frame_base = extract_unsigned_integer (buf, 4);

  addr = tdep->sigcontext_addr (next_frame);
  if (tdep->sc_reg_offset)
    {
      int i;

      gdb_assert (tdep->sc_num_regs <= I386_NUM_SAVED_REGS);

      for (i = 0; i < tdep->sc_num_regs; i++)
	if (tdep->sc_reg_offset[i] != -1)
	  cache->saved_regs[i] = addr + tdep->sc_reg_offset[i];
    }
  else
    {
      cache->saved_regs[I386_EIP_REGNUM] = addr + tdep->sc_pc_offset;
      cache->saved_regs[I386_ESP_REGNUM] = addr + tdep->sc_sp_offset;
    }

  cache->saved_regs_are_absolute = 1;
  cache->prologue_scan_status = full_scan_succeeded;

  *this_cache = cache;
  return cache;
}

static void
i386_sigtramp_frame_this_id (struct frame_info *next_frame, void **this_cache,
			     struct frame_id *this_id)
{
  struct x86_frame_cache *cache =
    i386_sigtramp_frame_cache (next_frame, this_cache);

  /* See the end of i386_push_dummy_call.  */
  (*this_id) = frame_id_build (cache->frame_base + 4, frame_pc_unwind (next_frame));
}

static void
i386_sigtramp_frame_prev_register (struct frame_info *next_frame,
				   void **this_cache,
				   /* APPLE LOCAL variable opt states.  */
				   int regnum, enum opt_state *optimizedp,
				   enum lval_type *lvalp, CORE_ADDR *addrp,
				   int *realnump, gdb_byte *valuep)
{
  /* Make sure we've initialized the cache.  */
  i386_sigtramp_frame_cache (next_frame, this_cache);

  x86_frame_prev_register (next_frame, this_cache, regnum,
			   optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind i386_sigtramp_frame_unwind =
{
  SIGTRAMP_FRAME,
  i386_sigtramp_frame_this_id,
  i386_sigtramp_frame_prev_register,
  NULL,   /* const struct frame_data *unwind_data */
  NULL,   /* frame_sniffer_ftype *sniffer */
  NULL    /* frame_prev_pc_ftype *prev_pc */
};

static const struct frame_unwind *
i386_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (get_frame_arch (next_frame));

  /* We shouldn't even bother if we don't have a sigcontext_addr
     handler.  */
  if (tdep->sigcontext_addr == NULL)
    return NULL;

  if (tdep->sigtramp_p != NULL)
    {
      if (tdep->sigtramp_p (next_frame))
	return &i386_sigtramp_frame_unwind;
    }

  if (tdep->sigtramp_start != 0)
    {
      CORE_ADDR pc = frame_pc_unwind (next_frame);

      gdb_assert (tdep->sigtramp_end != 0);
      if (pc >= tdep->sigtramp_start && pc < tdep->sigtramp_end)
	return &i386_sigtramp_frame_unwind;
    }

  return NULL;
}


static CORE_ADDR
i386_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct x86_frame_cache *cache = x86_frame_cache (next_frame, this_cache, 4);
  x86_finalize_saved_reg_locations (next_frame, cache); 

  return cache->frame_base - 4;
}

static const struct frame_base i386_frame_base =
{
  &i386_frame_unwind,
  i386_frame_base_address,
  i386_frame_base_address,
  i386_frame_base_address
};

static struct frame_id
i386_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  gdb_byte buf[4];
  CORE_ADDR fp;

  frame_unwind_register (next_frame, I386_EBP_REGNUM, buf);
  fp = extract_unsigned_integer (buf, 4);

  /* See the end of i386_push_dummy_call.  */
  return frame_id_build (fp + 8, frame_pc_unwind (next_frame));
}


/* Figure out where the longjmp will land.  Slurp the args out of the
   stack.  We expect the first arg to be a pointer to the jmp_buf
   structure from which we extract the address that we will land at.
   This address is copied into PC.  This routine returns non-zero on
   success.

   This function is 64-bit safe.  */

static int
i386_get_longjmp_target (CORE_ADDR *pc)
{
  gdb_byte buf[8];
  CORE_ADDR sp, jb_addr;
  int jb_pc_offset = gdbarch_tdep (current_gdbarch)->jb_pc_offset;
  int len = TYPE_LENGTH (builtin_type_void_func_ptr);

  /* If JB_PC_OFFSET is -1, we have no way to find out where the
     longjmp will land.  */
  if (jb_pc_offset == -1)
    return 0;

  /* Don't use I386_ESP_REGNUM here, since this function is also used
     for AMD64.  */
  regcache_cooked_read (current_regcache, SP_REGNUM, buf);
  sp = extract_typed_address (buf, builtin_type_void_data_ptr);
  if (target_read_memory (sp + len, buf, len))
    return 0;

  jb_addr = extract_typed_address (buf, builtin_type_void_data_ptr);
  if (target_read_memory (jb_addr + jb_pc_offset, buf, len))
    return 0;

  *pc = extract_typed_address (buf, builtin_type_void_func_ptr);
  return 1;
}


static CORE_ADDR
i386_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		      struct regcache *regcache, CORE_ADDR bp_addr, int nargs,
		      struct value **args, CORE_ADDR sp, int struct_return,
		      CORE_ADDR struct_addr)
{
  gdb_byte buf[4];
  int i;
  int argument_bytes = 0;
  int alignment_pad_bytes;

  /* APPLE LOCAL begin */
  /* The only difference between the Darwin x86 ABI and
     generic x86 is that our stack pointer has to be 16-byte aligned
     at the point where the return address is pushed on to the
     stack.  So we calculate how many bytes the argument/struct
     return will take once we push them, then we move the stack
     pointer before we start pushing args.  */

  for (i = nargs - 1; i >= 0; i--)
    {
      int len = TYPE_LENGTH (value_enclosing_type (args[i]));
      argument_bytes += (len + 3) & ~3;
    }
  if (struct_return)
    argument_bytes += 4;

  alignment_pad_bytes = (sp - argument_bytes) - ((sp - argument_bytes) & ~15);
  sp -= alignment_pad_bytes;
  /* APPLE LOCAL end */

  /* Push arguments in reverse order.  */
  for (i = nargs - 1; i >= 0; i--)
    {
      int len = TYPE_LENGTH (value_enclosing_type (args[i]));

      /* The System V ABI says that:

	 "An argument's size is increased, if necessary, to make it a
	 multiple of [32-bit] words.  This may require tail padding,
	 depending on the size of the argument."

	 This makes sure the stack says word-aligned.  */
      sp -= (len + 3) & ~3;
      write_memory (sp, value_contents_all (args[i]), len);
    }

  /* Push value address.  */
  if (struct_return)
    {
      sp -= 4;
      store_unsigned_integer (buf, 4, struct_addr);
      write_memory (sp, buf, 4);
    }

  /* Store return address.  */
  sp -= 4;
  store_unsigned_integer (buf, 4, bp_addr);
  write_memory (sp, buf, 4);
	
  /* Finally, update the stack pointer...  */
  store_unsigned_integer (buf, 4, sp);
  regcache_cooked_write (regcache, I386_ESP_REGNUM, buf);

#if 0
  /* No need to fake the frame pointer here.  We are setting up the
     stack frame *before* the prologue of the function being called,
     which is just going to save $ebp right away.  So we want it to
     (correctly) point to the previous $ebp, so that it can get saved
     properly. */
  /* ...and fake a frame pointer.  */
  regcache_cooked_write (regcache, I386_EBP_REGNUM, buf);
#endif

  /* MarkK wrote: This "+ 8" is all over the place:
     (i386_frame_this_id, i386_sigtramp_frame_this_id,
     i386_unwind_dummy_id).  It's there, since all frame unwinders for
     a given target have to agree (within a certain margin) on the
     definition of the stack address of a frame.  Otherwise
     frame_id_inner() won't work correctly.  Since DWARF2/GCC uses the
     stack address *before* the function call as a frame's CFA.  On
     the i386, when %ebp is used as a frame pointer, the offset
     between the contents %ebp and the CFA as defined by GCC.  */

  /* We use I386_EBP_REGNUM here, not $sp, because that's what the
     rest of the code is really going to be matching on. */

  return read_register (I386_EBP_REGNUM) + 8;
}

/* These registers are used for returning integers (and on some
   targets also for returning `struct' and `union' values when their
   size and alignment match an integer type).  */
#define LOW_RETURN_REGNUM	I386_EAX_REGNUM /* %eax */
#define HIGH_RETURN_REGNUM	I386_EDX_REGNUM /* %edx */

/* Read, for architecture GDBARCH, a function return value of TYPE
   from REGCACHE, and copy that into VALBUF.  */

static void
i386_extract_return_value (struct gdbarch *gdbarch, struct type *type,
			   struct regcache *regcache, gdb_byte *valbuf)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  int len = TYPE_LENGTH (type);
  gdb_byte buf[I386_MAX_REGISTER_SIZE];

  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      if (tdep->st0_regnum < 0)
	{
	  warning (_("Cannot find floating-point return value."));
	  memset (valbuf, 0, len);
	  return;
	}

      /* Floating-point return values can be found in %st(0).  Convert
	 its contents to the desired type.  This is probably not
	 exactly how it would happen on the target itself, but it is
	 the best we can do.  */
      regcache_raw_read (regcache, I386_ST0_REGNUM, buf);
      convert_typed_floating (buf, builtin_type_i387_ext, valbuf, type);
    }
  else
    {
      int low_size = register_size (current_gdbarch, LOW_RETURN_REGNUM);
      int high_size = register_size (current_gdbarch, HIGH_RETURN_REGNUM);

      if (len <= low_size)
	{
	  regcache_raw_read (regcache, LOW_RETURN_REGNUM, buf);
	  memcpy (valbuf, buf, len);
	}
      else if (len <= (low_size + high_size))
	{
	  regcache_raw_read (regcache, LOW_RETURN_REGNUM, buf);
	  memcpy (valbuf, buf, low_size);
	  regcache_raw_read (regcache, HIGH_RETURN_REGNUM, buf);
	  memcpy (valbuf + low_size, buf, len - low_size);
	}
      else
	internal_error (__FILE__, __LINE__,
			_("Cannot extract return value of %d bytes long."), len);
    }
}

/* Write, for architecture GDBARCH, a function return value of TYPE
   from VALBUF into REGCACHE.  */

static void
i386_store_return_value (struct gdbarch *gdbarch, struct type *type,
			 struct regcache *regcache, const gdb_byte *valbuf)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  int len = TYPE_LENGTH (type);

  /* Define I387_ST0_REGNUM such that we use the proper definitions
     for the architecture.  */
#define I387_ST0_REGNUM I386_ST0_REGNUM

  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      ULONGEST fstat;
      gdb_byte buf[I386_MAX_REGISTER_SIZE];

      if (tdep->st0_regnum < 0)
	{
	  warning (_("Cannot set floating-point return value."));
	  return;
	}

      /* Returning floating-point values is a bit tricky.  Apart from
         storing the return value in %st(0), we have to simulate the
         state of the FPU at function return point.  */

      /* Convert the value found in VALBUF to the extended
	 floating-point format used by the FPU.  This is probably
	 not exactly how it would happen on the target itself, but
	 it is the best we can do.  */
      convert_typed_floating (valbuf, type, buf, builtin_type_i387_ext);
      regcache_raw_write (regcache, I386_ST0_REGNUM, buf);

      /* Set the top of the floating-point register stack to 7.  The
         actual value doesn't really matter, but 7 is what a normal
         function return would end up with if the program started out
         with a freshly initialized FPU.  */
      regcache_raw_read_unsigned (regcache, I387_FSTAT_REGNUM, &fstat);
      fstat |= (7 << 11);
      regcache_raw_write_unsigned (regcache, I387_FSTAT_REGNUM, fstat);

      /* Mark %st(1) through %st(7) as empty.  Since we set the top of
         the floating-point register stack to 7, the appropriate value
         for the tag word is 0x3fff.  */
      regcache_raw_write_unsigned (regcache, I387_FTAG_REGNUM, 0x3fff);
    }
  else
    {
      int low_size = register_size (current_gdbarch, LOW_RETURN_REGNUM);
      int high_size = register_size (current_gdbarch, HIGH_RETURN_REGNUM);

      if (len <= low_size)
	regcache_raw_write_part (regcache, LOW_RETURN_REGNUM, 0, len, valbuf);
      else if (len <= (low_size + high_size))
	{
	  regcache_raw_write (regcache, LOW_RETURN_REGNUM, valbuf);
	  regcache_raw_write_part (regcache, HIGH_RETURN_REGNUM, 0,
				   len - low_size, valbuf + low_size);
	}
      else
	internal_error (__FILE__, __LINE__,
			_("Cannot store return value of %d bytes long."), len);
    }

#undef I387_ST0_REGNUM
}


/* This is the variable that is set with "set struct-convention", and
   its legitimate values.  */
static const char default_struct_convention[] = "default";
static const char pcc_struct_convention[] = "pcc";
static const char reg_struct_convention[] = "reg";
static const char *valid_conventions[] =
{
  default_struct_convention,
  pcc_struct_convention,
  reg_struct_convention,
  NULL
};
static const char *struct_convention = default_struct_convention;

/* Return non-zero if TYPE is a non-POD structure or union type.  */

//static int
//i386_non_pod_p (struct type *type)
//{
  ///* ??? A class with a base class certainly isn't POD, but does this
     //catch all non-POD structure types?  */
  //if (TYPE_CODE (type) == TYPE_CODE_STRUCT && TYPE_N_BASECLASSES (type) > 0)
    //return 1;
//
  //return 0;
//}

/* Return non-zero if TYPE, which is assumed to be a structure or
   union type, should be returned in registers for architecture
   GDBARCH.  */

static int
i386_reg_struct_return_p (struct gdbarch *gdbarch, struct type *type)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  enum type_code code = TYPE_CODE (type);
  int len = TYPE_LENGTH (type);

  gdb_assert (code == TYPE_CODE_STRUCT || code == TYPE_CODE_UNION);

  if (struct_convention == pcc_struct_convention
      || (struct_convention == default_struct_convention
	  && tdep->struct_return == pcc_struct_return))
    return 0;

  /* Structures consisting of a single `float', `double' or 'long
     double' member are returned in %st(0).  */
  if (code == TYPE_CODE_STRUCT && TYPE_NFIELDS (type) == 1)
    {
      type = check_typedef (TYPE_FIELD_TYPE (type, 0));
      if (TYPE_CODE (type) == TYPE_CODE_FLT)
	return (len == 4 || len == 8 || len == 12);
    }

  /* Is this a C++ object?  If so, look for a copy constructor.  */
  //if (i386_non_pod_p (type))
    //{
      //return 0;
      ///*  FIXME copy ctor searching.. */
      ///* return 0; */
    //}

  return (len == 1 || len == 2 || len == 4 || len == 8);
}

/* Determine, for architecture GDBARCH, how a return value of TYPE
   should be returned.  If it is supposed to be returned in registers,
   and READBUF is non-zero, read the appropriate value from REGCACHE,
   and copy it into READBUF.  If WRITEBUF is non-zero, write the value
   from WRITEBUF into REGCACHE.  */

static enum return_value_convention
i386_return_value (struct gdbarch *gdbarch, struct type *type,
		   struct regcache *regcache, gdb_byte *readbuf,
		   const gdb_byte *writebuf)
{
  enum type_code code = TYPE_CODE (type);

  if ((code == TYPE_CODE_STRUCT || code == TYPE_CODE_UNION)
      && !i386_reg_struct_return_p (gdbarch, type))
    {
      /* The System V ABI says that:

	 "A function that returns a structure or union also sets %eax
	 to the value of the original address of the caller's area
	 before it returns.  Thus when the caller receives control
	 again, the address of the returned object resides in register
	 %eax and can be used to access the object."

	 So the ABI guarantees that we can always find the return
	 value just after the function has returned.  */

      if (readbuf)
	{
	  ULONGEST addr;

	  regcache_raw_read_unsigned (regcache, I386_EAX_REGNUM, &addr);
	  read_memory (addr, readbuf, TYPE_LENGTH (type));
	}

      return RETURN_VALUE_ABI_RETURNS_ADDRESS;
    }

  /* This special case is for structures consisting of a single
     `float', `double' or 'long double' member.  These structures are
     returned in %st(0).  For these structures, we call ourselves
     recursively, changing TYPE into the type of the first member of
     the structure.  Since that should work for all structures that
     have only one member, we don't bother to check the member's type
     here.  */
  if (code == TYPE_CODE_STRUCT && TYPE_NFIELDS (type) == 1)
    {
      type = check_typedef (TYPE_FIELD_TYPE (type, 0));
      return i386_return_value (gdbarch, type, regcache, readbuf, writebuf);
    }

  if (readbuf)
    i386_extract_return_value (gdbarch, type, regcache, readbuf);
  if (writebuf)
    i386_store_return_value (gdbarch, type, regcache, writebuf);

  return RETURN_VALUE_REGISTER_CONVENTION;
}

/* Types for the MMX and SSE registers.  */
static struct type *i386_mmx_type;

/* Construct the type for MMX registers.  */
static struct type *
i386_build_mmx_type (void)
{
  /* The type we're building is this: */
#if 0
  union __gdb_builtin_type_vec64i 
  {
    int64_t uint64;
    int32_t v2_int32[2];
    int16_t v4_int16[4];
    int8_t v8_int8[8];
  };
#endif

  if (! i386_mmx_type)
    {
      struct type *t;

      t = init_composite_type ("__gdb_builtin_type_vec64i", TYPE_CODE_UNION);
      append_composite_type_field (t, "uint64", builtin_type_int64);
      append_composite_type_field (t, "v2_int32", builtin_type_v2_int32);
      append_composite_type_field (t, "v4_int16", builtin_type_v4_int16);
      append_composite_type_field (t, "v8_int8", builtin_type_v8_int8);

      TYPE_FLAGS (t) |= TYPE_FLAG_VECTOR;
      TYPE_NAME (t) = "builtin_type_vec64i";

      i386_mmx_type = t;
    }

  return i386_mmx_type;
}

/* Return the GDB type object for the "standard" data type of data in
   register REGNUM.  Perhaps %esi and %edi should go here, but
   potentially they could be used for things other than address.  */

static struct type *
i386_register_type (struct gdbarch *gdbarch, int regnum)
{
  if (regnum == I386_EIP_REGNUM)
    return builtin_type_void_func_ptr;

  if (regnum == I386_EBP_REGNUM || regnum == I386_ESP_REGNUM)
    return builtin_type_void_data_ptr;

  if (i386_fp_regnum_p (regnum))
    return builtin_type_i387_ext;

  if (i386_sse_regnum_p (gdbarch, regnum))
    /* APPLE LOCAL i386 vectors */
    return builtin_type_vec128i_big;

  if (i386_mmx_regnum_p (gdbarch, regnum))
    return i386_build_mmx_type ();

  return builtin_type_int;
}

/* Map a cooked register onto a raw register or memory.  For the i386,
   the MMX registers need to be mapped onto floating point registers.  */

static int
i386_mmx_regnum_to_fp_regnum (struct regcache *regcache, int regnum)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (get_regcache_arch (regcache));
  int mmxreg, fpreg;
  ULONGEST fstat;
  int tos;

  /* Define I387_ST0_REGNUM such that we use the proper definitions
     for REGCACHE's architecture.  */
#define I387_ST0_REGNUM tdep->st0_regnum

  mmxreg = regnum - tdep->mm0_regnum;
  regcache_raw_read_unsigned (regcache, I387_FSTAT_REGNUM, &fstat);
  tos = (fstat >> 11) & 0x7;
  fpreg = (mmxreg + tos) % 8;

  return (I387_ST0_REGNUM + fpreg);

#undef I387_ST0_REGNUM
}

static void
i386_pseudo_register_read (struct gdbarch *gdbarch, struct regcache *regcache,
			   int regnum, gdb_byte *buf)
{
  if (i386_mmx_regnum_p (gdbarch, regnum))
    {
      gdb_byte mmx_buf[MAX_REGISTER_SIZE];
      int fpnum = i386_mmx_regnum_to_fp_regnum (regcache, regnum);

      /* Extract (always little endian).  */
      regcache_raw_read (regcache, fpnum, mmx_buf);
      memcpy (buf, mmx_buf, register_size (gdbarch, regnum));
    }
  else
    regcache_raw_read (regcache, regnum, buf);
}

static void
i386_pseudo_register_write (struct gdbarch *gdbarch, struct regcache *regcache,
			    int regnum, const gdb_byte *buf)
{
  if (i386_mmx_regnum_p (gdbarch, regnum))
    {
      gdb_byte mmx_buf[MAX_REGISTER_SIZE];
      int fpnum = i386_mmx_regnum_to_fp_regnum (regcache, regnum);

      /* Read ...  */
      regcache_raw_read (regcache, fpnum, mmx_buf);
      /* ... Modify ... (always little endian).  */
      memcpy (mmx_buf, buf, register_size (gdbarch, regnum));
      /* ... Write.  */
      regcache_raw_write (regcache, fpnum, mmx_buf);
    }
  else
    regcache_raw_write (regcache, regnum, buf);
}


/* Return the register number of the register allocated by GCC after
   REGNUM, or -1 if there is no such register.  */

static int
i386_next_regnum (int regnum)
{
  /* GCC allocates the registers in the order:

     %eax, %edx, %ecx, %ebx, %esi, %edi, %ebp, %esp, ...

     Since storing a variable in %esp doesn't make any sense we return
     -1 for %ebp and for %esp itself.  */
  static int next_regnum[] =
  {
    I386_EDX_REGNUM,		/* Slot for %eax.  */
    I386_EBX_REGNUM,		/* Slot for %ecx.  */
    I386_ECX_REGNUM,		/* Slot for %edx.  */
    I386_ESI_REGNUM,		/* Slot for %ebx.  */
    -1, -1,			/* Slots for %esp and %ebp.  */
    I386_EDI_REGNUM,		/* Slot for %esi.  */
    I386_EBP_REGNUM		/* Slot for %edi.  */
  };

  if (regnum >= 0 && regnum < sizeof (next_regnum) / sizeof (next_regnum[0]))
    return next_regnum[regnum];

  return -1;
}

/* Return nonzero if a value of type TYPE stored in register REGNUM
   needs any special handling.  */

static int
i386_convert_register_p (int regnum, struct type *type)
{
  int len = TYPE_LENGTH (type);

  /* Values may be spread across multiple registers.  Most debugging
     formats aren't expressive enough to specify the locations, so
     some heuristics is involved.  Right now we only handle types that
     have a length that is a multiple of the word size, since GCC
     doesn't seem to put any other types into registers.  */
  if (len > 4 && len % 4 == 0)
    {
      int last_regnum = regnum;

      while (len > 4)
	{
	  last_regnum = i386_next_regnum (last_regnum);
	  len -= 4;
	}

      if (last_regnum != -1)
	return 1;
    }

  return i386_fp_regnum_p (regnum);
}

/* Read a value of type TYPE from register REGNUM in frame FRAME, and
   return its contents in TO.  */

static void
i386_register_to_value (struct frame_info *frame, int regnum,
			struct type *type, gdb_byte *to)
{
  int len = TYPE_LENGTH (type);

  /* FIXME: kettenis/20030609: What should we do if REGNUM isn't
     available in FRAME (i.e. if it wasn't saved)?  */

  if (i386_fp_regnum_p (regnum))
    {
      i387_register_to_value (frame, regnum, type, to);
      return;
    }

  /* Read a value spread across multiple registers.  */

  gdb_assert (len > 4 && len % 4 == 0);

  while (len > 0)
    {
      gdb_assert (regnum != -1);
      gdb_assert (register_size (current_gdbarch, regnum) == 4);

      get_frame_register (frame, regnum, to);
      regnum = i386_next_regnum (regnum);
      len -= 4;
      to += 4;
    }
}

/* Write the contents FROM of a value of type TYPE into register
   REGNUM in frame FRAME.  */

static void
i386_value_to_register (struct frame_info *frame, int regnum,
			struct type *type, const gdb_byte *from)
{
  int len = TYPE_LENGTH (type);

  if (i386_fp_regnum_p (regnum))
    {
      i387_value_to_register (frame, regnum, type, from);
      return;
    }

  /* Write a value spread across multiple registers.  */

  gdb_assert (len > 4 && len % 4 == 0);

  while (len > 0)
    {
      gdb_assert (regnum != -1);
      gdb_assert (register_size (current_gdbarch, regnum) == 4);

      put_frame_register (frame, regnum, from);
      regnum = i386_next_regnum (regnum);
      len -= 4;
      from += 4;
    }
}

/* Supply register REGNUM from the buffer specified by GREGS and LEN
   in the general-purpose register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

void
i386_supply_gregset (const struct regset *regset, struct regcache *regcache,
		     int regnum, const void *gregs, size_t len)
{
  const struct gdbarch_tdep *tdep = gdbarch_tdep (regset->arch);
  const gdb_byte *regs = gregs;
  int i;

  gdb_assert (len == tdep->sizeof_gregset);

  for (i = 0; i < tdep->gregset_num_regs; i++)
    {
      if ((regnum == i || regnum == -1)
	  && tdep->gregset_reg_offset[i] != -1)
	regcache_raw_supply (regcache, i, regs + tdep->gregset_reg_offset[i]);
    }
}

/* Collect register REGNUM from the register cache REGCACHE and store
   it in the buffer specified by GREGS and LEN as described by the
   general-purpose register set REGSET.  If REGNUM is -1, do this for
   all registers in REGSET.  */

void
i386_collect_gregset (const struct regset *regset,
		      const struct regcache *regcache,
		      int regnum, void *gregs, size_t len)
{
  const struct gdbarch_tdep *tdep = gdbarch_tdep (regset->arch);
  gdb_byte *regs = gregs;
  int i;

  gdb_assert (len == tdep->sizeof_gregset);

  for (i = 0; i < tdep->gregset_num_regs; i++)
    {
      if ((regnum == i || regnum == -1)
	  && tdep->gregset_reg_offset[i] != -1)
	regcache_raw_collect (regcache, i, regs + tdep->gregset_reg_offset[i]);
    }
}

/* Supply register REGNUM from the buffer specified by FPREGS and LEN
   in the floating-point register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
i386_supply_fpregset (const struct regset *regset, struct regcache *regcache,
		      int regnum, const void *fpregs, size_t len)
{
  const struct gdbarch_tdep *tdep = gdbarch_tdep (regset->arch);

  if (len == I387_SIZEOF_FXSAVE)
    {
      i387_supply_fxsave (regcache, regnum, fpregs);
      return;
    }

  gdb_assert (len == tdep->sizeof_fpregset);
  i387_supply_fsave (regcache, regnum, fpregs);
}

/* Collect register REGNUM from the register cache REGCACHE and store
   it in the buffer specified by FPREGS and LEN as described by the
   floating-point register set REGSET.  If REGNUM is -1, do this for
   all registers in REGSET.  */

static void
i386_collect_fpregset (const struct regset *regset,
		       const struct regcache *regcache,
		       int regnum, void *fpregs, size_t len)
{
  const struct gdbarch_tdep *tdep = gdbarch_tdep (regset->arch);

  if (len == I387_SIZEOF_FXSAVE)
    {
      i387_collect_fxsave (regcache, regnum, fpregs);
      return;
    }

  gdb_assert (len == tdep->sizeof_fpregset);
  i387_collect_fsave (regcache, regnum, fpregs);
}

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

const struct regset *
i386_regset_from_core_section (struct gdbarch *gdbarch,
			       const char *sect_name, size_t sect_size)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  if (strcmp (sect_name, ".reg") == 0 && sect_size == tdep->sizeof_gregset)
    {
      if (tdep->gregset == NULL)
	tdep->gregset = regset_alloc (gdbarch, i386_supply_gregset,
				      i386_collect_gregset);
      return tdep->gregset;
    }

  if ((strcmp (sect_name, ".reg2") == 0 && sect_size == tdep->sizeof_fpregset)
      || (strcmp (sect_name, ".reg-xfp") == 0
	  && sect_size == I387_SIZEOF_FXSAVE))
    {
      if (tdep->fpregset == NULL)
	tdep->fpregset = regset_alloc (gdbarch, i386_supply_fpregset,
				       i386_collect_fpregset);
      return tdep->fpregset;
    }

  return NULL;
}


/* Return whether the frame preceding NEXT_FRAME corresponds to a
   sigtramp routine.  */

static int
i386_sigtramp_p (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  char *name;

  find_pc_partial_function_no_inlined (pc, &name, NULL, NULL);
  return (name && strcmp ("_sigtramp", name) == 0);
}


/* We have two flavours of disassembly.  The machinery on this page
   deals with switching between those.  */

static int
i386_print_insn (bfd_vma pc, struct disassemble_info *info)
{
  gdb_assert (disassembly_flavor == att_flavor
	      || disassembly_flavor == intel_flavor);

  /* FIXME: kettenis/20020915: Until disassembler_options is properly
     constified, cast to prevent a compiler warning.  */
  info->disassembler_options = (char *) disassembly_flavor;
  info->mach = gdbarch_bfd_arch_info (current_gdbarch)->mach;

  return print_insn_i386 (pc, info);
}


/* i386 register groups.  In addition to the normal groups, add "mmx"
   and "sse".  */

static struct reggroup *i386_sse_reggroup;
static struct reggroup *i386_mmx_reggroup;

static void
i386_init_reggroups (void)
{
  i386_sse_reggroup = reggroup_new ("sse", USER_REGGROUP);
  i386_mmx_reggroup = reggroup_new ("mmx", USER_REGGROUP);
}

static void
i386_add_reggroups (struct gdbarch *gdbarch)
{
  reggroup_add (gdbarch, i386_sse_reggroup);
  reggroup_add (gdbarch, i386_mmx_reggroup);
  reggroup_add (gdbarch, general_reggroup);
  reggroup_add (gdbarch, float_reggroup);
  reggroup_add (gdbarch, all_reggroup);
  reggroup_add (gdbarch, save_reggroup);
  reggroup_add (gdbarch, restore_reggroup);
  reggroup_add (gdbarch, vector_reggroup);
  reggroup_add (gdbarch, system_reggroup);
}

int
i386_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
			  struct reggroup *group)
{
  int sse_regnum_p = (i386_sse_regnum_p (gdbarch, regnum)
		      || i386_mxcsr_regnum_p (gdbarch, regnum));
  int fp_regnum_p = (i386_fp_regnum_p (regnum)
		     || i386_fpc_regnum_p (regnum));
  int mmx_regnum_p = (i386_mmx_regnum_p (gdbarch, regnum));

  if (group == i386_mmx_reggroup)
    return mmx_regnum_p;
  if (group == i386_sse_reggroup)
    return sse_regnum_p;
  if (group == vector_reggroup)
    return (mmx_regnum_p || sse_regnum_p);
  if (group == float_reggroup)
    return fp_regnum_p;
  if (group == general_reggroup)
    return (!fp_regnum_p && !mmx_regnum_p && !sse_regnum_p);

  return default_register_reggroup_p (gdbarch, regnum, group);
}


/* Get the ARGIth function argument for the current function.  */

static CORE_ADDR
i386_fetch_pointer_argument (struct frame_info *frame, int argi, 
			     struct type *type)
{
  CORE_ADDR sp = get_frame_register_unsigned  (frame, I386_ESP_REGNUM);
  return read_memory_unsigned_integer (sp + (4 * (argi + 1)), 4);
}


static struct gdbarch *
i386_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
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

  /* APPLE LOCAL: This cpu family is only 32 bit, but we use wordsize to 
     distinguish between ppc32 and ppc64 -- so to allow for generic code,
     we have wordsize over here, too.  */
  tdep->wordsize = 4;

  /* General-purpose registers.  */
  tdep->gregset = NULL;
  tdep->gregset_reg_offset = NULL;
  tdep->gregset_num_regs = I386_NUM_GREGS;
  tdep->sizeof_gregset = 0;

  /* Floating-point registers.  */
  tdep->fpregset = NULL;
  tdep->sizeof_fpregset = I387_SIZEOF_FSAVE;

  /* The default settings include the FPU registers, the MMX registers
     and the SSE registers.  This can be overridden for a specific ABI
     by adjusting the members `st0_regnum', `mm0_regnum' and
     `num_xmm_regs' of `struct gdbarch_tdep', otherwise the registers
     will show up in the output of "info all-registers".  Ideally we
     should try to autodetect whether they are available, such that we
     can prevent "info all-registers" from displaying registers that
     aren't available.

     NOTE: kevinb/2003-07-13: ... if it's a choice between printing
     [the SSE registers] always (even when they don't exist) or never
     showing them to the user (even when they do exist), I prefer the
     former over the latter.  */

  tdep->st0_regnum = I386_ST0_REGNUM;

  /* The MMX registers are implemented as pseudo-registers.  Put off
     calculating the register number for %mm0 until we know the number
     of raw registers.  */
  tdep->mm0_regnum = 0;

  /* I386_NUM_XREGS includes %mxcsr, so substract one.  */
  tdep->num_xmm_regs = I386_NUM_XREGS - 1;

  tdep->jb_pc_offset = -1;
  tdep->struct_return = pcc_struct_return;
  tdep->sigtramp_start = 0;
  tdep->sigtramp_end = 0;
  tdep->sigtramp_p = i386_sigtramp_p;
  tdep->sigcontext_addr = NULL;
  tdep->sc_reg_offset = NULL;
  tdep->sc_pc_offset = -1;
  tdep->sc_sp_offset = -1;

  /* The format used for `long double' on almost all i386 targets is
     the i387 extended floating-point format.  In fact, of all targets
     in the GCC 2.95 tree, only OSF/1 does it different, and insists
     on having a `long double' that's not `long' at all.  */
  set_gdbarch_long_double_format (gdbarch, &floatformat_i387_ext);

  /* Although the i387 extended floating-point has only 80 significant
     bits, a `long double' actually takes up 96, probably to enforce
     alignment.  */
  /* APPLE LOCAL: long double is 128 bits on Mac OS X. */
  set_gdbarch_long_double_bit (gdbarch, 128);

  /* The default ABI includes general-purpose registers, 
     floating-point registers, and the SSE registers.  */
  set_gdbarch_num_regs (gdbarch, I386_SSE_NUM_REGS);
  set_gdbarch_register_name (gdbarch, i386_register_name);
  set_gdbarch_register_type (gdbarch, i386_register_type);

  /* Register numbers of various important registers.  */
  set_gdbarch_sp_regnum (gdbarch, I386_ESP_REGNUM); /* %esp */
  set_gdbarch_pc_regnum (gdbarch, I386_EIP_REGNUM); /* %eip */
  set_gdbarch_ps_regnum (gdbarch, I386_EFLAGS_REGNUM); /* %eflags */
  set_gdbarch_fp0_regnum (gdbarch, I386_ST0_REGNUM); /* %st(0) */
  /* APPLE LOCAL: Add the frame pointer register so it can be modified
     in expressions.  */
  set_gdbarch_deprecated_fp_regnum (gdbarch, I386_EBP_REGNUM); /* %ebp */

  /* NOTE: kettenis/20040418: GCC does have two possible register
     numbering schemes on the i386: dbx and SVR4.  These schemes
     differ in how they number %ebp, %esp, %eflags, and the
     floating-point registers, and are implemented by the arrays
     dbx_register_map[] and svr4_dbx_register_map in
     gcc/config/i386.c.  GCC also defines a third numbering scheme in
     gcc/config/i386.c, which it designates as the "default" register
     map used in 64bit mode.  This last register numbering scheme is
     implemented in dbx64_register_map, and is used for AMD64; see
     amd64-tdep.c.

     Currently, each GCC i386 target always uses the same register
     numbering scheme across all its supported debugging formats
     i.e. SDB (COFF), stabs and DWARF 2.  This is because
     gcc/sdbout.c, gcc/dbxout.c and gcc/dwarf2out.c all use the
     DBX_REGISTER_NUMBER macro which is defined by each target's
     respective config header in a manner independent of the requested
     output debugging format.

     This does not match the arrangement below, which presumes that
     the SDB and stabs numbering schemes differ from the DWARF and
     DWARF 2 ones.  The reason for this arrangement is that it is
     likely to get the numbering scheme for the target's
     default/native debug format right.  For targets where GCC is the
     native compiler (FreeBSD, NetBSD, OpenBSD, GNU/Linux) or for
     targets where the native toolchain uses a different numbering
     scheme for a particular debug format (stabs-in-ELF on Solaris)
     the defaults below will have to be overridden, like
     i386_elf_init_abi() does.  */

  /* Use the dbx register numbering scheme for stabs and COFF.  */
  set_gdbarch_stab_reg_to_regnum (gdbarch, i386_dbx_reg_to_regnum);
  set_gdbarch_sdb_reg_to_regnum (gdbarch, i386_dbx_reg_to_regnum);

  /* Use the SVR4 register numbering scheme for DWARF and DWARF 2.  */
  set_gdbarch_dwarf_reg_to_regnum (gdbarch, i386_svr4_reg_to_regnum);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, i386_svr4_reg_to_regnum);

  /* We don't define ECOFF_REG_TO_REGNUM, since ECOFF doesn't seem to
     be in use on any of the supported i386 targets.  */

  set_gdbarch_print_float_info (gdbarch, i387_print_float_info);

  set_gdbarch_get_longjmp_target (gdbarch, i386_get_longjmp_target);

  /* Call dummy code.  */
  set_gdbarch_push_dummy_call (gdbarch, i386_push_dummy_call);

  set_gdbarch_convert_register_p (gdbarch, i386_convert_register_p);
  set_gdbarch_register_to_value (gdbarch,  i386_register_to_value);
  set_gdbarch_value_to_register (gdbarch, i386_value_to_register);

  set_gdbarch_return_value (gdbarch, i386_return_value);

  set_gdbarch_skip_prologue (gdbarch, i386_skip_prologue);

  /* Stack grows downward.  */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  set_gdbarch_breakpoint_from_pc (gdbarch, i386_breakpoint_from_pc);
  set_gdbarch_decr_pc_after_break (gdbarch, 1);

  set_gdbarch_frame_args_skip (gdbarch, 8);

  /* Wire in the MMX registers.  */
  set_gdbarch_num_pseudo_regs (gdbarch, i386_num_mmx_regs);
  set_gdbarch_pseudo_register_read (gdbarch, i386_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, i386_pseudo_register_write);

  set_gdbarch_print_insn (gdbarch, i386_print_insn);

  set_gdbarch_unwind_dummy_id (gdbarch, i386_unwind_dummy_id);

  set_gdbarch_unwind_pc (gdbarch, i386_unwind_pc);

  /* Add the i386 register groups.  */
  i386_add_reggroups (gdbarch);
  set_gdbarch_register_reggroup_p (gdbarch, i386_register_reggroup_p);

  /* Helper for function argument information.  */
  set_gdbarch_fetch_pointer_argument (gdbarch, i386_fetch_pointer_argument);

  /* Hook in the DWARF CFI frame unwinder.  */
  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);
  frame_base_set_default (gdbarch, &i386_frame_base);
  set_gdbarch_adjust_ehframe_regnum (gdbarch, i386_adjust_ehframe_regnum);

  /* Hook in ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

  frame_unwind_append_sniffer (gdbarch, i386_sigtramp_frame_sniffer);
  frame_unwind_append_sniffer (gdbarch, i386_frame_sniffer);

  /* If we have a register mapping, enable the generic core file
     support, unless it has already been enabled.  */
  if (tdep->gregset_reg_offset
      && !gdbarch_regset_from_core_section_p (gdbarch))
    set_gdbarch_regset_from_core_section (gdbarch,
					  i386_regset_from_core_section);

  /* Unless support for MMX has been disabled, make %mm0 the first
     pseudo-register.  */
  if (tdep->mm0_regnum == 0)
    tdep->mm0_regnum = gdbarch_num_regs (gdbarch);

  return gdbarch;
}

/* APPLE LOCAL: a function for checking the prologue parser by hand. */

static void
maintenance_i386_prologue_parser (char *arg, int from_tty)
{
  char **argv;
  CORE_ADDR start_address, end_address;
  CORE_ADDR parsed_to;
  int argc;
  struct cleanup *cleanups;
  struct x86_frame_cache *cache;
  struct minimal_symbol *func;
  int parse_failed = 0;

  if (arg == NULL || arg[0] == '\0')
    return;

  argv = buildargv (arg);
  if (argv == NULL)
    return;
  cleanups = make_cleanup_freeargv (argv);

  for (argc = 0; argv[argc] != NULL && argv[argc][0] != '\0'; argc++)
    ;

  if (argc == 0)
    {
      do_cleanups (cleanups);
      return;
    }

  start_address = parse_and_eval_address (argv[0]);
  if (argc == 2)
    end_address = strtoul (argv[1], NULL, 16);
  else
    end_address = start_address + 48; /* 48 bytes is enough for a prologue */

  if (gdbarch_lookup_osabi (exec_bfd) == GDB_OSABI_DARWIN64)
    cache = x86_alloc_frame_cache (8);
  else
    cache = x86_alloc_frame_cache (4);

  parsed_to = x86_analyze_prologue (start_address, end_address + 1, cache);
  
  func = lookup_minimal_symbol_by_pc_section (start_address, NULL);
  printf_filtered ("Analyzing the prologue of '%s' 0x%s.\n", 
                   SYMBOL_LINKAGE_NAME (func), 
                   paddr_nz (SYMBOL_VALUE_ADDRESS (func)));
  if (func != lookup_minimal_symbol_by_pc_section (parsed_to, NULL))
    {
      printf_filtered ("Prologue scanner went to 0x%s (off the end of '%s')"
                        " trying to\nfind a prologue.  %s is frameless?\n",
                       paddr_nz (parsed_to),
                       SYMBOL_LINKAGE_NAME (func), SYMBOL_LINKAGE_NAME (func));
      parse_failed = 1;
    }
  else 
    {
      printf_filtered ("Prologue parser parsed to address 0x%s (%d bytes)",
                       paddr_nz (parsed_to), (int) (parsed_to - start_address));
      if (parsed_to == end_address -1)
        printf_filtered (" which is the entire length of the function\n");
      else
        {
          printf_filtered (".\n");
          if (cache->saved_regs[cache->ebp_regnum] == -1)
            {
              printf_filtered ("Didn't find push %%ebp and didn't parse the full range: prologue parse failed (frameless function?)\n");
              parse_failed = 1;
            }
        }
    }

  printf_filtered ("\n");
  if (cache->saved_regs[cache->ebp_regnum] == -1)
    {
      printf_filtered ("Did not find the push %%ebp\n");
      parse_failed = 1;
    }
  else
    printf_filtered ("Found push %%ebp\n");
  if (cache->ebp_is_frame_pointer == 0)
    {
      printf_filtered ("Did not find mov %%esp, %%ebp\n");
      parse_failed = 1;
    }
  if (cache->ebp_is_frame_pointer == 1)
    printf_filtered ("Found mov %%esp, %%ebp\n");

  if (parse_failed)
    printf_filtered ("\nFAILED TO PARSE func %s startaddr 0x%s\n\n",
                     SYMBOL_LINKAGE_NAME (func), paddr_nz (start_address));

  do_cleanups (cleanups);
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_i386_tdep (void);

void
_initialize_i386_tdep (void)
{
  register_gdbarch_init (bfd_arch_i386, i386_gdbarch_init);

  builtin_type_vec128i_big = build_builtin_type_vec128i_big ();

  /* Add the variable that controls the disassembly flavor.  */
  add_setshow_enum_cmd ("disassembly-flavor", no_class, valid_flavors,
			&disassembly_flavor, _("\
Set the disassembly flavor."), _("\
Show the disassembly flavor."), _("\
The valid values are \"att\" and \"intel\", and the default value is \"att\"."),
			NULL,
			NULL, /* FIXME: i18n: */
			&setlist, &showlist);

  /* APPLE LOCAL: maint i386-prologue-parser */
  add_cmd ("i386-prologue-parser", class_maintenance, 
           maintenance_i386_prologue_parser, 
           "Run the i386 prologue analyzer on a function.\n"
           "arg1 is start address of function\n"
           "arg2 is optional end-address, defaulting to startaddr + 32 bytes.", 
           &maintenancelist);

  /* Add the variable that controls the convention for returning
     structs.  */

  add_setshow_enum_cmd ("struct-convention", no_class, valid_conventions,
			&struct_convention, _("\
Set the convention for returning small structs."), _("\
Show the convention for returning small structs."), _("\
Valid values are \"default\", \"pcc\" and \"reg\", and the default value\n\
is \"default\"."),
			NULL,
			NULL, /* FIXME: i18n: */
			&setlist, &showlist);

  /* Initialize the i386 specific register groups.  */
  i386_init_reggroups ();
}
