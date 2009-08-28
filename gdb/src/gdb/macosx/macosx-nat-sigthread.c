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

#include "macosx-nat-sigthread.h"
#include "macosx-nat-inferior.h"
#include "macosx-nat-mutils.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <sys/time.h>
#include <sys/select.h>

static FILE *sigthread_stderr_re = NULL;
static int sigthread_debugflag = 0;

/* A re-entrant version for use by the signal handling thread */

void
sigthread_debug_re (const char *fmt, ...)
{
  va_list ap;
  if (sigthread_debugflag)
    {
      va_start (ap, fmt);
      fprintf (sigthread_stderr_re, "[%d sigthread]: ", getpid ());
      vfprintf (sigthread_stderr_re, fmt, ap);
      va_end (ap);
      fflush (sigthread_stderr_re);
    }
}

static void macosx_signal_thread (void *arg);

void
macosx_signal_thread_init (macosx_signal_thread_status *s)
{
  s->transmit_fd = -1;
  s->receive_fd = -1;

  s->inferior_pid = -1;
  s->signal_thread = THREAD_NULL;
}

static pthread_cond_t sigthread_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t sigthread_mutex = PTHREAD_MUTEX_INITIALIZER;

void
macosx_signal_thread_create (macosx_signal_thread_status *s, int pid)
{
  int fd[2];
  int ret;
  struct gdb_exception e;

  ret = pipe (fd);
  CHECK_FATAL (ret == 0);

  s->transmit_fd = fd[1];
  s->receive_fd = fd[0];

  s->inferior_pid = pid;

  pthread_mutex_lock (&sigthread_mutex);
  TRY_CATCH (e, RETURN_MASK_ERROR)
  {
    s->signal_thread =
      gdb_thread_fork ((gdb_thread_fn_t) &macosx_signal_thread, s);
  }
  if (e.reason != NO_ERROR)
    {
      pthread_mutex_unlock (&sigthread_mutex);
      throw_exception (e);
    }

  pthread_cond_wait (&sigthread_cond, &sigthread_mutex);
  pthread_mutex_unlock (&sigthread_mutex);

}

void
macosx_signal_thread_destroy (macosx_signal_thread_status *s)
{
  if (s->signal_thread != THREAD_NULL)
    {
      gdb_thread_kill (s->signal_thread);
    }

  if (s->receive_fd > 0)
    {
      delete_file_handler (s->receive_fd);
      close (s->receive_fd);
    }
  if (s->transmit_fd > 0)
    close (s->transmit_fd);

  macosx_signal_thread_init (s);
}

void
macosx_signal_thread_debug (FILE *f, macosx_signal_thread_status *s)
{
  fprintf (f, "                [SIGNAL THREAD]\n");
}

void
macosx_signal_thread_debug_status (FILE *f, WAITSTATUS status)
{
  if (WIFEXITED (status))
    {
      fprintf (f, "process exited with status %d\n", WEXITSTATUS (status));
    }
  else if (WIFSIGNALED (status))
    {
      fprintf (f, "process terminated with signal %d (%s)\n",
               WTERMSIG (status),
               target_signal_to_string (WTERMSIG (status)));
    }
  else if (WIFSTOPPED (status))
    {
      fprintf (f, "process stopped with signal %d (%s)\n", WSTOPSIG (status),
               target_signal_to_string (WSTOPSIG (status)));
    }
  else
    {
      fprintf (f, "unknown status value %d\n", status);
    }
}

static void
macosx_signal_thread (void *arg)
{
  int first_time = 1;
  macosx_signal_thread_status *s = (macosx_signal_thread_status *) arg;
  CHECK_FATAL (s != NULL);

#ifdef HAVE_PTHREAD_SETNAME_NP
  pthread_setname_np ("signal thread");
#endif

  for (;;)
    {
      macosx_signal_thread_message msg;
      WAITSTATUS status = 0;
      pid_t pid = 0;

      pthread_testcancel ();

      sigthread_debug_re
        ("macosx_signal_thread: waiting for events for pid %d\n",
         s->inferior_pid);

      if (first_time)
        {
          first_time = 0;
          pthread_mutex_lock (&sigthread_mutex);
          pthread_cond_broadcast (&sigthread_cond);
          pthread_mutex_unlock (&sigthread_mutex);
        }
      
      pid = waitpid (s->inferior_pid, &status, 0);

      sigthread_debug_re
        ("macosx_signal_thread: received event for pid %d\n",
         s->inferior_pid);

      if ((pid < 0) && (errno == ECHILD))
        {
          sigthread_debug_re
            ("macosx_signal_thread: no children present; waiting for parent\n");
          for (;;)
            {
              pthread_testcancel ();
              sched_yield ();
            }
        }

      if ((pid < 0) && (errno == EINTR))
        {
          sigthread_debug_re
            ("macosx_signal_thread: wait interrupted; continuing\n");
          continue;
        }

      if (pid < 0)
        {
          fprintf (sigthread_stderr_re,
                   "macosx_signal_thread: unexpected error: %s\n",
                   strerror (errno));
          abort ();
        }

      if (sigthread_debugflag)
        {
          sigthread_debug_re ("macosx_signal_thread: received event for pid %d: ", pid);
          macosx_signal_thread_debug_status (sigthread_stderr_re, status);
        }

      if (pid != s->inferior_pid)
        {
          fprintf (sigthread_stderr_re,
                   "macosx_signal_thread: event was for unexpected pid (got %d, was expecting %d)\n",
                   pid, s->inferior_pid);
          abort ();
        }

      msg.pid = pid;
      msg.status = status;
      write (s->transmit_fd, &msg, sizeof (msg));
    }
}

void
_initialize_macosx_nat_sigthread ()
{
  sigthread_stderr_re = fdopen (fileno (stderr), "w");

  add_setshow_boolean_cmd ("signals", no_class,
			   &sigthread_debugflag, _("\
Set if printing signal thread debugging statements."), _("\
Show if printing signal thread debugging statements."), NULL,
			   NULL, NULL,
			   &setdebuglist, &showdebuglist);
}
