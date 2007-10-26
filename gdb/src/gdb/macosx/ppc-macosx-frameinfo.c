/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004
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
#include "ppc-macosx-regnums.h"
#include "ppc-macosx-frameinfo.h"
#include "ppc-macosx-tdep.h"

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "target.h"
#include "gdbcore.h"
#include "symfile.h"
#include "objfiles.h"
#include "command.h"
#include "regcache.h"
#include "ppc-tdep.h"

/* Limit the number of skipped non-prologue instructions, as the
   examining of the prologue is expensive.  The current use that
   refine_prologue_limit makes of this is to look for cases where
   the scheduler moves a body instruction before the FIRST instruction
   of the prologue.  For now, let's just lose then, since doing
   a whole bunch of pc_line calls just for this eventuality is
   not a win.  */

static int max_skip_non_prologue_insns = 2;

/* utility functions */

static inline long
extract_ds (unsigned long insn)
{
  return ((insn & 0xfffc) ^ 0x8000) - 0x8000;
}

static inline int
SIGNED_SHORT (long x)
{
  if (sizeof (short) == 2)
    {
      return ((short) x);
    }
  else
    {
      return (((x & 0xffff) ^ 0x8000) - 0x8000);
    }
}

static inline int
GET_SRC_REG (long x)
{
  return (x >> 21) & 0x1f;
}

void
ppc_print_boundaries (ppc_function_boundaries * bounds)
{
  if (bounds->prologue_start != INVALID_ADDRESS)
    {
      printf_filtered
        (" The function prologue begins at 0x%s.\n",
         paddr_nz (bounds->prologue_start));
    }
  if (bounds->body_start != INVALID_ADDRESS)
    {
      printf_filtered
        (" The function body begins at 0x%s.\n", paddr_nz (bounds->body_start));
    }
  if (bounds->epilogue_start != INVALID_ADDRESS)
    {
      printf_filtered
        (" The function epilogue begins at 0x%s.\n",
         paddr_nz (bounds->epilogue_start));
    }
  if (bounds->function_end != INVALID_ADDRESS)
    {
      printf_filtered
        (" The function ends at 0x%s.\n",
         paddr_nz (bounds->function_end));
    }
}

void
ppc_print_properties (ppc_function_properties * props)
{
  if (props->frameless)
    {
      printf_filtered (" No stack frame has been allocated.\n");
    }
  else
    {
      printf_filtered (" A stack frame has been allocated.\n");
    }
  if (props->frameptr_reg >= 0)
    {
      printf_filtered
        (" The stack pointer has been saved by alloca() in r%d.\n",
         props->frameptr_reg);
    }
  if (props->offset < 0)
    {
      printf_filtered (" No registers have been saved.\n");
    }
  else
    {
      if (props->offset >= 0)
        {
          printf_filtered
            (" %d bytes of integer and floating-point registers have been saved:\n",
             props->offset);
	  printf_filtered (" 0x%s is the stack setup address.\n", 
                           paddr_nz (props->stack_offset_pc));
        }
      if (props->saved_gpr >= 0)
        {
          printf_filtered
            (" General-purpose registers r%d--r%d have been saved at offset 0x%x.\n",
             props->saved_gpr, 31, props->gpr_offset);
        }
      else
        {
          printf_filtered
            (" No general-purpose registers have been saved.\n");
        }
      if (props->saved_fpr >= 0)
        {
          printf_filtered
            (" Floating-point registers r%d--r%d have been saved at offset 0x%x.\n",
             props->saved_fpr, 31, props->fpr_offset);
        }
      else
        {
          printf_filtered (" No floating-point registers have been saved.\n");
        }
    }
  if (props->lr_saved)
    {
      printf_filtered
        (" The link register has been saved at offset 0x%x.\n",
         props->lr_offset);
    }
  else
    {
      if (props->lr_invalid != 0)
        printf_filtered (" The link register is still valid.\n");
      else if (props->lr_reg > -1)
        printf_filtered (" The link register is stored in r%d.\n",
                         props->lr_reg);
      else
        printf_filtered
          (" I have no idea where the link register is stored.\n");
    }

  if (props->cr_saved)
    {
      printf_filtered
        (" The condition register has been saved at offset 0x%x.\n",
         props->cr_offset);
    }
}

/* Return $pc value after skipping a function prologue and also return
   information about a function frame. */

