/* Target-dependent code for PowerPC systems using the SVR4 ABI
   for GDB, the GNU debugger.

   Copyright 2000, 2001, 2002, 2003, 2005 Free Software Foundation,
   Inc.

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
#include "gdbcore.h"
#include "inferior.h"
#include "regcache.h"
#include "value.h"
#include "gdb_string.h"
#include "gdb_assert.h"
#include "ppc-tdep.h"
#include "target.h"
#include "objfiles.h"
#include "infcall.h"

/* APPLE LOCAL begin ppc */
/* Round x up to the nearest multiple of s, assuming that s is a
   power of 2. */

#define ROUND_UP(x,s) ((((long) (x) - 1) & ~(long) ((s) - 1)) + (s))

/* Specifies ABI rules for pushing parameters into registers and onto
   the stack, for function calls.  */

struct ppc_stack_abi
{
  /* First available general-purpose register.  */
  int first_greg;
  /* Last available general-purpose register.  */
  int last_greg;

  /* First available floating-point register.  */
  int first_freg;
  /* Last available floating-point register.  */
  int last_freg;

  /* First available vector register.  */
  int first_vreg;
  /* Last available vector register.  */
  int last_vreg;

  /* Set if using a floating-point register causes the corresponding
     general-purpose register to be skipped for argument passing.  */
  int fregs_shadow_gregs;

  /* Set if register data is stored on the stack as well as in
     register.  */
  int regs_shadow_stack;

  /* Set if structure data is passed on the stack in-line, as opposed
     to via pointers to data passed after the argument data.  */
  int structs_with_args;

  /* Minimum number of bytes to remove for argument data.  */
  int min_argsize;

  /* Number of bytes to reserve for backchain information.  */
  int backchain_size;
};

struct ppc_stack_context
{
  /* Next available general-purpose register.  */
  int greg;
  /* Next available floating-point register.  */
  int freg;
  /* Next available vector register.  */
  int vreg;

  /* Current offset into the argument area.  */
  int argoffset;

  /* Current offset into the structure-passing area.  */
  int structoffset;

  /* Stack pointer, used as the base address for
     argoffset/structoffset for writing onto the argument and struct
     data areas.  */
  CORE_ADDR sp;
};


/* This function hides a bit of complexity with 64 bit
   PPC Darwin.  32 bit apps on PPC 64 systems can access all 64 bits
   of the registers, but only store 32 bits on the stack.  So wordsize
   is 4, but the register size is 8, and we have to make sure we get
   the data in the right part of the register.  FIXME: I didn't do
   this for LE PPC...  */

static void
ppc_copy_into_greg (struct regcache *regcache, int regno, int wordsize, 
		    int len, const gdb_byte *contents)
{
  int reg_size = register_size (current_gdbarch, regno);

  if (reg_size == wordsize)
    {
      regcache_raw_write (current_regcache, regno, contents);
    }
  else
    {
      int upper_half_size = reg_size - wordsize;
      int num_regs = (len/wordsize);
      int i;

      if (num_regs == 0)
	num_regs = 1;

      
      for (i = 0; i < num_regs; i++)
	{
	  char buf[8];
	  memset (buf, 0, 8);
	  memcpy (buf + upper_half_size, contents, wordsize);
	  regcache_cooked_write (regcache, regno + i,
				 (const bfd_byte *) buf);

	  contents += wordsize;
	}
    }
}

static void
ppc_copy_from_greg (struct regcache *regcache, int regno, int wordsize, 
		    int len, bfd_byte *buf)
{
  /* Write just the bottom part of the register. */
  int reg_size = register_size (current_gdbarch, regno);
  int num_regs = len / wordsize;

  if (reg_size == wordsize)
    {
      int i;
      for (i = 0; i < num_regs; i++)
	regcache_cooked_read (regcache, regno + i, buf + i * wordsize);
    }
  else
    {
      gdb_byte tmpbuf[8];
      int i;

      for (i = 0; i < num_regs; i++)
	{
	  regcache_cooked_read (regcache, regno + i, tmpbuf);
	  memcpy (buf + i * wordsize, tmpbuf + (reg_size - wordsize), wordsize);
	}
    }
}

/* Push the single argument ARG onto C, using the rules in ABI.  The
   parameter ARGNO is used for printing error and diagnostic messages.
   If DO_COPY is set, actually copy the data onto the stack/registers;
   otherwise just pre-flight the pushing to learn the appropriate
   offsets.  If FLOATONLY is set, we are scanning recursively through
   a structure to push floating-point arguments into floating-point
   registers. */

