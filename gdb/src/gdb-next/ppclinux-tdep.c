/* Target-dependent code for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994, 1995
   Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "target.h"
#include "language.h"
#include "gdbcore.h"
#include "symfile.h"
#include "objfiles.h"

#include <sys/types.h>
#include <sys/param.h>
#include <signal.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/procfs.h>
#include <link.h>
#include <elf.h>
#include <asm/ptrace.h>		/* for struct pt_regs */
#include <asm/sigcontext.h>	/* for struct sigcontext_struct */

#define PT_REGS_PTR_OFFSET \
	(STACK_FRAME_OVERHEAD + offsetof(struct sigcontext_struct, regs))

extern struct obstack frame_cache_obstack;

/* Static function prototypes */

static void frame_get_cache_fsr PARAMS ((struct frame_info *fi,
					 struct rs6000_framedata *fdatap));


/* Determine whether or not instruction at prevpc was a subroutine
   branch and if so whether or not stoppc is the subroutine branch
   target. */

int
at_subroutine_call_instruction_target(prevpc,stoppc)
     CORE_ADDR prevpc;
     CORE_ADDR stoppc;
{
  int instr;
  int opcode, ext_op, lk;

  instr = read_memory_unsigned_integer (prevpc, 4);

  opcode = (instr >> 26) & 0x1f;
  ext_op = (instr >> 1) & 0x3ff;
  lk = instr & 1;

  /* All the following does is check to make sure that we were on
     a branch instruction and that stoppc is not immediately after
     the instruction which we were just at.  We are not really checking
     the branch target, but that isn't really necessary for this
     subroutine to work. I guess this could be a possible FIXME. */

  return ((prevpc+4 != stoppc)
          && lk == 1
	  && (opcode == 18
	      || opcode == 16
	      || (opcode == 19 && (ext_op == 16 || ext_op == 528))));
}


/* return pc value after skipping a function prologue and also return
   information about a function frame.

   in struct rs6000_frameinfo fdata:
    - frameless is TRUE, if function does not have a frame.
    - nosavedpc is TRUE, if function does not save %pc value in its frame.
    - offset is the number of bytes used in the frame to save registers.
    - saved_gpr is the number of the first saved gpr.
    - saved_fpr is the number of the first saved fpr.
    - alloca_reg is the number of the register used for alloca() handling.
      Otherwise -1.
    - gpr_offset is the offset of the saved gprs
    - fpr_offset is the offset of the saved fprs
    - lr_offset is the offset of the saved lr
    - cr_offset is the offset of the saved cr
 */

#define SIGNED_SHORT(x) 						\
  ((sizeof (short) == 2)						\
   ? ((int)(short)(x))							\
   : ((int)((((x) & 0xffff) ^ 0x8000) - 0x8000)))

#define GET_SRC_REG(x) (((x) >> 21) & 0x1f)

CORE_ADDR
skip_prologue (pc, fdata)
     CORE_ADDR pc;
     struct rs6000_framedata *fdata; 
{
  CORE_ADDR orig_pc = pc;
  char buf[4];
  unsigned long op;
  long offset = 0;
  int lr_reg = 0;
  int cr_reg = 0;
  int reg;
  int framep = 0;
  int minimal_toc_loaded = 0;
  static struct rs6000_framedata zero_frame;

  *fdata = zero_frame;
  fdata->saved_gpr = -1;
  fdata->saved_fpr = -1;
  fdata->alloca_reg = -1;
  fdata->frameless = 1;
  fdata->nosavedpc = 1;

  if (target_read_memory (pc, buf, 4))
    return pc;			/* Can't access it -- assume no prologue. */

