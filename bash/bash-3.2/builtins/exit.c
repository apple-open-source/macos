/* exit.c, created from exit.def. */
#line 23 "exit.def"

#line 30 "exit.def"

#include <config.h>

#include "../bashtypes.h"
#include <stdio.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "../bashintl.h"

#include "../shell.h"
#include "../jobs.h"

#include "common.h"
#include "builtext.h"	/* for jobs_builtin */

extern int last_command_exit_value;
extern int running_trap, trap_saved_exit_value;
extern int subshell_environment;
extern sh_builtin_func_t *this_shell_builtin;
extern sh_builtin_func_t *last_shell_builtin;

static int exit_or_logout __P((WORD_LIST *));
static int sourced_logout;

int
exit_builtin (list)
     WORD_LIST *list;
{
  if (interactive)
    {
      fprintf (stderr, login_shell ? "logout\n" : "exit\n");
      fflush (stderr);
    }

  return (exit_or_logout (list));
}

#line 75 "exit.def"

/* How to logout. */
int
logout_builtin (list)
     WORD_LIST *list;
{
  if (login_shell == 0 /* && interactive */)
    {
      builtin_error (_("not login shell: use `exit'"));
      return (EXECUTION_FAILURE);
    }
  else
    return (exit_or_logout (list));
}

static int
exit_or_logout (list)
     WORD_LIST *list;
{
  int exit_value;

#if defined (JOB_CONTROL)
  int exit_immediate_okay;

  exit_immediate_okay = (interactive  == 0 ||
			 last_shell_builtin == exit_builtin ||
			 last_shell_builtin == logout_builtin ||
			 last_shell_builtin == jobs_builtin);

  /* Check for stopped jobs if the user wants to. */
  if (!exit_immediate_okay)
    {
      register int i;
      for (i = 0; i < js.j_jobslots; i++)
	if (jobs[i] && STOPPED (i))
	  {
	    fprintf (stderr, _("There are stopped jobs.\n"));

	    /* This is NOT superfluous because EOF can get here without
	       going through the command parser.  Set both last and this
	       so that either `exit', `logout', or ^D will work to exit
	       immediately if nothing intervenes. */
	    this_shell_builtin = last_shell_builtin = exit_builtin;
	    return (EXECUTION_FAILURE);
	  }
    }
#endif /* JOB_CONTROL */

  /* Get return value if present.  This means that you can type
     `logout 5' to a shell, and it returns 5. */

  /* If we're running the exit trap (running_trap == 1, since running_trap
     gets set to SIG+1), and we don't have a argument given to `exit'
     (list == 0), use the exit status we saved before running the trap
     commands (trap_saved_exit_value). */
  exit_value = (running_trap == 1 && list == 0) ? trap_saved_exit_value : get_exitstat (list);

  bash_logout ();

  last_command_exit_value = exit_value;

  /* Exit the program. */
  jump_to_top_level (EXITPROG);
  /*NOTREACHED*/
}

void
bash_logout ()
{
  /* Run our `~/.bash_logout' file if it exists, and this is a login shell. */
  if (login_shell && sourced_logout++ == 0 && subshell_environment == 0)
    {
      maybe_execute_file ("~/.bash_logout", 1);
#ifdef SYS_BASH_LOGOUT
      maybe_execute_file (SYS_BASH_LOGOUT, 1);
#endif
    }
}
