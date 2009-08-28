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

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "gdbthread.h"
#include "gdbcore.h"

#include <stdio.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <inttypes.h>

#include <libproc.h>
#include <sys/proc_info.h>

#include "macosx-nat-inferior.h"
#include "macosx-nat-inferior-util.h"
#include "macosx-nat-inferior-debug.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-infthread.h"

extern macosx_inferior_status *macosx_status;

static char *get_dispatch_queue_name (CORE_ADDR dispatch_qaddr);
static int get_dispatch_queue_flags (CORE_ADDR dispatch_qaddr, uint32_t *flags);

#define set_trace_bit(thread) modify_trace_bit (thread, 1)
#define clear_trace_bit(thread) modify_trace_bit (thread, 0)

/* We use this structure in iterate_over_threads to prune the
   thread list.  */
struct ptid_list
{
  unsigned int nthreads;
  ptid_t *ptids;
};

void
macosx_setup_registers_before_hand_call ()
{
  thread_t current_thread = ptid_get_tid (inferior_ptid);

  thread_abort_safely (current_thread);
}

#if defined (TARGET_I386)

/* Set/clear bit 8 (Trap Flag) of the EFLAGS processor control
   register to enable/disable single-step mode.  Handle new-style 
   32-bit, 64-bit, and old-style 32-bit interfaces in this function.  
   VALUE is a boolean, indicating whether to set (1) the Trap Flag 
   or clear it (0).  */

