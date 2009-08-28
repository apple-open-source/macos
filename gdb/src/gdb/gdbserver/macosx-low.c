#include <fcntl.h>
#include <mach/mach.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/ptrace.h>
#include <sys/termios.h>
#include <unistd.h>

#include "macosx-low.h"
#include "macosx-mutils.h"

#include "gdb/signals.h"

static unsigned char macosx_translate_exception (struct macosx_exception_thread_message *msg);

int low_debuglevel = 0;
extern unsigned long signal_pid;
static int last_sent_signal = 0;


static int gdbserver_has_a_terminal (void);
static void terminal_inferior (void);
static void terminal_ours (void);

/* Record terminal status separately for debugger and inferior.  */

static int terminal_is_ours = 0;
static int attached_to_process = 0;

/* TTY state for the inferior.  We save it whenever the inferior stops, and
   restore it when it resumes.  */
static int inferior_ttystate_err = -1;
static struct termios inferior_ttystate;
static pid_t inferior_process_group = -1;
static int inferior_tflags;

/* Our own tty state, which we restore every time we need to deal with the
   terminal.  We only set it once, when GDB first starts.  The settings of
   flags which readline saves and restores and unimportant.  */
static int our_ttystate_err = -1;
static struct termios our_ttystate;
static pid_t our_process_group = -1;
static int our_tflags;

static void
macosx_low_debug (int level, const char *fmt, ...)
{
  va_list ap;
  if (low_debuglevel >= level)
    {
      va_start (ap, fmt);
      printf ("[%d/%d macosx-low]: ", getpid (), mach_thread_self ());
      vprintf (fmt, ap);
      va_end (ap);
      fflush (stdout);
    }
}

/* mach_check_error & friends comes from macosx-tdep.c.  */
void
mach_check_error (kern_return_t ret, const char *file,
                  unsigned int line, const char *func)
{
  if (ret == KERN_SUCCESS)
    {
      return;
    }
  if (func == NULL)
    {
      func = "[UNKNOWN]";
    }

  error ("error on line %u of \"%s\" in function \"%s\": %s (0x%lx)\n",
         line, file, func, MACH_ERROR_STRING (ret), (unsigned long) ret);
}

void
mach_warn_error (kern_return_t ret, const char *file,
                 unsigned int line, const char *func)
{
  if (ret == KERN_SUCCESS)
    {
      return;
    }
  if (func == NULL)
    {
      func = "[UNKNOWN]";
    }

  warning ("error on line %u of \"%s\" in function \"%s\": %s (0x%ux)",
           line, file, func, MACH_ERROR_STRING (ret), ret);
}

static struct macosx_process_info *create_process (pid_t pid);

/* 
   The gdbserver code is a bit opaque because it (probably intentionally)
   makes a fuzzy distinction between threads and processes.  But on
   Mac OS X, threads aren't process-like.  We spawn ONE process, and it
   has a bunch of threads.  That's all.  So I will store the information
   for the current process in the macosx_thread_status.  Then
   every thread will get a pointer to the status for the current process.  */

struct macosx_process_info current_macosx_process;

/* create_process does the same thing as "add_process" in the 
   linux-low code.  There one "process" might have many LWP, so you
   may need to add more than one process.  But on Mac OS X, we have
   only one process, and then many threads.  */
   
static struct macosx_process_info *
create_process (int pid)
{
  struct macosx_process_info *process;
  kern_return_t kret;

  process = &current_macosx_process;

  memset (process, 0, sizeof (struct macosx_process_info));

  process->pid = pid;

  /* Now start up the thread that's going to watch the
     exception port.  */

  process->status = (macosx_exception_thread_status *)
    malloc (sizeof (macosx_exception_thread_status));
  macosx_exception_thread_init (process->status);

  kret = task_for_pid (mach_task_self (), pid, &(process->status->task));
  
  if (kret != KERN_SUCCESS)
    {
      error ("Unable to find Mach task port for process-id %d: %s (0x%lx).",
             pid, MACH_ERROR_STRING (kret), (unsigned long) kret);
    }

  macosx_exception_thread_create (process->status);

  return process;
}

struct mach_thread_list
{
  thread_array_t thread_list;
  unsigned int nthreads;
};

void
check_native_thread_exists (struct inferior_list_entry *entry, void *data)
{
  struct mach_thread_list *threads = (struct mach_thread_list *) data;
  int i;
  int found_it = 0;
  for (i = 0; i < threads->nthreads; i++)
    {
      if (entry->id == threads->thread_list[i])
	{
	  found_it = 1;
	  break;
	}
    }
  if (!found_it)
    {
      remove_thread ((struct thread_info *) entry);
    }
}

void
macosx_check_new_threads (struct macosx_process_info *process)
{
  thread_array_t thread_list = NULL;
  unsigned int nthreads = 0;
  kern_return_t kret;
  int i;
  struct mach_thread_list threads;

  /* We will need to free THREAD_LIST with a call to VM_DEALLOCATE.  */
  kret = task_threads (process->status->task, &thread_list, &nthreads);
  MACH_CHECK_ERROR (kret);

  /* First cull the threads that have exited.  */
  threads.nthreads = nthreads;
  threads.thread_list = thread_list;
  for_each_inferior_data (&all_threads, &threads,  check_native_thread_exists);

  /* Then add the new threads.  */
  for (i = 0; i < nthreads; i++)
    {
      if (find_inferior_id (&all_threads, thread_list[i]) == NULL)
	{
	  struct macosx_thread_info *new_thread = 
	    (struct macosx_thread_info *) malloc (sizeof (struct macosx_thread_info));
	  new_thread->process = process;
	  /* FIXME - should probably get the user thread id too...  
	     FIXME: The FSF added this "gdb_id" argument, which seems to be the pid.  But
	     it also looks like they use it to match what's sent with the vCont message.  
	     But that's supposed to be a TID.  So I'm redundantly supplying the thread id.  */
	  add_thread (thread_list[i], new_thread, thread_list[i]);
	}
    } 
  /* Free the memory given to use by the TASK_THREADS kernel call.  */
  kret = vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
                        (vm_size_t) (nthreads * sizeof (thread_t)));
  MACH_CHECK_ERROR (kret);
}

