#include <sys/wait.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <mach/mach.h>

#include "macosx-low.h"

#include "arm-regnums.h"
#include "arm-macosx-regnums.h"
#include "arm-macosx-thread-status.h"

int
arm_mach_o_query_v6 ()
{
  host_basic_info_data_t info;
  mach_msg_type_number_t count;

  count = HOST_BASIC_INFO_COUNT;
  host_info (mach_host_self (), HOST_BASIC_INFO, (host_info_t) & info,
             &count);

  return (info.cpu_type == CPU_TYPE_ARM &&
          info.cpu_subtype == CPU_SUBTYPE_ARM_V6);
}

unsigned long long
extract_unsigned_integer (const void *addr, int len)
{
  unsigned long long retval;
  const unsigned char *p;
  const unsigned char *startaddr = addr;
  const unsigned char *endaddr = startaddr + len;

  /* Start at the most significant end of the integer, and work towards
     the least significant.  */
  retval = 0;
  for (p = endaddr - 1; p >= startaddr; --p)
    retval = (retval << 8) | *p;
  return retval;
}


/* This is roughly cribbed from ppc-maocsx-regs.c.  We don't have the
   gdbarch stuff going in gdbserver, however.  So we can't just use it
   exactly...  */
static inline void
collect_unsigned_int (int regnum, unsigned int *addr)
{
  char buf[4];
  collect_register (regnum, buf);
  *addr = extract_unsigned_integer (buf, 4);
}

void
store_unsigned_integer (void *addr, int len, unsigned long long val)
{
  unsigned char *p;
  unsigned char *startaddr = (unsigned char *) addr;
  unsigned char *endaddr = startaddr + len;
  
  /* Start at the least significant end of the integer, and work towards
     the most significant.  */
  for (p = startaddr; p < endaddr; ++p)
    {
      *p = val & 0xff;
      val >>= 8;
    }
}

static inline void
supply_unsigned_int (int regnum, unsigned long long val)
{
  char buf[4] = { 0 };
  store_unsigned_integer (buf, 4, val);
  supply_register (regnum, buf);
}

void
arm_macosx_store_vfp_registers (struct gdb_arm_thread_fpstate *fp_regs)
{
  int i;
  uint32_t *r = fp_regs->r;
  for (i = 0; i < ARM_MACOSX_NUM_VFP_REGS; i++)
    collect_unsigned_int (ARM_FIRST_VFP_REGNUM + i, &r[i]);
  collect_unsigned_int (ARM_FPSCR_REGNUM, &fp_regs->fpscr);
}

void
arm_macosx_fetch_vfp_registers (struct gdb_arm_thread_fpstate *fp_regs)
{
  int i;
  uint32_t *r = fp_regs->r;
  for (i = 0; i < ARM_MACOSX_NUM_VFP_REGS; i++)
    supply_unsigned_int (ARM_FIRST_VFP_REGNUM + i, r[i]);
  supply_unsigned_int (ARM_FPSCR_REGNUM, fp_regs->fpscr);
}

void
arm_macosx_store_gp_registers (struct gdb_arm_thread_state *gp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_GP_REGS; i++)
    collect_unsigned_int (ARM_R0_REGNUM + i, &gp_regs->r[i]);
  collect_unsigned_int (ARM_PS_REGNUM, &gp_regs->cpsr);
}

void
arm_macosx_fetch_gp_registers (struct gdb_arm_thread_state *gp_regs)
{
  int i;
  for (i = 0; i < ARM_MACOSX_NUM_GP_REGS; i++)
    supply_unsigned_int (ARM_R0_REGNUM + i, gp_regs->r[i]);
  supply_unsigned_int (ARM_PS_REGNUM, gp_regs->cpsr);
}