CORE_ADDR
ppc_parse_instructions (CORE_ADDR start, CORE_ADDR end,
                        ppc_function_properties * props)
{
  CORE_ADDR pc = start;
  CORE_ADDR last_recognized_insn = start;
  int unrecognized_insn_count = 0; /* We want to allow some unrecognized instructions, 
				      but we don't want to keep scanning forever. 
				      So this is the number of unrecognized instructions
				      before we bail from the prologue scanning. */
  int max_insn = 6;             /* If we don't recognize an instruction, keep going
                                   at least this long.  This is supposed to handle
                                   the case where instructions we don't recognize
                                   get inserted into the prologue. */
  int saw_pic_base_setup = 0;
  unsigned int lr_reg = 0xffffffff;     /* temporary cookies that we use to tell us that
                                           we have seen the lr moved into a gpr.
                                           * Set to 0xffffffff at start.
                                           * Set to the stw instruction for the register we
                                           see in the mflr.
                                           * Set to 0 when we see the lr get stored on the
                                           stack.  */
  unsigned int lr_64_reg = 0xffffffff;  /* temporary cookies that we use to tell us that
                                           we have seen the lr moved into a gpr.
                                           This version is for std - used for 64 bit PPC.
                                           * Set to 0xffffffff at start.
                                           * Set to the stw instruction for the register we
                                           see in the mflr.
                                           * Set to 0 when we see the lr get stored on the
                                           stack.  */
  unsigned int cr_reg = 0xffffffff;     /* Same as lr_reg but for condition reg. */
  int offset2 = 0;              /* This seems redundant to me, but I am not going
                                   to bother to take it out right now.  */

  CHECK_FATAL (props != NULL);
  ppc_clear_function_properties (props);

  CHECK_FATAL (start != INVALID_ADDRESS);
  /* instructions must be word-aligned */
  CHECK_FATAL ((start % 4) == 0);
  /* instructions must be word-aligned */
  CHECK_FATAL ((end == INVALID_ADDRESS) || (end % 4) == 0);
  CHECK_FATAL ((end >= start) || (end == INVALID_ADDRESS));

  for (pc = start; (end == INVALID_ADDRESS) || (pc < end);
       pc += 4)
    {
      ULONGEST op = 0;
      int insn_recognized = 1;

      if (!safe_read_memory_unsigned_integer (pc, 4, &op))
        {
          ppc_debug ("ppc_parse_instructions: Got an error reading at 0x%s",
                     paddr_nz (pc));
          /* We got an error reading the PC, so let's get out of here... */
          return last_recognized_insn;
        }

      /* This bcl is part of the sequence:

         mflr r0 (optional - only if leaf function)
         bcl .+4
         mflr r31
         mtlr r0 (do this if you stored it at the top)
       */
      if ((op & 0xfe000005) == 0x42000005)      /* bcl .+4 another relocatable
                                                   way to access global data */
        {
          props->lr_invalid = pc;
          saw_pic_base_setup = 1;
          props->pic_base_address = pc + 4;
          goto processed_insn;
        }
      /* This mr r31,r12 is part of an ObjC selector prologue like this:
         mflr    r0
         stmw    r30,-8(r1)
         mr      r31,r12     (the PIC base was in r12, put it in r31)
         But don't get tricked into using this expression if we've already
         seen a normal pic base mflr insn.
         Note:  By convention, the address of the start of the function is placed
         in R12 when calling an ObjC selector, so we stuff the START address we
         were given in to pic_base_address on the hope that START was actually
         the start of the function.
       */
      if (!saw_pic_base_setup && (op == 0x7d9f6378 || op == 0x7d9e6378))
        {
          saw_pic_base_setup = 1;
          props->pic_base_reg = (op & 0x1f0000) >> 16;
          props->pic_base_address = start;
          goto processed_insn;
        }
      /* Look at other branch instructions.  There are a couple of MacOS
         X Special purpose routines that are used in function prologues.
         These are:

         * saveWorld: used to be used in user code to set up info for C++
         exceptions, though now it is only used in throw itself.
         * saveFP: saves the FP registers AND the lr
         * saveVec: saves the AltiVec registers.
         If the bl is not one of these, we are probably not in a prologue,
         and we should get out...

       */
      else if ((op & 0xfc000003) == 0x48000001) /* bl <FN> */
        {
          /* Look up the pc you are branching to, and make
             sure it is save_world.  The instruction in the bl
             is bits 6-31, with the last two bits zeroed, sign extended and
             added to the pc. */

          struct minimal_symbol *msymbol;
          LONGEST branch_target;
          int recognized_fn_in_prolog = 0;

          /* If the link register is still valid, here is where it gets bad... */
          if (props->lr_invalid == 0)
            props->lr_invalid = pc;

          branch_target = (op & 0x03fffffc);

          if ((branch_target & 0x02000000) == 0x02000000)
            {
              /* Sign extend the address offset */
              int addrsize = 26;        /* The number of significant bits in the address */
              ULONGEST valmask;
              valmask = (((ULONGEST) 1) << addrsize) - 1;
              branch_target &= valmask;
              if (branch_target & (valmask ^ (valmask >> 1)))
                {
                  branch_target |= ~valmask;
                }
              /* branch_target |= 0xfc000000; */
            }

          branch_target += pc;

          msymbol = lookup_minimal_symbol_by_pc (branch_target);
          if (msymbol)
            {
              if (strcmp (SYMBOL_LINKAGE_NAME (msymbol), "save_world") == 0)
                {
                  /* save_world currently saves all the volatile registers,
                     and saves $lr & $cr on the stack in the usual place.
                     if gcc changes, this needs to be updated as well. */

		  unsigned int i;

                  props->frameless = 0;
                  props->frameptr_used = 0;

                  props->lr_saved = pc;
                  props->lr_offset = TARGET_PTR_BIT / 4;
                  lr_reg = 0;
                  lr_64_reg = 0;

                  props->cr_saved = 1;
                  props->cr_offset = 4;
                  cr_reg = 0;

                  props->saved_gpr = 13;
                  props->gpr_offset = -220;
		  for (i = 13; i < 32; i++)
		    props->gpr_bitmap[i] = 1;

                  props->saved_fpr = 14;
                  props->fpr_offset = -144;
		  for (i = 14; i < 32; i++)
		    props->fpr_bitmap[i] = 1;

                  recognized_fn_in_prolog = 1;
                }
              else if (strcmp (SYMBOL_LINKAGE_NAME (msymbol), "saveFP") == 0)
                {
                  ULONGEST store_insn = 0;
                  int reg;

                  /* Decode the actual branch target to find the
                     lowest register that is stored: */
                  if (!safe_read_memory_unsigned_integer (branch_target, 4,
                                                          &store_insn))
                    {
                      ppc_debug
                        ("ppc_parse_instructions: Got an error reading at 0x%s",
                         paddr_nz (pc));
                      /* We got an error reading the PC,
                         so let's get out of here... */
                      return pc;
                    }

                  reg = GET_SRC_REG (store_insn);
                  if ((props->saved_fpr == -1) || (props->saved_fpr > reg))
                    {
                      unsigned int i;
		      props->saved_fpr = reg;
		      for (i = reg; i < 32; i++)
			props->fpr_bitmap[i] = 1;
                      props->fpr_offset = SIGNED_SHORT (store_insn) + offset2;
                    }
                  /* The LR also gets saved in saveFP... */
                  props->lr_saved = pc;
                  props->lr_offset = TARGET_PTR_BIT / 4;
                  lr_reg = 0;
                  lr_64_reg = 0;

                  recognized_fn_in_prolog = 1;
                }
              else if (strcmp (SYMBOL_LINKAGE_NAME (msymbol), "saveVec") == 0)
                {
                  /* FIXME: We can't currently get the AltiVec
                     registers, but we need to save them away for
                     when we can. */

                  recognized_fn_in_prolog = 1;
                }
            }
          /* If we didn't recognize this function, we are probably not
             in the prologue, so let's get out of here... */
          if (!recognized_fn_in_prolog)
            break;
          else
            goto processed_insn;
        }
      /* mflr is used BOTH to store away the real link register,
         and afterwards in
         bcl .+4
         mflr r31
         to set the pic base for globals.  For now, we only care
         about the link register, and not the pic base business.
         The link register store will ALWAYS be to r0, so ignore
         it if it is another register.
       */
      else if ((op & 0xfc1fffff) == 0x7c0802a6) /* mflr Rx */
        {
          int target_reg = (op & 0x03e00000) >> 21;
          if (saw_pic_base_setup)
            {
              props->pic_base_reg = target_reg;
            }
          else
            {
              props->lr_reg = target_reg;
              lr_reg = (op & 0x03e00000) | 0x90010000;
              lr_64_reg = (op & 0x03e00000) | 0xf8010000;
            }

          goto processed_insn;
        }
      else if ((op & 0xfc1fffff) == 0x7c0803a6) /* mtlr Rx */
        {
          /* We see this case when we have moved the lr aside to
             set up the pic base, but have not saved it on the
             stack because we are a leaf function.  We are now
             moving it back to the lr, so the lr is again valid */

          if (!props->lr_saved && (props->lr_reg > -1))
            {
              props->lr_valid_again = pc;
            }
          goto processed_insn;
        }
      else if ((op & 0xfc1fffff) == 0x7c000026) /* mfcr Rx */
        {
          /* Ditto for the cr move, we always use r0 for temp store
             in the prologue.
           */

          int target_reg = (op & 0x03e000000) >> 21;
          if (target_reg == 0)
            {
              cr_reg = (op & 0x03e00000) | 0x90010000;
            }
          goto processed_insn;
        }
      else if ((op & 0xfc1f0000) == 0xd8010000) /* stfd Rx,NUM(r1) */
        {
          int reg = GET_SRC_REG (op);
	  props->fpr_bitmap[reg] = 1;
          if ((props->saved_fpr == -1) || (props->saved_fpr > reg))
            {
	      props->saved_fpr = reg;
              props->fpr_offset = SIGNED_SHORT (op) + offset2;
            }
          goto processed_insn;
        }
      else if ((op & 0xfc1f0000) == 0xbc010000)	/* stmw Rx, NUM(r1) */
        {
	  unsigned int reg = GET_SRC_REG (op);
          if ((props->saved_gpr == -1) || (props->saved_gpr > reg))
            {
              unsigned int i;
	      props->saved_gpr = reg;
              props->gpr_offset = SIGNED_SHORT (op) + offset2;
	      for (i = reg; i < 32; i++)
		props->gpr_bitmap[i] = 1;
	    }
	  goto processed_insn;
	}
      else if (
	       (((op & 0xfc1f0000) == 0x90010000)
		&& (op & 0x03e00000) >= 0x01a00000)
	       /* stw rx,NUM(r1), rx >= r13 */
               ||
	       (((op & 0xfc1f0000) == 0xf8010000
		 && (op & 0x03e00000) >= 0x01a00000))
	       /* std rx,NUM(r1), rx >= r13 */
	       )
	{
	  unsigned int reg = GET_SRC_REG (op);
	  props->gpr_bitmap[reg] = 1;
          if ((props->saved_gpr == -1) || (props->saved_gpr > reg))
            {
	      props->saved_gpr = reg;
              props->gpr_offset = SIGNED_SHORT (op) + offset2;
	    }
	  goto processed_insn;
	}
      /* If we saw a mflr, then we set lr_reg to the stw insn that would
         match the register mflr moved the lr to.  Otherwise it is 0 or
         -1 so it won't match this... */
      else if ((op & 0xffff0000) == lr_reg || (op & 0xffff0000) == lr_64_reg)
        {
          /* st Rx,NUM(r1) where Rx == lr */
          props->lr_saved = pc;
          props->lr_offset = SIGNED_SHORT (op) + offset2;
          lr_reg = 0;
          lr_64_reg = 0;
          goto processed_insn;
        }
      else if ((op & 0xffff0000) == cr_reg)
        {
          /* st Rx,NUM(r1) where Rx == cr */
          props->cr_saved = 1;
          props->cr_offset = SIGNED_SHORT (op) + offset2;
          cr_reg = 0;
          goto processed_insn;

        }
      /* This is moving the stack pointer to the top of the new stack
         when the frame is < 32K */
      else if ((op & 0xffff0000) == 0xf8210000)
        {                       /* stdu r1,NUM(r1) */
          props->frameless = 0;
          props->offset = extract_ds (op);
	  props->stack_offset_pc = pc;
          offset2 = props->offset;
          goto processed_insn;

        }
      /* This is moving the stack pointer to the top of the new stack
         when the frame is < 32K */
      else if ((op & 0xffff0000) == 0x94210000
               || (op & 0xffff0000) == 0xf8210000)
        {                       /* stwu r1,NUM(r1)  or stdu r1,NUM(r1) */
          props->frameless = 0;
          props->offset = SIGNED_SHORT (op);
	  props->stack_offset_pc = pc;
          offset2 = props->offset;
          goto processed_insn;

        }
      /* The next three instructions are used to set up the stack when
         the frame is >= 32K */
      else if ((op & 0xffff0000) == 0x3c000000)
        {
          /* addis 0,0,NUM, (i.e. lis r0, NUM) used for >= 32k frames */
          props->offset = (op & 0x0000ffff) << 16;
	  props->stack_offset_pc = pc;
          props->frameless = 0;
          goto processed_insn;

        }
      /* APPLE LOCAL fix-and-continue begin */
      else if (op == 0x60000000)
        {
          /* ori 0,0,0 aka NOP */
          /* Used in fix and continue padded prologues */
          /* Don't count the nop's against the instruction limit
             or we will have problems with any complex prologues
             in fix & continue. */
          goto processed_insn;
        }
      /* APPLE LOCAL fix-and-continue end */
      else if ((op & 0xffff0000) == 0x60000000)
        {
          /* ori 0,0,NUM, 2nd half of >= 32k frames */
          props->offset |= (op & 0x0000ffff);
	  props->stack_offset_pc = pc;
          props->frameless = 0;
          goto processed_insn;

        }
      else if (op == 0x7c21016e)
        {                       /* stwux 1,1,0 */
          props->frameless = 0;
          offset2 = props->offset;
	  props->stack_offset_pc = pc;
          goto processed_insn;
        }
      /* Gcc on MacOS X uses r30 for the frame pointer */
      else if (op == 0x7c3e0b78)        /* mr r30, r1 */
        {
          props->frameptr_used = 1;
          props->frameptr_reg = 30;
          props->frameptr_pc = pc;
          goto processed_insn;

        }
      /* store parameters in stack via frame pointer - Using r31 */
      else if ((props->frameptr_used && props->frameptr_reg == 30) && ((op & 0xfc1f0000) == 0x901e0000 ||       /* st rx,NUM(r1) */
                                                                       (op & 0xfc1f0000) == 0xd81e0000 ||       /* stfd Rx,NUM(r1) */
                                                                       (op & 0xfc1f0000) == 0xfc1e0000))        /* frsp, fp?,NUM(r1) */
        {
          goto processed_insn;

        }
      /* These ones are not used in Apple's gcc for function prologues
         so far as I can tell.  I am keeping them around since we support
         other compilers, but this way we can tell which bits we need
         to change as we change the ABI that our gcc uses... */

      else if (op == 0x48000005)        /* bl .+4 used in -mrelocatable */
        {
          goto processed_insn;
        }
      else if (op == 0x48000004)        /* b .+4 (xlc) */
        {
          break;
        }
      else if ((op & 0xf8000001) == 0x48000000) /* b, ba, bctr - we have already
                                                   found all the branches that
                                                   we are expecting in the
                                                   prologue, any others mean we
                                                   have wandered too far afield. */
        {
          ULONGEST peekahead_op = 0;
          if (props->lr_invalid == 1
              && (end != INVALID_ADDRESS || pc + 4 < end)
              && safe_read_memory_unsigned_integer (pc + 4, 4, &peekahead_op)
              && peekahead_op == 0x0)
            {
              /* We've hit a fix-and-continue trampoline sequence, which ends
                 in a .long 0x0 after a bctr.  Let's set up the default non-leaf
                 function values and go with that.  This will be incorrect for
                 a leaf function that doesn't store the caller's PC on the stack,
                 but the failure mode for that will be it skipping a frame in
                 a backtrace.  The failure mode if we do nothing is a duplicated
                 frame and an error without any further backtracing.  */
              props->lr_offset = 8;
              props->lr_saved = 1;
              props->lr_reg = PPC_MACOSX_LR_REGNUM;
              props->lr_invalid = 0;
              props->frameless = 0;
              return pc + 4;
            }
          break;
        }
      else if (op == 0x44000002)        /* sc - this is not expected in
                                           a prologue at all */
        {
          break;
        }
      /* load up minimal toc pointer */
      else if ((op >> 22) == 0x20f && !props->minimal_toc_loaded)       /* l r31,... or l r30,... */
        {
          props->minimal_toc_loaded = 1;
          goto processed_insn;

        }
      /* store parameters in stack */
      else if ((op & 0xfc1f0000) == 0x90010000 ||       /* st rx,NUM(r1) */
               (op & 0xfc1f0000) == 0xd8010000 ||       /* stfd Rx,NUM(r1) */
               (op & 0xfc1f0000) == 0xfc010000) /* frsp, fp?,NUM(r1) */
        {
          goto processed_insn;

        }
      /* store parameters in stack via frame pointer - Using r31 */
      else if ((props->frameptr_used && props->frameptr_reg == 31) && ((op & 0xfc1f0000) == 0x901f0000 ||       /* st rx,NUM(r1) */
                                                                       (op & 0xfc1f0000) == 0xd81f0000 ||       /* stfd Rx,NUM(r1) */
                                                                       (op & 0xfc1f0000) == 0xfc1f0000))        /* frsp, fp?,NUM(r1) */
        {
          goto processed_insn;

        }
      /* update stack pointer */
      else if (((op & 0xffff0000) == 0x801e0000
                /* lwz 0,NUM(r30), used in V.4 -mrelocatable */
                || op == 0x7fc0f214)
               /* add r30,r0,r30, used in V.4 -mrelocatable */
               && lr_reg == 0x901e0000)
        {
          goto processed_insn;
        }
      else if ((op & 0xffff0000) == 0x3fc00000
               /* addis 30,0,foo@ha, used in V.4 -mminimal-toc */
               || (op & 0xffff0000) == 0x3bde0000)      /* addi 30,30,foo@l */
        {
          goto processed_insn;

        }
      /* Set up frame pointer */
      else if (op == 0x603f0000 /* oril r31, r1, 0x0 */
               || op == 0x7c3f0b78)     /* mr r31, r1 */
        {
          props->frameptr_used = 1;
          props->frameptr_reg = 31;
          props->frameptr_pc = pc;
          goto processed_insn;

        }

      /* Not sure why this is here, but it shows up in the gcc3
         prologues */
      else if (op == 0x7d9f6378)        /* mr r31, r12 */
        {
          goto processed_insn;
        }

      /* Set up frame pointer (this time check for r30)
         Note - I moved the "mr r30, r1" check up to the Apple uses
         these section, just to keep this thing more managable...  */
      else if (op == 0x603e0000)        /* oril r30, r1, 0x0 */
        {
          props->frameptr_used = 1;
          props->frameptr_reg = 30;
          props->frameptr_pc = pc;
          goto processed_insn;

        }
      /* Another way to set up the frame pointer.  */
      else if ((op & 0xfc1fffff) == 0x38010000) /* addi rX, r1, 0x0 */
        {
          props->frameptr_used = 1;
          props->frameptr_reg = (op & ~0x38010000) >> 21;
          props->frameptr_pc = pc;
          goto processed_insn;

        }
      /* unknown instruction */
      else
        {
          insn_recognized = 0;
	  unrecognized_insn_count++;
          /* We have exceeded our maximum scan length.  So we are going to
             exit the parse.  However, there may be cases where what we have
             learned so far leads us to believe that there are interesting
             bits of information that we should find in the instruction stream.
             We can make the scan more accurate even when we couldn't completely
             ingest the prologue by looking in a focused way for these bits.  */

          if (unrecognized_insn_count > max_insn)
            {
              break;
            }
        }
    processed_insn:
      if (insn_recognized)
        last_recognized_insn = pc;

    }

  /* This is kind of a hail mary section of the scanner.  Sometimes we
     stop scanning the prologue because we exceeded the maximum number
     of unrecognized instructions, or because we got to the end of the
     pc range but we aren't really done with the prologue.  There
     might still be some easy data we could recover if look a little
     further.  This does that but just for a couple of important
     things that aren't likely to show up in regular code.  
     Note, we don't always trust end, because that comes from trying to
     look at line number ranges for more patches coming from the first
     line of the function.  But we don't look very far for this 'cause it
     is expensive.  It is simpler to do a fast scan here that search the
     line tables.  */

  if (unrecognized_insn_count > max_insn || pc >= end) 
    { 
      int cleanup_length = 6;
      
      if ((!props->lr_saved
	   && props->lr_reg != 0xffffffff
	   && props->lr_invalid != 0
	   && props->lr_valid_again == INVALID_ADDRESS)
	  || (props->saved_gpr != -1 && props->offset == -1))
	{
	  /* We saw the link register made invalid, but we
	     also didn't see it saved, or we saw GPR registers saved
	     but didn't see the stack updated.  Let's try scanning forward
	     to see when it gets saved or moved back to the lr,
	     otherwise we will think the return address is always stored
	     in some register, and end up adding a garbage address to
	     the stack.  */
	  
	  for (; cleanup_length > 0; pc += 4, cleanup_length--)
	    {
	      ULONGEST op = 0;
	      
	      if (!safe_read_memory_unsigned_integer (pc, 4, &op))
		break;
	      
	      if ((op & 0xfc1fffff) == 0x7c0803a6)      /* mtlr Rx */
		{
		  props->lr_valid_again = pc;
		  last_recognized_insn = pc;
		}
	      else if ((op & 0xffff0000) == 0x94210000
		       || (op & 0xffff0000) == 0xf8210000)
		{                       /* stwu r1,NUM(r1)  or stdu r1,NUM(r1) */
		  props->frameless = 0;
		  props->stack_offset_pc = pc;
		  props->offset = SIGNED_SHORT (op);
		  last_recognized_insn = pc;
		}
	    }
	}
    }

  if (props->offset != -1)
    {
      props->offset = -props->offset;
    }

  /* We are to return the first instruction of the body, so up the
     last_recognized_insn by one.  However, if we are still at start
     we didn't actually recognize any instructions, so don't increment
     in that case.  */

  if (last_recognized_insn == start)
    return start;
  else
    return last_recognized_insn + 4;
}

