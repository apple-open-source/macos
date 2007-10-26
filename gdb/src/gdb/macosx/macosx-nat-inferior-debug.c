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
#include "gdbcore.h"
#include "gdbthread.h"

#include "bfd.h"

#include <sys/ptrace.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include <string.h>
#include <ctype.h>

#include "macosx-nat-inferior-debug.h"
#include "macosx-nat-dyld.h"
#include "macosx-nat-inferior.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-sigthread.h"
#include "macosx-nat-threads.h"

#include "macosx-xdep.h"

#include <AvailabilityMacros.h>

#include <mach/mach_vm.h>

FILE *inferior_stderr = NULL;
/* This controls output of inferior debugging.
   1 = basic exception handling
   2 = task management
   3 = thread management
   4 = pending_event_handler
   6 = most chatty level  */

int inferior_debug_flag = 0;
int timestamps_debug_flag = 0;

void
inferior_debug (int level, const char *fmt, ...)
{
  va_list ap;
  if (inferior_debug_flag >= level)
    {
      va_start (ap, fmt);
      fprintf (inferior_stderr, "[%d inferior]: ", getpid ());
      vfprintf (inferior_stderr, fmt, ap);
      va_end (ap);
      fflush (inferior_stderr);
    }
}

char unknown_exception_buf[32];

const char *
unparse_exception_type (unsigned int i)
{
  switch (i)
    {
    case EXC_BAD_ACCESS:
      return "EXC_BAD_ACCESS";
    case EXC_BAD_INSTRUCTION:
      return "EXC_BAD_INSTRUCTION";
    case EXC_ARITHMETIC:
      return "EXC_ARITHMETIC";
    case EXC_EMULATION:
      return "EXC_EMULATION";
    case EXC_SOFTWARE:
      return "EXC_SOFTWARE";
    case EXC_BREAKPOINT:
      return "EXC_BREAKPOINT";
    case EXC_SYSCALL:
      return "EXC_SYSCALL";
    case EXC_MACH_SYSCALL:
      return "EXC_MACH_SYSCALL";
    case EXC_RPC_ALERT:
      return "EXC_RPC_ALERT";
#ifdef EXC_CRASH
    case EXC_CRASH:
      return "EXC_CRASH";
#endif
    default:
      snprintf (unknown_exception_buf, 32, "??? (%d)", i);
      return unknown_exception_buf;
    }
}

const char *
unparse_protection (vm_prot_t p)
{
  switch (p)
    {
    case VM_PROT_NONE:
      return "---";
    case VM_PROT_READ:
      return "r--";
    case VM_PROT_WRITE:
      return "-w-";
    case VM_PROT_READ | VM_PROT_WRITE:
      return "rw-";
    case VM_PROT_EXECUTE:
      return "--x";
    case VM_PROT_EXECUTE | VM_PROT_READ:
      return "r-x";
    case VM_PROT_EXECUTE | VM_PROT_WRITE:
      return "-wx";
    case VM_PROT_EXECUTE | VM_PROT_WRITE | VM_PROT_READ:
      return "rwx";
    default:
      return "???";
    }
}

const char *
unparse_inheritance (vm_inherit_t i)
{
  switch (i)
    {
    case VM_INHERIT_SHARE:
      return "share";
    case VM_INHERIT_COPY:
      return "copy";
    case VM_INHERIT_NONE:
      return "none";
    default:
      return "???";
    }
}

void
macosx_debug_region (task_t task, mach_vm_address_t address)
{
  macosx_debug_regions (task, address, 1);
}

