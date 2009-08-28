/* Target-dependent code for AMD64.

   Copyright 2001, 2002, 2003, 2004, 2005 Free Software Foundation,
   Inc.  Contributed by Jiri Smid, SuSE Labs.

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
#include "block.h"
#include "dummy-frame.h"
#include "frame.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "inferior.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "objfiles.h"
#include "regcache.h"
#include "regset.h"
#include "symfile.h"
#include "complaints.h"

#include "gdb_assert.h"

#include "amd64-tdep.h"
#include "i387-tdep.h"
#include "x86-shared-tdep.h"

/* Note that the AMD64 architecture was previously known as x86-64.
   The latter is (forever) engraved into the canonical system name as
   returned by config.guess, and used as the name for the AMD64 port
   of GNU/Linux.  The BSD's have renamed their ports to amd64; they
   don't like to shout.  For GDB we prefer the amd64_-prefix over the
   x86_64_-prefix since it's so much easier to type.  */

/* Register information.  */

struct amd64_register_info
{
  char *name;
  struct type **type;
};

/* APPLE LOCAL: We'll compare this to NULL later on.  */
static struct type *amd64_sse_type = NULL;

static struct amd64_register_info const amd64_register_info[] =
{
  { "rax", &builtin_type_int64 },
  { "rbx", &builtin_type_int64 },
  { "rcx", &builtin_type_int64 },
  { "rdx", &builtin_type_int64 },
  { "rsi", &builtin_type_int64 },
  { "rdi", &builtin_type_int64 },
  { "rbp", &builtin_type_void_data_ptr },
  { "rsp", &builtin_type_void_data_ptr },

  /* %r8 is indeed register number 8.  */
  { "r8", &builtin_type_int64 },
  { "r9", &builtin_type_int64 },
  { "r10", &builtin_type_int64 },
  { "r11", &builtin_type_int64 },
  { "r12", &builtin_type_int64 },
  { "r13", &builtin_type_int64 },
  { "r14", &builtin_type_int64 },
  { "r15", &builtin_type_int64 },
  { "rip", &builtin_type_void_func_ptr },
  { "eflags", &builtin_type_int32 },
  { "cs", &builtin_type_int32 },
  { "ss", &builtin_type_int32 },
  { "ds", &builtin_type_int32 },
  { "es", &builtin_type_int32 },
  { "fs", &builtin_type_int32 },
  { "gs", &builtin_type_int32 },

  /* %st0 is register number 24.  */
  { "st0", &builtin_type_i387_ext },
  { "st1", &builtin_type_i387_ext },
  { "st2", &builtin_type_i387_ext },
  { "st3", &builtin_type_i387_ext },
  { "st4", &builtin_type_i387_ext },
  { "st5", &builtin_type_i387_ext },
  { "st6", &builtin_type_i387_ext },
  { "st7", &builtin_type_i387_ext },
  { "fctrl", &builtin_type_int32 },
  { "fstat", &builtin_type_int32 },
  { "ftag", &builtin_type_int32 },
  { "fiseg", &builtin_type_int32 },
  { "fioff", &builtin_type_int32 },
  { "foseg", &builtin_type_int32 },
  { "fooff", &builtin_type_int32 },
  { "fop", &builtin_type_int32 },

  /* %xmm0 is register number 40.  */
  { "xmm0", &amd64_sse_type },
  { "xmm1", &amd64_sse_type },
  { "xmm2", &amd64_sse_type },
  { "xmm3", &amd64_sse_type },
  { "xmm4", &amd64_sse_type },
  { "xmm5", &amd64_sse_type },
  { "xmm6", &amd64_sse_type },
  { "xmm7", &amd64_sse_type },
  { "xmm8", &amd64_sse_type },
  { "xmm9", &amd64_sse_type },
  { "xmm10", &amd64_sse_type },
  { "xmm11", &amd64_sse_type },
  { "xmm12", &amd64_sse_type },
  { "xmm13", &amd64_sse_type },
  { "xmm14", &amd64_sse_type },
  { "xmm15", &amd64_sse_type },
  { "mxcsr", &builtin_type_int32 }
};

/* Total number of registers.  */
#define AMD64_NUM_REGS \
  (sizeof (amd64_register_info) / sizeof (amd64_register_info[0]))

/* Return the name of register REGNUM.  */

static const char *
amd64_register_name (int regnum)
{
  if (regnum >= 0 && regnum < AMD64_NUM_REGS)
    return amd64_register_info[regnum].name;

  return NULL;
}

/* Return the GDB type object for the "standard" data type of data in
   register REGNUM. */

static struct type *
amd64_register_type (struct gdbarch *gdbarch, int regnum)
{
  gdb_assert (regnum >= 0 && regnum < AMD64_NUM_REGS);

  /* APPLE LOCAL: This would more appropriately be done in
     amd64_init_abi() but the type system isn't initialized far
     enough for build_builtin_type_vec128i_big () to execute at
     that point so we need to do it lazily here.  */

  if (amd64_sse_type == NULL)
    amd64_sse_type = build_builtin_type_vec128i_big ();

  return *amd64_register_info[regnum].type;
}

/* DWARF Register Number Mapping as defined in the System V psABI,
   section 3.6.  */

