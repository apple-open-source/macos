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

#include "ppc-macosx-regnums.h"
#include "ppc-macosx-thread-status.h"

/* This is roughly cribbed from ppc-macosx-regs.c.  We don't have the
   gdbarch stuff going in gdbserver, however.  So we can't just use it
   exactly...  */

void
store_unsigned_integer (void *addr, int len, unsigned long long val)
{
  unsigned char *p;
  unsigned char *startaddr = (unsigned char *) addr;
  unsigned char *endaddr = startaddr + len;
  
  /* Start at the least significant end of the integer, and work towards
     the most significant.  */
  for (p = endaddr - 1; p >= startaddr; --p)
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

static inline void
supply_unsigned_int_64 (int regnum, unsigned long long val)
{
  char buf[8] = { 0 };
  store_unsigned_integer (buf, 8, val);
  supply_register (regnum, buf);
}

void
ppc_macosx_fetch_gp_registers_64 (gdb_ppc_thread_state_64_t *gp_regs)
{
  int i;
  
  for (i = 0; i < PPC_MACOSX_NUM_GP_REGS; i++)
    {
      supply_unsigned_int_64 (PPC_MACOSX_FIRST_GP_REGNUM + i,
			      gp_regs->gpregs[i]);
    }
  
  supply_unsigned_int_64 (PPC_MACOSX_PC_REGNUM, gp_regs->srr0);
  supply_unsigned_int_64 (PPC_MACOSX_PS_REGNUM, gp_regs->srr1);
  supply_unsigned_int (PPC_MACOSX_CR_REGNUM, gp_regs->cr);
  supply_unsigned_int_64 (PPC_MACOSX_LR_REGNUM, gp_regs->lr);
  supply_unsigned_int_64 (PPC_MACOSX_CTR_REGNUM, gp_regs->ctr);
  supply_unsigned_int_64 (PPC_MACOSX_XER_REGNUM, gp_regs->xer);
  supply_unsigned_int (PPC_MACOSX_VRSAVE_REGNUM, gp_regs->vrsave);
}

void
ppc_macosx_fetch_fp_registers (gdb_ppc_thread_fpstate_t *fp_regs)
{
}

void
ppc_macosx_fetch_vp_registers (gdb_ppc_thread_vpstate_t *vp_regs)
{
  
}

/* Read register values from the inferior process.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
ppc_fetch_inferior_registers (int regno)
{
  thread_t current_thread = ((struct inferior_list_entry *) current_inferior)->id;
  
  if ((regno == -1) || PPC_MACOSX_IS_GP_REGNUM (regno)
      || PPC_MACOSX_IS_GSP_REGNUM (regno))
    {
      gdb_ppc_thread_state_64_t gp_regs;
      unsigned int gp_count = GDB_PPC_THREAD_STATE_64_COUNT;
      kern_return_t ret = thread_get_state
        (current_thread, GDB_PPC_THREAD_STATE_64, 
	 (thread_state_t) & gp_regs,
         &gp_count);
      MACH_CHECK_ERROR (ret);
      ppc_macosx_fetch_gp_registers_64 (&gp_regs);
    }
  
  if ((regno == -1) || PPC_MACOSX_IS_FP_REGNUM (regno)
      || PPC_MACOSX_IS_FSP_REGNUM (regno))
    {
      gdb_ppc_thread_fpstate_t fp_regs;
      unsigned int fp_count = GDB_PPC_THREAD_FPSTATE_COUNT;
      kern_return_t ret = thread_get_state
        (current_thread, GDB_PPC_THREAD_FPSTATE, 
	 (thread_state_t) & fp_regs,
         &fp_count);
      MACH_CHECK_ERROR (ret);
      ppc_macosx_fetch_fp_registers (&fp_regs);
    }
  
  if ((regno == -1) || PPC_MACOSX_IS_VP_REGNUM (regno)
      || PPC_MACOSX_IS_VSP_REGNUM (regno))
    {
      gdb_ppc_thread_vpstate_t vp_regs;
      unsigned int vp_count = GDB_PPC_THREAD_VPSTATE_COUNT;
      kern_return_t ret = thread_get_state
        (current_thread, GDB_PPC_THREAD_VPSTATE, 
	 (thread_state_t) & vp_regs,
         &vp_count);
      MACH_CHECK_ERROR (ret);
      ppc_macosx_fetch_vp_registers (&vp_regs);
    }
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
  for (p = startaddr; p < endaddr; ++p)
    retval = (retval << 8) | *p;
  return retval;
}

static inline void
collect_unsigned_int (int regnum, unsigned int *addr)
{
  char buf[4];
  collect_register (regnum, buf);
  *addr = extract_unsigned_integer (buf, 4);
}

static inline void
collect_unsigned_int_64 (int regnum, unsigned long long *addr)
{
  char buf[8];
  collect_register (regnum, buf);
  *addr = extract_unsigned_integer (buf, 8);
}
void
ppc_macosx_store_gp_registers_64 (gdb_ppc_thread_state_64_t *gp_regs)
{
  int i;
  
  for (i = 0; i < PPC_MACOSX_NUM_GP_REGS; i++)
    {
      collect_unsigned_int_64 (PPC_MACOSX_FIRST_GP_REGNUM + i,
                               &gp_regs->gpregs[i]);
    }
  
  collect_unsigned_int_64 (PPC_MACOSX_PC_REGNUM, &gp_regs->srr0);
  collect_unsigned_int_64 (PPC_MACOSX_PS_REGNUM, &gp_regs->srr1);
  collect_unsigned_int (PPC_MACOSX_CR_REGNUM, &gp_regs->cr);
  collect_unsigned_int_64 (PPC_MACOSX_LR_REGNUM, &gp_regs->lr);
  collect_unsigned_int_64 (PPC_MACOSX_CTR_REGNUM, &gp_regs->ctr);
  collect_unsigned_int_64 (PPC_MACOSX_XER_REGNUM, &gp_regs->xer);
  /* collect_unsigned_int (PPC_MACOSX_MQ_REGNUM, &gp_regs->mq); */
  collect_unsigned_int (PPC_MACOSX_VRSAVE_REGNUM, &gp_regs->vrsave);
}