  /* Assume that subsequent fetches can fail with low probability.  */
  pc -= 4;
  for (;;)
    {
      pc += 4;
      op = read_memory_unsigned_integer (pc, 4);

      if ((op & 0xfc1fffff) == 0x7c0802a6) {		/* mflr Rx */
	lr_reg = (op & 0x03e00000) | 0x90010000;
	continue;

      } else if ((op & 0xfc1fffff) == 0x7c000026) {	/* mfcr Rx */
	cr_reg = (op & 0x03e00000) | 0x90010000;
	continue;

      } else if ((op & 0xfc1f0000) == 0xd8010000) {	/* stfd Rx,NUM(r1) */
	reg = GET_SRC_REG (op);
	if (fdata->saved_fpr == -1 || fdata->saved_fpr > reg) {
	  fdata->saved_fpr = reg;
	  fdata->fpr_offset = SIGNED_SHORT (op) + offset;
	}
	continue;

      } else if (((op & 0xfc1f0000) == 0xbc010000) || 	/* stm Rx, NUM(r1) */
		 ((op & 0xfc1f0000) == 0x90010000 &&	/* st rx,NUM(r1), rx >= r13 */
		  (op & 0x03e00000) >= 0x01a00000)) {

	reg = GET_SRC_REG (op);
	if (fdata->saved_gpr == -1 || fdata->saved_gpr > reg) {
	  fdata->saved_gpr = reg;
	  fdata->gpr_offset = SIGNED_SHORT (op) + offset;
	}
	continue;

      } else if ((op & 0xffff0000) == 0x3c000000) {	/* addis 0,0,NUM, used for >= 32k frames */
	fdata->offset = (op & 0x0000ffff) << 16;
	fdata->frameless = 0;
	continue;

      } else if ((op & 0xffff0000) == 0x60000000) {	/* ori 0,0,NUM, 2nd half of >= 32k frames */
	fdata->offset |= (op & 0x0000ffff);
	fdata->frameless = 0;
	continue;

      } else if (lr_reg && (op & 0xffff0000) == lr_reg) { /* st Rx,NUM(r1) where Rx == lr */
	fdata->lr_offset = SIGNED_SHORT (op) + offset;
	fdata->nosavedpc = 0;
	lr_reg = 0;
	continue;

      } else if (cr_reg && (op & 0xffff0000) == cr_reg) { /* st Rx,NUM(r1) where Rx == cr */
	fdata->cr_offset = SIGNED_SHORT (op) + offset;
	cr_reg = 0;
	continue;

      } else if (op == 0x48000005) {			/* bl .+4 used in -mrelocatable */
	continue;

      } else if (op == 0x48000004) {			/* b .+4 (xlc) */
	break;

      } else if (((op & 0xffff0000) == 0x801e0000 ||	/* lwz 0,NUM(r30), used in V.4 -mrelocatable */
		  op == 0x7fc0f214) &&			/* add r30,r0,r30, used in V.4 -mrelocatable */
		 lr_reg == 0x901e0000) {
	continue;

      } else if ((op & 0xffff0000) == 0x3fc00000 ||	/* addis 30,0,foo@ha, used in V.4 -mminimal-toc */
		 (op & 0xffff0000) == 0x3bde0000) {	/* addi 30,30,foo@l */
	continue;

      } else if ((op & 0xfc000000) == 0x48000000	/* bl foo, to get the GOT */
              && read_memory_unsigned_integer(pc+(((((long) op)<<6)>>6) & ~3), 4) 
	                                 == 0x4e800021 /* blrl */ 
              && (read_memory_unsigned_integer(pc+4,4) & 0xfc1fffff) == 0x7c0802a6 /* mflr */) {
	pc += 4;	/* skip the mflr instruction */
	continue;

      } else if ((op & 0xfc000000) == 0x48000000) {	/* bl foo, to save fprs??? */

	fdata->frameless = 0;
	/* Don't skip over the subroutine call if it is not within the first
	   three instructions of the prologue.  */
	if ((pc - orig_pc) > 8)
	  break;

	op = read_memory_unsigned_integer (pc+4, 4);

	/* At this point, make sure this is not a trampoline function
	   (a function that simply calls another functions, and nothing else).
	   If the next is not a nop, this branch was part of the function
	   prologue. */

	if (op == 0x4def7b82 || op == 0)		/* crorc 15, 15, 15 */
	  break;					/* don't skip over this branch */

	continue;

      /* update stack pointer */
      } else if ((op & 0xffff0000) == 0x94210000) {	/* stu r1,NUM(r1) */
	fdata->frameless = 0;
	fdata->offset = SIGNED_SHORT (op);
	offset = fdata->offset;
	continue;

      } else if (op == 0x7c21016e) {			/* stwux 1,1,0 */
	fdata->frameless = 0;
	offset = fdata->offset;
	continue;

      /* Load up minimal toc pointer */
      } else if ((op >> 22) == 0x20f
	         && ! minimal_toc_loaded) {	/* l r31,... or l r30,... */
	minimal_toc_loaded = 1;
	continue;

      /* store parameters in stack */
      } else if ((op & 0xfc1f0000) == 0x90010000 ||	/* st rx,NUM(r1) */
		 (op & 0xfc1f0000) == 0xd8010000 ||	/* stfd Rx,NUM(r1) */
		 (op & 0xfc1f0000) == 0xfc010000) {	/* frsp, fp?,NUM(r1) */
	continue;

      /* store parameters in stack via frame pointer */
      } else if (framep &&
		 (op & 0xfc1f0000) == 0x901f0000 ||	/* st rx,NUM(r1) */
		 (op & 0xfc1f0000) == 0xd81f0000 ||	/* stfd Rx,NUM(r1) */
		 (op & 0xfc1f0000) == 0xfc1f0000) {	/* frsp, fp?,NUM(r1) */
	continue;

      /* Set up frame pointer */
      } else if (op == 0x603f0000			/* oril r31, r1, 0x0 */
		 || op == 0x7c3f0b78) {			/* mr r31, r1 */
	fdata->frameless = 0;
	framep = 1;
	fdata->alloca_reg = 31;
	continue;

      /* Another way to set up the frame pointer.  */
      } else if ((op & 0xfc1fffff) == 0x38010000) {	/* addi rX, r1, 0x0 */
	fdata->frameless = 0;
	framep = 1;
	fdata->alloca_reg = (op & ~0x38010000) >> 21;
	continue;

      } else {
	break;
      }
    }

  fdata->offset = - fdata->offset;
  return pc;
}


/*************************************************************************
  Support for creating pushind a dummy frame into the stack, and popping
  frames, etc. 
*************************************************************************/

/* The total size of dummy frame is 436, which is;

	32 gpr's	- 128 bytes
	32 fpr's	- 256   "
	7  the rest	- 28    "
	and 24 extra bytes for the callee's link area. The last 24 bytes
	for the link area might not be necessary, since it will be taken
	care of by push_arguments(). */

#define DUMMY_FRAME_SIZE 436

#define	DUMMY_FRAME_ADDR_SIZE 10

/* Make sure you initialize these in somewhere, in case gdb gives up what it
   was debugging and starts debugging something else. FIXMEibm */

static int dummy_frame_count = 0;
static int dummy_frame_size = 0;
static CORE_ADDR *dummy_frame_addr = 0;

extern int stop_stack_dummy;

/* push a dummy frame into stack, save all register. Currently we are saving
   only gpr's and fpr's, which is not good enough! FIXMEmgo */
   
