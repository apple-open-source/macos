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
#include "macosx-nat-excthread.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-inferior-debug.h"
#include "macosx-nat-inferior-util.h"

#include "defs.h"
#include "gdbcmd.h"
#include "event-loop.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <sys/time.h>
#include <sys/select.h>

#include <mach/mach_error.h>

static FILE *excthread_stderr = NULL;
static FILE *excthread_stderr_re = NULL;
static int excthread_debugflag = 0;

extern boolean_t exc_server (mach_msg_header_t *in, mach_msg_header_t *out);

#if 0
static int excthread_debug (const char *fmt, ...)
{
  va_list ap;
  if (excthread_debugflag) {
    va_start (ap, fmt);
    fprintf (excthread_stderr, "[%d excthread]: ", getpid ());
    vfprintf (excthread_stderr, fmt, ap);
    va_end (ap);
    return 0;
  } else {
    return 0;
  }
}
#endif

/* A re-entrant version for use by the signal handling thread */

void excthread_debug_re (const char *fmt, ...)
{
  va_list ap;
  if (excthread_debugflag) {
    va_start (ap, fmt);
    fprintf (excthread_stderr_re, "[%d excthread]: ", getpid ());
    vfprintf (excthread_stderr_re, fmt, ap);
    va_end (ap);
    fflush (excthread_stderr_re);
  }
}

static void macosx_exception_thread (void *arg);

static macosx_exception_thread_message *static_message = NULL;

kern_return_t catch_exception_raise_state
(mach_port_t port,
 exception_type_t exception_type, exception_data_t exception_data, mach_msg_type_number_t data_count,
 thread_state_flavor_t *state_flavor,
 thread_state_t in_state, mach_msg_type_number_t in_state_count,
 thread_state_t out_state, mach_msg_type_number_t out_state_count)
{
  return KERN_FAILURE;
}

kern_return_t catch_exception_raise_state_identity
(mach_port_t port, mach_port_t thread_port, mach_port_t task_port,
 exception_type_t exception_type, exception_data_t exception_data, mach_msg_type_number_t data_count,
 thread_state_flavor_t *state_flavor,
 thread_state_t in_state, mach_msg_type_number_t in_state_count,
 thread_state_t out_state, mach_msg_type_number_t out_state_count)
{
  kern_return_t kret;

  kret = mach_port_deallocate (mach_task_self(), task_port);
  MACH_CHECK_ERROR (kret);
  kret = mach_port_deallocate (mach_task_self(), thread_port);
  MACH_CHECK_ERROR (kret);

  return KERN_FAILURE;
}

kern_return_t catch_exception_raise
(mach_port_t port, mach_port_t thread_port, mach_port_t task_port, 
 exception_type_t exception_type, exception_data_t exception_data,
 mach_msg_type_number_t data_count)
{
#if 0
  kret = mach_port_deallocate (mach_task_self(), task_port);
  MACH_CHECK_ERROR (kret);
  kret = mach_port_deallocate (mach_task_self(), thread_port);
  MACH_CHECK_ERROR (kret);
#endif

  static_message->task_port = task_port;
  static_message->thread_port = thread_port;
  static_message->exception_type = exception_type;
  static_message->exception_data = exception_data;
  static_message->data_count = data_count;

  return KERN_SUCCESS;
}

void macosx_exception_thread_init (macosx_exception_thread_status *s)
{
  s->transmit_from_fd = -1;
  s->receive_from_fd = -1;
  s->transmit_to_fd = -1;
  s->receive_to_fd = -1;

  s->inferior_exception_port = PORT_NULL;

  memset (&s->saved_exceptions, 0, sizeof (s->saved_exceptions));
  memset (&s->saved_exceptions_step, 0, sizeof (s->saved_exceptions_step));

  s->saved_exceptions_stepping = 0;
  s->exception_thread = THREAD_NULL;
}

