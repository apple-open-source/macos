/* Intel 386 target-dependent stuff.

   Copyright 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996,
   1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004 Free Software
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

/* Names of the registers.  The first 10 registers match the register
   numbering scheme used by GCC for stabs and DWARF.  */

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

/* MMX registers.  */

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

/* Return the name of register REG.  */

const char *
i386_register_name (int reg)
{
  if (i386_mmx_regnum_p (current_gdbarch, reg))
    return i386_mmx_names[reg - I387_MM0_REGNUM];

  if (reg >= 0 && reg < i386_num_register_names)
    return i386_register_names[reg];

  return NULL;
}

/* Convert stabs register number REG to the appropriate register
   number used by GDB.  */

static int
i386_stab_reg_to_regnum (int reg)
{
  /* This implements what GCC calls the "default" register map.  */
  if (reg >= 0 && reg <= 7)
    {
      /* General-purpose registers.  */
      return reg;
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

/* Convert DWARF register number REG to the appropriate register
   number used by GDB.  */

static int
i386_dwarf_reg_to_regnum (int reg)
{
  /* The DWARF register numbering includes %eip and %eflags, and
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
      /* The SSE and MMX registers have identical numbers as in stabs.  */
      return i386_stab_reg_to_regnum (reg);
    }

  /* This will hopefully provoke a warning.  */
  return NUM_REGS + NUM_PSEUDO_REGS;
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
   
static const unsigned char *
i386_breakpoint_from_pc (CORE_ADDR *pc, int *len)
{
  static unsigned char break_insn[] = { 0xcc };	/* int 3 */
  
  *len = sizeof (break_insn);
  return break_insn;
}

#ifdef I386_REGNO_TO_SYMMETRY
#error "The Sequent Symmetry is no longer supported."
#endif

/* According to the System V ABI, the registers %ebp, %ebx, %edi, %esi
   and %esp "belong" to the calling function.  Therefore these
   registers should be saved if they're going to be modified.  */

/* The maximum number of saved registers.  This should include all
   registers mentioned above, and %eip.  */
#define I386_NUM_SAVED_REGS	I386_NUM_GREGS

struct i386_frame_cache
{
  /* Base address.  */
  CORE_ADDR base;      /* The frame base, usually the contents of the EBP */
  CORE_ADDR sp_offset;
  CORE_ADDR pc;        /* APPLE LOCAL: This is the function's start address */

  /* Saved registers.  */
  CORE_ADDR saved_regs[I386_NUM_SAVED_REGS];
  CORE_ADDR saved_sp;
  int pc_in_eax;

  /* Stack space reserved for local variables.  */
  long locals;
};

/* Allocate and initialize a frame cache.  */

static struct i386_frame_cache *
i386_alloc_frame_cache (void)
{
  struct i386_frame_cache *cache;
  int i;

  cache = FRAME_OBSTACK_ZALLOC (struct i386_frame_cache);

  /* Base address.  */
  cache->base = 0;
  cache->sp_offset = -4;
  cache->pc = 0;

  /* Saved registers.  We initialize these to -1 since zero is a valid
     offset (that's where %ebp is supposed to be stored).  */
  for (i = 0; i < I386_NUM_SAVED_REGS; i++)
    cache->saved_regs[i] = -1;
  cache->saved_sp = 0;
  cache->pc_in_eax = 0;

  /* Frameless until proven otherwise.  */
  cache->locals = -1;

  return cache;
}

/* If the instruction at PC is a jump, return the address of its
   target.  Otherwise, return PC.  */

static CORE_ADDR
i386_follow_jump (CORE_ADDR pc)
{
  unsigned char op;
  long delta = 0;
  int data16 = 0;
  /* APPLE LOCAL: We use 64 bit CORE_ADDR's,
     so when we add the offset, we need to make sure
     that it wraps properly.  FIXME: We'll have to do 
     this more cleverly for 64 bit Intel.  */
  unsigned long pc_32 = (unsigned long) pc;

  op = read_memory_unsigned_integer (pc, 1);
  if (op == 0x66)
    {
      data16 = 1;
      op = read_memory_unsigned_integer (pc + 1, 1);
    }

  switch (op)
    {
    case 0xe9:
      /* Relative jump: if data16 == 0, disp32, else disp16.  */
      if (data16)
	{
	  delta = read_memory_integer (pc + 2, 2);

	  /* Include the size of the jmp instruction (including the
             0x66 prefix).  */
	  delta += 4;
	}
      else
	{
	  delta = read_memory_integer (pc + 1, 4);

	  /* Include the size of the jmp instruction.  */
	  delta += 5;
	}
      break;
    case 0xeb:
      /* Relative jump, disp8 (ignore data16).  */
      delta = read_memory_integer (pc + data16 + 1, 1);

      delta += data16 + 2;
      break;
    }

  pc_32 += delta;
  return (CORE_ADDR) pc_32;
}

/* APPLE LOCAL:  FIXME, this function doesn't make any sense.  This
   would assume a calling convention where CALL isn't used by the
   caller.  The caller would have to push the saved EIP, then push
   the pointer to memory where the struct is returned, and then do
   a jmp to the called function.  Look at what this code is doing
   -- the it must occur before the function prologue setup is done.
   It's easier to see the relationship if you look at the pre-2003
   version of this file in i386_get_frame_setup.

   Oh, and gcc doesn't work like this on x86.  The pointer to the memory
   location where the return structure is written is the last thing put
   on the stack -- first the arguments, then the return address location,
   then the saved EIP by the call instruction.   jmolenda/2005-04-12 */

/* Check whether PC points at a prologue for a function returning a
   structure or union.  If so, it updates CACHE and returns the
   address of the first instruction after the code sequence that
   removes the "hidden" argument from the stack or CURRENT_PC,
   whichever is smaller.  Otherwise, return PC.  */

static CORE_ADDR
i386_analyze_struct_return (CORE_ADDR pc, CORE_ADDR current_pc,
			    struct i386_frame_cache *cache)
{
  /* Functions that return a structure or union start with:

        popl %eax             0x58
        xchgl %eax, (%esp)    0x87 0x04 0x24
     or xchgl %eax, 0(%esp)   0x87 0x44 0x24 0x00

     (the System V compiler puts out the second `xchg' instruction,
     and the assembler doesn't try to optimize it, so the 'sib' form
     gets generated).  This sequence is used to get the address of the
     return buffer for a function that returns a structure.  */
  static unsigned char proto1[3] = { 0x87, 0x04, 0x24 };
  static unsigned char proto2[4] = { 0x87, 0x44, 0x24, 0x00 };
  unsigned char buf[4];
  unsigned char op;

  if (current_pc <= pc)
    return pc;

  op = read_memory_unsigned_integer (pc, 1);

  if (op != 0x58)		/* popl %eax */
    return pc;

  read_memory (pc + 1, buf, 4);
  if (memcmp (buf, proto1, 3) != 0 && memcmp (buf, proto2, 4) != 0)
    return pc;

  if (current_pc == pc)
    {
      cache->sp_offset += 4;
      return current_pc;
    }

  if (current_pc == pc + 1)
    {
      cache->pc_in_eax = 1;
      return current_pc;
    }
  
  if (buf[1] == proto1[1])
    return pc + 4;
  else
    return pc + 5;
}

static CORE_ADDR
i386_skip_probe (CORE_ADDR pc)
{
  /* A function may start with

        pushl constant
        call _probe
	addl $4, %esp
	   
     followed by

        pushl %ebp

     etc.  */
  unsigned char buf[8];
  unsigned char op;

  op = read_memory_unsigned_integer (pc, 1);

  if (op == 0x68 || op == 0x6a)
    {
      int delta;

      /* Skip past the `pushl' instruction; it has either a one-byte or a
	 four-byte operand, depending on the opcode.  */
      if (op == 0x68)
	delta = 5;
      else
	delta = 2;

      /* Read the following 8 bytes, which should be `call _probe' (6
	 bytes) followed by `addl $4,%esp' (2 bytes).  */
      read_memory (pc + delta, buf, sizeof (buf));
      if (buf[0] == 0xe8 && buf[6] == 0xc4 && buf[7] == 0x4)
	pc += delta + sizeof (buf);
    }

  return pc;
}

/* Maximum instruction length we need to handle (includes operands).  */
/* APPLE LOCAL: 7 -- I use this array for an instruction that takes 7 bytes. */
#define I386_MAX_INSN_LEN	7

/* Instruction description.  */
struct i386_insn
{
  size_t len;
  unsigned char insn[I386_MAX_INSN_LEN];
  unsigned char mask[I386_MAX_INSN_LEN];
};

/* Search for the instruction at PC in the list SKIP_INSNS.  Return
   the first instruction description that matches.  Otherwise, return
   NULL.  */

/* APPLE LOCAL: AVOID_PROLOGUE_INSNS is a hack to skip over the byte 
   combination for push %ebp and mov %esp,%ebp while still allowing 
   all the other push r32 and mov r32, r32 opcodes.  */

static struct i386_insn *
i386_match_insn (CORE_ADDR pc, struct i386_insn *skip_insns, 
                 int avoid_prologue_insns)
{
  struct i386_insn *insn;
  unsigned char op;

  op = read_memory_unsigned_integer (pc, 1);

  if (avoid_prologue_insns && (op == 0x55 || op == 0x6a))
    return NULL;

  for (insn = skip_insns; insn->len > 0; insn++)
    {
      if ((op & insn->mask[0]) == insn->insn[0])
	{
	  unsigned char buf[I386_MAX_INSN_LEN - 1];
	  size_t i;
          int insn_matched;

          /* APPLE LOCAL: We have some 1-byte opcodes we need to recognize.  */
          if (insn->len == 1)
            return insn;

	  gdb_assert (insn->len > 1);
	  gdb_assert (insn->len <= I386_MAX_INSN_LEN);

	  read_memory (pc + 1, buf, insn->len - 1);

          /* APPLE LOCAL: Don't match mov %esp, %ebp */
          if (avoid_prologue_insns && insn->len == 2 
              && (op == 0x89 && buf[0] == 0xe5))
            continue;

          insn_matched = 1;
	  for (i = 1; i < insn->len; i++)
	    {
	      if ((buf[i - 1] & insn->mask[i]) != insn->insn[i])
                {
                  insn_matched = 0;
		  break;
                }
	    }
          /* APPLE LOCAL: In the FSF code this 'return insn' is in the
             loop, which makes no sense - the code as it stands in the FSF
             will never examine the 3rd byte of a 3+ byte opcode.  */
          if (insn_matched)
            return insn;
	}
    }

  return NULL;
}

/* Some special instructions that might be migrated by GCC into the
   part of the prologue that sets up the new stack frame.  Because the
   stack frame hasn't been setup yet, no registers have been saved
   yet, and only the scratch registers %eax, %ecx and %edx can be
   touched.  */

struct i386_insn i386_frame_setup_skip_insns[] =
{
  /* Check for `movb imm8, r' and `movl imm32, r'. 
    
     ??? Should we handle 16-bit operand-sizes here?  */

  /* `movb imm8, %al' and `movb imm8, %ah' */
  /* `movb imm8, %cl' and `movb imm8, %ch' */
  { 2, { 0xb0, 0x00 }, { 0xfa, 0x00 } },
  /* `movb imm8, %dl' and `movb imm8, %dh' */
  { 2, { 0xb2, 0x00 }, { 0xfb, 0x00 } },
  /* `movl imm32, %eax' and `movl imm32, %ecx' */
  { 5, { 0xb8 }, { 0xfe } },
  /* `movl imm32, %edx' */
  { 5, { 0xba }, { 0xff } },

  /* Check for `mov imm32, r32'.  Note that there is an alternative
     encoding for `mov m32, %eax'.

     ??? Should we handle SIB adressing here?
     ??? Should we handle 16-bit operand-sizes here?  */

  /* `movl m32, %eax' */
  { 5, { 0xa1 }, { 0xff } },
  /* `movl m32, %eax' and `movl m32, %ecx' */
  { 6, { 0x89, 0x05 }, {0xff, 0xf7 } },
  /* `movl m32, %edx' */
  { 6, { 0x89, 0x15 }, {0xff, 0xff } },


  /* APPLE LOCAL: "01 /r       ADD r/m32, r32" */
  { 2, { 0x01, 0xd0 }, { 0xff, 0xd0 } },
  /* APPLE LOCAL: "0F 57 /r    XORPS xmm1, xmm2/m128" */
  { 3, { 0x0f, 0x57 }, { 0xff, 0xff } },
  /* APPLE LOCAL: "0F B6 /r    MOVZX r32, r/m8" (aka `movzbl %al, %eax') */
  { 3, { 0x0f, 0xb6, 0xc0 }, { 0xff, 0xff, 0xc0 } },
  /* APPLE LOCAL: "0F B7 /r    MOVZX r32, r/m16" (aka `movzwl r16, r32') */
  { 3, { 0x0f, 0xb7, 0xc0 }, { 0xff, 0xff, 0xc0 } },
  /* APPLE LOCAL: "25 id       AND EAX, imm32" */
  { 5, { 0x25 }, { 0xff } },
  /* APPLE LOCAL: "31 /r       XOR r/m32, r32" */
  { 2, { 0x31, 0xc0 }, { 0xff, 0xc0 } },
  /* APPLE LOCAL: "40+ rd      INC r32" */
  { 1, { 0x40 }, { 0xf8 } },
  /* APPLE LOCAL: "48+rw       DEC r32" */
  { 1, { 0x48 }, { 0xf8 } },
  /* APPLE LOCAL: "50+rd       PUSH r32" */
  { 1, { 0x50 }, { 0xf8 } },
  /* APPLE LOCAL: "58+ rd      POP r32" */
  { 1, { 0x58 }, { 0xf8 } },
  /* APPLE LOCAL: "B8+ rw      MOV r16, imm16" */
  { 4, { 0x66, 0xb8 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "74 cb       JE rel8" */
  { 2, { 0x74 }, { 0xff } },
  /* APPLE LOCAL: "80 /7 ib    CMP r/m8, imm8" (aka `cmpb imm8,(r32)') */
  { 3, { 0x80, 0x38 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "81 /4 id    AND r/m32, imm32" */
  { 6, { 0x81, 0xe0 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "81 /0 id    ADD r/m32, imm32" */
  { 6, { 0x81, 0xc0 }, { 0xff, 0xf8 } }, 
  /* APPLE LOCAL: "83 /7 ib    CMP r/m32, imm8" */
  { 7, { 0x83, 0x3d }, { 0xff, 0xff } },
  /* APPLE LOCAL: "83 /0 ib    ADD r/m32, imm8" */
  { 3, { 0x83, 0xc0 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "83 /1 ib    OR r/m32, imm8" */
  { 3, { 0x83, 0xc8 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "83 /4 ib    AND r/m32, imm8" */
  { 3, { 0x83, 0xe0 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "83 /5 ib    SUB r/m32, imm8" */
  { 3, { 0x83, 0xe8 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "83 /7 ib    CMP r/m32, imm8" */
  { 3, { 0x83, 0xf8 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "85 /r       TEST r/m32, r32" */
  { 2, { 0x85, 0xc0 }, { 0xff, 0xc0 } },
  /* APPLE LOCAL: "89 /r       MOV r/m32, r32" (aka `mov r32, (r32)') */
  { 2, { 0x89, 0x00 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "89 /r       MOV r/m32, r32" (aka `mov r32, (r32)') */
  { 2, { 0x89, 0x10 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "89 /r       MOV r/m32, r32" */
  { 2, { 0x89, 0xc0 }, { 0xff, 0xc0 } }, 
  /* APPLE LOCAL: "8B /r       MOV r32, r/m32" (aka `mov imm8(r32), r32') */
  { 3, { 0x8b, 0x40 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "8B /r       MOV r32, r/m32" */
  { 6, { 0x8b, 0x80 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "8B /r       MOV r32, r/m32" (aka `mov imm32(r32), r32') */
  { 6, { 0x8b, 0x90 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "8D /r       LEA r32, m" (aka `lea imm8(r32), r32') */
  { 3, { 0x8d, 0x48 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "8D /r       LEA r32, m" (aka `lea imm32(r32), r32') */
  { 6, { 0x8d, 0x90 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "90          NOP"  for Fix & Continue trampoline padding. */
  { 1, { 0x90 }, { 0xff } },
  /* APPLE LOCAL: "A9 id       TEST EAX, imm32" */
  { 5, { 0xa9 }, { 0xff } },
  /* APPLE LOCAL: "C1 /4 ib    SAL r/m32, imm8" (aka `shl imm8, r32') */
  { 3, { 0xc1, 0xe0 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "C1 /7 ib    SAR r/m32, imm8" */
  { 3, { 0xc1, 0xf8 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "C6 /0       MOV r/m8, imm8" (aka `mov imm8, (r32)') */
  { 3, { 0xc6, 0x00 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "D9 EE       FLDZ" */
  { 2, { 0xd9, 0xee }, { 0xff, 0xff } },
  /* APPLE LOCAL: "E8 cd       CALL rel32" */
  { 5, { 0xe8 }, { 0xff } },
  /* APPLE LOCAL: "EB cb       JMP rel8" */
  { 2, { 0xeb }, { 0xff } },
  /* APPLE LOCAL: "F7 /6       DIV r/m32" */
  { 2, { 0xf7, 0xf0 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "FC          CLD" */
  { 1, { 0xfc }, { 0xff } },
  /* APPLE LOCAL: "FF /6       PUSH r/m32" */
  { 6, { 0xff, 0xb0 }, { 0xff, 0xf8 } },
  /* APPLE LOCAL: "FF /4       JMP r/m32" */
  { 2, { 0xff, 0xe0 }, { 0xff, 0xf8 } },


  /* Check for `xorl r32, r32' and the equivalent `subl r32, r32'.
     Because of the symmetry, there are actually two ways to encode
     these instructions; opcode bytes 0x29 and 0x2b for `subl' and
     opcode bytes 0x31 and 0x33 for `xorl'.  */

  /* `subl %eax, %eax' */
  { 2, { 0x29, 0xc0 }, { 0xfd, 0xff } },
  /* `subl %ecx, %ecx' */
  { 2, { 0x29, 0xc9 }, { 0xfd, 0xff } },
  /* `subl %edx, %edx' */
  { 2, { 0x29, 0xd2 }, { 0xfd, 0xff } },
  /* `xorl %eax, %eax' */
  { 2, { 0x31, 0xc0 }, { 0xfd, 0xff } },
  /* `xorl %ecx, %ecx' */
  { 2, { 0x31, 0xc9 }, { 0xfd, 0xff } },
  /* `xorl %edx, %edx' */
  { 2, { 0x31, 0xd2 }, { 0xfd, 0xff } },
  { 0 }
};

/* Check whether PC points at a code that sets up a new stack frame.
   If so, it updates CACHE and returns the address of the first
   instruction after the sequence that sets up the frame or LIMIT,
   whichever is smaller.  If we don't recognize the code, return PC.  */

static CORE_ADDR
i386_analyze_frame_setup (CORE_ADDR pc, CORE_ADDR limit,
			  struct i386_frame_cache *cache)
{
  struct i386_insn *insn;
  unsigned char op;
  int skip = 0;

  /* APPLE LOCAL: This function returns a CORE_ADDR which is the instruction
     following the last frame-setup instruction we saw such that "frame-setup
     instruction" is one of push %ebp, push $0x0, mov %esp, %ebp, 
     sub $xxx, $esp, or enter.
     Specifically, we may scan past some of these instructions but we don't
     want to return the last address we scanned to -- we must return the 
     address of the instruction after one of those frame setup insns so that
     i386_analyze_register_saves () can look for register saves that may exist
     after them.  
     (and you can have register saves without any frame setup, e.g. in a 
     frameless function.) 
     I pedantically changed all returns to return end_of_frame_setup so it
     is completely clear what is going on.  */
 
  CORE_ADDR end_of_frame_setup = pc;

  if (limit <= pc)
    return limit;

  /* APPLE LOCAL: Skip over non-prologue instructions until we hit
       push %ebp [ 0x55 ]
       push $0x0 [ 0x6a 0x00 ]
     the latter instruction is the frame-setup insn in start ().  */

  op = read_memory_unsigned_integer (pc + skip, 1);
  while (op != 0x55 
         && (op != 0x6a || read_memory_unsigned_integer (pc + skip + 1, 1) != 0)
         && pc + skip <= limit)
    {
      insn = i386_match_insn (pc + skip, i386_frame_setup_skip_insns, 1);
      if (insn)
        {
          skip += insn->len;
          op = read_memory_unsigned_integer (pc + skip, 1);
        }
      else
        break;
    }
  pc = pc + skip;
  skip = 0;

  /* APPLE LOCAL: If we're now at the limit, don't detect the push %ebp yet. */
  if (limit <= pc)  
    return end_of_frame_setup;

  if (op == 0x55 || op == 0x6a)  /* pushl %ebp || push $0x0 */
    {
      /* Take into account that we've executed the `pushl %ebp' that
	 starts this instruction sequence.  */
      cache->saved_regs[I386_EBP_REGNUM] = 0;
      cache->sp_offset += 4;
      /* APPLE LOCAL: Skip imm8 operand if we're on `push $0x0' [0x6a 0x00]. */
      if (op == 0x6a)
        pc += 2;
      else
        pc++;
      end_of_frame_setup = pc;

      /* If that's all, return now.  */
      if (limit <= pc)
	return end_of_frame_setup;

      /* Check for some special instructions that might be migrated by
	 GCC into the prologue and skip them.  At this point in the
	 prologue, code should only touch the scratch registers %eax,
	 %ecx and %edx, so while the number of posibilities is sheer,
	 it is limited.

	 Make sure we only skip these instructions if we later see the
	 `movl %esp, %ebp' that actually sets up the frame.  */
      while (pc + skip < limit)
	{
	  insn = i386_match_insn (pc + skip, i386_frame_setup_skip_insns, 1);
	  if (insn == NULL)
	    break;

	  skip += insn->len;
	}

      /* If that's all, return now.  */
      if (limit <= pc + skip)
	return end_of_frame_setup;

      op = read_memory_unsigned_integer (pc + skip, 1);

      /* Check for `movl %esp, %ebp' -- can be written in two ways.  */
      switch (op)
	{
	case 0x8b:
	  if (read_memory_unsigned_integer (pc + skip + 1, 1) != 0xec)
	    return end_of_frame_setup;
	  break;
	case 0x89:
	  if (read_memory_unsigned_integer (pc + skip + 1, 1) != 0xe5)
	    return end_of_frame_setup;
	  break;
	default:
	  return end_of_frame_setup;
	}

      /* OK, we actually have a frame.  We just don't know how large
	 it is yet.  Set its size to zero.  We'll adjust it if
	 necessary.  We also now commit to skipping the special
	 instructions mentioned before.  */
      cache->locals = 0;
      pc += (skip + 2);
      end_of_frame_setup = pc;

      /* If that's all, return now.  */
      if (limit <= pc)
	return end_of_frame_setup;

      /* Check for stack adjustment 

	    subl $XXX, %esp

	 NOTE: You can't subtract a 16-bit immediate from a 32-bit
	 reg, so we don't have to worry about a data16 prefix.  */
      op = read_memory_unsigned_integer (pc, 1);
      if (op == 0x83)
	{
	  /* `subl' with 8-bit immediate.  */
	  if (read_memory_unsigned_integer (pc + 1, 1) != 0xec)
	    /* Some instruction starting with 0x83 other than `subl'.  */
	    return pc;

	  /* `subl' with signed 8-bit immediate (though it wouldn't
	     make sense to be negative).  */
	  cache->locals = read_memory_integer (pc + 2, 1);
          end_of_frame_setup = pc + 3;
	  return end_of_frame_setup;
	}
      else if (op == 0x81)
	{
	  /* Maybe it is `subl' with a 32-bit immediate.  */
	  if (read_memory_unsigned_integer (pc + 1, 1) != 0xec)
	    /* Some instruction starting with 0x81 other than `subl'.  */
	    return end_of_frame_setup;

	  /* It is `subl' with a 32-bit immediate.  */
	  cache->locals = read_memory_integer (pc + 2, 4);
          end_of_frame_setup = pc + 6;
	  return end_of_frame_setup;
	}
      else
	{
	  /* Some instruction other than `subl'.  */
	  return end_of_frame_setup;
	}
    }
  else if (op == 0xc8)		/* enter */
    {
      cache->locals = read_memory_unsigned_integer (pc + 1, 2);
      end_of_frame_setup = pc + 4;
      return end_of_frame_setup;
    }

  return end_of_frame_setup;
}

/* APPLE LOCAL: Instructions we're expecting to find in a prologue,
   so we can skip over these and get to the PIC base setup instruction,
   if it is done in this function.  */

struct i386_insn i386_frame_setup_insns[] =
{
  /* Check for `push r' for any of the registers.  */
  { 1, { 0x50 }, { 0xf8 } },
  /* Check for `mov %esp,%ebp'.  */
  { 2, { 0x89, 0xe5 }, { 0xff, 0xff } },
  { 2, { 0x8b, 0xec }, { 0xff, 0xff } },
  /* Check for `sub imm8, %esp'.  */
  { 3, { 0x83, 0xec }, { 0xff, 0xff } },
  /* Check for `sub imm32, %esp'.  */
  { 6, { 0x81, 0xec }, { 0xff, 0xff } },
  /* Check for `enter imm16, imm8'.  */
  { 4, { 0xc8 }, { 0xff} },

  { 0 }
};

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
  struct i386_insn *insn;
  int found_call_insn = 0;
  unsigned char op;

  if (picbase_addr != NULL)
    *picbase_addr = -1;

  while (skip < limit)
    {
      insn = i386_match_insn (pc + skip, i386_frame_setup_insns, 0);
      if (insn)
        {
          skip += insn->len;
          continue;
        }
      insn = i386_match_insn (pc + skip, i386_frame_setup_skip_insns, 1);

      if (insn == NULL)
        {
          /* no matched instruction - give up */
          break;
        }

      /* Did we just find a CALL instruction?  It's probably our 
         picbase setup call.  */
      if (insn->insn[0] == 0xe8 && insn->len == 5)
        {
          found_call_insn = 1;
          break;
        }
      skip += insn->len;
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
        *picbase_reg = (enum i386_regnum) op | 0x7;
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

/* APPLE LOCAL: Find adjustments of the ESP so we can locate the
   caller's saved EIP and backtrace out of a frameless function. 
   PC is the start of the function.  CURRENT_PC is the current
   instruction, or the last instruction of the function to limit this
   search.  
   Returns a signed offset of how much the ESP has moved since the
   start of the function.  The return value should be a negative number
   or 0.  */

static int
i386_find_esp_adjustments (CORE_ADDR pc, CORE_ADDR current_pc)
{
  unsigned char op, next_op;
  int esp_change = 0;
  struct i386_insn *insn;

  if (pc == current_pc)
    return esp_change;
  
  /* We're looking for PUSH r32, POP r32, SUB $x,ESP, ADD $x ESP. */
 
  while (pc < current_pc)
    {
      op = read_memory_unsigned_integer (pc, 1);
      next_op = read_memory_unsigned_integer (pc + 1, 1);
      
      /* `push %ebp'?  We shouldn't see that in here; give up. */
      if (op == 0x55)
        return esp_change;
      /* `ret'? We're at the end of the func; stop parsing. */
      if (op == 0xc3)
        return esp_change;

      /* 50+rd       PUSH r32 */
      if ((op & 0xf8) == 0x50)
        {
          esp_change -= 4;
          pc += 1;
          continue;
        }
      /* 58+ rd      POP r32 */
      if ((op & 0xf8) == 0x58)
        {
          esp_change += 4;
          pc += 1;
          continue;
        }
      /* 83 /5 ib    SUB r/m32, imm8 */
      if (op == 0x83 && next_op == 0xec)
        {
          uint8_t imm8 = read_memory_integer (pc + 2, 1);
          esp_change -= imm8;
          pc += 3;
          continue;
        }
      /* 81 /5 id    SUB r/m32, imm32 */
      if (op == 0x81 && next_op == 0xec)
        {
          uint32_t imm32 = read_memory_integer (pc + 2, 4);
          esp_change -= imm32;
          pc += 6;
          continue;
        }
      /* 83 /0 ib    ADD r/m32, imm8 */
      if (op == 0x83 && next_op == 0xc4)
        {
          uint8_t imm8 = read_memory_integer (pc + 2, 1);
          esp_change += imm8;
          pc += 3;
          continue;
        }
      /* 81 /0 id    ADD r/m32, imm8 */
      if (op == 0x81 && next_op == 0xc4)
        {
          uint32_t imm32 = read_memory_integer (pc + 2, 4);
          esp_change += imm32;
          pc += 6;
          continue;
        }

      insn = i386_match_insn (pc, i386_frame_setup_skip_insns, 1);
      if (insn)
        pc += insn->len;
      else
        return esp_change;  /* Hit an instruction we don't know; stop here. */
    }
  return esp_change;
}

/* Check whether PC points at code that saves registers on the stack.
   If so, it updates CACHE and returns the address of the first
   instruction after the register saves or CURRENT_PC, whichever is
   smaller.  Otherwise, return PC.  */

static CORE_ADDR
i386_analyze_register_saves (CORE_ADDR pc, CORE_ADDR current_pc,
			     struct i386_frame_cache *cache)
{
  CORE_ADDR offset = 0;
  unsigned char op;
  int i;

  if (cache->locals > 0)
    offset -= cache->locals;
  for (i = 0; i < 8 && pc < current_pc; i++)
    {
      op = read_memory_unsigned_integer (pc, 1);
      if (op < 0x50 || op > 0x57)
	break;

      offset -= 4;
      cache->saved_regs[op - 0x50] = offset;
      cache->sp_offset += 4;
      pc++;
    }

  return pc;
}

/* Return number of args passed to a frame.
   Can return -1, meaning no way to tell.  */

static int
i386_frame_num_args (struct frame_info *fi)
{
#if 1
  return -1;
#else
  /* This loses because not only might the compiler not be popping the
     args right after the function call, it might be popping args from
     both this call and a previous one, and we would say there are
     more args than there really are.  */

  int retpc;
  unsigned char op;
  struct frame_info *pfi;

  /* On the i386, the instruction following the call could be:
     popl %ecx        -  one arg
     addl $imm, %esp  -  imm/4 args; imm may be 8 or 32 bits
     anything else    -  zero args.  */

  int frameless;

  frameless = FRAMELESS_FUNCTION_INVOCATION (fi);
  if (frameless)
    /* In the absence of a frame pointer, GDB doesn't get correct
       values for nameless arguments.  Return -1, so it doesn't print
       any nameless arguments.  */
    return -1;

  pfi = get_prev_frame (fi);
  if (pfi == 0)
    {
      /* NOTE: This can happen if we are looking at the frame for
         main, because FRAME_CHAIN_VALID won't let us go into start.
         If we have debugging symbols, that's not really a big deal;
         it just means it will only show as many arguments to main as
         are declared.  */
      return -1;
    }
  else
    {
      retpc = pfi->pc;
      op = read_memory_integer (retpc, 1);
      if (op == 0x59)		/* pop %ecx */
	return 1;
      else if (op == 0x83)
	{
	  op = read_memory_integer (retpc + 1, 1);
	  if (op == 0xc4)
	    /* addl $<signed imm 8 bits>, %esp */
	    return (read_memory_integer (retpc + 2, 1) & 0xff) / 4;
	  else
	    return 0;
	}
      else if (op == 0x81)	/* `add' with 32 bit immediate.  */
	{
	  op = read_memory_integer (retpc + 1, 1);
	  if (op == 0xc4)
	    /* addl $<imm 32>, %esp */
	    return read_memory_integer (retpc + 2, 4) / 4;
	  else
	    return 0;
	}
      else
	{
	  return 0;
	}
    }
#endif
}

/* Parse the first few instructions the function to see what registers
   were stored.
   
   We handle these cases:

   The startup sequence can be at the start of the function, or the
   function can start with a branch to startup code at the end.

   %ebp can be set up with either the 'enter' instruction, or "pushl
   %ebp, movl %esp, %ebp" (`enter' is too slow to be useful, but was
   once used in the System V compiler).

   Local space is allocated just below the saved %ebp by either the
   'enter' instruction, or by "subl $<size>, %esp".  'enter' has a 16
   bit unsigned argument for space to allocate, and the 'addl'
   instruction could have either a signed byte, or 32 bit immediate.

   Next, the registers used by this function are pushed.  With the
   System V compiler they will always be in the order: %edi, %esi,
   %ebx (and sometimes a harmless bug causes it to also save but not
   restore %eax); however, the code below is willing to see the pushes
   in any order, and will handle up to 8 of them.
 
   If the setup sequence is at the end of the function, then the next
   instruction will be a branch back to the start.  */

static CORE_ADDR
i386_analyze_prologue (CORE_ADDR pc, CORE_ADDR current_pc,
		       struct i386_frame_cache *cache)
{
  pc = i386_follow_jump (pc);
  pc = i386_analyze_struct_return (pc, current_pc, cache);
  pc = i386_skip_probe (pc);
  pc = i386_analyze_frame_setup (pc, current_pc, cache);
  return i386_analyze_register_saves (pc, current_pc, cache);
}

/* Return PC of first real instruction.  */

static CORE_ADDR
i386_skip_prologue (CORE_ADDR start_pc)
{
  static unsigned char pic_pat[6] =
  {
    0xe8, 0, 0, 0, 0,		/* call 0x0 */
    0x5b,			/* popl %ebx */
  };
  struct i386_frame_cache cache;
  CORE_ADDR pc;
  unsigned char op;
  int i;

  cache.locals = -1;
  pc = i386_analyze_prologue (start_pc, 0xffffffff, &cache);
  if (cache.locals < 0)
    return start_pc;

  /* Found valid frame setup.  */

  /* The native cc on SVR4 in -K PIC mode inserts the following code
     to get the address of the global offset table (GOT) into register
     %ebx:

        call	0x0
	popl    %ebx
        movl    %ebx,x(%ebp)    (optional)
        addl    y,%ebx

     This code is with the rest of the prologue (at the end of the
     function), so we have to skip it to get to the first real
     instruction at the start of the function.  */

  for (i = 0; i < 6; i++)
    {
      op = read_memory_unsigned_integer (pc + i, 1);
      if (pic_pat[i] != op)
	break;
    }
  if (i == 6)
    {
      int delta = 6;

      op = read_memory_unsigned_integer (pc + delta, 1);

      if (op == 0x89)		/* movl %ebx, x(%ebp) */
	{
	  op = read_memory_unsigned_integer (pc + delta + 1, 1);

	  if (op == 0x5d)	/* One byte offset from %ebp.  */
	    delta += 3;
	  else if (op == 0x9d)	/* Four byte offset from %ebp.  */
	    delta += 6;
	  else			/* Unexpected instruction.  */
	    delta = 0;

	  op = read_memory_unsigned_integer (pc + delta, 1);
	}

      /* addl y,%ebx */
      if (delta > 0 && op == 0x81
	  && read_memory_unsigned_integer (pc + delta + 1, 1) == 0xc3);
	{
	  pc += delta + 6;
	}
    }

  return i386_follow_jump (pc);
}

/* This function is 64-bit safe.  */

static CORE_ADDR
i386_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  char buf[8];

  frame_unwind_register (next_frame, PC_REGNUM, buf);
  return extract_typed_address (buf, builtin_type_void_func_ptr);
}


/* Normal frames.
   APPLE LOCAL: This function must handle the following possible function types:

   0.  We're in a function we cannot know anything about - we have no 
       symbol for it; we can't find the start address.

   1.  The function is frameless.
     a. The ESP hasn't reached its Final Resting Place for the body of
        the function yet.
     b. We've finished the prologue (wherein we move the ESP - SUBs to
        make space for local storage, PUSHes to preserve saved regs.)

   2.  The function sets up a frame and
     a. We haven't executed any prologue instructions.
     b. We've executed the initial push %ebp (this one is critical).
     c. We've executed the mov %esp, %ebp
     d. We've completed the entire prologue.
     e. We're in the middle of a function which has a prologue, 
        but we can't parse it (we hit an unknown instruction mid-prologue).

   When reading i386_frame_cache, keep these three function types in mind
   and the different stages for #1 and #2 - the behavior of this function
   differs greatly depending on where you are.  */

static struct i386_frame_cache *
i386_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct i386_frame_cache *cache;
  char buf[4];
  int i;
  int potentially_frameless;
  CORE_ADDR prologue_parsed_to = 0;
  CORE_ADDR current_pc;

  /* APPLE LOCAL:  If the frame we're examining is frame #0, we could 
     be frameless.  Or if NEXT_FRAME is _sigtramp(), then we could be 
     frameless.

      Explanation:  If a frameless function is executing when a
      signal is caught, the frameless function will have _sigtramp()
      as its next_frame, followed by whatever signal handler is defined.
      This is not as rare as you'd think, at least in a testsuite:
      sleep() calls nanosleep() which calls mach_wait_until() which is
      frameless.  If an alarm(1) is done before that sequence, you'll
      get a frameless function in the middle of the stack.  

     If potentially_frameless == 0, there's no way the function we're
     examining is frameless; it has a stack frame set up with 
     the saved-EBP/saved-EIP at the standard locations.  
     (not entirely true -- if gcc's -fomit-frame-pointer is used you can
     have a function that doesn't ever set up the EBP, but calls other
     functions.  Handling that situation correctly is not easy.)  */

  potentially_frameless = frame_relative_level (next_frame) == -1 
                       || get_frame_type (next_frame) == SIGTRAMP_FRAME;

  if (*this_cache)
    return *this_cache;

  cache = i386_alloc_frame_cache ();
  *this_cache = cache;

  /* For normal frames, saved-%eip is stored at 4(%ebp). */
  cache->saved_regs[I386_EIP_REGNUM] = 4;

  /* For normal frames, saved-%ebp is stored at 0(%ebp). */
  cache->saved_regs[I386_EBP_REGNUM] = 0;

  frame_unwind_register (next_frame, I386_EBP_REGNUM, buf);
  cache->base = extract_unsigned_integer (buf, 4);
  if (cache->base == 0)
    return cache;

  cache->pc = frame_func_unwind (next_frame); /* function start address */
  current_pc = frame_pc_unwind (next_frame);

  /* Only do i386_analyze_prologue () if we found a debug symbol pointing to
     the actual start of the function.  */
  if (cache->pc != 0)
    prologue_parsed_to = i386_analyze_prologue (cache->pc, current_pc, cache);

  /* The prologue parser didn't find any prologue instructions.
     And our current function has the potential to be frameless.  
     Let's go frameless.  Assume EBP is unused, or not yet used.

     We'll put ESP in the cache->base instead of EBP; for genuinely
     frameless (e.g. -momit-leaf-frame-pointer) functions, the
     the debug info for function args will be relative to ESP once its 
     setup/adjustements in the prologue are complete, so cache->base has
     to hold the stack pointer if we're to find them. */

  if ((cache->pc != 0 || current_pc == 0)
      /* We found a function-start addr, or PC is at 0 (CALL to a NULL ptr).  */
      && prologue_parsed_to == cache->pc
      /* The prologue parser didn't find any prologue instructions.  */
      && potentially_frameless)
      /* We have the potential to be frameless.  */
    {
      int esp_offset;
      CORE_ADDR actual_frame_base;
      esp_offset = i386_find_esp_adjustments (cache->pc, current_pc);
      frame_unwind_register (next_frame, I386_ESP_REGNUM, buf);
      actual_frame_base = extract_unsigned_integer (buf, 4) - esp_offset;
      cache->base = extract_unsigned_integer (buf, 4);
      cache->saved_sp = actual_frame_base + 4;
      cache->saved_regs[I386_EBP_REGNUM] = -1;
      cache->saved_regs[I386_EIP_REGNUM] = 0;
      /* NB: There's a good chance we didn't record register saves a la
         i386_analyze_register_saves.  It'd be nice to fix this.   
         For now we'll say "Debugging optimized code is an adventure!"
         jmolenda/2005-04-27 */

      for (i = 0; i < I386_NUM_SAVED_REGS; i++)
        if (cache->saved_regs[i] != -1)
          cache->saved_regs[i] += actual_frame_base;
      return cache;
    }

  if (cache->locals < 0 && potentially_frameless)
    {
      /* We've seen PART of a frame setup, but not the whole deal.
         We've probably executed just the `push %ebp'.
	 Right now, ESP has our real frame base address in it.  */

      frame_unwind_register (next_frame, I386_ESP_REGNUM, buf);
      cache->base = extract_unsigned_integer (buf, 4) + cache->sp_offset;
    }

  /* Now that we have the base address for the stack frame we can
     calculate the value of %esp in the calling frame.  */
  cache->saved_sp = cache->base + 8;

  /* Adjust all the saved registers such that they contain addresses
     instead of offsets.  */
  for (i = 0; i < I386_NUM_SAVED_REGS; i++)
    if (cache->saved_regs[i] != -1)
      cache->saved_regs[i] += cache->base;

  return cache;
}

static void
i386_frame_this_id (struct frame_info *next_frame, void **this_cache,
		    struct frame_id *this_id)
{
  struct i386_frame_cache *cache = i386_frame_cache (next_frame, this_cache);

  /* This marks the outermost frame.  */
  if (cache->base == 0)
    {
      *this_id = null_frame_id;
      return;
    }
  else
    {
      ULONGEST prev_frame_addr = 0;
      if (safe_read_memory_unsigned_integer
          (cache->base, TARGET_PTR_BIT / 8, &prev_frame_addr))
        {
	  if (prev_frame_addr == 0)
	    {
	      *this_id = null_frame_id;
	      return;
	    }
        }
    }

  /* See the end of i386_push_dummy_call.  */
  (*this_id) = frame_id_build (cache->base + 8, cache->pc);
}

static void
i386_frame_prev_register (struct frame_info *next_frame, void **this_cache,
			  int regnum, int *optimizedp,
			  enum lval_type *lvalp, CORE_ADDR *addrp,
			  int *realnump, void *valuep)
{
  struct i386_frame_cache *cache = i386_frame_cache (next_frame, this_cache);

  gdb_assert (regnum >= 0);

  /* The System V ABI says that:

     "The flags register contains the system flags, such as the
     direction flag and the carry flag.  The direction flag must be
     set to the forward (that is, zero) direction before entry and
     upon exit from a function.  Other user flags have no specified
     role in the standard calling sequence and are not preserved."

     To guarantee the "upon exit" part of that statement we fake a
     saved flags register that has its direction flag cleared.

     Note that GCC doesn't seem to rely on the fact that the direction
     flag is cleared after a function return; it always explicitly
     clears the flag before operations where it matters.

     FIXME: kettenis/20030316: I'm not quite sure whether this is the
     right thing to do.  The way we fake the flags register here makes
     it impossible to change it.  */

  if (regnum == I386_EFLAGS_REGNUM)
    {
      *optimizedp = 0;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (valuep)
	{
	  ULONGEST val;

	  /* Clear the direction flag.  */
	  val = frame_unwind_register_unsigned (next_frame,
						I386_EFLAGS_REGNUM);
	  val &= ~(1 << 10);
	  store_unsigned_integer (valuep, 4, val);
	}

      return;
    }

  if (regnum == I386_EIP_REGNUM && cache->pc_in_eax)
    {
      frame_register_unwind (next_frame, I386_EAX_REGNUM,
			     optimizedp, lvalp, addrp, realnump, valuep);
      return;
    }

  if (regnum == I386_ESP_REGNUM && cache->saved_sp)
    {
      *optimizedp = 0;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (valuep)
	{
	  /* Store the value.  */
	  store_unsigned_integer (valuep, 4, cache->saved_sp);
	}
      return;
    }

  if (regnum < I386_NUM_SAVED_REGS && cache->saved_regs[regnum] != -1)
    {
      *optimizedp = 0;
      *lvalp = lval_memory;
      *addrp = cache->saved_regs[regnum];
      *realnump = -1;
      if (valuep)
	{
	  /* Read the value in from memory.  */
	  read_memory (*addrp, valuep,
		       register_size (current_gdbarch, regnum));
	}
      return;
    }

  frame_register_unwind (next_frame, regnum,
			 optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind i386_frame_unwind =
{
  NORMAL_FRAME,
  i386_frame_this_id,
  i386_frame_prev_register
};

static const struct frame_unwind *
i386_frame_sniffer (struct frame_info *next_frame)
{
  return &i386_frame_unwind;
}


/* Signal trampolines.  */

static struct i386_frame_cache *
i386_sigtramp_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct i386_frame_cache *cache;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  CORE_ADDR addr;
  char buf[4];

  if (*this_cache)
    return *this_cache;

  cache = i386_alloc_frame_cache ();

  frame_unwind_register (next_frame, I386_ESP_REGNUM, buf);
  cache->base = extract_unsigned_integer (buf, 4) - 4;

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

  *this_cache = cache;
  return cache;
}

static void
i386_sigtramp_frame_this_id (struct frame_info *next_frame, void **this_cache,
			     struct frame_id *this_id)
{
  struct i386_frame_cache *cache =
    i386_sigtramp_frame_cache (next_frame, this_cache);

  /* See the end of i386_push_dummy_call.  */
  (*this_id) = frame_id_build (cache->base + 8, frame_pc_unwind (next_frame));
}

static void
i386_sigtramp_frame_prev_register (struct frame_info *next_frame,
				   void **this_cache,
				   int regnum, int *optimizedp,
				   enum lval_type *lvalp, CORE_ADDR *addrp,
				   int *realnump, void *valuep)
{
  /* Make sure we've initialized the cache.  */
  i386_sigtramp_frame_cache (next_frame, this_cache);

  i386_frame_prev_register (next_frame, this_cache, regnum,
			    optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind i386_sigtramp_frame_unwind =
{
  SIGTRAMP_FRAME,
  i386_sigtramp_frame_this_id,
  i386_sigtramp_frame_prev_register
};

static const struct frame_unwind *
i386_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  char *name;

  /* We shouldn't even bother to try if the OSABI didn't register
     a sigcontext_addr handler.  */
  if (!gdbarch_tdep (current_gdbarch)->sigcontext_addr)
    return NULL;

  find_pc_partial_function (pc, &name, NULL, NULL);
  if (PC_IN_SIGTRAMP (pc, name))
    return &i386_sigtramp_frame_unwind;

  return NULL;
}


static CORE_ADDR
i386_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct i386_frame_cache *cache = i386_frame_cache (next_frame, this_cache);

  return cache->base;
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
  char buf[4];
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
  char buf[8];
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
i386_push_dummy_call (struct gdbarch *gdbarch, CORE_ADDR func_addr,
		      struct regcache *regcache, CORE_ADDR bp_addr, int nargs,
		      struct value **args, CORE_ADDR sp, int struct_return,
		      CORE_ADDR struct_addr)
{
  char buf[4];
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
      int len = TYPE_LENGTH (VALUE_ENCLOSING_TYPE (args[i]));
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
      int len = TYPE_LENGTH (VALUE_ENCLOSING_TYPE (args[i]));

      /* The System V ABI says that:

	 "An argument's size is increased, if necessary, to make it a
	 multiple of [32-bit] words.  This may require tail padding,
	 depending on the size of the argument."

	 This makes sure the stack says word-aligned.  */
      sp -= (len + 3) & ~3;
      write_memory (sp, VALUE_CONTENTS_ALL (args[i]), len);
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
     defenition of the stack address of a frame.  Otherwise
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
			   struct regcache *regcache, void *valbuf)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  int len = TYPE_LENGTH (type);
  char buf[I386_MAX_REGISTER_SIZE];

  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      if (tdep->st0_regnum < 0)
	{
	  warning ("Cannot find floating-point return value.");
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
	  memcpy ((char *) valbuf + low_size, buf, len - low_size);
	}
      else
	internal_error (__FILE__, __LINE__,
			"Cannot extract return value of %d bytes long.", len);
    }
}

/* Write, for architecture GDBARCH, a function return value of TYPE
   from VALBUF into REGCACHE.  */

static void
i386_store_return_value (struct gdbarch *gdbarch, struct type *type,
			 struct regcache *regcache, const void *valbuf)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  int len = TYPE_LENGTH (type);

  /* Define I387_ST0_REGNUM such that we use the proper definitions
     for the architecture.  */
#define I387_ST0_REGNUM I386_ST0_REGNUM

  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      ULONGEST fstat;
      char buf[I386_MAX_REGISTER_SIZE];

      if (tdep->st0_regnum < 0)
	{
	  warning ("Cannot set floating-point return value.");
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
				   len - low_size, (char *) valbuf + low_size);
	}
      else
	internal_error (__FILE__, __LINE__,
			"Cannot store return value of %d bytes long.", len);
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

  return (len == 1 || len == 2 || len == 4 || len == 8);
}

/* Determine, for architecture GDBARCH, how a return value of TYPE
   should be returned.  If it is supposed to be returned in registers,
   and READBUF is non-zero, read the appropriate value from REGCACHE,
   and copy it into READBUF.  If WRITEBUF is non-zero, write the value
   from WRITEBUF into REGCACHE.  */

static enum return_value_convention
i386_return_value (struct gdbarch *gdbarch, struct type *type,
		   struct regcache *regcache, void *readbuf,
		   const void *writebuf)
{
  enum type_code code = TYPE_CODE (type);

  if ((code == TYPE_CODE_STRUCT || code == TYPE_CODE_UNION)
      && !i386_reg_struct_return_p (gdbarch, type))
    return RETURN_VALUE_STRUCT_CONVENTION;

  /* This special case is for structures consisting of a single
     `float' or `double' member.  These structures are returned in
     %st(0).  For these structures, we call ourselves recursively,
     changing TYPE into the type of the first member of the structure.
     Since that should work for all structures that have only one
     member, we don't bother to check the member's type here.  */
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


static struct type *
init_vector_type (struct type *elt_type, int n)
{
  struct type *array_type;
 
  array_type = create_array_type (0, elt_type,
				  create_range_type (0, builtin_type_int,
						     0, n-1));
  TYPE_FLAGS (array_type) |= TYPE_FLAG_VECTOR;
  return array_type;
}

static struct type *
build_builtin_type_vec128i_big (void)
{
  /* 128-bit Intel SIMD registers */
  struct type *t;

  struct type *int16_big;
  struct type *int32_big;
  struct type *int64_big;
  struct type *uint128_big;

  struct type *v4_float_big;
  struct type *v2_double_big;

  struct type *v16_int8_big;
  struct type *v8_int16_big;
  struct type *v4_int32_big;
  struct type *v2_int64_big;

  int16_big = init_type (TYPE_CODE_INT, 16 / 8, 0, "int16_t", (struct objfile *) NULL);
  TYPE_BYTE_ORDER (int16_big) = BFD_ENDIAN_BIG;
  int32_big = init_type (TYPE_CODE_INT, 32 / 8, 0, "int32_t", (struct objfile *) NULL);
  TYPE_BYTE_ORDER (int32_big) = BFD_ENDIAN_BIG;
  int64_big = init_type (TYPE_CODE_INT, 64 / 8, 0, "int64_t", (struct objfile *) NULL);
  TYPE_BYTE_ORDER (int64_big) = BFD_ENDIAN_BIG;
  uint128_big = init_type (TYPE_CODE_INT, 128 / 8, TYPE_FLAG_UNSIGNED, "uint128_t", (struct objfile *) NULL);
  TYPE_BYTE_ORDER (uint128_big) = BFD_ENDIAN_BIG;

  v4_float_big = init_vector_type (builtin_type_ieee_single_big, 4);
  v2_double_big = init_vector_type (builtin_type_ieee_double_big, 2);

  v2_int64_big = init_vector_type (int64_big, 2);
  v4_int32_big = init_vector_type (int32_big, 4);
  v8_int16_big = init_vector_type (int16_big, 8);
  v16_int8_big = init_vector_type (builtin_type_int8, 16);

  t = init_composite_type ("__gdb_builtin_type_vec128i_big", TYPE_CODE_UNION);
  append_composite_type_field (t, "v4_float", v4_float_big);
  append_composite_type_field (t, "v2_double", v2_double_big);
  append_composite_type_field (t, "v16_int8", v16_int8_big);
  append_composite_type_field (t, "v8_int16", v8_int16_big);
  append_composite_type_field (t, "v4_int32", v4_int32_big);
  append_composite_type_field (t, "v2_int64", v2_int64_big);
  append_composite_type_field (t, "uint128", uint128_big);

  TYPE_FLAGS (t) |= TYPE_FLAG_VECTOR;
  TYPE_NAME (t) = "builtin_type_vec128i_big";
  return t;
}

/* Return the GDB type object for the "standard" data type of data in
   register REGNUM.  Perhaps %esi and %edi should go here, but
   potentially they could be used for things other than address.  */

static struct type *
i386_register_type (struct gdbarch *gdbarch, int regnum)
{
  if (regnum == I386_EIP_REGNUM
      || regnum == I386_EBP_REGNUM || regnum == I386_ESP_REGNUM)
    return lookup_pointer_type (builtin_type_void);

  if (i386_fp_regnum_p (regnum))
    return builtin_type_i387_ext;

  if (i386_sse_regnum_p (gdbarch, regnum))
    return builtin_type_vec128i_big;

  if (i386_mmx_regnum_p (gdbarch, regnum))
    return builtin_type_vec64i;

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
			   int regnum, void *buf)
{
  if (i386_mmx_regnum_p (gdbarch, regnum))
    {
      char mmx_buf[MAX_REGISTER_SIZE];
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
			    int regnum, const void *buf)
{
  if (i386_mmx_regnum_p (gdbarch, regnum))
    {
      char mmx_buf[MAX_REGISTER_SIZE];
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
			struct type *type, void *to)
{
  int len = TYPE_LENGTH (type);
  char *buf = to;

  /* FIXME: kettenis/20030609: What should we do if REGNUM isn't
     available in FRAME (i.e. if it wasn't saved)?  */

  if (i386_fp_regnum_p (regnum))
    {
      i387_register_to_value (frame, regnum, type, to);
      return;
    }

  /* Read a value spread accross multiple registers.  */

  gdb_assert (len > 4 && len % 4 == 0);

  while (len > 0)
    {
      gdb_assert (regnum != -1);
      gdb_assert (register_size (current_gdbarch, regnum) == 4);

      get_frame_register (frame, regnum, buf);
      regnum = i386_next_regnum (regnum);
      len -= 4;
      buf += 4;
    }
}

/* Write the contents FROM of a value of type TYPE into register
   REGNUM in frame FRAME.  */

static void
i386_value_to_register (struct frame_info *frame, int regnum,
			struct type *type, const void *from)
{
  int len = TYPE_LENGTH (type);
  const char *buf = from;

  if (i386_fp_regnum_p (regnum))
    {
      i387_value_to_register (frame, regnum, type, from);
      return;
    }

  /* Write a value spread accross multiple registers.  */

  gdb_assert (len > 4 && len % 4 == 0);

  while (len > 0)
    {
      gdb_assert (regnum != -1);
      gdb_assert (register_size (current_gdbarch, regnum) == 4);

      put_frame_register (frame, regnum, buf);
      regnum = i386_next_regnum (regnum);
      len -= 4;
      buf += 4;
    }
}

/* Supply register REGNUM from the general-purpose register set REGSET
   to register cache REGCACHE.  If REGNUM is -1, do this for all
   registers in REGSET.  */

void
i386_supply_gregset (const struct regset *regset, struct regcache *regcache,
		     int regnum, const void *gregs, size_t len)
{
  const struct gdbarch_tdep *tdep = regset->descr;
  const char *regs = gregs;
  int i;

  gdb_assert (len == tdep->sizeof_gregset);

  for (i = 0; i < tdep->gregset_num_regs; i++)
    {
      if ((regnum == i || regnum == -1)
	  && tdep->gregset_reg_offset[i] != -1)
	regcache_raw_supply (regcache, i, regs + tdep->gregset_reg_offset[i]);
    }
}

/* Supply register REGNUM from the floating-point register set REGSET
   to register cache REGCACHE.  If REGNUM is -1, do this for all
   registers in REGSET.  */

static void
i386_supply_fpregset (const struct regset *regset, struct regcache *regcache,
		      int regnum, const void *fpregs, size_t len)
{
  const struct gdbarch_tdep *tdep = regset->descr;

  if (len == I387_SIZEOF_FXSAVE)
    {
      i387_supply_fxsave (regcache, regnum, fpregs);
      return;
    }

  gdb_assert (len == tdep->sizeof_fpregset);
  i387_supply_fsave (regcache, regnum, fpregs);
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
	{
	  tdep->gregset = XMALLOC (struct regset);
	  tdep->gregset->descr = tdep;
	  tdep->gregset->supply_regset = i386_supply_gregset;
	}
      return tdep->gregset;
    }

  if ((strcmp (sect_name, ".reg2") == 0 && sect_size == tdep->sizeof_fpregset)
      || (strcmp (sect_name, ".reg-xfp") == 0
	  && sect_size == I387_SIZEOF_FXSAVE))
    {
      if (tdep->fpregset == NULL)
	{
	  tdep->fpregset = XMALLOC (struct regset);
	  tdep->fpregset->descr = tdep;
	  tdep->fpregset->supply_regset = i386_supply_fpregset;
	}
      return tdep->fpregset;
    }

  return NULL;
}


#ifdef STATIC_TRANSFORM_NAME
/* SunPRO encodes the static variables.  This is not related to C++
   mangling, it is done for C too.  */

char *
sunpro_static_transform_name (char *name)
{
  char *p;
  if (IS_STATIC_TRANSFORM_NAME (name))
    {
      /* For file-local statics there will be a period, a bunch of
         junk (the contents of which match a string given in the
         N_OPT), a period and the name.  For function-local statics
         there will be a bunch of junk (which seems to change the
         second character from 'A' to 'B'), a period, the name of the
         function, and the name.  So just skip everything before the
         last period.  */
      p = strrchr (name, '.');
      if (p != NULL)
	name = p + 1;
    }
  return name;
}
#endif /* STATIC_TRANSFORM_NAME */



/* Stuff for WIN32 PE style DLL's but is pretty generic really. */

CORE_ADDR
i386_pe_skip_trampoline_code (CORE_ADDR pc, char *name)
{
  if (pc && read_memory_unsigned_integer (pc, 2) == 0x25ff) /* jmp *(dest) */
    {
      unsigned long indirect = read_memory_unsigned_integer (pc + 2, 4);
      struct minimal_symbol *indsym =
	indirect ? lookup_minimal_symbol_by_pc (indirect) : 0;
      char *symname = indsym ? SYMBOL_LINKAGE_NAME (indsym) : 0;

      if (symname)
	{
	  if (strncmp (symname, "__imp_", 6) == 0
	      || strncmp (symname, "_imp_", 5) == 0)
	    return name ? 1 : read_memory_unsigned_integer (indirect, 4);
	}
    }
  return 0;			/* Not a trampoline.  */
}


/* Return non-zero if PC and NAME show that we are in a signal
   trampoline.  */

static int
i386_pc_in_sigtramp (CORE_ADDR pc, char *name)
{
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


/* There are a few i386 architecture variants that differ only
   slightly from the generic i386 target.  For now, we don't give them
   their own source file, but include them here.  As a consequence,
   they'll always be included.  */

/* System V Release 4 (SVR4).  */

static int
i386_svr4_pc_in_sigtramp (CORE_ADDR pc, char *name)
{
  /* UnixWare uses _sigacthandler.  The origin of the other symbols is
     currently unknown.  */
  return (name && (strcmp ("_sigreturn", name) == 0
		   || strcmp ("_sigacthandler", name) == 0
		   || strcmp ("sigvechandler", name) == 0));
}

/* Assuming NEXT_FRAME is for a frame following a SVR4 sigtramp
   routine, return the address of the associated sigcontext (ucontext)
   structure.  */

static CORE_ADDR
i386_svr4_sigcontext_addr (struct frame_info *next_frame)
{
  char buf[4];
  CORE_ADDR sp;

  frame_unwind_register (next_frame, I386_ESP_REGNUM, buf);
  sp = extract_unsigned_integer (buf, 4);

  return read_memory_unsigned_integer (sp + 8, 4);
}


/* DJGPP.  */

static int
i386_go32_pc_in_sigtramp (CORE_ADDR pc, char *name)
{
  /* DJGPP doesn't have any special frames for signal handlers.  */
  return 0;
}


/* Generic ELF.  */

void
i386_elf_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* We typically use stabs-in-ELF with the DWARF register numbering.  */
  set_gdbarch_stab_reg_to_regnum (gdbarch, i386_dwarf_reg_to_regnum);
}

/* System V Release 4 (SVR4).  */

void
i386_svr4_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* System V Release 4 uses ELF.  */
  i386_elf_init_abi (info, gdbarch);

  /* System V Release 4 has shared libraries.  */
  set_gdbarch_in_solib_call_trampoline (gdbarch, in_plt_section);
  set_gdbarch_skip_trampoline_code (gdbarch, find_solib_trampoline_target);

  set_gdbarch_pc_in_sigtramp (gdbarch, i386_svr4_pc_in_sigtramp);
  tdep->sigcontext_addr = i386_svr4_sigcontext_addr;
  tdep->sc_pc_offset = 36 + 14 * 4;
  tdep->sc_sp_offset = 36 + 17 * 4;

  tdep->jb_pc_offset = 20;
}

/* DJGPP.  */

static void
i386_go32_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  set_gdbarch_pc_in_sigtramp (gdbarch, i386_go32_pc_in_sigtramp);

  tdep->jb_pc_offset = 36;
}

/* NetWare.  */

static void
i386_nw_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->jb_pc_offset = 24;
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
     and the SSE registers.  This can be overidden for a specific ABI
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
     caclulating the register number for %mm0 until we know the number
     of raw registers.  */
  tdep->mm0_regnum = 0;

  /* I386_NUM_XREGS includes %mxcsr, so substract one.  */
  tdep->num_xmm_regs = I386_NUM_XREGS - 1;

  tdep->jb_pc_offset = -1;
  tdep->struct_return = pcc_struct_return;
  tdep->sigtramp_start = 0;
  tdep->sigtramp_end = 0;
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
  set_gdbarch_long_double_bit (gdbarch, 96);

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

  /* Use the "default" register numbering scheme for stabs and COFF.  */
  set_gdbarch_stab_reg_to_regnum (gdbarch, i386_stab_reg_to_regnum);
  set_gdbarch_sdb_reg_to_regnum (gdbarch, i386_stab_reg_to_regnum);

  /* Use the DWARF register numbering scheme for DWARF and DWARF 2.  */
  set_gdbarch_dwarf_reg_to_regnum (gdbarch, i386_dwarf_reg_to_regnum);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, i386_dwarf_reg_to_regnum);

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
  set_gdbarch_pc_in_sigtramp (gdbarch, i386_pc_in_sigtramp);

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

static enum gdb_osabi
i386_coff_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "coff-go32-exe") == 0
      || strcmp (bfd_get_target (abfd), "coff-go32") == 0)
    return GDB_OSABI_GO32;

  return GDB_OSABI_UNKNOWN;
}

static enum gdb_osabi
i386_nlm_osabi_sniffer (bfd *abfd)
{
  return GDB_OSABI_NETWARE;
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
  struct i386_frame_cache *cache;
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

  cache = i386_alloc_frame_cache ();

  cache->saved_regs[I386_EBP_REGNUM] = -1;
  cache->locals = -1;

  parsed_to = i386_analyze_frame_setup (start_address, end_address + 1, cache);
  
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
      printf_filtered ("Prologue parser parsed to address 0x%s",
                       paddr_nz (parsed_to));
      if (parsed_to == end_address -1)
        printf_filtered (" which is the entire length of the function\n");
      else
        {
          printf_filtered (".\n");
          if (cache->saved_regs[I386_EBP_REGNUM] == -1)
            {
              printf_filtered ("Didn't find push %%ebp and didn't parse the full range: prologue parse failed (frameless function?)\n");
              parse_failed = 1;
            }
        }
    }

  printf_filtered ("\n");
  if (cache->saved_regs[I386_EBP_REGNUM] == -1)
    {
      printf_filtered ("Did not find the push %%ebp\n");
      parse_failed = 1;
    }
  else
    printf_filtered ("Found push %%ebp\n");
  if (cache->locals == -1)
    {
      printf_filtered ("Did not find mov %%esp, %%ebp\n");
      parse_failed = 1;
    }
  if (cache->locals >= 0)
    printf_filtered ("Found mov %%esp, %%ebp\n");
  if (cache->locals > 0)
      printf_filtered ("Found local storage setup\n");

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
  {
    struct cmd_list_element *new_cmd;

    new_cmd = add_set_enum_cmd ("disassembly-flavor", no_class,
				valid_flavors,
				&disassembly_flavor,
				"\
Set the disassembly flavor, the valid values are \"att\" and \"intel\", \
and the default value is \"att\".",
				&setlist);
    add_show_from_set (new_cmd, &showlist);
  }

  /* Add the variable that controls the convention for returning
     structs.  */
  {
    struct cmd_list_element *new_cmd;

    new_cmd = add_set_enum_cmd ("struct-convention", no_class,
				valid_conventions,
				&struct_convention, "\
Set the convention for returning small structs, valid values \
are \"default\", \"pcc\" and \"reg\", and the default value is \"default\".",
                                &setlist);
    add_show_from_set (new_cmd, &showlist);
  }

  /* APPLE LOCAL: maint i386-prologue-parser */
  add_cmd ("i386-prologue-parser", class_maintenance, 
           maintenance_i386_prologue_parser, 
           "Run the i386 prologue analyzer on a function.\n"
           "arg1 is start address of function\n"
           "arg2 is optional end-address, defaulting to startaddr + 32 bytes.", 
           &maintenancelist);


  gdbarch_register_osabi_sniffer (bfd_arch_i386, bfd_target_coff_flavour,
				  i386_coff_osabi_sniffer);
  gdbarch_register_osabi_sniffer (bfd_arch_i386, bfd_target_nlm_flavour,
				  i386_nlm_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_i386, 0, GDB_OSABI_SVR4,
			  i386_svr4_init_abi);
  gdbarch_register_osabi (bfd_arch_i386, 0, GDB_OSABI_GO32,
			  i386_go32_init_abi);
  gdbarch_register_osabi (bfd_arch_i386, 0, GDB_OSABI_NETWARE,
			  i386_nw_init_abi);

  /* Initialize the i386 specific register groups.  */
  i386_init_reggroups ();
}
