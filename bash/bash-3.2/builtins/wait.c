/* wait.c, created from wait.def. */
#line 33 "wait.def"

#line 43 "wait.def"

#include <config.h>

#include "../bashtypes.h"
#include <signal.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <chartypes.h>

#include "../bashansi.h"

#include "../shell.h"
#include "../jobs.h"
#include "common.h"
#include "bashgetopt.h"

extern int wait_signal_received;

procenv_t wait_intr_buf;

/* Wait for the pid in LIST to stop or die.  If no arguments are given, then
   wait for all of the active background processes of the shell and return
   0.  If a list of pids or job specs are given, return the exit status of
   the last one waited for. */

#define WAIT_RETURN(s) \
  do \
    { \
      interrupt_immediately = old_interrupt_immediately;\
      return (s);\
    } \
  while (0)

int
wait_builtin (list)
     WORD_LIST *list;
{
  int status, code;
  volatile int old_interrupt_immediately;

  USE_VAR(list);

  if (no_options (list))
    return (EX_USAGE);
  list = loptend;

  old_interrupt_immediately = interrupt_immediately;
  interrupt_immediately++;

  /* POSIX.2 says:  When the shell is waiting (by means of the wait utility)
     for asynchronous commands to complete, the reception of a signal for
     which a trap has been set shall cause the wait utility to return
     immediately with an exit status greater than 128, after which the trap
     associated with the signal shall be taken.

     We handle SIGINT here; it's the only one that needs to be treated
     specially (I think), since it's handled specially in {no,}jobs.c. */
  code = setjmp (wait_intr_buf);
  if (code)
    {
      status = 128 + wait_signal_received;
      WAIT_RETURN (status);
    }

  /* We support jobs or pids.
     wait <pid-or-job> [pid-or-job ...] */

  /* But wait without any arguments means to wait for all of the shell's
     currently active background processes. */
  if (list == 0)
    {
      wait_for_background_pids ();
      WAIT_RETURN (EXECUTION_SUCCESS);
    }

  status = EXECUTION_SUCCESS;
  while (list)
    {
      pid_t pid;
      char *w;
      intmax_t pid_value;

      w = list->word->word;
      if (DIGIT (*w))
	{
	  if (legal_number (w, &pid_value) && pid_value == (pid_t)pid_value)
	    {
	      pid = (pid_t)pid_value;
	      status = wait_for_single_pid (pid);
	    }
	  else
	    {
	      sh_badpid (w);
	      WAIT_RETURN (EXECUTION_FAILURE);
	    }
	}
#if defined (JOB_CONTROL)
      else if (*w && *w == '%')
	/* Must be a job spec.  Check it out. */
	{
	  int job;
	  sigset_t set, oset;

	  BLOCK_CHILD (set, oset);
	  job = get_job_spec (list);

	  if (INVALID_JOB (job))
	    {
	      if (job != DUP_JOB)
		sh_badjob (list->word->word);
	      UNBLOCK_CHILD (oset);
	      status = 127;	/* As per Posix.2, section 4.70.2 */
	      list = list->next;
	      continue;
	    }

	  /* Job spec used.  Wait for the last pid in the pipeline. */
	  UNBLOCK_CHILD (oset);
	  status = wait_for_job (job);
	}
#endif /* JOB_CONTROL */
      else
	{
	  sh_badpid (w);
	  status = EXECUTION_FAILURE;
	}
      list = list->next;
    }

  WAIT_RETURN (status);
}
