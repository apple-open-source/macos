#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <sys/time.h>
#include <sys/select.h>

#include "nextstep-nat-inferior.h"
#include "nextstep-nat-excthread.h"
#include "nextstep-nat-mutils.h"

#include "defs.h"
#include "gdbcmd.h"

static FILE *excthread_stderr = NULL;
static FILE *excthread_stderr_re = NULL;
static int excthread_debugflag = 0;

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

static void next_exception_thread (void *arg);

static next_exception_thread_message *static_message = NULL;

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
  kern_return_t kret;

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

void next_exception_thread_init (next_exception_thread_status *s)
{
  s->transmit_fd = -1;
  s->receive_fd = -1;

  s->inferior_exception_port = PORT_NULL;

  memset (&s->saved_exceptions, 0, sizeof (s->saved_exceptions));
  memset (&s->saved_exceptions_step, 0, sizeof (s->saved_exceptions_step));

  s->saved_exceptions_stepping = 0;
  s->exception_thread = THREAD_NULL;
}

void next_exception_thread_create (next_exception_thread_status *s, task_t task)
{
  int fd[2];
  int ret;
  kern_return_t kret;
 
  ret = pipe (fd);
  CHECK_FATAL (ret == 0);

  s->transmit_fd = fd[1];
  s->receive_fd = fd[0];

  kret = mach_port_allocate (mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &s->inferior_exception_port);
  MACH_CHECK_ERROR (kret);
  kret = mach_port_insert_right (mach_task_self(), s->inferior_exception_port,
				 s->inferior_exception_port, MACH_MSG_TYPE_MAKE_SEND);
  MACH_CHECK_ERROR (kret);

  if (inferior_bind_exception_port_flag) {

    next_save_exception_ports (task, &s->saved_exceptions);

    kret = task_set_exception_ports
      (task,
       EXC_MASK_ALL & ~(EXC_MASK_MACH_SYSCALL | EXC_MASK_SYSCALL | EXC_MASK_RPC_ALERT | EXC_MASK_SOFTWARE),
       s->inferior_exception_port, EXCEPTION_DEFAULT, THREAD_STATE_NONE);
    MACH_CHECK_ERROR (kret);
  }

  s->exception_thread = gdb_thread_fork ((gdb_thread_fn_t) &next_exception_thread, s);
}

void next_exception_thread_destroy (next_exception_thread_status *s)
{
  if (s->exception_thread != THREAD_NULL) {
    gdb_thread_kill (s->exception_thread);
  }

  if (s->receive_fd > 0)
    {
      delete_file_handler (s->receive_fd);
      close (s->receive_fd);
    }
  if (s->transmit_fd > 0) 
    close (s->transmit_fd);

  next_exception_thread_init (s);
}

void next_exception_thread_debug (FILE *f, next_exception_thread_status *s)
{
  fprintf (f, "                [EXCEPTION THREAD]\n");
}

static void next_exception_thread (void *arg)
{
  next_exception_thread_status *s = (next_exception_thread_status *) arg;
  CHECK_FATAL (s != NULL);

  for (;;) {

    unsigned char msgin_data[1024];
    msg_header_t *msgin_hdr = (msg_header_t *) msgin_data;

    unsigned char msgout_data[1024];
    msg_header_t *msgout_hdr = (msg_header_t *) msgout_data;

    next_exception_thread_message msgsend;

    kern_return_t kret;

    pthread_testcancel ();

    excthread_debug_re ("next_exception_thread: waiting for exceptions\n"); 
    kret = mach_msg (msgin_hdr, (MACH_RCV_MSG | MACH_RCV_INTERRUPT),
		     0, sizeof (msgin_data), s->inferior_exception_port, 0, MACH_PORT_NULL);
    if (kret == MACH_RCV_INTERRUPTED) { continue; }
    if (kret != KERN_SUCCESS) {
      fprintf (excthread_stderr_re, "next_exception_thread: error receiving exception message: %s (0x%lx)\n", 
	       MACH_ERROR_STRING (kret), (unsigned long) kret);
      abort ();
    }

    static_message = &msgsend;
    excthread_debug_re ("next_exception_thread: handling exception\n");
    kret = exc_server (msgin_hdr, msgout_hdr);
    static_message = NULL;
    
    kret = thread_suspend (msgsend.thread_port);
    if (kret != KERN_SUCCESS) {
      fprintf (excthread_stderr_re, "next_exception_thread: unable to suspend thread generating exception: %s (0x%lx)\n", 
	       MACH_ERROR_STRING (kret), (unsigned long) kret);
      abort ();
    }

    excthread_debug_re ("next_exception_thread: sending event\n");
    kret = mach_msg (msgout_hdr, (MACH_SEND_MSG | MACH_SEND_INTERRUPT),
		     msgout_hdr->msgh_size, 0,
		     MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if (kret == MACH_SEND_INTERRUPTED) { continue; }
    if (kret != KERN_SUCCESS) {
      fprintf (excthread_stderr_re, "next_exception_thread: error sending exception reply: %s (0x%lx)\n", 
	       MACH_ERROR_STRING (kret), (unsigned long) kret);
      abort ();
    }

    write (s->transmit_fd, &msgsend, sizeof (msgsend));
  }
}

void
_initialize_nextstep_nat_excthread ()
{
  struct cmd_list_element *cmd = NULL;

  excthread_stderr = fdopen (fileno (stderr), "w+");
  excthread_stderr_re = fdopen (fileno (stderr), "w+");

  cmd = add_set_cmd ("debug-exceptions", class_obscure, var_boolean, 
		     (char *) &excthread_debugflag,
		     "Set if printing exception thread debugging statements.",
		     &setlist);
  add_show_from_set (cmd, &showlist);		
}