static int amd64_dwarf_regmap[] =
{
  /* General Purpose Registers RAX, RDX, RCX, RBX, RSI, RDI.  */
  AMD64_RAX_REGNUM, AMD64_RDX_REGNUM,
  AMD64_RCX_REGNUM, AMD64_RBX_REGNUM,
  AMD64_RSI_REGNUM, AMD64_RDI_REGNUM,

  /* Frame Pointer Register RBP.  */
  AMD64_RBP_REGNUM,

  /* Stack Pointer Register RSP.  */
  AMD64_RSP_REGNUM,

  /* Extended Integer Registers 8 - 15.  */
  8, 9, 10, 11, 12, 13, 14, 15,

  /* Return Address RA.  Mapped to RIP.  */
  AMD64_RIP_REGNUM,

  /* SSE Registers 0 - 7.  */
  AMD64_XMM0_REGNUM + 0, AMD64_XMM1_REGNUM,
  AMD64_XMM0_REGNUM + 2, AMD64_XMM0_REGNUM + 3,
  AMD64_XMM0_REGNUM + 4, AMD64_XMM0_REGNUM + 5,
  AMD64_XMM0_REGNUM + 6, AMD64_XMM0_REGNUM + 7,

  /* Extended SSE Registers 8 - 15.  */
  AMD64_XMM0_REGNUM + 8, AMD64_XMM0_REGNUM + 9,
  AMD64_XMM0_REGNUM + 10, AMD64_XMM0_REGNUM + 11,
  AMD64_XMM0_REGNUM + 12, AMD64_XMM0_REGNUM + 13,
  AMD64_XMM0_REGNUM + 14, AMD64_XMM0_REGNUM + 15,

  /* Floating Point Registers 0-7.  */
  AMD64_ST0_REGNUM + 0, AMD64_ST0_REGNUM + 1,
  AMD64_ST0_REGNUM + 2, AMD64_ST0_REGNUM + 3,
  AMD64_ST0_REGNUM + 4, AMD64_ST0_REGNUM + 5,
  AMD64_ST0_REGNUM + 6, AMD64_ST0_REGNUM + 7
};

static const int amd64_dwarf_regmap_len =
  (sizeof (amd64_dwarf_regmap) / sizeof (amd64_dwarf_regmap[0]));

/* Convert DWARF register number REG to the appropriate register
   number used by GDB.  */

static int
amd64_dwarf_reg_to_regnum (int reg)
{
  int regnum = -1;

  if (reg >= 0 && reg < amd64_dwarf_regmap_len)
    regnum = amd64_dwarf_regmap[reg];

  /* APPLE LOCAL: Make this a complaint.
     We can hit this when trying to initialize the table of registers for
     a CFI program run.  See the comment at the top of dwarf2_frame_cache() */
  if (regnum == -1)
    complaint (&symfile_complaints, _("Unmapped DWARF Register #%d encountered."), reg);

  return regnum;
}

/* APPLE LOCAL: Read part of a register with extra swapping.

   The developer's view of the XMM registers is byteswapped from how
   it actually is -- an extra swapping on top of the usual little endian
   fun -- and so when we fetch/store the registers from the inferior
   we swap them so we're using the "user's view" of the registers.

   The main problem that this causes is that several functions in this file
   know how to store/retrieve values from the XMM registers as per the ABI
   conventions - and those conventions are written to the actual byte order
   of the XMM registers, not the user's expected view.  So we do an extra
   swapping of the reg values we've retrieved before/after changing them
   for ABI reasons.  */

static void
swapped_regcache_raw_write_part (struct regcache *regcache, int regnum,
                                 int offset, int len, const gdb_byte *buf)
{
  int j;
  gdb_byte swapper_buf[16];

  if (regnum < AMD64_XMM0_REGNUM || regnum > AMD64_XMM0_REGNUM + 15)
    {
       regcache_raw_write_part (regcache, regnum, offset, len, buf);
       return;
    }

  regcache_raw_read (regcache, regnum, swapper_buf);
  for (j = 0; j < 8; j++)
    {
      gdb_byte tmp = swapper_buf[j];
      swapper_buf[j] = swapper_buf[16 - j - 1];
      swapper_buf[16 - j - 1] = tmp;
    }

  memcpy (&swapper_buf[offset], buf, len);
  for (j = 0; j < 8; j++)
     {
       gdb_byte tmp = swapper_buf[j];
       swapper_buf[j] = swapper_buf[16 - j - 1];
       swapper_buf[16 - j - 1] = tmp;
     }
  regcache_raw_write (regcache, regnum, swapper_buf);
}

/* APPLE LOCAL: Read part of a register with extra swapping.

   The developer's view of the XMM registers is byteswapped from how
   it actually is -- an extra swapping on top of the usual little endian
   fun -- and so when we fetch/store the registers from the inferior
   we swap them so we're using the "user's view" of the registers.

   The main problem that this causes is that several functions in this file
   know how to store/retrieve values from the XMM registers as per the ABI
   conventions - and those conventions are written to the actual byte order
   of the XMM registers, not the user's expected view.  So we do an extra
   swapping of the reg values we've retrieved before/after changing them
   for ABI reasons.  */

