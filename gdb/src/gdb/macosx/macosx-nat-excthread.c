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
#include "gdbcmd.h"
#include "event-loop.h"
#include "inferior.h"
#include "exceptions.h"

#include "macosx-nat-inferior.h"
#include "macosx-nat-excthread.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-inferior-debug.h"
#include "macosx-nat-inferior-util.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <sys/time.h>
#include <sys/select.h>

#include <mach/mach_error.h>
#include <pthread.h>

/* We use this write_mutex to make sure the exception thread has
   finished writing all the exceptions it has gathered before the main
   thread reads them.  It's actually a little more complex than this,
   because the main thread is going to enter select first time round
   to wait till the exception thread wakes up.  Then it's going to
   call select with a 0 timeout to poll for other events.  We put the
   block between the first wake-up, and the subsequent reads.  
   Don't access it directly, but use the macosx_exception_get_write_lock
   and macosx_exception_release_write_lock functions.  */

static pthread_mutex_t write_mutex;

static FILE *excthread_stderr_re = NULL;
static int excthread_debugflag = 0;

#ifndef HAVE_64_BIT_MACH_EXCEPTIONS
#define mach_exc_server exc_server
#define MACH_EXCEPTION_CODES 0
#endif

extern boolean_t mach_exc_server (mach_msg_header_t * in, mach_msg_header_t * out);

/* This struct holds all the data for one mach message.  Since we might
   have to hold onto a sequence of messages while gdb thinks about them,
   we need this in a form we can put into an array conveniently.  We use
   unions for the hdr & data because the data extends beyond the header, 
   so we need to make room for it.  */
 
struct mach_msg_data
{
  union {
    mach_msg_header_t hdr;
    char data[1024];
  } msgin;
  union {
    mach_msg_header_t hdr;
    char data[1024];
  } msgout;
  macosx_exception_thread_message msgsend;
};

#define MACOSX_EXCEPTION_ARRAY_SIZE 10

/* This is the storage for the mach messages the exception thread
   fetches from the target.  They are allocated in the macosx_exception_thread_create
   and deallocated in macosx_exception_thread_destroy.  If we were ever to 
   have more than one exception thread, these should be thread-specific data,
   but I don't imagine we will ever really need to do that.  */

static struct mach_msg_data *msg_data = NULL;
static int msg_data_size = MACOSX_EXCEPTION_ARRAY_SIZE;

/* This either allocates space for the DATA_ARR if NULL is past
   in, or realloc's it to NUM_ELEM if not.  */

static void 
alloc_msg_data (struct mach_msg_data **data_arr, int num_elem)
{
  if (*data_arr == NULL)
    *data_arr = (struct mach_msg_data *) xmalloc (num_elem * sizeof (struct mach_msg_data));
  else
    *data_arr = (struct mach_msg_data *) xrealloc (*data_arr, 
						   num_elem * sizeof (struct mach_msg_data));
}

/* A re-entrant version for use by the signal handling thread */

static void
excthread_debug_re (int level, const char *fmt, ...)
{
  va_list ap;
  if (excthread_debugflag >= level)
    {
      va_start (ap, fmt);
      fprintf (excthread_stderr_re, "[%d excthread]: ", getpid ());
      vfprintf (excthread_stderr_re, fmt, ap);
      va_end (ap);
      fflush (excthread_stderr_re);
    }
}

static void excthread_debug_re_endline (int level)
{
  if (excthread_debugflag >= level)
    {
      fprintf (excthread_stderr_re, "\n");
      fflush (excthread_stderr_re);
    }
}

