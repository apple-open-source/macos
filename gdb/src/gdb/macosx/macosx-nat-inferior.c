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

#include "macosx-nat-dyld.h"
#include "macosx-nat-inferior.h"
#include "macosx-nat-infthread.h"
#include "macosx-nat-inferior-debug.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-excthread.h"
#include "macosx-nat-sigthread.h"
#include "macosx-nat-threads.h"
#include "macosx-xdep.h"
#include "macosx-nat-inferior-util.h"

#if WITH_CFM
#include "macosx-nat-cfm.h"
#endif

#include "defs.h"
#include "top.h"
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdbthread.h"
#include "regcache.h"
#include "environ.h"
#include "event-top.h"
#include "event-loop.h"
#include "inf-loop.h"

#include "bfd.h"

#include <sys/ptrace.h>
#include <sys/signal.h>
#include <machine/setjmp.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <mach/mach_error.h>

#ifndef EXC_SOFT_SIGNAL
#define EXC_SOFT_SIGNAL 0
#endif

#define _dyld_debug_make_runnable(a, b) DYLD_FAILURE
#define _dyld_debug_restore_runnable(a, b) DYLD_FAILURE
#define _dyld_debug_module_name(a, b, c, d, e) DYLD_FAILURE
#define _dyld_debug_set_error_func(a) DYLD_FAILURE
#define _dyld_debug_add_event_subscriber(a, b, c, d, e) DYLD_FAILURE

extern int standard_is_async_p (void);
extern int standard_can_async_p (void);

extern bfd *exec_bfd;

extern struct target_ops child_ops;
extern struct target_ops macosx_child_ops;

extern struct target_ops exec_ops;
extern struct target_ops macosx_exec_ops;

macosx_inferior_status *macosx_status = NULL;

int inferior_ptrace_flag = 1;
int inferior_ptrace_on_attach_flag = 1;
int inferior_bind_exception_port_flag = 1;
int inferior_handle_exceptions_flag = 1;
int inferior_handle_all_events_flag = 1;

struct target_ops macosx_child_ops;
struct target_ops macosx_exec_ops;

/* From inftarg.c */
extern void init_child_ops (void);
extern void init_exec_ops (void);

#if WITH_CFM
int inferior_auto_start_cfm_flag = 1;
#endif /* WITH_CFM */

int inferior_auto_start_dyld_flag = 1;

int macosx_fake_resume = 0;

enum macosx_source_type {
  NEXT_SOURCE_NONE = 0x0,
  NEXT_SOURCE_EXCEPTION = 0x1,
  NEXT_SOURCE_SIGNAL = 0x2,
  NEXT_SOURCE_CFM = 0x4,
  NEXT_SOURCE_ALL = 0x7
};

struct macosx_pending_event
{
  enum macosx_source_type type;
  unsigned char *buf;
  struct macosx_pending_event *next;
};

struct macosx_pending_event *pending_event_chain, *pending_event_tail;

static void (*async_client_callback) (enum inferior_event_type event_type, 
				      void *context);
static void *async_client_context;

static enum macosx_source_type macosx_fetch_event (struct macosx_inferior_status *inferior, 
					       unsigned char *buf, size_t len, 
					       unsigned int flags, int timeout);

static int macosx_service_event (enum macosx_source_type source,
			       unsigned char *buf, 
			       struct target_waitstatus *status);

static void macosx_handle_signal (macosx_signal_thread_message *msg, 
				struct target_waitstatus *status);

static void macosx_handle_exception (macosx_exception_thread_message *msg, 
				  struct target_waitstatus *status);

static int macosx_process_events (struct macosx_inferior_status *ns, 
				struct target_waitstatus *status, 
				int timeout, int service_first_event);

static void macosx_add_to_pending_events (enum macosx_source_type, unsigned char *buf);

static int macosx_post_pending_event (void);
static void macosx_pending_event_handler (void *data);
static ptid_t macosx_process_pending_event (struct macosx_inferior_status *ns, 
					  struct target_waitstatus *status,
					  gdb_client_data client_data);

static void macosx_clear_pending_events ();

static void macosx_child_stop (void);

static void macosx_child_resume (ptid_t ptid, int step, enum target_signal signal);

static ptid_t macosx_child_wait (ptid_t ptid, struct target_waitstatus *status, 
				 gdb_client_data client_data);

static void macosx_mourn_inferior ();

static int macosx_lookup_task (char *args, task_t *ptask, int *ppid);

static void macosx_child_attach (char *args, int from_tty);

static void macosx_child_detach (char *args, int from_tty);

static int macosx_kill_inferior (void *);
static void macosx_kill_inferior_safe ();

static void macosx_ptrace_me ();

static void macosx_ptrace_him (int pid);

static void macosx_child_create_inferior (char *exec_file, char *allargs, char **env);

static void macosx_child_files_info (struct target_ops *ops);

static char *macosx_pid_to_str (ptid_t tpid);

static int macosx_child_thread_alive (ptid_t tpid);

static void macosx_set_auto_start_dyld (char *args, int from_tty, 
				      struct cmd_list_element *c);

static void 
macosx_handle_signal (macosx_signal_thread_message *msg, 
		    struct target_waitstatus *status)
{
  kern_return_t kret;

  CHECK_FATAL (macosx_status != NULL);

  CHECK_FATAL (macosx_status->attached_in_ptrace);
  CHECK_FATAL (! macosx_status->stopped_in_ptrace);
  /* CHECK_FATAL (! macosx_status->stopped_in_softexc); */

  if (inferior_debug_flag) 
    {
      macosx_signal_thread_debug_status (stderr, msg->status);
    }

  if (msg->pid != macosx_status->pid) 
    {
      warning ("macosx_handle_signal: signal message was for pid %d, not for inferior process (pid %d)\n", 
	       msg->pid, macosx_status->pid);
      return;
    }
  
  if (WIFEXITED (msg->status)) 
    {
      status->kind = TARGET_WAITKIND_EXITED;
      status->value.integer = WEXITSTATUS (msg->status);
      return;
    }

  if (!WIFSTOPPED (msg->status)) 
    {
      status->kind = TARGET_WAITKIND_SIGNALLED;
      status->value.sig = target_signal_from_host (WTERMSIG (msg->status));
      return;
    }

  macosx_status->stopped_in_ptrace = 1;

  kret = macosx_inferior_suspend_mach (macosx_status);
  MACH_CHECK_ERROR (kret);

  prepare_threads_after_stop (macosx_status);

  status->kind = TARGET_WAITKIND_STOPPED;
  status->value.sig = target_signal_from_host (WSTOPSIG (msg->status));
}

