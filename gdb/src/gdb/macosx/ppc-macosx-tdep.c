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

#include "ppc-macosx-regs.h"
#include "ppc-macosx-tdep.h"
#include "ppc-macosx-frameinfo.h"
#include "ppc-macosx-frameops.h"

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

#include "libbfd.h"

#include "elf-bfd.h"
#include "dis-asm.h"
#include "ppc-tdep.h"
#include "gdbarch.h"
#include "osabi.h"

extern int backtrace_below_main;

#undef XMALLOC
#define XMALLOC(TYPE) ((TYPE*) xmalloc (sizeof (TYPE)))

/* 
   0x030     c linkage
   0x040     paramsavelen
   0x008     pad
   0x408     struct mcontext
   - 0x020   - ppc_exception_state_t
   - 0x0a0   - ppc_thread_state_t
   - 0x108   - ppc_float_state_t
   - 0x240   - ppc_vector_state_t
   0x040     siginfo_t
   0x020     ucontext
   0x0e0     redzone
*/

const unsigned int PPC_SIGCONTEXT_PC_OFFSET = 0x90;
const unsigned int PPC_SIGCONTEXT_SP_OFFSET = 0x9c;
 
static int ppc_debugflag = 0;
static unsigned int ppc_max_frame_size = UINT_MAX;