static int
wait_for_stop (pid_t pid)
{
  int status;
  pid_t wait_return;

  wait_return = waitpid (pid, &status, WUNTRACED);
  if (wait_return == -1)
    {  
      perror ("Error waiting for child to start up");
      return -1;
    }
  return wait_return;

}

/* Start an inferior process and returns its pid.
   ALLARGS is a vector of program-name and args. */

static int
macosx_create_inferior (char *program, char **allargs)
{
  struct macosx_process_info *new_process;
  pid_t pid;

  /* Set a flag indicating if we did not attach.  */
  attached_to_process = 0;

  /* Check if we have a tty, and if we do, remember our settings.  */
  gdbserver_has_a_terminal ();

  pid = fork ();
  if (pid < 0)
    perror_with_name ("fork");

  if (pid == 0)
    {
      /* Child process.  */
      ptrace (PT_TRACE_ME, 0, 0, 0);
      ptrace (PT_SIGEXC, 0, 0, 0);

      /* signal (__SIGRTMIN + 1, SIG_DFL); */

      /* Set the process group ID of the child process (from within the
         child process) to be in it's own process group. One of the 
	 SETPGID calls in the parent or child process will be redundant, 
	 but both are needed to avoid a race condition since we don't 
	 know which process will get to execute first. Passing zero for
	 the pid and pgrp will set the child's process group ID to match
	 its pid. */
      setpgid (0, 0);

      /* I am not sure why I need to sleep a bit here...  
	 But if I don't then the gdbserver goes comatose, and
	 actually locks up the terminal...  */
      sleep (1);
      execv (program, allargs);

      error ("Cannot exec %s: %s.\n", program,
	       strerror (errno));
    }
  
  
  /* Set the process group ID of the child process (from the parent
     process) to be in it's own process group. One of the SETPGID calls 
     in the parent or child process will be redundant, but both are 
     needed to avoid a race condition since we don't know which process 
     will get to execute first.  */
  setpgid (pid, pid);

  new_process = create_process (pid);
  macosx_check_new_threads (new_process);

  return pid;
}

/* FIXME - This hasn't been tested yet!  */
static int
macosx_attach (unsigned long pid)
{
  struct macosx_process_info *new_process;

  /* Check if we have a tty, and if we do, remember our settings.  */
  gdbserver_has_a_terminal ();

  /* Attach to running process with signals as exceptions.  */
  if (ptrace (PT_ATTACHEXC, pid, 0, 0) != 0)
    {
      fprintf (stderr, "Cannot attach to process %d: %s (%d)\n", pid,
	       strerror (errno), errno);
      fflush (stderr);

      return -1;
    }

  /* Set a flag indicating we attached.  */
  attached_to_process = 1;
  new_process = create_process (pid);
  macosx_check_new_threads (new_process);
  return 0;
}

static void
macosx_kill (void)
{
  ptrace (PT_KILL, signal_pid, 0, 0);
}

static void
macosx_detach (void)
{
  ptrace (PT_DETACH, signal_pid, 0, 0);
}

static int
macosx_thread_alive (unsigned long tid)
{
  int alive = 0;
  if (current_macosx_process.status)
    alive = macosx_thread_valid ( current_macosx_process.status->task, tid );
  macosx_low_debug (6, "macosx_thread_alive (T%8.8x) returning %d (all_threads = %d)\n", tid, alive, find_inferior_id (&all_threads, tid) != NULL);
  return alive;
}

/* This global pointer is not my fault, the linux code does this
   as well.  for_each_inferior should really take a void *.  */

struct thread_resume *resume_ptr;

static void
macosx_process_resume_requests (struct inferior_list_entry *entry)
{
  struct thread_info *thread = (struct thread_info *) entry;
  struct macosx_process_info *process;
  struct macosx_thread_info *macosx_thread
    = inferior_target_data (thread);
  int index = 0;
  int ret;
  
  process = get_thread_process (thread);

  /* Why doesn't the upper layers do this?  */
  regcache_invalidate_one (entry);

  
  /* We only call ptrace to update the thread if we were stopped by 
     a soft signal.  Otherwise we'll get an error from the kernel.  
     FIXME: How do we continue a thread with a signal if we weren't
     originally stopped in softexc?  */
  if (process->status->stopped_in_softexc && process->stopped_thread == entry->id)
    {
      int sig;

      /* Only one resume_ptr entry with thread of -1 means apply this
	 to all threads.  Otherwise, if the thread's not in the resume
	 request, continue it with a signal of 0.  */
      if (resume_ptr[0].thread == -1)
	sig = resume_ptr[0].sig;
      else
	{
	  while (resume_ptr[index].thread != -1 
		 && resume_ptr[index].thread != entry->id)
	    index++;
	  if (resume_ptr[index].thread == -1)
	    sig = 0;
	  else
	    sig = resume_ptr[index].sig;
	}
      
      macosx_low_debug (6, "Updating 0x%x with signal %d\n", entry->id, sig);
      ret = ptrace (PT_THUPDATE, process->pid, (caddr_t) entry->id,  
		    sig);
      if (ret != 0)
	perror ("Error calling PT_THUPDATE");
    }

  if (process->stepping)
    {
      if (process->thread_to_step != entry->id)
	{
	  macosx_low_debug (6, "Suspending thread 0x%x\n", entry->id);
	  thread_suspend (entry->id);
	  macosx_thread->suspend_count++;
	}
      else 
	{
	  macosx_low_debug (6, "Single stepping thread 0x%x\n", entry->id);
	  the_low_target.low_single_step_thread (entry->id, 1);
	  /* Make sure this thread is not suspended... */
	  while (macosx_thread->suspend_count > 0)
	    {
	      thread_resume (entry->id);
	      macosx_thread->suspend_count--;
	    }
	}
    }
  else
    {
      /* Make sure this thread is not suspended.  */
      while (macosx_thread->suspend_count > 0)
	{
	  thread_resume (entry->id);
	  macosx_thread->suspend_count--;
	}
      the_low_target.low_single_step_thread (entry->id, 0);
      
    }
}