void
push_dummy_frame ()
{
  /* stack pointer.  */
  CORE_ADDR sp;
  /* Same thing, target byte order.  */
  char sp_targ[4];

  /* link register.  */
  CORE_ADDR pc;
  /* Same thing, target byte order.  */
  char pc_targ[4];
  
  /* Needed to figure out where to save the dummy link area.
     FIXME: There should be an easier way to do this, no?  tiemann 9/9/95.  */
  struct rs6000_framedata fdata;

  int ii;

  target_fetch_registers (-1);

  if (dummy_frame_count >= dummy_frame_size) {
    dummy_frame_size += DUMMY_FRAME_ADDR_SIZE;
    if (dummy_frame_addr)
      dummy_frame_addr = (CORE_ADDR*) xrealloc 
        (dummy_frame_addr, sizeof(CORE_ADDR) * (dummy_frame_size));
    else
      dummy_frame_addr = (CORE_ADDR*) 
	xmalloc (sizeof(CORE_ADDR) * (dummy_frame_size));
  }
  
  sp = read_register(SP_REGNUM);
  pc = read_register(PC_REGNUM);
  store_address (pc_targ, 4, pc);

  (void) skip_prologue (get_pc_function_start (pc) + FUNCTION_START_OFFSET, &fdata);

  dummy_frame_addr [dummy_frame_count++] = sp;

  /* Be careful! If the stack pointer is not decremented first, then kernel 
     thinks he is free to use the space underneath it. And kernel actually 
     uses that area for IPC purposes when executing ptrace(2) calls. So 
     before writing register values into the new frame, decrement and update
     %sp first in order to secure your frame. */

  /* FIXME: We don't check if the stack really has this much space.
     This is a problem on the ppc simulator (which only grants one page
     (4096 bytes) by default.  */

  write_register (SP_REGNUM, sp-DUMMY_FRAME_SIZE);

  /* gdb relies on the state of current_frame. We'd better update it,
     otherwise things like do_registers_info() wouldn't work properly! */

  flush_cached_frames ();

  /* save program counter in link register's space. */
  write_memory (sp + (fdata.lr_offset ? fdata.lr_offset : DEFAULT_LR_SAVE),
	        pc_targ, 4);

  /* save all floating point and general purpose registers here. */

  /* fpr's, f0..f31 */
  for (ii = 0; ii < 32; ++ii)
    write_memory (sp-8-(ii*8), &registers[REGISTER_BYTE (31-ii+FP0_REGNUM)], 8);

  /* gpr's r0..r31 */
  for (ii=1; ii <=32; ++ii)
    write_memory (sp-256-(ii*4), &registers[REGISTER_BYTE (32-ii)], 4);

  /* so far, 32*2 + 32 words = 384 bytes have been written. 
     7 extra registers in our register set: pc, ps, cnd, lr, cnt, xer, mq */

  for (ii=1; ii <= (LAST_SP_REGNUM-FIRST_SP_REGNUM+1); ++ii) {
    write_memory (sp-384-(ii*4), 
	       &registers[REGISTER_BYTE (FPLAST_REGNUM + ii)], 4);
  }

  /* Save sp or so called back chain right here. */
  store_address (sp_targ, 4, sp);
  write_memory (sp-DUMMY_FRAME_SIZE, sp_targ, 4);
  sp -= DUMMY_FRAME_SIZE;

  /* And finally, this is the back chain. */
  write_memory (sp+8, pc_targ, 4);
}


/* Pop a dummy frame.

   In rs6000 when we push a dummy frame, we save all of the registers. This
   is usually done before user calls a function explicitly.

   After a dummy frame is pushed, some instructions are copied into stack,
   and stack pointer is decremented even more.  Since we don't have a frame
   pointer to get back to the parent frame of the dummy, we start having
   trouble poping it.  Therefore, we keep a dummy frame stack, keeping
   addresses of dummy frames as such.  When poping happens and when we
   detect that was a dummy frame, we pop it back to its parent by using
   dummy frame stack (`dummy_frame_addr' array). 

FIXME:  This whole concept is broken.  You should be able to detect
a dummy stack frame *on the user's stack itself*.  When you do,
then you know the format of that stack frame -- including its
saved SP register!  There should *not* be a separate stack in the
GDB process that keeps track of these dummy frames!  -- gnu@cygnus.com Aug92
 */
   
pop_dummy_frame ()
{
  CORE_ADDR sp, pc;
  int ii;
  sp = dummy_frame_addr [--dummy_frame_count];

  /* restore all fpr's. */
  for (ii = 1; ii <= 32; ++ii)
    read_memory (sp-(ii*8), &registers[REGISTER_BYTE (32-ii+FP0_REGNUM)], 8);

  /* restore all gpr's */
  for (ii=1; ii <= 32; ++ii) {
    read_memory (sp-256-(ii*4), &registers[REGISTER_BYTE (32-ii)], 4);
  }

  /* restore the rest of the registers. */
  for (ii=1; ii <=(LAST_SP_REGNUM-FIRST_SP_REGNUM+1); ++ii)
    read_memory (sp-384-(ii*4),
    		&registers[REGISTER_BYTE (FPLAST_REGNUM + ii)], 4);

  read_memory (sp-(DUMMY_FRAME_SIZE-8), 
	       &registers [REGISTER_BYTE(PC_REGNUM)], 4);

  /* when a dummy frame was being pushed, we had to decrement %sp first, in 
     order to secure astack space. Thus, saved %sp (or %r1) value, is not the
     one we should restore. Change it with the one we need. */

  *(int*)&registers [REGISTER_BYTE(FP_REGNUM)] = sp;

  /* Now we can restore all registers. */

  target_store_registers (-1);
  pc = read_pc ();
  flush_cached_frames ();
}


/* pop the innermost frame, go back to the caller. */