kern_return_t
modify_trace_bit (thread_t thread, int value)
{
  gdb_x86_thread_state_t state;
  unsigned int state_count = GDB_x86_THREAD_STATE_COUNT;
  kern_return_t kret;

  kret = thread_get_state (thread, GDB_x86_THREAD_STATE, 
                           (thread_state_t) &state, &state_count);
  if (kret == KERN_SUCCESS &&
      (state.tsh.flavor == GDB_x86_THREAD_STATE32 ||
       state.tsh.flavor == GDB_x86_THREAD_STATE64))
    {
      if (state.tsh.flavor == GDB_x86_THREAD_STATE32 
          && (state.uts.ts32.eflags & 0x100UL) != (value ? 1 : 0))
        {
          state.uts.ts32.eflags = 
                    (state.uts.ts32.eflags & ~0x100UL) | (value ? 0x100UL : 0);
          kret = thread_set_state (thread, GDB_x86_THREAD_STATE32, 
                                   (thread_state_t) & state.uts.ts32,
                                   GDB_x86_THREAD_STATE32_COUNT);
          MACH_PROPAGATE_ERROR (kret);
        }
      else if (state.tsh.flavor == GDB_x86_THREAD_STATE64 
               && (state.uts.ts64.rflags & 0x100UL) != (value ? 1 : 0))
        {
          state.uts.ts64.rflags = 
                     (state.uts.ts64.rflags & ~0x100UL) | (value ? 0x100UL : 0);
          kret = thread_set_state (thread, GDB_x86_THREAD_STATE, 
                                   (thread_state_t) &state, state_count);
          MACH_PROPAGATE_ERROR (kret);
        }
    }
  else
    {
      gdb_i386_thread_state_t state;
      
      state_count = GDB_i386_THREAD_STATE_COUNT;
      kret = thread_get_state (thread, GDB_i386_THREAD_STATE, 
                               (thread_state_t) &state, &state_count);
      MACH_PROPAGATE_ERROR (kret);

      if ((state.eflags & 0x100UL) != (value ? 1 : 0))
        {
          state.eflags = (state.eflags & ~0x100UL) | (value ? 0x100UL : 0);
          kret = thread_set_state (thread, GDB_i386_THREAD_STATE, 
                                   (thread_state_t) &state, state_count);
          MACH_PROPAGATE_ERROR (kret);
        }
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

#elif defined (TARGET_ARM) /* ARM HACK: kernel doesn't support hardware single step on ARM.  */
kern_return_t
modify_trace_bit (thread_t thread, int value)
{
  /* abort (); */
  return KERN_SUCCESS;
}
#else
#error "unknown architecture"
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

  kret = task_threads (inferior->task, &thread_list, &nthreads);
  MACH_CHECK_ERROR (kret);

  macosx_check_new_threads (thread_list, nthreads);

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

          if (tp->private->gdb_suspend_count > 0)
            inferior_debug (3, "**  Resuming thread 0x%x, gdb suspend count: "
                            "%d, real suspend count: %d\n",
                            thread_list[i], tp->private->gdb_suspend_count,
                            info.suspend_count);
	  else if (tp->private->gdb_suspend_count < 0)
	    inferior_debug (3, "**  Re-suspending thread 0x%x, original suspend count: "
                            "%d\n",
                            thread_list[i], tp->private->gdb_suspend_count);
          else
            inferior_debug (3, "**  Thread 0x%x was not suspended from gdb, "
                            "real suspend count: %d\n",
                            thread_list[i], info.suspend_count);
        }
      if (tp->private->gdb_suspend_count > 0)
	{
	  while (tp->private->gdb_suspend_count > 0)
	    {
	      thread_resume (thread_list[i]);
	      tp->private->gdb_suspend_count--;
	    }
	}
      else if (tp->private->gdb_suspend_count < 0)
	{
	  while (tp->private->gdb_suspend_count < 0)
	    {
	      thread_suspend (thread_list[i]);
	      tp->private->gdb_suspend_count++;
	    }
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
	    {
	      ptid_t ptid;
	      struct thread_info *tp = NULL;

	      ptid = ptid_build (inferior->pid, 0, current);
	      tp = find_thread_pid (ptid);
	      CHECK_FATAL (tp != NULL);
	      inferior_debug (3, "Resuming to single-step thread 0x%x "
			      "(thread is already suspended from outside of GDB)",
			      current);
	      while (info.suspend_count > 0)
		{
		  tp->private->gdb_suspend_count--;
		  info.suspend_count--;
		  thread_resume (current);
		}
	    }
          else
            error ("Unable to run only thread 0x%x "
                   "(thread is already suspended from outside of GDB)",
                   current);
        }
    }

  kret = task_threads (inferior->task, &thread_list, &nthreads);
  MACH_CHECK_ERROR (kret);

  if (step)
    inferior_debug (3, "*** Suspending threads to step: 0x%x\n", current);

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
          if (tp == NULL)
	    {
	      tp = add_thread (ptid);
	      if (create_private_thread_info (tp))
		tp->private->app_thread_port =
		  get_application_thread_port (thread_list[i]);
	      inferior_debug (3, "*** New thread 0x%x appeared while task was stopped.\n",
			      thread_list[i]);
	    }

	  if (tp->private->gdb_dont_suspend_stepping)
	    {
	      inferior_debug (3, "*** Allowing thread 0x%x to run - it's marked don't suspend.\n",
			      thread_list[i]);
	      continue;
	    }
          kret = thread_suspend (thread_list[i]);
          MACH_CHECK_ERROR (kret);
          tp->private->gdb_suspend_count++;
          inferior_debug (3, "*** Suspending thread 0x%x, suspend count %d\n",
                          thread_list[i], tp->private->gdb_suspend_count);
        }
      else if (stop_others)
        inferior_debug (3, "*** Allowing thread 0x%x to run from gdb\n",
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

/* get_application_thread_port returns the thread port number in the
   application's port namespace.  We get this so that we can present
   the user with the same port number they would see if they store
   away the thread id in their program.  It is for display purposes 
   only.  */

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

  /* To get the application name, we have to iterate over all the ports
     in the application and extract a right for them.  The right will include
     the port name in our port namespace, so we can use that to find the
     thread we are looking for.  Of course, we don't actually need another
     right to each of these ports, so we deallocate it when we are done.  */

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
          mach_port_deallocate (mach_task_self (), local_name);
          if (local_name == our_name)
            {
              match = names[i];
              break;
            }
        }
    }

  vm_deallocate (mach_task_self (), (vm_address_t) names,
                 names_count * sizeof (mach_port_t));
  vm_deallocate (mach_task_self (), (vm_address_t) types,
                 types_count * sizeof (mach_port_t));

  return (thread_t) match;
}