void
macosx_resume (struct thread_resume *resume_info)
{
  
  struct macosx_process_info *process;
  unsigned char charbuf[1] = { 0 };
  int index;

  resume_ptr = resume_info;

  process = get_thread_process (current_inferior);

  
  /* Go through the resume info and figure out if we are stepping
     or not, and which thread we are stepping.  Stash that in the
     process, and then run through all the threads to set them up.  */

  index = 0;
  process->stepping = 0;
  while (resume_info[index].thread != -1)
    {
      macosx_low_debug (6, "macosx_resume resume_info[%d] T%8.8x leave_stopped=%d, step=%d, signal=%d\n", index, resume_info[index].thread, resume_info[index].leave_stopped, resume_info[index].step, resume_info[index].sig);
      if (resume_info[index].step == 1)
	{
	  if (process->stepping == 1)
	    {
	      warning ("Can only step one thread at a time.  "
		       "Currently stepping %d, asked to step %d\n",
		       process->thread_to_step,
		       resume_info[index].thread);
	    }
	  else
	    {
	      process->stepping = 1;
	      process->thread_to_step = resume_info[index].thread;
	    }
	}
      index++;
    }
  macosx_low_debug (6, "macosx_resume resume_info[%d] T%8.8x leave_stopped=%d, step=%d, signal=%d\n", index, resume_info[index].thread, resume_info[index].step, resume_info[index].sig);

  terminal_inferior ();
  block_async_io ();
  enable_async_io ();
  for_each_inferior (&all_threads, macosx_process_resume_requests);
  /* If we got a step request, suspend all the other threads.  */
  
  write (process->status->transmit_to_fd, charbuf, 1);


  process->status->stopped_in_softexc = 0;
  macosx_low_debug (6, "Called macosx_resume\n");
}


static void
macosx_add_to_port_set (struct macosx_exception_thread_status *excthread,
                        fd_set * fds)
{
  FD_ZERO (fds);

  if (excthread->receive_from_fd > 0)
    {
      FD_SET (excthread->receive_from_fd, fds);
    }
  if (excthread->error_receive_fd > 0)
    {
      FD_SET (excthread->error_receive_fd, fds);
    }
}

static unsigned char 
macosx_translate_exception (struct macosx_exception_thread_message *msg)
{
  /* FIXME: We should check for new threads here.  */

  /* Now decode the exception message, and turn it into
     stop signal numbers that will make the upper layers
     happy.  */
  switch (msg->exception_type)
    {
    case EXC_BAD_ACCESS:
      return TARGET_SIGNAL_BUS;
      // return TARGET_EXC_BAD_ACCESS;
      break;
    case EXC_BAD_INSTRUCTION:
      return TARGET_SIGNAL_ILL;
      // return TARGET_EXC_BAD_INSTRUCTION;
      break;
    case EXC_ARITHMETIC:
      return TARGET_EXC_ARITHMETIC;
      break;
    case EXC_EMULATION:
      return TARGET_EXC_EMULATION;
      break;
    case EXC_SOFTWARE:
      {
        switch (msg->exception_data[0])
          {
          case EXC_SOFT_SIGNAL:
            return (unsigned char) msg->exception_data[1];
            break;
          default:
            return TARGET_EXC_SOFTWARE;
            break;
          }
      }
      break;
    case EXC_BREAKPOINT:
      /* Many internal GDB routines expect breakpoints to be reported
         as TARGET_SIGNAL_TRAP, and will report TARGET_EXC_BREAKPOINT
         as a spurious signal. */
      return TARGET_SIGNAL_TRAP;
      break;
    default:
      return TARGET_SIGNAL_UNKNOWN;
      break;
    }

}

/* This enum indicates the source for events.  This is
   different from the event type, since we can get signal
   events, for instance, from the exception source...  */

enum macosx_event_source 
  {
    MACOSX_SOURCE_NONE,
    MACOSX_SOURCE_ERROR,
    MACOSX_SOURCE_EXCEPTION,
    MACOSX_SOURCE_EXITED,
    MACOSX_SOURCE_STOPPED,
    MACOSX_SOURCE_SIGNALED
  };

/* This is the actual event type.  Some of these are
   synthetic, for instance the SINGLESTEP is a breakpoint
   event for the thread we were single-stepping...  */

enum macosx_event_type
  {
    MACOSX_TYPE_UNKNOWN,
    MACOSX_TYPE_EXCEPTION,
    MACOSX_TYPE_SIGNAL,
    MACOSX_TYPE_BREAKPOINT,
    MACOSX_TYPE_SINGLESTEP
  };