void
pop_frame ()
{
  CORE_ADDR pc, lr, sp, prev_sp;		/* %pc, %lr, %sp */
  struct rs6000_framedata fdata;
  struct frame_info *frame = get_current_frame ();
  int addr, ii;

  pc = read_pc ();
  sp = FRAME_FP (frame);

  if (stop_stack_dummy && dummy_frame_count) {
    pop_dummy_frame ();
    return;
  }

  /* Make sure that all registers are valid.  */
  read_register_bytes (0, NULL, REGISTER_BYTES);

  /* figure out previous %pc value. If the function is frameless, it is 
     still in the link register, otherwise walk the frames and retrieve the
     saved %pc value in the previous frame. */

  addr = get_pc_function_start (frame->pc) + FUNCTION_START_OFFSET;
  (void) skip_prologue (addr, &fdata);

  if (fdata.frameless)
    prev_sp = sp;
  else
    prev_sp = read_memory_unsigned_integer (sp, 4);
  if (fdata.lr_offset == 0)
    lr = read_register (LR_REGNUM);
  else
    lr = read_memory_unsigned_integer (prev_sp + fdata.lr_offset, 4);

  /* reset %pc value. */
  write_register (PC_REGNUM, lr);

  /* reset register values if any was saved earlier. */
  addr = prev_sp - fdata.offset;

  if (fdata.saved_gpr != -1)
    for (ii = fdata.saved_gpr; ii <= 31; ++ii) {
      read_memory (addr, &registers [REGISTER_BYTE (ii)], 4);
      addr += 4;
    }

  if (fdata.saved_fpr != -1)
    for (ii = fdata.saved_fpr; ii <= 31; ++ii) {
      read_memory (addr, &registers [REGISTER_BYTE (ii+FP0_REGNUM)], 8);
      addr += 8;
  }

  write_register (SP_REGNUM, prev_sp);
  target_store_registers (-1);
  flush_cached_frames ();
}

/* fixup the call sequence of a dummy function, with the real function address.
   its argumets will be passed by gdb. */

void
fix_call_dummy (dummyname, pc, fun, nargs, type)
     char *dummyname;
     CORE_ADDR pc;
     CORE_ADDR fun;
     int nargs;					/* not used */
     int type;					/* not used */
{
#define	TOC_ADDR_OFFSET		20
#define	TARGET_ADDR_OFFSET	28

  int ii;
  CORE_ADDR target_addr;
  CORE_ADDR tocvalue = 0;

  target_addr = fun;
#if 0
  tocvalue = find_toc_address (target_addr);
#endif

  ii  = *(int*)((char*)dummyname + TOC_ADDR_OFFSET);
  ii = (ii & 0xffff0000) | (tocvalue >> 16);
  *(int*)((char*)dummyname + TOC_ADDR_OFFSET) = ii;

  ii  = *(int*)((char*)dummyname + TOC_ADDR_OFFSET+4);
  ii = (ii & 0xffff0000) | (tocvalue & 0x0000ffff);
  *(int*)((char*)dummyname + TOC_ADDR_OFFSET+4) = ii;

  ii  = *(int*)((char*)dummyname + TARGET_ADDR_OFFSET);
  ii = (ii & 0xffff0000) | (target_addr >> 16);
  *(int*)((char*)dummyname + TARGET_ADDR_OFFSET) = ii;

  ii  = *(int*)((char*)dummyname + TARGET_ADDR_OFFSET+4);
  ii = (ii & 0xffff0000) | (target_addr & 0x0000ffff);
  *(int*)((char*)dummyname + TARGET_ADDR_OFFSET+4) = ii;
}

/* Pass the arguments in either registers, or in the stack. In RS6000,
   the first eight words of the argument list (that might be less than
   eight parameters if some parameters occupy more than one word) are
   passed in r3..r11 registers.  float and double parameters are
   passed in fpr's, in addition to that. Rest of the parameters if any
   are passed in user stack. There might be cases in which half of the
   parameter is copied into registers, the other half is pushed into
   stack.

   If the function is returning a structure, then the return address is passed
   in r3, then the first 7 words of the parametes can be passed in registers,
   starting from r4. */

