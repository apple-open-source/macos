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
#include "macosx-nat-inferior-debug.h"
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

#define set_trace_bit(thread) modify_trace_bit (thread, 1)
#define clear_trace_bit(thread) modify_trace_bit (thread, 0)

void
macosx_setup_registers_before_hand_call ()
{
  thread_t current_thread = ptid_get_tid (inferior_ptid);

  thread_abort_safely (current_thread);
}

#if defined (TARGET_I386)

#include "i386-macosx-thread-status.h"

kern_return_t
modify_trace_bit (thread_t thread, int value)
{
  i386_thread_state_t state;
  unsigned int state_count = i386_THREAD_STATE_COUNT;
  kern_return_t kret;

  kret =
    thread_get_state (thread, i386_THREAD_STATE, (thread_state_t) & state,
                      &state_count);
  MACH_PROPAGATE_ERROR (kret);

  if ((state.eflags & 0x100UL) != (value ? 1 : 0))
    {
      state.eflags = (state.eflags & ~0x100UL) | (value ? 0x100UL : 0);
      kret =
        thread_set_state (thread, i386_THREAD_STATE, (thread_state_t) & state,
                          state_count);
      MACH_PROPAGATE_ERROR (kret);
    }

  return KERN_SUCCESS;
}

#elif defined (TARGET_POWERPC)

#include "ppc-macosx-thread-status.h"

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

  if ((state.srr1 & 0x400UL) != (value ? 1 : 0))
    {
      state.srr1 = (state.srr1 & ~0x400UL) | (value ? 0x400UL : 0);
      kret =
        thread_set_state (thread, GDB_PPC_THREAD_STATE_64,
                          (thread_state_t) & state, state_count);
      MACH_PROPAGATE_ERROR (kret);
    }

  return KERN_SUCCESS;

}

#else
#error unknown architecture
#endif

void
prepare_threads_after_stop (struct macosx_inferior_status *inferior)
{
  thread_array_t thread_list = NULL;
  unsigned int nthreads = 0;
  kern_return_t kret;
  unsigned int i;

  if (inferior->exception_status.saved_exceptions_stepping)
    {
      kret = macosx_restore_exception_ports (inferior->task,
                                             &inferior->exception_status.
                                             saved_exceptions_step);
      MACH_CHECK_ERROR (kret);
      inferior->exception_status.saved_exceptions_stepping = 0;
    }

  macosx_check_new_threads ();

  kret = task_threads (inferior->task, &thread_list, &nthreads);
  MACH_CHECK_ERROR (kret);

  for (i = 0; i < nthreads; i++)
    {

      ptid_t ptid;
      struct thread_info *tp = NULL;

      ptid = ptid_build (inferior->pid, 0, thread_list[i]);
      tp = find_thread_pid (ptid);
      CHECK_FATAL (tp != NULL);
      if (inferior_debug_flag >= 2)
        {
          struct thread_basic_info info;
          unsigned int info_count = THREAD_BASIC_INFO_COUNT;
          kern_return_t kret;

          kret =
            thread_info (thread_list[i], THREAD_BASIC_INFO,
                         (thread_info_t) & info, &info_count);
          MACH_CHECK_ERROR (kret);

          if (tp->gdb_suspend_count > 0)
            inferior_debug (2, "**  Resuming thread 0x%x, gdb suspend count: "
                            "%d, real suspend count: %d\n",
                            thread_list[i], tp->gdb_suspend_count,
                            info.suspend_count);
          else
            inferior_debug (2, "**  Thread 0x%x was not suspended from gdb, "
                            "real suspend count: %d\n",
                            thread_list[i], info.suspend_count);
        }
      while (tp->gdb_suspend_count > 0)
        {
          thread_resume (thread_list[i]);
          tp->gdb_suspend_count--;
        }
      kret = clear_trace_bit (thread_list[i]);
      MACH_WARN_ERROR (kret);
    }

  kret =
    vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
                   (nthreads * sizeof (int)));
  MACH_WARN_ERROR (kret);
}