void ppc_debug (const char *fmt, ...)
{
  va_list ap;
  if (ppc_debugflag) {
    va_start (ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end (ap);
  }
}

/* function implementations */

void
ppc_init_extra_frame_info (int fromleaf, struct frame_info *frame)
{
  CHECK_FATAL (frame != NULL);

  frame->extra_info = (struct frame_extra_info *)
    frame_obstack_zalloc (sizeof (struct frame_extra_info));

  frame->extra_info->initial_sp = 0;
  frame->extra_info->bounds = NULL;
  frame->extra_info->props = 0;
}

void
ppc_print_extra_frame_info (struct frame_info *frame)
{
  if (get_frame_type (frame) == SIGTRAMP_FRAME) {
    printf_filtered (" This function was called from a signal handler.\n");
  } else {
    printf_filtered (" This function was not called from a signal handler.\n");
  }

  ppc_frame_cache_initial_stack_address (frame);
  if (frame->extra_info->initial_sp) {
    printf_filtered (" The initial stack pointer for this frame was 0x%lx.\n", 
		     (unsigned long) frame->extra_info->initial_sp);
  } else {
    printf_filtered (" Unable to determine initial stack pointer for this frame.\n");
  }    

  ppc_frame_cache_boundaries (frame, NULL);
  if (frame->extra_info->bounds != NULL) {
    ppc_print_boundaries (frame->extra_info->bounds);
  } else {
    printf_filtered (" Unable to determine function boundary information.\n");
  }

  ppc_frame_cache_properties (frame, NULL);
  if (frame->extra_info->props != NULL) {
    ppc_print_properties (frame->extra_info->props);
  } else {
    printf_filtered (" Unable to determine function property information.\n");
  }
}
/* We need to make sure that ppc_init_frame_pc_first won't longjmp, 
   since if it does it can leave a frame structure with no extra_info
   allocated in get_prev_frame, and there are way too many places that
   assume that a valid frame has a valid extra_info to catch them all. */

struct ppc_init_frame_pc_args
{
  int fromleaf;
  struct frame_info *frame;
};

int
ppc_init_frame_pc_first_unsafe (struct ppc_init_frame_pc_args *args)
{
  struct frame_info *frame = args->frame;
  struct frame_info *next;

  CHECK_FATAL (frame != NULL);
  next = get_next_frame (frame);
  CHECK_FATAL (next != NULL);
  frame->pc = ppc_frame_saved_pc (next);

  return 1;
}

CORE_ADDR
ppc_init_frame_pc_first (int fromleaf, struct frame_info *frame)
{
  struct ppc_init_frame_pc_args args;

  args.fromleaf = fromleaf;
  args.frame = frame;

  if (! catch_errors ((catch_errors_ftype *) ppc_init_frame_pc_first_unsafe, 
		      &args, "", RETURN_MASK_ERROR))
    {
      ppc_debug ("ppc_init_frame_pc_first: got an error calling %s.\n", 
		 "ppc_frame_saved_pc.\n");
    }

  return frame->pc;
}

CORE_ADDR
ppc_init_frame_pc (int fromleaf, struct frame_info *frame)
{
  CHECK_FATAL (frame != NULL);
  return frame->pc;
}

/* This function hides a little complication when you are looking for
   a the saved pc in a frame which either does not save the pc, or saves
   it but hasn't done so yet.  In that case, the pc may still be in the
   link register, but it ALSO might have gotten displaced by the PIC base
   setting call - which does "blc pc+4; mflr r31" so the lr is wrong for
   a little while. */

CORE_ADDR
ppc_get_unsaved_pc (struct frame_info *frame, ppc_function_properties *props)
{
  if ((props->lr_reg == -1)
      || ((props->lr_invalid == 0) 
	  || (frame->pc <= props->lr_invalid)
	  || (frame->pc > props->lr_valid_again)))
    {
      ULONGEST val;
      ppc_debug ("ppc_get_unsaved_pc: link register was not saved.\n");
      frame_read_unsigned_register (frame, LR_REGNUM, &val);
      return val;
    }
  else
    {
      ULONGEST val;
      ppc_debug ("ppc_get_unsaved_pc: link register stashed in register %d.\n",
		 props->lr_reg);
      frame_read_unsigned_register (frame, props->lr_reg, &val);
      return val;
    }
}

CORE_ADDR
ppc_frame_find_pc (frame)
     struct frame_info *frame;
{
  CORE_ADDR prev;

  CHECK_FATAL (frame != NULL);

  if (DEPRECATED_PC_IN_CALL_DUMMY (frame->pc, frame->frame, frame->frame))
    {
      ULONGEST pc;
      frame_unwind_unsigned_register (frame, PC_REGNUM, &pc);
      return pc;
    }

  if (get_next_frame (frame))
    if (DEPRECATED_PC_IN_CALL_DUMMY (get_next_frame (frame)->pc, get_next_frame (frame)->frame, get_next_frame (frame)->frame))
      {
	ULONGEST pc;
	frame_unwind_unsigned_register (frame, LR_REGNUM, &pc);
	return pc;
      }
  
  if (get_frame_type (frame) == SIGTRAMP_FRAME) {
    CORE_ADDR psp = read_memory_unsigned_integer (frame->frame, 4);
    return read_memory_unsigned_integer (psp + PPC_SIGCONTEXT_PC_OFFSET, 4);
  }

  prev = ppc_frame_chain (frame);
  if ((prev == 0) || (! ppc_frame_chain_valid (prev, frame))) { 
    ppc_debug ("ppc_frame_find_pc: previous stack frame not valid: returning 0\n");
    return 0; 
  }
  ppc_debug ("ppc_frame_find_pc: value of prev is 0x%lx\n", (unsigned long) prev);

  if (ppc_frame_cache_properties (frame, NULL) != 0) 
    {
      ppc_debug ("ppc_frame_find_pc: unable to find properties of function containing 0x%lx\n",
		 (unsigned long) frame->pc);
      ppc_debug ("ppc_frame_find_pc: assuming link register saved in normal location\n");
      
      if (ppc_frameless_function_invocation (frame)) 
	{
	  /* FIXME: Formally, we ought to check for frameless function
	     invocation, but in fact, ppc_frameless_function_invocation
	     first calls ppc_frame_cache_properties and returns 0 if it
	     fails.  Since this just failed above, we won't ever get
	     into this branch. But I don't know another way to figure
	     this out. */
	  return read_register (LR_REGNUM);
	} 
      else 
	{
	  ppc_function_properties lprops;
	  CORE_ADDR body_start;
	  if (frame->pc % 4 != 0)
	    {
	      /* My we did get lost...  Can't call ppc_parse_instructions,
		 it will just error out.  So let's just guess.  If our frame
		 is the next from the top, try the link register, otherwise 
		 the stack is probably the safest place to look at this point...  */
	      if (frame->level == 1)
		{
		  ULONGEST val;
		  frame_read_unsigned_register (frame, LR_REGNUM, &val);
		  return val;
		}
	      else
		return read_memory_unsigned_integer (prev + DEFAULT_LR_SAVE, 4);
	    }

	  body_start = ppc_parse_instructions (frame->pc, INVALID_ADDRESS, 
					       &lprops);
          
	  /* We get here if we are a bit lost: after all we weren't
	     able to successfully run ppc_frame_cache_properties.  So
	     we should treat the lprops with some caution.  One common
	     mistake is to falsly assume the lr hasn't been saved
	     (since un-interpretible prologue and frameless prologue
	     look kind of the same).  So add this obvious check
	     here for being a leaf fn. in the first branch... */
	  
	  if ((get_next_frame (frame) == NULL && lprops.lr_saved == 0) || 
	      (lprops.lr_saved >= frame->pc))
	    {
      	      /* Either we aren't saving the link register, or our scan
		 shows that the link register WILL be saved, but has not
		 been yet.  So figure out where it actually is.  To do this,
		 we need to see if the lr is ever stomped, and if so, if we
		 are within that part of the prologue.
	      */
	      return ppc_get_unsaved_pc (frame, &lprops);
	    }
	  else
	    {
	      /* The link register is safely on the stack... */
	      return read_memory_unsigned_integer (prev + DEFAULT_LR_SAVE, 4);
	    }
	}
    }
  else
    {
      ppc_function_properties *props;

      props = frame->extra_info->props;
      CHECK_FATAL (props != NULL);

      if (props->lr_saved)
	{
	  if (props->lr_saved < frame->pc)
	    {
	      /* The link register is safely on the stack... */
	      return read_memory_unsigned_integer (prev 
						   + props->lr_offset, 4);
	    }
	  else 
	    {
	      ppc_debug ("ppc_frame_find_pc: function did not save link register\n");
	      return ppc_get_unsaved_pc (frame, props);
	    }
        }
      else if ((get_next_frame (frame) != NULL)
	       && (get_frame_type (get_next_frame (frame)) == SIGTRAMP_FRAME))
	{
	  ppc_debug ("ppc_frame_find_pc: %s\n", 
		     "using link area from signal handler.");
	  return read_memory_unsigned_integer (frame->frame - 0x320 
					       + DEFAULT_LR_SAVE, 4);
	}
      else if (get_next_frame (frame) && ppc_is_dummy_frame (get_next_frame (frame))) 
	{
	  ppc_debug ("ppc_frame_find_pc: using link area from call dummy\n");
	  return read_memory_unsigned_integer (frame->frame - 0x1c, 4);
	}
      else if (!props->lr_saved)
	{
	  return ppc_get_unsaved_pc (frame, props);
	}
      else
	{
	  ppc_debug ("ppc_frame_find_pc: function is not a leaf\n");
	  ppc_debug ("ppc_frame_find_pc: assuming link register saved in normal location\n");
	  return read_memory_unsigned_integer (prev + DEFAULT_LR_SAVE, 4);
	}
    }
  /* Should never get here, just quiets the compiler. */
  return 0;
}

CORE_ADDR
ppc_frame_saved_pc (struct frame_info *frame)
{
  return (ppc_frame_find_pc (frame));
}


CORE_ADDR
ppc_frame_saved_pc_after_call (struct frame_info *frame)
{
  CHECK_FATAL (frame != NULL);
  return read_register (LR_REGNUM);
}

CORE_ADDR
ppc_frame_chain (struct frame_info *frame)
{
  CORE_ADDR psp = read_memory_unsigned_integer (frame->frame, 4);

  if (DEPRECATED_PC_IN_CALL_DUMMY (frame->pc, frame->frame, frame->frame))
    return psp;
	    
  if (get_frame_type (frame) == SIGTRAMP_FRAME) {
    return read_memory_unsigned_integer (psp + PPC_SIGCONTEXT_SP_OFFSET, 4);
  }

  /* If a frameless function is interrupted by a signal, no change to
     the stack pointer */
  if (get_next_frame (frame) != NULL
      && (get_frame_type (get_next_frame (frame)) == SIGTRAMP_FRAME)
      && ppc_frameless_function_invocation (frame)) {
    return frame->frame;
  }

  return psp;
}

int 
ppc_frame_chain_valid (CORE_ADDR chain, struct frame_info *frame)
{
  unsigned long retval;

  if (chain == 0) { return 0; }

  /* reached end of stack? */
  if (!safe_read_memory_unsigned_integer (chain, 4, &retval))
    {
        ppc_debug ("ppc_frame_chain_valid: Got an error reading at 0x%lx",
                   chain);
        return 0;
    }
  else
    if (retval == 0) { return 0; }

#if 0
  if (inside_entry_func (frame->pc)) { return 0; }
  if (inside_main_func (frame->pc)) { return 0; }
#endif

  /* check for bogus stack frames */
  if (! (chain >= frame->frame)) {
    warning ("ppc_frame_chain_valid: stack pointer from 0x%lx to 0x%lx "
	     "grows upward; assuming invalid\n",
	     (unsigned long) frame->frame, (unsigned long) chain);
    return 0;
  }
  if ((chain - frame->frame) > ppc_max_frame_size) {
    warning ("ppc_frame_chain_valid: stack frame from 0x%lx to 0x%lx "
	     "larger than ppc-maximum-frame-size bytes; assuming invalid",
	     (unsigned long) frame->frame, (unsigned long) chain);
    return 0;
  }

  return 1;
}

int
ppc_is_dummy_frame (struct frame_info *frame)
{
  return (DEPRECATED_PC_IN_CALL_DUMMY (get_frame_pc (frame),
				       get_frame_base (frame),
				       get_frame_base (frame)));
}

/* Return the address of a frame. This is the inital %sp value when the frame
   was first allocated. For functions calling alloca(), it might be saved in
   the frame pointer register. */

CORE_ADDR
ppc_frame_cache_initial_stack_address (struct frame_info *frame)
{
  CHECK_FATAL (frame != NULL);
  CHECK_FATAL (frame->extra_info != NULL);
    
  if (frame->extra_info->initial_sp == 0) { 
    frame->extra_info->initial_sp = ppc_frame_initial_stack_address (frame);
  }
  return frame->extra_info->initial_sp;
}

CORE_ADDR
ppc_frame_initial_stack_address (struct frame_info *frame)
{
  CORE_ADDR tmpaddr;
  struct frame_info *callee;

  /* Find out if this function is using an alloca register. */

  if (ppc_frame_cache_properties (frame, NULL) != 0) {
    ppc_debug ("ppc_frame_initial_stack_address: unable to find properties of " 
		 "function containing 0x%lx\n", frame->pc);
    return 0;
  }

  /* Read and cache saved registers if necessary. */

  ppc_frame_cache_saved_regs (frame);

  /* If no alloca register is used, then frame->frame is the value of
     %sp for this frame, and it is valid.  Also check to make sure we 
     aren't in the prologue BEFORE the frameptr is set up.  If we are
     use frame->frame since it will still be valid.  */

  if (!frame->extra_info->props->frameptr_used
      || frame->extra_info->props->frameptr_pc >= frame->pc) {
    frame->extra_info->initial_sp = frame->frame;
    return frame->extra_info->initial_sp;
  }

  /* This function has an alloca register. If this is the top-most frame
     (with the lowest address), the value in alloca register is valid. */

  if (! get_next_frame (frame)) 
    {
      frame->extra_info->initial_sp 
	= read_register (frame->extra_info->props->frameptr_reg);     
      return frame->extra_info->initial_sp;
    }

  /* Otherwise, this is a caller frame. Callee has usually already saved
     registers, but there are exceptions (such as when the callee
     has no parameters). Find the address in which caller's alloca
     register is saved. */

  for (callee = get_next_frame (frame); callee != NULL; 
       callee = get_next_frame (callee)) 
    {

      ppc_frame_cache_saved_regs (callee);
      
      /* this is the address in which alloca register is saved. */
      
      tmpaddr = callee->saved_regs[frame->extra_info->props->frameptr_reg];
      if (tmpaddr) {
	frame->extra_info->initial_sp 
	  = read_memory_unsigned_integer (tmpaddr, 4); 
	return frame->extra_info->initial_sp;
      }

      /* Go look into deeper levels of the frame chain to see if any one of
	 the callees has saved the frame pointer register. */
    }

  /* If frame pointer register was not saved, by the callee (or any of
     its callees) then the value in the register is still good. */

  frame->extra_info->initial_sp 
    = read_register (frame->extra_info->props->frameptr_reg);     
  return frame->extra_info->initial_sp;
}

CORE_ADDR
ppc_extract_struct_value_address (char regbuf[REGISTER_BYTES])
{
  return extract_unsigned_integer (&regbuf[REGISTER_BYTE (GP0_REGNUM + 3)], 4);
}

void
ppc_extract_return_value (struct type *valtype, char *regbuf, char *valbuf)
{
  int offset = 0;

  if (TYPE_CODE (valtype) == TYPE_CODE_FLT)
    {

      double dd;
      float ff;
      /* floats and doubles are returned in fpr1. fpr's have a size of 8 bytes.
         We need to truncate the return value into float size (4 byte) if
         necessary. */

      if (TYPE_LENGTH (valtype) > 4)	/* this is a double */
	memcpy (valbuf,
		&regbuf[REGISTER_BYTE (FP0_REGNUM + 1)],
		TYPE_LENGTH (valtype));
      else
	{			/* float */
	  memcpy (&dd, &regbuf[REGISTER_BYTE (FP0_REGNUM + 1)], 8);
	  ff = (float) dd;
	  memcpy (valbuf, &ff, sizeof (float));
	}
    }
  else if (TYPE_CODE (valtype) == TYPE_CODE_ARRAY
           && TYPE_LENGTH (valtype) == 16
           && TYPE_VECTOR (valtype))
    {
      memcpy (valbuf, regbuf + REGISTER_BYTE (VP0_REGNUM + 2),
	      TYPE_LENGTH (valtype));
    }
  else
    {
      /* return value is copied starting from r3. */
      if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG
	  && TYPE_LENGTH (valtype) < REGISTER_RAW_SIZE (3))
	offset = REGISTER_RAW_SIZE (3) - TYPE_LENGTH (valtype);

      memcpy (valbuf,
	      regbuf + REGISTER_BYTE (3) + offset,
	      TYPE_LENGTH (valtype));
    }
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
  if (ret != 0) { return 0; }

  return bounds.body_start;
}