void
ppc_clear_function_boundaries_request (ppc_function_boundaries_request *
                                       request)
{
  request->min_start = INVALID_ADDRESS;
  request->max_end = INVALID_ADDRESS;

  request->contains_pc = INVALID_ADDRESS;

  request->prologue_start = INVALID_ADDRESS;
  request->body_start = INVALID_ADDRESS;
  request->epilogue_start = INVALID_ADDRESS;
  request->function_end = INVALID_ADDRESS;
}

void
ppc_clear_function_boundaries (ppc_function_boundaries * boundaries)
{
  boundaries->prologue_start = INVALID_ADDRESS;
  boundaries->body_start = INVALID_ADDRESS;
  boundaries->epilogue_start = INVALID_ADDRESS;
  boundaries->function_end = INVALID_ADDRESS;
}

void
ppc_clear_function_properties (ppc_function_properties * properties)
{
  unsigned int i;

  for (i = 0; i < 32; i++)
    {
      properties->gpr_bitmap[i] = 0;
      properties->fpr_bitmap[i] = 0;
    }

  properties->offset = -1;
  properties->stack_offset_pc = INVALID_ADDRESS;

  properties->saved_gpr = -1;
  properties->saved_fpr = -1;
  properties->gpr_offset = 0;
  properties->fpr_offset = 0;

  properties->frameptr_reg = -1;
  properties->frameptr_used = 0;
  properties->frameptr_pc = INVALID_ADDRESS;

  properties->frameless = 1;

  properties->lr_saved = 0;
  properties->lr_offset = -1;
  properties->lr_reg = -1;
  properties->lr_invalid = 0;
  properties->lr_valid_again = INVALID_ADDRESS;

  properties->cr_saved = 0;
  properties->cr_offset = -1;

  properties->minimal_toc_loaded = 0;
  properties->pic_base_reg = 0;
  properties->pic_base_address = INVALID_ADDRESS;
}