void macosx_exception_thread_create (macosx_exception_thread_status *s, task_t task)
{
  int fd[2];
  int ret;
  kern_return_t kret;
 
  ret = pipe (fd);
  CHECK_FATAL (ret == 0);
  s->transmit_from_fd = fd[1];
  s->receive_from_fd = fd[0];

  ret = pipe (fd);
  CHECK_FATAL (ret == 0);
  s->transmit_to_fd = fd[1];
  s->receive_to_fd = fd[0];

  kret = mach_port_allocate (mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &s->inferior_exception_port);
  MACH_CHECK_ERROR (kret);
  kret = mach_port_insert_right (mach_task_self(), s->inferior_exception_port,
				 s->inferior_exception_port, MACH_MSG_TYPE_MAKE_SEND);
  MACH_CHECK_ERROR (kret);

  if (inferior_bind_exception_port_flag) {

    macosx_save_exception_ports (task, &s->saved_exceptions);

    kret = task_set_exception_ports
      (task,
       EXC_MASK_ALL,
       s->inferior_exception_port, EXCEPTION_DEFAULT, THREAD_STATE_NONE);
    MACH_CHECK_ERROR (kret);
  }

  s->exception_thread = gdb_thread_fork ((gdb_thread_fn_t) &macosx_exception_thread, s);
}

void macosx_exception_thread_destroy (macosx_exception_thread_status *s)
{
  if (s->exception_thread != THREAD_NULL) {
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

  macosx_exception_thread_init (s);
}

static void macosx_exception_thread (void *arg)
{
  macosx_exception_thread_status *s = (macosx_exception_thread_status *) arg;
  CHECK_FATAL (s != NULL);

  for (;;) {

    unsigned char buf[1];

    unsigned char msgin_data[1024];
    mach_msg_header_t *msgin_hdr = (mach_msg_header_t *) msgin_data;

    unsigned char msgout_data[1024];
    mach_msg_header_t *msgout_hdr = (mach_msg_header_t *) msgout_data;

    macosx_exception_thread_message msgsend;

    kern_return_t kret;

    pthread_testcancel ();

    excthread_debug_re ("macosx_exception_thread: waiting for exceptions\n"); 
    kret = mach_msg (msgin_hdr, (MACH_RCV_MSG | MACH_RCV_INTERRUPT),
		     0, sizeof (msgin_data), s->inferior_exception_port, 0, MACH_PORT_NULL);
    if (kret == MACH_RCV_INTERRUPTED) { continue; }
    if (kret != KERN_SUCCESS) {
      fprintf (excthread_stderr_re, "macosx_exception_thread: error receiving exception message: %s (0x%lx)\n", 
	       MACH_ERROR_STRING (kret), (unsigned long) kret);
      abort ();
    }

    static_message = &msgsend;
    excthread_debug_re ("macosx_exception_thread: parsing exception\n");
    kret = exc_server (msgin_hdr, msgout_hdr);
    static_message = NULL;
    
    write (s->transmit_from_fd, &msgsend, sizeof (msgsend));
    read (s->receive_to_fd, &buf, 1);
  
    excthread_debug_re ("macosx_exception_thread: sending exception reply with type %s\n",
			unparse_exception_type (msgsend.exception_type));
    kret = mach_msg (msgout_hdr, (MACH_SEND_MSG | MACH_SEND_INTERRUPT),
		     msgout_hdr->msgh_size, 0,
		     MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if (kret == MACH_SEND_INTERRUPTED) { continue; }
    if (kret != KERN_SUCCESS) {
      fprintf (excthread_stderr_re, "macosx_exception_thread: error sending exception reply: %s (0x%lx)\n", 
	       MACH_ERROR_STRING (kret), (unsigned long) kret);
      abort ();
    }
  }
}

void
_initialize_macosx_nat_excthread ()
{
  struct cmd_list_element *cmd = NULL;

  excthread_stderr = fdopen (fileno (stderr), "w+");
  excthread_stderr_re = fdopen (fileno (stderr), "w+");

  cmd = add_set_cmd ("exceptions", class_obscure, var_boolean, 
		     (char *) &excthread_debugflag,
		     "Set if printing exception thread debugging statements.",
		     &setdebuglist);
  add_show_from_set (cmd, &showdebuglist);		
}