static void excthread_debug_exception (int level, mach_msg_header_t *msg)
{
  if (excthread_debugflag < level)
    return;

  fprintf (excthread_stderr_re, "{");
  fprintf (excthread_stderr_re, " bits: 0x%lx", (unsigned long) msg->msgh_bits);
  fprintf (excthread_stderr_re, ", size: 0x%lx", (unsigned long) msg->msgh_size);
  fprintf (excthread_stderr_re, ", remote-port: 0x%lx", (unsigned long) msg->msgh_remote_port);
  fprintf (excthread_stderr_re, ", local-port: 0x%lx", (unsigned long) msg->msgh_local_port);
  fprintf (excthread_stderr_re, ", reserved: 0x%lx", (unsigned long) msg->msgh_reserved);
  fprintf (excthread_stderr_re, ", id: 0x%lx", (unsigned long) msg->msgh_id);

  if (msg->msgh_size > 24)
    {
      const unsigned long *buf = ((unsigned long *) msg) + 6;
      unsigned int i;
      
      fprintf (excthread_stderr_re, ", data:");
      for (i = 0; i < (msg->msgh_size / 4) - 6; i++)
	fprintf (excthread_stderr_re, " 0x%08lx", buf[i]);
    }

  fprintf (excthread_stderr_re, " }");
}

static void excthread_debug_message (int level, macosx_exception_thread_message *msg)
{
  if (excthread_debugflag < level)
    return;
  
  fprintf (excthread_stderr_re, "{ task: 0x%lx, thread: 0x%lx, type: %s",
	   (unsigned long) msg->task_port,
	   (unsigned long) msg->thread_port,
	   unparse_exception_type (msg->exception_type));

  if ((msg->exception_type == EXC_SOFTWARE) &&
      (msg->data_count == 2) &&
      (msg->exception_data[0] == EXC_SOFT_SIGNAL))
    {
      const char *signame;
      signame = target_signal_to_name ((unsigned int) target_signal_from_host (msg->exception_data[1]));

#ifdef HAVE_64_BIT_MACH_EXCEPTIONS
      fprintf (excthread_stderr_re, ", subtype: EXC_SOFT_SIGNAL, signal: %s (%lld)",
	       signame, msg->exception_data[1]);
#else
      fprintf (excthread_stderr_re, ", subtype: EXC_SOFT_SIGNAL, signal: %s (%d)",
	       signame, msg->exception_data[1]);
#endif
    }
  else
    {
      unsigned int i;
      for (i = 0; i < msg->data_count; i++)
	{
	  fprintf (excthread_stderr_re, ", data[%d]: 0x%lx", i, (unsigned long) msg->exception_data[i]);
	}
    }

  fprintf (excthread_stderr_re, " }");
  fflush (excthread_stderr_re);
}

/* These two routines manage the exception lock we use to make sure
   the main thread doesn't start reading exception data before the
   exception thread is all the way done writing it.  */

#define EXCEPTION_WRITE_LOCK_LEVEL  6
void
macosx_exception_get_write_lock (macosx_exception_thread_status *s)
{
  
  if (excthread_debugflag >= EXCEPTION_WRITE_LOCK_LEVEL)
    {
      pthread_t this_thread = pthread_self ();
      if (this_thread == s->exception_thread)
	excthread_debug_re (6, "Acquiring write lock for exception thread\n");
      else
	inferior_debug (6, "Acquiring write lock for main thread\n");
    }
  pthread_mutex_lock (&write_mutex);
}

void 
macosx_exception_release_write_lock (macosx_exception_thread_status *s)
{
  if (excthread_debugflag >= EXCEPTION_WRITE_LOCK_LEVEL)
    {
      pthread_t this_thread = pthread_self ();
      if (this_thread == s->exception_thread)
	excthread_debug_re (6, "Releasing write lock for exception thread\n");
      else
	inferior_debug (6, "Releasing write lock for main thread\n");
    }
  pthread_mutex_unlock (&write_mutex);
}

static void macosx_exception_thread (void *arg);

static macosx_exception_thread_message *static_message = NULL;

kern_return_t
#ifdef HAVE_64_BIT_MACH_EXCEPTIONS
  catch_mach_exception_raise_state
#else
  catch_exception_raise_state
