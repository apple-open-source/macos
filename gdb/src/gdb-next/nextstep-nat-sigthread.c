#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <sys/time.h>
#include <sys/select.h>

#include "nextstep-nat-sigthread.h"
#include "nextstep-nat-mutils.h"

#include "defs.h"
#include "gdbcmd.h"

static FILE *sigthread_stderr = NULL;
static FILE *sigthread_stderr_re = NULL;
static int sigthread_debugflag = 0;

static int sigthread_debug (const char *fmt, ...)
{
  va_list ap;
  if (sigthread_debugflag) {
    va_start (ap, fmt);
    fprintf (sigthread_stderr, "[%d sigthread]: ", getpid ());
    vfprintf (sigthread_stderr, fmt, ap);
    va_end (ap);
    return 0;
  } else {
    return 0;
  }
}

/* A re-entrant version for use by the signal handling thread */

void sigthread_debug_re (const char *fmt, ...)
{
  va_list ap;
  if (sigthread_debugflag) {
    va_start (ap, fmt);
    fprintf (sigthread_stderr_re, "[%d sigthread]: ", getpid ());
    vfprintf (sigthread_stderr_re, fmt, ap);
    va_end (ap);
    fflush (sigthread_stderr_re);
  }
}

static void next_signal_thread (void *arg);

void next_signal_thread_init (next_signal_thread_status *s)
{
  s->transmit_fd = -1;
  s->receive_fd = -1;

  s->inferior_pid = -1;
  s->signal_thread = THREAD_NULL;
}

void next_signal_thread_create (next_signal_thread_status *s, int pid)
{
  int fd[2];
  int ret;
 
  ret = pipe (fd);
  CHECK_FATAL (ret == 0);

  s->transmit_fd = fd[1];
  s->receive_fd = fd[0];

  s->inferior_pid = pid;

  s->signal_thread = gdb_thread_fork ((gdb_thread_fn_t) &next_signal_thread, s);
}

void next_signal_thread_destroy (next_signal_thread_status *s)
{
  if (s->signal_thread != THREAD_NULL) {
    gdb_thread_kill (s->signal_thread);
  }

  if (s->receive_fd > 0)
    {
      delete_file_handler (s->receive_fd);
      close (s->receive_fd);
    }
  if (s->transmit_fd > 0) 
    close (s->transmit_fd);

  next_signal_thread_init (s);
}

void next_signal_thread_debug (FILE *f, next_signal_thread_status *s)
{
  fprintf (f, "                [SIGNAL THREAD]\n");
}

void next_signal_thread_debug_status (FILE *f, WAITSTATUS status)
{
  if (WIFEXITED (status)) {
    fprintf (f, "process exited with status %d\n", WEXITSTATUS (status));
  } else if (WIFSIGNALED (status)) {
    fprintf (f, "process terminated with signal %d (%s)\n", WTERMSIG (status),
	     target_signal_to_string (WTERMSIG (status)));
  } else if (WIFSTOPPED (status)) {
    fprintf (f, "process stopped with signal %d (%s)\n", WSTOPSIG (status),
	     target_signal_to_string (WSTOPSIG (status)));
  } else {
    fprintf (f, "next_debug_status: unknown status value %d\n", status);
  }
}

static void next_signal_thread (void *arg)
{
  next_signal_thread_status *s = (next_signal_thread_status *) arg;
  CHECK_FATAL (s != NULL);

  for (;;) {

    next_signal_thread_message msg;
    WAITSTATUS status = 0;
    pid_t pid = 0;

    pthread_testcancel ();

    sigthread_debug_re ("next_signal_thread: waiting for signals for pid %d\n", s->inferior_pid);
    pid = waitpid (s->inferior_pid, &status, 0);
    sigthread_debug_re ("next_signal_thread: done waiting for signals for pid %d\n", s->inferior_pid);

    if ((pid < 0) && (errno == ECHILD)) {
      sigthread_debug_re ("next_signal_thread: no children present: waiting for parent\n");
      for (;;) {
	pthread_testcancel ();
	pause ();
	pthread_testcancel ();
      }
    }

    if ((pid < 0) && (errno == EINTR)) {
      sigthread_debug_re ("next_signal_thread: wait interrupted; continuing\n");
      continue;
    }

    if (pid < 0) {
      fprintf (sigthread_stderr_re, "next_signal_thread: unexpected error: %s\n", strerror (errno));
      abort ();
    }

    if (sigthread_debugflag) {
      sigthread_debug_re ("next_signal_thread: got status %d for pid %d (expected inferior is %d)\n",
			  status, pid, s->inferior_pid);
      sigthread_debug_re ("next_signal_thread: got signal ");
      next_signal_thread_debug_status (sigthread_stderr_re, status);
    }

    if (pid != s->inferior_pid) {
      fprintf (sigthread_stderr_re,
	       "next_signal_thread: got status value %d for unexpected pid %d\n", status, pid);
      abort ();
    }

    msg.pid = pid;
    msg.status = status;
    write (s->transmit_fd, &msg, sizeof (msg));
  }
}

void
_initialize_nextstep_nat_sigthread ()
{
  struct cmd_list_element *cmd = NULL;

  sigthread_stderr = fdopen (fileno (stderr), "w+");
  sigthread_stderr_re = fdopen (fileno (stderr), "w+");

  cmd = add_set_cmd ("debug-signals", class_obscure, var_boolean, 
		     (char *) &sigthread_debugflag,
		     "Set if printing signal thread debugging statements.",
		     &setlist);
  add_show_from_set (cmd, &showlist);		
}