void prepare_threads_before_run
  (struct macosx_inferior_status *inferior, int step, thread_t current,
   int stop_others)
{
  thread_array_t thread_list = NULL;
  unsigned int nthreads = 0;
  kern_return_t kret;
  unsigned int i;

  prepare_threads_after_stop (inferior);

  /*
     if (! step) {
     CHECK_FATAL (current == THREAD_NULL);
     }
   */

  if (step || stop_others)
    {
      struct thread_basic_info info;
      unsigned int info_count = THREAD_BASIC_INFO_COUNT;
      kern_return_t kret;

      kret =
        thread_info (current, THREAD_BASIC_INFO, (thread_info_t) & info,
                     &info_count);
      MACH_CHECK_ERROR (kret);

      if (info.suspend_count != 0)
        {
          if (step)
            error ("Unable to single-step thread 0x%x "
                   "(thread is already suspended from outside of GDB)",
                   current);
          else
            error ("Unable to run only thread 0x%x "
                   "(thread is already suspended from outside of GDB)",
                   current);
        }
    }

  kret = task_threads (inferior->task, &thread_list, &nthreads);
  MACH_CHECK_ERROR (kret);

  if (step)
    inferior_debug (2, "*** Suspending threads to step: 0x%x\n", current);

  for (i = 0; i < nthreads; i++)
    {

      /* Don't need to do this either, since it is already done in prepare_threads_after_stop
         kret = clear_trace_bit (thread_list[i]);

         MACH_CHECK_ERROR (kret);
       */

      if ((stop_others) && (thread_list[i] != current))
        {

          ptid_t ptid;
          struct thread_info *tp = NULL;

          ptid = ptid_build (inferior->pid, 0, thread_list[i]);
          tp = find_thread_pid (ptid);
          CHECK_FATAL (tp != NULL);
          kret = thread_suspend (thread_list[i]);
          MACH_CHECK_ERROR (kret);
          tp->gdb_suspend_count++;
          inferior_debug (2, "*** Suspending thread 0x%x, suspend count %d\n",
                          thread_list[i], tp->gdb_suspend_count);
        }
      else if (stop_others)
        inferior_debug (2, "*** Allowing thread 0x%x to run from gdb\n",
                        thread_list[i]);
    }

  kret =
    vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
                   (nthreads * sizeof (int)));
  MACH_CHECK_ERROR (kret);

  if (step)
    {
      set_trace_bit (current);
    }

  if (step)
    {
      kret =
        macosx_save_exception_ports (inferior->task,
                                     &inferior->exception_status.
                                     saved_exceptions_step);
      MACH_CHECK_ERROR (kret);
      inferior->exception_status.saved_exceptions_stepping = 1;
    }
}

char *
unparse_run_state (int run_state)
{
  switch (run_state)
    {
    case TH_STATE_RUNNING:
      return "RUNNING";
    case TH_STATE_STOPPED:
      return "STOPPED";
    case TH_STATE_WAITING:
      return "WAITING";
    case TH_STATE_UNINTERRUPTIBLE:
      return "UNINTERRUPTIBLE";
    case TH_STATE_HALTED:
      return "HALTED";
    default:
      return "[UNKNOWN]";
    }
}

thread_t
get_application_thread_port (thread_t our_name)
{
  mach_msg_type_number_t i;
  mach_port_name_array_t names;
  mach_msg_type_number_t names_count;
  mach_port_type_array_t types;
  mach_msg_type_number_t types_count;
  mach_port_t match = 0;
  kern_return_t ret;

  ret = mach_port_names (macosx_status->task, &names, &names_count, &types,
                   &types_count);
  if (ret != KERN_SUCCESS)
    {
      warning ("Error %d getting port names from mach_port_names", ret);
      return (thread_t) 0x0;
    }

  for (i = 0; i < names_count; i++)
    {
      mach_port_t local_name;
      mach_msg_type_name_t local_type;

      ret = mach_port_extract_right (macosx_status->task,
                                     names[i],
                                     MACH_MSG_TYPE_COPY_SEND,
                                     &local_name, &local_type);
      if (ret == KERN_SUCCESS)
        {
          mach_port_deallocate (mach_task_self (), local_name); // don't actually need another right
          if (local_name == our_name)
            {
              match = names[i];
              break;
            }
        }
    }

  vm_deallocate (mach_task_self (), (vm_address_t) names,
                 names_count * sizeof (mach_port_t));

  return (thread_t) match;
}