struct dispatch_offsets_info {
  ULONGEST version;              /* dqo_version */
  ULONGEST label_offset;         /* dqo_label */
  ULONGEST label_size;           /* dqo_label_size */
  ULONGEST flags_offset;         /* dqo_flags */
  ULONGEST flags_size;           /* dqo_flags_size */
};


/* libdispatch has a structure (symbol name dispatch_queue_offsets) which
   tells us where to find the name and flags for a work queue in the inferior.
   v. libdispatch's (non-public) src/queue_private.h for the definition of this 
   structure.  */

static struct dispatch_offsets_info *
read_dispatch_offsets ()
{
  struct minimal_symbol *dispatch_queue_offsets;
  static struct dispatch_offsets_info *dispatch_offsets = NULL;
  if (dispatch_offsets != NULL)
    return dispatch_offsets;

  dispatch_queue_offsets = lookup_minimal_symbol 
                           ("dispatch_queue_offsets", NULL, NULL);
  if (dispatch_queue_offsets == NULL
      || SYMBOL_VALUE_ADDRESS (dispatch_queue_offsets) == 0
      || SYMBOL_VALUE_ADDRESS (dispatch_queue_offsets) == -1)
    return NULL;

  dispatch_offsets = (struct dispatch_offsets_info *)
                      xmalloc (sizeof (struct dispatch_offsets_info));

  if (safe_read_memory_unsigned_integer 
       (SYMBOL_VALUE_ADDRESS (dispatch_queue_offsets), 2, 
        &dispatch_offsets->version) == 0)
    {
      xfree (dispatch_offsets);
      dispatch_offsets = NULL;
      return NULL;
    }

  if (safe_read_memory_unsigned_integer 
       (SYMBOL_VALUE_ADDRESS (dispatch_queue_offsets) + 2, 2, 
        &dispatch_offsets->label_offset) == 0)
    {
      xfree (dispatch_offsets);
      dispatch_offsets = NULL;
      return NULL;
    }

  if (safe_read_memory_unsigned_integer 
       (SYMBOL_VALUE_ADDRESS (dispatch_queue_offsets) + 4, 2, 
        &dispatch_offsets->label_size) == 0)
    {
      xfree (dispatch_offsets);
      dispatch_offsets = NULL;
      return NULL;
    }

  if (safe_read_memory_unsigned_integer 
       (SYMBOL_VALUE_ADDRESS (dispatch_queue_offsets) + 6, 2, 
        &dispatch_offsets->flags_offset) == 0)
    {
      xfree (dispatch_offsets);
      dispatch_offsets = NULL;
      return NULL;
    }

  if (safe_read_memory_unsigned_integer 
       (SYMBOL_VALUE_ADDRESS (dispatch_queue_offsets) + 8, 2, 
        &dispatch_offsets->flags_size) == 0)
    {
      xfree (dispatch_offsets);
      dispatch_offsets = NULL;
      return NULL;
    }
  
  return dispatch_offsets;
}

/* Retrieve the libdispatch work queue name given the dispatch_qaddr
   from thread_info (..., THREAD_IDENTIFIER_INFO, ...).  
   Returns NULL if a name could not be found.
   Returns a pointer to a static character buffer if it was found.  */

static char *
get_dispatch_queue_name (CORE_ADDR dispatch_qaddr)
{
  static char namebuf[96];
  struct dispatch_offsets_info *dispatch_offsets = read_dispatch_offsets ();
  int wordsize = TARGET_PTR_BIT / 8;
  ULONGEST queue;

  namebuf[0] = '\0';

  if (dispatch_qaddr != 0 
      && dispatch_offsets != NULL
      && safe_read_memory_unsigned_integer (dispatch_qaddr, wordsize,
                                            &queue) != 0
      && queue != 0)
    {
      char *queue_buf = NULL;
      errno = 0;
      if (target_read_string (queue + dispatch_offsets->label_offset, 
                              &queue_buf, sizeof (namebuf) - 1, &errno) > 1
          && errno == 0)
        {
          if (queue_buf && queue_buf[0] != '\0')
            strlcpy (namebuf, queue_buf, sizeof (namebuf));
        }
      if (queue_buf)
        xfree (queue_buf);
    }
  return namebuf;
}