static void 
macosx_handle_exception (macosx_exception_thread_message *msg, 
			 struct target_waitstatus *status)
{
  kern_return_t kret;

  CHECK_FATAL (status != NULL);
  CHECK_FATAL (macosx_status != NULL);

  if (inferior_ptrace_flag)
    {
      CHECK_FATAL (macosx_status->attached_in_ptrace);
      CHECK_FATAL (! macosx_status->stopped_in_ptrace);
    }

  if (inferior_debug_flag) 
    {
      inferior_debug (2, "macosx_handle_exception: received exception message\n");
    }
  
  if (msg->task_port != macosx_status->task)
    {
      /* If the exception was for a child other than the process being
	 debugged, reset the exception ports for the child back to
	 default, and resume.  Ideally the exception ports would never
	 have been set to the one as modified by GDB in the first
	 place, but this should work in most cases. */

      inferior_debug (2, "macosx_handle_exception: exception was for child of process being debugged\n");

      kret = macosx_restore_exception_ports (msg->task_port, &macosx_status->exception_status.saved_exceptions);
      MACH_WARN_ERROR (kret);

      macosx_inferior_resume_mach (macosx_status, 0);

      status->kind = TARGET_WAITKIND_SPURIOUS;
      return;
    }

  macosx_status->last_thread = msg->thread_port;

  kret = macosx_inferior_suspend_mach (macosx_status);
  MACH_CHECK_ERROR (kret);

  macosx_check_new_threads ();
  
  prepare_threads_after_stop (macosx_status);

  status->kind = TARGET_WAITKIND_STOPPED;

  switch (msg->exception_type) 
    {
    case EXC_BAD_ACCESS:
      status->value.sig = TARGET_EXC_BAD_ACCESS;
      break;
    case EXC_BAD_INSTRUCTION:
      status->value.sig = TARGET_EXC_BAD_INSTRUCTION;
      break;
    case EXC_ARITHMETIC:
      status->value.sig = TARGET_EXC_ARITHMETIC;
      break;
    case EXC_EMULATION:
      status->value.sig = TARGET_EXC_EMULATION;
      break;
    case EXC_SOFTWARE:
      {
	switch (msg->exception_data[0])
	  {
	  case EXC_SOFT_SIGNAL:
	    status->value.sig = msg->exception_data[1];
	    macosx_status->stopped_in_softexc = 1;
	    break;
	  default:
	    status->value.sig = TARGET_EXC_SOFTWARE;
	    break;
	  }
      }
      break;
    case EXC_BREAKPOINT:
      /* Many internal GDB routines expect breakpoints to be reported
	 as TARGET_SIGNAL_TRAP, and will report TARGET_EXC_BREAKPOINT
	 as a spurious signal. */
      status->value.sig = TARGET_SIGNAL_TRAP;
      break;
    default:
      status->value.sig = TARGET_SIGNAL_UNKNOWN;
      break;
    }
}

static void 
macosx_add_to_port_set (struct macosx_inferior_status *inferior, 
		      fd_set *fds, int flags)
{
  FD_ZERO (fds);

  if ((flags & NEXT_SOURCE_EXCEPTION) 
      && (inferior->exception_status.receive_from_fd > 0)) 
    {
      FD_SET (inferior->exception_status.receive_from_fd, fds);
    }

  if ((flags & NEXT_SOURCE_SIGNAL) 
      && (inferior->signal_status.receive_fd > 0)) 
    {
      FD_SET (inferior->signal_status.receive_fd, fds);
    }
}

/* TIMEOUT is either -1, 0, or greater than 0.
   For 0, check if there is anything to read, but don't block.
   For -1, block until there is something to read.
   For >0, block at least the specified number of microseconds, or until there
   is something to read.  
   The kernel doesn't give better than ~1HZ (0.01 sec) resolution, so 
   don't use this as a high accuracy timer. */

static enum macosx_source_type 
macosx_fetch_event (struct macosx_inferior_status *inferior, 
                 unsigned char *buf, size_t len, 
                 unsigned int flags, int timeout)
{
  fd_set fds;
  int fd, ret;
  struct timeval tv;
  
  CHECK_FATAL (len >= sizeof (macosx_exception_thread_message));
  CHECK_FATAL (len >= sizeof (macosx_signal_thread_message));

  tv.tv_sec = 0;
  tv.tv_usec = timeout;

  macosx_add_to_port_set (inferior, &fds, flags);
  
  for (;;) {
    if (timeout == -1) {
      ret = select (FD_SETSIZE, &fds, NULL, NULL, NULL); 
    } else { 
      ret = select (FD_SETSIZE, &fds, NULL, NULL, &tv); 
    }
    if ((ret < 0) && (errno == EINTR)) {
      continue;
    }
    if (ret < 0) {
      internal_error (__FILE__, __LINE__, "unable to select: %s", strerror (errno));
    }
    if (ret == 0) {
      return NEXT_SOURCE_NONE;
    }
    break;
  }

  fd = inferior->exception_status.receive_from_fd;
  if ((fd > 0) && FD_ISSET (fd, &fds)) {
    read (fd, buf, sizeof (macosx_exception_thread_message));
    return NEXT_SOURCE_EXCEPTION; 
  }

  fd = inferior->signal_status.receive_fd;
  if ((fd > 0) && FD_ISSET (fd, &fds)) {
    read (fd, buf, sizeof (macosx_signal_thread_message));
    return NEXT_SOURCE_SIGNAL; 
  }

  return NEXT_SOURCE_NONE;
} 

/* This takes the data from an event and puts it on the tail of the
   "pending event" chain. */

static void 
macosx_add_to_pending_events (enum macosx_source_type type, unsigned char *buf)
{
  struct macosx_pending_event *new_event;

  new_event = (struct macosx_pending_event *) 
    xmalloc (sizeof (struct macosx_pending_event));

  new_event->type = type;

  if (type == NEXT_SOURCE_SIGNAL)
    {
      macosx_signal_thread_message *mssg;
      mssg = (macosx_signal_thread_message *) 
	xmalloc (sizeof (macosx_signal_thread_message));
      memcpy (mssg, buf, sizeof (macosx_signal_thread_message));
      inferior_debug (1, "macosx_add_to_pending_events: adding a signal event to the pending events.\n");
      new_event->buf = (void *) mssg;
    }
  else if (type == NEXT_SOURCE_EXCEPTION)
    {
      macosx_exception_thread_message *mssg;
      mssg = (macosx_exception_thread_message *) 
	xmalloc (sizeof (macosx_exception_thread_message));
      memcpy (mssg, buf, sizeof(macosx_exception_thread_message));
      inferior_debug (1, "macosx_add_to_pending_events: adding an exception event to the pending events.\n");
      new_event->buf = (void *) mssg;
    }

  new_event->next = NULL;

  if (pending_event_chain == NULL) 
    {
      pending_event_chain = new_event;
      pending_event_tail = new_event;
    }
  else
    {
      pending_event_tail->next = new_event;
      pending_event_tail = new_event;
    }
}

