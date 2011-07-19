/* test.c, created from test.def. */
#line 23 "test.def"

#line 93 "test.def"

#line 101 "test.def"

#include <config.h>

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include "../bashansi.h"

#include "../shell.h"
#include "../test.h"
#include "common.h"

extern char *this_command_name;

/* TEST/[ builtin. */
int
test_builtin (list)
     WORD_LIST *list;
{
  char **argv;
  int argc, result;

  /* We let Matthew Bradburn and Kevin Braunsdorf's code do the
     actual test command.  So turn the list of args into an array
     of strings, since that is what their code wants. */
  if (list == 0)
    {
      if (this_command_name[0] == '[' && !this_command_name[1])
	{
	  builtin_error ("missing `]'");
	  return (EX_BADUSAGE);
	}

      return (EXECUTION_FAILURE);
    }

  argv = make_builtin_argv  (list, &argc);
  result = test_command (argc, argv);
  free ((char *)argv);

  return (result);
}
