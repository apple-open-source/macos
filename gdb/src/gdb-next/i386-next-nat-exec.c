#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdbcmd.h"

#include "i386-thread-status.h"
#include "i386-next-tdep.h"

#include "nextstep-nat-mutils.h"
#include "nextstep-nat-inferior.h"

extern next_inferior_status *next_status;

static void validate_inferior_registers (int regno)
{
  int i;
  if (regno == -1) {
    for (i = 0; i < NUM_REGS; i++) {
      if (!register_valid[i])
        fetch_inferior_registers(i);
    }
  } else if (! register_valid[regno]) {
    fetch_inferior_registers (regno);
  }
}

/* Read register values from the inferior process.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void fetch_inferior_registers (int regno)
{
  int current_pid;
  thread_t current_thread;

  next_thread_list_lookup_by_id
    (next_status, inferior_pid, &current_pid, &current_thread);

  if ((regno == -1) || IS_GP_REGNUM (regno) || IS_GSP_REGNUM (regno)) {
    int i;
    gdb_i386_thread_state_t gp_regs;
    unsigned int gp_count = GDB_i386_THREAD_STATE_COUNT;
    kern_return_t ret = thread_get_state
      (current_thread, GDB_i386_THREAD_STATE, (thread_state_t) &gp_regs, &gp_count);
    MACH_CHECK_ERROR (ret);
    i386_next_fetch_gp_registers (registers, &gp_regs);
    i386_next_fetch_sp_registers (registers, &gp_regs);
    for (i = FIRST_GP_REGNUM; i <= LAST_GP_REGNUM; i++) {
      register_valid[i] = 1;
    }
    for (i = FIRST_GSP_REGNUM; i <= LAST_GSP_REGNUM; i++) {
      register_valid[i] = 1;
    }
  }

  if ((regno == -1) || IS_FP_REGNUM (regno)) {
    int i;
    gdb_i386_thread_fpstate_t fp_regs;
    unsigned int fp_count = GDB_i386_THREAD_FPSTATE_COUNT;
    kern_return_t ret = thread_get_state
      (current_thread, GDB_i386_THREAD_FPSTATE, (thread_state_t) &fp_regs, &fp_count);
    i386_next_fetch_fp_registers (registers, &fp_regs);
    for (i = FIRST_FP_REGNUM; i <= LAST_FP_REGNUM; i++) {
      register_valid[i] = 1;
    }
  }
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void store_inferior_registers (int regno)
{
  int current_pid;
  thread_t current_thread;

  next_thread_list_lookup_by_id
    (next_status, inferior_pid, &current_pid, &current_thread);

  validate_inferior_registers (regno);

  if ((regno == -1) || IS_GP_REGNUM (regno) || IS_GSP_REGNUM (regno)) {
    gdb_i386_thread_state_t gp_regs;
    kern_return_t ret;
    i386_next_store_gp_registers (registers, &gp_regs);
    i386_next_store_sp_registers (registers, &gp_regs);
    ret = thread_set_state (current_thread, GDB_i386_THREAD_STATE,
			    (thread_state_t) &gp_regs, GDB_i386_THREAD_STATE_COUNT);
    MACH_CHECK_ERROR (ret);
  }

  if ((regno == -1) || IS_FP_REGNUM (regno)) {
    gdb_i386_thread_fpstate_t fp_regs;
    kern_return_t ret;
    i386_next_store_fp_registers (registers, &fp_regs);
    ret = thread_set_state (current_thread, GDB_i386_THREAD_FPSTATE,
			    (thread_state_t) &fp_regs, GDB_i386_THREAD_FPSTATE_COUNT);
    MACH_CHECK_ERROR (ret);
  }
}