/* Determines whether the current frame has allocated a frame on the
   stack or not.  */

int
ppc_frameless_function_invocation (struct frame_info *frame)
{
  /* if not a leaf, it's not frameless (unless it was interrupted by a
     signal or a call_dummy) */

  if ((get_next_frame (frame) != NULL)
      && ! (get_frame_type (get_next_frame (frame)) == SIGTRAMP_FRAME)
      && ! ppc_is_dummy_frame (get_next_frame (frame)))
    { 
      return 0; 
    }

  if (ppc_frame_cache_properties (frame, NULL) != 0) {
    /* Distinguish two cases here.  In general, it is safer to assume that the
       function is not frameless if we don't know any better.  But if the pc is not
       readable, then we probably crashed chasing a bad function pointer, in which
       case it is for all practical purposes frameless.  
       FIXME: We should actually go through and get ppc_frame_cache_properties to
       return error codes that we can use to distinguish these two cases.  */
    
    unsigned long op;
    if (frame->pc % 4 != 0 || !safe_read_memory_unsigned_integer (frame->pc, 4, &op))
      {
	ppc_debug ("frameless_function_invocation: unable to find properties of " 
		   "function containing %s which is not readable; assuming frameless\n", 
		   paddr_nz (frame->pc));
	return 1;
      }
    else
      {
	ppc_debug ("frameless_function_invocation: unable to find properties of " 
		   "function containing %s; assuming not frameless\n", 
		   paddr_nz (frame->pc));
	return 0;
      }
  }
  
  return frame->extra_info->props->frameless;
}