int
ppc_find_function_boundaries (ppc_function_boundaries_request *request,
                              ppc_function_boundaries *reply)
{
  ppc_function_properties props;
  CORE_ADDR lim_pc;

  CHECK_FATAL (request != NULL);
  CHECK_FATAL (reply != NULL);

  if (request->prologue_start != INVALID_ADDRESS)
    {
      reply->prologue_start = request->prologue_start;
    }
  else if (request->contains_pc != INVALID_ADDRESS)
    {
      reply->prologue_start = get_pc_function_start (request->contains_pc);
      if (reply->prologue_start == 0)
        return -1;
    }

  CHECK_FATAL (reply->prologue_start != INVALID_ADDRESS);

  if ((reply->prologue_start % 4) != 0)
    return -1;

  /* Use the line number information to limit the prologue search if
     it is available */

  lim_pc = refine_prologue_limit
    (reply->prologue_start, 0, max_skip_non_prologue_insns);

  if (lim_pc != 0)
    reply->body_start = lim_pc;
  else
    lim_pc = INVALID_ADDRESS;

  reply->body_start = ppc_parse_instructions
    (reply->prologue_start, lim_pc, &props);

  return 0;
}

/* Put here the code to store, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.  */

CORE_ADDR *
ppc_frame_saved_regs (struct frame_info *next_frame, void **this_cache)
{
  struct ppc_frame_cache *cache;
  ppc_function_properties *props;
  CORE_ADDR *saved_regs;
  CORE_ADDR prev_sp, this_pc, this_func;
  int i;

  cache = ppc_frame_cache (next_frame, this_cache);

  if (cache->saved_regs_valid)
    return cache->saved_regs;

  saved_regs = cache->saved_regs;

  props = ppc_frame_function_properties (next_frame, this_cache);
  if (props == NULL)
    return NULL;

  prev_sp = ppc_frame_find_prev_sp (next_frame, this_cache);

  if (props->cr_saved)
    {
      saved_regs[PPC_MACOSX_CR_REGNUM] = prev_sp + props->cr_offset;
    }

  if (props->lr_saved)
    {
      saved_regs[PPC_MACOSX_LR_REGNUM] = prev_sp + props->lr_offset;
    }

  if (props->frameless
      && ((props->saved_fpr != -1) || (props->saved_gpr != -1)))
    {
      ppc_debug ("frame_find_saved_regs: "
                 "registers marked as saved in frameless function; ignoring\n");
      return saved_regs;
    }

  if (props->saved_fpr >= 0)
    {
      for (i = props->saved_fpr; i < 32; i++)
        {
          int fpr = PPC_MACOSX_FIRST_FP_REGNUM + i;
	  if (props->fpr_bitmap[i])
	    {
	      long offset = props->fpr_offset +
                        ((i - props->saved_fpr) * register_size 
                          (current_gdbarch, PPC_MACOSX_FIRST_FP_REGNUM));
	      saved_regs[fpr] = prev_sp + offset;
	    }
        }
    }

  if (props->saved_gpr >= 0)
    {
      for (i = props->saved_gpr; i < 32; i++)
        {
          int gpr = PPC_MACOSX_FIRST_GP_REGNUM + i;
          int wordsize = (gdbarch_tdep (current_gdbarch))->wordsize;
          /* Need to use the wordsize here rather than the register
             size since the G5 always has 8 byte registers, but 32
             bit apps only store into the lower wordsize.  */
	  if (props->gpr_bitmap[i])
	    {
	      long offset =
		props->gpr_offset + ((i - props->saved_gpr) * wordsize);
	      saved_regs[gpr] = prev_sp + offset;
	    }
        }
    }

  /* The frame pointer is a really important register to get correct.
     If we haven't updated the frame pointer register in 'this' func
     yet, then let's assume that the register still contains the caller's
     correct frame pointer register.  Mark the location in saved_regs as
     -1 so the current reg value will be returned as the prev function's
     reg value.  */

  this_pc = frame_pc_unwind (next_frame);
  this_func = frame_func_unwind (next_frame);
  if (props->frameptr_used && props->frameptr_reg > 0
     && props->frameptr_pc != INVALID_ADDRESS
     && this_pc >= this_func
     && this_pc <= props->frameptr_pc)
    {
      saved_regs[props->frameptr_reg] = -1;
    }

  cache->saved_regs_valid = 1;

  return saved_regs;
}