/* Retrieve the libdispatch work queue flags given the dispatch_qaddr
   from thread_info (..., THREAD_IDENTIFIER_INFO, ...).  
   Returns 1 if it was able to retrieve the flags field.  */

static int
get_dispatch_queue_flags (CORE_ADDR dispatch_qaddr, uint32_t *flags)
{
  int wordsize = TARGET_PTR_BIT / 8;
  struct dispatch_offsets_info *dispatch_offsets = read_dispatch_offsets ();
  ULONGEST queue;
  ULONGEST buf;

  if (flags == NULL)
    return 0;

  if (dispatch_qaddr != 0
      && dispatch_offsets != NULL
      && safe_read_memory_unsigned_integer (dispatch_qaddr, wordsize,
                                            &queue) != 0
      && queue != 0
      && safe_read_memory_unsigned_integer 
                            (queue + dispatch_offsets->flags_offset, 
                             dispatch_offsets->flags_size, &buf) != 0)
    {
      *flags = buf;
      return 1;
    }

  return 0;
}

static void
print_thread_info (thread_t tid, int *gdb_thread_id)
{
  struct thread_basic_info info;
  unsigned int info_count = THREAD_BASIC_INFO_COUNT;
  kern_return_t kret;
  thread_t app_thread_name;
  ptid_t ptid = ptid_build (macosx_status->pid, 0, tid);
  struct thread_info *tp = find_thread_pid (ptid);
  ptid_t current_ptid;
  current_ptid = inferior_ptid;

  kret =
    thread_info (tid, THREAD_BASIC_INFO, (thread_info_t) &info, &info_count);
  MACH_CHECK_ERROR (kret);

  if (tp == NULL)
    app_thread_name = get_application_thread_port (tid);
  else
    app_thread_name = tp->private->app_thread_port;

  if (gdb_thread_id)
    {
      printf_filtered ("Thread %d has current state \"%s\"\n", 
                       *gdb_thread_id, unparse_run_state (info.run_state));
      printf_filtered ("\tMach port #0x%s (gdb port #0x%s)\n",
                       paddr_nz (app_thread_name), paddr_nz (tid));
    }
  else
    {
      printf_filtered ("Port # 0x%lx (gdb port # 0x%lx) has current state \"%s\"\n",
                       (unsigned long) app_thread_name,
                       (unsigned long) tid, unparse_run_state (info.run_state));
    }

    printf_filtered ("\tframe 0: ");
    switch_to_thread (tp->ptid);
    print_stack_frame (get_selected_frame (NULL), 0, LOCATION);
    switch_to_thread (current_ptid);

#ifdef HAVE_THREAD_IDENTIFIER_INFO_DATA_T
  thread_identifier_info_data_t tident;
  info_count = THREAD_IDENTIFIER_INFO_COUNT;
  kret = thread_info (tid, THREAD_IDENTIFIER_INFO, (thread_info_t) &tident, 
                      &info_count);
  MACH_CHECK_ERROR (kret);

  printf_filtered ("\tpthread ID: 0x%s\n",
                   paddr_nz (tident.thread_handle));
  printf_filtered ("\tsystem-wide unique thread id: 0x%s\n", 
                   paddr_nz (tident.thread_id));

  /* If the pthread_self() value for this thread is 0x0, we have a thread
     that is very early in the setup process (e.g. it is in 
     _pthread_struct_init () or something like that) -- it most likely 
     has a bogus dispatch_qaddr value.  Save ourself a couple of 
     memory_error()s and just skip this processing.  */
  if (tident.thread_handle != 0)
    {
      char *queue_name = get_dispatch_queue_name (tident.dispatch_qaddr);
      if (queue_name && queue_name[0] != '\0')
        printf_filtered ("\tdispatch queue name: \"%s\"\n", queue_name);

      uint32_t queue_flags;
      if (get_dispatch_queue_flags (tident.dispatch_qaddr, &queue_flags))
        {
                printf_filtered ("\tdispatch queue flags: 0x%x", queue_flags);
                /* Constants defined in libdispatch's src/private.h,
                   dispatch_queue_flags_t */
                if (queue_flags & 0x1)
                  printf_filtered (" (concurrent)");
                if (queue_flags & 0x4)
                  printf_filtered (" (always locked)");
                printf_filtered ("\n");
        }
    }

  struct proc_threadinfo pth;
  int retval;
  retval = proc_pidinfo (PIDGET (ptid), PROC_PIDTHREADINFO, 
                         tident.thread_handle, &pth, sizeof (pth));
  if (retval != 0)
    {
      if (pth.pth_name[0] != '\0')
        printf_filtered ("\tthread name: \"%s\"\n", pth.pth_name);

      printf_filtered ("\ttotal user time: 0x%" PRIx64 "\n", pth.pth_user_time);
      printf_filtered ("\ttotal system time: 0x%" PRIx64 "\n", pth.pth_system_time);
      printf_filtered ("\tscaled cpu usage percentage: %d\n", pth.pth_cpu_usage);
      printf_filtered ("\tscheduling policy in effect: 0x%x\n", pth.pth_policy);
      printf_filtered ("\trun state: 0x%x", pth.pth_run_state);
      switch (pth.pth_run_state) {
          case TH_STATE_RUNNING: printf_filtered (" (RUNNING)\n"); break;
          case TH_STATE_STOPPED: printf_filtered (" (STOPPED)\n"); break;
          case TH_STATE_WAITING: printf_filtered (" (WAITING)\n"); break;
          case TH_STATE_UNINTERRUPTIBLE: printf_filtered (" (UNINTERRUPTIBLE)\n"); break;
          case TH_STATE_HALTED: printf_filtered (" (HALTED)\n"); break;
          default: printf_filtered ("\n");
      }
      printf_filtered ("\tflags: 0x%x", pth.pth_flags);
      switch (pth.pth_flags) {
          case TH_FLAGS_SWAPPED: printf_filtered (" (SWAPPED)\n"); break;
          case TH_FLAGS_IDLE: printf_filtered (" (IDLE)\n"); break;
          default: printf_filtered ("\n");
      }
      printf_filtered ("\tnumber of seconds that thread has slept: %d\n", 
                                                   pth.pth_sleep_time);
      printf_filtered ("\tcurrent priority: %d\n", pth.pth_priority);
      printf_filtered ("\tmax priority: %d\n", pth.pth_maxpriority);
    }
#endif /* HAVE_THREAD_IDENTIFIER_INFO_DATA_T */

  printf_filtered ("\tsuspend count: %d", info.suspend_count);

  if (info.sleep_time == 0)
    {
      printf_filtered (".\n");
    }
  else
    {
      printf_filtered (", and has been sleeping for %d seconds.\n",
                       info.sleep_time);
    }
  if (tp == NULL)
    printf_filtered ("\tCould not find the thread in gdb's internal thread list.\n");
  else if (tp->private->gdb_dont_suspend_stepping)
    printf_filtered ("\tSet to run while stepping.\n");
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
      print_thread_info (thread_list[i], NULL);
    }

  kret =
    vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
                   (nthreads * sizeof (int)));
  MACH_CHECK_ERROR (kret);
}

