#include "nextstep-nat-inferior.h"

struct next_exception_info;

/* print string representation of ptrace() request value */

const char *ptrace_request_unparse PARAMS ((int request));

/* call system ptrace(), logging debugging information as appropriate */

int call_ptrace PARAMS ((int request, int pid, int arg3, int arg4));

/* clear all values in a next_inferior_status structure */

void next_inferior_reset PARAMS ((next_inferior_status *s));

/* tear down a next_inferior_status structure, killing all helper
   threads and deallocating all ports and/or allocated memory.  the
   task and associated pid must both be known to the kernel to be
   dead. */

void next_inferior_destroy PARAMS ((next_inferior_status *s));

/* check that the inferior is valid and still exists (both the UNIX
   pid and the Mach task).  */

int next_inferior_valid PARAMS ((next_inferior_status *s));

/* verify that the inferior exists and is stopped, either by being
   stopped in ptrace(), by having a suspend count of 1, or both.
   abort() if the inferior no longer exists or is not stopped. */

void next_inferior_check_stopped PARAMS ((next_inferior_status *s));

/* suspend the mach portion of the inferior task, causing it to have a
   suspend count of 1. */

kern_return_t next_inferior_suspend_mach PARAMS ((next_inferior_status *s));

/* resume the mach portion of the inferior task <count> times.  if
   <count> is -1, resume the mach portion of the inferior task until
   it has a suspend count of 0. */

kern_return_t next_inferior_resume_mach PARAMS ((next_inferior_status *s, int count));

/* inferior must be valid, stopped, and attached via ptrace.  cause
   inferior to be stopped in ptrace() by sending (and handling) a
   SIGSTOP.  abort() if process is invalid, not stopped, already
   stopped in ptrace, or not attached via ptrace. */

void next_inferior_suspend_ptrace PARAMS ((next_inferior_status *s));

/* inferior must be valid, and attached via ptrace.  cause inferior to
   exit from ptrace, suspending the Mach portion to prevent further
   execution. abort() if process is invalid, not stopped via ptrace, or
   not attached via ptrace(). */

void next_inferior_resume_ptrace PARAMS ((next_inferior_status *s, int nsignal, int val));

void next_thread_list_lookup_by_id PARAMS ((next_inferior_status *s, int id, int *ppid, thread_t *pthread));

void next_thread_list_lookup_by_info PARAMS ((next_inferior_status *s, int pid, thread_t thread, int *ppid));

int next_thread_list_insert PARAMS ((next_inferior_status *s, int pid, thread_t thread));

void next_thread_list_destroy PARAMS ((next_thread_entry *tpid));

void next_save_exception_ports PARAMS ((task_t task, struct next_exception_info *info));

void next_restore_exception_ports PARAMS ((task_t task, struct next_exception_info *info));