static void
ppc_push_argument (struct ppc_stack_abi *abi, 
		   struct ppc_stack_context *c, struct value *arg,
		   int argno, int do_copy, int floatonly)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  struct type *type = check_typedef (value_type (arg));
  int len = TYPE_LENGTH (type);

  gdb_byte buf[16];

  c->argoffset = ROUND_UP (c->argoffset, 4);

  switch (TYPE_CODE (type)) {
      
  case TYPE_CODE_FLT:
    {
      if (c->freg <= abi->last_freg)
	{
	  struct value *rval;
	  struct type *rtype;
	  int rlen;
	  /* APPLE LOCAL: If the thing is already a long double type,
	     don't cast it to a builtin type double, since there are two
	     long double types, and we will pass an 16 byte long double
	     wrong if we assume it is an 8 byte double.  */
	  if (strcmp (TYPE_NAME (type), "long double") != 0)
	    {
	      rval  = value_cast (builtin_type_double, arg);
	      rtype = check_typedef (value_type (rval));
	      rlen = TYPE_LENGTH (rtype);
	    }
	  else
	    {
	      rval = arg;
	      rtype = type;
	      rlen = len;
	    }

	  /* APPLE LOCAL: GCC 4.0 has 16 byte long doubles */
	  if ((len != 4) && (len != 8) && (len != 16))
	    error ("floating point parameter had unexpected size");

	  if (rlen != 8 && rlen != 16)
	    error ("floating point parameter had unexpected size");

	  if (do_copy)
	    regcache_raw_write (current_regcache, FP0_REGNUM + c->freg, 
				value_contents (rval));
	  if (do_copy && ! floatonly && abi->fregs_shadow_gregs)
	    ppc_copy_into_greg (current_regcache, c->greg, tdep->wordsize, len, value_contents (arg));
	  if (do_copy && ! floatonly && abi->regs_shadow_stack)
	    write_memory (c->sp + c->argoffset, value_contents (arg), len);

	  c->freg++;
	  /* APPLE LOCAL: We took up two registers...  */
	  if (rlen == 16)
	    c->freg++;

	  if (! floatonly && (abi->fregs_shadow_gregs) && (c->greg <= abi->last_greg))
	    c->greg += len / 4;
	  if (! floatonly && abi->regs_shadow_stack)
	    c->argoffset += len;
	}
      else if (! floatonly)
	{
	  if ((len != 4) && (len != 8) && (len != 16))
	    error ("floating point parameter had unexpected size");

	  c->argoffset = ROUND_UP (c->argoffset, len);
	  if (do_copy)
	    write_memory (c->sp + c->argoffset, value_contents (arg), len);
	  c->argoffset += len;
	}
      break;
    }

  case TYPE_CODE_INT:
  case TYPE_CODE_ENUM:
  case TYPE_CODE_PTR:
  case TYPE_CODE_REF:
    {
      int nregs;
      gdb_byte *val_contents;

      if (floatonly)
	break;

      /* APPLE LOCAL: Huge Hack...  The problem is that if we are a 32
	 bit app on Mac OS X, the registers are really 64 bits, but we
	 don't want to pass all 64 bits.  So if we get passed a value
	 that came from a register, and it's length is > the wordsize,
	 cast it to the wordsize first before passing it in.  */
      if (VALUE_REGNUM (arg) != -1 && len == 8 && tdep->wordsize == 4)
	{
	  len = 4;
	  val_contents = value_contents (arg) + 4;
	}
      else
	val_contents = value_contents (arg);
      /* END APPLE LOCAL  */

      nregs = (len <= 4) ? 1 : 2;
      if ((len != 1) && (len != 2) && (len != 4) && (len != 8))
	error ("integer parameter had unexpected size");

      if (c->greg <= abi->last_greg)
	{
	  /* If the parameter fits in the remaining argument registers, write it to
	     the registers, and to the stack if the abi requires it. */

	  if (do_copy)
	    {
	      /* Split the argument between registers & the stack if it
		 doesn't fit in the remaining registers.  */
	      int regs_avaliable = abi->last_greg - c->greg + 1;
	      if (regs_avaliable >= nregs)
		regs_avaliable = nregs;

	      ppc_copy_into_greg (current_regcache, c->greg, 
				  tdep->wordsize, regs_avaliable * 4, val_contents);
	    }
	  if (do_copy && abi->regs_shadow_stack)
	    write_memory (c->sp + c->argoffset, val_contents, len);

	  c->greg += nregs;
	  if (abi->regs_shadow_stack)
	    c->argoffset += (nregs * 4);
	}
      else 
	{
	  /* If we've filled up the registers, then just write it on the stack. */

	  if (do_copy)
	    write_memory (c->sp + c->argoffset, val_contents, len);
	  c->argoffset += (nregs * 4);
	}
      break;
    }

  case TYPE_CODE_STRUCT:
  case TYPE_CODE_UNION:
    {
      if (! abi->structs_with_args)
	{
	  if (floatonly)
	    break;

	  if (len > 4)
	    {
	      /* Rounding to the nearest multiple of 8 may not be necessary,
		 but it is safe.  Particularly since we don't know the
		 field types of the structure */
	      c->structoffset = ROUND_UP (c->structoffset, 8);
	      if (do_copy)
		{
		  write_memory (c->sp + c->structoffset, value_contents (arg), len);
		  store_unsigned_integer (buf, 4, c->sp + c->structoffset);
		}
	      c->structoffset += ROUND_UP (len, 8);
	    }
	  else
	    if (do_copy)
	      {
		memset (buf, 0, 4);
		memcpy (buf, value_contents (arg), len);
	      }

	  if (c->greg <= abi->last_greg)
	    {
	      if (do_copy)
		ppc_copy_into_greg (current_regcache, c->greg, tdep->wordsize, 4, buf);
	      c->greg++;
	    }
	  else
	    {
	      if (do_copy)
		write_memory (c->sp + c->argoffset, buf, 4);
	      c->argoffset += 4;
	    }
	  break;
	}
      else
	{
	  int i;
	  int regspace = (abi->last_greg - c->greg + 1) * 4;
	  int stackspace = (len <= regspace) ? 0 : (len - regspace);
	  int writereg = (regspace > len) ? len : regspace;
	  int writestack = abi->regs_shadow_stack ? len : stackspace;

	  for (i = 0; i < TYPE_NFIELDS (type); i++)
	    {
	      struct value *subarg = value_field (arg, i);

	      ppc_push_argument (abi, c, subarg, argno, do_copy, 1);
	    }

	  if (floatonly)
	    break;

	  if (do_copy)
	    {
	      gdb_byte *ptr = value_contents (arg);
	      if (len < 4)
		{
		  memset (buf, 0, 4);
		  if ((len == 1) || (len == 2))
		    memcpy (buf + 4 - len, ptr, len);
		  else
		    memcpy (buf, ptr, len);
		  ptr = buf;
		}
	      ppc_copy_into_greg (current_regcache, c->greg, 
				  tdep->wordsize, (writereg < 4) ? 4 : writereg, ptr);
	      write_memory (c->sp + c->argoffset, ptr,
			    (writestack < 4) ? 4 : writestack);
	    }

	  c->greg += ROUND_UP (writereg, 4) / 4;
	  c->argoffset += writestack;
	}
      break;
    }

  case TYPE_CODE_ARRAY:
    {
      if (floatonly)
	break;

      if (! TYPE_VECTOR (type))
	error ("non-vector array type");
      if (len != 16)
	error ("unexpected vector length");

      if (c->vreg <= abi->last_vreg)
	{
	  if (do_copy)
	    regcache_raw_write (current_regcache, tdep->ppc_vr0_regnum + c->vreg,
				value_contents (arg));
	  c->vreg++;
	}
      else
	{
	  /* Vector arguments must be aligned to 16 bytes on
	     the stack. */
	  c->argoffset = ROUND_UP (c->argoffset, 16);
	  if (do_copy)
	    write_memory (c->sp + c->argoffset, value_contents (arg), len);
	  c->argoffset += len;
	}
      break;
    }

  default:
    error ("argument %d has unknown type code 0x%x (%s)",
	   argno, TYPE_CODE (type),
	   type_code_name (TYPE_CODE (type)));
  }

  return;
}