static void
macosx_clear_pending_events ()
{
  struct macosx_pending_event *event_ptr = pending_event_chain;
  
  while (event_ptr != NULL)
    {
      pending_event_chain = event_ptr->next;
      xfree (event_ptr->buf);
      xfree (event_ptr);
      event_ptr = pending_event_chain;
    }
}

/* This extracts the top of the pending event chain and posts a gdb event
   with its content to the gdb event queue.  Returns 0 if there were no
   pending events to be posted, 1 otherwise. */

static int
macosx_post_pending_event (void)
{
  struct macosx_pending_event *event;

  if (pending_event_chain == NULL)
    {
      inferior_debug (1, "macosx_post_pending_event: no events to post\n");
      return 0;
    }
  else
    {
      event = pending_event_chain;
      pending_event_chain = pending_event_chain->next;
      if (pending_event_chain == NULL)
	pending_event_tail = NULL;

      inferior_debug (1, "macosx_post_pending_event: consuming event off queue\n");
      gdb_queue_event (macosx_pending_event_handler, (void *) event, HEAD);

      return 1;
    }
}

static void
macosx_pending_event_handler (void *data)
{
  inferior_debug (1, "Called in macosx_pending_event_handler\n");
  async_client_callback (INF_REG_EVENT, data);
}

static int
macosx_service_event (enum macosx_source_type source,
		      unsigned char *buf, struct target_waitstatus *status)
{
  if (source == NEXT_SOURCE_EXCEPTION)
    {
      inferior_debug (1, "macosx_service_events: got exception message\n");
      CHECK_FATAL (inferior_bind_exception_port_flag);
      macosx_handle_exception ((macosx_exception_thread_message *) buf, status);
      if (status->kind != TARGET_WAITKIND_SPURIOUS) 
	{
	  CHECK_FATAL (inferior_handle_exceptions_flag);
	  return 1;
	}
    }
  else if (source == NEXT_SOURCE_SIGNAL) 
    {
      inferior_debug (2, "macosx_service_events: got signal message\n");
      macosx_handle_signal ((macosx_signal_thread_message *) buf, status);
      CHECK_FATAL (status->kind != TARGET_WAITKIND_SPURIOUS);
      if (!inferior_handle_all_events_flag) 
	{
	  return 1;
	}	    
    }
  else 
    {
      error ("got message from unknown source: 0x%08x\n", source);
      return 0;
    }
  return 1;
}

/* This drains the event sources.  The first event found is directly
   handled.  The rest are placed on the pending events queue, to be
   handled the next time that the inferior is "run".

   Returns: The number of events found. */

static int
macosx_process_events (struct macosx_inferior_status *inferior, 
		     struct target_waitstatus *status, 
		     int timeout, int service_first_event)
{
    enum macosx_source_type source;
    unsigned char buf[1024];
    int event_count;

    CHECK_FATAL (status->kind == TARGET_WAITKIND_SPURIOUS);

    source = macosx_fetch_event (inferior, buf, sizeof (buf), 
				 NEXT_SOURCE_ALL, timeout);
    if (source == NEXT_SOURCE_NONE) 
      {
	return 0;
      }

    event_count = 1;

    if (service_first_event)
      {
	if (macosx_service_event (source, buf, status) == 0)
	  return 0;
      } 
    else
      {
	macosx_add_to_pending_events (source, buf);
      }

    /* FIXME: we want to poll in macosx_fetch_event because otherwise we
       arbitrarily wait however long the wait quanta for select is
       (seemingly ~.01 sec).  However, if we do this we aren't giving
       the mach exception thread a chance to run, and see if there are
       any more exceptions available.  Normally this is okay, because
       there really IS only one message, but to be correct we need to
       use some thread synchronization. */
    for (;;) 
      {
	source = macosx_fetch_event (inferior, buf, sizeof (buf), 
				   NEXT_SOURCE_ALL, 0);
	if (source == NEXT_SOURCE_NONE) 
	  { 
	    break;
	  }
	else
	  {
	    event_count++;

	    /* Stuff the remaining events onto the pending_events queue.
	       These will be dispatched when we run again. */
	    /* PENDING_EVENTS */
	    macosx_add_to_pending_events (source, buf);
	  }
      }

    inferior_debug (2, "macosx_process_events: returning with (status->kind == %d)\n", 
		  status->kind);
    return event_count;
}

void macosx_check_new_threads ()
{
  thread_array_t thread_list = NULL;
  unsigned int nthreads = 0;

  kern_return_t kret;
  unsigned int i;

  kret = task_threads (macosx_status->task, &thread_list, &nthreads);
  if (kret != KERN_SUCCESS) { return; }
  MACH_CHECK_ERROR (kret);
  
  for (i = 0; i < nthreads; i++) {
    ptid_t ptid = ptid_build (macosx_status->pid, 0, thread_list[i]);
    if (! in_thread_list (ptid)) {
      add_thread (ptid);
    }
  }

  kret = vm_deallocate (mach_task_self(), (vm_address_t) thread_list, (nthreads * sizeof (int)));
  MACH_CHECK_ERROR (kret);
}

/* This differs from child_stop in that we don't send "-inferior_process_group" to 
   kill when we are attached to the process, we just send inferior_process_group.
   Even this is kind of a lie, since inferior_process_group really isn't, it is just the
   pid of the child process, look at "terminal_init_inferior" in inflow.c, which
   sets inferior_process_group.  This just passes in the pid of the child process!
   I think all the job control stuff in inflow.c looks bogus to me, we ought to use
   MacOS X specific versions everywhere we can, and avoid that mess...
*/

static void 
macosx_child_stop (void)
{
  extern pid_t inferior_process_group;
  int ret;

  ret = kill (inferior_process_group, SIGINT);
}

