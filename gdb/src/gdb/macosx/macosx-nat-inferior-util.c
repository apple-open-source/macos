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

#include "macosx-nat-inferior-util.h"
#include "macosx-nat-dyld-process.h"
#include "macosx-nat-inferior-debug.h"

#include "macosx-nat-dyld-info.h"
#include "macosx-nat-dyld-path.h"
#include "macosx-nat-inferior.h"
#include "macosx-nat-mutils.h"

#include "defs.h"
#include "inferior.h"
#include "symfile.h"
#include "symtab.h"
#include "gdbcmd.h"
#include "objfiles.h"

#include <mach/mach.h>

#include <mach-o/nlist.h>
#include <mach-o/loader.h>
#include <mach-o/dyld_debug.h>

#include <string.h>
#include <signal.h>
#include <sys/ptrace.h>

#include "mach-o.h"

const char *ptrace_request_unparse (int request)
{
  switch (request) {
  case PTRACE_TRACEME: return "PTRACE_TRACEME";
  case PTRACE_PEEKTEXT: return "PTRACE_PEEKTEXT";
  case PTRACE_PEEKDATA: return "PTRACE_PEEKDATA";
  case PTRACE_PEEKUSER: return "PTRACE_PEEKUSER";
  case PTRACE_POKETEXT: return "PTRACE_POKETEXT";
  case PTRACE_POKEDATA: return "PTRACE_POKEDATA";
  case PTRACE_POKEUSER: return "PTRACE_POKEUSER";
  case PTRACE_CONT: return "PTRACE_CONT";
  case PTRACE_KILL: return "PTRACE_KILL";
  case PTRACE_SINGLESTEP: return "PTRACE_SINGLESTEP";
  case PTRACE_ATTACH: return "PTRACE_ATTACH";
  case PTRACE_DETACH: return "PTRACE_DETACH";
  case PTRACE_SIGEXC: return "PTRACE_SIGEXC";
  case PTRACE_THUPDATE: return "PTRACE_THUPDATE";
  case PTRACE_ATTACHEXC: return "PTRACE_ATTACHEXC";
  default: return "[UNKNOWN]";
  }
}

int call_ptrace (int request, int pid, int arg3, int arg4)
{
  int ret;
  errno = 0;
  ret = ptrace (request, pid, (caddr_t) arg3, arg4);

  inferior_debug (2, "ptrace (%s, %d, %d, %d): %d (%s)\n",
		  ptrace_request_unparse (request),
		  pid, arg3, arg4, ret, (ret != 0) ? strerror (errno) : "no error");
  return ret;
}

void macosx_inferior_reset (macosx_inferior_status *s)
{
  s->pid = 0;
  s->task = TASK_NULL;

  s->attached_in_ptrace = 0;
  s->stopped_in_ptrace = 0;
  s->stopped_in_softexc = 0;

  s->suspend_count = 0;

  s->last_thread = THREAD_NULL;

  /* Default to uses_dyld.  This really only matters when we go to set the
     dyld breakpoint, and we will always check before then. */

  s->dyld_status.uses_dyld = 1;

  macosx_signal_thread_init (&s->signal_status);

  macosx_exception_thread_init (&s->exception_status);

#if WITH_CFM
  macosx_cfm_thread_init (&s->cfm_status);
#endif /* WITH_CFM */
}

void macosx_inferior_destroy (macosx_inferior_status *s)
{
  macosx_signal_thread_destroy (&s->signal_status);
  macosx_exception_thread_destroy (&s->exception_status);
#if WITH_CFM
  macosx_cfm_thread_destroy (&s->cfm_status);
#endif /* WITH_CFM */
  
  s->task = TASK_NULL;
  s->pid = 0;

  macosx_inferior_reset (s);
}

int macosx_inferior_valid (macosx_inferior_status *s)
{
  kern_return_t kret;
  int ret;

  struct task_basic_info info;
  unsigned int info_count = TASK_BASIC_INFO_COUNT;

  kret = task_info (s->task, TASK_BASIC_INFO, (task_info_t) &info, &info_count);
  if (kret != KERN_SUCCESS) { return 0; }

  ret = kill (s->pid, 0);
  if (ret != 0) { return 0; }

  return 1;
}

void macosx_inferior_check_stopped (macosx_inferior_status *s)
{
  CHECK (s != NULL);

  CHECK (s->task != TASK_NULL);
  CHECK (s->pid != 0);

  if (s->stopped_in_ptrace || s->stopped_in_softexc) { 
    CHECK (s->attached_in_ptrace);
  }

  CHECK ((s->stopped_in_ptrace == 1) || (s->stopped_in_softexc == 1) || (s->suspend_count > 0));
  CHECK ((s->suspend_count == 0) || (s->suspend_count == 1));
}  