/* Push the NARGS arguments in ARGS onto the stack, using the rules in
   ABI.  The parameter SP shouuld be the address of the stack before
   the arguments are pushed.  If STRUCT_RETURN is set, push the
   address in STRUCT_ADDR for use in returning structure data. */

static CORE_ADDR
ppc_push_arguments (struct ppc_stack_abi *abi, int nargs, struct value **args,
		    CORE_ADDR sp, int struct_return, CORE_ADDR struct_addr)
{
  int argno;
  gdb_byte buf[4];
  CORE_ADDR saved_sp;

  struct ppc_stack_context preview;
  struct ppc_stack_context write;

  preview.greg = abi->first_greg;
  preview.freg = abi->first_freg;
  preview.vreg = abi->first_vreg;

  preview.argoffset = 0;
  preview.structoffset = 0;
  preview.sp = 0;

  if (struct_return)
    {
      preview.greg++;
      if (abi->regs_shadow_stack)
	preview.argoffset += 4;
    }

  /* Figure out how much new stack space is required for arguments
     which don't fit in registers.  Unlike the PowerOpen ABI, the
     SysV ABI doesn't reserve any extra space for parameters which
     are put in registers. */

  for (argno = 0; argno < nargs; argno++)
    {
      ppc_push_argument (abi, &preview, args[argno], argno, 0, 0);
    }

  /* Round the argument area up to the minimum argument area size, if
     one is provided.  */

  if (preview.argoffset < abi->min_argsize)
    preview.argoffset = abi->min_argsize;

  /* Get current SP location */
  saved_sp = read_sp ();

  sp -= preview.argoffset + preview.structoffset;

  /* Allocate space for backchain and callee's saved lr */
  sp -= abi->backchain_size;

  /* A Hack to make AltiVec happy by keeping pushed parameters from
     stepping on the next frame up.  Not clear what part of the
     calculation is messed up, but it doesn't actually hurt to make a
     dummy frame lower down than other frames.  Empirically, an
     adjustment of 32 or more fixes, 16 is not enough.  */
  sp -= 32;

  /* Make sure that we maintain 16 byte alignment */
  sp &= ~0x0f;

  /* Update %sp before proceeding any further */
  write_register (SP_REGNUM, sp);

  /* write the backchain */
  store_unsigned_integer (buf, 4, saved_sp);
  write_memory (sp, buf, 4);

  write.greg = abi->first_greg;
  write.freg = abi->first_freg;
  write.vreg = abi->first_vreg;

  write.argoffset = abi->backchain_size;
  write.structoffset = write.argoffset + preview.argoffset;
  write.sp = sp;

  /* Fill in r3 with the return structure, if any.  */
  if (struct_return)
    {
      store_unsigned_integer (buf, 4, struct_addr);
      ppc_copy_into_greg (current_regcache, write.greg, 
			  gdbarch_tdep (current_gdbarch)->wordsize, 4, buf);
      write.greg++;
      if (abi->regs_shadow_stack)
	write.argoffset += 4;
    }

  /* Now fill in the registers and stack.  */
  for (argno = 0; argno < nargs; argno++)
    {
      ppc_push_argument (abi, &write, args[argno], argno, 1, 0);
    }

  return sp;
}

/* Pass the arguments in either registers, or in the stack. Using the
   ppc sysv ABI, the first eight words of the argument list (that might
   be less than eight parameters if some parameters occupy more than one
   word) are passed in r3..r10 registers.  float and double parameters are
   passed in fpr's, in addition to that. Rest of the parameters if any
   are passed in user stack. 

   If the function is returning a structure, then the return address is passed
   in r3, then the first 7 words of the parametes can be passed in registers,
   starting from r4. */

CORE_ADDR
ppc_sysv_abi_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
			      struct regcache *regcache, CORE_ADDR bp_addr,
			      int nargs, struct value **args, CORE_ADDR sp,
			      int struct_return, CORE_ADDR struct_addr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  struct ppc_stack_abi abi;
  CORE_ADDR ret;

  abi.first_greg = 3;
  abi.last_greg = 10;
  abi.first_freg = 1;
  abi.last_freg = 8;
  abi.first_vreg = 2;
  abi.last_vreg = 13;
  abi.fregs_shadow_gregs = 0;
  abi.regs_shadow_stack = 0;
  abi.structs_with_args = 0;
  abi.min_argsize = 0;
  abi.backchain_size = 8;

  ret = ppc_push_arguments (&abi, nargs, args, sp, struct_return, struct_addr);

  /* Point the inferior function call's return address at the dummy's
     breakpoint.  */
  regcache_cooked_write_signed (regcache, tdep->ppc_lr_regnum, bp_addr);

  target_store_registers (-1);
  return ret;
}

/* Pass the arguments in either registers, or in the stack.  On Darwin,
   the first eight words of the argument list (that might be less than
   eight parameters if some parameters occupy more than one word) are
   passed in r3..r13 registers.  float and double parameters are
   passed in fpr's, in addition to that.  Rest of the parameters if any
   are passed in user stack.  There might be cases in which half of the
   parameter is copied into registers, the other half is pushed into
   stack.

   Stack must be aligned on 64-bit boundaries when synthesizing
   function calls.

   If the function is returning a structure, then the return address is passed
   in r3, then the first 7 words of the parameters can be passed in registers,
   starting from r4.  */

CORE_ADDR
ppc_darwin_abi_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
				struct regcache *regcache, CORE_ADDR bp_addr,
				int nargs, struct value **args, CORE_ADDR sp,
				int struct_return, CORE_ADDR struct_addr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  struct ppc_stack_abi abi;
  CORE_ADDR ret;

  abi.first_greg = 3;
  abi.last_greg = 10;
  abi.first_freg = 1;
  abi.last_freg = 13;
  abi.first_vreg = 2;
  abi.last_vreg = 13;
  abi.fregs_shadow_gregs = 1;
  abi.regs_shadow_stack = 1;
  abi.structs_with_args = 1;
  abi.min_argsize = 32;
  abi.backchain_size = 24;

  ret = ppc_push_arguments (&abi, nargs, args, sp, struct_return, struct_addr);

  /* Point the inferior function call's return address at the dummy's
     breakpoint.  */
  regcache_cooked_write_signed (regcache, tdep->ppc_lr_regnum, bp_addr);

  target_store_registers (-1);
  return ret;
}
/* APPLE LOCAL end ppc */