static thread_t
parse_thread (char *tidstr, int *gdb_thread_id)
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

  if (gdb_thread_id)
    *gdb_thread_id = pid_to_thread_id (ptid);

  return ptid_get_tid (ptid);
}

void
info_thread_command (char *tidstr, int from_tty)
{
  int gdb_thread_id;
  thread_t thread = parse_thread (tidstr, &gdb_thread_id);
  print_thread_info (thread, &gdb_thread_id);
}

static void
thread_suspend_command (char *tidstr, int from_tty)
{
  kern_return_t kret;
  thread_t thread;

  thread = parse_thread (tidstr, NULL);
  kret = thread_suspend (thread);

  MACH_CHECK_ERROR (kret);
}

static int 
thread_match_callback (struct thread_info *t, void *thread_ptr)
{
  LONGEST desired_thread = *(LONGEST *) thread_ptr;

  struct private_thread_info *pt = (struct private_thread_info *) t->private;
  if (pt->app_thread_port == desired_thread)
    return 1;
  else
    return 0;
}

static void
thread_dont_suspend_while_stepping_command (char *arg, int from_tty)
{
  struct thread_info *tp;
  int on_or_off = 0;
#define TDS_ERRSTR "Usage: on|off <THREAD ID>|-port <EXPR>"

  while (*arg == ' ' || *arg == '\t')
    arg++;

  if (*arg == '\0')
    error (TDS_ERRSTR);

  if (strstr (arg, "on") == arg)
    {
      on_or_off = 1;
      arg += 2;
    }
  else if (strstr (arg, "off") == arg)
    {
      on_or_off = 0;
      arg += 3;
    }
  else
    error (TDS_ERRSTR);
    
  while (*arg == ' ' || *arg == '\t')
    arg++;

  if (strstr (arg, "-port") == arg)
    {
      LONGEST thread_no;
      arg += 5;
      while (*arg == ' ' || *arg == '\t')
	arg++;

      if (*arg == '\0')
	error ("No expression of -port flag.");

      thread_no = parse_and_eval_long (arg);
      
      tp = iterate_over_threads (thread_match_callback, &thread_no);
    }
  else
    {
      int threadno;
      char *endptr;

      threadno = strtol (arg, &endptr, 0);
      if (*endptr != '\0')
	error ("Junk at end of thread id: \"%s\".", endptr);
      tp = find_thread_id (threadno);
    }

  if (tp == NULL)
    error ("Couldn't find thread matching: \"%s\".", arg);
  else
    {
      printf_unfiltered ("Setting thread %d to %s while stepping other threads.\n",
			 tp->num,
			 on_or_off ? "run" : "stop");
      tp->private->gdb_dont_suspend_stepping = on_or_off;
    }
}

