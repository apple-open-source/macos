#ifndef __GDB_MACOSX_NAT_INFERIOR_UTIL_H__
#define __GDB_MACOSX_NAT_INFERIOR_UTIL_H__

#include <mach/mach.h>
#include "macosx-nat-inferior.h"
#include "inferior.h"

struct macosx_exception_info;

/* Print string representation of ptrace() request value. */

const char *ptrace_request_unparse (int request);

/* Call system ptrace(), logging debugging information as appropriate. */

int call_ptrace (int request, int pid, PTRACE_ARG3_TYPE arg3, int arg4);

/* Clear all values in a macosx_inferior_status structure. */

void macosx_inferior_reset (macosx_inferior_status *s);

/* Tear down a macosx_inferior_status structure, killing all helper
   threads and deallocating all ports and/or allocated memory.  The
   task and associated pid must both be known to the kernel to be
   dead. */

void macosx_inferior_destroy (macosx_inferior_status *s);

/* Check that the inferior is valid and still exists (both the UNIX
   pid and the Mach task).  */

int macosx_inferior_valid (macosx_inferior_status *s);

/* Verify that the inferior exists and is stopped, either by being
   stopped in ptrace(), by having a suspend count of 1, or both.
   abort() if the inferior no longer exists or is not stopped. */

void macosx_inferior_check_stopped (macosx_inferior_status *s);

/* Suspend the mach portion of the inferior task, causing it to have a
   suspend count of 1. */

kern_return_t macosx_inferior_suspend_mach (macosx_inferior_status *s);

/* Resume the mach portion of the inferior task <count> times.  If
   <count> is -1, resume the mach portion of the inferior task until
   it has a suspend count of 0. */

kern_return_t macosx_inferior_resume_mach (macosx_inferior_status *s, int count);

/* Inferior must be valid, stopped, and attached via ptrace.  Cause
   inferior to be stopped in ptrace() by sending (and handling) a
   SIGSTOP.  abort() if process is invalid, not stopped, already
   stopped in ptrace, or not attached via ptrace. */

void macosx_inferior_suspend_ptrace (macosx_inferior_status *s);

/* Inferior must be valid, and attached via ptrace.  Cause inferior to
   exit from ptrace, suspending the Mach portion to prevent further
   execution. abort() if process is invalid, not stopped via ptrace, or
   not attached via ptrace(). */

void macosx_inferior_resume_ptrace (macosx_inferior_status *s,
                                    unsigned int thread, int nsignal,
                                    int val);

kern_return_t macosx_save_exception_ports (task_t task,
                                           struct macosx_exception_info *info);

kern_return_t macosx_restore_exception_ports (task_t task,
                                              struct macosx_exception_info *info);

#endif /* __GDB_MACOSX_NAT_INFERIOR_UTIL_H__ */