static void 
macosx_child_resume (ptid_t ptid, int step, enum target_signal signal)
{
  int nsignal = target_signal_to_host (signal);
  struct target_waitstatus status;

  int pid;
  thread_t thread;

  if (ptid_equal (ptid, minus_one_ptid))
    ptid = inferior_ptid;

  pid = ptid_get_pid (ptid);
  thread = ptid_get_tid (ptid);

  CHECK_FATAL (tm_print_insn != NULL);
  CHECK_FATAL (macosx_status != NULL);

  macosx_inferior_check_stopped (macosx_status);
  if (! macosx_inferior_valid (macosx_status))
    return;

  /* Check for pending events.  If we find any, then we won't really resume,
     but rather we will extract the first event from the pending events
     queue, and post it to the gdb event queue, and then "pretend" that we
     have in fact resumed. */

  inferior_debug (2, "macosx_child_resume: checking for pending events\n");
  status.kind = TARGET_WAITKIND_SPURIOUS;
  macosx_process_events (macosx_status, &status, 0, 0);
    
  inferior_debug (1, "macosx_child_resume: %s process with signal %d\n", 
		  step ? "stepping" : "continuing", nsignal);

  if (macosx_post_pending_event ())
    {
      /* QUESTION: Do I need to lie about target_executing here? */
      macosx_fake_resume = 1;
      if (target_is_async_p ())
	target_executing = 1;
      return;
    }

  if (macosx_status->stopped_in_ptrace || macosx_status->stopped_in_softexc) {
    macosx_inferior_resume_ptrace (macosx_status, thread, nsignal, PTRACE_CONT);
  }

  if (! macosx_inferior_valid (macosx_status))
    return;

  if (step)
    prepare_threads_before_run (macosx_status, step, thread, 1);
  else
    prepare_threads_before_run (macosx_status, 0, THREAD_NULL, 0);

  macosx_inferior_resume_mach (macosx_status, -1);

  if (event_loop_p && target_can_async_p ())
    target_async (inferior_event_handler, 0);

  if (target_is_async_p ())
    target_executing = 1;
}

static ptid_t 
macosx_process_pending_event (struct macosx_inferior_status *ns, 
			    struct target_waitstatus *status,
			    gdb_client_data client_data)
{
  struct macosx_pending_event *event 
    = (struct macosx_pending_event *) client_data;

  inferior_debug (1, "Processing pending event type: %d\n", event->type);
  macosx_service_event (event->type, (unsigned char *) event->buf, status);

  return ptid_build (macosx_status->pid, 0, macosx_status->last_thread);
}

/*
 * This fetches & processes events from the exception & signal file
 * descriptors.  If client_data is not NULL, then the client data must
 * hold a macosx_pending_event structure, and in that case, this just
 * processes that event.
 */
 
ptid_t 
macosx_wait (struct macosx_inferior_status *ns, 
	   struct target_waitstatus *status,
	   gdb_client_data client_data)
{
  CHECK_FATAL (ns != NULL);
  
  if (client_data != NULL) 
    return macosx_process_pending_event (ns, status, client_data);

  set_sigint_trap ();
  set_sigio_trap ();
  
  status->kind = TARGET_WAITKIND_SPURIOUS;
  while (status->kind == TARGET_WAITKIND_SPURIOUS)
    macosx_process_events (ns, status, -1, 1);
  
  clear_sigio_trap ();
  clear_sigint_trap();

  if ((status->kind == TARGET_WAITKIND_EXITED) 
      || (status->kind == TARGET_WAITKIND_SIGNALLED)) 
    return null_ptid;

  macosx_check_new_threads ();

  if (! macosx_thread_valid (macosx_status->task, macosx_status->last_thread)) 
    {
      if (macosx_task_valid (macosx_status->task)) 
	{
	  warning ("Currently selected thread no longer alive; selecting intial thread");
	  macosx_status->last_thread = macosx_primary_thread_of_task (macosx_status->task);
	}
    }

  return ptid_build (macosx_status->pid, 0, macosx_status->last_thread);
}

static ptid_t 
macosx_child_wait (ptid_t pid, struct target_waitstatus *status, 
		   gdb_client_data client_data)
{
  CHECK_FATAL (macosx_status != NULL);
  return macosx_wait (macosx_status, status, client_data);
}

static void macosx_mourn_inferior ()
{
  unpush_target (&macosx_child_ops);
  child_ops.to_mourn_inferior ();
  macosx_inferior_destroy (macosx_status);

  inferior_ptid = null_ptid;
  attach_flag = 0;

  /* We were doing this just so that we would reset the results of
     info dyld to the original state.  But it had the unintended
     consequence that gdb would try to reinsert breakpoints when you
     were quitting gdb with the inferior still running.  Furthermore,
     errors here seem to hit the async code in a particularly bad 
     place.

     On the one hand, it is arguable whether a "correct" info dyld
     output in this case is the history of the previous run, or the
     next run...

     FIXME: See if we can figure out the minimal subset of the 
     operations in macosx_init_dyld_symfile that will clear the dyld
     state without all the bad effects of the full function. */

#if 0
  if (symfile_objfile != NULL) 
    {
      CHECK_FATAL (symfile_objfile->obfd != NULL);
      macosx_init_dyld_symfile (symfile_objfile->obfd);
    } 
  else 
    {
      macosx_init_dyld_symfile (NULL); 
    }
#endif

  macosx_clear_pending_events();
}