static void
thread_resume_command (char *tidstr, int from_tty)
{
  kern_return_t kret;
  thread_t tid;
  struct thread_basic_info info;
  unsigned int info_count = THREAD_BASIC_INFO_COUNT;

  tid = parse_thread (tidstr, NULL);

  kret =
    thread_info (tid, THREAD_BASIC_INFO, (thread_info_t) & info, &info_count);
  MACH_CHECK_ERROR (kret);
  if (info.suspend_count == 0)
    error ("Attempt to resume a thread with suspend count of 0");

  kret = thread_resume (tid);

  MACH_CHECK_ERROR (kret);
}

static int
mark_dead_if_thread_is_gone (struct thread_info *tp, void *data)
{
  struct ptid_list *ptids = (struct ptid_list *) data;
  int i;
  int found_it = 0;

  for (i = 0; i < ptids->nthreads; i++)
    {
      if (ptid_equal(tp->ptid, ptids->ptids[i]))
	{
	  found_it = 1;
	  break;
	}      
    }
  if (!found_it)
    {
      tp->ptid = pid_to_ptid (-1);
    }
  return 0;
}

/* This compares the list of threads from task_threads, and gdb's
   current thread list, and removes the ones that are inactive.  It
   works much the same as "prune_threads" except that that function
   ends up calling task_threads for each thread in the thread list,
   which isn't very efficient.  */