/* Read register values from the inferior process.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
arm_fetch_inferior_registers (int regno)
{
  int i;
  thread_t current_thread = ((struct inferior_list_entry *) current_inferior)->id;
  /* gdbserver will ask for all registers through gdbserver/regcache.c 
     by passing zero as the value for REGNO. We also check for the standard
     gdb method of passing -1 for REGNO.  */
  int get_all = (regno == -1) || (regno == 0);
  if (get_all || ARM_MACOSX_IS_GP_RELATED_REGNUM (regno))
    {
      struct gdb_arm_thread_state gp_regs;
      unsigned int gp_count = GDB_ARM_THREAD_STATE_COUNT;
      kern_return_t ret = thread_get_state
        (current_thread, GDB_ARM_THREAD_STATE, (thread_state_t) & gp_regs,
         &gp_count);
      if (ret != KERN_SUCCESS)
       {
         warning ("Error calling thread_get_state for GP registers for thread 0x%4.4x", 
		  current_thread);
         MACH_CHECK_ERROR (ret);
       }
      arm_macosx_fetch_gp_registers (&gp_regs);
    }

  if (get_all || ARM_MACOSX_IS_FP_RELATED_REGNUM (regno))
    {
      /* We don't have F0-F7, though they need to exist in our register
         numbering scheme so we can connect to remote gdbserver's that use
	 FSF register numbers.  */
      char buf[FP_REGISTER_RAW_SIZE] = { 0 };
      for (i = ARM_F0_REGNUM; i <= ARM_F7_REGNUM; i++)
	supply_register (i, buf);
      supply_register (ARM_FPS_REGNUM, buf);
    }

  if ((get_all || ARM_MACOSX_IS_VFP_RELATED_REGNUM (regno))
       && arm_mach_o_query_v6 ())
    {
      struct gdb_arm_thread_fpstate fp_regs;
      unsigned int fp_count = GDB_ARM_THREAD_FPSTATE_COUNT;
      kern_return_t ret;
      ret = thread_get_state (current_thread, GDB_ARM_THREAD_FPSTATE,
                              (thread_state_t) & fp_regs,
                              &fp_count);
      if (ret != KERN_SUCCESS)
       {
         warning ("Error calling thread_get_state for VFP registers for thread 0x%4.4x", 
		  current_thread);
         MACH_CHECK_ERROR (ret);
       }
      arm_macosx_fetch_vfp_registers (&fp_regs);
    }
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
arm_store_inferior_registers (int regno)
{
  thread_t current_thread = ((struct inferior_list_entry *) current_inferior)->id;

  if ((regno == -1) || ARM_MACOSX_IS_GP_RELATED_REGNUM (regno))
    {
      struct gdb_arm_thread_state gp_regs;
      kern_return_t ret;
      arm_macosx_store_gp_registers (&gp_regs);
      ret = thread_set_state (current_thread, GDB_ARM_THREAD_STATE,
                              (thread_state_t) & gp_regs,
                              GDB_ARM_THREAD_STATE_COUNT);
      MACH_CHECK_ERROR (ret);
    }

  if (((regno == -1) || ARM_MACOSX_IS_VFP_RELATED_REGNUM (regno))
       && arm_mach_o_query_v6 ())
    {
      struct gdb_arm_thread_fpstate fp_regs;
      kern_return_t ret;
      arm_macosx_store_vfp_registers (&fp_regs);
      ret = thread_set_state (current_thread, GDB_ARM_THREAD_FPSTATE,
                              (thread_state_t) & fp_regs,
                              GDB_ARM_THREAD_FPSTATE_COUNT);
      MACH_CHECK_ERROR (ret);
    }
}

/* The following "read_" functions are just so I can 
   copy arm_get_next_pc from arm-tdep.c with minimal
   changes.  */

static unsigned long
read_memory_integer (unsigned long addr, int size)
{
  char buf[32];

  if (size > 32)
    error ("Called read_memory_integer with size: %d.", size);

  read_inferior_memory (addr, buf, size);
  return extract_unsigned_integer (buf, size); 
}

static unsigned long
read_register (int regno)
{
  unsigned int retval;
  collect_unsigned_int (regno, &retval);
  return retval;
}

/* FIXME: This is wrong for now.  */

/* Addresses for calling Thumb functions have the bit 0 set and if
   bit 1 is set, it has to be thumb since it isn't a mutliple of four.
   Here are some macros to test, set, or clear bit 0 of addresses.  */
#define IS_THUMB_ADDR(addr)	((addr) & 3)

