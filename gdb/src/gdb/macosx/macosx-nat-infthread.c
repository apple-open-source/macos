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

#include "macosx-nat-inferior.h"
#include "macosx-nat-inferior-util.h"
#include "macosx-nat-mutils.h"

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "gdbthread.h"

#include <stdio.h>
#include <sys/param.h>
#include <sys/dir.h>

extern macosx_inferior_status *macosx_status;

#include <mach/machine/thread_status.h>

#if defined (TARGET_I386)

#define THREAD_STATE i386_THREAD_STATE
#define THREAD_STRUCT i386_thread_state_t
#define THREAD_COUNT i386_THREAD_STATE_COUNT

#define TRACE_BIT_SET(s) ((s).eflags & 0x100UL)
#define SET_TRACE_BIT(s) ((s).eflags |= 0x100UL)
#define CLEAR_TRACE_BIT(s) ((s).eflags &= ~0x100UL)

#elif defined (TARGET_POWERPC)

#define THREAD_STATE PPC_THREAD_STATE
#define THREAD_STRUCT struct ppc_thread_state
#define THREAD_COUNT PPC_THREAD_STATE_COUNT

#define TRACE_BIT_SET(s) ((s).srr1 & 0x400UL)
#define SET_TRACE_BIT(s) ((s).srr1 |= 0x400UL)
#define CLEAR_TRACE_BIT(s) ((s).srr1 &= ~0x400UL)

#else
#error unknown architecture
#endif

kern_return_t set_trace_bit (thread_t thread)
{
  THREAD_STRUCT state;
  unsigned int state_count = THREAD_COUNT;
  kern_return_t	kret;
    
  kret = thread_get_state (thread, THREAD_STATE, (thread_state_t) &state, &state_count);
  MACH_PROPAGATE_ERROR (kret);

  if (! TRACE_BIT_SET (state)) {
    SET_TRACE_BIT (state);
    kret = thread_set_state (thread, THREAD_STATE, (thread_state_t) &state, state_count);
    MACH_PROPAGATE_ERROR (kret);
  }

  return KERN_SUCCESS;
}

kern_return_t clear_trace_bit (thread_t thread)
{
  THREAD_STRUCT state;
  unsigned int state_count = THREAD_COUNT;
  kern_return_t kret;

  kret = thread_get_state (thread, THREAD_STATE, (thread_state_t) &state, &state_count);
  MACH_PROPAGATE_ERROR (kret);

  if (TRACE_BIT_SET (state)) {
    CLEAR_TRACE_BIT (state);
    kret = thread_set_state (thread, THREAD_STATE, (thread_state_t) &state, state_count);
    MACH_PROPAGATE_ERROR (kret);
  }

  return KERN_SUCCESS;
}

void prepare_threads_after_stop (struct macosx_inferior_status *inferior)
{
  thread_array_t thread_list = NULL;
  unsigned int nthreads = 0;
  kern_return_t kret;
  unsigned int i;
  
  if (inferior->exception_status.saved_exceptions_stepping) {
    kret = macosx_restore_exception_ports (inferior->task, &inferior->exception_status.saved_exceptions_step);
    MACH_CHECK_ERROR (kret);
    inferior->exception_status.saved_exceptions_stepping = 0;
  }

  macosx_check_new_threads ();

  kret = task_threads (inferior->task, &thread_list, &nthreads);
  MACH_CHECK_ERROR (kret);
  
  for (i = 0; i < nthreads; i++) {

    ptid_t ptid;
    struct thread_info *tp = NULL;

    ptid = ptid_build (inferior->pid, 0, thread_list[i]);
    tp = find_thread_pid (ptid);
    CHECK_FATAL (tp != NULL);
    while (tp->gdb_suspend_count > 0) {
      thread_resume (thread_list[i]);
      tp->gdb_suspend_count--;
    }
    kret = clear_trace_bit (thread_list[i]);
    MACH_WARN_ERROR (kret);
  }
  
  kret = vm_deallocate (mach_task_self(), (vm_address_t) thread_list, (nthreads * sizeof (int)));
  MACH_WARN_ERROR (kret);
}

void prepare_threads_before_run
  (struct macosx_inferior_status *inferior, int step, thread_t current, int stop_others)
{
  thread_array_t thread_list = NULL;
  unsigned int nthreads = 0;
  kern_return_t kret;
  unsigned int i;
  
  macosx_check_new_threads ();
  prepare_threads_after_stop (inferior);

  if (! step) {
    CHECK_FATAL (current == THREAD_NULL);
  }

  if (step) {
    struct thread_basic_info info;
    unsigned int info_count = THREAD_BASIC_INFO_COUNT;
    kern_return_t kret;
    
    kret = thread_info (current, THREAD_BASIC_INFO, (thread_info_t) &info, &info_count);
    MACH_CHECK_ERROR (kret);

    if (info.suspend_count != 0) {
      error ("Unable to single-step thread (thread is already suspended from outside of GDB)");
    }
  }

  kret = task_threads (inferior->task, &thread_list, &nthreads);
  MACH_CHECK_ERROR (kret);
  
  for (i = 0; i < nthreads; i++)  {

    kret = clear_trace_bit (thread_list[i]);
    MACH_CHECK_ERROR (kret);
    if ((step && stop_others) && (thread_list[i] != current)) {

      ptid_t ptid;
      struct thread_info *tp = NULL;
      
      ptid = ptid_build (inferior->pid, 0, thread_list[i]);
      tp = find_thread_pid (ptid);
      CHECK_FATAL (tp != NULL);
      kret = thread_suspend (thread_list[i]);
      MACH_CHECK_ERROR (kret);
      tp->gdb_suspend_count++;
    }
  }

  kret = vm_deallocate (mach_task_self(), (vm_address_t) thread_list, (nthreads * sizeof (int)));
  MACH_CHECK_ERROR (kret);
  
  if (step && stop_others) {
    set_trace_bit (current);
  }

  if (step) {
    kret = macosx_save_exception_ports (inferior->task, &inferior->exception_status.saved_exceptions_step);
    MACH_CHECK_ERROR (kret);
    inferior->exception_status.saved_exceptions_stepping = 1;
  }
}