/* This stuff is for managing the queue of simultaneous events.
   We don't actually hold events past the call to macosx_wait.
   You can't really do that, because the user might ask you to
   do something that starts the target again (like call a function).
   And you can't restart the target without replying to all the 
   messages outstanding.  So instead we push back the events that
   we aren't going to respond to.  */

struct macosx_event
{
  enum macosx_event_source source;
  enum macosx_event_type type;
  char *data;
  struct macosx_event *next;
  struct macosx_event *prev;
};

static struct macosx_event *macosx_add_to_events (struct macosx_process_info *inferior,
						  enum macosx_event_source source,
						  char *data);
static void macosx_clear_events ();


/* These two variables hold the head & tail of the static event chain.  */
static struct macosx_event *macosx_event_chain, *macosx_event_tail;

static enum macosx_event_type
macosx_exception_event_type (struct macosx_process_info *inferior,
			     macosx_exception_thread_message *mssg)
{
  if (inferior->stepping
      && (mssg->exception_type == EXC_BREAKPOINT)
      && (inferior->thread_to_step == mssg->thread_port))
    return MACOSX_TYPE_SINGLESTEP;

  if (mssg->exception_type == EXC_BREAKPOINT)
    return MACOSX_TYPE_BREAKPOINT;

  if (mssg->exception_type == EXC_SOFTWARE 
      && mssg->data_count == 2 
      && mssg->exception_data[0] == EXC_SOFT_SIGNAL)
    return MACOSX_TYPE_SIGNAL;

  return MACOSX_TYPE_EXCEPTION;
}

static struct macosx_event *
macosx_add_to_events (struct macosx_process_info *inferior,
		      enum macosx_event_source source,
		      char *data)
{
  struct macosx_event *new_event;

  new_event = (struct macosx_event *)
    malloc (sizeof (struct macosx_event));
  new_event->type = MACOSX_TYPE_UNKNOWN;
  new_event->source = source;

  if (source == MACOSX_SOURCE_EXCEPTION)
    {
      macosx_exception_thread_message *mssg;
      mssg = (macosx_exception_thread_message *)
        malloc (sizeof (macosx_exception_thread_message));
      memcpy (mssg, data, sizeof (macosx_exception_thread_message));
      macosx_low_debug (6, "macosx_add_to_events: adding an exception event "
		      "to the pending events.\n");
      new_event->data = (void *) mssg;
      new_event->type = macosx_exception_event_type (inferior, mssg);
    }
  else if (source == MACOSX_SOURCE_STOPPED)
    {
      macosx_low_debug (6, "macosx_add_to_events: Adding a stopped event.\n");
      new_event->data = data;
      new_event->type = MACOSX_TYPE_SIGNAL;
    }
  else
    {
      /* For now we are only adding exception events. */
      error ("Unrecognized event type in macosx_add_to_events.\n");
    }

  
  if (macosx_event_chain == NULL)
    {
      macosx_event_chain = new_event;
      macosx_event_tail = new_event;
      new_event->prev = NULL;
      new_event->next = NULL;
    }
  else
    {
      new_event->prev = macosx_event_tail;
      new_event->next = NULL;
      macosx_event_tail->next = new_event;
      macosx_event_tail = new_event;
    }
  return new_event;
}

/* macosx_service_event sends EVENT to the gdb event queue.  It
   returns the resume response that the upper layers of the server
   expect.  */
static unsigned char
macosx_service_event (struct macosx_process_info *inferior, 
		      struct macosx_event *event)
{
  macosx_exception_thread_message *mssg =
    (macosx_exception_thread_message *) event->data;
  struct inferior_list_entry *this_inferior;

  /* Switch the current thread to the one we are servicing.  */
  this_inferior = find_inferior_id (&all_threads, mssg->thread_port);
  if (this_inferior == NULL)
    {
      warning ("The inferior for this thread has gone away");
      abort ();
    }

  current_inferior = (struct thread_info *) this_inferior;

  /* Now clear the stepping state.  If the kernel can single step
     on this processor, then we just unset the stepping flag.
     Otherwise, we just delete the single stepping breakpoint.
     If we happened to stop in another thread, tough luck for now.
     I'm just going to report the stop and be done with it.

     */
  if (inferior->stepping)
    {
      the_low_target.low_clear_single_step (mssg->thread_port);
      inferior->stepping = 0;
    }

  if (event->source == MACOSX_SOURCE_EXCEPTION)
    {
      inferior->stopped_thread = mssg->thread_port;
      if (event->type == MACOSX_TYPE_SIGNAL)
	inferior->status->stopped_in_softexc = 1;
      return macosx_translate_exception (mssg);
    }
  else if (event->source == MACOSX_SOURCE_STOPPED)
    {
      return (int) event->data;
    }
  else
    error ("Message of unknown source: %d passed to macosx_service_event.\n", 
	   event->source);
	
  return (unsigned char) 0;

}

static void
macosx_free_event (struct macosx_event *event_ptr)
{
  if (event_ptr->source == MACOSX_SOURCE_EXCEPTION)
    free (event_ptr->data);
  free (event_ptr);
}

static void
macosx_clear_events ()
{
  struct macosx_event *event_ptr = macosx_event_chain;

  while (event_ptr != NULL)
    {
      macosx_event_chain = event_ptr->next;
      macosx_free_event (event_ptr);
      event_ptr = macosx_event_chain;
    }
}

/* FIXME: This is a place holder right now.  For ARM & PPC,
   the pc is left at the trap address when the trap is hit,
   so we have nothing to do here.  On x86, the PC is moved
   over the trap.  So we would have to back it up in that 
   case.  */
void
macosx_backup_threads_before_break (struct macosx_event *event_ptr)
{
  return;
}