static void
swapped_regcache_raw_read_part (struct regcache *regcache, int regnum,
                                int offset, int len, gdb_byte *buf)
{
  int j;
  gdb_byte swapper_buf[16];

  if (regnum < AMD64_XMM0_REGNUM || regnum > AMD64_XMM0_REGNUM + 15)
    {
       regcache_raw_read_part (regcache, regnum, offset, len, buf);
       return;
    }

  regcache_raw_read (regcache, regnum, swapper_buf);
  for (j = 0; j < 8; j++)
    {
      gdb_byte tmp = swapper_buf[j];
      swapper_buf[j] = swapper_buf[16 - j - 1];
      swapper_buf[16 - j - 1] = tmp;
    }

  memcpy (buf, &swapper_buf[offset], len);
}

/* Return nonzero if a value of type TYPE stored in register REGNUM
   needs any special handling.  */

static int
amd64_convert_register_p (int regnum, struct type *type)
{
  return i386_fp_regnum_p (regnum);
}


/* Register classes as defined in the psABI.  */

enum amd64_reg_class
{
  AMD64_INTEGER,
  AMD64_SSE,
  AMD64_SSEUP,
  AMD64_X87,
  AMD64_X87UP,
  AMD64_COMPLEX_X87,
  AMD64_NO_CLASS,
  AMD64_MEMORY
};

/* Return the union class of CLASS1 and CLASS2.  See the psABI for
   details.  */

static enum amd64_reg_class
amd64_merge_classes (enum amd64_reg_class class1, enum amd64_reg_class class2)
{
  /* Rule (a): If both classes are equal, this is the resulting class.  */
  if (class1 == class2)
    return class1;

  /* Rule (b): If one of the classes is NO_CLASS, the resulting class
     is the other class.  */
  if (class1 == AMD64_NO_CLASS)
    return class2;
  if (class2 == AMD64_NO_CLASS)
    return class1;

  /* Rule (c): If one of the classes is MEMORY, the result is MEMORY.  */
  if (class1 == AMD64_MEMORY || class2 == AMD64_MEMORY)
    return AMD64_MEMORY;

  /* Rule (d): If one of the classes is INTEGER, the result is INTEGER.  */
  if (class1 == AMD64_INTEGER || class2 == AMD64_INTEGER)
    return AMD64_INTEGER;

  /* Rule (e): If one of the classes is X87, X87UP, COMPLEX_X87 class,
     MEMORY is used as class.  */
  if (class1 == AMD64_X87 || class1 == AMD64_X87UP
      || class1 == AMD64_COMPLEX_X87 || class2 == AMD64_X87
      || class2 == AMD64_X87UP || class2 == AMD64_COMPLEX_X87)
    return AMD64_MEMORY;

  /* Rule (f): Otherwise class SSE is used.  */
  return AMD64_SSE;
}

static void amd64_classify (struct type *type, enum amd64_reg_class class[2]);

/* Return non-zero if TYPE is a non-POD structure or union type.  */

static int
amd64_non_pod_p (struct type *type)
{
  /* ??? A class with a base class certainly isn't POD, but does this
     catch all non-POD structure types?  */
  if (TYPE_CODE (type) == TYPE_CODE_STRUCT && TYPE_N_BASECLASSES (type) > 0)
    return 1;

  return 0;
}

/* Classify TYPE according to the rules for aggregate (structures and
   arrays) and union types, and store the result in CLASS.  */

static void
amd64_classify_aggregate (struct type *type, enum amd64_reg_class class[2])
{
  int len = TYPE_LENGTH (type);

  /* 1. If the size of an object is larger than two eightbytes, or in
        C++, is a non-POD structure or union type, or contains
        unaligned fields, it has class memory.  */
  if (len > 16 || amd64_non_pod_p (type))
    {
      class[0] = class[1] = AMD64_MEMORY;
      return;
    }

  /* 2. Both eightbytes get initialized to class NO_CLASS.  */
  class[0] = class[1] = AMD64_NO_CLASS;

  /* 3. Each field of an object is classified recursively so that
        always two fields are considered. The resulting class is
        calculated according to the classes of the fields in the
        eightbyte: */

  if (TYPE_CODE (type) == TYPE_CODE_ARRAY)
    {
      struct type *subtype = check_typedef (TYPE_TARGET_TYPE (type));

      /* All fields in an array have the same type.  */
      amd64_classify (subtype, class);
      if (len > 8 && class[1] == AMD64_NO_CLASS)
	class[1] = class[0];
    }
  else
    {
      int i;

      /* Structure or union.  */
      gdb_assert (TYPE_CODE (type) == TYPE_CODE_STRUCT
		  || TYPE_CODE (type) == TYPE_CODE_UNION);

      for (i = 0; i < TYPE_NFIELDS (type); i++)
	{
	  struct type *subtype = check_typedef (TYPE_FIELD_TYPE (type, i));
	  int pos = TYPE_FIELD_BITPOS (type, i) / 64;
	  enum amd64_reg_class subclass[2];

	  /* Ignore static fields.  */
	  if (TYPE_FIELD_STATIC (type, i))
	    continue;

	  gdb_assert (pos == 0 || pos == 1);

	  amd64_classify (subtype, subclass);
	  class[pos] = amd64_merge_classes (class[pos], subclass[0]);
	  if (pos == 0)
	    class[1] = amd64_merge_classes (class[1], subclass[1]);
	}
    }

  /* 4. Then a post merger cleanup is done:  */

  /* Rule (a): If one of the classes is MEMORY, the whole argument is
     passed in memory.  */
  if (class[0] == AMD64_MEMORY || class[1] == AMD64_MEMORY)
    class[0] = class[1] = AMD64_MEMORY;

  /* Rule (b): If SSEUP is not preceeded by SSE, it is converted to
     SSE.  */
  if (class[0] == AMD64_SSEUP)
    class[0] = AMD64_SSE;
  if (class[1] == AMD64_SSEUP && class[0] != AMD64_SSE)
    class[1] = AMD64_SSE;
}