void
macosx_debug_regions (task_t task, mach_vm_address_t address, int max)
{
  kern_return_t kret;
  vm_region_basic_info_data_64_t info, prev_info;
  mach_vm_address_t prev_address;
  mach_vm_size_t size, prev_size;

  mach_port_t object_name;
  mach_msg_type_number_t count;

  int nsubregions = 0;
  int num_printed = 0;

  count = VM_REGION_BASIC_INFO_COUNT_64;
  kret = mach_vm_region (task, &address, &size, VM_REGION_BASIC_INFO_64,
			 (vm_region_info_t) &info, &count, &object_name);
  if (kret != KERN_SUCCESS)
    {
      printf_filtered ("No memory regions.");
      return;
    }
  memcpy (&prev_info, &info, sizeof (vm_region_basic_info_data_64_t));
  prev_address = address;
  prev_size = size;
  nsubregions = 1;

  for (;;)
    {
      int print = 0;
      int done = 0;

      address = prev_address + prev_size;

      /* Check to see if address space has wrapped around. */
      if (address == 0)
        print = done = 1;

      if (!done)
        {
          count = VM_REGION_BASIC_INFO_COUNT_64;
          kret =
            mach_vm_region (task, &address, &size, VM_REGION_BASIC_INFO_64,
                 	      (vm_region_info_t) &info, &count, &object_name);
          if (kret != KERN_SUCCESS)
            {
              size = 0;
              print = done = 1;
            }
        }

      if (address != prev_address + prev_size)
        print = 1;

      if ((info.protection != prev_info.protection)
          || (info.max_protection != prev_info.max_protection)
          || (info.inheritance != prev_info.inheritance)
          || (info.shared != prev_info.reserved)
          || (info.reserved != prev_info.reserved))
        print = 1;

      if (print)
        {
          if (num_printed == 0)
            printf_filtered ("Region ");
          else
            printf_filtered ("   ... ");

          printf_filtered ("from 0x%s to 0x%s (%s, max %s; %s, %s, %s)",
                           paddr_nz (prev_address),
                           paddr_nz (prev_address + prev_size),
                           unparse_protection (prev_info.protection),
                           unparse_protection (prev_info.max_protection),
                           unparse_inheritance (prev_info.inheritance),
                           prev_info.shared ? "shared" : "private",
                           prev_info.reserved ? "reserved" : "not-reserved");

          if (nsubregions > 1)
            printf_filtered (" (%d sub-regions)", nsubregions);

          printf_filtered ("\n");

          prev_address = address;
          prev_size = size;
          memcpy (&prev_info, &info, sizeof (vm_region_basic_info_data_64_t));
          nsubregions = 1;

          num_printed++;
        }
      else
        {
          prev_size += size;
          nsubregions++;
        }

      if ((max > 0) && (num_printed >= max))
        done = 1;

      if (done)
        break;
    }
}

void
macosx_debug_port_info (task_t task, mach_port_t port)
{
#if 0
  kern_return_t kret;
  mach_port_status_t status;

  kret = mach_port_get_receive_status (task, port, &status);
  MACH_CHECK_ERROR (kret);

  printf_unfiltered ("Port 0x%lx in task 0x%lx:\n", (unsigned long) port,
                     (unsigned long) task);
  printf_unfiltered ("  port set: 0x%lx\n", status.mps_pset);
  printf_unfiltered ("     seqno: 0x%lx\n", status.mps_seqno);
  printf_unfiltered ("   mscount: 0x%lx\n", status.mps_mscount);
  printf_unfiltered ("    qlimit: 0x%lx\n", status.mps_qlimit);
  printf_unfiltered ("  msgcount: 0x%lx\n", status.mps_msgcount);
  printf_unfiltered ("  sorights: 0x%lx\n", status.mps_sorights);
  printf_unfiltered ("   srights: 0x%lx\n", status.mps_srights);
  printf_unfiltered (" pdrequest: 0x%lx\n", status.mps_pdrequest);
  printf_unfiltered (" nsrequest: 0x%lx\n", status.mps_nsrequest);
  printf_unfiltered ("     flags: 0x%lx\n", status.mps_flags);
#endif /* 0 */
}

void
macosx_debug_task_port_info (mach_port_t task)
{
#if 0

  kern_return_t ret;
  unsigned int i;
  port_name_array_t names;
  port_type_array_t types;
  unsigned int nnames, ntypes;

  if (!inferior_debug_flag)
    {
      return;
    }

  ret = port_names (task, &names, &nnames, &types, &ntypes);
  MACH_WARN_ERROR (ret);
  if (ret != KERN_SUCCESS)
    {
      return;
    }

  CHECK_FATAL (nnames == ntypes);

  fprintf (inferior_stderr,
           "macosx_debug_task_port_info: ports for task 0x%lx:\n",
           (long) task);
  for (i = 0; i < nnames; i++)
    {
      char *s = NULL;
      switch (types[i])
        {
        case PORT_TYPE_SEND:
          s = "SEND";
          break;
        case PORT_TYPE_RECEIVE_OWN:
          s = "RECEIVE_OWN";
          break;
        case PORT_TYPE_SET:
          s = "SET";
          break;
        default:
          s = "[UNKNOWN]";
          break;
        }
      fprintf (inferior_stderr, "  0x%lx: %s\n", (long) names[i], s);
    }

  ret =
    vm_deallocate (task_self (), (vm_address_t) names,
                   nnames * sizeof (port_name_t));
  MACH_WARN_ERROR (ret);

  ret =
    vm_deallocate (task_self (), (vm_address_t) types,
                   ntypes * sizeof (port_type_t));
  MACH_WARN_ERROR (ret);

#endif /* 0 */
}