/* We keep a signal handler active for SIGCHLD.  That way if one of the waitpid
   events is delivered for the child when we aren't waiting in select, we can
   test for it before entering select.  */

static int got_sigchld;

static void
sigchld_handler (int signo)
{
  macosx_low_debug (6, "Called sigchld_handler (%d)\n", signo);
  got_sigchld = 1;
}

static int
check_for_sigchld ()
{
  int retval = got_sigchld;
  got_sigchld = 0;
  return retval;
}

/* This is the place where we use select to wait for
   events from the target.  We wait for the given TIMEOUT.
   If message is an exception, or an error, we write the
   message into BUF.  If we are stopped, exited or signaled,
   then the exit code or signal is written in RETVAL.  */

enum macosx_event_source
macosx_fetch_event (struct macosx_process_info *inferior,
		     int timeout,
		     char *buf,
		     int *retval)
{
  fd_set fds;
  int fd, ret;
  struct timeval tv;
  struct timeval *tv_ptr;

  int bypass_select = 0;
  int select_errno = 0;
  unsigned char check_for_exc_signal = 0;
  static macosx_exception_thread_message saved_msg = {0};
  *retval = 0;
  macosx_low_debug (6, "macosx_fetch_event called with timeout %d.\n", timeout);

  /* Check if we have a saved mach exception after a previous call to this
     function had select interrupted by a signal that didn't have a 
     matching exception posted to the exception thread.  */
  if (saved_msg.task_port != MACH_PORT_NULL)
    {
      macosx_low_debug (6, "Getting mach exception event from cache.\n");
      memcpy (buf, &saved_msg, sizeof (saved_msg));
      /* Invalidate the task port sot this cached exception doesn't get 
         used again.  */
      saved_msg.task_port = MACH_PORT_NULL;
      *retval = 0;
      return MACOSX_SOURCE_EXCEPTION;
    }

  if (timeout == -1)
    tv_ptr = NULL;
  else
    {
      tv.tv_sec = 0;
      tv.tv_usec = timeout;
      tv_ptr = &tv;
    }
  
  for (;;)
    {
      macosx_add_to_port_set (inferior->status, &fds);
      select_errno = 0;
  
      if (check_for_sigchld ())
	{
	  macosx_low_debug (6, "sigchild handler got a hit before"
			    " entering select\n");
	  bypass_select = 1;
	}
      else
        {
          ret = select (FD_SETSIZE, &fds, NULL, NULL, tv_ptr);
	  /* Save errno from our select call in case any other system
	     functions mess with it before we use it.  */
	  if (ret < 0)
	    select_errno = errno;
        }

      macosx_low_debug (6, "Woke up from select: bypass: %d ret: %d errno %d.\n",
			bypass_select, ret, select_errno);

      /* If we didn't call select, or if select was interrupted, we need to
         check for other special reasons we may have stopped.  */
      if (bypass_select || select_errno == EINTR)
        {
	  pid_t retpid;
	  int wstatus;

	  /* If we got interrupted, check to make sure it wasn't
	     because the process died.  Don't call waitpid with  */
	  retpid = waitpid (-1, &wstatus, WNOHANG | WUNTRACED );
	  if (retpid == 0)
	    continue;
	  if (retpid != inferior->pid)
	    {
	      /* This signal was not for our inferior process, ignore it.  */
	      macosx_low_debug (6, "Interrupted for signal for pid %d\n", retpid);
	      continue;
	    }
	  if (WIFEXITED (wstatus))
	    {
	      macosx_low_debug (6, "Process %d exited with status %d\n", 
		      retpid, WEXITSTATUS (wstatus));
	      *retval = WEXITSTATUS (wstatus);
	      return MACOSX_SOURCE_EXITED;
	    }
	  else if (WIFSIGNALED (wstatus))
	    {
	      macosx_low_debug (6, "Process %d terminated with signal %d\n", 
		      retpid, WTERMSIG (wstatus));
	      *retval = WTERMSIG (wstatus);
	      return MACOSX_SOURCE_SIGNALED;
	    }
	  else if (WIFSTOPPED (wstatus))
	    {
	      macosx_low_debug (6, "Process %d stopped with signal %d\n", 
		      retpid, WSTOPSIG (wstatus));
	      *retval = WSTOPSIG (wstatus);
	      
	      /* When a signal is sent to gdb and passed on to gdbserver, 
	         macosx_send_signal () gets called which will issue a 
		 kill (pid, signo) where PID is the process ID of the 
		 inferior. This can cause the select function call above to
		 return with an error code of -1 and errno set to EINTR. 
		 This indicates that the system call was interrupted. We
		 may also get a matching mach exception posted to our 
		 exception thread. We don't want to return that we stopped 
		 due to a signal in this case because we could end up 
		 getting another identical mach exception version for this
		 signal the next time through this function. So for now 
		 we note that we found this kind of signal, and we will
		 check our exception thread queue and see if we did indeed
		 get a duplicate.  */
	      if (*retval == last_sent_signal && !bypass_select 
		  && select_errno == EINTR)
		{
		  macosx_low_debug (6, "select () was interrupted with signal "
				    "%d, check for a matching mach exception "
				    "message.\n", *retval);
		  check_for_exc_signal = last_sent_signal;
		}
	      else
		return MACOSX_SOURCE_STOPPED;
	    }
          continue;
        }
      if (ret < 0)
        {
          error ("unable to select: %s", strerror (select_errno));
        }
      if (ret == 0)
        {
	  return MACOSX_SOURCE_NONE;
        }
      break;
    }

  fd = inferior->status->error_receive_fd;
  if (fd > 0 && FD_ISSET (fd, &fds))
    {
      macosx_low_debug (6, "Reading event from error fd.\n");
      read (fd, buf, 1);
      return MACOSX_SOURCE_ERROR;
    }

  fd = inferior->status->receive_from_fd;
  if (fd > 0 && FD_ISSET (fd, &fds))
    {
      macosx_low_debug (6, "Reading event from exception thread.\n");
      read (fd, buf, sizeof (macosx_exception_thread_message));

      /* Did select get interrupted by a signal?  */
      if (check_for_exc_signal != 0)
	{
	  /* Yes it did, check for a duplicate signal in our exception
	     thread queue.  */
	  macosx_exception_thread_message *msg = (macosx_exception_thread_message *)buf;
	  if (macosx_exception_event_type (inferior, msg) == MACOSX_TYPE_SIGNAL)
	    {
	      /* We got a signal from the from the exception thread that we need
		 to check against the one we are looking for.  */
		 
	      if (((unsigned char) msg->exception_data[1]) == check_for_exc_signal)
		{
		  /* We got a signal from the exception thread that matches the
		     one that interrupted select. We can disregard the signal 
		     we detected by inspecting wstatus and return the one from
		     the exception thread. */
		  macosx_low_debug (6, "Matching mach exception event received "
					"for signal %d.\n", check_for_exc_signal);
		     
		  /* Reset the LAST_SENT_SIGNAL if we find a match so that it 
		     doesn't get used again.  */
		  last_sent_signal = 0;

		  /* Reset the *RETVAL to zero for the MACOSX_SOURCE_EXCEPTION 
		     return code below.  */
		  *retval = 0;
		}
	      else
		{
		  /* We found something in the exception thread that doesn't
		     match the reason that select was interrupted. We need to 
		     cache the exception message we just read for the next 
		     call to this function and return MACOSX_SOURCE_STOPPED 
		     with *RETVAL containing the signal number.  */
		     
		  macosx_low_debug (6, "Exception thread event signal (%d) doesn't "
				    "match the reason select was interrupted "
				    "(%d).\n",
				    (unsigned char) msg->exception_data[1], *retval);

		  /* Save the mach exception message buffer so we can
		     re-use it next time.  */
		  memcpy(&saved_msg, msg, sizeof(saved_msg));
		  return MACOSX_SOURCE_STOPPED;
		}
	    }
	}
      return MACOSX_SOURCE_EXCEPTION;
    }

  /* We shouldn't get here... */

  return MACOSX_SOURCE_ERROR;
}