#endif
  (mach_port_t port,
   exception_type_t exception_type, mach_exception_data_t exception_data,
   mach_msg_type_number_t data_count, thread_state_flavor_t * state_flavor,
   thread_state_t in_state, mach_msg_type_number_t in_state_count,
   thread_state_t out_state, mach_msg_type_number_t out_state_count)
{
  return KERN_FAILURE;
}

kern_return_t
#ifdef HAVE_64_BIT_MACH_EXCEPTIONS
  catch_mach_exception_raise_state_identity
#else
  catch_exception_raise_state_identity
#endif
  (mach_port_t port, mach_port_t thread_port, mach_port_t task_port,
   exception_type_t exception_type, mach_exception_data_t exception_data,
   mach_msg_type_number_t data_count, thread_state_flavor_t * state_flavor,
   thread_state_t in_state, mach_msg_type_number_t in_state_count,
   thread_state_t out_state, mach_msg_type_number_t out_state_count)
{
  kern_return_t kret;

  kret = mach_port_deallocate (mach_task_self (), task_port);
  MACH_CHECK_ERROR (kret);
  kret = mach_port_deallocate (mach_task_self (), thread_port);
  MACH_CHECK_ERROR (kret);

  return KERN_FAILURE;
}

kern_return_t
#ifdef HAVE_64_BIT_MACH_EXCEPTIONS
  catch_mach_exception_raise
#else
  catch_exception_raise
#endif
  (mach_port_t port, mach_port_t thread_port, mach_port_t task_port,
   exception_type_t exception_type, mach_exception_data_t exception_data,
   mach_msg_type_number_t data_count)
{
#if 0
  kret = mach_port_deallocate (mach_task_self (), task_port);
  MACH_CHECK_ERROR (kret);
  kret = mach_port_deallocate (mach_task_self (), thread_port);
  MACH_CHECK_ERROR (kret);
#endif

  static_message->task_port = task_port;
  static_message->thread_port = thread_port;
  static_message->exception_type = exception_type;
  static_message->exception_data = exception_data;
  static_message->data_count = data_count;

  /* If the task is not the same, it is for our child process.
     In that case, return KERN_FAILURE so the exception will 
     get routed on to the child.  */
  extern macosx_inferior_status *macosx_status;
  if (macosx_status->task == task_port)
    return KERN_SUCCESS;
  else
    return KERN_FAILURE;
}

void
macosx_exception_thread_init (macosx_exception_thread_status *s)
{
  s->transmit_from_fd = -1;
  s->receive_from_fd = -1;
  s->transmit_to_fd = -1;
  s->receive_to_fd = -1;
  s->error_transmit_fd = -1;
  s->error_receive_fd = -1;

  s->inferior_exception_port = MACH_PORT_NULL;

  memset (&s->saved_exceptions, 0, sizeof (s->saved_exceptions));
  memset (&s->saved_exceptions_step, 0, sizeof (s->saved_exceptions_step));

  s->saved_exceptions_stepping = 0;
  s->exception_thread = THREAD_NULL;
  s->shutting_down = 0;
}

pthread_mutex_t excthread_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t excthread_cond = PTHREAD_COND_INITIALIZER;