void
macosx_debug_inferior_status (macosx_inferior_status *s)
{
  kern_return_t ret;
  thread_array_t thread_list;
  unsigned int thread_count;
  unsigned int i;

  fprintf (inferior_stderr,
           "macosx_debug_inferior_status: current status:\n");
  fprintf (inferior_stderr, "              inferior task: 0x%lx\n",
           (unsigned long) s->task);

  macosx_signal_thread_debug (inferior_stderr, &s->signal_status);

  fprintf (inferior_stderr,
           "macosx_debug_inferior_status: information on debugger task:\n");
  macosx_debug_task_port_info (mach_task_self ());

  fprintf (inferior_stderr,
           "macosx_debug_inferior_status: information on inferior task:\n");
  macosx_debug_task_port_info (s->task);

  fprintf (inferior_stderr,
           "macosx_debug_inferior_status: information on debugger threads:\n");
  ret = task_threads (mach_task_self (), &thread_list, &thread_count);
  MACH_CHECK_ERROR (ret);

  for (i = 0; i < thread_count; i++)
    {
      fprintf (inferior_stderr, "  thread: 0x%lx\n", (long) thread_list[i]);
    }

  ret = vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
                       (vm_size_t) (thread_count * sizeof (thread_t)));
  MACH_CHECK_ERROR (ret);

  fprintf (inferior_stderr,
           "macosx_debug_inferior_status: information on inferior threads:\n");
  ret = task_threads (s->task, &thread_list, &thread_count);
  MACH_CHECK_ERROR (ret);

  for (i = 0; i < thread_count; i++)
    {
      fprintf (inferior_stderr, "  thread: 0x%lx\n", (long) thread_list[i]);
    }

  ret = vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
                       (vm_size_t) (thread_count * sizeof (thread_t)));
  MACH_CHECK_ERROR (ret);

  fflush (inferior_stderr);     /* just in case we are talking to a pipe */
}

void
macosx_debug_message (mach_msg_header_t * msg)
{
  if (!inferior_debug_flag)
    {
      return;
    }
  fprintf (inferior_stderr,
           "[%d inferior]: macosx_debug_message: message contents:\n",
           getpid ());
  fprintf (inferior_stderr, "        msgh_bits: 0x%lx\n",
           (long) msg->msgh_bits);
  fprintf (inferior_stderr, "        msgh_size: 0x%lx\n",
           (long) msg->msgh_size);
  fprintf (inferior_stderr, " msgh_remote_port: 0x%lx\n",
           (long) msg->msgh_remote_port);
  fprintf (inferior_stderr, "  msgh_local_port: 0x%lx\n",
           (long) msg->msgh_local_port);
  fprintf (inferior_stderr, "    msgh_reserved: 0x%lx\n",
           (long) msg->msgh_reserved);
  fprintf (inferior_stderr, "          msgh_id: 0x%lx\n",
           (long) msg->msgh_id);
}

void
macosx_debug_notification_message (struct macosx_inferior_status *inferior,
                                   mach_msg_header_t * msg)
{
  if (msg->msgh_id == MACH_NOTIFY_PORT_DELETED)
    {
      mach_port_deleted_notification_t *dmsg =
        (mach_port_deleted_notification_t *) msg;
      if (dmsg->not_port == inferior->task)
        {
          inferior_debug (2,
                          "macosx_process_message: deletion message for task port 0x%lx\n",
                          (unsigned long) dmsg->not_port);
        }
      else
        {
          inferior_debug (2,
                          "macosx_process_message: deletion message for unknown port 0x%lx; ignoring\n",
                          (unsigned long) dmsg->not_port);
        }
    }
  else
    {
      warning
        ("macosx_process_message: unknown notification type 0x%lx; ignoring",
         (unsigned long) msg->msgh_id);
    }
}

void
_initialize_macosx_inferior_debug ()
{
  add_setshow_boolean_cmd ("timestamps", class_obscure,
			   &timestamps_debug_flag, _("\
Set if GDB print timestamps before any terminal output."), _("\
Show if GDB print timestamps before any terminal output."), NULL,
			   NULL, NULL,
			   &setdebuglist, &showdebuglist);

  add_setshow_zinteger_cmd ("inferior", class_obscure,
			    &inferior_debug_flag, _("\
Set if printing inferior communication debugging statements."), _("\
Show if printing inferior communication debugging statements."), NULL,
			    NULL, NULL,
			    &setdebuglist, &showdebuglist);
}