/* Handle the return-value conventions specified by the SysV 32-bit
   PowerPC ABI (including all the supplements):

   no floating-point: floating-point values returned using 32-bit
   general-purpose registers.

   Altivec: 128-bit vectors returned using vector registers.

   e500: 64-bit vectors returned using the full full 64 bit EV
   register, floating-point values returned using 32-bit
   general-purpose registers.

   GCC (broken): Small struct values right (instead of left) aligned
   when returned in general-purpose registers.  */

static enum return_value_convention
do_ppc_sysv_return_value (struct gdbarch *gdbarch, struct type *type,
			  /* APPLE LOCAL gdb_byte */
			  struct regcache *regcache, gdb_byte *readbuf,
			  /* APPLE LOCAL gdb_byte */
			  const gdb_byte *writebuf, int broken_gcc)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  gdb_assert (tdep->wordsize == 4);
  if (TYPE_CODE (type) == TYPE_CODE_FLT
      && TYPE_LENGTH (type) <= 8
      && ppc_floating_point_unit_p (gdbarch))
    {
      if (readbuf)
	{
	  /* Floats and doubles stored in "f1".  Convert the value to
	     the required type.  */
	  gdb_byte regval[MAX_REGISTER_SIZE];
	  struct type *regtype = register_type (gdbarch,
                                                tdep->ppc_fp0_regnum + 1);
	  regcache_cooked_read (regcache, tdep->ppc_fp0_regnum + 1, regval);
	  convert_typed_floating (regval, regtype, readbuf, type);
	}
      if (writebuf)
	{
	  /* Floats and doubles stored in "f1".  Convert the value to
	     the register's "double" type.  */
	  gdb_byte regval[MAX_REGISTER_SIZE];
	  struct type *regtype = register_type (gdbarch, tdep->ppc_fp0_regnum);
	  convert_typed_floating (writebuf, type, regval, regtype);
	  regcache_cooked_write (regcache, tdep->ppc_fp0_regnum + 1, regval);
	}
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  /* APPLE LOCAL: gcc 3.3 had 8 byte long doubles, but gcc 4.0 uses 16 byte
     long doubles even for 32 bit ppc.  They are stored across f1 & f2. */
  /* Big floating point values get stored in adjacent floating
     point registers.  */
  if (TYPE_CODE (type) == TYPE_CODE_FLT
      && (TYPE_LENGTH (type) == 16 || TYPE_LENGTH (type) == 32))
    {
      if (writebuf || readbuf != NULL)
	{
	  int i;
	  for (i = 0; i < TYPE_LENGTH (type) / 8; i++)
	    {
	      if (writebuf != NULL)
		regcache_cooked_write (regcache, FP0_REGNUM + 1 + i,
				       (const bfd_byte *) writebuf + i * 8);
	      if (readbuf != NULL)
		regcache_cooked_read (regcache, FP0_REGNUM + 1 + i,
				      (bfd_byte *) readbuf + i * 8);
	    }
	}
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  /* END APPLE LOCAL */
  if ((TYPE_CODE (type) == TYPE_CODE_INT && TYPE_LENGTH (type) == 8)
      || (TYPE_CODE (type) == TYPE_CODE_FLT && TYPE_LENGTH (type) == 8))
    {
      if (readbuf)
	{
	  /* A long long, or a double stored in the 32 bit r3/r4.  */
	  ppc_copy_from_greg (regcache, tdep->ppc_gp0_regnum + 3, 
			      tdep->wordsize, 8, (bfd_byte *) readbuf);
	}
      if (writebuf)
	{
	  /* A long long, or a double stored in the 32 bit r3/r4.  */
	  ppc_copy_into_greg (regcache, tdep->ppc_gp0_regnum + 3, 
			      tdep->wordsize, 8, writebuf);
	}
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  if (TYPE_CODE (type) == TYPE_CODE_INT
      && TYPE_LENGTH (type) <= tdep->wordsize)
    {
      if (readbuf)
	{
	  /* Some sort of integer stored in r3.  Since TYPE isn't
	     bigger than the register, sign extension isn't a problem
	     - just do everything unsigned.  */
	  ULONGEST regval;
	  regcache_cooked_read_unsigned (regcache, tdep->ppc_gp0_regnum + 3,
					 &regval);
	  store_unsigned_integer (readbuf, TYPE_LENGTH (type), regval);
	}
      if (writebuf)
	{
	  /* Some sort of integer stored in r3.  Use unpack_long since
	     that should handle any required sign extension.  */
	  regcache_cooked_write_unsigned (regcache, tdep->ppc_gp0_regnum + 3,
					  unpack_long (type, writebuf));
	}
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  if (TYPE_LENGTH (type) == 16
      && TYPE_CODE (type) == TYPE_CODE_ARRAY
      && TYPE_VECTOR (type) && tdep->ppc_vr0_regnum >= 0)
    {
      if (readbuf)
	{
	  /* Altivec places the return value in "v2".  */
	  regcache_cooked_read (regcache, tdep->ppc_vr0_regnum + 2, readbuf);
	}
      if (writebuf)
	{
	  /* Altivec places the return value in "v2".  */
	  regcache_cooked_write (regcache, tdep->ppc_vr0_regnum + 2, writebuf);
	}
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  if (TYPE_LENGTH (type) == 8
      && TYPE_CODE (type) == TYPE_CODE_ARRAY
      && TYPE_VECTOR (type) && tdep->ppc_ev0_regnum >= 0)
    {
      /* The e500 ABI places return values for the 64-bit DSP types
	 (__ev64_opaque__) in r3.  However, in GDB-speak, ev3
	 corresponds to the entire r3 value for e500, whereas GDB's r3
	 only corresponds to the least significant 32-bits.  So place
	 the 64-bit DSP type's value in ev3.  */
      if (readbuf)
	regcache_cooked_read (regcache, tdep->ppc_ev0_regnum + 3, readbuf);
      if (writebuf)
	regcache_cooked_write (regcache, tdep->ppc_ev0_regnum + 3, writebuf);
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  if (broken_gcc && TYPE_LENGTH (type) <= 8)
    {
      if (readbuf)
	{
	  /* GCC screwed up.  The last register isn't "left" aligned.
	     Need to extract the least significant part of each
	     register and then store that.  */
	  /* Transfer any full words.  */
	  int word = 0;
	  while (1)
	    {
	      ULONGEST reg;
	      int len = TYPE_LENGTH (type) - word * tdep->wordsize;
	      if (len <= 0)
		break;
	      if (len > tdep->wordsize)
		len = tdep->wordsize;
	      regcache_cooked_read_unsigned (regcache,
					     tdep->ppc_gp0_regnum + 3 + word,
					     &reg);
	      store_unsigned_integer (((bfd_byte *) readbuf
				       + word * tdep->wordsize), len, reg);
	      word++;
	    }
	}
      if (writebuf)
	{
	  /* GCC screwed up.  The last register isn't "left" aligned.
	     Need to extract the least significant part of each
	     register and then store that.  */
	  /* Transfer any full words.  */
	  int word = 0;
	  while (1)
	    {
	      ULONGEST reg;
	      int len = TYPE_LENGTH (type) - word * tdep->wordsize;
	      if (len <= 0)
		break;
	      if (len > tdep->wordsize)
		len = tdep->wordsize;
	      reg = extract_unsigned_integer (((const bfd_byte *) writebuf
					       + word * tdep->wordsize), len);
	      regcache_cooked_write_unsigned (regcache,
					      tdep->ppc_gp0_regnum + 3 + word,
					      reg);
	      word++;
	    }
	}
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  if (TYPE_LENGTH (type) <= 8)
    {
      if (readbuf)
	{
	  /* This matches SVr4 PPC, it does not match GCC.  */
	  /* The value is right-padded to 8 bytes and then loaded, as
	     two "words", into r3/r4.  */
	  ppc_copy_from_greg (regcache, tdep->ppc_gp0_regnum + 3,
                              tdep->wordsize, TYPE_LENGTH (type), readbuf);
	}
      if (writebuf)
	{
	  /* This matches SVr4 PPC, it does not match GCC.  */
	  /* The value is padded out to 8 bytes and then loaded, as
	     two "words" into r3/r4.  */
	  gdb_byte regvals[MAX_REGISTER_SIZE * 2];
	  memset (regvals, 0, sizeof regvals);
	  memcpy (regvals, writebuf, TYPE_LENGTH (type));
	  regcache_cooked_write (regcache, tdep->ppc_gp0_regnum + 3,
				 regvals + 0 * tdep->wordsize);
	  if (TYPE_LENGTH (type) > tdep->wordsize)
	    regcache_cooked_write (regcache, tdep->ppc_gp0_regnum + 4,
				   regvals + 1 * tdep->wordsize);
	}
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  return RETURN_VALUE_STRUCT_CONVENTION;
}

/* APPLE LOCAL begin Darwin */
enum return_value_convention
ppc_darwin_abi_return_value (struct gdbarch *gdbarch, struct type *valtype,
			     struct regcache *regcache, gdb_byte *readbuf,
			     const gdb_byte *writebuf)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  if (TYPE_CODE (valtype) == TYPE_CODE_STRUCT
      || TYPE_CODE (valtype) == TYPE_CODE_UNION)
    {
      if (readbuf != NULL)
	{
	  /* CORE_ADDR is more logical, but ULONGEST finesses the situation
	     of G5 registers in 32-bit mode.  */
	  ULONGEST addr;
	  
	  regcache_cooked_read (regcache, tdep->ppc_gp0_regnum + 3, (gdb_byte *) &addr);
	  read_memory (addr, readbuf, TYPE_LENGTH (valtype));
	  
	}
      return RETURN_VALUE_ABI_RETURNS_ADDRESS;
    }
  else
    return do_ppc_sysv_return_value (gdbarch, valtype, regcache, readbuf,
				     writebuf, 0);
}
/* APPLE LOCAL end Darwin */

enum return_value_convention
ppc_sysv_abi_return_value (struct gdbarch *gdbarch, struct type *valtype,
			   struct regcache *regcache, gdb_byte *readbuf,
			   const gdb_byte *writebuf)
{
  return do_ppc_sysv_return_value (gdbarch, valtype, regcache, readbuf,
				   writebuf, 0);
}

enum return_value_convention
ppc_sysv_abi_broken_return_value (struct gdbarch *gdbarch,
				  struct type *valtype,
				  struct regcache *regcache,
				  gdb_byte *readbuf, const gdb_byte *writebuf)
{
  return do_ppc_sysv_return_value (gdbarch, valtype, regcache, readbuf,
				   writebuf, 1);
}

/* APPLE LOCAL begin Darwin */
CORE_ADDR
ppc64_darwin_abi_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
				struct regcache *regcache, CORE_ADDR bp_addr,
				int nargs, struct value **args, CORE_ADDR sp,
				int struct_return, CORE_ADDR struct_addr)
{
  return ppc64_sysv_abi_push_dummy_call
    (gdbarch, function, regcache, bp_addr, nargs, args, sp, struct_return, struct_addr);
}

enum return_value_convention
ppc64_darwin_abi_return_value (struct gdbarch *gdbarch, struct type *valtype,
			       struct regcache *regcache, gdb_byte *readbuf,
			       const gdb_byte *writebuf)
{
  return ppc64_sysv_abi_return_value (gdbarch, valtype, regcache, readbuf, writebuf);
}
/* APPLE LOCAL end Darwin */

/* The helper function for 64-bit SYSV push_dummy_call.  Converts the
   function's code address back into the function's descriptor
   address.

   Find a value for the TOC register.  Every symbol should have both
   ".FN" and "FN" in the minimal symbol table.  "FN" points at the
   FN's descriptor, while ".FN" points at the entry point (which
   matches FUNC_ADDR).  Need to reverse from FUNC_ADDR back to the
   FN's descriptor address (while at the same time being careful to
   find "FN" in the same object file as ".FN").  */

static int
convert_code_addr_to_desc_addr (CORE_ADDR code_addr, CORE_ADDR *desc_addr)
{
  struct obj_section *dot_fn_section;
  struct minimal_symbol *dot_fn;
  struct minimal_symbol *fn;
  CORE_ADDR toc;
  /* Find the minimal symbol that corresponds to CODE_ADDR (should
     have a name of the form ".FN").  */
  dot_fn = lookup_minimal_symbol_by_pc (code_addr);
  if (dot_fn == NULL || SYMBOL_LINKAGE_NAME (dot_fn)[0] != '.')
    return 0;
  /* Get the section that contains CODE_ADDR.  Need this for the
     "objfile" that it contains.  */
  dot_fn_section = find_pc_section (code_addr);
  if (dot_fn_section == NULL || dot_fn_section->objfile == NULL)
    return 0;
  /* Now find the corresponding "FN" (dropping ".") minimal symbol's
     address.  Only look for the minimal symbol in ".FN"'s object file
     - avoids problems when two object files (i.e., shared libraries)
     contain a minimal symbol with the same name.  */
  fn = lookup_minimal_symbol (SYMBOL_LINKAGE_NAME (dot_fn) + 1, NULL,
			      dot_fn_section->objfile);
  if (fn == NULL)
    return 0;
  /* Found a descriptor.  */
  (*desc_addr) = SYMBOL_VALUE_ADDRESS (fn);
  return 1;
}

/* Pass the arguments in either registers, or in the stack. Using the
   ppc 64 bit SysV ABI.

   This implements a dumbed down version of the ABI.  It always writes
   values to memory, GPR and FPR, even when not necessary.  Doing this
   greatly simplifies the logic. */

CORE_ADDR
ppc64_sysv_abi_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
				struct regcache *regcache, CORE_ADDR bp_addr,
				int nargs, struct value **args, CORE_ADDR sp,
				int struct_return, CORE_ADDR struct_addr)
{
  CORE_ADDR func_addr = find_function_addr (function, NULL);
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  /* By this stage in the proceedings, SP has been decremented by "red
     zone size" + "struct return size".  Fetch the stack-pointer from
     before this and use that as the BACK_CHAIN.  */
  const CORE_ADDR back_chain = read_sp ();
  /* See for-loop comment below.  */
  int write_pass;
  /* Size of the Altivec's vector parameter region, the final value is
     computed in the for-loop below.  */
  LONGEST vparam_size = 0;
  /* Size of the general parameter region, the final value is computed
     in the for-loop below.  */
  LONGEST gparam_size = 0;
  /* Kevin writes ... I don't mind seeing tdep->wordsize used in the
     calls to align_up(), align_down(), etc.  because this makes it
     easier to reuse this code (in a copy/paste sense) in the future,
     but it is a 64-bit ABI and asserting that the wordsize is 8 bytes
     at some point makes it easier to verify that this function is
     correct without having to do a non-local analysis to figure out
     the possible values of tdep->wordsize.  */
  gdb_assert (tdep->wordsize == 8);

  /* Go through the argument list twice.

     Pass 1: Compute the function call's stack space and register
     requirements.

     Pass 2: Replay the same computation but this time also write the
     values out to the target.  */

  for (write_pass = 0; write_pass < 2; write_pass++)
    {
      int argno;
      /* Next available floating point register for float and double
         arguments.  */
      int freg = 1;
      /* Next available general register for non-vector (but possibly
         float) arguments.  */
      int greg = 3;
      /* Next available vector register for vector arguments.  */
      int vreg = 2;
      /* The address, at which the next general purpose parameter
         (integer, struct, float, ...) should be saved.  */
      CORE_ADDR gparam;
      /* Address, at which the next Altivec vector parameter should be
         saved.  */
      CORE_ADDR vparam;

      if (!write_pass)
	{
	  /* During the first pass, GPARAM and VPARAM are more like
	     offsets (start address zero) than addresses.  That way
	     the accumulate the total stack space each region
	     requires.  */
	  gparam = 0;
	  vparam = 0;
	}
      else
	{
	  /* Decrement the stack pointer making space for the Altivec
	     and general on-stack parameters.  Set vparam and gparam
	     to their corresponding regions.  */
	  vparam = align_down (sp - vparam_size, 16);
	  gparam = align_down (vparam - gparam_size, 16);
	  /* Add in space for the TOC, link editor double word,
	     compiler double word, LR save area, CR save area.  */
	  sp = align_down (gparam - 48, 16);
	}

      /* If the function is returning a `struct', then there is an
         extra hidden parameter (which will be passed in r3)
         containing the address of that struct..  In that case we
         should advance one word and start from r4 register to copy
         parameters.  This also consumes one on-stack parameter slot.  */
      if (struct_return)
	{
	  if (write_pass)
	    regcache_cooked_write_signed (regcache,
					  tdep->ppc_gp0_regnum + greg,
					  struct_addr);
	  greg++;
	  gparam = align_up (gparam + tdep->wordsize, tdep->wordsize);
	}

      for (argno = 0; argno < nargs; argno++)
	{
	  struct value *arg = args[argno];
	  struct type *type = check_typedef (value_type (arg));
	  const bfd_byte *val = value_contents (arg);
	  if (TYPE_CODE (type) == TYPE_CODE_FLT && TYPE_LENGTH (type) <= 8)
	    {
	      /* Floats and Doubles go in f1 .. f13.  They also
	         consume a left aligned GREG,, and can end up in
	         memory.  */
	      if (write_pass)
		{
		  if (ppc_floating_point_unit_p (current_gdbarch)
		      && freg <= 13)
		    {
		      gdb_byte regval[MAX_REGISTER_SIZE];
		      struct type *regtype
                        = register_type (gdbarch, tdep->ppc_fp0_regnum);
		      convert_typed_floating (val, type, regval, regtype);
		      regcache_cooked_write (regcache,
                                             tdep->ppc_fp0_regnum + freg,
					     regval);
		    }
		  if (greg <= 10)
		    {
		      /* The ABI states "Single precision floating
		         point values are mapped to the first word in
		         a single doubleword" and "... floating point
		         values mapped to the first eight doublewords
		         of the parameter save area are also passed in
		         general registers").

		         This code interprets that to mean: store it,
		         left aligned, in the general register.  */
		      gdb_byte regval[MAX_REGISTER_SIZE];
		      memset (regval, 0, sizeof regval);
		      memcpy (regval, val, TYPE_LENGTH (type));
		      regcache_cooked_write (regcache,
					     tdep->ppc_gp0_regnum + greg,
					     regval);
		    }
		  write_memory (gparam, val, TYPE_LENGTH (type));
		}
	      /* Always consume parameter stack space.  */
	      freg++;
	      greg++;
	      gparam = align_up (gparam + TYPE_LENGTH (type), tdep->wordsize);
	    }
	  else if (TYPE_LENGTH (type) == 16 && TYPE_VECTOR (type)
		   && TYPE_CODE (type) == TYPE_CODE_ARRAY
		   && tdep->ppc_vr0_regnum >= 0)
	    {
	      /* In the Altivec ABI, vectors go in the vector
	         registers v2 .. v13, or when that runs out, a vector
	         annex which goes above all the normal parameters.
	         NOTE: cagney/2003-09-21: This is a guess based on the
	         PowerOpen Altivec ABI.  */
	      if (vreg <= 13)
		{
		  if (write_pass)
		    regcache_cooked_write (regcache,
					   tdep->ppc_vr0_regnum + vreg, val);
		  vreg++;
		}
	      else
		{
		  if (write_pass)
		    write_memory (vparam, val, TYPE_LENGTH (type));
		  vparam = align_up (vparam + TYPE_LENGTH (type), 16);
		}
	    }
	  else if ((TYPE_CODE (type) == TYPE_CODE_INT
		    || TYPE_CODE (type) == TYPE_CODE_ENUM
		    || TYPE_CODE (type) == TYPE_CODE_PTR)
		   && TYPE_LENGTH (type) <= 8)
	    {
	      /* Scalars and Pointers get sign[un]extended and go in
	         gpr3 .. gpr10.  They can also end up in memory.  */
	      if (write_pass)
		{
		  /* Sign extend the value, then store it unsigned.  */
		  ULONGEST word = unpack_long (type, val);
		  /* Convert any function code addresses into
		     descriptors.  */
		  if (TYPE_CODE (type) == TYPE_CODE_PTR
		      && TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_FUNC)
		    {
		      CORE_ADDR desc = word;
		      convert_code_addr_to_desc_addr (word, &desc);
		      word = desc;
		    }
		  if (greg <= 10)
		    regcache_cooked_write_unsigned (regcache,
						    tdep->ppc_gp0_regnum +
						    greg, word);
		  write_memory_unsigned_integer (gparam, tdep->wordsize,
						 word);
		}
	      greg++;
	      gparam = align_up (gparam + TYPE_LENGTH (type), tdep->wordsize);
	    }
	  else
	    {
	      int byte;
	      for (byte = 0; byte < TYPE_LENGTH (type);
		   byte += tdep->wordsize)
		{
		  if (write_pass && greg <= 10)
		    {
		      gdb_byte regval[MAX_REGISTER_SIZE];
		      int len = TYPE_LENGTH (type) - byte;
		      if (len > tdep->wordsize)
			len = tdep->wordsize;
		      memset (regval, 0, sizeof regval);
		      /* WARNING: cagney/2003-09-21: As best I can
		         tell, the ABI specifies that the value should
		         be left aligned.  Unfortunately, GCC doesn't
		         do this - it instead right aligns even sized
		         values and puts odd sized values on the
		         stack.  Work around that by putting both a
		         left and right aligned value into the
		         register (hopefully no one notices :-^).
		         Arrrgh!  */
		      /* Left aligned (8 byte values such as pointers
		         fill the buffer).  */
		      memcpy (regval, val + byte, len);
		      /* Right aligned (but only if even).  */
		      if (len == 1 || len == 2 || len == 4)
			memcpy (regval + tdep->wordsize - len,
				val + byte, len);
		      regcache_cooked_write (regcache, greg, regval);
		    }
		  greg++;
		}
	      if (write_pass)
		/* WARNING: cagney/2003-09-21: Strictly speaking, this
		   isn't necessary, unfortunately, GCC appears to get
		   "struct convention" parameter passing wrong putting
		   odd sized structures in memory instead of in a
		   register.  Work around this by always writing the
		   value to memory.  Fortunately, doing this
		   simplifies the code.  */
		write_memory (gparam, val, TYPE_LENGTH (type));
	      if (write_pass)
		/* WARNING: cagney/2004-06-20: It appears that GCC
		   likes to put structures containing a single
		   floating-point member in an FP register instead of
		   general general purpose.  */
	      /* Always consume parameter stack space.  */
	      gparam = align_up (gparam + TYPE_LENGTH (type), tdep->wordsize);
	    }
	}

      if (!write_pass)
	{
	  /* Save the true region sizes ready for the second pass.  */
	  vparam_size = vparam;
	  /* Make certain that the general parameter save area is at
	     least the minimum 8 registers (or doublewords) in size.  */
	  if (greg < 8)
	    gparam_size = 8 * tdep->wordsize;
	  else
	    gparam_size = gparam;
	}
    }

  /* Update %sp.   */
  regcache_cooked_write_signed (regcache, SP_REGNUM, sp);

  /* Write the backchain (it occupies WORDSIZED bytes).  */
  write_memory_signed_integer (sp, tdep->wordsize, back_chain);

  /* Point the inferior function call's return address at the dummy's
     breakpoint.  */
  regcache_cooked_write_signed (regcache, tdep->ppc_lr_regnum, bp_addr);

  /* Use the func_addr to find the descriptor, and use that to find
     the TOC.  */
  {
    CORE_ADDR desc_addr;
    if (convert_code_addr_to_desc_addr (func_addr, &desc_addr))
      {
	/* The TOC is the second double word in the descriptor.  */
	CORE_ADDR toc =
	  read_memory_unsigned_integer (desc_addr + tdep->wordsize,
					tdep->wordsize);
	regcache_cooked_write_unsigned (regcache,
					tdep->ppc_gp0_regnum + 2, toc);
      }
  }

  return sp;
}


/* The 64 bit ABI retun value convention.

   Return non-zero if the return-value is stored in a register, return
   0 if the return-value is instead stored on the stack (a.k.a.,
   struct return convention).

   For a return-value stored in a register: when WRITEBUF is non-NULL,
   copy the buffer to the corresponding register return-value location
   location; when READBUF is non-NULL, fill the buffer from the
   corresponding register return-value location.  */
enum return_value_convention
ppc64_sysv_abi_return_value (struct gdbarch *gdbarch, struct type *valtype,
			     struct regcache *regcache, gdb_byte *readbuf,
			     const gdb_byte *writebuf)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* This function exists to support a calling convention that
     requires floating-point registers.  It shouldn't be used on
     processors that lack them.  */
  gdb_assert (ppc_floating_point_unit_p (gdbarch));

  /* Floats and doubles in F1.  */
  if (TYPE_CODE (valtype) == TYPE_CODE_FLT && TYPE_LENGTH (valtype) <= 8)
    {
      gdb_byte regval[MAX_REGISTER_SIZE];
      struct type *regtype = register_type (gdbarch, tdep->ppc_fp0_regnum);
      if (writebuf != NULL)
	{
	  convert_typed_floating (writebuf, valtype, regval, regtype);
	  regcache_cooked_write (regcache, tdep->ppc_fp0_regnum + 1, regval);
	}
      if (readbuf != NULL)
	{
	  regcache_cooked_read (regcache, tdep->ppc_fp0_regnum + 1, regval);
	  convert_typed_floating (regval, regtype, readbuf, valtype);
	}
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  if ((TYPE_CODE (valtype) == TYPE_CODE_INT
       || TYPE_CODE (valtype) == TYPE_CODE_ENUM)
      && TYPE_LENGTH (valtype) <= 8)
    {
      /* Integers in r3.  */
      if (writebuf != NULL)
	{
	  /* Be careful to sign extend the value.  */
	  regcache_cooked_write_unsigned (regcache, tdep->ppc_gp0_regnum + 3,
					  unpack_long (valtype, writebuf));
	}
      if (readbuf != NULL)
	{
	  /* Extract the integer from r3.  Since this is truncating the
	     value, there isn't a sign extension problem.  */
	  ULONGEST regval;
	  regcache_cooked_read_unsigned (regcache, tdep->ppc_gp0_regnum + 3,
					 &regval);
	  store_unsigned_integer (readbuf, TYPE_LENGTH (valtype), regval);
	}
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  /* All pointers live in r3.  */
  if (TYPE_CODE (valtype) == TYPE_CODE_PTR)
    {
      /* All pointers live in r3.  */
      if (writebuf != NULL)
	regcache_cooked_write (regcache, tdep->ppc_gp0_regnum + 3, writebuf);
      if (readbuf != NULL)
	regcache_cooked_read (regcache, tdep->ppc_gp0_regnum + 3, readbuf);
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  if (TYPE_CODE (valtype) == TYPE_CODE_ARRAY
      && TYPE_LENGTH (valtype) <= 8
      && TYPE_CODE (TYPE_TARGET_TYPE (valtype)) == TYPE_CODE_INT
      && TYPE_LENGTH (TYPE_TARGET_TYPE (valtype)) == 1)
    {
      /* Small character arrays are returned, right justified, in r3.  */
      int offset = (register_size (gdbarch, tdep->ppc_gp0_regnum + 3)
		    - TYPE_LENGTH (valtype));
      if (writebuf != NULL)
	regcache_cooked_write_part (regcache, tdep->ppc_gp0_regnum + 3,
				    offset, TYPE_LENGTH (valtype), writebuf);
      if (readbuf != NULL)
	regcache_cooked_read_part (regcache, tdep->ppc_gp0_regnum + 3,
				   offset, TYPE_LENGTH (valtype), readbuf);
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  /* Big floating point values get stored in adjacent floating
     point registers.  */
  if (TYPE_CODE (valtype) == TYPE_CODE_FLT
      && (TYPE_LENGTH (valtype) == 16 || TYPE_LENGTH (valtype) == 32))
    {
      if (writebuf || readbuf != NULL)
	{
	  int i;
	  for (i = 0; i < TYPE_LENGTH (valtype) / 8; i++)
	    {
	      if (writebuf != NULL)
		regcache_cooked_write (regcache, tdep->ppc_fp0_regnum + 1 + i,
				       (const bfd_byte *) writebuf + i * 8);
	      if (readbuf != NULL)
		regcache_cooked_read (regcache, tdep->ppc_fp0_regnum + 1 + i,
				      (bfd_byte *) readbuf + i * 8);
	    }
	}
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  /* Complex values get returned in f1:f2, need to convert.  */
  if (TYPE_CODE (valtype) == TYPE_CODE_COMPLEX
      && (TYPE_LENGTH (valtype) == 8 || TYPE_LENGTH (valtype) == 16))
    {
      if (regcache != NULL)
	{
	  int i;
	  for (i = 0; i < 2; i++)
	    {
	      gdb_byte regval[MAX_REGISTER_SIZE];
	      struct type *regtype =
		register_type (current_gdbarch, tdep->ppc_fp0_regnum);
	      if (writebuf != NULL)
		{
		  convert_typed_floating ((const bfd_byte *) writebuf +
					  i * (TYPE_LENGTH (valtype) / 2),
					  valtype, regval, regtype);
		  regcache_cooked_write (regcache,
                                         tdep->ppc_fp0_regnum + 1 + i,
					 regval);
		}
	      if (readbuf != NULL)
		{
		  regcache_cooked_read (regcache,
                                        tdep->ppc_fp0_regnum + 1 + i,
                                        regval);
		  convert_typed_floating (regval, regtype,
					  (bfd_byte *) readbuf +
					  i * (TYPE_LENGTH (valtype) / 2),
					  valtype);
		}
	    }
	}
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  /* Big complex values get stored in f1:f4.  */
  if (TYPE_CODE (valtype) == TYPE_CODE_COMPLEX && TYPE_LENGTH (valtype) == 32)
    {
      if (regcache != NULL)
	{
	  int i;
	  for (i = 0; i < 4; i++)
	    {
	      if (writebuf != NULL)
		regcache_cooked_write (regcache, tdep->ppc_fp0_regnum + 1 + i,
				       (const bfd_byte *) writebuf + i * 8);
	      if (readbuf != NULL)
		regcache_cooked_read (regcache, tdep->ppc_fp0_regnum + 1 + i,
				      (bfd_byte *) readbuf + i * 8);
	    }
	}
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  return RETURN_VALUE_STRUCT_CONVENTION;
}

CORE_ADDR
ppc64_sysv_abi_adjust_breakpoint_address (struct gdbarch *gdbarch,
					  CORE_ADDR bpaddr)
{
  /* PPC64 SYSV specifies that the minimal-symbol "FN" should point at
     a function-descriptor while the corresponding minimal-symbol
     ".FN" should point at the entry point.  Consequently, a command
     like "break FN" applied to an object file with only minimal
     symbols, will insert the breakpoint into the descriptor at "FN"
     and not the function at ".FN".  Avoid this confusion by adjusting
     any attempt to set a descriptor breakpoint into a corresponding
     function breakpoint.  Note that GDB warns the user when this
     adjustment is applied - that's ok as otherwise the user will have
     no way of knowing why their breakpoint at "FN" resulted in the
     program stopping at ".FN".  */
  return gdbarch_convert_from_func_ptr_addr (gdbarch, bpaddr, &current_target);
}