kern_return_t macosx_inferior_suspend_mach (macosx_inferior_status *s)
{
  kern_return_t kret;

  CHECK (s != NULL);
  CHECK (macosx_task_valid (s->task));

  if (s->suspend_count == 0) {
    inferior_debug (2, "suspending task\n");
    kret = task_suspend (s->task);
    if (kret != KERN_SUCCESS) {
      return kret;
    }
    s->suspend_count++;
    inferior_debug (2, "suspended task (suspend count now %d)\n", s->suspend_count);
  }

  return KERN_SUCCESS;
}

kern_return_t macosx_inferior_resume_mach (macosx_inferior_status *s, int count)
{
  kern_return_t kret;
  
  CHECK (s != NULL);
  CHECK (macosx_task_valid (s->task));

  for (;;) {
    if (s->suspend_count == 0) {
      break;
    }
    if (count == 0) {
      break;
    }
    inferior_debug (2, "resuming task\n");
    kret = task_resume (s->task);
    if (kret != KERN_SUCCESS) {
      return kret;
    }
    s->suspend_count--;
    count--;
    inferior_debug (2, "resumed task (suspend count now %d)\n", s->suspend_count);
  }

  {
    unsigned char charbuf[1] = { 0 };
    write (s->exception_status.transmit_to_fd, charbuf, 1);
  }

  return KERN_SUCCESS;
}

void macosx_inferior_suspend_ptrace (macosx_inferior_status *s)
{
  struct target_waitstatus status;

  CHECK (s != NULL);
  CHECK (s->attached_in_ptrace);
  CHECK (macosx_inferior_valid (s));
  macosx_inferior_check_stopped (s);
  CHECK ((! s->stopped_in_ptrace) && (! s->stopped_in_softexc));

  macosx_inferior_suspend_mach (s);
  kill (s->pid, SIGSTOP);
  macosx_inferior_resume_mach (s, -1);
  
  macosx_wait (s, &status, NULL);
  CHECK (status.kind == TARGET_WAITKIND_STOPPED);
  CHECK (status.value.sig == TARGET_SIGNAL_STOP);
}

void macosx_inferior_resume_ptrace (macosx_inferior_status *s, unsigned int thread, int nsignal, int val)
{
  CHECK (s != NULL);
  CHECK ((val == PTRACE_DETACH) || (val == PTRACE_CONT));

  CHECK (s->attached_in_ptrace);
  CHECK (macosx_inferior_valid (s));
  macosx_inferior_check_stopped (s);
  CHECK (s->stopped_in_ptrace || s->stopped_in_softexc);

  macosx_inferior_suspend_mach (s);
  
  if ((s->stopped_in_softexc) && (thread != 0)) {
    if (call_ptrace (PTRACE_THUPDATE, s->pid, thread, nsignal) != 0) 
      error ("Error calling ptrace (%s (0x%lx), %d, %d, %d): %s",
	     ptrace_request_unparse (PTRACE_THUPDATE), PTRACE_THUPDATE,
	     s->pid, 1, nsignal, strerror (errno));
  }

  if ((s->stopped_in_ptrace && (! s->stopped_in_softexc)) 
      || (val == PTRACE_DETACH)) {
    if (call_ptrace (val, s->pid, 1, nsignal) != 0)
      error ("Error calling ptrace (%s (0x%lx), %d, %d, %d): %s",
	     ptrace_request_unparse (val), val,
	     s->pid, 1, nsignal, strerror (errno));
  }

  s->stopped_in_softexc = 0;
  s->stopped_in_ptrace = 0;

  if (val == PTRACE_DETACH)
    s->attached_in_ptrace = 0;
}

kern_return_t macosx_save_exception_ports
(task_t task, struct macosx_exception_info *info)
{
  kern_return_t kret;

  info->count = (sizeof (info->ports) / sizeof (info->ports[0]));
  kret = task_get_exception_ports 
    (task,
     EXC_MASK_ALL,
     info->masks, &info->count, info->ports, info->behaviors, info->flavors);
  if (kret != KERN_SUCCESS)
    return kret;

  return KERN_SUCCESS;
}

kern_return_t macosx_restore_exception_ports
(task_t task, struct macosx_exception_info *info)
{
  int i;
  kern_return_t kret;

  for (i = 0; i < info->count; i++) {
    kret = task_set_exception_ports
      (task, info->masks[i], info->ports[i], info->behaviors[i], info->flavors[i]);
    if (kret != KERN_SUCCESS)
      return kret;
  }

  return KERN_SUCCESS;
}
