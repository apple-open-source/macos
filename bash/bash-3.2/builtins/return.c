/* return.c, created from return.def. */
#line 23 "return.def"

#line 31 "return.def"

#include <config.h>

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include "../bashintl.h"

#include "../shell.h"
#include "common.h"

extern int last_command_exit_value;
extern int subshell_environment;
extern int return_catch_flag, return_catch_value;

/* If we are executing a user-defined function then exit with the value
   specified as an argument.  if no argument is given, then the last
   exit status is used. */
int
return_builtin (list)
     WORD_LIST *list;
{
  return_catch_value = get_exitstat (list);

  if (return_catch_flag)
    longjmp (return_catch, 1);
  else
    {
      builtin_error (_("can only `return' from a function or sourced script"));
      return (EXECUTION_FAILURE);
    }
}