CORE_ADDR
push_arguments (nargs, args, sp, struct_return, struct_addr)
     int nargs;
     value_ptr *args;
     CORE_ADDR sp;
     int struct_return;
     CORE_ADDR struct_addr;
{
  int ii, len;
  int argno;					/* current argument number */
  int argbytes;					/* current argument byte */
  char tmp_buffer [50];
  int f_argno = 0;				/* current floating point argno */
  value_ptr arg;
  struct type *type;

  CORE_ADDR saved_sp, pc;

  if ( dummy_frame_count <= 0)
    printf_unfiltered ("FATAL ERROR -push_arguments()! frame not found!!\n");

  /* The first eight words of ther arguments are passed in registers. Copy
     them appropriately.

     If the function is returning a `struct', then the first word (which 
     will be passed in r3) is used for struct return address. In that
     case we should advance one word and start from r4 register to copy 
     parameters. */

  ii =  struct_return ? 1 : 0;

  for (argno=0, argbytes=0; argno < nargs && ii<8; ++ii) {

    arg = args[argno];
    type = check_typedef (VALUE_TYPE (arg));
    len = TYPE_LENGTH (type);

    if (TYPE_CODE (type) == TYPE_CODE_FLT) {

      /* floating point arguments are passed in fpr's, as well as gpr's.
         There are 13 fpr's reserved for passing parameters. At this point
         there is no way we would run out of them. */

      if (len > 8)
        printf_unfiltered (
"Fatal Error: a floating point parameter #%d with a size > 8 is found!\n", argno);

      memcpy (&registers[REGISTER_BYTE(FP0_REGNUM + 1 + f_argno)], VALUE_CONTENTS (arg), 
         len);
      ++f_argno;
    }

    if (len > 4) {

      /* Argument takes more than one register. */
      while (argbytes < len) {

	*(int*)&registers[REGISTER_BYTE(ii+3)] = 0;
	memcpy (&registers[REGISTER_BYTE(ii+3)], 
			 ((char*)VALUE_CONTENTS (arg))+argbytes, 
			(len - argbytes) > 4 ? 4 : len - argbytes);
	++ii, argbytes += 4;

	if (ii >= 8)
	  goto ran_out_of_registers_for_arguments;
      }
      argbytes = 0;
      --ii;
    }
    else {        /* Argument can fit in one register. No problem. */
      *(int*)&registers[REGISTER_BYTE(ii+3)] = 0;
      memcpy (&registers[REGISTER_BYTE(ii+3)], VALUE_CONTENTS (arg), len);
    }
    ++argno;
  }

ran_out_of_registers_for_arguments:

  /* location for 8 parameters are always reserved. */
  sp -= 4 * 8;

  /* another six words for back chain, TOC register, link register, etc. */
  sp -= 24;

  /* if there are more arguments, allocate space for them in 
     the stack, then push them starting from the ninth one. */

  if ((argno < nargs) || argbytes) {
    int space = 0, jj;

    if (argbytes) {
      space += ((len - argbytes + 3) & -4);
      jj = argno + 1;
    }
    else
      jj = argno;

    for (; jj < nargs; ++jj) {
      value_ptr val = args[jj];
      space += ((TYPE_LENGTH (VALUE_TYPE (val))) + 3) & -4;
    }

    /* add location required for the rest of the parameters */
    space = (space + 7) & -8;
    sp -= space;

    /* This is another instance we need to be concerned about securing our
	stack space. If we write anything underneath %sp (r1), we might conflict
	with the kernel who thinks he is free to use this area. So, update %sp
	first before doing anything else. */

    write_register (SP_REGNUM, sp);

    /* if the last argument copied into the registers didn't fit there 
       completely, push the rest of it into stack. */

    if (argbytes) {
      write_memory (
        sp+24+(ii*4), ((char*)VALUE_CONTENTS (arg))+argbytes, len - argbytes);
      ++argno;
      ii += ((len - argbytes + 3) & -4) / 4;
    }

    /* push the rest of the arguments into stack. */
    for (; argno < nargs; ++argno) {

      arg = args[argno];
      type = check_typedef (VALUE_TYPE (arg));
      len = TYPE_LENGTH (type);


      /* float types should be passed in fpr's, as well as in the stack. */
      if (TYPE_CODE (type) == TYPE_CODE_FLT && f_argno < 13) {

        if (len > 8)
          printf_unfiltered (
"Fatal Error: a floating point parameter #%d with a size > 8 is found!\n", argno);

        memcpy (&registers[REGISTER_BYTE(FP0_REGNUM + 1 + f_argno)], VALUE_CONTENTS (arg), 
           len);
        ++f_argno;
      }

      write_memory (sp+24+(ii*4), (char *) VALUE_CONTENTS (arg), len);
      ii += ((len + 3) & -4) / 4;
    }
  }
  else
    /* Secure stack areas first, before doing anything else. */
    write_register (SP_REGNUM, sp);

  saved_sp = dummy_frame_addr [dummy_frame_count - 1];
  read_memory (saved_sp, tmp_buffer, 24);
  write_memory (sp, tmp_buffer, 24);

  /* set back chain properly */
  store_address (tmp_buffer, 4, saved_sp);
  write_memory (sp, tmp_buffer, 4);

  target_store_registers (-1);
  return sp;
}

/* a given return value in `regbuf' with a type `valtype', extract and copy its
   value into `valbuf' */

void
extract_return_value (valtype, regbuf, valbuf)
     struct type *valtype;
     char regbuf[REGISTER_BYTES];
     char *valbuf;
{
  int offset = 0;

  if (TYPE_CODE (valtype) == TYPE_CODE_FLT) {

    double dd; float ff;
    /* floats and doubles are returned in fpr1. fpr's have a size of 8 bytes.
       We need to truncate the return value into float size (4 byte) if
       necessary. */

    if (TYPE_LENGTH (valtype) > 4) 		/* this is a double */
      memcpy (valbuf, &regbuf[REGISTER_BYTE (FP0_REGNUM + 1)],
						TYPE_LENGTH (valtype));
    else {		/* float */
      memcpy (&dd, &regbuf[REGISTER_BYTE (FP0_REGNUM + 1)], 8);
      ff = (float)dd;
      memcpy (valbuf, &ff, sizeof(float));
    }
  }
  else {
    /* return value is copied starting from r3. */
    if (TARGET_BYTE_ORDER == BIG_ENDIAN
	&& TYPE_LENGTH (valtype) < REGISTER_RAW_SIZE (3))
      offset = REGISTER_RAW_SIZE (3) - TYPE_LENGTH (valtype);

    memcpy (valbuf, regbuf + REGISTER_BYTE (3) + offset,
	    TYPE_LENGTH (valtype));
  }
}


/* keep structure return address in this variable.
   FIXME:  This is a horrid kludge which should not be allowed to continue
   living.  This only allows a single nested call to a structure-returning
   function.  Come on, guys!  -- gnu@cygnus.com, Aug 92  */

CORE_ADDR rs6000_struct_return_address;

/* Determines whether the function FI has a frame on the stack or not.  */

int
frameless_function_invocation (fi)
     struct frame_info *fi;
{
  CORE_ADDR func_start;
  struct rs6000_framedata fdata;

  if (fi->next != NULL && !fi->next->signal_handler_caller)
    /* Don't even think about framelessness except on the innermost frame.
       or in a frame previous to a signal handler caller */
    return 0;
  
  func_start = get_pc_function_start (fi->pc) + FUNCTION_START_OFFSET;