/* Classify TYPE, and store the result in CLASS.  */

static void
amd64_classify (struct type *type, enum amd64_reg_class class[2])
{
  enum type_code code = TYPE_CODE (type);
  int len = TYPE_LENGTH (type);

  class[0] = class[1] = AMD64_NO_CLASS;

  /* Arguments of types (signed and unsigned) _Bool, char, short, int,
     long, long long, and pointers are in the INTEGER class.  Similarly,
     range types, used by languages such as Ada, are also in the INTEGER
     class.  */
  if ((code == TYPE_CODE_INT || code == TYPE_CODE_ENUM
       || code == TYPE_CODE_RANGE || code == TYPE_CODE_BOOL
       || code == TYPE_CODE_PTR || code == TYPE_CODE_REF)
      && (len == 1 || len == 2 || len == 4 || len == 8))
    class[0] = AMD64_INTEGER;

  /* Arguments of types float, double and __m64 are in class SSE.  */
  else if (code == TYPE_CODE_FLT && (len == 4 || len == 8))
    /* FIXME: __m64 .  */
    class[0] = AMD64_SSE;

  /* Arguments of types __float128 and __m128 are split into two
     halves.  The least significant ones belong to class SSE, the most
     significant one to class SSEUP.  */
  /* FIXME: __float128, __m128.  */

  /* The 64-bit mantissa of arguments of type long double belongs to
     class X87, the 16-bit exponent plus 6 bytes of padding belongs to
     class X87UP.  */
  else if (code == TYPE_CODE_FLT && len == 16)
    /* Class X87 and X87UP.  */
    class[0] = AMD64_X87, class[1] = AMD64_X87UP;

  /* Aggregates.  */
  else if (code == TYPE_CODE_ARRAY || code == TYPE_CODE_STRUCT
	   || code == TYPE_CODE_UNION)
    amd64_classify_aggregate (type, class);
}

static enum return_value_convention
amd64_return_value (struct gdbarch *gdbarch, struct type *type,
		    struct regcache *regcache,
		    gdb_byte *readbuf, const gdb_byte *writebuf)
{
  enum amd64_reg_class class[2];
  int len = TYPE_LENGTH (type);
  static int integer_regnum[] = { AMD64_RAX_REGNUM, AMD64_RDX_REGNUM };
  static int sse_regnum[] = { AMD64_XMM0_REGNUM, AMD64_XMM1_REGNUM };
  int integer_reg = 0;
  int sse_reg = 0;
  int i;

  gdb_assert (!(readbuf && writebuf));

  /* 1. Classify the return type with the classification algorithm.  */
  amd64_classify (type, class);

  /* 2. If the type has class MEMORY, then the caller provides space
     for the return value and passes the address of this storage in
     %rdi as if it were the first argument to the function. In effect,
     this address becomes a hidden first argument.

     On return %rax will contain the address that has been passed in
     by the caller in %rdi.  */
  if (class[0] == AMD64_MEMORY)
    {
      /* As indicated by the comment above, the ABI guarantees that we
         can always find the return value just after the function has
         returned.  */

      if (readbuf)
	{
	  ULONGEST addr;

	  regcache_raw_read_unsigned (regcache, AMD64_RAX_REGNUM, &addr);
	  read_memory (addr, readbuf, TYPE_LENGTH (type));
	}

      return RETURN_VALUE_ABI_RETURNS_ADDRESS;
    }

  gdb_assert (class[1] != AMD64_MEMORY);
  gdb_assert (len <= 16);

  for (i = 0; len > 0; i++, len -= 8)
    {
      int regnum = -1;
      int offset = 0;

      switch (class[i])
	{
	case AMD64_INTEGER:
	  /* 3. If the class is INTEGER, the next available register
	     of the sequence %rax, %rdx is used.  */
	  regnum = integer_regnum[integer_reg++];
	  break;

	case AMD64_SSE:
	  /* 4. If the class is SSE, the next available SSE register
             of the sequence %xmm0, %xmm1 is used.  */
	  regnum = sse_regnum[sse_reg++];
	  break;

	case AMD64_SSEUP:
	  /* 5. If the class is SSEUP, the eightbyte is passed in the
	     upper half of the last used SSE register.  */
	  gdb_assert (sse_reg > 0);
	  regnum = sse_regnum[sse_reg - 1];
	  offset = 8;
	  break;

	case AMD64_X87:
	  /* 6. If the class is X87, the value is returned on the X87
             stack in %st0 as 80-bit x87 number.  */
	  regnum = AMD64_ST0_REGNUM;
	  if (writebuf)
	    i387_return_value (gdbarch, regcache);
	  break;

	case AMD64_X87UP:
	  /* 7. If the class is X87UP, the value is returned together
             with the previous X87 value in %st0.  */
	  gdb_assert (i > 0 && class[0] == AMD64_X87);
	  regnum = AMD64_ST0_REGNUM;
	  offset = 8;
	  len = 2;
	  break;

	case AMD64_NO_CLASS:
	  continue;

	default:
	  gdb_assert (!"Unexpected register class.");
	}

      gdb_assert (regnum != -1);

      /* APPLE LOCAL: We keep the XMM registers in the "user's view"
         byte order inside gdb so we need to unswap them before the
         ABI read/writes which assume the actual machine byte order.  */

      if ((readbuf || writebuf) 
          && (regnum == sse_regnum[0] || regnum == sse_regnum[1]))
        {
          if (readbuf)
	    swapped_regcache_raw_read_part (regcache, regnum, offset, 
                                    min (len, 8), readbuf + i * 8);
          if (writebuf)
	    swapped_regcache_raw_write_part (regcache, regnum, offset, 
                                     min (len, 8), writebuf + i * 8);
          continue;
        }

      if (readbuf)
	regcache_raw_read_part (regcache, regnum, offset, min (len, 8),
				readbuf + i * 8);
      if (writebuf)
	regcache_raw_write_part (regcache, regnum, offset, min (len, 8),
				 writebuf + i * 8);
    }

  return RETURN_VALUE_REGISTER_CONVENTION;
}