void macosx_fetch_task_info (struct kinfo_proc **info, size_t *count)
{
  struct kinfo_proc *proc;
  unsigned int control[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
  size_t length;

  CHECK_FATAL (info != NULL);
  CHECK_FATAL (count != NULL);

  sysctl (control, 3, NULL, &length, NULL, 0);
  proc = (struct kinfo_proc *) xmalloc (length);
  sysctl (control, 3, proc, &length, NULL, 0);

  *count = length / sizeof (struct kinfo_proc);
  *info = proc;
}

char **macosx_process_completer_quoted (char *text, char *word, int quote)
{
  struct kinfo_proc *proc = NULL;
  size_t count, i;
  
  char **procnames = NULL;
  char **ret = NULL;
  int quoted = 0;

  if (text[0] == '"') {
    quoted = 1;
  }

  macosx_fetch_task_info (&proc, &count);
  
  procnames = (char **) xmalloc ((count + 1) * sizeof (char *));

  for (i = 0; i < count; i++) {
    char *temp = (char *) xmalloc (strlen (proc[i].kp_proc.p_comm) + 1 + 16);
    sprintf (temp, "%s.%d", proc[i].kp_proc.p_comm, proc[i].kp_proc.p_pid);
    procnames[i] = (char *) xmalloc (strlen (temp) * 2 + 2 + 1);
    if (quote) {
      if (quoted) {
	sprintf (procnames[i], "\"%s\"", temp);
      } else {
	char *s = temp;
	char *t = procnames[i];
	while (*s != '\0') {
	  if (strchr ("\" ", *s) != NULL) {
	    *t++ = '\\';
	    *t++ = *s++;
	  } else {
	    *t++ = *s++; 
	  }
	}
	*t++ = '\0';
      }
    } else {
      sprintf (procnames[i], "%s", temp);
    }
  }
  procnames[i] = NULL;

  ret = complete_on_enum ((const char **) procnames, text, word);

  xfree (proc);
  return ret;
}

char **macosx_process_completer (char *text, char *word)
{
  return macosx_process_completer_quoted (text, word, 1);
}

static void macosx_lookup_task_remote (char *host_str, char *pid_str, int pid, task_t *ptask, int *ppid)
{
  CHECK_FATAL (ptask != NULL);
  CHECK_FATAL (ppid != NULL);

  error ("Unable to attach to remote processes on Mach 3.0 (no netname_look_up ()).");
}

static void macosx_lookup_task_local (char *pid_str, int pid, task_t *ptask, int *ppid)
{
  CHECK_FATAL (ptask != NULL);
  CHECK_FATAL (ppid != NULL);

  if (pid_str == NULL) {

    task_t itask;
    kern_return_t kret;

    kret = task_for_pid (mach_task_self(), pid, &itask);
    if (kret != KERN_SUCCESS) {
      error ("Unable to access task for process-id %d: %s.", pid, MACH_ERROR_STRING (kret));
    }
    *ptask = itask;
    *ppid = pid;

  } else {

    struct cleanup *cleanups = NULL;
    char **ret = macosx_process_completer_quoted (pid_str, pid_str, 0);
    char *tmp = NULL;
    char *tmp2 = NULL;
    unsigned long lpid = 0;

    task_t itask;
    kern_return_t kret;

    cleanups = make_cleanup (free, ret);

    if ((ret == NULL) || (ret[0] == NULL)) {
      error ("Unable to locate process named \"%s\".", pid_str);
    }
    if (ret[1] != NULL) {
      error ("Multiple processes exist with the name \"%s\".", pid_str);
    }
  
    tmp = strrchr (ret[0], '.');
    if (tmp == NULL) {
      error ("Unable to parse process-specifier \"%s\" (does not contain process-id)", ret[0]);
    }
    tmp++;
    lpid = strtoul (tmp, &tmp2, 10);
    if (! isdigit (*tmp) || (*tmp2 != '\0')) {
      error ("Unable to parse process-specifier \"%s\" (does not contain process-id)", ret[0]);
    }
    if ((lpid > INT_MAX) || ((lpid == ULONG_MAX) && (errno == ERANGE))) {
      error ("Unable to parse process-id \"%s\" (integer overflow).", ret[0]);
    }
    pid = lpid;

    kret = task_for_pid (mach_task_self(), pid, &itask);
    if (kret != KERN_SUCCESS) {
      error ("Unable to locate task for process-id %d: %s.", pid, MACH_ERROR_STRING (kret));
    }

    *ptask = itask;
    *ppid = pid;

    do_cleanups (cleanups);
  }
}    

static int macosx_lookup_task (char *args, task_t *ptask, int *ppid)
{
  char *host_str = NULL;
  char *pid_str = NULL;
  char *tmp = NULL;

  struct cleanup *cleanups = NULL;
  char **argv = NULL;
  unsigned int argc;
 
  unsigned long lpid = 0;
  int pid = 0;

  CHECK_FATAL (ptask != NULL);
  CHECK_FATAL (ppid != NULL);

  *ptask = TASK_NULL; 
  *ppid = 0;

  if (args == NULL) {
    return 0;
  }

  argv = buildargv (args);
  if (argv == NULL) {
    nomem (0);
  }

  cleanups = make_cleanup_freeargv (argv);

  for (argc = 0; argv[argc] != NULL; argc++);

  switch (argc) {
  case 1:
    pid_str = argv[0];
    break;
  case 2:
    host_str = argv[0];
    pid_str = argv[1];
    break;
  default:
    error ("Usage: attach [host] <pid|pid-string>.");
    break;
  }

  CHECK_FATAL (pid_str != NULL);
  lpid = strtoul (pid_str, &tmp, 10);
  if (isdigit (*pid_str) && (*tmp == '\0')) {
    if ((lpid > INT_MAX) || ((lpid == ULONG_MAX) && (errno == ERANGE))) {
      error ("Unable to locate pid \"%s\" (integer overflow).", pid_str);
    }
    pid_str = NULL;
    pid = lpid;
  }

  if (host_str != NULL) {
    macosx_lookup_task_remote (host_str, pid_str, pid, ptask, ppid);
  } else {
    macosx_lookup_task_local (pid_str, pid, ptask, ppid);
  }

  do_cleanups (cleanups);
  return 0;
}

static void
macosx_set_auto_start_dyld (char *args, int from_tty,
			  struct cmd_list_element *c)
{

  /* Don't want to bother with stopping the target to set this... */
  if (target_executing)
    return;

  /* If we are so early on that the macosx_status hasn't gotten allocated
     yet, this will fail, but we also won't have needed to do anything, 
     so we can safely just exit. */
  if (macosx_status == NULL)
    return;

  /* If we are turning off watching dyld, we need to remove
     the breakpoint... */

  if (!inferior_auto_start_dyld_flag)
    {
      macosx_clear_start_breakpoint ();
      return;
    }

  /* If the inferior is not running, then all we have to do
     is set the flag, which is done in generic code. */

  if (ptid_equal (inferior_ptid, null_ptid))
    return;

  macosx_dyld_update (1);
  macosx_set_start_breakpoint (exec_bfd);
  macosx_dyld_update (0);
  
}

static void macosx_child_attach (char *args, int from_tty)
{
  struct target_waitstatus w;
  task_t itask;
  int pid;
  int ret;
  kern_return_t kret;

  if (args == NULL) {
    error_no_arg ("process-id to attach");
  }

  macosx_lookup_task (args, &itask, &pid);
  if (itask == TASK_NULL) {
    error ("unable to locate task");
  }

  if (itask == mach_task_self ()) {
    error ("unable to debug self");
  }

  CHECK_FATAL (macosx_status != NULL);
  macosx_inferior_destroy (macosx_status);

  macosx_create_inferior_for_task (macosx_status, itask, pid);

  macosx_exception_thread_create (&macosx_status->exception_status, macosx_status->task);

  if (inferior_ptrace_on_attach_flag) {

    ret = call_ptrace (PTRACE_ATTACHEXC, pid, 0, 0);
    if (ret != 0) {
      macosx_inferior_destroy (macosx_status);
      if (errno == EPERM) {
	error ("Unable to attach to process-id %d: %s (%d).\n"
	       "This request requires that the target process be neither setuid nor "
	       "setgid and have the same real userid as the debugger, or that the "
	       "debugger be running with adminstrator privileges.",
	       pid, strerror (errno), errno);
      } else {
	error ("Unable to attach to process-id %d: %s (%d)",
	       pid, strerror (errno), errno);
      }
    }
    
    macosx_status->attached_in_ptrace = 1;
    macosx_status->stopped_in_ptrace = 0;
    macosx_status->stopped_in_softexc = 0;

    macosx_status->suspend_count = 0;

  } else if (inferior_bind_exception_port_flag) {
    
    kret = macosx_inferior_suspend_mach (macosx_status);
    if (kret != KERN_SUCCESS) {
      macosx_inferior_destroy (macosx_status);
      MACH_CHECK_ERROR (kret);
    }
  }
  
  macosx_check_new_threads ();

  inferior_ptid = ptid_build (pid, 0, macosx_status->last_thread);
  attach_flag = 1;

  push_target (&macosx_child_ops);

  if (macosx_status->attached_in_ptrace) {
    /* read attach notification */
    macosx_wait (macosx_status, &w, NULL);

    macosx_signal_thread_create (&macosx_status->signal_status, macosx_status->pid);
    macosx_wait (macosx_status, &w, NULL);
  }
  
  if (inferior_auto_start_dyld_flag) {
    macosx_dyld_update (1);
    macosx_set_start_breakpoint (exec_bfd);
    macosx_dyld_update (0);
  }
}

static void macosx_child_detach (char *args, int from_tty)
{
  kern_return_t kret;

  CHECK_FATAL (macosx_status != NULL);

  if (ptid_equal (inferior_ptid, null_ptid)) {
    return;
  }

  if (! macosx_inferior_valid (macosx_status)) {
    target_mourn_inferior ();
    return;
  }
  
  macosx_inferior_check_stopped (macosx_status);
  CHECK (macosx_inferior_valid (macosx_status));
    
  if (macosx_status->attached_in_ptrace
      && (! macosx_status->stopped_in_ptrace)
      && (! macosx_status->stopped_in_softexc)) {
    macosx_inferior_suspend_ptrace (macosx_status);
    CHECK_FATAL (macosx_status->stopped_in_ptrace);
  }

  if (inferior_bind_exception_port_flag) {
    kret = macosx_restore_exception_ports (macosx_status->task, &macosx_status->exception_status.saved_exceptions);
    MACH_CHECK_ERROR (kret);
  }

  if (macosx_status->attached_in_ptrace) {
    macosx_inferior_resume_ptrace (macosx_status, 0, 0, PTRACE_DETACH);
  }

  if (! macosx_inferior_valid (macosx_status)) {
    target_mourn_inferior ();
    return;
  }

  macosx_inferior_suspend_mach (macosx_status);

  if (! macosx_inferior_valid (macosx_status)) {
    target_mourn_inferior ();
    return;
  }

  prepare_threads_before_run (macosx_status, 0, THREAD_NULL, 0);
  macosx_inferior_resume_mach (macosx_status, -1);

  target_mourn_inferior ();
  return;
}

static int macosx_kill_inferior (void *arg)
{
  kern_return_t *errval = (kern_return_t *) arg;

  CHECK_FATAL (macosx_status != NULL);
  *errval = KERN_SUCCESS;

  if (ptid_equal (inferior_ptid, null_ptid)) {
    return 1;
  }

  if (! macosx_inferior_valid (macosx_status)) {
    target_mourn_inferior ();
    return 1;
  }

  macosx_inferior_check_stopped (macosx_status);
  CHECK (macosx_inferior_valid (macosx_status));
  
  if (macosx_status->attached_in_ptrace
      && (! macosx_status->stopped_in_ptrace)
      && (! macosx_status->stopped_in_softexc)) {
    macosx_inferior_suspend_ptrace (macosx_status);
    CHECK_FATAL ((macosx_status->stopped_in_ptrace || macosx_status->stopped_in_softexc));
  }
  
  macosx_inferior_suspend_mach (macosx_status);
  prepare_threads_before_run (macosx_status, 0, THREAD_NULL, 0);
  
  if (macosx_status->attached_in_ptrace) {
    CHECK_FATAL (macosx_status->stopped_in_ptrace || macosx_status->stopped_in_softexc);
    if (call_ptrace (PTRACE_KILL, macosx_status->pid, 0, 0) != 0) {
	  error ("macosx_child_detach: ptrace (%d, %d, %d, %d): %s",
		 PTRACE_KILL, macosx_status->pid, 0, 0, strerror (errno));
    }
    macosx_status->stopped_in_ptrace = 0;
    macosx_status->stopped_in_softexc = 0;
  }

  if (! macosx_inferior_valid (macosx_status)) {
    target_mourn_inferior ();
    return 1;
  }

  macosx_inferior_resume_mach (macosx_status, -1);
  sched_yield ();
  target_mourn_inferior ();

  return 1;
}

static void macosx_kill_inferior_safe ()
{
  kern_return_t kret;
  int ret;

  ret = catch_errors (macosx_kill_inferior, &kret, 
     "error while killing target (killing anyway): ", RETURN_MASK_ALL);

  if (ret == 0) {
    kret = task_terminate (macosx_status->task);
    MACH_WARN_ERROR (kret);
    sched_yield ();
    target_mourn_inferior ();
  }
}

static void macosx_ptrace_me ()
{
  call_ptrace (PTRACE_TRACEME, 0, 0, 0);
  call_ptrace (PTRACE_SIGEXC, 0, 0, 0);
}

static void macosx_ptrace_him (int pid)
{
  task_t itask;
  kern_return_t kret;
  int traps_expected;

  CHECK_FATAL (! macosx_status->attached_in_ptrace);
  CHECK_FATAL (! macosx_status->stopped_in_ptrace);
  CHECK_FATAL (! macosx_status->stopped_in_softexc);
  CHECK_FATAL (macosx_status->suspend_count == 0);

  kret = task_for_pid (mach_task_self(), pid, &itask);
  {
    char buf[64];
    sprintf (buf, "%s=%d", "TASK", itask);
    putenv (buf);
  }
  if (kret != KERN_SUCCESS) {
    error ("Unable to find Mach task port for process-id %d: %s (0x%lx).", 
	   pid, MACH_ERROR_STRING (kret), (unsigned long) kret);
  }

  inferior_debug (2, "inferior task: 0x%08x, pid: %d\n", itask, pid);
  
  push_target (&macosx_child_ops);
  macosx_create_inferior_for_task (macosx_status, itask, pid);

  macosx_signal_thread_create (&macosx_status->signal_status, macosx_status->pid);
  macosx_exception_thread_create (&macosx_status->exception_status, macosx_status->task);

  macosx_status->attached_in_ptrace = 1;
  macosx_status->stopped_in_ptrace = 0;
  macosx_status->stopped_in_softexc = 0;

  macosx_status->suspend_count = 0;

  traps_expected = (start_with_shell_flag ? 2 : 1);
  startup_inferior (traps_expected);
  
  if (ptid_equal (inferior_ptid, null_ptid)) {
    return;
  }

  if (! macosx_task_valid (macosx_status->task)) {
    target_mourn_inferior ();
    return;
  }

  macosx_inferior_check_stopped (macosx_status);
  CHECK (macosx_inferior_valid (macosx_status));

  if (inferior_ptrace_flag) {
    CHECK_FATAL (macosx_status->attached_in_ptrace);
    CHECK_FATAL (macosx_status->stopped_in_ptrace || macosx_status->stopped_in_softexc);
  } else {
    macosx_inferior_resume_ptrace (macosx_status, 0, 0, PTRACE_DETACH);
    CHECK_FATAL (! macosx_status->attached_in_ptrace);
    CHECK_FATAL (! macosx_status->stopped_in_ptrace);
    CHECK_FATAL (! macosx_status->stopped_in_softexc);
  }
}

static void macosx_child_create_inferior (char *exec_file, char *allargs, char **env)
{
  fork_inferior (exec_file, allargs, env, macosx_ptrace_me, macosx_ptrace_him, NULL, NULL);
  if (ptid_equal (inferior_ptid, null_ptid))
    return;

  macosx_clear_start_breakpoint ();
  if (inferior_auto_start_dyld_flag) {
    macosx_set_start_breakpoint (exec_bfd);
  }

  attach_flag = 0;

  if (event_loop_p && target_can_async_p ())
    target_async (inferior_event_handler, 0);

  clear_proceed_status ();
  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_0, 0);
}

