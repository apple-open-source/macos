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

/* The name of the ppc_thread_state structure, and the names of its
   members, have been changed for Unix conformance reasons.  The easiest
   way to have gdb build on systems with the older names and systems
   with the newer names is to build this compilation unit with the
   non-conformant define below.  This doesn't seem to cause the resulting
   binary any problems but it seems like it could cause us problems in
   the future.  It'd be good to remove this at some point when compiling on
   Tiger is no longer important.  */

#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcore.h"
#include "value.h"
#include "gdbcmd.h"
#include "inferior.h"

#include <sys/param.h>
#include <sys/sysctl.h>

#include "macosx-nat-mutils.h"
#include "macosx-nat-inferior.h"
#include "macosx-nat-inferior-debug.h"

extern macosx_inferior_status *macosx_status;

#define CHECK_ARGS(what, args) \
{ if ((NULL == args) || ((args[0] != '0') && (args[1] != 'x'))) error(what" must be specified with 0x..."); }

#define PRINT_FIELD(structure, field) \
printf_unfiltered(#field":\t%#lx\n", (unsigned long) (structure)->field)

#define task_self mach_task_self
#define port_names mach_port_names
#define task_by_unix_pid task_for_pid
#define port_name_array_t mach_port_array_t
#define port_type_array_t mach_port_array_t

static void
info_mach_tasks_command (char *args, int from_tty)
{
  int sysControl[4];
  int count, index;
  size_t length;
  struct kinfo_proc *procInfo;

  sysControl[0] = CTL_KERN;
  sysControl[1] = KERN_PROC;
  sysControl[2] = KERN_PROC_ALL;

  sysctl (sysControl, 3, NULL, &length, NULL, 0);
  procInfo = (struct kinfo_proc *) xmalloc (length);
  sysctl (sysControl, 3, procInfo, &length, NULL, 0);

  count = (length / sizeof (struct kinfo_proc));
  printf_unfiltered ("%d processes:\n", count);
  for (index = 0; index < count; ++index)
    {
      kern_return_t result;
      mach_port_t taskPort;

      result =
        task_by_unix_pid (mach_task_self (), procInfo[index].kp_proc.p_pid,
                          &taskPort);
      if (KERN_SUCCESS == result)
        {
          printf_unfiltered ("    %s is %d has task %#x\n",
                             procInfo[index].kp_proc.p_comm,
                             procInfo[index].kp_proc.p_pid, taskPort);
        }
      else
        {
          printf_unfiltered ("    %s is %d unknown task port\n",
                             procInfo[index].kp_proc.p_comm,
                             procInfo[index].kp_proc.p_pid);
        }
    }

  xfree (procInfo);
}

static void
info_mach_task_command (char *args, int from_tty)
{
  union
  {
    struct task_basic_info basic;
    struct task_events_info events;
    struct task_thread_times_info thread_times;
  } task_info_data;

  kern_return_t result;
  unsigned int info_count;
  task_t task;

  CHECK_ARGS ("Task", args);
  sscanf (args, "0x%x", &task);

  printf_unfiltered ("TASK_BASIC_INFO:\n");
  info_count = TASK_BASIC_INFO_COUNT;
  result = task_info (task,
                      TASK_BASIC_INFO,
                      (task_info_t) & task_info_data.basic, &info_count);
  MACH_CHECK_ERROR (result);

  PRINT_FIELD (&task_info_data.basic, suspend_count);
  PRINT_FIELD (&task_info_data.basic, virtual_size);
  PRINT_FIELD (&task_info_data.basic, resident_size);
#if 0
  PRINT_FIELD (&task_info_data.basic, user_time);
  PRINT_FIELD (&task_info_data.basic, system_time);
  printf_unfiltered ("\nTASK_EVENTS_INFO:\n");
  info_count = TASK_EVENTS_INFO_COUNT;
  result = task_info (task,
                      TASK_EVENTS_INFO,
                      (task_info_t) & task_info_data.events, &info_count);
  MACH_CHECK_ERROR (result);

  PRINT_FIELD (&task_info_data.events, faults);
  PRINT_FIELD (&task_info_data.events, zero_fills);
  PRINT_FIELD (&task_info_data.events, reactivations);
  PRINT_FIELD (&task_info_data.events, pageins);
  PRINT_FIELD (&task_info_data.events, cow_faults);
  PRINT_FIELD (&task_info_data.events, messages_sent);
  PRINT_FIELD (&task_info_data.events, messages_received);
#endif
  printf_unfiltered ("\nTASK_THREAD_TIMES_INFO:\n");
  info_count = TASK_THREAD_TIMES_INFO_COUNT;
  result = task_info (task,
                      TASK_THREAD_TIMES_INFO,
                      (task_info_t) & task_info_data.thread_times,
                      &info_count);
  MACH_CHECK_ERROR (result);

#if 0
  PRINT_FIELD (&task_info_data.thread_times, user_time);
  PRINT_FIELD (&task_info_data.thread_times, system_time);
#endif
}

static void
info_mach_ports_command (char *args, int from_tty)
{
  port_name_array_t port_names_data;
  port_type_array_t port_types_data;
  unsigned int name_count, type_count;
  kern_return_t result;
  int index;
  task_t task;

  CHECK_ARGS ("Task", args);
  sscanf (args, "0x%x", &task);

  result = port_names (task,
                       &port_names_data,
                       &name_count, &port_types_data, &type_count);
  MACH_CHECK_ERROR (result);

  CHECK_FATAL (name_count == type_count);

  printf_unfiltered ("Ports for task %#x:\n", task);
  for (index = 0; index < name_count; ++index)
    {
      printf_unfiltered ("port name: %#x, type %#x\n",
                         port_names_data[index], port_types_data[index]);
    }

  vm_deallocate (task_self (), (vm_address_t) port_names_data,
                 (name_count * sizeof (mach_port_t)));
  vm_deallocate (task_self (), (vm_address_t) port_types_data,
                 (type_count * sizeof (mach_port_type_t)));
}

static void
info_mach_port_command (char *args, int from_tty)
{
  task_t task;
  mach_port_t port;

  CHECK_ARGS ("Task and port", args);
  sscanf (args, "0x%x 0x%x", &task, &port);

  macosx_debug_port_info (task, port);
}

static void
info_mach_threads_command (char *args, int from_tty)
{
  thread_array_t thread_array;
  unsigned int thread_count;
  kern_return_t result;
  task_t task;
  int i;

  CHECK_ARGS ("Task", args);
  sscanf (args, "0x%x", &task);

  result = task_threads (task, &thread_array, &thread_count);
  MACH_CHECK_ERROR (result);

  printf_unfiltered ("Threads in task %#x:\n", task);
  for (i = 0; i < thread_count; ++i)
    {
      printf_unfiltered ("    %#x\n", thread_array[i]);
    }

  vm_deallocate (task_self (), (vm_address_t) thread_array,
                 (thread_count * sizeof (thread_t)));
}

static void
info_mach_thread_command (char *args, int from_tty)
{
  union
  {
    struct thread_basic_info basic;
  } thread_info_data;

  thread_t thread;
  kern_return_t result;
  unsigned int info_count;

  CHECK_ARGS ("Thread", args);
  sscanf (args, "0x%x", &thread);

  printf_unfiltered ("THREAD_BASIC_INFO\n");
  info_count = THREAD_BASIC_INFO_COUNT;
  result = thread_info (thread,
                        THREAD_BASIC_INFO,
                        (thread_info_t) & thread_info_data.basic,
                        &info_count);
  MACH_CHECK_ERROR (result);

#if 0
  PRINT_FIELD (&thread_info_data.basic, user_time);
  PRINT_FIELD (&thread_info_data.basic, system_time);
#endif
  PRINT_FIELD (&thread_info_data.basic, cpu_usage);
  PRINT_FIELD (&thread_info_data.basic, run_state);
  PRINT_FIELD (&thread_info_data.basic, flags);
  PRINT_FIELD (&thread_info_data.basic, suspend_count);
  PRINT_FIELD (&thread_info_data.basic, sleep_time);

#ifdef __ppc__
  {
    union
    {
      struct __darwin_ppc_thread_state thread;
      struct __darwin_ppc_exception_state exception;
    } thread_state;
    int register_count, i;
    unsigned int *register_data;

    info_count = PPC_THREAD_STATE_COUNT;
    result = thread_get_state (thread,
                               PPC_THREAD_STATE,
                               (thread_state_t) & thread_state.thread,
                               &info_count);
    MACH_CHECK_ERROR (result);

    printf_unfiltered ("\nPPC_THREAD_STATE \n");
    register_data = &thread_state.thread.__r0;
    register_count = 0;
    for (i = 0; i < 8; ++i)
      {
        printf_unfiltered ("r%02d: 0x%08x    ", register_count++,
                           *register_data++);
        printf_unfiltered ("r%02d: 0x%08x    ", register_count++,
                           *register_data++);
        printf_unfiltered ("r%02d: 0x%08x    ", register_count++,
                           *register_data++);
        printf_unfiltered ("r%02d: 0x%08x\n", register_count++,
                           *register_data++);
      }

    printf_unfiltered ("srr0: 0x%08x    srr1: 0x%08x\n",
                       thread_state.thread.__srr0, thread_state.thread.__srr1);
    printf_unfiltered ("cr:   0x%08x    xer:  0x%08x\n",
                       thread_state.thread.__cr, thread_state.thread.__xer);
    printf_unfiltered ("lr:   0x%08x    ctr:  0x%08x\n",
                       thread_state.thread.__lr, thread_state.thread.__ctr);
  }
#endif
}

void
info_mach_regions_command (char *exp, int from_tty)
{
  if ((!macosx_status) || (macosx_status->task == TASK_NULL))
    {
      error ("Inferior not available");
    }

  macosx_debug_regions (macosx_status->task, 0, -1);
}

void
info_mach_region_command (char *exp, int from_tty)
{
  struct expression *expr;
  struct value *val;

  vm_address_t address;

  expr = parse_expression (exp);
  val = evaluate_expression (expr);
  if (TYPE_CODE (value_type (val)) == TYPE_CODE_REF)
    {
      val = value_ind (val);
    }
  /* In rvalue contexts, such as this, functions are coerced into
     pointers to functions. */
  if (TYPE_CODE (value_type (val)) == TYPE_CODE_FUNC
      && VALUE_LVAL (val) == lval_memory)
    {
      address = VALUE_ADDRESS (val);
    }
  else
    {
      address = value_as_address (val);
    }

  if ((!macosx_status) || (macosx_status->task == TASK_NULL))
    {
      error ("Inferior not available");
    }

  macosx_debug_region (macosx_status->task, address);
}

void
_initialize_macosx_info_commands (void)
{
  add_info ("mach-tasks", info_mach_tasks_command,
            "Get list of tasks in system.");
  add_info ("mach-ports", info_mach_ports_command,
            "Get list of ports in a task.");
  add_info ("mach-port", info_mach_port_command,
            "Get info on a specific port.");
  add_info ("mach-task", info_mach_task_command,
            "Get info on a specific task.");
  add_info ("mach-threads", info_mach_threads_command,
            "Get list of threads in a task.");
  add_info ("mach-thread", info_mach_thread_command,
            "Get info on a specific thread.");

  add_info ("mach-regions", info_mach_regions_command,
            "Get information on all mach region for the current inferior.");
  add_info ("mach-region", info_mach_region_command,
            "Get information on mach region at given address.");
}