const char *gdb_register_names[] =
{
  "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
  "r8", "r9", "r10","r11","r12","r13","r14","r15",
  "r16","r17","r18","r19","r20","r21","r22","r23",
  "r24","r25","r26","r27","r28","r29","r30","r31",
  "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
  "f8", "f9", "f10","f11","f12","f13","f14","f15",
  "f16","f17","f18","f19","f20","f21","f22","f23",
  "f24","f25","f26","f27","f28","f29","f30","f31",
  "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
  "v8", "v9", "v10","v11","v12","v13","v14","v15",
  "v16","v17","v18","v19","v20","v21","v22","v23",
  "v24","v25","v26","v27","v28","v29","v30","v31",
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

void ppc_store_struct_return (CORE_ADDR addr, CORE_ADDR sp)
{
  write_register (SRA_REGNUM, addr);
}

void ppc_store_return_value (struct type *type, char *valbuf)
{
  /* Floating point values are returned starting from FPR1 and up.
     Say a double_double_double type could be returned in
     FPR1/FPR2/FPR3 triple.  */

  /* Everything else is returned in GPR3 and up.  */

  unsigned int regbase = -1;
  unsigned int offset = 0;

  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    regbase = FP0_REGNUM + 1;
  else if ((TYPE_CODE (type) == TYPE_CODE_ARRAY) && TYPE_VECTOR (type))
    regbase = VP0_REGNUM + 2;
  else
    regbase = GP0_REGNUM + 3;
  
  if ((REGISTER_RAW_SIZE (regbase) > TYPE_LENGTH (type))
      && (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG))
  {
    offset = REGISTER_RAW_SIZE (regbase) - TYPE_LENGTH (type);
  }

  if ((TYPE_CODE (type) == TYPE_CODE_FLT) && (TYPE_LENGTH (type) < 8))
    {
      char buf[8];
      double d = extract_floating (valbuf, TYPE_LENGTH (type));
      store_floating (buf, 8, d);
      deprecated_write_register_bytes (REGISTER_BYTE (regbase), buf, 8);
    }
  else
    deprecated_write_register_bytes (REGISTER_BYTE (regbase) + offset, valbuf, TYPE_LENGTH (type));
}

CORE_ADDR ppc_push_return_address (CORE_ADDR pc, CORE_ADDR sp)
{
  unsigned char buf[4];
  store_address (buf, 4, CALL_DUMMY_ADDRESS ());
  write_register (LR_REGNUM, CALL_DUMMY_ADDRESS ());
  write_memory (sp + 8, buf, 4);
  return sp;
}

/* Nonzero if register N requires conversion
   from raw format to virtual format.
   The register format for rs6000 floating point registers is always
   double, we need a conversion if the memory format is float.  */

int ppc_register_convertible (int regno)
{
  return ((regno >= FIRST_FP_REGNUM) && (regno <= LAST_FP_REGNUM));
}

/* Convert data from raw format for register REGNUM in buffer FROM
   to virtual format with type TYPE in buffer TO.  */

void ppc_register_convert_to_virtual
(int regno, struct type *type, char *from, char *to)
{ 
  if (TYPE_LENGTH (type) != REGISTER_RAW_SIZE (regno)) 
    { 
      double val = extract_floating (from, REGISTER_RAW_SIZE (regno)); 
      store_floating (to, TYPE_LENGTH (type), val); 
    } 
  else 
    memcpy (to, from, REGISTER_RAW_SIZE (regno)); 
}

/* Convert data from virtual format with type TYPE in buffer FROM
   to raw format for register REGNUM in buffer TO.  */

void ppc_register_convert_to_raw
(struct type *type, int regno, char *from, char *to)
{ 
  if (TYPE_LENGTH (type) != REGISTER_RAW_SIZE (regno)) 
    { 
      double val = extract_floating (from, TYPE_LENGTH (type)); 
      store_floating (to, REGISTER_RAW_SIZE (regno), val); 
    } 
  else 
    memcpy (to, from, REGISTER_RAW_SIZE (regno)); 
}

/* Sequence of bytes for breakpoint instruction.  */

#define BIG_BREAKPOINT { 0x7f, 0xe0, 0x00, 0x08 }
#define LITTLE_BREAKPOINT { 0x08, 0x00, 0xe0, 0x7f }

static const unsigned char *
ppc_breakpoint_from_pc (CORE_ADDR *addr, int *size)
{
  static unsigned char big_breakpoint[] = BIG_BREAKPOINT;
  static unsigned char little_breakpoint[] = LITTLE_BREAKPOINT;
  *size = 4;
  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
    return big_breakpoint;
  else
    return little_breakpoint;
}

static struct type *
ppc_register_virtual_type (int n)
{
  if (((n >= FIRST_GP_REGNUM) && (n <= LAST_GP_REGNUM))
      || (n == PC_REGNUM)
      || (n == PS_REGNUM)
      || (n == LR_REGNUM)
      || (n == CTR_REGNUM)
      || (n == XER_REGNUM))
    {
      if (gdbarch_osabi (current_gdbarch) == GDB_OSABI_DARWIN64)
	return builtin_type_unsigned_long_long;
      else
	return builtin_type_unsigned_int;
    }
  if ((n >= FIRST_VP_REGNUM) && (n <= LAST_VP_REGNUM))
    return builtin_type_vec128;
  if ((n >= FIRST_FP_REGNUM) && (n <= LAST_FP_REGNUM))
    return builtin_type_double;
  return builtin_type_unsigned_int;
}

int
ppc_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
			 struct reggroup *group)
{
  if ((regnum == VSCR_REGNUM) || (regnum == VRSAVE_REGNUM))
    if (group == vector_reggroup)
      return 1;