static CORE_ADDR
amd64_push_arguments (struct regcache *regcache, int nargs,
		      struct value **args, CORE_ADDR sp, int struct_return)
{
  static int integer_regnum[] =
  {
    AMD64_RDI_REGNUM,		/* %rdi */
    AMD64_RSI_REGNUM,		/* %rsi */
    AMD64_RDX_REGNUM,		/* %rdx */
    AMD64_RCX_REGNUM,		/* %rcx */
    8,				/* %r8 */
    9				/* %r9 */
  };
  static int sse_regnum[] =
  {
    /* %xmm0 ... %xmm7 */
    AMD64_XMM0_REGNUM + 0, AMD64_XMM1_REGNUM,
    AMD64_XMM0_REGNUM + 2, AMD64_XMM0_REGNUM + 3,
    AMD64_XMM0_REGNUM + 4, AMD64_XMM0_REGNUM + 5,
    AMD64_XMM0_REGNUM + 6, AMD64_XMM0_REGNUM + 7,
  };
  struct value **stack_args = alloca (nargs * sizeof (struct value *));
  int num_stack_args = 0;
  int num_elements = 0;
  int element = 0;
  int integer_reg = 0;
  int sse_reg = 0;
  int i;

  /* Reserve a register for the "hidden" argument.  */
  if (struct_return)
    integer_reg++;

  for (i = 0; i < nargs; i++)
    {
      struct type *type = value_type (args[i]);
      int len = TYPE_LENGTH (type);
      enum amd64_reg_class class[2];
      int needed_integer_regs = 0;
      int needed_sse_regs = 0;
      int j;

      /* Classify argument.  */
      amd64_classify (type, class);

      /* Calculate the number of integer and SSE registers needed for
         this argument.  */
      for (j = 0; j < 2; j++)
	{
	  if (class[j] == AMD64_INTEGER)
	    needed_integer_regs++;
	  else if (class[j] == AMD64_SSE)
	    needed_sse_regs++;
	}

      /* Check whether enough registers are available, and if the
         argument should be passed in registers at all.  */
      if (integer_reg + needed_integer_regs > ARRAY_SIZE (integer_regnum)
	  || sse_reg + needed_sse_regs > ARRAY_SIZE (sse_regnum)
	  || (needed_integer_regs == 0 && needed_sse_regs == 0))
	{
	  /* The argument will be passed on the stack.  */
	  num_elements += ((len + 7) / 8);
	  stack_args[num_stack_args++] = args[i];
	}
      else
	{
	  /* The argument will be passed in registers.  */
	  const gdb_byte *valbuf = value_contents (args[i]);
	  gdb_byte buf[8];

	  gdb_assert (len <= 16);

	  for (j = 0; len > 0; j++, len -= 8)
	    {
	      int regnum = -1;
	      int offset = 0;

	      switch (class[j])
		{
		case AMD64_INTEGER:
		  regnum = integer_regnum[integer_reg++];
		  break;

		case AMD64_SSE:
		  regnum = sse_regnum[sse_reg++];
		  break;

		case AMD64_SSEUP:
		  gdb_assert (sse_reg > 0);
		  regnum = sse_regnum[sse_reg - 1];
		  offset = 8;
		  break;

		default:
		  gdb_assert (!"Unexpected register class.");
		}

	      gdb_assert (regnum != -1);
	      memset (buf, 0, sizeof buf);
	      memcpy (buf, valbuf + j * 8, min (len, 8));

              /* APPLE LOCAL: We keep the XMM registers in the "user's view"
                 byte order inside gdb so we need to unswap them before the  
                 ABI read/writes which assume the actual machine byte order.  */

              if (class[j] == AMD64_SSE || class[j] == AMD64_SSEUP)
	        swapped_regcache_raw_write_part (regcache, regnum, offset, 8, 
                                                 buf);
              else
	        regcache_raw_write_part (regcache, regnum, offset, 8, buf);
	    }
	}
    }

  /* Allocate space for the arguments on the stack.  */
  sp -= num_elements * 8;