  /* If we failed to find the start of the function, it is a mistake
     to inspect the instructions. */

  if (!func_start)
    return 0;

  (void) skip_prologue (func_start, &fdata);
  return fdata.frameless;
}

/* Return the PC saved in a frame */

unsigned long
frame_saved_pc (fi)
     struct frame_info *fi;
{
  CORE_ADDR func_start;
  struct rs6000_framedata fdata;
  int frameless;

  if (fi->signal_handler_caller) 
    {
      CORE_ADDR pt_regs_addr = read_memory_unsigned_integer (fi->frame + PT_REGS_PTR_OFFSET, 4);
      /* return the NIP in the pt_regs structure */
      return read_memory_unsigned_integer(pt_regs_addr + offsetof(struct pt_regs, nip), 4);
    }

  func_start = get_pc_function_start (fi->pc) + FUNCTION_START_OFFSET;

  /* If we failed to find the start of the function, it is a mistake
     to inspect the instructions. */
  if (!func_start)
    return 0;

  (void) skip_prologue (func_start, &fdata);

  if (fdata.lr_offset == 0 && fi->next != NULL)
    return read_memory_unsigned_integer (rs6000_frame_chain (fi) + DEFAULT_LR_SAVE, 4);

  if (fdata.lr_offset == 0)
    return read_register (LR_REGNUM);

  return read_memory_unsigned_integer (rs6000_frame_chain (fi) + fdata.lr_offset, 4);
}

/* If saved registers of frame FI are not known yet, read and cache them.
   &FDATAP contains rs6000_framedata; TDATAP can be NULL,
   in which case the framedata are read.  */

static void
frame_get_cache_fsr (fi, fdatap)
     struct frame_info *fi;
     struct rs6000_framedata *fdatap;
{
  int ii;
  CORE_ADDR frame_addr; 
  struct rs6000_framedata work_fdata;

  if (fi->cache_fsr)
    return;
  
  if (fdatap == NULL) {
    fdatap = &work_fdata;
    (void) skip_prologue (get_pc_function_start (fi->pc), fdatap);
  }

  fi->cache_fsr = (struct frame_saved_regs *)
      obstack_alloc (&frame_cache_obstack, sizeof (struct frame_saved_regs));
  memset (fi->cache_fsr, '\0', sizeof (struct frame_saved_regs));

  if (fi->prev && fi->prev->frame)
    frame_addr = fi->prev->frame;
  else
    frame_addr = read_memory_unsigned_integer (fi->frame, 4);
  
  /* if != -1, fdatap->saved_fpr is the smallest number of saved_fpr.
     All fpr's from saved_fpr to fp31 are saved.  */

  if (fdatap->saved_fpr >= 0) {
    int fpr_offset = frame_addr + fdatap->fpr_offset;
    for (ii = fdatap->saved_fpr; ii < 32; ii++) {
      fi->cache_fsr->regs [FP0_REGNUM + ii] = fpr_offset;
      fpr_offset += 8;
    }
  }

  /* if != -1, fdatap->saved_gpr is the smallest number of saved_gpr.
     All gpr's from saved_gpr to gpr31 are saved.  */
  
  if (fdatap->saved_gpr >= 0) {
    int gpr_offset = frame_addr + fdatap->gpr_offset;
    for (ii = fdatap->saved_gpr; ii < 32; ii++) {
      fi->cache_fsr->regs [ii] = gpr_offset;
      gpr_offset += 4;
    }
  }

  /* If != 0, fdatap->cr_offset is the offset from the frame that holds
     the CR.  */
  if (fdatap->cr_offset != 0)
    fi->cache_fsr->regs [CR_REGNUM] = frame_addr + fdatap->cr_offset;

  /* If != 0, fdatap->lr_offset is the offset from the frame that holds
     the LR.  */
  if (fdatap->lr_offset != 0)
    fi->cache_fsr->regs [LR_REGNUM] = frame_addr + fdatap->lr_offset;
}

/* Return the address of a frame. This is the inital %sp value when the frame
   was first allocated. For functions calling alloca(), it might be saved in
   an alloca register. */

CORE_ADDR
frame_initial_stack_address (fi)
     struct frame_info *fi;
{
  CORE_ADDR tmpaddr;
  struct rs6000_framedata fdata;
  struct frame_info *callee_fi;

  /* if the initial stack pointer (frame address) of this frame is known,
     just return it. */

  if (fi->initial_sp)
    return fi->initial_sp;

  /* If we're in a signal handler caller, fi->frame is fine */
  if (fi->signal_handler_caller) {
    fi->initial_sp = fi->frame;
    return fi->initial_sp;
  }

  /* find out if this function is using an alloca register.. */

  (void) skip_prologue (get_pc_function_start (fi->pc), &fdata);

  /* if saved registers of this frame are not known yet, read and cache them. */

  if (!fi->cache_fsr)
    frame_get_cache_fsr (fi, &fdata);

  /* If no alloca register used, then fi->frame is the value of the %sp for
     this frame, and it is good enough. */

  if (fdata.alloca_reg < 0) {
    fi->initial_sp = fi->frame;
    return fi->initial_sp;
  }

  /* This function has an alloca register. If this is the top-most frame
     (with the lowest address), the value in alloca register is good. */

  if (!fi->next)
    return fi->initial_sp = read_register (fdata.alloca_reg);     

  /* Otherwise, this is a caller frame. Callee has usually already saved
     registers, but there are exceptions (such as when the callee
     has no parameters). Find the address in which caller's alloca
     register is saved. */