void
macosx_prune_threads (thread_array_t thread_list, unsigned int nthreads)
{
  struct ptid_list ptid_list;
  kern_return_t kret;
  unsigned int i;
  int dealloc_thread_list = (thread_list == NULL);

  if (thread_list == NULL)
    {
      kret = task_threads (macosx_status->task, &thread_list, &nthreads);
      MACH_CHECK_ERROR (kret);
    }

  ptid_list.nthreads = nthreads;
  ptid_list.ptids = (ptid_t *) xmalloc (nthreads * sizeof (ptid_t));
 
  for (i = 0; i < nthreads; i++)
    {
      ptid_t ptid = ptid_build (macosx_status->pid, 0, thread_list[i]);
      ptid_list.ptids[i] = ptid;
    }
  if (dealloc_thread_list)
    {
      kret =
	vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
		       (nthreads * sizeof (int)));
      MACH_CHECK_ERROR (kret);
    }

  iterate_over_threads (mark_dead_if_thread_is_gone, &ptid_list);
  prune_threads ();
}

/* Print the details about a thread, intended for an MI-like interface.  */

void 
macosx_print_thread_details (struct ui_out *uiout, ptid_t ptid)
{
  thread_t tid = ptid_get_tid (ptid);
  struct thread_basic_info info;
  unsigned int info_count = THREAD_BASIC_INFO_COUNT;
  kern_return_t kret;
  thread_t app_thread_name;
  struct thread_info *tp = find_thread_pid (ptid);

  kret =
    thread_info (tid, THREAD_BASIC_INFO, (thread_info_t) &info, &info_count);
  MACH_CHECK_ERROR (kret);

  if (tp == NULL)
    app_thread_name = get_application_thread_port (tid);
  else
    app_thread_name = tp->private->app_thread_port;

  ui_out_field_string (uiout, "state", unparse_run_state (info.run_state));
  ui_out_field_fmt (uiout, "mach-port-number", "0x%s", 
                    paddr_nz (app_thread_name));

#ifdef HAVE_THREAD_IDENTIFIER_INFO_DATA_T
  thread_identifier_info_data_t tident;
  info_count = THREAD_IDENTIFIER_INFO_COUNT;
  kret = thread_info (tid, THREAD_IDENTIFIER_INFO, (thread_info_t) &tident, 
                      &info_count);
  MACH_CHECK_ERROR (kret);

  ui_out_field_fmt (uiout, "pthread-id", "0x%s", 
                    paddr_nz (tident.thread_handle));
  ui_out_field_fmt (uiout, "unique-id", "0x%s", 
                    paddr_nz (tident.thread_id));


  struct proc_threadinfo pth;
  int retval;
  retval = proc_pidinfo (PIDGET (ptid), PROC_PIDTHREADINFO, 
                         tident.thread_handle, &pth, sizeof (pth));
  if (retval != 0 && pth.pth_name[0] != '\0')
    ui_out_field_string (uiout, "name", pth.pth_name);

  if (tident.thread_handle != 0)
    {
      char *queue_name = get_dispatch_queue_name (tident.dispatch_qaddr);
      if (queue_name && queue_name[0] != '\0')
        ui_out_field_string (uiout, "workqueue", queue_name);
    }
#endif /* HAVE_THREAD_IDENTIFIER_INFO_DATA_T */
}


void
_initialize_threads ()
{
  add_cmd ("suspend", class_run, thread_suspend_command,
           "Increment the suspend count of a thread.", &thread_cmd_list);

  add_cmd ("resume", class_run, thread_resume_command,
           "Decrement the suspend count of a thread.", &thread_cmd_list);

  add_cmd ("dont-suspend-while-stepping", class_run, thread_dont_suspend_while_stepping_command,
	   "Usage: on|off <THREAD ID>|-port <EXPR>\
Toggle whether to not suspend this thread while single stepping the target on or off.\
With the -port option, EXPR is evaluated as an expression in the target, which should \
resolve to the Mach port number of the thread port for a thread in the target program.", 
	   &thread_cmd_list);

  add_info ("thread", info_thread_command, "Get information on thread.");

  add_info ("task", info_task_command, "Get information on task.");
}