void
macosx_exception_thread_create (macosx_exception_thread_status *s,
                                task_t task)
{
  int fd[2];
  int ret;
  kern_return_t kret;
  pthread_mutexattr_t attrib;
  struct gdb_exception e;

  ret = pipe (fd);
  CHECK_FATAL (ret == 0);
  s->transmit_from_fd = fd[1];
  s->receive_from_fd = fd[0];

  ret = pipe (fd);
  CHECK_FATAL (ret == 0);
  s->error_transmit_fd = fd[1];
  s->error_receive_fd = fd[0];

  ret = pipe (fd);
  CHECK_FATAL (ret == 0);
  s->transmit_to_fd = fd[1];
  s->receive_to_fd = fd[0];
  s->task = task;

  s->shutting_down = 0;

  kret =
    mach_port_allocate (mach_task_self (), MACH_PORT_RIGHT_RECEIVE,
                        &s->inferior_exception_port);
  MACH_CHECK_ERROR (kret);
  kret =
    mach_port_insert_right (mach_task_self (), s->inferior_exception_port,
                            s->inferior_exception_port,
                            MACH_MSG_TYPE_MAKE_SEND);
  MACH_CHECK_ERROR (kret);

  if (inferior_bind_exception_port_flag)
    {
      macosx_save_exception_ports (task, &s->saved_exceptions);

      kret = task_set_exception_ports
        (task,
         EXC_MASK_ALL,
         s->inferior_exception_port, EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES, THREAD_STATE_NONE);
      MACH_CHECK_ERROR (kret);
    }

  alloc_msg_data (&msg_data, msg_data_size);
 
  pthread_mutexattr_init (&attrib);
  pthread_mutex_init (&write_mutex, &attrib);

  pthread_mutex_lock (&excthread_mutex);
  TRY_CATCH (e, RETURN_MASK_ERROR)
    {
      s->exception_thread =
	gdb_thread_fork ((gdb_thread_fn_t) &macosx_exception_thread, s);
    }
  if (e.reason != NO_ERROR)
    {
      pthread_mutex_unlock (&excthread_mutex);
      throw_exception (e);
    }

  pthread_cond_wait (&excthread_cond, &excthread_mutex);
  pthread_mutex_unlock (&excthread_mutex);
}

void
macosx_exception_thread_destroy (macosx_exception_thread_status *s)
{
  if (s->exception_thread != THREAD_NULL)
    {

      /* Let's destroy the exception port here, so that we
	 will force the exception thread out of any mach_msg we
	 may be sitting in.  */

      s->shutting_down = 1;
      mach_port_destroy (mach_task_self (), s->inferior_exception_port);
      mach_port_destroy (mach_task_self (), s->task);
      /* The exception thread may have hit an error, in which
	 case it's sitting in read wondering what to do.  Tell
	 it to exit here.  */
      if (s->transmit_to_fd >= 0)
	{
	  unsigned char charbuf[1] = { 1 };
	  write (s->transmit_to_fd, charbuf, 1);
	}
      gdb_thread_kill (s->exception_thread);
    }

  if (s->receive_from_fd > 0)
    {
      delete_file_handler (s->receive_from_fd);
      close (s->receive_from_fd);
    }
  if (s->transmit_from_fd > 0)
    close (s->transmit_from_fd);
  if (s->receive_to_fd > 0)
    close (s->receive_to_fd);
  if (s->transmit_to_fd > 0)
    close (s->transmit_to_fd);
  if (s->error_transmit_fd > 0)
    close (s->error_transmit_fd);
  if (s->error_receive_fd > 0)
    {
      delete_file_handler (s->error_receive_fd);
      close (s->error_receive_fd);
    }

  xfree (msg_data);
  msg_data = NULL;
  msg_data_size = MACOSX_EXCEPTION_ARRAY_SIZE;

  pthread_mutex_destroy (&write_mutex);
  macosx_exception_thread_init (s);
}