  for (callee_fi = fi->next; callee_fi; callee_fi = callee_fi->next) {

    if (!callee_fi->cache_fsr)
      frame_get_cache_fsr (callee_fi, NULL);

    /* this is the address in which alloca register is saved. */

    tmpaddr = callee_fi->cache_fsr->regs [fdata.alloca_reg];
    if (tmpaddr) {
      fi->initial_sp = read_memory_unsigned_integer (tmpaddr, 4); 
      return fi->initial_sp;
    }

    /* Go look into deeper levels of the frame chain to see if any one of
       the callees has saved alloca register. */
  }

  /* If alloca register was not saved, by the callee (or any of its callees)
     then the value in the register is still good. */

  return fi->initial_sp = read_register (fdata.alloca_reg);     
}


CORE_ADDR
rs6000_frame_chain (thisframe)
     struct frame_info *thisframe;
{
  CORE_ADDR fp;
  if (inside_entry_file ((thisframe)->pc))
    return 0;
  if (thisframe->signal_handler_caller) 
    {
      CORE_ADDR pt_regs_addr;
      pt_regs_addr = read_memory_unsigned_integer (thisframe->frame + PT_REGS_PTR_OFFSET, 4);
      /* fetch address of saved r1 from the pt_regs structure */
      fp = read_memory_unsigned_integer(pt_regs_addr + offsetof(struct pt_regs, gpr[1]), 4);
    }
  else
    fp = read_memory_unsigned_integer ((thisframe)->frame, 4);

  return fp;
}

int
gdb_print_insn_powerpc (memaddr, info)
     bfd_vma memaddr;
     disassemble_info *info;
{
  if (TARGET_BYTE_ORDER == BIG_ENDIAN)
    return print_insn_big_powerpc (memaddr, info);
  else
    return print_insn_little_powerpc (memaddr, info);
}

void
init_extra_frame_info (fromleaf, fi)
    int fromleaf;
    struct frame_info *fi;
{
    fi->initial_sp = 0;
    fi->cache_fsr  = 0;
    if (fi->next != 0) {
	/* We're called from get_prev_frame_info; check to see if
	   this is a signal frame by looking to see if the pc points
	   at trampoline code */
	char buf[8];
	if (target_read_memory(fi->pc, buf, sizeof(buf)) != 0)
	    return;
	if (   extract_unsigned_integer(buf,4)   == 0x38007777	/* li r0, 0x7777 */
	    || extract_unsigned_integer(buf+4,4) == 0x44000002  /* sc */ ) {
	    fi->signal_handler_caller = 1;
	}
    }
}

/* Almost of of the following code with the exception of the signal handling
   stuff was swiped from tm-rs6000.h. */

void
frame_find_saved_regs(struct frame_info *fi, struct frame_saved_regs *fsr)
{
  int ii;
  CORE_ADDR frame_addr, func_start;
  struct rs6000_framedata fdata;

  if (fi->signal_handler_caller) {
    CORE_ADDR pt_regs_addr = read_memory_unsigned_integer (fi->frame + PT_REGS_PTR_OFFSET, 4);
    memset (fsr, '\0', sizeof (*fsr));
    fsr->regs[PC_REGNUM]  = pt_regs_addr + offsetof(struct pt_regs, nip);
    fsr->regs[PS_REGNUM]  = pt_regs_addr + offsetof(struct pt_regs, msr);
    fsr->regs[CR_REGNUM]  = pt_regs_addr + offsetof(struct pt_regs, ccr);
    fsr->regs[LR_REGNUM]  = pt_regs_addr + offsetof(struct pt_regs, link);
    fsr->regs[CTR_REGNUM] = pt_regs_addr + offsetof(struct pt_regs, ctr);
    fsr->regs[XER_REGNUM] = pt_regs_addr + offsetof(struct pt_regs, xer);
#if 0
    fsr->regs[MQ_REGNUM]  = pt_regs_addr + offsetof(struct pt_regs, mq);
#endif
    for (ii=0; ii<32; ii++) {
      fsr->regs[GP0_REGNUM+ii] = pt_regs_addr + offsetof(struct pt_regs, gpr[0]) + 4*ii;
    }
#if 0  /* FIXME: enable this once mklinux header files get more up to date */
    for (ii=0; ii<4; ii++) {	/* only four fpu regs saved in struct pt_regs */
      fsr->regs[FP0_REGNUM+ii] = pt_regs_addr + offsetof(struct pt_regs, fpr[0]) + 4*ii;
    }
#endif
    return;
  }

  /* find the start of the function and collect info about its frame. */

  func_start = get_pc_function_start (fi->pc) + FUNCTION_START_OFFSET;
  (void) skip_prologue (func_start, &fdata);
  memset (fsr, '\0', sizeof (*fsr));

  /* if there were any saved registers, figure out parent's stack pointer. */
  /* the following is true only if the frame doesn't have a call to alloca(),
      FIXME. */
  if (fdata.saved_fpr == 0 && fdata.saved_gpr == 0 &&
      fdata.lr_offset == 0 && fdata.cr_offset == 0) {
    frame_addr = 0;

  } else if (fi->prev && fi->prev->frame) {
    frame_addr = fi->prev->frame;

  } else {
    frame_addr = read_memory_unsigned_integer (fi->frame, 4);
  }

  /* if != -1, fdata.saved_fpr is the smallest number of saved_fpr. All
     fpr's from saved_fpr to f31 are saved. */
  if (fdata.saved_fpr >= 0) {
    int fpr_offset = frame_addr + fdata.fpr_offset;
    for (ii = fdata.saved_fpr; ii < 32; ii++) {
      fsr->regs [FP0_REGNUM + ii] = fpr_offset;
      fpr_offset += 8;
    }
  }

  /* if != -1, fdata.saved_gpr is the smallest number of saved_gpr. All
     gpr's from saved_gpr to r31 are saved. */
  if (fdata.saved_gpr >= 0) {
    int gpr_offset = frame_addr + fdata.gpr_offset;
    for (ii = fdata.saved_gpr; ii < 32; ii++) {
      fsr->regs [ii] = gpr_offset;
      gpr_offset += 4;
    }
  }

  /* If != 0, fdata.cr_offset is the offset from the frame that holds
     the CR */
  if (fdata.cr_offset != 0) {
    fsr->regs [CR_REGNUM] = frame_addr + fdata.cr_offset;
  }

  /* If != 0, fdata.cr_offset is the offset from the frame that holds
     the LR */
  if (fdata.lr_offset != 0) {
    fsr->regs [LR_REGNUM] = frame_addr + fdata.lr_offset;
  }
}

