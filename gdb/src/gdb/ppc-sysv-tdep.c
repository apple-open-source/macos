/* Target-dependent code for PowerPC systems using the SVR4 ABI
   for GDB, the GNU debugger.

   Copyright 2000, 2001, 2002 Free Software Foundation, Inc.

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

#include "ppc-tdep.h"

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

/* Push the single argument ARG onto C, using the rules in ABI.  The
   parameter ARGNO is used for printing error and diagnostic messages.
   If DO_COPY is set, actually copy the data onto the stack/registers;
   otherwise just pre-flight the pushing to learn the appropriate
   offsets.  If FLOATONLY is set, we are scanning recursively through
   a structure to push floating-point arguments into floating-point registers. */

static void
ppc_push_argument
(struct ppc_stack_abi *abi, struct ppc_stack_context *c, struct value *arg,
 int argno, int do_copy, int floatonly)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  struct type *type = check_typedef (VALUE_TYPE (arg));
  int len = TYPE_LENGTH (type);

  char buf[16];

  c->argoffset = ROUND_UP (c->argoffset, 4);

  switch (TYPE_CODE (type)) {
      
  case TYPE_CODE_FLT:
    {
      if (c->freg <= abi->last_freg)
	{
	  struct value *rval = value_cast (builtin_type_double, arg);
	  struct type *rtype = check_typedef (VALUE_TYPE (rval));
	  int rlen = TYPE_LENGTH (rtype);

	  if ((len != 4) && (len != 8))
	    error ("floating point parameter had unexpected size");

	  if (rlen != 8)
	    error ("floating point parameter had unexpected size");

	  if (do_copy)
	    memcpy (&deprecated_registers[REGISTER_BYTE (FP0_REGNUM + c->freg)],
		    VALUE_CONTENTS (rval), rlen);
	  if (do_copy && ! floatonly && abi->fregs_shadow_gregs)
	    memcpy (&deprecated_registers[REGISTER_BYTE (c->greg)],
		    VALUE_CONTENTS (arg), len);
	  if (do_copy && ! floatonly && abi->regs_shadow_stack)
	    write_memory (c->sp + c->argoffset, VALUE_CONTENTS (arg), len);

	  c->freg++;
	  if (! floatonly && (abi->fregs_shadow_gregs) && (c->greg <= abi->last_greg))
	    c->greg += len / 4;
	  if (! floatonly && abi->regs_shadow_stack)
	    c->argoffset += len;
	}
      else if (! floatonly)
	{
	  if ((len != 4) && (len != 8))
	    error ("floating point parameter had unexpected size");

	  c->argoffset = ROUND_UP (c->argoffset, len);
	  if (do_copy)
	    write_memory (c->sp + c->argoffset, VALUE_CONTENTS (arg), len);
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

      if (floatonly)
	break;

      nregs = (len <= 4) ? 1 : 2;
      if ((len != 1) && (len != 2) && (len != 4) && (len != 8))
	error ("integer parameter had unexpected size");

      c->greg = abi->first_greg + (ROUND_UP ((c->greg - abi->first_greg), nregs));
      c->argoffset = ROUND_UP (c->argoffset, nregs * 4);
	    
      if ((c->greg + nregs) > (abi->last_greg + 1))
	{
	  c->greg = abi->last_greg + 1;

	  if (do_copy)
	    write_memory (c->sp + c->argoffset, (char *) VALUE_CONTENTS (arg), len);
	  c->argoffset += (nregs * 4);
	}
      else
	{
	  if (do_copy)
	    memcpy (&deprecated_registers[REGISTER_BYTE (c->greg)],
		    VALUE_CONTENTS (arg), nregs * 4);
	  if (do_copy && abi->regs_shadow_stack)
	    write_memory (c->sp + c->argoffset, (char *) VALUE_CONTENTS (arg), len);

	  c->greg += nregs;
	  if (abi->regs_shadow_stack)
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
		  write_memory (c->sp + c->structoffset, VALUE_CONTENTS (arg), len);
		  store_address (buf, 4, c->sp + c->structoffset);
		}
	      c->structoffset += ROUND_UP (len, 8);
	    }
	  else
	    if (do_copy)
	      {
		memset (buf, 0, 4);
		memcpy (buf, VALUE_CONTENTS (arg), len);
	      }

	  if (c->greg <= abi->last_greg)
	    {
	      if (do_copy)
		memcpy (&deprecated_registers[REGISTER_BYTE (c->greg)], buf, 4);
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
	      char *ptr = VALUE_CONTENTS (arg);
	      if (len < 4)
		{
		  memset (buf, 0, 4);
		  if ((len == 1) || (len == 2))
		    memcpy (buf + 4 - len, ptr, len);
		  else
		    memcpy (buf, ptr, len);
		  ptr = buf;
		}
	      memcpy (&deprecated_registers[REGISTER_BYTE (c->greg)], ptr,
		      (writereg < 4) ? 4 : writereg);
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
	    memcpy (&deprecated_registers[REGISTER_BYTE (tdep->ppc_vr0_regnum + c->vreg)],
		    VALUE_CONTENTS (arg), len);
	  c->vreg++;
	}
      else
	{
	  /* Vector arguments must be aligned to 16 bytes on
	     the stack. */
	  c->argoffset = ROUND_UP (c->argoffset, 16);
	  if (do_copy)
	    write_memory (c->sp + c->argoffset, VALUE_CONTENTS (arg), len);
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
  char buf[4];
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

  /* Make sure that we maintain 16 byte alignment */
  sp &= ~0x0f;

  /* Update %sp before proceeding any further */
  write_register (SP_REGNUM, sp);

  /* write the backchain */
  store_address (buf, 4, saved_sp);
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
      store_address (buf, 4, struct_addr);
      memcpy (&deprecated_registers[REGISTER_BYTE (write.greg)], buf, 4);
      write.greg++;
      if (abi->regs_shadow_stack)
	write.argoffset += 4;
    }

  /* Now fill in the registers and stack.  */
  for (argno = 0; argno < nargs; argno++)
    {
      ppc_push_argument (abi, &write, args[argno], argno, 1, 0);
    }

  target_store_registers (-1);
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
ppc_sysv_abi_push_arguments (int nargs, struct value **args, CORE_ADDR sp,
			     int struct_return, CORE_ADDR struct_addr)
{
  struct ppc_stack_abi abi;

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

  return ppc_push_arguments (&abi, nargs, args, sp, struct_return, struct_addr);
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
ppc_darwin_abi_push_arguments (int nargs, struct value **args, CORE_ADDR sp,
			       int struct_return, CORE_ADDR struct_addr)
{
  struct ppc_stack_abi abi;

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

  return ppc_push_arguments (&abi, nargs, args, sp, struct_return, struct_addr);
}

/* Until November 2001, gcc was not complying to the SYSV ABI for 
   returning structures less than or equal to 8 bytes in size.  It was
   returning everything in memory.  When this was corrected, it wasn't
   fixed for native platforms.  */
int     
ppc_sysv_abi_broken_use_struct_convention (int gcc_p, struct type *value_type)
{  
  if (TYPE_LENGTH (value_type) == 16 
      && TYPE_VECTOR (value_type))
    return 0;                            

  return generic_use_struct_convention (gcc_p, value_type);
}

/* Structures 8 bytes or less long are returned in the r3 & r4
   registers, according to the SYSV ABI. */
int
ppc_sysv_abi_use_struct_convention (int gcc_p, struct type *value_type)
{
  if (TYPE_LENGTH (value_type) == 16
      && TYPE_VECTOR (value_type))
    return 0;

  return (TYPE_LENGTH (value_type) > 8);
}   