  if (regnum == FPSCR_REGNUM)
    if (group == float_reggroup)
      return 1;

  return default_register_reggroup_p (gdbarch, regnum, group);
}

int ppc_use_struct_convention (int gccp, struct type *value_type)
{
  if ((TYPE_LENGTH (value_type) == 16)
      && TYPE_VECTOR (value_type))
    return 0;

  return 1;
}

/* keep this as multiple of 16 ($sp requires 16 byte alignment) */

#define INSTRUCTION_SIZE 4

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
  tdep->ppc_lr_regnum = LR_REGNUM;

  set_gdbarch_sp_regnum (gdbarch, SP_REGNUM);
  set_gdbarch_fp_regnum (gdbarch, FP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, PC_REGNUM);
  set_gdbarch_ps_regnum (gdbarch, -1);
  set_gdbarch_fp0_regnum (gdbarch, FP0_REGNUM);
  set_gdbarch_npc_regnum (gdbarch, -1);
  
  set_gdbarch_read_pc (gdbarch, generic_target_read_pc);
  set_gdbarch_write_pc (gdbarch, generic_target_write_pc);
  set_gdbarch_read_fp (gdbarch, generic_target_read_fp);
  set_gdbarch_read_sp (gdbarch, generic_target_read_sp);
  set_gdbarch_write_sp (gdbarch, generic_target_write_sp);

  set_gdbarch_num_regs (gdbarch, NUM_REGS);
  set_gdbarch_sp_regnum (gdbarch, SP_REGNUM);
  set_gdbarch_fp_regnum (gdbarch, FP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, PC_REGNUM);
  set_gdbarch_register_name (gdbarch, ppc_register_name);
  set_gdbarch_register_bytes (gdbarch, REGISTER_BYTES);
  set_gdbarch_max_register_raw_size (gdbarch, 16);
  set_gdbarch_max_register_virtual_size (gdbarch, 16);
  set_gdbarch_register_virtual_type (gdbarch, ppc_register_virtual_type);

  set_gdbarch_ptr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_short_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_int_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_long_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_float_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_long_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_addr_bit (gdbarch, 4 * TARGET_CHAR_BIT);

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
      set_gdbarch_long_double_format (gdbarch, &floatformat_ieee_double_little);
      break;
    default:
      internal_error (__FILE__, __LINE__, "ppc_gdbarch_init: bad byte order for float format");
    }

  set_gdbarch_call_dummy_length (gdbarch, 0);
  set_gdbarch_call_dummy_location (gdbarch, AT_ENTRY_POINT);
  set_gdbarch_call_dummy_address (gdbarch, entry_point_address);
  set_gdbarch_call_dummy_breakpoint_offset_p (gdbarch, 1);
  set_gdbarch_call_dummy_breakpoint_offset (gdbarch, 0);
  set_gdbarch_call_dummy_start_offset (gdbarch, 0);

  set_gdbarch_sizeof_call_dummy_words (gdbarch, 0);
  set_gdbarch_call_dummy_words (gdbarch, NULL);

  set_gdbarch_call_dummy_p (gdbarch, 1);
  set_gdbarch_call_dummy_stack_adjust_p (gdbarch, 0);
  set_gdbarch_push_dummy_frame (gdbarch, generic_push_dummy_frame);
  set_gdbarch_save_dummy_frame_tos (gdbarch, generic_save_dummy_frame_tos);
  set_gdbarch_call_dummy_stack_adjust_p (gdbarch, 0);
  set_gdbarch_fix_call_dummy (gdbarch, generic_fix_call_dummy);
  set_gdbarch_push_return_address (gdbarch, ppc_push_return_address);

  set_gdbarch_register_convertible (gdbarch, ppc_register_convertible);
  set_gdbarch_register_convert_to_virtual (gdbarch, ppc_register_convert_to_virtual);
  set_gdbarch_register_convert_to_raw (gdbarch, ppc_register_convert_to_raw);
  set_gdbarch_stab_reg_to_regnum (gdbarch, ppc_macosx_stab_reg_to_regnum);

  set_gdbarch_deprecated_extract_return_value (gdbarch, ppc_extract_return_value);
  
  set_gdbarch_push_arguments (gdbarch, ppc_darwin_abi_push_arguments);

  set_gdbarch_store_struct_return (gdbarch, ppc_store_struct_return);
  set_gdbarch_deprecated_store_return_value (gdbarch, ppc_store_return_value);
  set_gdbarch_deprecated_extract_struct_value_address (gdbarch, ppc_extract_struct_value_address);
  set_gdbarch_pop_frame (gdbarch, rs6000_pop_frame);
  set_gdbarch_use_struct_convention (gdbarch, ppc_use_struct_convention);

  set_gdbarch_skip_prologue (gdbarch, ppc_skip_prologue);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_decr_pc_after_break (gdbarch, 0);
  set_gdbarch_function_start_offset (gdbarch, 0);
  set_gdbarch_breakpoint_from_pc (gdbarch, ppc_breakpoint_from_pc);

  set_gdbarch_frame_args_skip (gdbarch, 0);

  set_gdbarch_frame_chain_valid (gdbarch, ppc_frame_chain_valid);

  set_gdbarch_frameless_function_invocation (gdbarch, ppc_frameless_function_invocation);
  set_gdbarch_frame_chain (gdbarch, ppc_frame_chain);
  set_gdbarch_frame_saved_pc (gdbarch, ppc_frame_saved_pc);

  set_gdbarch_frame_init_saved_regs (gdbarch, ppc_frame_cache_saved_regs);
  set_gdbarch_init_extra_frame_info (gdbarch, ppc_init_extra_frame_info);

  set_gdbarch_frame_args_address (gdbarch, ppc_frame_cache_initial_stack_address);
  set_gdbarch_frame_locals_address (gdbarch, ppc_frame_cache_initial_stack_address);
  set_gdbarch_saved_pc_after_call (gdbarch, ppc_frame_saved_pc_after_call);

  set_gdbarch_skip_trampoline_code (gdbarch, ppc_macosx_skip_trampoline_code);
  set_gdbarch_dynamic_trampoline_nextpc (gdbarch, ppc_macosx_dynamic_trampoline_nextpc);
  set_gdbarch_frame_num_args (gdbarch, frame_num_args_unknown);

  set_gdbarch_in_solib_call_trampoline (gdbarch, ppc_macosx_in_solib_call_trampoline);
  set_gdbarch_in_solib_return_trampoline (gdbarch, ppc_macosx_in_solib_return_trampoline);
  set_gdbarch_deprecated_init_frame_pc_first (gdbarch, ppc_init_frame_pc_first);
  set_gdbarch_deprecated_init_frame_pc (gdbarch, ppc_init_frame_pc);

  set_gdbarch_register_reggroup_p (gdbarch, ppc_register_reggroup_p);

  /* For now, initialize for a 32-bit ABI by default, in case no osabi
     is recognized by GDB.  This prevents problems when attaching to
     other processes, where exec_bfd does not get set.  We should
     probably remove this call once we fix exec_bfd to be set properly
     when attaching. */
  ppc_macosx_init_abi (info, gdbarch);

  /* Hook in ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

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
		     void (print_fun) (struct ui_out *uiout, int frame_num, 
				       CORE_ADDR pc, CORE_ADDR fp))
{
  CORE_ADDR fp = 0;
  static CORE_ADDR sigtramp_start = 0;
  static CORE_ADDR sigtramp_end = 0;
  struct frame_info *fi = NULL;
  int i = 0;
  int err = 0;
  unsigned long next_fp;
  unsigned long pc;

  if (sigtramp_start == 0) 
    {
      char *name;
      struct minimal_symbol *msymbol;

      msymbol = lookup_minimal_symbol ("_sigtramp", NULL, NULL);
      if (msymbol == NULL)
	warning ("Couldn't find minimal symbol for \"_sigtramp\" - backtraces may be unreliable");
      else
	{
	  pc = SYMBOL_VALUE_ADDRESS (msymbol);
	  if (find_pc_partial_function (pc, &name, 
					&sigtramp_start, &sigtramp_end) == 0)
	    {
	      error ("Couldn't find _sigtramp symbol -- backtraces will be unreliable");
	    }
	}
    }

  /* Get the first two frames.  If anything funky is going on, it will
     be here.  The second frame helps us get above frameless functions
     called from signal handlers.  Above these frames we have to deal
     with sigtramps and alloca frames, that is about all. */  

  if (show_frames)
    ui_out_list_begin (uiout, "frames");
  
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
    print_fun (uiout, i, fi->pc, fi->frame);
  i++;

  if (i >= count_limit)
    goto ppc_count_finish;

  fi = get_prev_frame (fi);
  if (fi == NULL)
    goto ppc_count_finish;

  pc = fi->pc;
  fp = fi->frame;

  if (show_frames && print_fun && (i < print_limit))
    print_fun (uiout, i, pc, fp);
  i++;
  
  if (!backtrace_below_main && inside_main_func (pc))
    goto ppc_count_finish;

  if (! safe_read_memory_unsigned_integer (fp, 4, &next_fp))
    goto ppc_count_finish;

  if (i >= count_limit)
    goto ppc_count_finish;

  while (1)
    {
      if ((sigtramp_start<= pc) && (pc <= sigtramp_end))
	{
	  fp = next_fp + 0x70 + 0xc;
	  if (! safe_read_memory_unsigned_integer (fp, 4, &next_fp)) 
	    goto ppc_count_finish;
	  if (!safe_read_memory_unsigned_integer (fp - 0xc, 4, &pc))
	    goto ppc_count_finish;
	  fp = next_fp;
	  if (! safe_read_memory_unsigned_integer (fp, 4, &next_fp))
	    goto ppc_count_finish;
	}
      else
	{
	  fp = next_fp;
	  if (! safe_read_memory_unsigned_integer (fp, 4, &next_fp))
	    goto ppc_count_finish;
	  if (next_fp == 0)
	    goto ppc_count_finish;
	  if (! safe_read_memory_unsigned_integer
	      (fp + DEFAULT_LR_SAVE, 4, &pc))
	    goto ppc_count_finish;
	}	

      if (show_frames && print_fun && (i < print_limit))
	print_fun (uiout, i, pc, fp);
      i++;

      if (!backtrace_below_main && inside_main_func (pc))
	goto ppc_count_finish;
      
      if (i >= count_limit)
	goto ppc_count_finish;
    }
  
 ppc_count_finish:
  if (show_frames)
    ui_out_list_end (uiout);
  
  *count = i;
  return (! err);
}