void
init_frame_pc_first(int fromleaf, struct frame_info *fi)
{
    if (fromleaf) {
	if (   fi->next 
	    && fi->next->next 
	    && fi->next->next->signal_handler_caller) {
	    /* next next frame is a signal handler caller...
	       This is a bit confusing, so an explanation is in order.
	       fi is the frame set we are determining pc for.  fi->next
	       is the frameless callee of fi.  And fi->next->next is the
	       frame of the signal trampoline code. */
	    CORE_ADDR pt_regs_addr = 
		read_memory_unsigned_integer (fi->next->next->frame + PT_REGS_PTR_OFFSET, 4);
            fi->pc = read_memory_unsigned_integer(pt_regs_addr + offsetof(struct pt_regs, link), 4);
	}
	else {
	    /* normal leaf case; frame is at the top */
	    fi->pc = SAVED_PC_AFTER_CALL(fi->next);
	}
    }
    else {
	if (fi->next) {
	    /* not top-most frame */
	    fi->pc = FRAME_SAVED_PC(fi->next);
	}
	else {
	    /* top-most frame; just fetch current pc value */
	    fi->pc = read_pc();
	}
    }
}

CORE_ADDR
skip_trampoline_code (CORE_ADDR pc)
{
    char buf[4];
    struct obj_section *sect;
    struct objfile *objfile;
    unsigned long insn;
    CORE_ADDR plt_start = 0;
    CORE_ADDR symtab    = 0;
    CORE_ADDR strtab    = 0;
    int num_slots       = -1;
    int reloc_index	= -1;
    CORE_ADDR plt_table;
    CORE_ADDR reloc;
    CORE_ADDR sym;
    Elf32_Word symidx;
    char symname[1024];
    struct minimal_symbol *msymbol;

    /* Find the section pc is in; return if not in .plt */
    sect = find_pc_section(pc);
    if (!sect || strcmp(sect->the_bfd_section->name, ".plt") != 0)
	return 0;

    objfile = sect->objfile;

    /* Pick up the instruction at pc.  It had better be of the
       form
       		li r11, IDX

       where IDX is an index into the plt_table.  */

    if (target_read_memory(pc, buf, 4) != 0)
	return 0;
    insn = extract_unsigned_integer(buf, 4);
    
    if ( (insn & 0xffff0000) != 0x39600000 /* li r11, VAL */ )
	return 0;

    reloc_index  = (insn << 16) >> 16;

    /* Find the objfile that pc is in and obtain the information
       necessary for finding the symbol name. */
    for (sect = objfile->sections; sect < objfile->sections_end; ++sect) {
	const char *secname = sect->the_bfd_section->name;
	if (strcmp(secname, ".plt") == 0) {
	    plt_start = sect->addr;
	}
	else if (strcmp(secname, ".rela.plt") == 0) {
	    num_slots = ((int) sect->endaddr - (int) sect->addr) / 12;
	}
	else if (strcmp(secname, ".dynsym") == 0) {
	    symtab = sect->addr;
	}
	else if (strcmp(secname, ".dynstr") == 0) {
	    strtab = sect->addr;
	}
    }

    /* Make sure we have all the information we need. */
    if (plt_start == 0 || num_slots == -1 || symtab == 0 || strtab == 0)
	return 0;

    /* Compute the value of the plt table */
    plt_table = plt_start + 72 + 8*num_slots;

    /* Get address of the relocation entry (Elf32_Rela) */
    if (target_read_memory(plt_table + reloc_index, buf, 4) != 0)
	return 0;
    reloc = extract_address(buf, 4);

    sect = find_pc_section(reloc);
    if (!sect)
	return 0;

    if (strcmp(sect->the_bfd_section->name, ".text") == 0) {
	return reloc;
    }

    /* Now get the r_info field which is the relocation type and symbol
       index. */
    if (target_read_memory(reloc+4, buf, 4) != 0)
	return 0;
    symidx = extract_unsigned_integer(buf, 4);

    /* Shift out the relocation type leaving just the symbol index */
    symidx = ELF32_R_SYM(symidx);

    /* compute the address of the symbol */
    sym = symtab + symidx * sizeof(Elf32_Sym);

    /* Fetch the string table index */
    if (target_read_memory(sym, buf, 4) != 0)
	return 0;
    symidx = extract_unsigned_integer(buf, 4);

    /* Fetch the string; we don't know how long it is.  Is it possible
       that the following will fail because we're trying to fetch too
       much? */
    if (target_read_memory(strtab+symidx, symname, sizeof(symname)) != 0)
	return 0;

    /* This might not work right if we have multiple symbols with the
       same name; the only way to really get it right is to perform
       the same sort of lookup as the dynamic linker. */
    msymbol = lookup_minimal_symbol_text(symname, NULL, NULL);
    if (!msymbol)
	return 0;

    return SYMBOL_VALUE_ADDRESS (msymbol);
}

void
_initialize_ppclinux_tdep ()
{
  tm_print_insn = gdb_print_insn_powerpc;
}