static void macosx_child_files_info (struct target_ops *ops)
{
  CHECK_FATAL (macosx_status != NULL);
  macosx_debug_inferior_status (macosx_status);
}

static char *macosx_pid_to_str (ptid_t ptid)
{
  static char buf[128];
  int pid = ptid_get_pid (ptid);
  thread_t thread = ptid_get_tid (ptid);
  
  sprintf (buf, "process %d thread 0x%lx", pid, (unsigned long) thread);
  return buf;
}

static int 
macosx_child_thread_alive (ptid_t ptid)
{
  return macosx_thread_valid (macosx_status->task, ptid_get_tid (ptid));
}

void update_command (char *args, int from_tty)
{
  registers_changed ();
  reinit_frame_cache ();
}

void macosx_create_inferior_for_task
(struct macosx_inferior_status *inferior, task_t task, int pid)
{
  CHECK_FATAL (inferior != NULL);

  macosx_inferior_destroy (inferior);
  macosx_inferior_reset (inferior);

  inferior->task = task;
  inferior->pid = pid;

  inferior->attached_in_ptrace = 0;
  inferior->stopped_in_ptrace = 0;
  inferior->stopped_in_softexc = 0;

  inferior->suspend_count = 0;

  inferior->last_thread = macosx_primary_thread_of_task (inferior->task);
}