  /* The psABI says that "The end of the input argument area shall be
     aligned on a 16 byte boundary."  */
  sp &= ~0xf;

  /* Write out the arguments to the stack.  */
  for (i = 0; i < num_stack_args; i++)
    {
      struct type *type = value_type (stack_args[i]);
      const gdb_byte *valbuf = value_contents (stack_args[i]);
      int len = TYPE_LENGTH (type);

      write_memory (sp + element * 8, valbuf, len);
      element += ((len + 7) / 8);
    }

  /* The psABI says that "For calls that may call functions that use
     varargs or stdargs (prototype-less calls or calls to functions
     containing ellipsis (...) in the declaration) %al is used as
     hidden argument to specify the number of SSE registers used.  */
  regcache_raw_write_unsigned (regcache, AMD64_RAX_REGNUM, sse_reg);
  return sp; 
}

static CORE_ADDR
amd64_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		       struct regcache *regcache, CORE_ADDR bp_addr,
		       int nargs, struct value **args,	CORE_ADDR sp,
		       int struct_return, CORE_ADDR struct_addr)
{
  gdb_byte buf[8];

  /* Pass arguments.  */
  sp = amd64_push_arguments (regcache, nargs, args, sp, struct_return);

  /* Pass "hidden" argument".  */
  if (struct_return)
    {
      store_unsigned_integer (buf, 8, struct_addr);
      regcache_cooked_write (regcache, AMD64_RDI_REGNUM, buf);
    }

  /* Store return address.  */
  sp -= 8;
  store_unsigned_integer (buf, 8, bp_addr);
  write_memory (sp, buf, 8);

  /* Finally, update the stack pointer...  */
  store_unsigned_integer (buf, 8, sp);
  regcache_cooked_write (regcache, AMD64_RSP_REGNUM, buf);

  /* ...and fake a frame pointer.  */
  regcache_cooked_write (regcache, AMD64_RBP_REGNUM, buf);

  return sp + 16;
}


/* The maximum number of saved registers.  This should include %rip.  */
#define AMD64_NUM_SAVED_REGS	AMD64_NUM_GREGS

/* Return PC of first real instruction.  */

static CORE_ADDR
amd64_skip_prologue (CORE_ADDR start_pc)
{
  struct x86_frame_cache cache;
  CORE_ADDR pc, endaddr;

  x86_initialize_frame_cache (&cache, 8);

  if (find_pc_partial_function_no_inlined (start_pc, NULL, NULL, &endaddr) == 0)
    endaddr = start_pc + 512;  /* 512 bytes is more than enough.  */
  endaddr = refine_prologue_limit (start_pc, endaddr, 3);
  pc = x86_analyze_prologue (start_pc, endaddr, &cache);

  return pc;
}

static const struct frame_unwind amd64_frame_unwind =
{
  NORMAL_FRAME,
  x86_frame_this_id,
  x86_frame_prev_register,
  NULL,   /* const struct frame_data *unwind_data */
  NULL,   /* frame_sniffer_ftype *sniffer */
  NULL    /* frame_prev_pc_ftype *prev_pc */
};

static const struct frame_unwind *
amd64_frame_sniffer (struct frame_info *next_frame)
{
  return &amd64_frame_unwind;
}


/* Signal trampolines.  */

/* FIXME: kettenis/20030419: Perhaps, we can unify the 32-bit and
   64-bit variants.  This would require using identical frame caches
   on both platforms.  */

static struct x86_frame_cache *
amd64_sigtramp_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct x86_frame_cache *cache;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  CORE_ADDR addr;
  gdb_byte buf[8];
  int i;

  if (*this_cache)
    return *this_cache;

  cache = x86_alloc_frame_cache (8);

  frame_unwind_register (next_frame, AMD64_RSP_REGNUM, buf);
  cache->frame_base = extract_unsigned_integer (buf, 8);

  addr = tdep->sigcontext_addr (next_frame);
  gdb_assert (tdep->sc_reg_offset);
  gdb_assert (tdep->sc_num_regs <= AMD64_NUM_SAVED_REGS);
  for (i = 0; i < tdep->sc_num_regs; i++)
    if (tdep->sc_reg_offset[i] != -1)
      cache->saved_regs[i] = addr + tdep->sc_reg_offset[i];
  cache->saved_regs_are_absolute = 1;

  cache->prologue_scan_status = full_scan_succeeded;
  cache->saved_regs_are_absolute = 1;

  *this_cache = cache;
  return cache;
}

static void
amd64_sigtramp_frame_this_id (struct frame_info *next_frame,
			      void **this_cache, struct frame_id *this_id)
{
  struct x86_frame_cache *cache =
    amd64_sigtramp_frame_cache (next_frame, this_cache);

  (*this_id) = frame_id_build (cache->frame_base + 8, frame_pc_unwind (next_frame));
}

