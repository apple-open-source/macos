#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <sys/time.h>
#include <sys/select.h>

#include "nextstep-nat-inferior.h"
#include "nextstep-nat-cfmthread.h"
#include "nextstep-nat-mutils.h"

#include "nextstep-nat-cfm.h"

#include "defs.h"
#include "gdbcmd.h"

#include "CodeFragmentInfoPriv.h"

extern next_inferior_status *next_status;

static FILE *cfmthread_stderr = NULL;
static FILE *cfmthread_stderr_re = NULL;
static int cfmthread_debugflag = 0;

static int cfmthread_debug (const char *fmt, ...)
{
  va_list ap;
  if (cfmthread_debugflag) {
    va_start (ap, fmt);
    fprintf (cfmthread_stderr, "[%d cfmthread]: ", getpid ());
    vfprintf (cfmthread_stderr, fmt, ap);
    va_end (ap);
    return 0;
  } else {
    return 0;
  }
}

/* A re-entrant version for use by the signal handling thread */

void cfmthread_debug_re (const char *fmt, ...)
{
  va_list ap;
  if (cfmthread_debugflag) {
    va_start (ap, fmt);
    fprintf (cfmthread_stderr_re, "[%d cfmthread]: ", getpid ());
    vfprintf (cfmthread_stderr_re, fmt, ap);
    va_end (ap);
    fflush (cfmthread_stderr_re);
  }
}

static void next_cfm_thread (void *arg);

void next_cfm_thread_init (next_cfm_thread_status *s)
{
  gClosures = NULL;

  s->transmit_fd = -1;
  s->receive_fd = -1;

  s->info_api_cookie = NULL;
  s->cfm_send_right = MACH_PORT_NULL;
  s->cfm_receive_right = MACH_PORT_NULL;
	
  s->cfm_thread = THREAD_NULL;
}

void next_cfm_thread_create (next_cfm_thread_status *s, task_t task)
{
  int fd[2];
  int ret;
  kern_return_t	kret;
  
  ret = pipe (fd);
  CHECK_FATAL (ret == 0);

  s->transmit_fd = fd[1];
  s->receive_fd = fd[0];

  kret = mach_port_allocate (mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &s->cfm_receive_right);
  MACH_CHECK_ERROR (kret);
	
  kret = mach_port_allocate (task, MACH_PORT_RIGHT_RECEIVE, &s->cfm_send_right);
  MACH_CHECK_ERROR (kret);
	
  kret = mach_port_destroy (task, s->cfm_send_right);
  MACH_CHECK_ERROR (kret);

  kret = mach_port_insert_right (task, s->cfm_send_right, s->cfm_receive_right, MACH_MSG_TYPE_MAKE_SEND);
  MACH_CHECK_ERROR (kret);

  kret = CCFM_SetInfoSPITarget ((MPProcessID) task, s->info_api_cookie, s->cfm_send_right);
  MACH_CHECK_ERROR (kret);

  s->cfm_thread = gdb_thread_fork ((gdb_thread_fn_t) &next_cfm_thread, s);
}

void next_cfm_thread_destroy (next_cfm_thread_status *s)
{
  port_deallocate (mach_task_self(), s->cfm_receive_right);
  port_deallocate (mach_task_self(), s->cfm_send_right);

  s->cfm_receive_right = PORT_NULL;
  s->cfm_send_right = PORT_NULL;

  if (s->cfm_thread != THREAD_NULL) {
    gdb_thread_kill (s->cfm_thread);
  }

  if (s->receive_fd > 0)
    {
      delete_file_handler (s->receive_fd);
      close (s->receive_fd);
    }
  if (s->transmit_fd > 0) 
    close (s->transmit_fd);

  next_cfm_thread_init (s);
}

void next_cfm_thread_debug (FILE *f, next_cfm_thread_status *s)
{
  fprintf (f, "                [CFM THREAD]\n");
}

static void next_cfm_thread (void *arg)
{
  next_cfm_thread_status *s = (next_cfm_thread_status *) arg;
  CHECK_FATAL (s != NULL);

  for (;;) {

    unsigned char msgin_data[1024];
    msg_header_t *msgin_hdr = (msg_header_t *) msgin_data;
    next_cfm_message *msgin = (next_cfm_message *) msgin_data;

    unsigned char msgout_data[1024];
    msg_header_t *msgout_hdr = (msg_header_t *) msgout_data;

    next_cfm_thread_message msgsend;

    kern_return_t kret;

    pthread_testcancel ();

    kret = mach_msg (msgin_hdr, (MACH_RCV_MSG | MACH_RCV_INTERRUPT),
		     0, sizeof (msgin_data), s->cfm_receive_right, 0, MACH_PORT_NULL);
    if (kret == MACH_RCV_INTERRUPTED) { continue; }
    if (kret != KERN_SUCCESS) {
      fprintf (cfmthread_stderr_re, "next_cfm_thread: error receiving cfm message: %s (0x%lx)\n", 
	       MACH_ERROR_STRING (kret), (unsigned long) kret);
      abort ();
    }

    kret = task_suspend (next_status->task);
    if (kret != KERN_SUCCESS) {
      fprintf (cfmthread_stderr_re, "next_cfm_thread: unable to suspend task generating event: %s (0x%lx)\n", 
	       MACH_ERROR_STRING (kret), (unsigned long) kret);
      abort ();
    }

    memset (msgout_data, '\0', sizeof (msgout_data));

    msgout_hdr->msgh_bits = MACH_MSGH_BITS (MACH_MSG_TYPE_COPY_SEND, 0);
    msgout_hdr->msgh_size = sizeof (msg_header_t);
    msgout_hdr->msgh_local_port = MACH_PORT_NULL;
    msgout_hdr->msgh_remote_port = msgin_hdr->msgh_remote_port;
    msgout_hdr->msgh_reserved = 0;
    msgout_hdr->msgh_id = 0;
    
    kret = mach_msg (msgout_hdr, (MACH_SEND_MSG | MACH_SEND_INTERRUPT),
		     msgout_hdr->msgh_size, 0,
		     MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if (kret == MACH_SEND_INTERRUPTED) { continue; }
    MACH_CHECK_ERROR (kret);
    if (kret != KERN_SUCCESS) {
      fprintf (cfmthread_stderr_re, "next_cfm_thread: error sending cfm message: %s (0x%lx)\n", 
	       MACH_ERROR_STRING (kret), (unsigned long) kret);
      abort ();
    }

    memcpy (&msgsend, msgin_data + sizeof (msg_header_t), sizeof (next_cfm_thread_message));
    write (s->transmit_fd, &msgsend, sizeof (msgsend));
  }
}

void
_initialize_nextstep_nat_cfmthread ()
{
  struct cmd_list_element *cmd = NULL;

  cfmthread_stderr = fdopen (fileno (stderr), "w+");
  cfmthread_stderr_re = fdopen (fileno (stderr), "w+");

  cmd = add_set_cmd ("debug-cfm", class_obscure, var_boolean, 
		     (char *) &cfmthread_debugflag,
		     "Set if printing cfm thread debugging statements.",
		     &setlist);
  add_show_from_set (cmd, &showlist);		
}