static int remote_async_terminal_ours_p = 1;
static void (*ofunc) (int);
static PTR sigint_remote_twice_token;
static PTR sigint_remote_token;

static void remote_interrupt_twice (int signo); 
static void remote_interrupt (int signo);
static void handle_remote_sigint_twice (int sig);
static void handle_remote_sigint (int sig);
static void async_remote_interrupt_twice (gdb_client_data arg);
static void async_remote_interrupt (gdb_client_data arg);

static void
interrupt_query (void)
{
  target_terminal_ours ();

  if (query ("Interrupted while waiting for the program.\n\
Give up (and stop debugging it)? "))
    {
      target_mourn_inferior ();
      throw_exception (RETURN_QUIT);
    }

  target_terminal_inferior ();
}

static void
remote_interrupt_twice (int signo)
{
  signal (signo, ofunc);
  interrupt_query ();
  signal (signo, remote_interrupt);
}

static void
remote_interrupt (int signo)
{
  signal (signo, remote_interrupt_twice);
  target_stop ();
}

static void
handle_remote_sigint_twice (int sig)
{
  signal (sig, handle_sigint);
  sigint_remote_twice_token =
    create_async_signal_handler (inferior_event_handler_wrapper, NULL);
  mark_async_signal_handler_wrapper (sigint_remote_twice_token);
}

static void
handle_remote_sigint (int sig)
{
  signal (sig, handle_remote_sigint_twice);
  sigint_remote_twice_token =
    create_async_signal_handler (async_remote_interrupt_twice, NULL);
  mark_async_signal_handler_wrapper (sigint_remote_token);
}

static void
async_remote_interrupt_twice (gdb_client_data arg)
{
  if (target_executing)
    {
      interrupt_query ();
      signal (SIGINT, handle_remote_sigint);
    }
}

static void
async_remote_interrupt (gdb_client_data arg)
{
  target_stop ();
}

static void
cleanup_sigint_signal_handler (void *dummy)
{
  signal (SIGINT, handle_sigint);
  if (sigint_remote_twice_token)
    delete_async_signal_handler ((struct async_signal_handler **) &sigint_remote_twice_token);
  if (sigint_remote_token)
    delete_async_signal_handler ((struct async_signal_handler **) &sigint_remote_token);
}

static void
initialize_sigint_signal_handler (void)
{
  sigint_remote_token =
    create_async_signal_handler (async_remote_interrupt, NULL);
  signal (SIGINT, handle_remote_sigint);
}

static void
macosx_terminal_inferior (void)
{
  terminal_inferior ();

  if (!sync_execution)
    return;
  if (!remote_async_terminal_ours_p)
    return;
  CHECK_FATAL (sync_execution);
  CHECK_FATAL (remote_async_terminal_ours_p);
  delete_file_handler (input_fd);
  remote_async_terminal_ours_p = 0;
  initialize_sigint_signal_handler ();
}