CORE_ADDR ppc_macosx_skip_trampoline_code (CORE_ADDR pc)
{
  CORE_ADDR newpc;

  newpc = dyld_symbol_stub_function_address (pc, NULL);
  if (newpc != 0)
    return newpc;

  newpc = decode_fix_and_continue_trampoline (pc);
  return newpc;
}

CORE_ADDR ppc_macosx_dynamic_trampoline_nextpc (CORE_ADDR pc)
{
  return dyld_symbol_stub_function_address (pc, NULL);
}

int ppc_macosx_in_solib_return_trampoline (CORE_ADDR pc, char *name)
{
  return 0;
}

int ppc_macosx_in_solib_call_trampoline (CORE_ADDR pc, char *name)
{
  if (ppc_macosx_skip_trampoline_code (pc) != 0) { return 1; }
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
ppc_throw_catch_find_typeinfo (struct frame_info *curr_frame, int exception_type)
{
  struct minimal_symbol *typeinfo_sym;
  ULONGEST typeinfo_ptr;
  char *typeinfo_str;
  
  if (exception_type == EX_EVENT_THROW)
    {
      frame_unwind_unsigned_register (curr_frame, GP0_REGNUM + 4, &typeinfo_ptr);
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
      unsigned long type_obj_addr;

      frame_unwind_unsigned_register (curr_frame, GP0_REGNUM + 3, &typeinfo_ptr);
      
      /* This is also a bit bogus.  We assume that an unsigned integer is the 
	 same size as an address on our system.  */
      if (safe_read_memory_unsigned_integer (typeinfo_ptr - 48, 4, &type_obj_addr))
	typeinfo_sym = lookup_minimal_symbol_by_pc (type_obj_addr);
    }

  if (!typeinfo_sym)
    return NULL;

  typeinfo_str = typeinfo_sym->ginfo.language_specific.cplus_specific.demangled_name; 
  if ((typeinfo_str == NULL)
      || (strstr(typeinfo_str, "typeinfo for ") != typeinfo_str))
    return NULL;

  return typeinfo_str + strlen ("typeinfo for ");
}

static void
ppc_macosx_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  set_gdbarch_register_size (gdbarch, 4);
}