/* ppc_frame_cache_properties -
   This function creates the ppc_function_properties
   bit of the extra_info structure of FRAME, if it is not already
   present.  If RETPROPS is not NULL, it copies the properties
   to retprops (provided by caller).

   Returns: 0 on success */

struct ppc_function_properties *
ppc_frame_function_properties (struct frame_info *next_frame,
                               void **this_cache)
{
  struct ppc_frame_cache *cache = ppc_frame_cache (next_frame, this_cache);
  struct ppc_function_properties *props = &cache->properties;

  ppc_function_boundaries *bounds;
  ppc_function_boundaries lbounds;

  struct frame_info *this_frame = get_prev_frame (next_frame);

  ppc_clear_function_properties (props);

  bounds = ppc_frame_function_boundaries (next_frame, this_cache);
  if (bounds != NULL)
    lbounds = *bounds;
  else
    {
      /* BOUNDS can be NULL if there is no minsym for the address and 
         THIS_FRAME can be NULL if NEXT_FRAME is inside the main function. 
	 We need to be sure to not call get_frame_pc() using a NULL frame.  */
      lbounds.prologue_start = this_frame ? get_frame_pc (this_frame) : 
					    INVALID_ADDRESS;
      lbounds.body_start = INVALID_ADDRESS;
      lbounds.epilogue_start = INVALID_ADDRESS;
      lbounds.function_end = INVALID_ADDRESS;
    }

  if ((lbounds.prologue_start % 4) != 0)
    {
      if (frame_relative_level (this_frame) == 1)
        {
          props->lr_saved = 0;
          props->lr_offset = 0;
          return props;
        }
      else
        {
          props->lr_offset = 8;
          props->lr_saved = 1;
          props->lr_reg = PPC_MACOSX_LR_REGNUM;
          props->lr_invalid = 0;
          props->frameless = 0;
          ppc_debug ("ppc_frame_cache_properties: %s\n",
                     "Couldn't find function boundaries/parse fn prologue;"
                     "overriding and fetching $lr from link area");
          return props;
        }
    }

  /* By the time we get here, ppc_frame_cache_boundaries will
     have already called ppc_parse_instructions to find the
     body start.  If that is true, use the body start it found
     to limit the search. */

  ppc_parse_instructions (lbounds.prologue_start, lbounds.body_start, props);

#if 0
  /* If a frameless function is interrupted by a signal, no change to
     the stack pointer.  */
  if (get_next_frame (frame) != NULL
      && (get_frame_type (get_next_frame (frame)) == SIGTRAMP_FRAME)
      && ppc_frameless_function_invocation (frame))
    {
      return sp;
    }
#endif

  /* ppc_parse_instructions sometimes gets the stored pc location
     wrong.  However, we know that for all frames but frame 0
     the pc is on the stack.  So force that here. */

  if ((frame_relative_level (next_frame) >= 0)
      && (get_frame_type (next_frame) != DUMMY_FRAME)
      && (get_frame_type (next_frame) != SIGTRAMP_FRAME))
    {
      /* This is a bogus value, but we need to tell it some value
         before the current $pc for the rest of the code to
         work properly. */
      props->lr_offset = TARGET_PTR_BIT / 4;
      props->lr_saved = 1;
      props->lr_reg = PPC_MACOSX_LR_REGNUM;
      props->lr_invalid = 0;
      props->frameless = 0;
      ppc_debug ("ppc_frame_cache_properties: %s\n",
                 "a non-leaf frame appeared not to save $lr;"
                 "overriding and fetching from link area");
    }

  return props;
}

struct ppc_function_boundaries *
ppc_frame_function_boundaries (struct frame_info *next_frame,
                               void **this_cache)
{
  struct ppc_frame_cache *cache = ppc_frame_cache (next_frame, this_cache);
  struct ppc_function_boundaries *bounds = &cache->boundaries;

  ppc_function_boundaries_request request;
  int ret;

  if (cache->boundaries_status == 1)
    return bounds;
  /* If the boundaries_status is -1 it means we didn't get the boundaries
     right.  Don't try again, since that is no more likely to work now
     than it did last time.  But don't return the bounds or our callers
     might think they are good...  */
  else if (cache->boundaries_status == -1)
    return NULL;

  ppc_clear_function_boundaries (bounds);
  ppc_clear_function_boundaries_request (&request);

  request.contains_pc = frame_unwind_address_in_block (next_frame);

  if (request.contains_pc == INVALID_ADDRESS)
    {
      cache->boundaries_status = -1;
      return NULL;
    }

  ret = ppc_find_function_boundaries (&request, bounds);
  if (ret != 0)
    {
      cache->boundaries_status = -1;
      return NULL;
    }

  cache->boundaries_status = 1;
  return bounds;
}