static void
macosx_terminal_ours (void)
{
  terminal_ours ();

  if (!sync_execution)
    return;
  if (remote_async_terminal_ours_p)
    return;
  CHECK_FATAL (sync_execution);
  CHECK_FATAL (!remote_async_terminal_ours_p);
  cleanup_sigint_signal_handler (NULL);

  add_file_handler (input_fd, stdin_event_handler, 0);

  remote_async_terminal_ours_p = 1;
}

static void
macosx_file_handler (int error, gdb_client_data client_data)
{
  async_client_callback (INF_REG_EVENT, async_client_context);
}

static void
macosx_async (void (*callback) (enum inferior_event_type event_type, 
			      void *context), void *context)
{
  if (current_target.to_async_mask_value == 0)
    internal_error (__FILE__, __LINE__, "Calling remote_async when async is masked");

  if (callback != NULL)
    {
      async_client_callback = callback;
      async_client_context = context;
      if (macosx_status->exception_status.receive_from_fd > 0)
	  add_file_handler (macosx_status->exception_status.receive_from_fd,
			    macosx_file_handler, NULL);
      if (macosx_status->signal_status.receive_fd > 0)
	  add_file_handler (macosx_status->signal_status.receive_fd, 
			    macosx_file_handler, NULL);
    }
  else
    {
      if (macosx_status->exception_status.receive_from_fd > 0)
	delete_file_handler (macosx_status->exception_status.receive_from_fd);
      if (macosx_status->signal_status.receive_fd > 0)
	delete_file_handler (macosx_status->signal_status.receive_fd);
    }
}

void 
_initialize_macosx_inferior ()
{
  struct cmd_list_element *cmd;

  CHECK_FATAL (macosx_status == NULL);
  macosx_status = (struct macosx_inferior_status *)
      xmalloc (sizeof (struct macosx_inferior_status));

  macosx_inferior_reset (macosx_status);

  dyld_init_paths (&macosx_status->dyld_status.path_info);
  dyld_objfile_info_init (&macosx_status->dyld_status.current_info);

  init_child_ops ();
  macosx_child_ops = child_ops;
  child_ops.to_can_run = NULL;

  init_exec_ops ();
  macosx_exec_ops = exec_ops;
  exec_ops.to_can_run = NULL;

  macosx_exec_ops.to_shortname = "macos-exec";
  macosx_exec_ops.to_longname = "Mac OS X executable";
  macosx_exec_ops.to_doc = "Mac OS X executable";
  macosx_exec_ops.to_can_async_p = standard_can_async_p;
  macosx_exec_ops.to_is_async_p = standard_is_async_p;

  macosx_child_ops.to_shortname = "macos-child";
  macosx_child_ops.to_longname = "Mac OS X child process";
  macosx_child_ops.to_doc = "Mac OS X child process (started by the \"run\" command).";
  macosx_child_ops.to_attach = macosx_child_attach;
  macosx_child_ops.to_detach = macosx_child_detach;
  macosx_child_ops.to_create_inferior = macosx_child_create_inferior;
  macosx_child_ops.to_files_info = macosx_child_files_info;
  macosx_child_ops.to_wait = macosx_child_wait;
  macosx_child_ops.to_mourn_inferior = macosx_mourn_inferior;
  macosx_child_ops.to_kill = macosx_kill_inferior_safe;
  macosx_child_ops.to_stop = macosx_child_stop;
  macosx_child_ops.to_resume = macosx_child_resume;
  macosx_child_ops.to_thread_alive = macosx_child_thread_alive;
  macosx_child_ops.to_pid_to_str = macosx_pid_to_str;
  macosx_child_ops.to_load = NULL;
  macosx_child_ops.to_xfer_memory = mach_xfer_memory;
  macosx_child_ops.to_can_async_p = standard_can_async_p;
  macosx_child_ops.to_is_async_p = standard_is_async_p;
  macosx_child_ops.to_terminal_inferior = macosx_terminal_inferior;
  macosx_child_ops.to_terminal_ours = macosx_terminal_ours;
  macosx_child_ops.to_async = macosx_async; 
  macosx_child_ops.to_async_mask_value = 1;
  macosx_child_ops.to_bind_function = dyld_lookup_and_bind_function;

  add_target (&macosx_exec_ops);
  add_target (&macosx_child_ops);

  inferior_stderr = fdopen (fileno (stderr), "w");
  inferior_debug (2, "GDB task: 0x%lx, pid: %d\n", mach_task_self(), getpid());

  cmd = add_set_cmd ("inferior-bind-exception-port", class_obscure, var_boolean, 
		     (char *) &inferior_bind_exception_port_flag,
		     "Set if GDB should bind the task exception port.",
		     &setlist);
  add_show_from_set (cmd, &showlist);		

  cmd = add_set_cmd ("inferior-handle-exceptions", class_obscure, var_boolean, 
		     (char *) &inferior_handle_exceptions_flag,
		     "Set if GDB should handle exceptions or pass them to the UNIX handler.",
		     &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd ("inferior-handle-all-events", class_obscure, var_boolean, 
		     (char *) &inferior_handle_all_events_flag,
		     "Set if GDB should immediately handle all exceptions upon each stop, "
		     "or only the first received.",
		     &setlist);
  add_show_from_set (cmd, &showlist);		

  cmd = add_set_cmd ("inferior-ptrace", class_obscure, var_boolean, 
		     (char *) &inferior_ptrace_flag,
		     "Set if GDB should attach to the subprocess using ptrace ().",
		     &setlist);
  add_show_from_set (cmd, &showlist);		
  
  cmd = add_set_cmd ("inferior-ptrace-on-attach", class_obscure, var_boolean, 
		     (char *) &inferior_ptrace_on_attach_flag,
		     "Set if GDB should attach to the subprocess using ptrace ().",
		     &setlist);
  add_show_from_set (cmd, &showlist);		
  
  cmd = add_set_cmd ("inferior-auto-start-dyld", class_obscure, var_boolean, 
		     (char *) &inferior_auto_start_dyld_flag,
		     "Set if GDB should enable debugging of dyld shared libraries.",
		     &setlist);
  add_show_from_set (cmd, &showlist);
  set_cmd_sfunc (cmd, macosx_set_auto_start_dyld);

#if WITH_CFM
  cmd = add_set_cmd ("inferior-auto-start-cfm", class_obscure, var_boolean, 
		     (char *) &inferior_auto_start_cfm_flag,
		     "Set if GDB should enable debugging of CFM shared libraries.",
		     &setlist);
  add_show_from_set (cmd, &showlist);
#endif /* WITH_CFM */

  add_com ("update", class_obscure, update_command,
	   "Re-read current state information from inferior.");
}