static void
ppc_macosx_init_abi_64 (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  set_gdbarch_register_size (gdbarch, 8);
}

static enum gdb_osabi
ppc_mach_o_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "mach-o-be") == 0
      || strcmp (bfd_get_target (abfd), "mach-o-le") == 0
      || strcmp (bfd_get_target (abfd), "mach-o-fat") == 0)
    return GDB_OSABI_DARWIN;

  return GDB_OSABI_UNKNOWN;
}

void
_initialize_ppc_tdep ()
{
  struct cmd_list_element *cmd = NULL;

  register_gdbarch_init (bfd_arch_powerpc, ppc_gdbarch_init);

  tm_print_insn = print_insn_big_powerpc;

  gdbarch_register_osabi_sniffer (bfd_arch_powerpc, bfd_target_mach_o_flavour,
				  ppc_mach_o_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_powerpc, 0, GDB_OSABI_DARWIN,
			  ppc_macosx_init_abi);

  gdbarch_register_osabi (bfd_arch_powerpc, 0, GDB_OSABI_DARWIN64,
			  ppc_macosx_init_abi_64);

  cmd = add_set_cmd ("ppc", class_obscure, var_boolean, 
		     (char *) &ppc_debugflag,
		     "Set if printing PPC stack analysis debugging statements.",
		     &setdebuglist),
  add_show_from_set (cmd, &showdebuglist);		

  cmd = add_set_cmd
    ("ppc-maximum-frame-size", class_obscure, var_uinteger,
     (char *) &ppc_max_frame_size,
     "Set the maximum size to expect for a valid PPC frame.",
     &setlist);
  add_show_from_set (cmd, &showlist);
}