char *unparse_run_state (int run_state)
{
  switch (run_state) {
  case TH_STATE_RUNNING: return "RUNNING";
  case TH_STATE_STOPPED: return "STOPPED";
  case TH_STATE_WAITING: return "WAITING";
  case TH_STATE_UNINTERRUPTIBLE: return "UNINTERRUPTIBLE";
  case TH_STATE_HALTED: return "HALTED";
  default: return "[UNKNOWN]";
  }
}

void print_thread_info (thread_t tid)
{
  struct thread_basic_info info;
  unsigned int info_count = THREAD_BASIC_INFO_COUNT;
  kern_return_t kret;

  kret = thread_info (tid, THREAD_BASIC_INFO, (thread_info_t) &info, &info_count);
  MACH_CHECK_ERROR (kret);

  printf_filtered ("Thread 0x%lx has current state \"%s\"\n",
		   (unsigned long) tid, unparse_run_state (info.run_state));
  printf_filtered ("Thread 0x%lx has a suspend count of %d",
		   (unsigned long) tid, info.suspend_count);
  if (info.sleep_time == 0) {
    printf_filtered (".\n");
  } else {
    printf_filtered (", and has been sleeping for %d seconds.\n", info.sleep_time);
  }
}

void info_task_command (char *args, int from_tty)
{
  struct task_basic_info info;
  unsigned int info_count = TASK_BASIC_INFO_COUNT;

  thread_array_t thread_list = NULL;
  unsigned int nthreads = 0;

  kern_return_t kret;
  unsigned int i;
  
  kret = task_info (macosx_status->task, TASK_BASIC_INFO, (task_info_t) &info, &info_count);
  MACH_CHECK_ERROR (kret);

  printf_filtered ("Inferior task 0x%lx has a suspend count of %d.\n", (unsigned long) macosx_status->task, info.suspend_count);

  kret = task_threads (macosx_status->task, &thread_list, &nthreads);
  MACH_CHECK_ERROR (kret);
  
  printf_filtered ("The task has %lu threads:\n", (unsigned long) nthreads);
  for (i = 0; i < nthreads; i++) {
    print_thread_info (thread_list[i]);
  }
  
  kret = vm_deallocate (mach_task_self(), (vm_address_t) thread_list, (nthreads * sizeof (int)));
  MACH_CHECK_ERROR (kret);
}

static thread_t parse_thread (char *tidstr)
{
  ptid_t ptid;
  
  if (ptid_equal (inferior_ptid, null_ptid)) {
    error ("The program being debugged is not being run.");
  }
 
  if (tidstr != NULL) {

    int num = atoi (tidstr);
    ptid = thread_id_to_pid (num);

    if (ptid_equal (ptid, minus_one_ptid)) {
      error ("Thread ID %d not known.  Use the \"info threads\" command to\n"
	     "see the IDs of currently known threads.", num);
    }
  } else {
    ptid = inferior_ptid;
  }

  if (! target_thread_alive (ptid)) {
    error ("Thread ID %s does not exist.\n", target_pid_to_str (ptid));
  }

  return ptid_get_tid (ptid);
}

void info_thread_command (char *tidstr, int from_tty)
{
  thread_t thread = parse_thread (tidstr);
  print_thread_info (thread);
}

static void thread_suspend_command (char *tidstr, int from_tty)
{
  kern_return_t kret;
  thread_t thread;

  thread = parse_thread (tidstr);
  kret = thread_suspend (thread);

  MACH_CHECK_ERROR (kret);
}

static void thread_resume_command (char *tidstr, int from_tty)
{
  kern_return_t kret;
  thread_t thread;

  thread = parse_thread (tidstr);
  kret = thread_resume (thread);
  
  MACH_CHECK_ERROR (kret);
}

void
_initialize_threads ()
{
  add_cmd ("suspend", class_run, thread_suspend_command,
	  "Suspend a thread.", &thread_cmd_list);

  add_cmd ("resume", class_run, thread_resume_command,
	  "Resume a thread.", &thread_cmd_list);

  add_info ("thread", info_thread_command,
	    "Get information on thread.");

  add_info ("task", info_task_command,
	    "Get information on task.");
}