static unsigned char
macosx_wait_for_event (struct thread_info *child, 
		       char *status, 
		       unsigned int timeout)
{
  struct macosx_process_info *inferior;
  enum macosx_event_source type;
  char buf[1024];
  int val;
  int event_count, keep_looking;
  int first_time;
  struct macosx_event *stepping_event = NULL, *signal_event = NULL;
  unsigned char retval;

  macosx_low_debug (6, "Called macosx_wait_for_event\n");

  inferior = get_thread_process (current_inferior);
 
  macosx_clear_events (inferior);

  /* N.B. event_count is not necessarily the number of events that
     actually get added.  For instance, we sometimes break out of
     select in macosx_fetch_event with a WIFSTOPPED message for a 
     signal that we ALSO get an EXC_SOFTWARE signal for.  GO FIGURE...
     That signal event we should just ignore, so I don't even bother
     to queue it up (which then keeps me from having to ignore it
     later.  */
  event_count = 0;
  keep_looking = 1;
  first_time = 1;
  /* In multi-threaded apps on Mac OS X, we you will occasionally get
     more than one exception event queued up before the first one is
     delivered to us.  This can cause a problem, for instance if we go
     to single step one thread over a breakpoint.  If we don't drain
     all the exceptions, then as soon as we restart the target, it's
     going to hit another exception immediately.  SO...  what we do is
     wait with whatever timeout we want for the first exception, then
     set the timeout to 0 and poll for whatever events remain.  Then
     we will handle one of these events, and push the others back to
     the target.  */
  while (keep_looking)
    {
      type = macosx_fetch_event (inferior, timeout, buf, &val);

      /* We're going to sit in select till we get the first event.  Then
	 make sure we can get the write lock here.  That will make sure
	 we don't pass out of this loop while the exception thread is still
	 in the process of writing more data.  Note, only do this the
         first time through, since you can't grab a lock from yourself... */
      if (first_time && type != MACOSX_SOURCE_EXITED)
	{
	  first_time = 0;
	  macosx_check_new_threads (inferior);
	  macosx_exception_get_write_lock (inferior->status);
	}
      switch (type)
	{
	  /* Note, if we were to get an exception event, and then 
	     notification that the target has exited without running
	     in-between, we're going to drop the exception event on
	     the floor here.  There's not much we could do with it,
	     so this isn't a big deal.  */
	  
	case MACOSX_SOURCE_EXITED:
	  *status = 'W';
	  macosx_exception_release_write_lock (inferior->status);
	  return val;
	case MACOSX_SOURCE_SIGNALED:
	  *status = 'X';
	  macosx_exception_release_write_lock (inferior->status);
	  return val;
	case MACOSX_SOURCE_STOPPED:
	  {
	    struct macosx_event *event;
	    warning ("Got stopped on signal not from SOFTEXC.\n");
	    event = macosx_add_to_events (inferior, 
					  MACOSX_SOURCE_STOPPED,
					  (void *) val);
	    event_count++;
	    break;
	  }
	case MACOSX_SOURCE_EXCEPTION:
	  {
	    struct macosx_event *event;
	    event = macosx_add_to_events (inferior, 
					  MACOSX_SOURCE_EXCEPTION,
					  buf);
	    event_count++;
	    if (event->type == MACOSX_TYPE_SINGLESTEP)
	      stepping_event = event;
	    else if (event->type == MACOSX_TYPE_SIGNAL)
	      signal_event = event;
	    break;
	  }
	case MACOSX_SOURCE_NONE:
	  keep_looking = 0;
	  break;
	case MACOSX_SOURCE_ERROR:
	  warning ("Got an error fetching events.");
	  *status = 'W';
	  macosx_exception_release_write_lock (inferior->status);
	  return val;
	}
      timeout = 0;
    }
  
  /* Okay, now the exception thread is waiting for us to wake it 
     up.  We should release the lock so it will be able to
     acquire it when it wakes up & gets more data.  */

  macosx_exception_release_write_lock (inferior->status);

  /* Now we have to go through the events we gathered, and dispatch the
     one we are going to handle now to gdb, and push the others back to
     the target.  */

  if (event_count == 1)
    {
      switch (macosx_event_chain->source)
	{
	case MACOSX_SOURCE_EXCEPTION:
	  {
	    *status = 'T';
	    retval = macosx_service_event (inferior, macosx_event_chain);
	    break;
	  }
	case MACOSX_SOURCE_STOPPED:
	  {
	    *status = 'T';
	    
	    retval = (int) macosx_event_chain->data;
	    break;
	  }
	default:
	  error ("Only handling exceptions here");
	} 
    }
  else if (stepping_event != NULL)
    {
      macosx_low_debug (6, "More than one event, with stepping event.\n");
      macosx_backup_threads_before_break (stepping_event);
      retval = macosx_service_event (inferior, stepping_event);
      *status = 'T';
    }
  else if (signal_event != NULL)
    {
      /* For now, handle the signal event, and discard
	 the others...  Set the status to indicate an error,
         but we'll set it back when we find the exception event.  */
      macosx_backup_threads_before_break (NULL);
      retval = macosx_service_event (inferior, signal_event);
      *status = 'T';
    }
  else
    {
      struct macosx_event *event_ptr;
      int random_selector, nevents;
      /* At this point, we should only have breakpoint exception events.
	 We want to pick some random one of these, and handle it.  Just
	 in case some future change puts other types of events on the
	 queue, I'm going to go through and explicitly pick out a 
	 breakpoint event.  */

      nevents = 0;
      for (event_ptr = macosx_event_chain; event_ptr != NULL; 
	   event_ptr = event_ptr->next)
	{
	  if (event_ptr->source == MACOSX_SOURCE_EXCEPTION 
	      && event_ptr->type == MACOSX_TYPE_BREAKPOINT)
	    nevents++;
	}
      
      random_selector = (int) ((nevents * (double) rand ()) / (RAND_MAX + 1.0));
      for (event_ptr = macosx_event_chain; event_ptr != NULL; 
	   event_ptr = event_ptr->next)
	{
	  if (random_selector == 0)
	    break;
	  else
	    random_selector--;
	}
      
      macosx_backup_threads_before_break (event_ptr);
      retval = macosx_service_event (inferior, event_ptr);
      *status = 'T';
    }
  
  macosx_clear_events ();
  return retval;  
}