void
print_thread_info (thread_t tid)
{
  struct thread_basic_info info;
  unsigned int info_count = THREAD_BASIC_INFO_COUNT;
  kern_return_t kret;
  thread_t app_thread_name;

  kret =
    thread_info (tid, THREAD_BASIC_INFO, (thread_info_t) & info, &info_count);
  MACH_CHECK_ERROR (kret);

  /* FIXME: We store the application port name in the private structure
     in the thread_info.  Can we use that here rather than having to
     call get_application_thread_port? */

  app_thread_name = get_application_thread_port (tid);

  printf_filtered ("Thread 0x%lx (local 0x%lx) has current state \"%s\"\n",
                   (unsigned long) app_thread_name,
                   (unsigned long) tid, unparse_run_state (info.run_state));
  printf_filtered ("Thread 0x%lx has a suspend count of %d",
                   (unsigned long) app_thread_name, info.suspend_count);
  if (info.sleep_time == 0)
    {
      printf_filtered (".\n");
    }
  else
    {
      printf_filtered (", and has been sleeping for %d seconds.\n",
                       info.sleep_time);
    }
}

void
info_task_command (char *args, int from_tty)
{
  struct task_basic_info info;
  unsigned int info_count = TASK_BASIC_INFO_COUNT;

  thread_array_t thread_list = NULL;
  unsigned int nthreads = 0;

  kern_return_t kret;
  unsigned int i;

  kret =
    task_info (macosx_status->task, TASK_BASIC_INFO, (task_info_t) & info,
               &info_count);
  MACH_CHECK_ERROR (kret);

  printf_filtered ("Inferior task 0x%lx has a suspend count of %d.\n",
                   (unsigned long) macosx_status->task, info.suspend_count);

  kret = task_threads (macosx_status->task, &thread_list, &nthreads);
  MACH_CHECK_ERROR (kret);

  printf_filtered ("The task has %lu threads:\n", (unsigned long) nthreads);
  for (i = 0; i < nthreads; i++)
    {
      print_thread_info (thread_list[i]);
    }

  kret =
    vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
                   (nthreads * sizeof (int)));
  MACH_CHECK_ERROR (kret);
}

static thread_t
parse_thread (char *tidstr)
{
  ptid_t ptid;

  if (ptid_equal (inferior_ptid, null_ptid))
    {
      error ("The program being debugged is not being run.");
    }

  if (tidstr != NULL)
    {

      int num = atoi (tidstr);
      ptid = thread_id_to_pid (num);

      if (ptid_equal (ptid, minus_one_ptid))
        {
          error
            ("Thread ID %d not known.  Use the \"info threads\" command to\n"
             "see the IDs of currently known threads.", num);
        }
    }
  else
    {
      ptid = inferior_ptid;
    }

  if (!target_thread_alive (ptid))
    {
      error ("Thread ID %s does not exist.\n", target_pid_to_str (ptid));
    }

  return ptid_get_tid (ptid);
}

void
info_thread_command (char *tidstr, int from_tty)
{
  thread_t thread = parse_thread (tidstr);
  print_thread_info (thread);
}

static void
thread_suspend_command (char *tidstr, int from_tty)
{
  kern_return_t kret;
  thread_t thread;

  thread = parse_thread (tidstr);
  kret = thread_suspend (thread);

  MACH_CHECK_ERROR (kret);
}

static void
thread_resume_command (char *tidstr, int from_tty)
{
  kern_return_t kret;
  thread_t tid;
  struct thread_basic_info info;
  unsigned int info_count = THREAD_BASIC_INFO_COUNT;

  tid = parse_thread (tidstr);

  kret =
    thread_info (tid, THREAD_BASIC_INFO, (thread_info_t) & info, &info_count);
  MACH_CHECK_ERROR (kret);
  if (info.suspend_count == 0)
    error ("Attempt to resume a thread with suspend count of 0");

  kret = thread_resume (tid);

  MACH_CHECK_ERROR (kret);
}

void
_initialize_threads ()
{
  add_cmd ("suspend", class_run, thread_suspend_command,
           "Increment the suspend count of a thread.", &thread_cmd_list);

  add_cmd ("resume", class_run, thread_resume_command,
           "Decrement the suspend count of a thread.", &thread_cmd_list);

  add_info ("thread", info_thread_command, "Get information on thread.");

  add_info ("task", info_task_command, "Get information on task.");
}