static void
amd64_sigtramp_frame_prev_register (struct frame_info *next_frame,
				    void **this_cache,
				    /* APPLE LOCAL variable opt states.  */
				    int regnum, enum opt_state *optimizedp,
				    enum lval_type *lvalp, CORE_ADDR *addrp,
				    int *realnump, gdb_byte *valuep)
{
  /* Make sure we've initialized the cache.  */
  amd64_sigtramp_frame_cache (next_frame, this_cache);

  x86_frame_prev_register (next_frame, this_cache, regnum,
			   optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind amd64_sigtramp_frame_unwind =
{
  SIGTRAMP_FRAME,
  amd64_sigtramp_frame_this_id,
  amd64_sigtramp_frame_prev_register,
  NULL,   /* const struct frame_data *unwind_data */
  NULL,   /* frame_sniffer_ftype *sniffer */
  NULL    /* frame_prev_pc_ftype *prev_pc */
};

static const struct frame_unwind *
amd64_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (get_frame_arch (next_frame));

  /* We shouldn't even bother if we don't have a sigcontext_addr
     handler.  */
  if (tdep->sigcontext_addr == NULL)
    return NULL;

  if (tdep->sigtramp_p != NULL)
    {
      if (tdep->sigtramp_p (next_frame))
	return &amd64_sigtramp_frame_unwind;
    }

  if (tdep->sigtramp_start != 0)
    {
      CORE_ADDR pc = frame_pc_unwind (next_frame);

      gdb_assert (tdep->sigtramp_end != 0);
      if (pc >= tdep->sigtramp_start && pc < tdep->sigtramp_end)
	return &amd64_sigtramp_frame_unwind;
    }

  return NULL;
}


static CORE_ADDR
amd64_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct x86_frame_cache *cache = x86_frame_cache (next_frame, this_cache, 8);

  return cache->frame_base;
}

static const struct frame_base amd64_frame_base =
{
  &amd64_frame_unwind,
  amd64_frame_base_address,
  amd64_frame_base_address,
  amd64_frame_base_address
};

static struct frame_id
amd64_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  gdb_byte buf[8];
  CORE_ADDR fp;

  frame_unwind_register (next_frame, AMD64_RBP_REGNUM, buf);
  fp = extract_unsigned_integer (buf, 8);

  return frame_id_build (fp + 16, frame_pc_unwind (next_frame));
}

/* 16 byte align the SP per frame requirements.  */

static CORE_ADDR
amd64_frame_align (struct gdbarch *gdbarch, CORE_ADDR sp)
{
  return sp & -(CORE_ADDR)16;
}


/* Supply register REGNUM from the buffer specified by FPREGS and LEN
   in the floating-point register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
amd64_supply_fpregset (const struct regset *regset, struct regcache *regcache,
		       int regnum, const void *fpregs, size_t len)
{
  const struct gdbarch_tdep *tdep = gdbarch_tdep (regset->arch);

  gdb_assert (len == tdep->sizeof_fpregset);
  amd64_supply_fxsave (regcache, regnum, fpregs);
}

/* Collect register REGNUM from the register cache REGCACHE and store
   it in the buffer specified by FPREGS and LEN as described by the
   floating-point register set REGSET.  If REGNUM is -1, do this for
   all registers in REGSET.  */

static void
amd64_collect_fpregset (const struct regset *regset,
			const struct regcache *regcache,
			int regnum, void *fpregs, size_t len)
{
  const struct gdbarch_tdep *tdep = gdbarch_tdep (regset->arch);

  gdb_assert (len == tdep->sizeof_fpregset);
  amd64_collect_fxsave (regcache, regnum, fpregs);
}

static CORE_ADDR
amd64_fetch_pointer_argument (struct frame_info *frame, int argi,
                              struct type *type)
{
  static int integer_regnum[] =
  {
    AMD64_RDI_REGNUM,           /* %rdi */
    AMD64_RSI_REGNUM,           /* %rsi */
    AMD64_RDX_REGNUM,           /* %rdx */
    AMD64_RCX_REGNUM,           /* %rcx */
    8,                          /* %r8 */
    9                           /* %r9 */
  };

  if (argi < 6)
    return get_frame_register_unsigned (frame, integer_regnum[argi]);
  else
    return 0;
}

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

static const struct regset *
amd64_regset_from_core_section (struct gdbarch *gdbarch,
				const char *sect_name, size_t sect_size)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  if (strcmp (sect_name, ".reg2") == 0 && sect_size == tdep->sizeof_fpregset)
    {
      if (tdep->fpregset == NULL)
	tdep->fpregset = regset_alloc (gdbarch, amd64_supply_fpregset,
				       amd64_collect_fpregset);

      return tdep->fpregset;
    }

  return i386_regset_from_core_section (gdbarch, sect_name, sect_size);
}