static unsigned char
macosx_wait (char *status)
{
  /* Use child to hold the thread that we are going to resume.  The
     Linux code stores a global that it uses to remember which child
     it wants to resume.  */
  unsigned char w;
  struct thread_info *child = NULL;

  enable_async_io ();
  unblock_async_io ();
  w = macosx_wait_for_event (child, status, -1);
  disable_async_io ();
  terminal_ours ();


  macosx_low_debug (6, "Called macosx_wait\n");
  return (unsigned char) w;
}

static void
macosx_fetch_registers (int regno)
{
  struct macosx_process_info *inferior;
  inferior = get_thread_process (current_inferior);
  
  the_low_target.low_fetch_registers (regno);
  macosx_low_debug (6, "Called macosx_fetch_registers (%d)\n", regno);
}

static void 
macosx_store_registers (int regno)
{
  struct macosx_process_info *inferior;
  inferior = get_thread_process (current_inferior);
  
  the_low_target.low_store_registers (regno);
  macosx_low_debug (6, "Called macosx_store_registers (%d)\n", regno);
}

static int
macosx_read_memory (CORE_ADDR memaddr, unsigned char *myaddr, int len)
{
  struct macosx_process_info *inferior;
  int result;

  inferior = get_thread_process (current_inferior);

  result = mach_xfer_memory (memaddr, myaddr, len, 0, inferior->status->task);

  macosx_low_debug (6, "Called macosx_read_memory\n");
  if (result > 0)
    return 0;
  else
    return errno;

}

static int
macosx_write_memory (CORE_ADDR memaddr, const unsigned char *myaddr, int len)
{
  struct macosx_process_info *inferior;
  int result; 

  inferior = get_thread_process (current_inferior);

  result = mach_xfer_memory (memaddr, myaddr, len, 1, inferior->status->task);

  macosx_low_debug (6, "Called macosx_write_memory\n");
  if (result > 0)
    return 0;
  else
    return errno;
}

static void
macosx_lookup_symbols (void)
{
  macosx_low_debug (6, "UNIMPLEMENTED: Called macosx_lookup_symbols\n");
}

static void
macosx_send_signal (int signum)
{
  macosx_low_debug (6, "Called macosx_send_signal -> kill (%d, %d);\n", 
		    signal_pid, signum);
  /* Save the last signal we sent to SIGNAL_PID in case it interrupts our
     select function call in macosx_fetch_event.  */
  last_sent_signal = signum;
  kill (signal_pid, signum);
}