static void
macosx_exception_thread (void *arg)
{
  int first_time = 1;
  macosx_exception_thread_status *s = (macosx_exception_thread_status *) arg;
  CHECK_FATAL (s != NULL);
  
  int next_msg_ctr;

#ifdef HAVE_PTHREAD_SETNAME_NP
  pthread_setname_np ("exception thread");
#endif

  for (;;)
    {
      unsigned char buf[1];
      mach_msg_option_t receive_options;
      kern_return_t kret;
      int counter;

      excthread_debug_re (1, "waiting for exceptions\n");
      receive_options = MACH_RCV_MSG | MACH_RCV_INTERRUPT;
      next_msg_ctr = 0;
      
      /* This is the main loop where we wait for events, and send them to 
	 the main thread.  We do this in several stages:

            a) Wait with no timeout for some event from the target.
            b) Suspend the task
	    c) Parse the event we got & store it in MSG_DATA
	    d) Poll for any other events that are available, and
	       parse & add them to MSG_DATA.
	    e) Send all the events to the main thread
	    f) Wait till the main thread wakes us up.
	    g) Send all msg replies back to the target.
	    h) restart the target.

	  We need to do it this way because for multi-threaded programs there
	  are often multiple breakpoint events queued up when we stop, and if
	  we don't clear them all, then when we next start the target, we
	  will hit the other queued ones.  This will get in the way of 
	  performing tasks like single stepping over the breakpoint trap.

	  There is equivalent code on the macosx-nat-inferior.c side of this
	  (in macosx_process_events) that decides what to do with multiple 
	  events.  */
      while (1)
	{
	  pthread_testcancel ();
	  
	  if (first_time)
	    {
	      first_time = 0;
	      pthread_mutex_lock (&excthread_mutex);
	      pthread_cond_signal (&excthread_cond);
	      pthread_mutex_unlock (&excthread_mutex);
	    }

	  kret =
	    mach_msg (&msg_data[next_msg_ctr].msgin.hdr, receive_options, 0,
		      sizeof (msg_data[next_msg_ctr].msgin.data), 
		      s->inferior_exception_port, 0,
		      MACH_PORT_NULL);
	  
	  if (s->shutting_down)
	    {
	      excthread_debug_re (1, "Shutting down - got out of mach_msg with: : %s (0x%lx)\n",
		   MACH_ERROR_STRING (kret), (unsigned long) kret);
	      next_msg_ctr = -1;
	      break;;
	    }

	  if (kret == MACH_RCV_INTERRUPTED)
	    {  
	      kern_return_t kret;
	      struct task_basic_info info;
	      unsigned int info_count = TASK_BASIC_INFO_COUNT;
	      
	      /* When we go to kill the target, we get an interrupt,
		 but the target is dead.  If we go back & try mach_msg
		 again, we just hang.  So we need to check that the
		 task is still alive before we wait for it.  */

	      excthread_debug_re (1, "receive interrupted\n");
	      kret =
		task_info (s->task, TASK_BASIC_INFO, 
			   (task_info_t) &info, &info_count);
	      if (kret != KERN_SUCCESS)
		{
		  excthread_debug_re (1, "task no longer valid\n");
		  next_msg_ctr = -1;
		  break;
		}
	      else
		continue;
	    }
	  else if (kret == MACH_RCV_TIMED_OUT)
	    {
	      excthread_debug_re (1, "no more exceptions\n");
	      break;
	    }
	  else if (kret != KERN_SUCCESS)
	    {
	      excthread_debug_re
		  (0, "error receiving exception message: %s (0x%lx)\n",
		   MACH_ERROR_STRING (kret), (unsigned long) kret);
	      write (s->error_transmit_fd, "e", 1);
	      next_msg_ctr = -1;
	      break;
	    }

	  if (next_msg_ctr == 0)
	    {
	      excthread_debug_re (2, "suspending task\n");
	      task_suspend (s->task);
	    }
	  excthread_debug_re (3, "parsing exception\n");
	  static_message = &msg_data[next_msg_ctr].msgsend;
	  kret = mach_exc_server (&msg_data[next_msg_ctr].msgin.hdr, 
			     &msg_data[next_msg_ctr].msgout.hdr);
	  static_message = NULL;
	  
	  excthread_debug_re (2, "received exception %d:", next_msg_ctr);
	  excthread_debug_exception (2, &msg_data[next_msg_ctr].msgin.hdr);
	  excthread_debug_re_endline (2);
	  
	  /* The msgsend exception data field is a pointer to the data
	     we got from the Mach message.  We are passing this
	     pointer to the main thread.  We should really make a copy
	     of the data, and pass a pointer to the copy, since we
	     don't know that fetching a second event might not free
	     this data.  And indeed sometimes it does!  */
	  {
	    mach_exception_data_t copy = (mach_exception_data_t) xmalloc (msg_data[next_msg_ctr].msgsend.data_count 
								* sizeof (mach_exception_data_type_t));
	    memcpy (copy, msg_data[next_msg_ctr].msgsend.exception_data, 
		    msg_data[next_msg_ctr].msgsend.data_count 
		    * sizeof (mach_exception_data_type_t));
	    msg_data[next_msg_ctr].msgsend.exception_data = copy;
	  }

	  next_msg_ctr++;
	  if (next_msg_ctr == msg_data_size)
	    {
	      msg_data_size += MACOSX_EXCEPTION_ARRAY_SIZE;
	      alloc_msg_data (&msg_data, msg_data_size);
	    }
	  receive_options |= MACH_RCV_TIMEOUT;

	}

      macosx_exception_get_write_lock (s);
      for (counter = 0; counter < next_msg_ctr; counter++) 
	{

	  excthread_debug_re (1, "sending exception to main thread: %d ", counter);
	  excthread_debug_message (1, &msg_data[counter].msgsend);
	  excthread_debug_re_endline (1);
	  write (s->transmit_from_fd, &msg_data[counter].msgsend, sizeof (msg_data[counter].msgsend));
	  
	}
      macosx_exception_release_write_lock (s);

      excthread_debug_re (3, "waiting for gdb\n");
      read (s->receive_to_fd, &buf, 1);
      excthread_debug_re (3, "done waiting for gdb\n");
      /* The main thread uses 0 to mean continue on, and
	 1 to mean exit.  */
      if (buf[0] == 1)
	return;

      for (counter = 0; counter < next_msg_ctr; counter++) 
	{
	  if (excthread_debugflag)
	    {
	      excthread_debug_re (2, "sending exception reply: ");
	      excthread_debug_exception (2, &msg_data[counter].msgout.hdr);
	      excthread_debug_re_endline (2);
	    }
	  
	  kret = mach_msg (&msg_data[counter].msgout.hdr, (MACH_SEND_MSG | MACH_SEND_INTERRUPT),
			   msg_data[counter].msgout.hdr.msgh_size, 0,
			   MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	  
	  if (kret == MACH_SEND_INTERRUPTED)
	    {
	      excthread_debug_re (1, "macosx_exceptions_thread: reply interrupted\n");
	      continue;
	    }
	  
	  if (kret != KERN_SUCCESS)
	    {
	      if (msg_data[counter].msgsend.task_port == s->task)
		{
		  excthread_debug_re
		    (0, "error sending exception reply: %s (0x%lx)\n",
		     MACH_ERROR_STRING (kret), (unsigned long) kret);
		  abort ();
		}
	      else
		{
		  excthread_debug_re
		    (0, "error sending exception reply to child task: 0x%x: %s (0x%lx)\n",
		     msg_data[counter].msgsend.task_port, MACH_ERROR_STRING (kret), (unsigned long) kret);
		}
	    }
	  /* Remember to free the copy of the exception data that we
	     made when we received the data above.  */
	  xfree (msg_data[counter].msgsend.exception_data);
	}
      excthread_debug_re (2, "Resuming task\n");
      task_resume (s->task);
      if (kret != KERN_SUCCESS)
	excthread_debug_re (2, "resumed task.\n");
      else
	excthread_debug_re (2, "resume task failed\n");

    }
}

void
_initialize_macosx_nat_excthread ()
{
  excthread_stderr_re = fdopen (fileno (stderr), "w");

  add_setshow_zinteger_cmd ("exceptions", class_obscure,
			    &excthread_debugflag, _("\
Set if printing exception thread debugging statements."), _("\
Show if printing exception thread debugging statements."), NULL,
			    NULL, NULL,
			    &setdebuglist, &showdebuglist);
}