void
amd64_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* AMD64 generally uses `fxsave' instead of `fsave' for saving its
     floating-point registers.  */
  tdep->sizeof_fpregset = I387_SIZEOF_FXSAVE;

  /* AMD64 has an FPU and 16 SSE registers.  */
  tdep->st0_regnum = AMD64_ST0_REGNUM;
  tdep->num_xmm_regs = 16;

  /* This is what all the fuss is about.  */
  set_gdbarch_long_bit (gdbarch, 64);
  set_gdbarch_long_long_bit (gdbarch, 64);
  set_gdbarch_ptr_bit (gdbarch, 64);

  /* In contrast to the i386, on AMD64 a `long double' actually takes
     up 128 bits, even though it's still based on the i387 extended
     floating-point format which has only 80 significant bits.  */
  set_gdbarch_long_double_bit (gdbarch, 128);

  set_gdbarch_num_regs (gdbarch, AMD64_NUM_REGS);
  set_gdbarch_register_name (gdbarch, amd64_register_name);
  set_gdbarch_register_type (gdbarch, amd64_register_type);

  /* Register numbers of various important registers.  */
  set_gdbarch_sp_regnum (gdbarch, AMD64_RSP_REGNUM); /* %rsp */
  set_gdbarch_pc_regnum (gdbarch, AMD64_RIP_REGNUM); /* %rip */
  set_gdbarch_ps_regnum (gdbarch, AMD64_EFLAGS_REGNUM); /* %eflags */
  set_gdbarch_fp0_regnum (gdbarch, AMD64_ST0_REGNUM); /* %st(0) */
  /* APPLE LOCAL: Add the frame pointer register so it can be modified
     in expressions.  */
  set_gdbarch_deprecated_fp_regnum (gdbarch, AMD64_RBP_REGNUM); /* %rbp */

  /* The "default" register numbering scheme for AMD64 is referred to
     as the "DWARF Register Number Mapping" in the System V psABI.
     The preferred debugging format for all known AMD64 targets is
     actually DWARF2, and GCC doesn't seem to support DWARF (that is
     DWARF-1), but we provide the same mapping just in case.  This
     mapping is also used for stabs, which GCC does support.  */
  set_gdbarch_stab_reg_to_regnum (gdbarch, amd64_dwarf_reg_to_regnum);
  set_gdbarch_dwarf_reg_to_regnum (gdbarch, amd64_dwarf_reg_to_regnum);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, amd64_dwarf_reg_to_regnum);

  /* We don't override SDB_REG_RO_REGNUM, since COFF doesn't seem to
     be in use on any of the supported AMD64 targets.  */

  /* Call dummy code.  */
  set_gdbarch_push_dummy_call (gdbarch, amd64_push_dummy_call);
  set_gdbarch_frame_align (gdbarch, amd64_frame_align);
  set_gdbarch_frame_red_zone_size (gdbarch, 128);

  set_gdbarch_convert_register_p (gdbarch, amd64_convert_register_p);
  set_gdbarch_register_to_value (gdbarch, i387_register_to_value);
  set_gdbarch_value_to_register (gdbarch, i387_value_to_register);

  set_gdbarch_return_value (gdbarch, amd64_return_value);

  set_gdbarch_skip_prologue (gdbarch, amd64_skip_prologue);

  /* Avoid wiring in the MMX registers for now.  */
  set_gdbarch_num_pseudo_regs (gdbarch, 0);
  tdep->mm0_regnum = -1;

  set_gdbarch_unwind_dummy_id (gdbarch, amd64_unwind_dummy_id);

  frame_unwind_append_sniffer (gdbarch, amd64_sigtramp_frame_sniffer);
  frame_unwind_append_sniffer (gdbarch, amd64_frame_sniffer);
  frame_base_set_default (gdbarch, &amd64_frame_base);

  /* If we have a register mapping, enable the generic core file support.  */
  if (tdep->gregset_reg_offset)
    set_gdbarch_regset_from_core_section (gdbarch,
					  amd64_regset_from_core_section);
  /* APPLE LOCAL: A handy little function.  */
  set_gdbarch_fetch_pointer_argument (gdbarch, amd64_fetch_pointer_argument);

}


#define I387_ST0_REGNUM AMD64_ST0_REGNUM

/* The 64-bit FXSAVE format differs from the 32-bit format in the
   sense that the instruction pointer and data pointer are simply
   64-bit offsets into the code segment and the data segment instead
   of a selector offset pair.  The functions below store the upper 32
   bits of these pointers (instead of just the 16-bits of the segment
   selector).  */

/* Fill register REGNUM in REGCACHE with the appropriate
   floating-point or SSE register value from *FXSAVE.  If REGNUM is
   -1, do this for all registers.  This function masks off any of the
   reserved bits in *FXSAVE.  */

void
amd64_supply_fxsave (struct regcache *regcache, int regnum,
		      const void *fxsave)
{
  i387_supply_fxsave (regcache, regnum, fxsave);

  if (fxsave && gdbarch_ptr_bit (get_regcache_arch (regcache)) == 64)
    {
      const gdb_byte *regs = fxsave;

      if (regnum == -1 || regnum == I387_FISEG_REGNUM)
	regcache_raw_supply (regcache, I387_FISEG_REGNUM, regs + 12);
      if (regnum == -1 || regnum == I387_FOSEG_REGNUM)
	regcache_raw_supply (regcache, I387_FOSEG_REGNUM, regs + 20);
    }
}

/* Fill register REGNUM (if it is a floating-point or SSE register) in
   *FXSAVE with the value from REGCACHE.  If REGNUM is -1, do this for
   all registers.  This function doesn't touch any of the reserved
   bits in *FXSAVE.  */

void
amd64_collect_fxsave (const struct regcache *regcache, int regnum,
		      void *fxsave)
{
  gdb_byte *regs = fxsave;

  i387_collect_fxsave (regcache, regnum, fxsave);

  if (gdbarch_ptr_bit (get_regcache_arch (regcache)) == 64)
    {
      if (regnum == -1 || regnum == I387_FISEG_REGNUM)
	regcache_raw_collect (regcache, I387_FISEG_REGNUM, regs + 12);
      if (regnum == -1 || regnum == I387_FOSEG_REGNUM)
	regcache_raw_collect (regcache, I387_FOSEG_REGNUM, regs + 20);
    }
}