static int
macosx_read_auxv (CORE_ADDR offset, unsigned char *myaddr, unsigned int len)
{
  macosx_low_debug (6, "UNIMPLEMENTED: Called macosx_read_auxv\n");
  return 0;
}

int
macosx_insert_watchpoint (char type, CORE_ADDR addr, int len)
{
  return 1;
}

int
macosx_remove_watchpoint (char type, CORE_ADDR addr, int len)
{
  return 1;
}

int
macosx_stopped_by_watchpoint (void)
{
  return 0;
}

CORE_ADDR
macosx_stopped_data_address (void)
{
  return 0;
}

/* Does GDBSERVER have a terminal (on stdin)?  */
static int
gdbserver_has_a_terminal (void)
{
  static enum
  {
    no = 0, yes = 1, have_not_checked = 2
  }
  we_have_a_terminal = have_not_checked;

  switch (we_have_a_terminal)
    {
    case no:
      return 0;

    case yes:
      return 1;

    case have_not_checked:
      /* Get all the current tty settings (including whether we have 
         a tty at all!).  We need to do this before the inferior is
	 launched or attached to. */
      we_have_a_terminal = no;
      if (isatty (STDIN_FILENO))
	{
	  our_tflags = fcntl (STDIN_FILENO, F_GETFL, 0);
	  our_ttystate_err = tcgetattr (STDIN_FILENO, &our_ttystate);

	  if (our_ttystate_err == 0)
	    {
	      we_have_a_terminal = yes;
	      our_process_group = tcgetpgrp (0);
	    }
	}
      macosx_low_debug (6, "%s () => %d\n", __FUNCTION__, 
			we_have_a_terminal == yes);
      return we_have_a_terminal == yes;

    default:
      /* "Can't happen".  */
      break;
    }
  return 0;
}

static void
terminal_inferior (void)
{
  if (gdbserver_has_a_terminal () && terminal_is_ours && 
      inferior_ttystate_err == 0)
    {
      int result;
      result = fcntl (STDIN_FILENO, F_SETFL, inferior_tflags);

      /* Because we were careful to not change in or out of raw mode in
         terminal_ours, we will not change in our out of raw mode with
         this call, so we don't flush any input.  */
      result = tcsetattr (STDIN_FILENO, TCSANOW, &inferior_ttystate);

      /* If attach_flag is set, we don't know whether we are sharing a
         terminal with the inferior or not.  (attaching a process
         without a terminal is one case where we do not; attaching a
         process which we ran from the same shell as GDB via `&' is
         one case where we do, I think (but perhaps this is not
         `sharing' in the sense that we need to save and restore tty
         state)).  I don't know if there is any way to tell whether we
         are sharing a terminal.  So what we do is to go through all
         the saving and restoring of the tty state, but ignore errors
         setting the process group, which will happen if we are not
         sharing a terminal).  */

      if (inferior_process_group >= 0)
	result = tcsetpgrp (STDIN_FILENO, inferior_process_group);
    }
  terminal_is_ours = 0;
}


static void
terminal_ours (void)
{
  if (gdbserver_has_a_terminal () == 0)
    return;

  if (terminal_is_ours == 0)
    {
      /* Ignore this signal since it will happen when we try to set the
         pgrp.  */
      void (*osigttou) () = NULL;
      int result;

      terminal_is_ours = 1;

      osigttou = (void (*)()) signal (SIGTTOU, SIG_IGN);

      inferior_ttystate_err = tcgetattr (STDIN_FILENO, &inferior_ttystate);

      /* FIXME: For MacOS X: if you are attaching then the most common
	 case is that the process you are attaching to is NOT running
	 from the same terminal.  In this case, the code below will
	 erroneously swap the PID that you got from the process with
	 the process group for gdb.  This will cause interrupting the
	 process to fail later on.  So we will just NOT do this when
	 we are attaching...  
	 In the long run, we should just not use this terminal code for
         MacOS X, since it is broken in a bunch of ways... */

      if (attached_to_process)
	inferior_process_group = -1;
      else
	inferior_process_group = tcgetpgrp (0);

      inferior_tflags = fcntl (STDIN_FILENO, F_GETFL, 0);

      result = tcsetattr (STDIN_FILENO, TCSANOW, &our_ttystate);

      if (our_process_group >= 0)
	result = tcsetpgrp (STDIN_FILENO, our_process_group);

      /* Is there a reason this is being done twice?  It happens both
         places we use F_SETFL, so I'm inclined to think perhaps there
         is some reason, however perverse.  Perhaps not though...  */
      result = fcntl (STDIN_FILENO, F_SETFL, our_tflags);
      
      /* Restore the previous signal handler.  */
      signal (SIGTTOU, osigttou);

    }
}

static struct target_ops macosx_target_ops = {
  macosx_create_inferior,
  macosx_attach,
  macosx_kill,
  macosx_detach,
  macosx_thread_alive,
  macosx_resume,
  macosx_wait,
  macosx_fetch_registers,
  macosx_store_registers,
  macosx_read_memory,
  macosx_write_memory,
  macosx_lookup_symbols,
  macosx_send_signal,
  macosx_read_auxv,
  macosx_insert_watchpoint,
  macosx_remove_watchpoint,
  macosx_stopped_by_watchpoint,
  macosx_stopped_data_address
};

int using_threads;
int debug_threads;

void
initialize_low (void)
{
  using_threads = 1;
  set_target_ops (&macosx_target_ops);
  init_registers ();
  signal (SIGCHLD, sigchld_handler);
#if 0 /* FIXME: Figure out what this does.  */
  linux_init_signals ();
#endif
}