void
ppc_macosx_store_fp_registers (gdb_ppc_thread_fpstate_t *fp_regs)
{
}

void
ppc_macosx_store_vp_registers (gdb_ppc_thread_vpstate_t *vp_regs)
{
  
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
ppc_store_inferior_registers (int regno)
{
  thread_t current_thread = ((struct inferior_list_entry *) current_inferior)->id;
  
  // validate_inferior_registers (regno);
  
  if ((regno == -1) || PPC_MACOSX_IS_GP_REGNUM (regno)
      || PPC_MACOSX_IS_GSP_REGNUM (regno))
    {
      gdb_ppc_thread_state_64_t gp_regs;
      kern_return_t ret;
      ppc_macosx_store_gp_registers_64 (&gp_regs);
      ret = thread_set_state (current_thread, GDB_PPC_THREAD_STATE_64,
                              (thread_state_t) & gp_regs,
                              GDB_PPC_THREAD_STATE_64_COUNT);
      MACH_CHECK_ERROR (ret);
    }
  
  if ((regno == -1) || PPC_MACOSX_IS_FP_REGNUM (regno)
      || PPC_MACOSX_IS_FSP_REGNUM (regno))
    {
      gdb_ppc_thread_fpstate_t fp_regs;
      kern_return_t ret;
      ppc_macosx_store_fp_registers (&fp_regs);
      ret = thread_set_state (current_thread, GDB_PPC_THREAD_FPSTATE,
                              (thread_state_t) & fp_regs,
                              GDB_PPC_THREAD_FPSTATE_COUNT);
      MACH_CHECK_ERROR (ret);
    }
  
  if ((regno == -1) || PPC_MACOSX_IS_VP_REGNUM (regno)
      || PPC_MACOSX_IS_VSP_REGNUM (regno))
    {
      gdb_ppc_thread_vpstate_t vp_regs;
      kern_return_t ret;
      ppc_macosx_store_vp_registers (&vp_regs);
      ret = thread_set_state (current_thread, GDB_PPC_THREAD_VPSTATE,
                              (thread_state_t) & vp_regs,
                              GDB_PPC_THREAD_VPSTATE_COUNT);
      MACH_CHECK_ERROR (ret);
    }
}


#define set_trace_bit(thread) modify_trace_bit (thread, 1)
#define clear_trace_bit(thread) modify_trace_bit (thread, 0)


  /* Set the single step bit in the processor status register.  */
kern_return_t
modify_trace_bit (thread_t thread, int value)
{
  gdb_ppc_thread_state_64_t state;
  unsigned int state_count = GDB_PPC_THREAD_STATE_64_COUNT;
  kern_return_t kret;

  kret =
    thread_get_state (thread, GDB_PPC_THREAD_STATE_64,
                      (thread_state_t) & state, &state_count);
  MACH_PROPAGATE_ERROR (kret);

  if ((state.srr1 & 0x400ULL) != (value ? 1 : 0))
    {
      state.srr1 = (state.srr1 & ~0x400ULL) | (value ? 0x400ULL : 0);
      kret =
        thread_set_state (thread, GDB_PPC_THREAD_STATE_64,
                          (thread_state_t) & state, state_count);
      MACH_PROPAGATE_ERROR (kret);
    }

  return KERN_SUCCESS;

}

void
ppc_single_step_thread (thread_t thread, int on)
{
  modify_trace_bit (thread, on);
}

int
ppc_clear_single_step (thread_t thread)
{
}

struct macosx_target_ops the_low_target =
{
  ppc_fetch_inferior_registers,
  ppc_store_inferior_registers,
  ppc_single_step_thread,
  ppc_clear_single_step
};