int
arm_pc_is_thumb (unsigned long pc)
{
  /* The only place this function is currently used is for determining
     if an address has the bit 0 set when calling arm_get_next_pc() and
     thumb_get_next_pc(), so just look at bit 0 and bit 1 to be safe.  */
  if (IS_THUMB_ADDR (pc))
    return 1;

  /* We don't have access to any symbols in gdbserver, so we can't do
     any symbol lookups. If we ever need to more with this function we
     may need to figure out what we can do to determine if a pc value
     is arm or thumb using a system map, extra executable section, or
     some other way.  */
  return 0;
}

/* Remove useless bits from addresses in a running program.  */
static unsigned long
arm_addr_bits_remove (unsigned long val)
{
  return (val & (arm_pc_is_thumb (val) ? 0xfffffffe : 0xfffffffc));
}

#define ADDR_BITS_REMOVE(addr) (arm_addr_bits_remove (addr))

/* The following code is all needed to find the instruction following
   the current instruction so we can single step.  It is all taken from
   arm-tdep.c.  */
static int
condition_true (unsigned long cond, unsigned long status_reg)
{
  if (cond == INST_AL || cond == INST_NV)
    return 1;

  switch (cond)
    {
    case INST_EQ:
      return ((status_reg & FLAG_Z) != 0);
    case INST_NE:
      return ((status_reg & FLAG_Z) == 0);
    case INST_CS:
      return ((status_reg & FLAG_C) != 0);
    case INST_CC:
      return ((status_reg & FLAG_C) == 0);
    case INST_MI:
      return ((status_reg & FLAG_N) != 0);
    case INST_PL:
      return ((status_reg & FLAG_N) == 0);
    case INST_VS:
      return ((status_reg & FLAG_V) != 0);
    case INST_VC:
      return ((status_reg & FLAG_V) == 0);
    case INST_HI:
      return ((status_reg & (FLAG_C | FLAG_Z)) == FLAG_C);
    case INST_LS:
      return ((status_reg & (FLAG_C | FLAG_Z)) != FLAG_C);
    case INST_GE:
      return (((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0));
    case INST_LT:
      return (((status_reg & FLAG_N) == 0) != ((status_reg & FLAG_V) == 0));
    case INST_GT:
      return (((status_reg & FLAG_Z) == 0) &&
	      (((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0)));
    case INST_LE:
      return (((status_reg & FLAG_Z) != 0) ||
	      (((status_reg & FLAG_N) == 0) != ((status_reg & FLAG_V) == 0)));
    }
  return 1;
}

/* Support routines for single stepping.  Calculate the next PC value.  */
#define submask(x) ((1L << ((x) + 1)) - 1)
#define bit(obj,st) (((obj) >> (st)) & 1)
#define bits(obj,st,fn) (((obj) >> (st)) & submask ((fn) - (st)))
#define sbits(obj,st,fn) \
  ((long) (bits(obj,st,fn) | ((long) bit(obj,fn) * ~ submask (fn - st))))
#define BranchDest(addr,instr) \
  ((CORE_ADDR) (((long) (addr)) + 8 + (sbits (instr, 0, 23) << 2)))
#define ARM_PC_32 1

static unsigned long
shifted_reg_val (unsigned long inst, int carry, unsigned long pc_val,
		 unsigned long status_reg)
{
  unsigned long res, shift;
  int rm = bits (inst, 0, 3);
  unsigned long shifttype = bits (inst, 5, 6);

  if (bit (inst, 4))
    {
      int rs = bits (inst, 8, 11);
      shift = (rs == 15 ? pc_val + 8 : read_register (rs)) & 0xFF;
    }
  else
    shift = bits (inst, 7, 11);

  res = (rm == 15
	 ? ((pc_val | (ARM_PC_32 ? 0 : status_reg))
	    + (bit (inst, 4) ? 12 : 8))
	 : read_register (rm));

  switch (shifttype)
    {
    case 0:			/* LSL */
      res = shift >= 32 ? 0 : res << shift;
      break;

    case 1:			/* LSR */
      res = shift >= 32 ? 0 : res >> shift;
      break;

    case 2:			/* ASR */
      if (shift >= 32)
	shift = 31;
      res = ((res & 0x80000000L)
	     ? ~((~res) >> shift) : res >> shift);
      break;

    case 3:			/* ROR/RRX */
      shift &= 31;
      if (shift == 0)
	res = (res >> 1) | (carry ? 0x80000000L : 0);
      else
	res = (res >> shift) | (res << (32 - shift));
      break;
    }

  return res & 0xffffffff;
}

/* Return number of 1-bits in VAL.  */

static int
bitcount (unsigned long val)
{
  int nbits;
  for (nbits = 0; val != 0; nbits++)
    val &= val - 1;		/* delete rightmost 1-bit in val */
  return nbits;
}


CORE_ADDR
thumb_get_next_pc (CORE_ADDR pc)
{
  unsigned long pc_val = ((unsigned long) pc) + 4;	/* PC after prefetch */
  unsigned short inst1 = read_memory_integer (pc, 2);
  CORE_ADDR nextpc = pc + 2;		/* default is next instruction */
  unsigned long offset;

  if ((inst1 & 0xff00) == 0xbd00)	/* pop {rlist, pc} */
    {
      CORE_ADDR sp;

      /* Fetch the saved PC from the stack.  It's stored above
         all of the other registers.  */
      offset = bitcount (bits (inst1, 0, 7)) * 4; /* DEPRECATED_REGISTER_SIZE; */
      sp = read_register (ARM_SP_REGNUM);
      nextpc = (CORE_ADDR) read_memory_integer (sp + offset, 4);
      nextpc = ADDR_BITS_REMOVE (nextpc);
      if (nextpc == pc)
	error ("Infinite loop detected");
    }
  else if ((inst1 & 0xf000) == 0xd000)	/* conditional branch */
    {
      unsigned long status = read_register (ARM_PS_REGNUM);
      unsigned long cond = bits (inst1, 8, 11);
      if (cond != 0x0f && condition_true (cond, status))    /* 0x0f = SWI */
	nextpc = pc_val + (sbits (inst1, 0, 7) << 1);
    }
  else if ((inst1 & 0xf800) == 0xe000)	/* unconditional branch */
    {
      nextpc = pc_val + (sbits (inst1, 0, 10) << 1);
    }
  else if ((inst1 & 0xf800) == 0xf000)	/* long branch with link, and blx */
    {
      unsigned short inst2 = read_memory_integer (pc + 2, 2);
      offset = (sbits (inst1, 0, 10) << 12) + (bits (inst2, 0, 10) << 1);
      nextpc = pc_val + offset;
      /* For BLX make sure to clear the low bits.  */
      if (bits (inst2, 11, 12) == 1)
	nextpc = nextpc & 0xfffffffc;
    }
  else if ((inst1 & 0xff00) == 0x4700)	/* bx REG, blx REG */
    {
      if (bits (inst1, 3, 6) == 0x0f)
	nextpc = pc_val;
      else
	nextpc = read_register (bits (inst1, 3, 6));

      nextpc = ADDR_BITS_REMOVE (nextpc);
      if (nextpc == pc)
	error ("Infinite loop detected");
    }

  return nextpc;
}

unsigned long
arm_get_next_pc (unsigned long pc)
{
  unsigned long pc_val;
  unsigned long this_instr;
  unsigned long status;
  unsigned long nextpc;

  pc_val = (unsigned long) pc;
  this_instr = read_memory_integer (pc, 4);
  status = read_register (ARM_PS_REGNUM);
  nextpc = (pc_val + 4);	/* Default case */

  if (condition_true (bits (this_instr, 28, 31), status))
    {
      switch (bits (this_instr, 24, 27))
	{
	case 0x0:
	case 0x1:			/* data processing */
	case 0x2:
	case 0x3:
	  {
	    unsigned long operand1, operand2, result = 0;
	    unsigned long rn;
	    int c;

	    if (bits (this_instr, 12, 15) != 15)
	      break;

	    if (bits (this_instr, 22, 25) == 0
		&& bits (this_instr, 4, 7) == 9)	/* multiply */
	      error ("Illegal update to pc in instruction");

            /* BX <reg>, BLX <reg> */
            if (bits (this_instr, 4, 28) == 0x12fff1
                || bits (this_instr, 4, 28) == 0x12fff3)
              {
                rn = bits (this_instr, 0, 3);
                result = (rn == 15) ? pc_val + 8 : read_register (rn);
                nextpc = (CORE_ADDR) ADDR_BITS_REMOVE (result);

                if (nextpc == pc)
                  error ("Infinite loop detected");

                return nextpc;
              }

	    /* Multiply into PC */
	    c = (status & FLAG_C) ? 1 : 0;
	    rn = bits (this_instr, 16, 19);
	    operand1 = (rn == 15) ? pc_val + 8 : read_register (rn);

	    if (bit (this_instr, 25))
	      {
		unsigned long immval = bits (this_instr, 0, 7);
		unsigned long rotate = 2 * bits (this_instr, 8, 11);
		operand2 = ((immval >> rotate) | (immval << (32 - rotate)))
		  & 0xffffffff;
	      }
	    else		/* operand 2 is a shifted register */
	      operand2 = shifted_reg_val (this_instr, c, pc_val, status);

	    switch (bits (this_instr, 21, 24))
	      {
	      case 0x0:	/*and */
		result = operand1 & operand2;
		break;

	      case 0x1:	/*eor */
		result = operand1 ^ operand2;
		break;

	      case 0x2:	/*sub */
		result = operand1 - operand2;
		break;

	      case 0x3:	/*rsb */
		result = operand2 - operand1;
		break;

	      case 0x4:	/*add */
		result = operand1 + operand2;
		break;

	      case 0x5:	/*adc */
		result = operand1 + operand2 + c;
		break;

	      case 0x6:	/*sbc */
		result = operand1 - operand2 + c;
		break;

	      case 0x7:	/*rsc */
		result = operand2 - operand1 + c;
		break;

	      case 0x8:
	      case 0x9:
	      case 0xa:
	      case 0xb:	/* tst, teq, cmp, cmn */
		result = (unsigned long) nextpc;
		break;

	      case 0xc:	/*orr */
		result = operand1 | operand2;
		break;

	      case 0xd:	/*mov */
		/* Always step into a function.  */
		result = operand2;
		break;

	      case 0xe:	/*bic */
		result = operand1 & ~operand2;
		break;

	      case 0xf:	/*mvn */
		result = ~operand2;
		break;
	      }
	    nextpc = (CORE_ADDR) ADDR_BITS_REMOVE (result);

	    if (nextpc == pc)
	      error ("Infinite loop detected");
	    break;
	  }

	case 0x4:
	case 0x5:		/* data transfer */
	case 0x6:
	case 0x7:
	  if (bit (this_instr, 20))
	    {
	      /* load */
	      if (bits (this_instr, 12, 15) == 15)
		{
		  /* rd == pc */
		  unsigned long rn;
		  unsigned long base;

		  if (bit (this_instr, 22))
		    error ("Illegal update to pc in instruction");

		  /* byte write to PC */
		  rn = bits (this_instr, 16, 19);
		  base = (rn == 15) ? pc_val + 8 : read_register (rn);
		  if (bit (this_instr, 24))
		    {
		      /* pre-indexed */
		      int c = (status & FLAG_C) ? 1 : 0;
		      unsigned long offset =
		      (bit (this_instr, 25)
		       ? shifted_reg_val (this_instr, c, pc_val, status)
		       : bits (this_instr, 0, 11));

		      if (bit (this_instr, 23))
			base += offset;
		      else
			base -= offset;
		    }
		  nextpc = (CORE_ADDR) read_memory_integer ((CORE_ADDR) base,
							    4);

		  nextpc = ADDR_BITS_REMOVE (nextpc);

		  if (nextpc == pc)
		    error ("Infinite loop detected");
		}
	    }
	  break;

	case 0x8:
	case 0x9:		/* block transfer */
	  if (bit (this_instr, 20))
	    {
	      /* LDM */
	      if (bit (this_instr, 15))
		{
		  /* loading pc */
		  int offset = 0;

		  if (bit (this_instr, 23))
		    {
		      /* up */
		      unsigned long reglist = bits (this_instr, 0, 14);
		      offset = bitcount (reglist) * 4;
		      if (bit (this_instr, 24))		/* pre */
			offset += 4;
		    }
		  else if (bit (this_instr, 24))
		    offset = -4;

		  {
		    unsigned long rn_val =
		    read_register (bits (this_instr, 16, 19));
		    nextpc =
		      (CORE_ADDR) read_memory_integer ((CORE_ADDR) (rn_val
								  + offset),
						       4);
		  }
		  nextpc = ADDR_BITS_REMOVE (nextpc);
		  if (nextpc == pc)
		    error ("Infinite loop detected");
		}
	    }
	  break;

	case 0xb:		/* branch & link */
	case 0xa:		/* branch */
	  {
	    nextpc = BranchDest (pc, this_instr);

	    /* BLX */
	    if (bits (this_instr, 28, 31) == INST_NV)
	      nextpc |= bit (this_instr, 24) << 1;

	    nextpc = ADDR_BITS_REMOVE (nextpc);
	    if (nextpc == pc)
	      error ("Infinite loop detected");
	    break;
	  }

	case 0xc:
	case 0xd:
	case 0xe:		/* coproc ops */
	case 0xf:		/* SWI */
	  break;

	default:
	  warning ("Bad bit-field extraction\n");
	  return (pc);
	}
    }

  return nextpc;
}

/* We are currently keeping only one single-step breakpoint
   around.  We don't need to step over it or anything, since
   we just want it to implement software single stepping.  When
   we do that, we suspend all but the stepping thread, get the 
   next pc, put a breakpoint on that, continue, then remove that
   breakpoint and report that we've hit the step.  */

static struct arm_breakpoint
{
  char old_data[4];
  unsigned int where;
  int is_thumb;
} single_step_breakpoint;

static char arm_le_breakpoint[] = {0xFE,0xDE,0xFF,0xE7};
static char thumb_le_breakpoint[] = {0xfe,0xde};

void
set_single_step_breakpoint (unsigned int where, int is_thumb)
{
  char *breakpoint_data;
  int breakpoint_len;

  if (single_step_breakpoint.where != 0)
    warning ("set_single_step_breakpoint called with old breakpoint "
	     " uncleared.  Old address: 0x%x", 
	     single_step_breakpoint.where);

  if (is_thumb)
    {
      single_step_breakpoint.is_thumb = 1;
      breakpoint_len = 2;
      breakpoint_data = thumb_le_breakpoint;
    }
  else
    {
      single_step_breakpoint.is_thumb = 0;
      breakpoint_len = 4;
      breakpoint_data = arm_le_breakpoint;
    }
  
  single_step_breakpoint.where = where;
  (*the_target->read_memory) (where, single_step_breakpoint.old_data,
			      breakpoint_len);
  (*the_target->write_memory) (where, breakpoint_data,
			       breakpoint_len);
}

void
delete_single_step_breakpoint ()
{
  if (single_step_breakpoint.where == 0)
    return;

  (*the_target->write_memory) (single_step_breakpoint.where, 
			       single_step_breakpoint.old_data,
			       single_step_breakpoint.is_thumb ? 2 : 4);
  single_step_breakpoint.where = 0;
}

/* This function sets up the breakpoint on the next address
   that will get executed.  We also set the current inferior
   to THREAD.  */


void
arm_single_step_thread (thread_t thread, int on)
{
  unsigned long pc;
  unsigned long cpsr;
  unsigned long next_pc;
  int is_thumb;
  struct thread_info *old_current = current_inferior;

  if (on)
    {
      current_inferior = (struct thread_info *) find_inferior_id (&all_threads, thread);
      if (current_inferior == NULL)
	{
	  current_inferior = old_current;
	  error ("Could not find inferior for thread %d while stepping.",
		 thread);
	}
      
      collect_register_by_name ("pc", &pc);
      collect_register_by_name ("cpsr", &cpsr);
      is_thumb = (cpsr & FLAG_T) != 0;
      if (is_thumb)
	next_pc = thumb_get_next_pc (pc);
      else
	next_pc = arm_get_next_pc (pc);

      set_single_step_breakpoint (next_pc, is_thumb);
      
      current_inferior = old_current;
    }
  else
    delete_single_step_breakpoint ();
}

/* This may not be necessary.  */

int
arm_clear_single_step (thread_t thread)
{
  delete_single_step_breakpoint ();
  return 1;
}

struct macosx_target_ops the_low_target =
{
  arm_fetch_inferior_registers,
  arm_store_inferior_registers,
  arm_single_step_thread,
  arm_clear_single_step
};
